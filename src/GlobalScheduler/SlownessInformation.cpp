/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2009 Javier Celaya
 *
 *  This file is part of PeerComp.
 *
 *  PeerComp is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  PeerComp is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with PeerComp; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */



#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/shared_array.hpp>
#include <limits>
#include <algorithm>
#include <sstream>
#include "Logger.hpp"
#include "SlownessInformation.hpp"
#include "Time.hpp"
#include "TaskDescription.hpp"
#include "MinSlownessScheduler.hpp"
using std::vector;
using std::list;
using std::make_pair;


unsigned int SlownessInformation::numClusters = 125;
unsigned int SlownessInformation::numIntervals = 5;
unsigned int SlownessInformation::numPieces = 64;
const double SlownessInformation::infinity = std::numeric_limits<double>::infinity();


SlownessInformation::LAFunction::LAFunction(const list<boost::shared_ptr<Task> > & tasks, double power) {
    // Trivial case
    if (tasks.empty()) {
        LogMsg("Ex.RI.Aggr", DEBUG) << "Creating availability info for empty queue and power " << power;
        pieces.push_back(make_pair(minTaskLength, SubFunction(0.0, 0.0, 1.0 / power, 0.0)));
        return;
    }

    // General case
    LogMsg("Ex.RI.Aggr", DEBUG) << "Creating availability info for " << tasks.size() << " tasks and power " << power;

    // List of tasks
    Time now = Time::getCurrentTime();
    vector<MinSlownessScheduler::TaskProxy> tps;
    for (list<boost::shared_ptr<Task> >::const_iterator i = tasks.begin(); i != tasks.end(); ++i)
        tps.push_back(MinSlownessScheduler::TaskProxy(*i, now));

    // List of slowness values where existing tasks change order, except the first one
    vector<double> lBounds;
    for (vector<MinSlownessScheduler::TaskProxy>::iterator it = ++tps.begin(); it != tps.end(); ++it)
        for (vector<MinSlownessScheduler::TaskProxy>::iterator jt = it; jt != tps.end(); ++jt)
            if (it->a != jt->a) {
                double l = (jt->r - it->r) / (it->a - jt->a);
                if (l > 0.0) {
                    lBounds.push_back(l);
                }
            }
    std::sort(lBounds.begin(), lBounds.end());

    // The new task
    tps.push_back(MinSlownessScheduler::TaskProxy(minTaskLength, power));

    while (true) {
        // Order the queue and calculate minimum slowness
        // The new task is at the end of the queue
        vector<double> lBoundsTmp(lBounds);
        lBoundsTmp.push_back(0.0);
        // Add order change values for the new task
        for (size_t i = 1; i < tps.size() - 1; ++i) {
            // The new task is the last in the vector
            if (tps[i].a != tps.back().a) {
                double l = tps[i].r / (tps.back().a - tps[i].a);
                if (l > 0.0)
                    lBoundsTmp.push_back(l);
            }
        }
        std::sort(lBoundsTmp.begin(), lBoundsTmp.end());
        // Add also the maximum value
        lBoundsTmp.push_back(lBoundsTmp.back() + 1.0);

        // Sort tasks to minimize the maximum slowness
        MinSlownessScheduler::TaskProxy::sortMinSlowness(tps, lBoundsTmp);

        // Update the index of the task that sets maximum slowness
        size_t newTaskPos = tps.size() - 1, maxlTaskPos = 0;
        // For each task, calculate its slowness and its tendency
        double e = tps.front().t, maxSlowness = (e - tps.front().r) / tps.front().a, maxTendency = 0.0;
        tps.front().tsum = tps.front().t;
        for (size_t i = 1; i < tps.size(); ++i) {
            double tendency = i < newTaskPos ? 0.0 : 1.0 / tps[i].a;
            if (tps[i].id == -1) {
                newTaskPos = i;
                tendency = -1.0;
                tps[i].tsum = tps[i - 1].tsum;
            } else
                tps[i].tsum = tps[i - 1].tsum + tps[i].t;
            e += tps[i].t;
            double slowness = (e - tps[i].r) / tps[i].a;
            if (slowness > maxSlowness || (slowness == maxSlowness && tendency > maxTendency)) {
                maxSlowness = slowness;
                maxlTaskPos = i;
                maxTendency = tendency;
            }
        }

        // Calculate possible order and maximum changes, and take the nearest one
        MinSlownessScheduler::TaskProxy & tm = tps[maxlTaskPos], & tn = tps[newTaskPos];
        double a, b, c, minA = infinity, curA = tn.a;

        //              std::ostringstream osstmp;
        //              for (size_t i = 0; i < tps.size(); ++i)
        //                      osstmp << tps[i].id << ',';
        //              LogMsg("Ex.RI.Aggr", DEBUG) << "At " << curA << " maxTask is " << tm.id << " and the task order is " << osstmp.str();
        //
        // Calculate next interval based on the task that is setting the current minimum and its value
        // The current minimum slowness task is before the new task
        if (maxlTaskPos < newTaskPos) {
            SubFunction sf(0.0, 0.0, 0.0, (tm.tsum - tm.r) / tm.a);
            if (pieces.empty() || pieces.back().second != sf)
                pieces.push_back(make_pair(tn.a, sf));

            // See at which values of a other tasks mark the minimum slowness
            // The new task
            a = tm.a * tn.tsum / (tm.tsum - tm.a / power - tm.r);
            if (a > curA && a < minA) minA = a;

            // Tasks after the new one
            for (size_t i = newTaskPos + 1; i < tps.size(); ++i) {
                a = (tps[i].a * (tm.tsum - tm.r) / tm.a - tps[i].tsum + tps[i].r) * power;
                if (a > curA && a < minA) minA = a;
            }

            // See at which value of a the new task would change position with its next task
            if (newTaskPos < tps.size() - 1) {
                MinSlownessScheduler::TaskProxy & tn1 = tps[newTaskPos + 1];
                // Solve the equation L(a) = frac(r_{k+1} - r_k)(a - a_{k+1})
                a = tn1.a - tm.a * tn1.r / (tm.tsum - tm.r);
                if (a > curA && a < minA) minA = a;
            }
        }

        // The current minimum slowness task is after the new task
        else if (maxlTaskPos > newTaskPos) {
            SubFunction sf(0.0, 1.0 / (tm.a * power), 0.0, (tm.tsum - tm.r) / tm.a);
            if (pieces.empty() || pieces.back().second != sf)
                pieces.push_back(make_pair(tn.a, sf));

            // See at which values of a other tasks mark the minimum slowness
            // Tasks before the new one
            for (size_t i = 0; i < newTaskPos; ++i) {
                a = (tm.a * (tps[i].tsum - tps[i].r) / tps[i].a - tm.tsum + tm.r) * power;
                if (a > curA && a < minA) minA = a;
            }

            // The new task
            c = tn.tsum * tm.a * power;
            b = (tm.tsum - tm.r) * power - tm.a;
            if (b*b + 4*c >= 0) {
                a = (-b + sqrt(b * b + 4 * c)) / 2.0;
                if (a > curA && a < minA) minA = a;
            }

            // Tasks after the new one
            for (size_t i = newTaskPos + 1; i < tps.size(); ++i) {
                a = ((tm.tsum - tm.r) * tps[i].a - (tps[i].tsum - tps[i].r) * tm.a) * power / (tm.a - tps[i].a);
                if (a > curA && a < minA) minA = a;
            }

            // See at which value of a the new task would change position with its next task
            if (newTaskPos < tps.size() - 1) {
                MinSlownessScheduler::TaskProxy & tn1 = tps[newTaskPos + 1];
                // Solve the second grade equation L(a) = frac(r_{k+1} - r_k)(a - a_{k+1})
                //c = (- tm.a * tn1.r - tn1.a * (tm.tsum - tm.r)) * power;   // c is already negated
                c = (tm.a * tn1.r + tn1.a * (tm.tsum - tm.r)) * power;   // c is already negated
                b = (tm.tsum - tm.r) * power - tn1.a;
                if (b*b + 4*c >= 0) {
                    a = (-b + sqrt(b * b + 4 * c)) / 2.0;
                    if (a > curA && a < minA) minA = a;
                }
            }

            // See at which value of a other tasks change order
            if (!lBounds.empty() && lBounds.back() > maxSlowness) {
                size_t i = 0;
                while (lBounds[i] <= maxSlowness) ++i;
                a = (lBounds[i] * tm.a - tm.tsum + tm.r) * power;
                if (a > curA && a < minA) minA = a;
            }
        }

        // The current minimum slowness task is the new task
        else if (maxlTaskPos == newTaskPos) {
            SubFunction sf(tm.tsum, 0.0, 1.0 / power, 0.0);
            if (pieces.empty() || pieces.back().second != sf)
                pieces.push_back(make_pair(tn.a, sf));

            // See at which values of a other tasks mark the minimum slowness
            // Tasks before the new one
            for (size_t i = 0; i < newTaskPos; ++i) {
                a = tps[i].a * tm.tsum / (tps[i].tsum - tps[i].a / power - tps[i].r);
                if (a > curA && a < minA) minA = a;
            }

            // Tasks after the new one
            for (size_t i = newTaskPos + 1; i < tps.size(); ++i) {
                c = tm.tsum * tps[i].a * power;
                b = (tps[i].tsum - tps[i].r) * power - tps[i].a;
                if (b*b + 4*c >= 0) {
                    a = (-b + sqrt(b * b + 4 * c)) / 2.0;
                    if (a > curA && a < minA) minA = a;
                }
            }

            // See at which value of a the new task would change position with its next task
            if (newTaskPos < tps.size() - 1) {
                MinSlownessScheduler::TaskProxy & tn1 = tps[newTaskPos + 1];
                // Solve the second grade equation L(a) = frac(r_{k+1} - r_k)(a - a_{k+1})
                c = tm.tsum * tn1.a * power;
                b = (tm.tsum - tn1.r) * power - tn1.a;
                if (b*b + 4*c >= 0) {
                    a = (-b + sqrt(b * b + 4 * c)) / 2.0;
                    if (a > curA && a < minA) minA = a;
                }
            }

            // See at which value of a other tasks change order
            if (!lBounds.empty() && lBounds.front() < maxSlowness) {
                size_t i = lBounds.size() - 1;
                while (lBounds[i] >= maxSlowness) --i;
                a = tm.tsum / (lBounds[i] - 1.0 / power);
                if (a > curA && a < minA) minA = a;
            }
        }

        // If no minimum was found, the function ends here
        if (minA == infinity) break;

        // Update new task size
        tn.a = minA + 1.0;
        tn.t = tn.a / power;
        // Put the new task at the end
        if (newTaskPos < tps.size() - 1) {
            MinSlownessScheduler::TaskProxy tmp = tn;
            tn = tps.back();
            tps.back() = tmp;
        }
    }
}


void SlownessInformation::LAFunction::modifyReference(Time oldRef, Time newRef) {
    double difference = (newRef - oldRef).seconds();
    for (vector<std::pair<double, SubFunction> >::iterator next = pieces.begin(), it = next++;
            next != pieces.end(); it = next++) {
        // For every subfunction with x > 0.0, substract (newRef - oldRef)
        if (it->second.x > 0.0)
            it->second.x = it->second.x > difference ? it->second.x - difference : 0.0;
        // Recalculate the limit
        double alpha = (it->second.y - next->second.y);
        double b = it->second.z1 - next->second.z1 + it->second.z2 - next->second.z2;
        double c = it->second.x - next->second.x;
        if (alpha == 0.0) {
            if (b != 0)
                next->first = -c / b + 1.0;
        } else if (b*b - 4.0*alpha*c >= 0) {
            if (alpha < 0.0)
                next->first = (-b - std::sqrt(b * b - 4.0 * alpha * c)) / (2.0 * alpha) + 1.0;
            else
                next->first = (-b + std::sqrt(b * b - 4.0 * alpha * c)) / (2.0 * alpha) + 1.0;
        }
        // Calculate the limit only if the function was continuous?
    }
    if (pieces.back().second.x > 0.0)
        pieces.back().second.x = pieces.back().second.x > difference ? pieces.back().second.x - difference : 0.0;
}


template<int numF, class S> void SlownessInformation::LAFunction::stepper(const LAFunction * (&f)[numF], S & step) {
    // PRE: f size is numF, and numF >= 2, all functions have at least one piece
    double edges[4] = { minTaskLength, 0.0, 0.0, 0.0 };
    double & s = edges[0], e;
    vector<std::pair<double, SubFunction> >::const_iterator cur[numF], next[numF];

    for (unsigned int i = 0; i < numF; ++i) {
        next[i] = f[i]->pieces.begin();
        cur[i] = next[i]++;
    }

    while (s < infinity) {
        // Look for next task length and function
        unsigned int nextF = 0;
        e = infinity;
        for (unsigned int i = 0; i < numF; ++i) {
            if (next[i] != f[i]->pieces.end() && next[i]->first < e) {
                e = next[i]->first;
                nextF = i;
            }
        }

        if (e > s) {
            // Check crossing points between f[0] and f[1]
            double a = cur[0]->second.y - cur[1]->second.y;
            double b = cur[0]->second.z1 - cur[1]->second.z1 + cur[0]->second.z2 - cur[1]->second.z2;
            double c = cur[0]->second.x - cur[1]->second.x;
            int numEdges = 1;
            if (a == 0) {
                if (b != 0) {
                    double cp = -c / b;
                    if (cp > s && cp < e)
                        edges[numEdges++] = cp;
                }
            } else if (b == 0) {
                double cp = -c / a;
                if (cp > s*s && cp < e*e)
                    edges[numEdges++] = sqrt(cp);
            } else {
                double square = b * b - 4.0 * a * c;
                if (square == 0) {
                    double cp = -b / (2.0 * a);
                    if (cp > s && cp < e)
                        edges[numEdges++] = cp;
                } else if (square > 0) {
                    double cp1 = (-b + sqrt(square)) / (2.0 * a);
                    double cp2 = (-b - sqrt(square)) / (2.0 * a);
                    if (cp1 > cp2) {
                        // swap them
                        cp1 += cp2;  // cp1' = cp1 + cp2
                        cp2 = cp1 - cp2; // cp2' = cp1' - cp2 = cp1
                        cp1 -= cp2; // cp1'' = cp1' - cp1 = cp2
                    }
                    if (cp1 > s && cp1 < e)
                        edges[numEdges++] = cp1;
                    if (cp2 > s && cp2 < e)
                        edges[numEdges++] = cp2;
                }
            }
            edges[numEdges++] = e;
            for (int i = 1; i < numEdges; ++i) {
                double mid = edges[i] < infinity ? (edges[i - 1] + edges[i]) / 2.0 : edges[i - 1] + 1000.0;
                step(edges[i - 1], edges[i], cur, c / mid + a * mid + b > 0 ? 0 : 1);
            }
        }
        s = e;
        // Advance iterator
        cur[nextF] = next[nextF]++;
    }
}


struct SlownessInformation::LAFunction::minStep {
    void operator()(double a, double b, const vector<std::pair<double, SubFunction> >::const_iterator f[2], int max) {
        if (pieces.empty() || pieces.back().second != f[max ^ 1]->second)
            pieces.push_back(make_pair(a, f[max ^ 1]->second));
    }

    std::vector<std::pair<double, SlownessInformation::LAFunction::SubFunction> > pieces;
};


void SlownessInformation::LAFunction::min(const LAFunction & l, const LAFunction & r) {
    minStep step;
    const LAFunction * f[] = {&l, &r};
    stepper(f, step);
    pieces.swap(step.pieces);
}


struct SlownessInformation::LAFunction::maxStep {
    void operator()(double a, double b, const vector<std::pair<double, SubFunction> >::const_iterator f[2], int max) {
        if (pieces.empty() || pieces.back().second != f[max]->second)
            pieces.push_back(make_pair(a, f[max]->second));
    }

    std::vector<std::pair<double, SlownessInformation::LAFunction::SubFunction> > pieces;
};


void SlownessInformation::LAFunction::max(const LAFunction & l, const LAFunction & r) {
    maxStep step;
    const LAFunction * f[] = {&l, &r};
    stepper(f, step);
    pieces.swap(step.pieces);
}


struct SlownessInformation::LAFunction::maxDiffStep {
    maxDiffStep(unsigned int lv, unsigned int rv) {
        val[0] = lv;
        val[1] = rv;
    }

    void operator()(double a, double b, const vector<std::pair<double, SubFunction> >::const_iterator f[4], int max) {
        SubFunction sf(f[2]->second.x + f[3]->second.x + val[max^1] * (f[max]->second.x - f[max ^1]->second.x),
                       f[2]->second.y + f[3]->second.y + val[max^1] * (f[max]->second.y - f[max ^1]->second.y),
                       f[2]->second.z1 + f[3]->second.z1 + val[max^1] * (f[max]->second.z1 - f[max ^1]->second.z1),
                       f[2]->second.z2 + f[3]->second.z2 + val[max^1] * (f[max]->second.z2 - f[max ^1]->second.z2));
        if (pieces.empty() || pieces.back().second != sf)
            pieces.push_back(make_pair(a, sf));
    }

    unsigned int val[2];
    std::vector<std::pair<double, SlownessInformation::LAFunction::SubFunction> > pieces;
};


void SlownessInformation::LAFunction::maxDiff(const LAFunction & l, const LAFunction & r,
        unsigned int lv, unsigned int rv, const LAFunction & maxL, const LAFunction & maxR) {
    maxDiffStep step(lv, rv);
    const LAFunction * f[] = {&l, &r, &maxL, &maxR};
    stepper(f, step);
    pieces.swap(step.pieces);
}


struct SlownessInformation::LAFunction::sqdiffStep {
    sqdiffStep(int lv, int rv, double _ah) : result(0.0), ah(_ah) {
        val[0] = lv;
        val[1] = rv;
    }

    void operator()(double a, double b, const vector<std::pair<double, SubFunction> >::const_iterator f[2], int max) {
        if (b == infinity) b = ah;
        I = max ^ 1;
        u = f[max]->second.x - f[I]->second.x;
        v = f[max]->second.y - f[I]->second.y;
        w = f[max]->second.z1 - f[I]->second.z1 + f[max]->second.z2 - f[I]->second.z2;
        ab = a * b;
        ba = b - a;
        ba2 = b * b - a * a;
        ba3 = b * b * b - a * a * a;
        fracba = b / a;
        // Compute v * int((f - f1)^2)
        double tmp = (u * u / ab + 2.0 * u * v + w * w) * ba + w * v * ba2 + v * v * ba3 / 3.0 + 2.0 * u * w * log(fracba);
        result += val[I] * tmp;
    }

    int val[2];
    int I;
    double result, ah;
    double u, v, w, ab, ba, ba2, ba3, fracba;
};


double SlownessInformation::LAFunction::sqdiff(const LAFunction & r, double ah) const {
    sqdiffStep step(1, 1, ah);
    const LAFunction * f[] = {this, &r};
    stepper(f, step);
    return step.result;
}


struct SlownessInformation::LAFunction::maxAndLossStep {
    maxAndLossStep(unsigned int lv, unsigned int rv, double _ah) : ss(lv, rv, _ah) {}

    void operator()(double a, double b, const vector<std::pair<double, SubFunction> >::const_iterator f[4], int max) {
        ms(a, b, f, max);
        ss(a, b, f, max);
        int lin = 3 - max;
        // Add 2 * v * int((f - f1)*(f1 - max))
        //              double u2 = f[ss.I]->second.x - f[lin]->second.x;
        //              double v2 = f[ss.I]->second.y - f[lin]->second.y;
        //              double w2 = f[ss.I]->second.z - f[lin]->second.z;
        //              double tmp = ss.val[ss.I] * ((ss.u*u2/ss.ab + u2*ss.v + ss.u*v2 + ss.w*w2)*ss.ba + (ss.w*v2 + ss.v*w2)*ss.ba2/2.0 + ss.v*v2*ss.ba3/3.0 + (u2*ss.w + ss.u*w2)*log(ss.fracba));

        // Add 2 * int((f - f1)*(f1 - max_i(f1i)))
        double u2 = f[lin]->second.x;
        double v2 = f[lin]->second.y;
        double w2 = f[lin]->second.z1 + f[lin]->second.z2;
        double tmp = (ss.u * u2 / ss.ab + u2 * ss.v + ss.u * v2 + ss.w * w2) * ss.ba + (ss.w * v2 + ss.v * w2) * ss.ba2 / 2.0 + ss.v * v2 * ss.ba3 / 3.0 + (u2 * ss.w + ss.u * w2) * log(ss.fracba);
        ss.result += 2.0 * tmp;
    }

    sqdiffStep ss;
    maxStep ms;
};


double SlownessInformation::LAFunction::maxAndLoss(const LAFunction & l, const LAFunction & r, unsigned int lv, unsigned int rv,
        const LAFunction & maxL, const LAFunction & maxR, double ah) {
    maxAndLossStep step(lv, rv, ah);
    const LAFunction * f[] = {&l, &r, &maxL, &maxR};
    stepper(f, step);
    pieces.swap(step.ms.pieces);
    return step.ss.result;
}


// A search algorithm to find a good reduced solution
namespace {
struct ResultCost {
    SlownessInformation::LAFunction result;
    double cost;
    ResultCost() {}
    ResultCost(const SlownessInformation::LAFunction & r, double c) : result(r), cost(c) {}
    bool operator<(const ResultCost & r) {
        return cost < r.cost;
    }
};
}


double SlownessInformation::LAFunction::reduceMax(unsigned int v, double ah, unsigned int quality) {
    if (pieces.size() > SlownessInformation::numPieces) {
        const LAFunction * f[] = {NULL, this};
        list<ResultCost> candidates(1);
        candidates.front().cost = 0.0;
        candidates.front().result = *this;
        while (candidates.front().result.pieces.size() > SlownessInformation::numPieces) {
            // Take next candidate and compute possibilities
            vector<std::pair<double, SubFunction> > best;
            best.swap(candidates.front().result.pieces);
            candidates.pop_front();
            for (vector<std::pair<double, SubFunction> >::iterator next = ++best.begin(), cur = next++, prev = best.begin();
                    cur != best.end(); prev = cur, cur = next++) {
                candidates.push_back(ResultCost());
                LAFunction & func = candidates.back().result;
                // Maintain subfunctions from begin to prev - 1
                func.pieces.assign(best.begin(), prev);
                // Join prev with cur
                double a = prev->first, b = cur->first, c = next == best.end() ? ah : next->first;
                double pc = (b - a) / (c - a), cc = (c - b) / (c - a);
                SubFunction join(prev->second.x * pc + cur->second.x * cc,
                                 prev->second.y * pc + cur->second.y * cc,
                                 prev->second.z1 * pc + cur->second.z1 * cc,
                                 prev->second.z2 * pc + cur->second.z2 * cc);
                func.pieces.push_back(make_pair(a, join));
                // Maintain subfunctions from next to end
                func.pieces.insert(func.pieces.end(), next, best.end());
                f[0] = &func;
                sqdiffStep ls(1, 1, ah);
                stepper(f, ls);
                candidates.back().cost = ls.result;
                // Retain only K best candidates, to reduce the exponential explosion of possibilities
                candidates.sort();
                if (candidates.size() > quality)
                    candidates.pop_back();
            }
        }
        pieces.swap(candidates.front().result.pieces);
        return v * candidates.front().cost;
    }
    return 0.0;
}


double SlownessInformation::LAFunction::getSlowness(uint64_t a) const {
    vector<std::pair<double, SubFunction> >::const_iterator next = pieces.begin(), it = next++;
    while (next != pieces.end() && next->first < a) it = next++;
    return it->second.value(a);
}


double SlownessInformation::LAFunction::estimateSlowness(uint64_t a, unsigned int n) const {
    vector<std::pair<double, SubFunction> >::const_iterator next = pieces.begin(), it = next++;
    while (next != pieces.end()) {
        // Recalculate the limit
        double alpha = n * (it->second.y - next->second.y);
        double b = n * (it->second.z1 - next->second.z1) + it->second.z2 - next->second.z2;
        double c = it->second.x - next->second.x;
        double limit = next->first;
        if (alpha == 0.0) {
            if (b != 0)
                limit = -c / b + 1.0;
        } else if (b*b - 4.0*alpha*c >= 0) {
            if (alpha < 0.0)
                limit = (-b - std::sqrt(b * b - 4.0 * alpha * c)) / (2.0 * alpha) + 1.0;
            else
                limit = (-b + std::sqrt(b * b - 4.0 * alpha * c)) / (2.0 * alpha) + 1.0;
        }
        // Calculate the limit only if the function was continuous?
        // If the limit is still before a, advance
        if (limit < a)
            it = next++;
        else break;
    }
    return it->second.value(a, n);
}


void SlownessInformation::LAFunction::update(uint64_t length, unsigned int n) {
    // TODO
}


double SlownessInformation::LAFunction::getSlowestMachine() const {
    double result = 0.0;
    for (size_t i = 0; i < pieces.size(); ++i)
        if (pieces[i].second.z1 > result)
            result = pieces[i].second.z1;
    return result;
}


double SlownessInformation::MDLCluster::distance(const MDLCluster & r, MDLCluster & sum) const {
    sum.aggregate(*this, r);
    double result = 0.0;
    if (reference) {
        if (reference->memRange) {
            double loss = ((double)sum.accumMsq / (sum.value * reference->memRange * reference->memRange));
            if (((minM - reference->minM) * numIntervals / reference->memRange) != ((r.minM - reference->minM) * numIntervals / reference->memRange))
                loss += 100.0;
            result += loss;
        }
        if (reference->diskRange) {
            double loss = ((double)sum.accumDsq / (sum.value * reference->diskRange * reference->diskRange));
            if (((minD - reference->minD) * numIntervals / reference->diskRange) != ((r.minD - reference->minD) * numIntervals / reference->diskRange))
                loss += 100.0;
            result += loss;
        }
        if (reference->slownessRange) {
            double sqrange = reference->slownessRange * reference->slownessRange;
            double loss = (sum.accumLsq / (sum.value * sqrange));
            if ((maxL.sqdiff(reference->minL, reference->lengthHorizon) * numIntervals / sqrange) !=
                    (r.maxL.sqdiff(r.reference->minL, reference->lengthHorizon) * numIntervals / sqrange))
                loss += 100.0;
            result += loss;
        }
    }
    return result;
}


bool SlownessInformation::MDLCluster::far(const MDLCluster & r) const {
    if (reference->memRange) {
        if (((minM - reference->minM) * numIntervals / reference->memRange) != ((r.minM - reference->minM) * numIntervals / reference->memRange))
            return true;
    }
    if (reference->diskRange) {
        if (((minD - reference->minD) * numIntervals / reference->diskRange) != ((r.minD - reference->minD) * numIntervals / reference->diskRange))
            return true;
    }
    // TODO
    //      if (minA.isFree() != r.minA.isFree()) return true;
    //      if (reference->availRange) {
    //              if (floor(minA.sqdiff(reference->minA, reference->aggregationTime, reference->horizon) * numIntervals / reference->availRange)
    //                              != floor(r.minA.sqdiff(reference->minA, reference->aggregationTime, reference->horizon) * numIntervals / reference->availRange)) {
    //                      return true;
    //              }
    //      }
    return false;
}


void SlownessInformation::MDLCluster::aggregate(const MDLCluster & r) {
    aggregate(*this, r);
}


void SlownessInformation::MDLCluster::aggregate(const MDLCluster & l, const MDLCluster & r) {
    LogMsg("Ex.RI.Aggr", DEBUG) << "Aggregating " << *this << " and " << r;
    reference = l.reference;
    // Update minimums/maximums and sum up values
    uint32_t newMinM = l.minM < r.minM ? l.minM : r.minM;
    uint32_t newMinD = l.minD < r.minD ? l.minD : r.minD;
    uint64_t ldm = l.minM - newMinM, rdm = r.minM - newMinM;
    accumMsq = l.accumMsq + l.value * ldm * ldm + 2 * ldm * l.accumMln
               + r.accumMsq + r.value * rdm * rdm + 2 * rdm * r.accumMln;
    accumMln = l.accumMln + l.value * ldm + r.accumMln + r.value * rdm;
    uint64_t ldd = l.minD - newMinD, rdd = r.minD - newMinD;
    accumDsq = l.accumDsq + l.value * ldd * ldd + 2 * ldd * l.accumDln
               + r.accumDsq + r.value * rdd * rdd + 2 * rdd * r.accumDln;
    accumDln = l.accumDln + l.value * ldd + r.accumDln + r.value * rdd;

    LAFunction newMaxL;
    accumLsq = l.accumLsq + r.accumLsq
               + newMaxL.maxAndLoss(l.maxL, r.maxL, l.value, r.value, l.accumMaxL, r.accumMaxL, reference->lengthHorizon);
    //accumMaxL.max(l.accumMaxL, r.accumMaxL);
    accumMaxL.maxDiff(l.maxL, r.maxL, l.value, r.value, l.accumMaxL, r.accumMaxL);

    minM = newMinM;
    minD = newMinD;
    maxL.swap(newMaxL);
    value = l.value + r.value;
    // Reduce the max function to avoid piece explosion
    //accumLsq += value * maxL.reduceMax(reference->lengthHorizon);
}


void SlownessInformation::MDLCluster::reduce() {
    accumLsq += maxL.reduceMax(value, reference->lengthHorizon);
    accumMaxL.reduceMax(1, reference->lengthHorizon);
}


void SlownessInformation::setAvailability(uint32_t m, uint32_t d, const std::list<boost::shared_ptr<Task> > & tasks,
        double power, double minSlowness) {
    minM = maxM = m;
    minD = maxD = d;
    minimumSlowness = maximumSlowness = minSlowness;
    summary.clear();
    summary.pushBack(MDLCluster(this, m, d, tasks, power));
    minL = maxL = summary[0].maxL;
    lengthHorizon = minL.getHorizon();
}


void SlownessInformation::getFunctions(const TaskDescription & req, std::vector<std::pair<LAFunction *, unsigned int> > & f) {
    unsigned int size = summary.getSize();
    for (unsigned int i = 0; i < size; ++i)
        if (summary[i].fulfills(req))
            f.push_back(std::make_pair(&summary[i].maxL, summary[i].value));
}


double SlownessInformation::getSlowestMachine() const {
    return maxL.getSlowestMachine();
}


void SlownessInformation::join(const SlownessInformation & r) {
    if (!r.summary.isEmpty()) {
        LogMsg("Ex.RI.Aggr", DEBUG) << "Aggregating two summaries:";

        if (summary.isEmpty()) {
            // operator= forbidden
            minM = r.minM;
            maxM = r.maxM;
            minD = r.minD;
            maxD = r.maxD;
            minL = r.minL;
            maxL = r.maxL;
            lengthHorizon = r.lengthHorizon;
            minimumSlowness = r.minimumSlowness;
            maximumSlowness = r.maximumSlowness;
            rkref = r.rkref;
            summary.add(r.summary);
        } else {
            if (minM > r.minM) minM = r.minM;
            if (maxM < r.maxM) maxM = r.maxM;
            if (minD > r.minD) minD = r.minD;
            if (maxD < r.maxD) maxD = r.maxD;
            minL.min(minL, r.minL);
            maxL.max(maxL, r.maxL);
            if (lengthHorizon < r.lengthHorizon)
                lengthHorizon = r.lengthHorizon;
            if (minimumSlowness > r.minimumSlowness)
                minimumSlowness = r.minimumSlowness;
            if (maximumSlowness < r.maximumSlowness)
                maximumSlowness = r.maximumSlowness;
            unsigned int rstart = summary.getSize();
            summary.add(r.summary);
            unsigned int size = summary.getSize();
            // Change reference
            if (rkref > r.rkref) {
                for (unsigned int i = rstart; i < size; ++i) {
                    summary[i].maxL.modifyReference(r.rkref, rkref);
                    summary[i].accumMaxL.modifyReference(r.rkref, rkref);
                }
            } else if (rkref < r.rkref) {
                for (unsigned int i = 0; i < rstart; ++i) {
                    summary[i].maxL.modifyReference(rkref, r.rkref);
                    summary[i].accumMaxL.modifyReference(rkref, r.rkref);
                }
                rkref = r.rkref;
            }
        }
        unsigned int size = summary.getSize();
        for (unsigned int i = 0; i < size; i++)
            summary[i].reference = this;
    }
}


void SlownessInformation::updateRkReference(Time newRef) {
    unsigned int size = summary.getSize();
    for (unsigned int i = 0; i < size; i++) {
        summary[i].maxL.modifyReference(rkref, newRef);
        summary[i].accumMaxL.modifyReference(rkref, newRef);
    }
    rkref = newRef;
}


void SlownessInformation::reduce() {
    unsigned int size = summary.getSize();
    for (unsigned int i = 0; i < size; i++)
        summary[i].reference = this;
    // Set up clustering variables
    memRange = maxM - minM;
    diskRange = maxD - minD;
    slownessRange = maxL.sqdiff(minL, lengthHorizon);
    summary.clusterize(numClusters);
    for (unsigned int i = 0; i < size; i++)
        summary[i].reduce();
}


void SlownessInformation::output(std::ostream & os) const {
    os << minimumSlowness << "s/i, ";
    os << '(' << minM << "MB, " << maxM << "MB) ";
    os << '(' << minD << "MB, " << maxD << "MB) ";
    os << '(' << minL << ", " << maxL << ") (";
    for (unsigned int i = 0; i < summary.getSize(); i++)
        os << summary[i] << ',';
    os << ')';
}


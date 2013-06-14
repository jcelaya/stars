/*
 *  STaRS, Scalable Task Routing approach to distributed Scheduling
 *  Copyright (C) 2013 Javier Celaya
 *
 *  This file is part of STaRS.
 *
 *  STaRS is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  STaRS is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with STaRS; if not, see <http://www.gnu.org/licenses/>.
 */

#include <utility>
#include <list>
#include "LAFunction.hpp"
#include "Logger.hpp"
using std::vector;
using std::make_pair;

namespace stars {

unsigned int LAFunction::numPieces = 64;


LAFunction::LAFunction(TaskProxy::List curTasks,
        const std::vector<double> & switchValues, double power) {
    // Trivial case
    if (curTasks.empty()) {
        LogMsg("Ex.RI.Aggr", DEBUG) << "Creating availability info for empty queue and power " << power;
        pieces.push_back(make_pair(minTaskLength, SubFunction(0.0, 0.0, 1.0 / power, 0.0)));
        return;
    }

    // General case
    LogMsg("Ex.RI.Aggr", DEBUG) << "Creating availability info for " << curTasks.size() << " tasks and power " << power;
    Time now = Time::getCurrentTime();

    // The new task
    curTasks.push_back(TaskProxy(minTaskLength, power, now));
    for (auto i = curTasks.begin(); i != curTasks.end(); ++i)
        i->r = (i->rabs - now).seconds();

    while(true) {
        // Order the queue and calculate minimum slowness
        // The new task is at the end of the queue
        vector<double> svCur(switchValues);
        if (!svCur.empty()) {
            // Add order change values for the new task
            for (auto i = ++curTasks.begin(); i != curTasks.end(); ++i) {
                // The new task is the last in the vector
                if (i->a != curTasks.back().a) {
                    double l = i->r / (curTasks.back().a - i->a);
                    if (l > svCur.front())
                        svCur.push_back(l);
                }
            }
            std::sort(svCur.begin(), svCur.end());
            // Remove duplicate values
            svCur.erase(std::unique(svCur.begin(), svCur.end()), svCur.end());

            // Sort tasks to minimize the maximum slowness
            curTasks.sortMinSlowness(svCur);
        }

        // Update the index of the task that sets maximum slowness
        TaskProxy::List::iterator tn, tm = curTasks.begin();
        // For each task, calculate its slowness and its tendency
        double e = curTasks.front().t, maxSlowness = (e - curTasks.front().r) / curTasks.front().a, maxTendency = 0.0;
        curTasks.front().tsum = curTasks.front().t;
        bool beforeNewTask = true, minBeforeNew = true;
        for (TaskProxy::List::iterator i = curTasks.begin(), prev = i++; i != curTasks.end(); prev = i++) {
            double tendency = beforeNewTask ? 0.0 : 1.0 / i->a;
            if (i->id == (unsigned int)-1) {
                tn = i;
                tendency = -1.0;
                i->tsum = prev->tsum;
                beforeNewTask = false;
            } else
                i->tsum = prev->tsum + i->t;
            e += i->t;
            double slowness = (e - i->r) / i->a;
            if (slowness > maxSlowness || (slowness == maxSlowness && tendency > maxTendency)) {
                maxSlowness = slowness;
                tm = i;
                minBeforeNew = beforeNewTask;
                maxTendency = tendency;
            }
        }

        // Calculate possible order and maximum changes, and take the nearest one
        double a, b, c, minA = INFINITY, curA = tn->a;
        TaskProxy::List::iterator tn1 = tn;
        ++tn1;

        // Calculate next interval based on the task that is setting the current minimum and its value
        // The current minimum slowness task is the new task
        if (tm == tn) {
            SubFunction sf(tm->tsum, 0.0, 1.0 / power, 0.0);
            if (pieces.empty() || pieces.back().second != sf)
                pieces.push_back(make_pair(tn->a, sf));

            // See at which values of a other tasks mark the minimum slowness
            // Tasks before the new one
            for (auto i = curTasks.begin(); i != tn; ++i) {
                a = i->a * tm->tsum / (i->tsum - i->a / power - i->r);
                if (a > curA && a < minA) minA = a;
            }

            // Tasks after the new one
            for (auto i = tn1; i != curTasks.end(); ++i) {
                c = tm->tsum * i->a * power;
                b = (i->tsum - i->r) * power - i->a;
                if (b*b + 4*c >= 0) {
                    a = (-b + sqrt(b*b + 4*c)) / 2.0;
                    if (a > curA && a < minA) minA = a;
                }
            }

            // See at which value of a the new task would change position with its next task
            if (tn1 != curTasks.end()) {
                // Solve the second grade equation L(a) = frac(r_{k+1} - r_k)(a - a_{k+1})
                c = tm->tsum * tn1->a * power;
                b = (tm->tsum - tn1->r) * power - tn1->a;
                if (b*b + 4*c >= 0) {
                    a = (-b + sqrt(b*b + 4*c)) / 2.0;
                    if (a > curA && a < minA) minA = a;
                }
            }

            // See at which value of a other tasks change order
            if (!svCur.empty() && svCur.front() < maxSlowness) {
                size_t i = svCur.size() - 1;
                while (svCur[i] >= maxSlowness) --i;
                a = tm->tsum / (svCur[i] - 1.0 / power);
                if (a > curA && a < minA) minA = a;
            }
        }

        // The current minimum slowness task is before the new task
        else if (minBeforeNew) {
            SubFunction sf(0.0, 0.0, 0.0, (tm->tsum - tm->r) / tm->a);
            if (pieces.empty() || pieces.back().second != sf)
                pieces.push_back(make_pair(tn->a, sf));

            // See at which values of a other tasks mark the minimum slowness
            // The new task
            a = tm->a * tn->tsum / (tm->tsum - tm->a / power - tm->r);
            if (a > curA && a < minA) minA = a;

            // Tasks after the new one
            for (auto i = tn1; i != curTasks.end(); ++i) {
                a = (i->a * (tm->tsum - tm->r) / tm->a - i->tsum + i->r) * power;
                if (a > curA && a < minA) minA = a;
            }

            // See at which value of a the new task would change position with its next task
            if (tn1 != curTasks.end()) {
                // Solve the equation L(a) = frac(r_{k+1} - r_k)(a - a_{k+1})
                a = tn1->a - tm->a * tn1->r / (tm->tsum - tm->r);
                if (a > curA && a < minA) minA = a;
            }
        }

        // The current minimum slowness task is after the new task
        else {
            SubFunction sf(0.0, 1.0 / (tm->a * power), 0.0, (tm->tsum - tm->r) / tm->a);
            if (pieces.empty() || pieces.back().second != sf)
                pieces.push_back(make_pair(tn->a, sf));

            // See at which values of a other tasks mark the minimum slowness
            // Tasks before the new one
            for (auto i = curTasks.begin(); i != tn; ++i) {
                a = (tm->a * (i->tsum - i->r) / i->a - tm->tsum + tm->r) * power;
                if (a > curA && a < minA) minA = a;
            }

            // The new task
            c = tn->tsum * tm->a * power;
            b = (tm->tsum - tm->r) * power - tm->a;
            if (b*b + 4*c >= 0) {
                a = (-b + sqrt(b*b + 4*c)) / 2.0;
                if (a > curA && a < minA) minA = a;
            }

            // Tasks after the new one
            for (auto i = tn1; i != curTasks.end(); ++i) {
                a = ((tm->tsum - tm->r) * i->a - (i->tsum - i->r) * tm->a) * power / (tm->a - i->a);
                if (a > curA && a < minA) minA = a;
            }

            // See at which value of a the new task would change position with its next task
            if (tn1 != curTasks.end()) {
                // Solve the second grade equation L(a) = frac(r_{k+1} - r_k)(a - a_{k+1})
                c = (tm->a * tn1->r + tn1->a * (tm->tsum - tm->r)) * power;   // c is already negated
                b = (tm->tsum - tm->r) * power - tn1->a;
                if (b*b + 4*c >= 0) {
                    a = (-b + sqrt(b*b + 4*c)) / 2.0;
                    if (a > curA && a < minA) minA = a;
                }
            }

            // See at which value of a other tasks change order
            if (!svCur.empty() && svCur.back() > maxSlowness) {
                size_t i = 0;
                while (svCur[i] <= maxSlowness) ++i;
                a = (svCur[i] * tm->a - tm->tsum + tm->r) * power;
                if (a > curA && a < minA) minA = a;
            }
        }

        // If no minimum was found, the function ends here
        if (minA == INFINITY) break;

        // Update new task size
        tn->a = minA + 1.0;
        tn->t = tn->a / power;
        // Put the new task at the end
        if (tn1 != curTasks.end()) {
            curTasks.splice(curTasks.end(), curTasks, tn);
        }
    }
}


struct StepInformation {
    double * edges;
    LAFunction::PieceVector::const_iterator * f;
    int max;
};

template<int numF, typename Func> void LAFunction::stepper(const LAFunction * (&f)[numF], Func step) {
    // PRE: f size is numF, and numF >= 2, all functions have at least one piece
    double edges[4] = { minTaskLength, 0.0, 0.0, 0.0 };
    double & s = edges[0], e;
    StepInformation si;
    PieceVector::const_iterator cur[numF], next[numF];
    si.f = cur;

    for (unsigned int i = 0; i < numF; ++i) {
        next[i] = f[i]->pieces.begin();
        cur[i] = next[i]++;
    }

    while (s < INFINITY) {
        // Look for next task length and function
        unsigned int nextF = 0;
        e = INFINITY;
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
                double square = b*b - 4.0*a*c;
                if (square == 0) {
                    double cp = -b / (2.0*a);
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
            for (si.edges = edges; si.edges < edges + numEdges - 1; ++si.edges) {
                double mid = si.edges[1] < INFINITY ? (si.edges[0] + si.edges[1]) / 2.0 : si.edges[0] + 1000.0;
                si.max = c / mid + a * mid + b > 0 ? 0 : 1;
                step(si);
            }
        }
        s = e;
        // Advance iterator
        cur[nextF] = next[nextF]++;
    }
}


void LAFunction::min(const LAFunction & l, const LAFunction & r) {
    const LAFunction * functions[] = {&l, &r};
    PieceVector somePieces;
    stepper(functions, [&] (const StepInformation & si) {
        if (somePieces.empty() || somePieces.back().second != si.f[si.max ^ 1]->second)
            somePieces.push_back(make_pair(si.edges[0], si.f[si.max ^ 1]->second));
    } );
    pieces = std::move(somePieces);
}


void LAFunction::max(const LAFunction & l, const LAFunction & r) {
    const LAFunction * functions[] = {&l, &r};
    PieceVector somePieces;
    stepper(functions, [&] (const StepInformation & si) {
        if (somePieces.empty() || somePieces.back().second != si.f[si.max]->second)
            somePieces.push_back(make_pair(si.edges[0], si.f[si.max]->second));
    } );
    pieces = std::move(somePieces);
}


void LAFunction::maxDiff(const LAFunction & l, const LAFunction & r,
        unsigned int lv, unsigned int rv, const LAFunction & maxL, const LAFunction & maxR) {
    unsigned int val[2] = { lv, rv };
    const LAFunction * functions[] = {&l, &r, &maxL, &maxR};
    PieceVector somePieces;
    stepper(functions, [&] (const StepInformation & si) {
        SubFunction sf(si.f[2]->second.x + si.f[3]->second.x + val[si.max^1] * (si.f[si.max]->second.x - si.f[si.max ^1]->second.x),
                si.f[2]->second.y + si.f[3]->second.y + val[si.max^1] * (si.f[si.max]->second.y - si.f[si.max ^1]->second.y),
                si.f[2]->second.z1 + si.f[3]->second.z1 + val[si.max^1] * (si.f[si.max]->second.z1 - si.f[si.max ^1]->second.z1),
                si.f[2]->second.z2 + si.f[3]->second.z2 + val[si.max^1] * (si.f[si.max]->second.z2 - si.f[si.max ^1]->second.z2));
        if (somePieces.empty() || somePieces.back().second != sf)
            somePieces.push_back(make_pair(si.edges[0], sf));
    } );
    pieces = std::move(somePieces);
}


struct sqdiffStep {
    sqdiffStep(int lv, int rv, double _ah) : val{ lv, rv }, result(0.0), ah(_ah) {}

    void operator()(const StepInformation & si) {
        double b = si.edges[1] == INFINITY ? ah : si.edges[1];
        I = si.max ^ 1;
        u = si.f[si.max]->second.x - si.f[I]->second.x;
        v = si.f[si.max]->second.y - si.f[I]->second.y;
        w = si.f[si.max]->second.z1 - si.f[I]->second.z1 + si.f[si.max]->second.z2 - si.f[I]->second.z2;
        ab = si.edges[0]*b;
        ba = b - si.edges[0];
        ba2 = b*b - si.edges[0]*si.edges[0];
        ba3 = b*b*b - si.edges[0]*si.edges[0]*si.edges[0];
        fracba = b/si.edges[0];
        // Compute v * int((f - f1)^2)
        double tmp = (u*u/ab + 2.0*u*v + w*w)*ba + w*v*ba2 + v*v*ba3/3.0 + 2.0*u*w*log(fracba);
        result += val[I] * tmp;
    }

    int val[2];
    int I;
    double result, ah;
    double u, v, w, ab, ba, ba2, ba3, fracba;
};


double LAFunction::sqdiff(const LAFunction & r, double ah) const {
    sqdiffStep step(1, 1, ah);
    const LAFunction * functions[] = {this, &r};
    stepper(functions, [&] (const StepInformation & si) {
        step(si);
    } );
    return step.result;
}


double LAFunction::maxAndLoss(const LAFunction & l, const LAFunction & r, unsigned int lv, unsigned int rv,
        const LAFunction & maxL, const LAFunction & maxR, double ah) {
    sqdiffStep ss(lv, rv, ah);
    const LAFunction * functions[] = {&l, &r, &maxL, &maxR};
    PieceVector somePieces;
    stepper(functions, [&] (const StepInformation & si) {
        if (somePieces.empty() || somePieces.back().second != si.f[si.max]->second)
            somePieces.push_back(make_pair(si.edges[0], si.f[si.max]->second));

        ss(si);
        int lin = 3 - si.max;
        // Add 2 * v * int((f - f1)*(f1 - max))
//		double u2 = f[ss.I]->second.x - f[lin]->second.x;
//		double v2 = f[ss.I]->second.y - f[lin]->second.y;
//		double w2 = f[ss.I]->second.z - f[lin]->second.z;
//		double tmp = ss.val[ss.I] * ((ss.u*u2/ss.ab + u2*ss.v + ss.u*v2 + ss.w*w2)*ss.ba + (ss.w*v2 + ss.v*w2)*ss.ba2/2.0 + ss.v*v2*ss.ba3/3.0 + (u2*ss.w + ss.u*w2)*log(ss.fracba));

        // Add 2 * int((f - f1)*(f1 - max_i(f1i)))
        double u2 = si.f[lin]->second.x;
        double v2 = si.f[lin]->second.y;
        double w2 = si.f[lin]->second.z1 + si.f[lin]->second.z2;
        double tmp = (ss.u*u2/ss.ab + u2*ss.v + ss.u*v2 + ss.w*w2)*ss.ba
                + (ss.w*v2 + ss.v*w2)*ss.ba2/2.0 + ss.v*v2*ss.ba3/3.0
                + (u2*ss.w + ss.u*w2)*log(ss.fracba);
        ss.result += 2.0 * tmp;
    } );
    pieces = std::move(somePieces);
    return ss.result;
}


// A search algorithm to find a good reduced solution
struct ResultCost {
    LAFunction result;
    double cost;
    ResultCost() {}
    ResultCost(const LAFunction & r, double c) : result(r), cost(c) {}
    bool operator<(const ResultCost & r) {
        return cost < r.cost;
    }
};

double LAFunction::reduceMax(unsigned int v, double ah, unsigned int quality) {
    if (pieces.size() > numPieces) {
        const LAFunction * functions[] = {NULL, this};
        std::list<ResultCost> candidates(1);
        candidates.front().cost = 0.0;
        candidates.front().result = *this;
        while (candidates.front().result.pieces.size() > numPieces) {
            // Take next candidate and compute possibilities
            PieceVector best;
            best.swap(candidates.front().result.pieces);
            candidates.pop_front();
            for (PieceVector::iterator next = ++best.begin(), cur = next++, prev = best.begin();
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
                functions[0] = &func;
                sqdiffStep ls(1, 1, ah);
                stepper(functions, ls);
                candidates.back().cost = ls.result;
                // Retain only K best candidates, to reduce the exponential explosion of possibilities
                candidates.sort();
                if (candidates.size() > quality)
                    candidates.pop_back();
            }
        }
        pieces = std::move(candidates.front().result.pieces);
        return v * candidates.front().cost;
    }
    return 0.0;
}


double LAFunction::getSlowness(uint64_t a) const {
    PieceVector::const_iterator next = pieces.begin(), it = next++;
    while (next != pieces.end() && next->first < a) it = next++;
    return it->second.value(a);
}


double LAFunction::estimateSlowness(uint64_t a, unsigned int n) const {
    PieceVector::const_iterator next = pieces.begin(), it = next++;
    while (next != pieces.end() && next->first < a) it = next++;
    return it->second.value(a, n);
}


void LAFunction::update(uint64_t length, unsigned int n) {
    // Invalidate?
    pieces.clear();
    pieces.push_back(make_pair((double)minTaskLength, SubFunction(0.0, INFINITY, 0.0, 0.0)));
    // TODO
//    for (size_t i = 0; i < pieces.size(); ++i)
//        pieces[i].second.z2 += length * n * pieces[i].second.y;
}


double LAFunction::getSlowestMachine() const {
    double result = 0.0;
    for (size_t i = 0; i < pieces.size(); ++i)
        if (pieces[i].second.z1 > result)
            result = pieces[i].second.z1;
    return result;
}

} // namespace stars

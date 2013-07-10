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
#include "ZAFunction.hpp"
#include "Logger.hpp"
using std::vector;
using std::make_pair;

namespace stars {

unsigned int ZAFunction::numPieces = 10;


ZAFunction::ZAFunction(FSPTaskList curTasks, double power) {
    // Trivial case
    if (curTasks.empty()) {
        LogMsg("Ex.RI.Aggr", DEBUG) << "Creating availability info for empty queue and power " << power;
        pieces.push_back(SubFunction(minTaskLength, 0.0, 0.0, 1.0 / power, 0.0));
        return;
    }

    // General case
    LogMsg("Ex.RI.Aggr", DEBUG) << "Creating availability info for " << curTasks.size() << " tasks and power " << power;

    // The new task
    curTasks.push_back(TaskProxy(minTaskLength, power, Time::getCurrentTime()));
    curTasks.updateReleaseTime();

    const vector<double> & boundaries = curTasks.getBoundaries();

    while(true) {
        // Order the queue and calculate minimum slowness
        // The new task is at the end of the queue
        vector<double> svCur(boundaries);
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
        FSPTaskList::iterator tn, tm = curTasks.begin();
        // For each task, calculate its slowness and its tendency
        double e = curTasks.front().t, maxSlowness = (e - curTasks.front().r) / curTasks.front().a, maxTendency = 0.0;
        curTasks.front().tsum = curTasks.front().t;
        bool beforeNewTask = true, minBeforeNew = true;
        for (FSPTaskList::iterator i = curTasks.begin(), prev = i++; i != curTasks.end(); prev = i++) {
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
        FSPTaskList::iterator tn1 = tn;
        ++tn1;

        // Calculate next interval based on the task that is setting the current minimum and its value
        // The current minimum slowness task is the new task
        if (tm == tn) {
            SubFunction sf(tn->a, tm->tsum, 0.0, 1.0 / power, 0.0);
            if (pieces.empty() || !sf.extends(pieces.back()))
                pieces.push_back(sf);

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
//            if (tn1 != curTasks.end()) {
//                // Solve the second grade equation L(a) = frac(r_{k+1} - r_k)(a - a_{k+1})
//                c = tm->tsum * tn1->a * power;
//                b = (tm->tsum - tn1->r) * power - tn1->a;
//                if (b*b + 4*c >= 0) {
//                    a = (-b + sqrt(b*b + 4*c)) / 2.0;
//                    if (a > curA && a < minA) minA = a;
//                }
//            }

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
            SubFunction sf(tn->a, 0.0, 0.0, 0.0, (tm->tsum - tm->r) / tm->a);
            if (pieces.empty() || !sf.extends(pieces.back()))
                pieces.push_back(sf);

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
            SubFunction sf(tn->a, 0.0, 1.0 / (tm->a * power), 0.0, (tm->tsum - tm->r) / tm->a);
            if (pieces.empty() || !sf.extends(pieces.back()))
                pieces.push_back(sf);

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
    ZAFunction::PieceVector::const_iterator * f;
    int max;
};

template<int numF, typename Func> void ZAFunction::stepper(const ZAFunction * (&f)[numF], Func step) {
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
            if (next[i] != f[i]->pieces.end() && next[i]->leftEndpoint < e) {
                e = next[i]->leftEndpoint;
                nextF = i;
            }
        }

        if (e > s) {
            // Check crossing points between f[0] and f[1]
            double a = cur[0]->y - cur[1]->y;
            double b = cur[0]->z1 - cur[1]->z1 + cur[0]->z2 - cur[1]->z2;
            double c = cur[0]->x - cur[1]->x;
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


void ZAFunction::min(const ZAFunction & l, const ZAFunction & r) {
    const ZAFunction * functions[] = {&l, &r};
    PieceVector somePieces;
    stepper(functions, [&] (const StepInformation & si) {
        if (somePieces.empty() || !si.f[si.max ^ 1]->extends(somePieces.back()))
            somePieces.push_back(SubFunction(si.edges[0], *si.f[si.max ^ 1]));
    } );
    pieces = std::move(somePieces);
}


void ZAFunction::max(const ZAFunction & l, const ZAFunction & r) {
    const ZAFunction * functions[] = {&l, &r};
    PieceVector somePieces;
    stepper(functions, [&] (const StepInformation & si) {
        if (somePieces.empty() || !si.f[si.max]->extends(somePieces.back()))
            somePieces.push_back(SubFunction(si.edges[0], *si.f[si.max]));
    } );
    pieces = std::move(somePieces);
}


void ZAFunction::maxDiff(const ZAFunction & l, const ZAFunction & r,
        unsigned int lv, unsigned int rv, const ZAFunction & maxL, const ZAFunction & maxR) {
    unsigned int val[2] = { lv, rv };
    const ZAFunction * functions[] = {&l, &r, &maxL, &maxR};
    PieceVector somePieces;
    stepper(functions, [&] (const StepInformation & si) {
        SubFunction sf(si.edges[0], si.f[2]->x + si.f[3]->x + val[si.max^1] * (si.f[si.max]->x - si.f[si.max ^1]->x),
                si.f[2]->y + si.f[3]->y + val[si.max^1] * (si.f[si.max]->y - si.f[si.max ^1]->y),
                si.f[2]->z1 + si.f[3]->z1 + val[si.max^1] * (si.f[si.max]->z1 - si.f[si.max ^1]->z1),
                si.f[2]->z2 + si.f[3]->z2 + val[si.max^1] * (si.f[si.max]->z2 - si.f[si.max ^1]->z2));
        if (somePieces.empty() || !sf.extends(somePieces.back()))
            somePieces.push_back(sf);
    } );
    pieces = std::move(somePieces);
}


struct sqdiffStep {
    sqdiffStep(int lv, int rv, double _ah) : val{ lv, rv }, result(0.0), ah(_ah) {}

    void operator()(const StepInformation & si) {
        double b = si.edges[1] == INFINITY ? ah : si.edges[1];
        I = si.max ^ 1;
        u = si.f[si.max]->x - si.f[I]->x;
        v = si.f[si.max]->y - si.f[I]->y;
        w = si.f[si.max]->z1 - si.f[I]->z1 + si.f[si.max]->z2 - si.f[I]->z2;
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


double ZAFunction::sqdiff(const ZAFunction & r, double ah) const {
    sqdiffStep step(1, 1, ah);
    const ZAFunction * functions[] = {this, &r};
    stepper(functions, [&] (const StepInformation & si) {
        step(si);
    } );
    return step.result;
}


double ZAFunction::maxAndLoss(const ZAFunction & l, const ZAFunction & r, unsigned int lv, unsigned int rv,
        const ZAFunction & maxL, const ZAFunction & maxR, double ah) {
    sqdiffStep ss(lv, rv, ah);
    const ZAFunction * functions[] = {&l, &r, &maxL, &maxR};
    PieceVector somePieces;
    stepper(functions, [&] (const StepInformation & si) {
        if (somePieces.empty() || !si.f[si.max]->extends(somePieces.back()))
            somePieces.push_back(SubFunction(si.edges[0], *si.f[si.max]));

        ss(si);
        int lin = 3 - si.max;
        // Add 2 * v * int((f - f1)*(f1 - max))
//		double u2 = f[ss.I]->x - f[lin]->x;
//		double v2 = f[ss.I]->y - f[lin]->y;
//		double w2 = f[ss.I]->z - f[lin]->z;
//		double tmp = ss.val[ss.I] * ((ss.u*u2/ss.ab + u2*ss.v + ss.u*v2 + ss.w*w2)*ss.ba + (ss.w*v2 + ss.v*w2)*ss.ba2/2.0 + ss.v*v2*ss.ba3/3.0 + (u2*ss.w + ss.u*w2)*log(ss.fracba));

        // Add 2 * int((f - f1)*(f1 - max_i(f1i)))
        double u2 = si.f[lin]->x;
        double v2 = si.f[lin]->y;
        double w2 = si.f[lin]->z1 + si.f[lin]->z2;
        double tmp = (ss.u*u2/ss.ab + u2*ss.v + ss.u*v2 + ss.w*w2)*ss.ba
                + (ss.w*v2 + ss.v*w2)*ss.ba2/2.0 + ss.v*v2*ss.ba3/3.0
                + (u2*ss.w + ss.u*w2)*log(ss.fracba);
        ss.result += 2.0 * tmp;
    } );
    pieces = std::move(somePieces);
    return ss.result;
}


double ZAFunction::reduceMax(double horizon, unsigned int quality) {
    if (pieces.size() > numPieces) {
        const ZAFunction * functions[] = {NULL, this};
        std::multimap<double, ZAFunction> candidates;
        candidates.insert(std::make_pair(0.0, *this));
        while (candidates.begin()->second.pieces.size() > numPieces) {
            // Take next candidate and compute possibilities
            auto best = candidates.begin();
            vector<ZAFunction> options(best->second.getReductionOptions(horizon));
            candidates.erase(best);
            for (auto & func: options) {
                functions[0] = &func;
                sqdiffStep ls(1, 1, horizon);
                stepper(functions, ls);
                // Retain only K best candidates, to reduce the exponential explosion of possibilities
                if (candidates.size() < quality) {
                    candidates.insert(std::make_pair(ls.result, std::move(func)));
                } else {
                    auto max = --candidates.end();
                    if (ls.result < max->first) {
                        candidates.erase(max);
                        candidates.insert(std::make_pair(ls.result, std::move(func)));
                    }
                }
            }
        }
        pieces = std::move(candidates.begin()->second.pieces);
        return candidates.begin()->first;
    }
    return 0.0;
}


vector<ZAFunction> ZAFunction::getReductionOptions(double horizon) const {
    vector<ZAFunction> result(pieces.size() - 1);
    PieceVector::const_iterator next = ++pieces.begin(), cur = next++, prev = pieces.begin();
    for (auto & func: result) {
        // Maintain subfunctions from begin to prev - 1
        func.pieces.assign(pieces.begin(), prev);
        // Join prev with cur
        func.pieces.push_back(SubFunction(*prev, *cur, next == pieces.end() ? horizon : next->leftEndpoint));
        // Maintain subfunctions from next to end
        func.pieces.insert(func.pieces.end(), next, pieces.end());
        prev = cur;
        cur = next++;
    }
    return result;
}


ZAFunction::SubFunction::SubFunction(const SubFunction & l, const SubFunction & r, double rightEndpoint) {
    double a[] = { l.leftEndpoint, r.leftEndpoint, rightEndpoint };
    double b[] = { l.value(l.leftEndpoint), l.value(r.leftEndpoint), r.value(rightEndpoint) };
    if (b[1] < r.value(r.leftEndpoint)) {
        b[1] = r.value(r.leftEndpoint);
    }
    fromThreePoints(a, b);
    if (!isBiggerThan(l, r, rightEndpoint)) {
        a[1] = a[0];
        b[1] = l.slope(a[0]);
        fromTwoPointsAndSlope(a, b);
        if (!isBiggerThan(l, r, rightEndpoint)) {
            a[1] = a[2];
            b[1] = r.slope(a[2]);
            fromTwoPointsAndSlope(a, b);
        }
    }
//    leftEndpoint = l.leftEndpoint;
//    double a = l.leftEndpoint, b = r.leftEndpoint, c = rightEndpoint;
//    double pc = (b - a) / (c - a), cc = (c - b) / (c - a);
//    x = l.x * pc + r.x * cc;
//    y = l.y * pc + r.y * cc;
//    z1 = l.z1 * pc + r.z1 * cc;
//    z2 = l.z2 * pc + r.z2 * cc;
}


void ZAFunction::SubFunction::fromThreePoints(double a[3], double b[3]) {
    leftEndpoint = a[0];
    x = (b[2] - b[0] - (b[1] - b[0])*(a[2] - a[0])/(a[1] - a[0])) * a[0]*a[1]*a[2] / ((a[1] - a[2]) * (a[0] - a[2]));
    y = (b[1] - b[0]) / (a[1] - a[0]) + x / (a[0]*a[1]);
    z1 = b[0] - a[0]*y - x/a[0];
    z2 = 0.0; // TODO: This is a conservative option
}


void ZAFunction::SubFunction::fromTwoPointsAndSlope(double a[3], double b[3]) {
    double bprima = b[1];
    bool leftTangent = a[0] == a[1];
    leftEndpoint = a[0];
    x = (b[2] - b[0] - (a[2] - a[0])*bprima) * (leftTangent ? a[0]*a[0]*a[2] : -a[2]*a[2]*a[0]) / ((a[0] - a[2])*(a[0] - a[2]));
    y = bprima + x / (leftTangent ? a[0]*a[0] : a[2]*a[2]);
    z1 = b[0] - a[0]*y - x/a[0];
    z2 = 0.0; // TODO
}


bool ZAFunction::SubFunction::isBiggerThan(const SubFunction & l, const SubFunction & r, double rightEndpoint) const {
    // Pre: this function touches l at l.leftEndpoint and r at rightEndpoint
    double b2 = value(r.leftEndpoint) * 1.00001;  // Some margin...
    bool midPoint = b2 >= l.value(r.leftEndpoint) && b2 >= r.value(r.leftEndpoint);
    double b1prima = slope(l.leftEndpoint);
    b1prima += abs(b1prima) * 0.00001;
    double lb1prima = l.slope(l.leftEndpoint);
    double b3prima = slope(rightEndpoint);
    b3prima += abs(b3prima) * 0.00001;
    double rb3prima = r.slope(rightEndpoint);
    bool slopes = b1prima >= lb1prima && b3prima <= rb3prima;
    return midPoint && slopes;
}


double ZAFunction::getSlowness(uint64_t a) const {
    PieceVector::const_iterator next = pieces.begin(), it = next++;
    while (next != pieces.end() && next->covers(a)) it = next++;
    return it->value(a);
}


double ZAFunction::estimateSlowness(uint64_t a, unsigned int n) const {
    PieceVector::const_iterator next = pieces.begin(), it = next++;
    while (next != pieces.end() && next->covers(a)) it = next++;
    return it->value(a, n);
}


void ZAFunction::update(uint64_t length, unsigned int n) {
    // FIXME: Invalidate?
    pieces.clear();
    pieces.push_back(SubFunction(minTaskLength, 0.0, INFINITY, 0.0, 0.0));
    // TODO
//    for (size_t i = 0; i < pieces.size(); ++i)
//        pieces[i].z2 += length * n * pieces[i].y;
}


double ZAFunction::getSlowestMachine() const {
    double result = 0.0;
    for (auto & i: pieces)
        if (i.z1 > result)
            result = i.z1;
    return result;
}

} // namespace stars

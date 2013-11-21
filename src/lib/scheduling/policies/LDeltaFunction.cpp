/*  STaRS, Scalable Task Routing approach to distributed Scheduling
 *  Copyright (C) 2012 Javier Celaya
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

#include "LDeltaFunction.hpp"
#include "Logger.hpp"

namespace stars {

unsigned int LDeltaFunction::numPieces = 10;


LDeltaFunction::LDeltaFunction(double power, const std::list<std::shared_ptr<Task> > & queue) : slope(power) {
    if (!queue.empty()) {
        Time now = Time::getCurrentTime();
        std::list<Time> endpoints;

        if (queue.size() > 1) {
            Time nextStart = queue.back()->getDescription().getDeadline();
            endpoints.push_front(nextStart);
            // Calculate the estimated ending time for each scheduled task
            auto firstTask = --queue.rend();
            for (auto i = queue.rbegin(); i != firstTask; ++i) {
                // If there is time between tasks, add a new hole
                if ((*i)->getDescription().getDeadline() < nextStart) {
                    endpoints.push_front(nextStart);
                    endpoints.push_front((*i)->getDescription().getDeadline());
                    nextStart = (*i)->getDescription().getDeadline() - (*i)->getEstimatedDuration();
                } else {
                    nextStart = nextStart - (*i)->getEstimatedDuration();
                }
            }
            endpoints.push_front(nextStart);
        }
        // First task is special, as it is not preemptible
        endpoints.push_front(now + queue.front()->getEstimatedDuration());

        // For each point, calculate its availability
        double avail = 0.0;
        for (std::list<Time>::iterator B = endpoints.begin(), A = B++; B != endpoints.end(); ++B, A = B++) {
            points.push_back(FlopsBeforeDelta(*A, avail));
            avail += (*B - *A).seconds() * power;
            points.push_back(FlopsBeforeDelta(*B, avail));
        }
        points.push_back(FlopsBeforeDelta(endpoints.back(), avail));
    }
}


template<int numF, class S> void LDeltaFunction::stepper(const LDeltaFunction * (&f)[numF],
        const Time & ref, const Time & h, S & step) {
    // PRE: f size is numF, and numF >= 2
    Time a = ref, b;
    PieceVector::const_iterator it[numF];
    double m[numF], fa[numF];
    FlopsBeforeDelta lastPoint[numF];

    for (unsigned int i = 0; i < numF; i++) {
        it[i] = f[i]->points.begin();
        if (it[i] != f[i]->points.end() && it[i]->first < a) a = it[i]->first;
        fa[i] = 0.0;
        m[i] = it[i] != f[i]->points.end() ? 0.0 : f[i]->slope;
    }
    for (unsigned int i = 0; i < numF; i++) {
        lastPoint[i].first = a;
        lastPoint[i].second = 0;
    }

    while (a < h) {
        // Look for next time and function
        unsigned int next = 0;
        b = h;
        for (unsigned int i = 0; i < numF; i++) {
            if (it[i] != f[i]->points.end() && it[i]->first < b) {
                b = it[i]->first;
                next = i;
            }
        }

        if (b > a) {
            double fb0 = lastPoint[0].second + m[0] * (b - lastPoint[0].first).seconds();
            double fb1 = lastPoint[1].second + m[1] * (b - lastPoint[1].first).seconds();
            // Check crossings, fb must be ordered
            if ((fb0 - fb1) * (fa[0] - fa[1]) < 0.0) {
                // If they cross, use two segments
                int order = fa[0] < fa[1] || (fa[0] == fa[1] && m[0] < m[1]) ? 0 : 1;
                Time bb = a + Duration((fa[1] - fa[0]) / (m[0] - m[1]));
                if (bb > a) {
                    step(a, bb, fa, m, order);
                    // Update values
                    for (unsigned int i = 0; i < numF; i++) {
                        fa[i] = lastPoint[i].second + m[i] * (bb - lastPoint[i].first).seconds();
                    }
                }
                if (b > bb) {
                    step(bb, b, fa, m, order ^ 1);
                    // Update values
                    for (unsigned int i = 0; i < numF; i++) {
                        fa[i] = lastPoint[i].second + m[i] * (b - lastPoint[i].first).seconds();
                    }
                }
            } else {
                step(a, b, fa, m, fa[0] < fa[1] || (fa[0] == fa[1] && m[0] < m[1]) ? 0 : 1);
                // Update values
                for (unsigned int i = 0; i < numF; i++) {
                    fa[i] = lastPoint[i].second + m[i] * (b - lastPoint[i].first).seconds();
                }
            }
        }
        a = b;
        // Advance iterator
        if (it[next] != f[next]->points.end()) {
            lastPoint[next] = *it[next];
            fa[next] = lastPoint[next].second;
            it[next]++;
        }
        m[next] = it[next] == f[next]->points.end() ? f[next]->slope : (it[next]->second - fa[next]) / (it[next]->first - a).seconds();
    }
}


namespace TCISteps {
struct minStep {
    LDeltaFunction::PieceVector points;
    double mm;
    double lasty;
    void operator()(Time a, Time b, double fa[2], double m[2], int i) {
        if (mm != m[i]) {
            points.push_back(LDeltaFunction::FlopsBeforeDelta(a, fa[i]));
            mm = m[i];
        }
        lasty = fa[i] + m[i] * (b - a).seconds();
    }
    minStep(unsigned int maxPoints) : mm(0.0), lasty(0.0) {
        points.reserve(maxPoints);
    }
};
}

void LDeltaFunction::min(const LDeltaFunction & l, const LDeltaFunction & r) {
    if (!l.points.empty() || !r.points.empty()) {
        Time horizon = l.points.empty() ? r.points.back().first : l.points.back().first,
                       ct = Time::getCurrentTime();
        TCISteps::minStep ms(2 * (l.points.size() > r.points.size() ? l.points.size() : r.points.size()));
        // Enough space for all the points
        const LDeltaFunction * f[] = {&l, &r};
        stepper(f, ct, horizon, ms);
        ms.points.push_back(FlopsBeforeDelta(horizon, ms.lasty));
        points.swap(ms.points);
        points.reserve(points.size());
    }
    // Trivial case
    slope = l.slope < r.slope ? l.slope : r.slope;
}


namespace TCISteps {
struct maxStep {
    LDeltaFunction::PieceVector points;
    double mm;
    double lasty;
    void operator()(Time a, Time b, double fa[2], double m[2], int i) {
        i = i ^ 1;
        if (mm != m[i]) {
            points.push_back(LDeltaFunction::FlopsBeforeDelta(a, fa[i]));
            mm = m[i];
        }
        lasty = fa[i] + m[i] * (b - a).seconds();
    }
    maxStep(unsigned int maxPoints) : mm(0.0), lasty(0.0) {
        points.reserve(maxPoints);
    }
};
}

void LDeltaFunction::max(const LDeltaFunction & l, const LDeltaFunction & r) {
    if (!l.points.empty() || !r.points.empty()) {
        Time horizon = l.points.empty() ? r.points.back().first : l.points.back().first,
                       ct = Time::getCurrentTime();
        // Enough space for all the points
        TCISteps::maxStep ms(2 * (l.points.size() > r.points.size() ? l.points.size() : r.points.size()));
        const LDeltaFunction * f[] = {&l, &r};
        stepper(f, ct, horizon, ms);
        ms.points.push_back(FlopsBeforeDelta(horizon, ms.lasty));
        points.swap(ms.points);
        points.reserve(points.size());
    }
    // Trivial case
    slope = l.slope < r.slope ? r.slope : l.slope;
}


namespace TCISteps {
struct sqdiffStep {
    double result;
    unsigned int v[2];
    double dt, n1, n2, K, cta;
    Time ref;
    void operator()(Time a, Time b, double fa[2], double m[2], int i) {
        int I = i ^ 1;
        n1 = fa[I] - fa[i];
        n2 = m[I] - m[i];
        dt = (b - a).seconds();
        // Calculate v*int((f(x) - f1(x))/(x - ct))^2
//        if (n1 == 0.0 && n2 == 0.0) return;
//        // Avoid division by zero
//        cta = (a - ref).seconds() + 1.0;
//        K = n1 - n2 * cta;
//        double r = v[I] * (n2 * n2 * dt + 2.0 * n2 * K * log(dt / cta + 1) + K * K * dt / (cta * (dt + cta)));

        // Calculate v*int((f(x) - f1(x))/(x - ct))^2
        double r = v[I] * ((n2 * n2 * dt / 3.0 + n2 * n1) * dt + n1 * n1) * dt;

        // There are rounding problems that can make r < 0.0
        if (r < 0.0) {
            if (r < -0.001)
                Logger::msg("Ex.RI.Aggr", WARN, "Result is negative: ", r);
            r = 0.0;
        }
        result += r;
    }
    sqdiffStep(unsigned int lv, unsigned int rv, const Time & _r) : result(0.0), ref(_r) {
        v[0] = lv;
        v[1] = rv;
    }
};

struct minAndSqdiffStep {
    minStep min;
    sqdiffStep s;
    void operator()(Time a, Time b, double fa[4], double m[4], int i) {
        min(a, b, fa, m, i);
        s(a, b, fa, m, i);
    }
    minAndSqdiffStep(unsigned int maxPoints, unsigned int lv, unsigned int rv, const Time & ref)
            : min(maxPoints), s(lv, rv, ref) {}
};
}

double LDeltaFunction::sqdiff(const LDeltaFunction & r,
        const Time & ref, const Time & h) const {
    TCISteps::sqdiffStep ls(1.0, 1.0, ref);
    const LDeltaFunction * f[] = {this, &r};
    stepper(f, ref, h, ls);
    return ls.result;
}


namespace TCISteps {
struct lossStep {
    sqdiffStep s;
    void operator()(Time a, Time b, double fa[4], double m[4], int i) {
        int I = i ^ 1;
        s(a, b, fa, m, i);
        // Calculate v*int((f(x) - f1(x))/(x - ct))^2
//        if (s.n1 == 0.0 && s.n2 == 0.0) return;
//        double n3 = m[3 - i] - m[I];
//        double T = fa[3 - i] - fa[I] - n3 * s.cta;
//        double r = 2.0 * s.v[I] * (s.n2 * n3 * s.dt + (s.K * n3 + s.n2 * T) * log(s.dt / s.cta + 1) + s.K * T * s.dt / (s.cta * (s.dt + s.cta)));

        // Calculate v*int((f(x) - f1(x))/(x - ct))^2
        double mc = m[3 - i], ca = fa[3 - i];
        double r = ((s.n2 * mc * s.dt / 3 + (s.n1 * mc + s.n2 * ca) / 2.0) * s.dt + s.n1 * ca) * s.dt;

        // There are rounding problems that can make r < 0.0
        if (r < 0.0) {
            if (r < -0.001)
                Logger::msg("Ex.RI.Aggr", WARN, "Result is negative: ", r);
            r = 0.0;
        }
        s.result += r;
    }
    lossStep(unsigned int lv, unsigned int rv, const Time & ref) : s(lv, rv, ref) {}
};

struct minAndLossStep {
    minStep min;
    lossStep l;
    void operator()(Time a, Time b, double fa[4], double m[4], int i) {
        min(a, b, fa, m, i);
        l(a, b, fa, m, i);
    }
    minAndLossStep(unsigned int maxPoints, unsigned int lv, unsigned int rv, const Time & ref)
            : min(maxPoints), l(lv, rv, ref) {}
};
}

double LDeltaFunction::minAndLoss(const LDeltaFunction & l, const LDeltaFunction & r, unsigned int lv, unsigned int rv,
        const LDeltaFunction & lc, const LDeltaFunction & rc, const Time & ref, const Time & h) {
    // Enough space for all the points
    unsigned int size = l.points.size() > r.points.size() ? l.points.size() : r.points.size();
    TCISteps::minAndLossStep ls(2 * size, lv, rv, ref);
    const LDeltaFunction * f[] = {&l, &r, &lc, &rc};
    stepper(f, ref, h, ls);
    if (size > 0) {
        ls.min.points.push_back(FlopsBeforeDelta(h, ls.min.lasty));
        points.swap(ls.min.points);
        points.reserve(points.size());
    } else points.clear();
    slope = l.slope < r.slope ? l.slope : r.slope;
    return ls.l.s.result;
}


namespace TCISteps {
    struct lcStep {
        LDeltaFunction::PieceVector points;
        double c[2];
        double mm;
        double lasty;
        void operator()(Time a, Time b, double fa[2], double m[2], int i) {
            double newm = c[0] * m[0] + c[1] * m[1];
            if (mm != newm) {
                lasty = c[0] * fa[0] + c[1] * fa[1];
                points.push_back(LDeltaFunction::FlopsBeforeDelta(a, lasty));
                mm = newm;
            }
            lasty = c[0] * (fa[0] + m[0] * (b - a).seconds()) + c[1] * (fa[1] + m[1] * (b - a).seconds());

        }
        lcStep(unsigned int maxPoints, double lc, double rc) : mm(0.0), lasty(0.0) {
            points.reserve(maxPoints);
            c[0] = lc;
            c[1] = rc;
        }
    };
}

void LDeltaFunction::lc(const LDeltaFunction & l, const LDeltaFunction & r, double lc, double rc) {
    Time ct = Time::getCurrentTime(), horizon = ct;
    unsigned int size = 0;
    if (!l.points.empty()) {
        if (horizon < l.points.back().first) horizon = l.points.back().first;
        size += l.points.size();
    }
    if (!r.points.empty()) {
        if (horizon < r.points.back().first) horizon = r.points.back().first;
        size += r.points.size();
    }
    if (size > 0) {
        // Enough space for all the points
        TCISteps::lcStep ms(2 * size, lc, rc);
        const LDeltaFunction * f[2] = { &l, &r };
        stepper(f, ct, horizon, ms);
        ms.points.push_back(FlopsBeforeDelta(horizon, ms.lasty));
        points.swap(ms.points);
        points.reserve(points.size());
    }
    // Trivial case
    slope = lc * l.slope + rc * r.slope;
}


// A search algorithm to find best reduced solution
struct ResultCost {
    LDeltaFunction result;
    double cost;
    ResultCost() : cost(0.0) {}
    ResultCost(const LDeltaFunction & r, double c) : result(r), cost(c) {}
    bool operator<(const ResultCost & r) {
        return cost < r.cost;
    }
};


// FIXME: This method does not work well... why?
double LDeltaFunction::reduceMin(unsigned int v, LDeltaFunction & c,
        const Time & ref, const Time & h, unsigned int quality) {
//    if (points.size() > numPieces) {
//        const LDeltaFunction * f[] = {this, NULL, &c};
//        list<ResultCost> candidates(1);
//        candidates.front().result = *this;
//        candidates.front().cost = 0.0;
//        while (candidates.front().result.points.size() > numPieces) {
//            // Take next candidate and compute possibilities
//            LDeltaFunction best;
//            best.transfer(candidates.front().result);
//            candidates.pop_front();
//            double prevm = 0.0, curm = 0.0;
//            vector<pair<Time, uint64_t> >::iterator next = best.points.begin(), cur = next++, prev = cur;
//            while (next != best.points.end()) {
//                double nextm = (next->second - (double)cur->second) / (next->first - cur->first).seconds();
//                // Type of point
//                if (nextm <= curm || (nextm > curm && curm > prevm)) {
//                    LDeltaFunction func;
//                    func.slope = slope;
//                    if (nextm <= curm) {
//                        // concave, reduce it
//                        func.points.assign(best.points.begin(), cur);
//                        func.points.insert(func.points.end(), next, best.points.end());
//                    } else {
//                        // convex, reduce it
//                        func.points.assign(best.points.begin(), prev);
//                        double diffx = (prev->second + nextm * (cur->first - prev->first).seconds() - cur->second) / (nextm - prevm);
//                        func.points.push_back(FlopsBeforeDelta(prev->first + Duration(diffx), prev->second + prevm * diffx));
//                        func.points.insert(func.points.end(), next, best.points.end());
//                    }
//                    f[1] = &func;
//                    TCISteps::lossStep ls(v, 0, ref);
//                    stepper(f, ref, h, ls);
//                    // Retain only K best candidates, to reduce the exponential explosion of possibilities
//                    candidates.push_back(ResultCost());
//                    candidates.back().result.transfer(func);
//                    candidates.back().cost = ls.s.result;
//                    candidates.sort();
//                    if (candidates.size() > quality)
//                        candidates.pop_back();
//                }
//                prevm = curm;
//                curm = nextm;
//                prev = cur;
//                cur = next++;
//            }
//            // TODO: why this happens...
//            if (candidates.empty()) return 0;
//        }
//        transfer(candidates.front().result);
//        return candidates.front().cost;
//    }
    return 0.0;
}


// FIXME: This method does not work well... why?
double LDeltaFunction::reduceMax(const Time & ref, const Time & h, unsigned int quality) {
//    if (points.size() > numPieces) {
//        const LDeltaFunction * f[] = {NULL, this};
//        list<ResultCost> candidates(1);
//        candidates.front().result = *this;
//        candidates.front().cost = 0.0;
//        while (candidates.front().result.points.size() > numPieces) {
//            // Take next candidate and compute possibilities
//            LDeltaFunction best;
//            best.transfer(candidates.front().result);
//            candidates.pop_front();
//            vector<pair<Time, uint64_t> >::iterator next = ++best.points.begin(), cur = next++, prev = best.points.begin();
//            double prevm = 0.0, curm = (cur->second - prev->second) / (cur->first - prev->first).seconds();
//            while (next != best.points.end()) {
//                double nextm = (next->second - (double)cur->second) / (next->first - cur->first).seconds();
//                // Type of point
//                if (nextm > curm || (nextm <= curm && curm <= prevm)) {
//                    LDeltaFunction func;
//                    func.slope = slope;
//                    if (nextm <= curm) {
//                        // concave, reduce it
//                        func.points.assign(best.points.begin(), prev);
//                        double diffx = (cur->second - nextm * (cur->first - prev->first).seconds() - prev->second) / (prevm - nextm);
//                        func.points.push_back(FlopsBeforeDelta(prev->first + Duration(diffx), prev->second + prevm * diffx));
//                        func.points.insert(func.points.end(), next, best.points.end());
//                    } else {
//                        // convex, reduce it
//                        func.points.assign(best.points.begin(), cur);
//                        func.points.insert(func.points.end(), next, best.points.end());
//                    }
//                    f[0] = &func;
//                    TCISteps::sqdiffStep ls(1, 0, ref);
//                    stepper(f, ref, h, ls);
//                    // Retain only K best candidates, to reduce the exponential explosion of possibilities
//                    candidates.push_back(ResultCost());
//                    candidates.back().result.transfer(func);
//                    candidates.back().cost = ls.result;
//                    candidates.sort();
//                    if (candidates.size() > quality)
//                        candidates.pop_back();
//                }
//                prevm = curm;
//                curm = nextm;
//                prev = cur;
//                cur = next++;
//            }
//        }
//        transfer(candidates.front().result);
//        return candidates.front().cost;
//    }
    return 0.0;
}


double LDeltaFunction::getAvailabilityBefore(Time d) const {
    Time ct = Time::getCurrentTime();
    if (points.empty() && d > ct) {
        return slope * ((d - ct).seconds() - 1.0);
    } else if (d <= ct || d < points.front().first) {
        return 0;
    } else {
        PieceVector::const_iterator next = points.begin(), prev = next++;
        while (next != points.end() && next->first < d) prev = next++;
        if (next == points.end()) {
            return prev->second + (d - prev->first).seconds() * slope;
        } else {
            double intervalSlope = next->first != prev->first ? (next->second - prev->second) / (next->first - prev->first).seconds() : 0.0;
            return prev->second + (d - prev->first).seconds() * intervalSlope;
        }
    }
}


void LDeltaFunction::update(uint64_t length, Time deadline, Time horizon) {
    // Assume the availability at deadline is greater than length
    if (points.empty()) {
        // Task is assigned at the beginning
        points.push_back(FlopsBeforeDelta(Time::getCurrentTime() + Duration(length / slope), 0.0));
        points.push_back(FlopsBeforeDelta(horizon, slope * (horizon - points.back().first).seconds()));
    } else {
        // Find last point to erase
        FlopsBeforeDelta prev = points.front();
        int lastElim = 0, psize = points.size();
        while (lastElim < psize && points[lastElim].first <= deadline) prev = points[lastElim++];
        // Calculate availability at deadline
        double finalAvail;
        if (lastElim == psize) {
            finalAvail = prev.second;
        } else {
            finalAvail = prev.second + (deadline - prev.first).seconds() * (points[lastElim].second - prev.second) / (points[lastElim].first - prev.first).seconds();
        }
        finalAvail -= length;
        // Find first point to erase
        prev = points.front();
        int firstElim = 0;
        while (firstElim < lastElim && points[firstElim].second < finalAvail) prev = points[firstElim++];
        // Calculate time with finalAvail availability
        Time taskStart = prev.first +
                         Duration((finalAvail - prev.second) * (points[firstElim].first - prev.first).seconds() / (points[firstElim].second - prev.second));
        // Insert two new points before lastElim, and eliminate the rest
        if (lastElim - firstElim > 2) {
            for (int i = firstElim + 2, j = lastElim; j < psize; i++, j++)
                points[i] = points[j];
            points.resize(psize - lastElim + firstElim + 2);
        } else if (lastElim - firstElim < 2) {
            points.resize(psize - lastElim + firstElim + 2);
            // Use signed int so that condition j >= lastElim works when lastElim = 0
            for (int i = points.size() - 1, j = psize - 1; j >= lastElim; i--, j--)
                points[i] = points[j];
        }
        points[firstElim] = FlopsBeforeDelta(taskStart, finalAvail);
        points[firstElim + 1] = FlopsBeforeDelta(deadline, finalAvail);
        // Update availability after lastElim
        for (unsigned int i = lastElim; i < points.size(); i++)
            points[i].second -= length;
    }
}

}

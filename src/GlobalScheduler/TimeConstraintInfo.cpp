/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2007 Javier Celaya
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

#include <algorithm>
#include <list>
#include <cmath>
#include <limits>
#include <boost/date_time/gregorian/gregorian.hpp>
#include "Logger.hpp"
#include "TimeConstraintInfo.hpp"
using namespace std;
using namespace boost;


unsigned int TimeConstraintInfo::numClusters = 125;
unsigned int TimeConstraintInfo::numIntervals = 5;
unsigned int TimeConstraintInfo::numRefPoints = 8;


TimeConstraintInfo::ATFunction::ATFunction(double power, const list<Time> & p) : slope(power) {
    // For each point, calculate its availability
    uint64_t avail = 0;
    for (list<Time>::const_iterator B = p.begin(), A = B++; A != p.end(); B++, A = B++) {
        points.push_back(make_pair(*A, avail));
        LogMsg("Ex.RI.Aggr", DEBUG) << "At " << *A << ", availability " << avail;
        avail += (*B - *A).seconds() * power;
        points.push_back(make_pair(*B, avail));
        LogMsg("Ex.RI.Aggr", DEBUG) << "At " << *B << ", availability " << avail;
    }
}


template<int numF, class S> void TimeConstraintInfo::ATFunction::stepper(const ATFunction * (&f)[numF],
        const Time & ref, const Time & h, S & step) {
    // PRE: f size is numF, and numF >= 2
    Time a = ref, b;
    vector<pair<Time, uint64_t> >::const_iterator it[numF];
    double m[numF], fa[numF];
    pair<Time, uint64_t> lastPoint[numF];

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
    vector<pair<Time, uint64_t> > points;
    double mm;
    double lasty;
    void operator()(Time a, Time b, double fa[2], double m[2], int i) {
        if (mm != m[i]) {
            points.push_back(make_pair(a, fa[i]));
            mm = m[i];
        }
        lasty = fa[i] + m[i] * (b - a).seconds();
    }
    minStep(unsigned int maxPoints) : mm(0.0), lasty(0.0) {
        points.reserve(maxPoints);
    }
};
}

void TimeConstraintInfo::ATFunction::min(const ATFunction & l, const ATFunction & r) {
    if (!l.points.empty() || !r.points.empty()) {
        Time horizon = l.points.empty() ? r.points.back().first : l.points.back().first,
                       ct = Time::getCurrentTime();
        TCISteps::minStep ms(2 * (l.points.size() > r.points.size() ? l.points.size() : r.points.size()));
        // Enough space for all the points
        const ATFunction * f[] = {&l, &r};
        stepper(f, ct, horizon, ms);
        ms.points.push_back(make_pair(horizon, (uint64_t)ms.lasty));
        points.swap(ms.points);
        points.reserve(points.size());
    }
    // Trivial case
    slope = l.slope < r.slope ? l.slope : r.slope;
}


namespace TCISteps {
struct maxStep {
    vector<pair<Time, uint64_t> > points;
    double mm;
    double lasty;
    void operator()(Time a, Time b, double fa[2], double m[2], int i) {
        i = i ^ 1;
        if (mm != m[i]) {
            points.push_back(make_pair(a, fa[i]));
            mm = m[i];
        }
        lasty = fa[i] + m[i] * (b - a).seconds();
    }
    maxStep(unsigned int maxPoints) : mm(0.0), lasty(0.0) {
        points.reserve(maxPoints);
    }
};
}

void TimeConstraintInfo::ATFunction::max(const ATFunction & l, const ATFunction & r) {
    if (!l.points.empty() || !r.points.empty()) {
        Time horizon = l.points.empty() ? r.points.back().first : l.points.back().first,
                       ct = Time::getCurrentTime();
        // Enough space for all the points
        TCISteps::maxStep ms(2 * (l.points.size() > r.points.size() ? l.points.size() : r.points.size()));
        const ATFunction * f[] = {&l, &r};
        stepper(f, ct, horizon, ms);
        ms.points.push_back(make_pair(horizon, (uint64_t)ms.lasty));
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
        if (n1 == 0.0 && n2 == 0.0) return;
        dt = (b - a).seconds();
        // Avoid division by zero
        cta = (a - ref).seconds() + 1.0;
        K = n1 - n2 * cta;
        double r = v[I] * (n2 * n2 * dt + 2.0 * n2 * K * log(dt / cta + 1) + K * K * dt / (cta * (dt + cta)));
        // There are rounding problems that can make r < 0.0
        if (r < 0.0) {
//   if (r < -0.001)
//    LogMsg("Ex.RI.Aggr", WARN) << "Result is negative: " << r;
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

double TimeConstraintInfo::ATFunction::sqdiff(const ATFunction & r,
        const Time & ref, const Time & h) const {
    TCISteps::sqdiffStep ls(1.0, 1.0, ref);
    const ATFunction * f[] = {this, &r};
    stepper(f, ref, h, ls);
    return ls.result;
}


namespace TCISteps {
struct lossStep {
    sqdiffStep s;
    void operator()(Time a, Time b, double fa[4], double m[4], int i) {
        s(a, b, fa, m, i);
        if (s.n1 == 0.0 && s.n2 == 0.0) return;
        int I = i ^ 1;
        double n3 = m[3 - i] - m[I];
        double T = fa[3 - i] - fa[I] - n3 * s.cta;
        double r = 2.0 * s.v[I] * (s.n2 * n3 * s.dt + (s.K * n3 + s.n2 * T) * log(s.dt / s.cta + 1) + s.K * T * s.dt / (s.cta * (s.dt + s.cta)));
        // There are rounding problems that can make r < 0.0
        if (r < 0.0) {
//   if (r < -0.001)
//    LogMsg("Ex.RI.Aggr", WARN) << "Result is negative: " << r;
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

double TimeConstraintInfo::ATFunction::minAndLoss(const ATFunction & l, const ATFunction & r, unsigned int lv, unsigned int rv,
        const ATFunction & lc, const ATFunction & rc, const Time & ref, const Time & h) {
    // Enough space for all the points
    unsigned int size = l.points.size() > r.points.size() ? l.points.size() : r.points.size();
    TCISteps::minAndLossStep ls(2 * size, lv, rv, ref);
    const ATFunction * f[] = {&l, &r, &lc, &rc};
    stepper(f, ref, h, ls);
    if (size > 0) {
        ls.min.points.push_back(make_pair(h, (uint64_t)ls.min.lasty));
        points.swap(ls.min.points);
        points.reserve(points.size());
    } else points.clear();
    slope = l.slope < r.slope ? l.slope : r.slope;
    return ls.l.s.result;
}


namespace TCISteps {
    struct lcStep {
        vector<pair<Time, uint64_t> > points;
        double c[2];
        double mm;
        double lasty;
        void operator()(Time a, Time b, double fa[2], double m[2], int i) {
            double newm = c[0] * m[0] + c[1] * m[1];
            if (mm != newm) {
                lasty = c[0] * fa[0] + c[1] * fa[1];
                points.push_back(make_pair(a, lasty));
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

void TimeConstraintInfo::ATFunction::lc(const ATFunction & l, const ATFunction & r, double lc, double rc) {
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
        const ATFunction * f[2] = { &l, &r };
        stepper(f, ct, horizon, ms);
        ms.points.push_back(make_pair(horizon, (uint64_t)ms.lasty));
        points.swap(ms.points);
        points.reserve(points.size());
    }
    // Trivial case
    slope = lc * l.slope + rc * r.slope;
}


// A search algorithm to find best reduced solution
struct ResultCost {
    TimeConstraintInfo::ATFunction result;
    double cost;
    ResultCost() {}
    ResultCost(const TimeConstraintInfo::ATFunction & r, double c) : result(r), cost(c) {}
    bool operator<(const ResultCost & r) {
        return cost < r.cost;
    }
};

double TimeConstraintInfo::ATFunction::reduceMin(unsigned int v, ATFunction & c,
        const Time & ref, const Time & h, unsigned int quality) {
    if (points.size() > TimeConstraintInfo::numRefPoints) {
        const ATFunction * f[] = {this, NULL, &c};
        list<ResultCost> candidates(1);
        candidates.front().result = *this;
        candidates.front().cost = 0.0;
        while (candidates.front().result.points.size() > TimeConstraintInfo::numRefPoints) {
            // Take next candidate and compute possibilities
            ATFunction best;
            best.transfer(candidates.front().result);
            candidates.pop_front();
            double prevm = 0.0, curm = 0.0;
            vector<pair<Time, uint64_t> >::iterator next = best.points.begin(), cur = next++, prev = cur;
            while (next != best.points.end()) {
                double nextm = (next->second - (double)cur->second) / (next->first - cur->first).seconds();
                // Type of point
                if (nextm <= curm || (nextm > curm && curm > prevm)) {
                    ATFunction func;
                    func.slope = slope;
                    if (nextm <= curm) {
                        // concave, reduce it
                        func.points.assign(best.points.begin(), cur);
                        func.points.insert(func.points.end(), next, best.points.end());
                    } else {
                        // convex, reduce it
                        func.points.assign(best.points.begin(), prev);
                        double diffx = (prev->second + nextm * (cur->first - prev->first).seconds() - cur->second) / (nextm - prevm);
                        func.points.push_back(make_pair(prev->first + Duration(diffx), prev->second + prevm * diffx));
                        func.points.insert(func.points.end(), next, best.points.end());
                    }
                    f[1] = &func;
                    TCISteps::lossStep ls(v, 0, ref);
                    stepper(f, ref, h, ls);
                    // Retain only K best candidates, to reduce the exponential explosion of possibilities
                    candidates.push_back(ResultCost());
                    candidates.back().result.transfer(func);
                    candidates.back().cost = ls.s.result;
                    candidates.sort();
                    if (candidates.size() > quality)
                        candidates.pop_back();
                }
                prevm = curm;
                curm = nextm;
                prev = cur;
                cur = next++;
            }
        }
        transfer(candidates.front().result);
        return candidates.front().cost;
    }
    return 0.0;
}


double TimeConstraintInfo::ATFunction::reduceMax(const Time & ref, const Time & h, unsigned int quality) {
    if (points.size() > TimeConstraintInfo::numRefPoints) {
        const ATFunction * f[] = {NULL, this};
        list<ResultCost> candidates(1);
        candidates.front().result = *this;
        candidates.front().cost = 0.0;
        while (candidates.front().result.points.size() > TimeConstraintInfo::numRefPoints) {
            // Take next candidate and compute possibilities
            ATFunction best;
            best.transfer(candidates.front().result);
            candidates.pop_front();
            vector<pair<Time, uint64_t> >::iterator next = ++best.points.begin(), cur = next++, prev = best.points.begin();
            double prevm = 0.0, curm = (cur->second - prev->second) / (cur->first - prev->first).seconds();
            while (next != best.points.end()) {
                double nextm = (next->second - (double)cur->second) / (next->first - cur->first).seconds();
                // Type of point
                if (nextm > curm || (nextm <= curm && curm <= prevm)) {
                    ATFunction func;
                    func.slope = slope;
                    if (nextm <= curm) {
                        // concave, reduce it
                        func.points.assign(best.points.begin(), prev);
                        double diffx = (cur->second - nextm * (cur->first - prev->first).seconds() - prev->second) / (prevm - nextm);
                        func.points.push_back(make_pair(prev->first + Duration(diffx), prev->second + prevm * diffx));
                        func.points.insert(func.points.end(), next, best.points.end());
                    } else {
                        // convex, reduce it
                        func.points.assign(best.points.begin(), cur);
                        func.points.insert(func.points.end(), next, best.points.end());
                    }
                    f[0] = &func;
                    TCISteps::sqdiffStep ls(1, 0, ref);
                    stepper(f, ref, h, ls);
                    // Retain only K best candidates, to reduce the exponential explosion of possibilities
                    candidates.push_back(ResultCost());
                    candidates.back().result.transfer(func);
                    candidates.back().cost = ls.result;
                    candidates.sort();
                    if (candidates.size() > quality)
                        candidates.pop_back();
                }
                prevm = curm;
                curm = nextm;
                prev = cur;
                cur = next++;
            }
        }
        transfer(candidates.front().result);
        return candidates.front().cost;
    }
    return 0.0;
}


uint64_t TimeConstraintInfo::ATFunction::getAvailabilityBefore(Time d) const {
    Time ct = Time::getCurrentTime();
    if (points.empty() && d > ct) {
        return slope * (d - ct).seconds();
    } else if (d <= ct || d < points.front().first) {
        return 0;
    } else {
        vector<pair<Time, uint64_t> >::const_iterator next = points.begin(), prev = next++;
        while (next != points.end() && next->first < d) prev = next++;
        if (next == points.end()) {
            return prev->second + (d - prev->first).seconds() * slope;
        } else {
            return prev->second + (d - prev->first).seconds() * (next->second - prev->second) / (next->first - prev->first).seconds();
        }
    }
}


void TimeConstraintInfo::ATFunction::update(uint64_t length, Time deadline, Time horizon) {
    // Assume the availability at deadline is greater than length
    if (points.empty()) {
        // Task is assigned at the beginning
        points.push_back(make_pair(Time::getCurrentTime() + Duration(length / slope), 0));
        points.push_back(make_pair(horizon, slope * (horizon - points.back().first).seconds()));
    } else {
        // Find last point to erase
        pair<Time, uint64_t> prev = points.front();
        int lastElim = 0, psize = points.size();
        while (lastElim < psize && points[lastElim].first <= deadline) prev = points[lastElim++];
        // Calculate availability at deadline
        uint64_t finalAvail;
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
        points[firstElim] = make_pair(taskStart, finalAvail);
        points[firstElim + 1] = make_pair(deadline, finalAvail);
        // Update availability after lastElim
        for (unsigned int i = lastElim; i < points.size(); i++)
            points[i].second -= length;
    }
}


void TimeConstraintInfo::MDFCluster::aggregate(const MDFCluster & l, const MDFCluster & r) {
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

    ATFunction newMinA;
    accumAsq = l.accumAsq + r.accumAsq
               + newMinA.minAndLoss(l.minA, r.minA, l.value, r.value, l.accumMaxA, r.accumMaxA, reference->aggregationTime, reference->horizon);
    accumMaxA.max(l.accumMaxA, r.accumMaxA);

    minM = newMinM;
    minD = newMinD;
    minA.transfer(newMinA);
    value = l.value + r.value;
}


void TimeConstraintInfo::MDFCluster::aggregate(const MDFCluster & r) {
    aggregate(*this, r);
}


double TimeConstraintInfo::MDFCluster::distance(const MDFCluster & r, MDFCluster & sum) const {
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
        if (reference->availRange) {
            double loss = (sum.accumAsq / reference->availRange / sum.value);
// TODO
//   if (floor(minA.sqdiff(reference->minA, reference->aggregationTime, reference->horizon) * numIntervals / reference->availRange)
//     != floor(r.minA.sqdiff(reference->minA, reference->aggregationTime, reference->horizon) * numIntervals / reference->availRange)) {
//    loss += 100.0;
//   }
            if (minA.isFree() != r.minA.isFree()) loss += 100.0;
            result += loss;
        }
    }
    return result;
}


bool TimeConstraintInfo::MDFCluster::far(const MDFCluster & r) const {
    if (reference->memRange) {
        if (((minM - reference->minM) * numIntervals / reference->memRange) != ((r.minM - reference->minM) * numIntervals / reference->memRange))
            return true;
    }
    if (reference->diskRange) {
        if (((minD - reference->minD) * numIntervals / reference->diskRange) != ((r.minD - reference->minD) * numIntervals / reference->diskRange))
            return true;
    }
    if (minA.isFree() != r.minA.isFree()) return true;
// TODO
// if (reference->availRange) {
//  if (floor(minA.sqdiff(reference->minA, reference->aggregationTime, reference->horizon) * numIntervals / reference->availRange)
//    != floor(r.minA.sqdiff(reference->minA, reference->aggregationTime, reference->horizon) * numIntervals / reference->availRange)) {
//   return true;
//  }
// }
    return false;
}


void TimeConstraintInfo::MDFCluster::reduce() {
    accumAsq += minA.reduceMin(value, accumMaxA, reference->aggregationTime, reference->horizon);
    accumMaxA.reduceMax(reference->aggregationTime, reference->horizon);
}


void TimeConstraintInfo::addNode(uint32_t mem, uint32_t disk, double power, const std::list<Time> & p) {
    MDFCluster tmp(this, mem, disk, power, p);
    summary.pushBack(tmp);
    if (summary.getSize() == 1) {
        minM = maxM = mem;
        minD = maxD = disk;
        minA = maxA = tmp.minA;
        horizon = tmp.minA.getHorizon();
    } else {
        if (minM > mem) minM = mem;
        if (maxM < mem) maxM = mem;
        if (minD > disk) minD = disk;
        if (maxD < disk) maxD = disk;
        minA.min(minA, tmp.minA);
        maxA.max(maxA, tmp.minA);
        if (horizon < tmp.minA.getHorizon()) horizon = tmp.minA.getHorizon();
    }
}


void TimeConstraintInfo::join(const TimeConstraintInfo & r) {
    if (!r.summary.isEmpty()) {
        LogMsg("Ex.RI.Aggr", DEBUG) << "Aggregating two summaries:";

        if (summary.isEmpty()) {
            // operator= forbidden
            minM = r.minM;
            maxM = r.maxM;
            minD = r.minD;
            maxD = r.maxD;
            minA = r.minA;
            maxA = r.maxA;
            horizon = r.horizon;
        } else {
            if (minM > r.minM) minM = r.minM;
            if (maxM < r.maxM) maxM = r.maxM;
            if (minD > r.minD) minD = r.minD;
            if (maxD < r.maxD) maxD = r.maxD;
            minA.min(minA, r.minA);
            maxA.max(maxA, r.maxA);
            if (horizon < r.horizon) horizon = r.horizon;
        }
        summary.add(r.summary);
        unsigned int size = summary.getSize();
        for (unsigned int i = 0; i < size; i++)
            summary[i].setReference(this);
    }
}


void TimeConstraintInfo::reduce() {
    // Set up clustering variables
    aggregationTime = Time::getCurrentTime();
    memRange = maxM - minM;
    diskRange = maxD - minD;
    availRange = maxA.sqdiff(minA, aggregationTime, horizon);
    summary.clusterize(numClusters);
    unsigned int size = summary.getSize();
    for (unsigned int i = 0; i < size; i++)
        summary[i].reduce();
}


void TimeConstraintInfo::getAvailability(list<AssignmentInfo> & ai, const TaskDescription & desc) const {
    LogMsg("Ex.RI.Comp", DEBUG) << "Looking on " << *this;
    if (desc.getDeadline() > Time::getCurrentTime()) {
        // Make a list of suitable cells
        for (size_t i = 0; i < summary.getSize(); i++) {
            unsigned long int avail = summary[i].minA.getAvailabilityBefore(desc.getDeadline());
            if (summary[i].value > 0 && avail >= desc.getLength() && summary[i].minM >= desc.getMaxMemory() && summary[i].minD >= desc.getMaxDisk()) {
                unsigned int numTasks = avail / desc.getLength();
                unsigned long int restAvail = avail % desc.getLength();
                ai.push_back(AssignmentInfo(i, summary[i].value * numTasks, summary[i].minM - desc.getMaxMemory(), summary[i].minD - desc.getMaxDisk(), restAvail));
            }
        }
    }
}


void TimeConstraintInfo::update(const list<TimeConstraintInfo::AssignmentInfo> & ai, const TaskDescription & desc) {
    // For each cluster
    for (list<AssignmentInfo>::const_iterator it = ai.begin(); it != ai.end(); it++) {
        MDFCluster & cluster = summary[it->cluster];
        // Create new one
        MDFCluster tmp(cluster);
        uint64_t avail = cluster.minA.getAvailabilityBefore(desc.getDeadline());
        uint64_t tasksPerNode = avail / desc.getLength();
        unsigned int numNodes = it->numTasks / tasksPerNode;
        if (it->numTasks % tasksPerNode) numNodes++;

        // Update the old one, just take out the affected nodes
        // NOTE: do not touch accum values, do not know how...
        cluster.value -= numNodes;
        // Update the new one
        tmp.value = numNodes;
        if (tasksPerNode > it->numTasks)
            tmp.minA.update(desc.getLength() * it->numTasks, desc.getDeadline(), horizon);
        else
            tmp.minA.update(desc.getLength() * tasksPerNode, desc.getDeadline(), horizon);

        summary.pushBack(tmp);
        minA.min(minA, tmp.minA);
    }
}


void TimeConstraintInfo::output(std::ostream& os) const {
    for (size_t i = 0; i < summary.getSize(); i++)
        os << "(" << summary[i] << ')';
}

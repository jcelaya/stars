/*
 *  STaRS, Scalable Task Routing approach to distributed Scheduling
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


#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/shared_array.hpp>
#include <limits>
#include <algorithm>
#include <sstream>
#include "Logger.hpp"
#include "MSPAvailabilityInformation.hpp"
#include "Time.hpp"
#include "TaskDescription.hpp"
using std::vector;
using std::list;
using std::make_pair;


REGISTER_MESSAGE(MSPAvailabilityInformation);


unsigned int MSPAvailabilityInformation::numClusters = 125;
unsigned int MSPAvailabilityInformation::numIntervals = 5;
unsigned int MSPAvailabilityInformation::numPieces = 64;


void TaskProxy::sort(list<TaskProxy> & curTasks, double slowness) {
    if (curTasks.size() > 1) {
        // Sort the vector, leaving the first task as is
        list<TaskProxy> tmp;
        tmp.splice(tmp.begin(), curTasks, curTasks.begin());
        for (list<TaskProxy>::iterator i = curTasks.begin(); i != curTasks.end(); ++i)
            i->setSlowness(slowness);
        curTasks.sort();
        curTasks.splice(curTasks.begin(), tmp, tmp.begin());
    }
}


bool TaskProxy::meetDeadlines(const list<TaskProxy> & curTasks, double slowness, Time e) {
    for (list<TaskProxy>::const_iterator i = curTasks.begin(); i != curTasks.end(); ++i)
        if ((e += Duration(i->t)) > i->getDeadline(slowness))
            return false;
    return true;
}


void TaskProxy::sortMinSlowness(list<TaskProxy> & curTasks, const std::vector<double> & switchValues) {
    Time now = Time::getCurrentTime();
    // Trivial case, first switch value is minimum slowness so that first task meets deadline
    if (switchValues.size() == 1) {
        TaskProxy::sort(curTasks, switchValues.front() + 1.0);
        return;
    }
    // Calculate interval by binary search
    unsigned int minLi = 0, maxLi = switchValues.size() - 1;
    while (maxLi > minLi + 1) {
        unsigned int medLi = (minLi + maxLi) >> 1;
        TaskProxy::sort(curTasks, (switchValues[medLi] + switchValues[medLi + 1]) / 2.0);
        // For each app, check whether it is going to finish in time or not.
        if (TaskProxy::meetDeadlines(curTasks, switchValues[medLi], now))
            maxLi = medLi;
        else
            minLi = medLi;
    }
    // Sort them one last time
    // If maxLi is still size-1, check interval lBounds[maxLi]-infinite
    if (maxLi == switchValues.size() - 1 && !TaskProxy::meetDeadlines(curTasks, switchValues.back(), now))
        TaskProxy::sort(curTasks, switchValues.back() + 1.0);
    else
        TaskProxy::sort(curTasks, (switchValues[minLi] + switchValues[minLi + 1]) / 2.0);
}


void TaskProxy::getSwitchValues(const std::list<TaskProxy> & proxys, std::vector<double> & switchValues) {
    switchValues.clear();
    if (!proxys.empty()) {
        // Minimum switch value is first task slowness
        Time firstTaskEndTime = Time::getCurrentTime() + Duration(proxys.front().t);
        switchValues.push_back((firstTaskEndTime - proxys.front().rabs).seconds() / proxys.front().a);
        // Calculate bounds with the rest of the tasks, except the first
        for (list<TaskProxy>::const_iterator it = ++proxys.begin(); it != proxys.end(); ++it) {
            for (list<TaskProxy>::const_iterator jt = it; jt != proxys.end(); ++jt) {
                if (it->a != jt->a) {
                    double l = (jt->rabs - it->rabs).seconds() / (it->a - jt->a);
                    if (l > switchValues.front()) {
                        switchValues.push_back(l);
                    }
                }
            }
        }
        std::sort(switchValues.begin(), switchValues.end());
        // Remove duplicate values
        switchValues.erase(std::unique(switchValues.begin(), switchValues.end()), switchValues.end());
    }
}


double TaskProxy::getSlowness(const std::list<TaskProxy> & proxys) {
    double minSlowness = 0.0;
    Time e = Time::getCurrentTime();
    // For each task, calculate finishing time
    for (list<TaskProxy>::const_iterator i = proxys.begin(); i != proxys.end(); ++i) {
        e += Duration(i->t);
        double slowness = (e - i->rabs).seconds() / i->a;
        if (slowness > minSlowness)
            minSlowness = slowness;
    }

    return minSlowness;
}


MSPAvailabilityInformation::LAFunction::LAFunction(list<TaskProxy> curTasks,
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
    for (list<TaskProxy>::iterator i = curTasks.begin(); i != curTasks.end(); ++i)
        i->r = (i->rabs - now).seconds();

    while(true) {
        // Order the queue and calculate minimum slowness
        // The new task is at the end of the queue
        vector<double> svCur(switchValues);
        if (!svCur.empty()) {
            // Add order change values for the new task
            for (list<TaskProxy>::iterator i = ++curTasks.begin(); i != curTasks.end(); ++i) {
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
            TaskProxy::sortMinSlowness(curTasks, svCur);
        }

        // Update the index of the task that sets maximum slowness
        list<TaskProxy>::iterator tn, tm = curTasks.begin();
        // For each task, calculate its slowness and its tendency
        double e = curTasks.front().t, maxSlowness = (e - curTasks.front().r) / curTasks.front().a, maxTendency = 0.0;
        curTasks.front().tsum = curTasks.front().t;
        bool beforeNewTask = true, minBeforeNew = true;
        for (list<TaskProxy>::iterator i = curTasks.begin(), prev = i++; i != curTasks.end(); prev = i++) {
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
        list<TaskProxy>::iterator tn1 = tn;
        ++tn1;

//		std::ostringstream osstmp;
//		for (size_t i = 0; i < curTasks.size(); ++i)
//			osstmp << curTasks[i].id << ',';
//		LogMsg("Ex.RI.Aggr", DEBUG) << "At " << curA << " maxTask is " << tm.id << " and the task order is " << osstmp.str();
//
        // Calculate next interval based on the task that is setting the current minimum and its value
        // The current minimum slowness task is the new task
        if (tm == tn) {
            SubFunction sf(tm->tsum, 0.0, 1.0 / power, 0.0);
            if (pieces.empty() || pieces.back().second != sf)
                pieces.push_back(make_pair(tn->a, sf));

            // See at which values of a other tasks mark the minimum slowness
            // Tasks before the new one
            for (list<TaskProxy>::iterator i = curTasks.begin(); i != tn; ++i) {
                a = i->a * tm->tsum / (i->tsum - i->a / power - i->r);
                if (a > curA && a < minA) minA = a;
            }

            // Tasks after the new one
            for (list<TaskProxy>::iterator i = tn1; i != curTasks.end(); ++i) {
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
            for (list<TaskProxy>::iterator i = tn1; i != curTasks.end(); ++i) {
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
            for (list<TaskProxy>::iterator i = curTasks.begin(); i != tn; ++i) {
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
            for (list<TaskProxy>::iterator i = tn1; i != curTasks.end(); ++i) {
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


void MSPAvailabilityInformation::LAFunction::modifyReference(Time oldRef, Time newRef) {
    // FIXME: It seems better not doing anything here...
//    double difference = (newRef - oldRef).seconds();
//    for (vector<std::pair<double, SubFunction> >::iterator next = pieces.begin(), it = next++;
//            next != pieces.end(); it = next++) {
//        // For every subfunction with x > 0.0, substract (newRef - oldRef)
//        if (it->second.x > 0.0)
//            it->second.x = it->second.x > difference ? it->second.x - difference : 0.0;
//        // Recalculate the limit
//        double alpha = (it->second.y - next->second.y);
//        double b = it->second.z1 - next->second.z1 + it->second.z2 - next->second.z2;
//        double c = it->second.x - next->second.x;
//        if (alpha == 0.0) {
//            if (b != 0)
//                next->first = -c / b + 1.0;
//        } else if (b*b - 4.0*alpha*c >= 0) {
//            if (alpha < 0.0)
//                next->first = (-b - std::sqrt(b*b - 4.0*alpha*c)) / (2.0*alpha) + 1.0;
//            else
//                next->first = (-b + std::sqrt(b*b - 4.0*alpha*c)) / (2.0*alpha) + 1.0;
//        }
//        // Calculate the limit only if the function was continuous?
//    }
//    if (pieces.back().second.x > 0.0)
//        pieces.back().second.x = pieces.back().second.x > difference ? pieces.back().second.x - difference : 0.0;
}


template<int numF, class S> void MSPAvailabilityInformation::LAFunction::stepper(const LAFunction * (&f)[numF], S & step) {
    // PRE: f size is numF, and numF >= 2, all functions have at least one piece
    double edges[4] = { minTaskLength, 0.0, 0.0, 0.0 };
    double & s = edges[0], e;
    vector<std::pair<double, SubFunction> >::const_iterator cur[numF], next[numF];

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
            for (int i = 1; i < numEdges; ++i) {
                double mid = edges[i] < INFINITY ? (edges[i - 1] + edges[i]) / 2.0 : edges[i - 1] + 1000.0;
                step(edges[i - 1], edges[i], cur, c / mid + a * mid + b > 0 ? 0 : 1);
            }
        }
        s = e;
        // Advance iterator
        cur[nextF] = next[nextF]++;
    }
}


struct MSPAvailabilityInformation::LAFunction::minStep {
    void operator()(double a, double b, const vector<std::pair<double, SubFunction> >::const_iterator f[2], int max) {
        if (pieces.empty() || pieces.back().second != f[max ^ 1]->second)
            pieces.push_back(make_pair(a, f[max ^ 1]->second));
    }

    std::vector<std::pair<double, MSPAvailabilityInformation::LAFunction::SubFunction> > pieces;
};


void MSPAvailabilityInformation::LAFunction::min(const LAFunction & l, const LAFunction & r) {
    minStep step;
    const LAFunction * f[] = {&l, &r};
    stepper(f, step);
    pieces.swap(step.pieces);
}


struct MSPAvailabilityInformation::LAFunction::maxStep {
    void operator()(double a, double b, const vector<std::pair<double, SubFunction> >::const_iterator f[2], int max) {
        if (pieces.empty() || pieces.back().second != f[max]->second)
            pieces.push_back(make_pair(a, f[max]->second));
    }

    std::vector<std::pair<double, MSPAvailabilityInformation::LAFunction::SubFunction> > pieces;
};


void MSPAvailabilityInformation::LAFunction::max(const LAFunction & l, const LAFunction & r) {
    maxStep step;
    const LAFunction * f[] = {&l, &r};
    stepper(f, step);
    pieces.swap(step.pieces);
}


struct MSPAvailabilityInformation::LAFunction::maxDiffStep {
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
    std::vector<std::pair<double, MSPAvailabilityInformation::LAFunction::SubFunction> > pieces;
};


void MSPAvailabilityInformation::LAFunction::maxDiff(const LAFunction & l, const LAFunction & r,
        unsigned int lv, unsigned int rv, const LAFunction & maxL, const LAFunction & maxR) {
    maxDiffStep step(lv, rv);
    const LAFunction * f[] = {&l, &r, &maxL, &maxR};
    stepper(f, step);
    pieces.swap(step.pieces);
}


struct MSPAvailabilityInformation::LAFunction::sqdiffStep {
    sqdiffStep(int lv, int rv, double _ah) : result(0.0), ah(_ah) {
        val[0] = lv;
        val[1] = rv;
    }

    void operator()(double a, double b, const vector<std::pair<double, SubFunction> >::const_iterator f[2], int max) {
        if (b == INFINITY) b = ah;
        I = max ^ 1;
        u = f[max]->second.x - f[I]->second.x;
        v = f[max]->second.y - f[I]->second.y;
        w = f[max]->second.z1 - f[I]->second.z1 + f[max]->second.z2 - f[I]->second.z2;
        ab = a*b;
        ba = b - a;
        ba2 = b*b - a*a;
        ba3 = b*b*b - a*a*a;
        fracba = b/a;
        // Compute v * int((f - f1)^2)
        double tmp = (u*u/ab + 2.0*u*v + w*w)*ba + w*v*ba2 + v*v*ba3/3.0 + 2.0*u*w*log(fracba);
        result += val[I] * tmp;
    }

    int val[2];
    int I;
    double result, ah;
    double u, v, w, ab, ba, ba2, ba3, fracba;
};


double MSPAvailabilityInformation::LAFunction::sqdiff(const LAFunction & r, double ah) const {
    sqdiffStep step(1, 1, ah);
    const LAFunction * f[] = {this, &r};
    stepper(f, step);
    return step.result;
}


struct MSPAvailabilityInformation::LAFunction::maxAndLossStep {
    maxAndLossStep(unsigned int lv, unsigned int rv, double _ah) : ss(lv, rv, _ah) {}

    void operator()(double a, double b, const vector<std::pair<double, SubFunction> >::const_iterator f[4], int max) {
        ms(a, b, f, max);
        ss(a, b, f, max);
        int lin = 3 - max;
        // Add 2 * v * int((f - f1)*(f1 - max))
//		double u2 = f[ss.I]->second.x - f[lin]->second.x;
//		double v2 = f[ss.I]->second.y - f[lin]->second.y;
//		double w2 = f[ss.I]->second.z - f[lin]->second.z;
//		double tmp = ss.val[ss.I] * ((ss.u*u2/ss.ab + u2*ss.v + ss.u*v2 + ss.w*w2)*ss.ba + (ss.w*v2 + ss.v*w2)*ss.ba2/2.0 + ss.v*v2*ss.ba3/3.0 + (u2*ss.w + ss.u*w2)*log(ss.fracba));

        // Add 2 * int((f - f1)*(f1 - max_i(f1i)))
        double u2 = f[lin]->second.x;
        double v2 = f[lin]->second.y;
        double w2 = f[lin]->second.z1 + f[lin]->second.z2;
        double tmp = (ss.u*u2/ss.ab + u2*ss.v + ss.u*v2 + ss.w*w2)*ss.ba + (ss.w*v2 + ss.v*w2)*ss.ba2/2.0 + ss.v*v2*ss.ba3/3.0 + (u2*ss.w + ss.u*w2)*log(ss.fracba);
        ss.result += 2.0 * tmp;
    }

    sqdiffStep ss;
    maxStep ms;
};


double MSPAvailabilityInformation::LAFunction::maxAndLoss(const LAFunction & l, const LAFunction & r, unsigned int lv, unsigned int rv,
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
    MSPAvailabilityInformation::LAFunction result;
    double cost;
    ResultCost() {}
    ResultCost(const MSPAvailabilityInformation::LAFunction & r, double c) : result(r), cost(c) {}
    bool operator<(const ResultCost & r) {
        return cost < r.cost;
    }
};
}


double MSPAvailabilityInformation::LAFunction::reduceMax(unsigned int v, double ah, unsigned int quality) {
    if (pieces.size() > MSPAvailabilityInformation::numPieces) {
        const LAFunction * f[] = {NULL, this};
        list<ResultCost> candidates(1);
        candidates.front().cost = 0.0;
        candidates.front().result = *this;
        while (candidates.front().result.pieces.size() > MSPAvailabilityInformation::numPieces) {
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


double MSPAvailabilityInformation::LAFunction::getSlowness(uint64_t a) const {
    vector<std::pair<double, SubFunction> >::const_iterator next = pieces.begin(), it = next++;
    while (next != pieces.end() && next->first < a) it = next++;
    return it->second.value(a);
}


double MSPAvailabilityInformation::LAFunction::estimateSlowness(uint64_t a, unsigned int n) const {
    vector<std::pair<double, SubFunction> >::const_iterator next = pieces.begin(), it = next++;
    while (next != pieces.end() && next->first < a) it = next++;
    // FIXME: Since functions do not need to be continuous, limits are not adjusted
//    while (next != pieces.end()) {
//        // Recalculate the limit
//        double alpha = n * (it->second.y - next->second.y);
//        double b = n * (it->second.z1 - next->second.z1) + it->second.z2 - next->second.z2;
//        double c = it->second.x - next->second.x;
//        double limit = next->first;
//        if (alpha == 0.0) {
//            if (b != 0)
//                limit = -c / b + 1.0;
//        } else if (b*b - 4.0*alpha*c >= 0) {
//            if (alpha < 0.0)
//                limit = (-b - std::sqrt(b*b - 4.0*alpha*c)) / (2.0*alpha) + 1.0;
//            else
//                limit = (-b + std::sqrt(b*b - 4.0*alpha*c)) / (2.0*alpha) + 1.0;
//        }
//        // Calculate the limit only if the function was continuous?
//        // If the limit is still before a, advance
//        if (limit < a)
//            it = next++;
//        else break;
//    }
    return it->second.value(a, n);
}


void MSPAvailabilityInformation::LAFunction::update(uint64_t length, unsigned int n) {
    // Invalidate?
    pieces.clear();
    pieces.push_back(make_pair((double)minTaskLength, SubFunction(0.0, INFINITY, 0.0, 0.0)));
    // TODO
//    for (size_t i = 0; i < pieces.size(); ++i)
//        pieces[i].second.z2 += length * n * pieces[i].second.y;
}


double MSPAvailabilityInformation::LAFunction::getSlowestMachine() const {
    double result = 0.0;
    for (size_t i = 0; i < pieces.size(); ++i)
        if (pieces[i].second.z1 > result)
            result = pieces[i].second.z1;
    return result;
}


double MSPAvailabilityInformation::MDLCluster::distance(const MDLCluster & r, MDLCluster & sum) const {
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


bool MSPAvailabilityInformation::MDLCluster::far(const MDLCluster & r) const {
    if (reference->memRange) {
        if (((minM - reference->minM) * numIntervals / reference->memRange) != ((r.minM - reference->minM) * numIntervals / reference->memRange))
            return true;
    }
    if (reference->diskRange) {
        if (((minD - reference->minD) * numIntervals / reference->diskRange) != ((r.minD - reference->minD) * numIntervals / reference->diskRange))
            return true;
    }
// TODO
//	if (minA.isFree() != r.minA.isFree()) return true;
//	if (reference->availRange) {
//		if (floor(minA.sqdiff(reference->minA, reference->aggregationTime, reference->horizon) * numIntervals / reference->availRange)
//				!= floor(r.minA.sqdiff(reference->minA, reference->aggregationTime, reference->horizon) * numIntervals / reference->availRange)) {
//			return true;
//		}
//	}
    return false;
}


void MSPAvailabilityInformation::MDLCluster::aggregate(const MDLCluster & r) {
    aggregate(*this, r);
}


void MSPAvailabilityInformation::MDLCluster::aggregate(const MDLCluster & l, const MDLCluster & r) {
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


void MSPAvailabilityInformation::MDLCluster::reduce() {
    accumLsq += maxL.reduceMax(value, reference->lengthHorizon);
    accumMaxL.reduceMax(1, reference->lengthHorizon);
}


void MSPAvailabilityInformation::setAvailability(uint32_t m, uint32_t d, const list<TaskProxy> & curTasks,
        const std::vector<double> & switchValues, double power, double minSlowness) {
    minM = maxM = m;
    minD = maxD = d;
    minimumSlowness = maximumSlowness = minSlowness;
    summary.clear();
    summary.pushBack(MDLCluster(this, m, d, curTasks, switchValues, power));
    minL = maxL = summary[0].maxL;
    lengthHorizon = minL.getHorizon();
}


void MSPAvailabilityInformation::getFunctions(const TaskDescription & req, std::vector<std::pair<LAFunction *, unsigned int> > & f) {
    unsigned int size = summary.getSize();
    for (unsigned int i = 0; i < size; ++i)
        if (summary[i].fulfills(req))
            f.push_back(std::make_pair(&summary[i].maxL, summary[i].value));
}


double MSPAvailabilityInformation::getSlowestMachine() const {
    return maxL.getSlowestMachine();
}


void MSPAvailabilityInformation::join(const MSPAvailabilityInformation & r) {
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


void MSPAvailabilityInformation::updateRkReference(Time newRef) {
    unsigned int size = summary.getSize();
    for (unsigned int i = 0; i < size; i++) {
        summary[i].maxL.modifyReference(rkref, newRef);
        summary[i].accumMaxL.modifyReference(rkref, newRef);
    }
    rkref = newRef;
}


void MSPAvailabilityInformation::reduce() {
    // Set up clustering variables
    memRange = maxM - minM;
    diskRange = maxD - minD;
    slownessRange = maxL.sqdiff(minL, lengthHorizon);
    summary.clusterize(numClusters);
    unsigned int size = summary.getSize();
    for (unsigned int i = 0; i < size; i++)
        summary[i].reduce();
}


void MSPAvailabilityInformation::output(std::ostream & os) const {
    os << minimumSlowness << "s/i, ";
    os << '(' << minM << "MB, " << maxM << "MB) ";
    os << '(' << minD << "MB, " << maxD << "MB) ";
    os << '(' << minL << ", " << maxL << ") (";
    for (unsigned int i = 0; i < summary.getSize(); i++)
        os << summary[i] << ',';
    os << ')';
}

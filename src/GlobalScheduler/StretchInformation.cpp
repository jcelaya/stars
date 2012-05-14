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
#include "Logger.hpp"
#include "StretchInformation.hpp"
#include "Time.hpp"
#include "TaskDescription.hpp"
#include "MinStretchScheduler.hpp"
using std::vector;
using std::list;
using std::make_pair;


unsigned int StretchInformation::numClusters = 125;
unsigned int StretchInformation::numIntervals = 5;
unsigned int StretchInformation::numPieces = 64;
static double infinity = std::numeric_limits<double>::infinity();


static bool extendsToRight(const std::vector<StretchInformation::HSWFunction::Piece> & b,
                           const StretchInformation::HSWFunction::Piece & l,
                           const StretchInformation::HSWFunction::Piece & r,
                           int upos) {
    return l.d == r.d && l.e == r.e && (l.nw == upos || (l.nw != -1 && upos != -1 && b[l.nw].d == b[upos].d && b[l.nw].e == b[upos].e)) && l.f == r.f;
}


void StretchInformation::HSWFunction::insertNextTo(const Piece & tmpp, int & lpos, int & upos, vector<Piece> & b) {
    int pos;
    // We assume it does not extend the upper piece, maybe it extends the left piece...
    if (lpos != -1 && extendsToRight(b, b[lpos], tmpp, upos)) {
        // Yes it does, so the current position is lpos
        pos = lpos;
        lpos = b[lpos].pw;
        while (b[lpos].ns != -1)
            lpos = b[lpos].ns;
    } else {
        pos = b.size();
        b.push_back(tmpp);
        Piece & p = b.back();
        p.nw = upos;
        if (lpos != -1) {
            // Calculate its previous piece in the S coordinate
            while (b[lpos].w(p.s) >= p.w(p.s)) {
                // Update links in the previous column
                b[lpos].ns = pos;
                if (b[lpos].w(p.s) == p.w(p.s)) p.ps = lpos;
                lpos = b[lpos].pw;
                while (b[lpos].ns != -1)
                    lpos = b[lpos].ns;
            }
            if (p.ps == -1)
                p.ps = lpos;
        }
    }
    // Update link in the next row
    if (upos != -1 && b[upos].pw == -1)
        b[upos].pw = pos;
    upos = pos;
}


StretchInformation::HSWFunction::HSWFunction(list<AppDesc> & apps, double power) {
    // Trivial case
    if (apps.empty()) {
        LogMsg("Ex.RI.Aggr", DEBUG) << "Creating availability info for empty queue and power " << power;
        minStretch = 1.0 / power;
        pieces.push_back(Piece(minStretch, 0.0, 0.0, SubFunction(power, 0.0, 0.0)));
        return;
    }

    // First (running) task is counted as a a single app on its own
    AppDesc firstApp = apps.front();
    if (apps.size() == 1) {
        LogMsg("Ex.RI.Aggr", DEBUG) << "Creating availability info for single app and power " << power;
        minStretch = (firstApp.a - firstApp.r) / firstApp.w;
        pieces.push_back(Piece(minStretch, 0.0, 0.0, SubFunction()));
        pieces.back().nw = 1;
        pieces.push_back(Piece(minStretch, firstApp.a, 0.0, SubFunction(power, 0.0, firstApp.a * power)));
        pieces.back().pw = 0;
        return;
    }

    LogMsg("Ex.RI.Aggr", DEBUG) << "Creating availability info for " << apps.size() << " apps and power " << power;

    // Calculate minimum stretch, assume tasks are correctly ordered.
    double e = 0.0;
    minStretch = 0.0;
    // For each task, calculate finishing time
    for (list<AppDesc>::iterator appi = apps.begin(); appi != apps.end(); ++appi) {
        e += appi->a;
        double stretch = (e - appi->r) / appi->w;
        if (stretch > minStretch) minStretch = stretch;
    }
    // Insert piece for first app
    pieces.push_back(Piece(minStretch, 0.0, 0.0, SubFunction()));
    // Extract first app from the list, it is not needed anymore
    apps.pop_front();

    // Calculate cross stretch values greater than the minimum stretch.
    vector<double> tmp;
    for (list<AppDesc>::iterator appi = apps.begin(); appi != apps.end(); ++appi) {
        for (list<AppDesc>::iterator appj = appi; appj != apps.end(); ++appj) {
            if (appj->w != appi->w) {
                double crossStretch = (appj->r - appi->r) / (appi->w - appj->w);
                if (crossStretch > minStretch) {
                    tmp.push_back(crossStretch);
                    std::push_heap(tmp.begin(), tmp.end());
                }
            }
        }
    }
    // Sort them and remove duplicate values
    vector<double> crossValues(1, infinity);
    double last = -1.0;
    for (int i = tmp.size(); i > 0; --i) {
        std::pop_heap(tmp.begin(), tmp.begin() + i);
        double value = tmp[i - 1];
        if (value != last) {
            crossValues.push_back(value);
            last = value;
        }
    }
    crossValues.push_back(minStretch);

    for (vector<double>::reverse_iterator nextsij = crossValues.rbegin(), cursij = nextsij++; nextsij != crossValues.rend(); cursij = nextsij++) {
        // Sort the app queue
        double sortStretch = *nextsij == infinity ? *cursij + 1.0 : (*cursij + *nextsij) / 2.0;
        for (list<AppDesc>::iterator appi = apps.begin(); appi != apps.end(); ++appi)
            appi->setStretch(sortStretch);
        apps.sort();
        double asum = firstApp.a;
        for (list<AppDesc>::iterator appi = apps.begin(); appi != apps.end(); ++appi)
            appi->asum = (asum += appi->a);

        // Calculate touch stretch values (stretch values where two tasks start or end touching each other)
        // between each pair of cross stretch values.
        tmp.clear();
        for (list<AppDesc>::iterator next = apps.begin(), appi = next++; appi != apps.end(); appi = next++) {
            double asum = 0.0;
            for (list<AppDesc>::iterator appj = next; appj != apps.end(); ++appj) {
                asum += appj->a;
                if (appj->w != appi->w) {
                    double touchStretch = (appj->r - appi->r - asum) / (appi->w - appj->w);
                    if (touchStretch > *cursij && touchStretch < *nextsij) {
                        // It is in the current stretch interval, check deadlines.
                        bool meetDeadlines = true;
                        double end = appi->getDeadline(touchStretch);
                        for (list<AppDesc>::iterator appk = next; appk != apps.end() && meetDeadlines; ++appk)
                            meetDeadlines = (end += appk->a) <= 1.0001 * appk->getDeadline(touchStretch);
                        if (meetDeadlines) {
                            tmp.push_back(touchStretch);
                            std::push_heap(tmp.begin(), tmp.end());
                        }
                    }
                }
            }
        }
        // Sort them and remove duplicate values
        vector<double> svalues(1, *nextsij);
        last = -1.0;
        for (int i = tmp.size(); i > 0; --i) {
            std::pop_heap(tmp.begin(), tmp.begin() + i);
            double value = tmp[i - 1];
            if (value != last) {
                svalues.push_back(value);
                last = value;
            }
        }
        svalues.push_back(*cursij);

        // With all the stretch values, calculate pieces in between
        int prevPieceS = -1;
        for (vector<double>::reverse_iterator nextS = svalues.rbegin(), curS = nextS++; nextS != svalues.rend(); curS = nextS++) {
            int prevPieceW = -1, nextps = -1;
            // Calculate the pieces
            for (list<AppDesc>::reverse_iterator appi = apps.rbegin(); appi != apps.rend();) {
                // Insert the hole after this app, looking for a previous piece which this hole is an extension of
                insertNextTo(Piece(*curS, appi->r, appi->w, SubFunction(power, 0.0, appi->asum * power)),
                             prevPieceS, prevPieceW, pieces);
                if (nextps == -1) nextps = prevPieceW;

                // Now calculate range limits for this app, which may span other apps
                Piece appPiece(*curS, appi->r, appi->w, SubFunction(0.0, appi->w * power, (appi->asum - appi->r) * power));
                double tmpNextS = *nextS == infinity ? *curS * 2.0 : *nextS,
                                  curEnd = appi->getDeadline(*curS),
                                           nextEnd = appi->getDeadline(tmpNextS);
                do {
                    // Advance apps as long as d_i > x_i+1 in both curS and nextS
                    curEnd -= appi->a;
                    nextEnd -= appi->a;
                    appPiece.d -= appi->a;
                    ++appi;
                } while (appi != apps.rend() && appi->getDeadline(*curS) >= curEnd && appi->getDeadline(tmpNextS) >= nextEnd);
                //} while (appi != apps.rend() && appi->getDeadline(*curS) >= 0.9999*curEnd && appi->getDeadline(tmpNextS) >= 0.9999*nextEnd);
                insertNextTo(appPiece, prevPieceS, prevPieceW, pieces);
            }
            // Last hole
            insertNextTo(Piece(*curS, firstApp.a, 0.0, SubFunction(power, 0.0, firstApp.a * power)),
                         prevPieceS, prevPieceW, pieces);
            // The piece under the last hole is the first one
            pieces[prevPieceW].pw = 0;
            // Also, the last hole of the first column is the piece over the first one
            if (prevPieceS == -1) pieces[0].nw = prevPieceW;
            prevPieceS = nextps;
        }
    }
}


void StretchInformation::HSWFunction::Piece::intersection(const vector<Piece> & b,
        const Piece & r, Piece * result, int & numConstructed) const {
// double const k = 1.0; //0.99999; // Ignore very little differences
// double startS = s < r.s ? r.s : s,
//   endS = Se > r.Se ? r.Se : Se;
// if (startS < k * endS) {
//  // Further squeeze the stretch interval, by calculating cross points between alternate top and bottom limits
//  double topBottomStretch = ee == r.es ? startS : (r.ds - de) / (ee - r.es);
//  if (startS < topBottomStretch && topBottomStretch < endS) {
//   if (ee > r.es) startS = topBottomStretch;
//   else endS = topBottomStretch;
//  }
//  double bottomTopStretch = es == r.ee ? startS : (r.de - ds) / (es - r.ee);
//  if (startS < bottomTopStretch && bottomTopStretch < endS) {
//   if (es > r.ee) endS = bottomTopStretch;
//   else startS = bottomTopStretch;
//  }
//  if (startS < k * endS) {
//   numConstructed = 0;
//   // Now we may have up to three ranges
//   // See where bottom limits cross each other
//   double s1 = es == r.es ? startS : (r.ds - ds) / (es - r.es);
//   double s2 = ee == r.ee ? startS : (r.de - de) / (ee - r.ee);
//   if (s1 < startS || endS < s1) s1 = startS;
//   if (startS < k * s1) {
//    // See where top limits cross each other, first between startS and s1
//    if (startS < k * s2 && s2 < k * s1) {
//     // We have two ranges here
//     {
//      double midS = (startS + s2) / 2.0;
//      double startD = ds / midS + es < r.ds / midS + r.es ? r.ds : ds,
//        endD = de / midS + ee > r.de / midS + r.ee ? r.de : de,
//        startE = ds / midS + es < r.ds / midS + r.es ? r.es : es,
//        endE = de / midS + ee > r.de / midS + r.ee ? r.ee : ee;
//      // Check that this interval is not empty
//      if (startD / midS + startE < endD / midS + endE)
//       b[numConstructed++] = Range(startS, s2, startD, startE, endD, endE);
//     }
//     {
//      double midS = (s2 + s1) / 2.0;
//      double startD = ds / midS + es < r.ds / midS + r.es ? r.ds : ds,
//        endD = de / midS + ee > r.de / midS + r.ee ? r.de : de,
//        startE = ds / midS + es < r.ds / midS + r.es ? r.es : es,
//        endE = de / midS + ee > r.de / midS + r.ee ? r.ee : ee;
//      // Check that this interval is not empty
//      if (startD / midS + startE < endD / midS + endE)
//       b[numConstructed++] = Range(s2, s1, startD, startE, endD, endE);
//     }
//    } else {
//     // Only one range
//     double midS = (startS + s1) / 2.0;
//     double startD = ds / midS + es < r.ds / midS + r.es ? r.ds : ds,
//       endD = de / midS + ee > r.de / midS + r.ee ? r.de : de,
//       startE = ds / midS + es < r.ds / midS + r.es ? r.es : es,
//       endE = de / midS + ee > r.de / midS + r.ee ? r.ee : ee;
//     // Check that this interval is not empty
//     if (startD / midS + startE < endD / midS + endE)
//      b[numConstructed++] = Range(startS, s1, startD, startE, endD, endE);
//    }
//   }
//   // Now with (s1, endS), see where top limits cross each other
//   if (s1 < k * s2 && s2 < k * endS) {
//    // We have two ranges here
//    {
//     double midS = (s1 + s2) / 2.0;
//     double startD = ds / midS + es < r.ds / midS + r.es ? r.ds : ds,
//       endD = de / midS + ee > r.de / midS + r.ee ? r.de : de,
//       startE = ds / midS + es < r.ds / midS + r.es ? r.es : es,
//       endE = de / midS + ee > r.de / midS + r.ee ? r.ee : ee;
//     // Check that this interval is not empty
//     if (startD / midS + startE < endD / midS + endE)
//      b[numConstructed++] = Range(s1, s2, startD, startE, endD, endE);
//    }
//    {
//     double midS = endS == inf ? s2 + 1.0 : (s2 + endS) / 2.0;
//     double startD = ds / midS + es < r.ds / midS + r.es ? r.ds : ds,
//       endD = de / midS + ee > r.de / midS + r.ee ? r.de : de,
//       startE = ds / midS + es < r.ds / midS + r.es ? r.es : es,
//       endE = de / midS + ee > r.de / midS + r.ee ? r.ee : ee;
//     // Check that this interval is not empty
//     if (startD / midS + startE < endD / midS + endE)
//      b[numConstructed++] = Range(s2, endS, startD, startE, endD, endE);
//    }
//   } else {
//    // Only one range
//    double midS = endS == inf ? s1 + 1.0 : (s1 + endS) / 2.0;
//    double startD = ds / midS + es < r.ds / midS + r.es ? r.ds : ds,
//      endD = de / midS + ee > r.de / midS + r.ee ? r.de : de,
//      startE = ds / midS + es < r.ds / midS + r.es ? r.es : es,
//      endE = de / midS + ee > r.de / midS + r.ee ? r.ee : ee;
//    // Check that this interval is not empty
//    if (startD / midS + startE < endD / midS + endE)
//     b[numConstructed++] = Range(s1, endS, startD, startE, endD, endE);
//   }
//  }
// }
}


template<class S> void StretchInformation::HSWFunction::stepper(const HSWFunction & l, const HSWFunction & r, S & step) {
    // First, calculate the intersection of the empty part of l in r
    bool left = l.getMinStretch() > r.getMinStretch();
    const vector<Piece> & ps = left ? r.pieces : l.pieces;
    double minStretch = left ? l.getMinStretch() : r.getMinStretch();

    vector<bool> notVisitedYet(ps.size(), true);
    vector<int> pieceStack(1, 0);
    pieceStack.reserve(ps.size());
    while (!pieceStack.empty()) {
        double Se = infinity, de = infinity, ee = infinity;
        const Piece & p = ps[pieceStack.back()];
        notVisitedYet[pieceStack.back()] = false;
        pieceStack.pop_back();
        if (p.nw != -1) {
            if (notVisitedYet[p.nw])
                pieceStack.push_back(p.nw);
            de = ps[p.nw].d;
            ee = ps[p.nw].e;
        }
        if (p.ns != -1) {
            if (ps[p.ns].s < minStretch) {
                if (notVisitedYet[p.ns])
                    pieceStack.push_back(p.ns);
                Se = ps[p.ns].s;
            } else Se = minStretch;
        }
        if (left)
            step(p.s, p.d, p.e, Se, de, ee, SubFunction(), p.f);
        else
            step(p.s, p.d, p.e, Se, de, ee, p.f, SubFunction());
    }

    // TODO
// Range intersections[3];
// int numIntersections;
// // For the rest, for each piece in l, calculate intersections in r
// for (vector<std::pair<Range, SubFunction> >::const_iterator rangei = l.pieces.begin(); rangei != l.pieces.end(); ++rangei) {
//  for (vector<std::pair<Range, SubFunction> >::const_iterator rangej = r.pieces.begin(); rangej != r.pieces.end(); ++rangej) {
//   rangei->first.intersection(rangej->first, intersections, numIntersections);
//   for (int i = 0; i < numIntersections; ++i) {
//    step(intersections[i], rangei->second, rangej->second);
//   }
//  }
// }
}


struct StretchInformation::HSWFunction::minStep {
// void operator()(const Range & i, const SubFunction & l, const SubFunction & r) {
//  // Check if subfunctions cross each other
//  if (l.a != r.a) {
//   // When l.a > r.a, l function is greater than r over the cut, and lower under it.
//   const StretchInformation::HSWFunction::SubFunction * minUp = l.a > r.a ? &r : &l;
//   const StretchInformation::HSWFunction::SubFunction * minDown = l.a > r.a ? &l : &r;
//   // Calculate the cut, and use the intersection algorithm to extract the resulting pieces.
//   double dcut = (l.c - r.c) / (l.a - r.a), ecut = (r.b - l.b) / (l.a - r.a);
//   // Intervals where minUp is lower
//   i.intersection(Range(i.Ss, i.Se, dcut, ecut, infinity, infinity), intersections, numIntersections);
//   for (int j = 0; j < numIntersections; ++j)
//    insert(intersections[j], *minUp, pieces);
//   // Intervals where minDown is lower
//   i.intersection(Range(i.Ss, i.Se, 0, 0, dcut, ecut), intersections, numIntersections);
//   for (int j = 0; j < numIntersections; ++j)
//    insert(intersections[j], *minDown, pieces);
//  } else if (l.b != r.b) {
//   // When l.b > r.b, l function is greater than r after the cut, and lower before it.
//   const StretchInformation::HSWFunction::SubFunction * minAfter = l.b > r.b ? &r : &l;
//   const StretchInformation::HSWFunction::SubFunction * minBefore = l.b > r.b ? &l : &r;
//   // Calculate the cut, and use the intersection algorithm to extract the resulting pieces.
//   double Scut = (l.c - r.c) / (l.b - r.b);
//   Range cut(i);
//   // Intervals where minAfter is lower
//   cut.Ss = Scut;
//   insert(cut, *minAfter, pieces);
//   // Intervals where minBefore is lower
//   cut.Ss = i.Ss;
//   cut.Se = Scut;
//   insert(cut, *minBefore, pieces);
//  } else {
//   // They do not cut each other
//   insert(i, l.c > r.c ? l : r, pieces);
//  }
// }
//
    vector<Piece> pieces;
// Range intersections[3];
    int numIntersections;
};


void StretchInformation::HSWFunction::min(const HSWFunction & l, const HSWFunction & r) {
    minStep step;
// stepper(l, r, step);
    pieces.swap(step.pieces);
    minStretch = l.minStretch > r.minStretch ? r.minStretch : l.minStretch;
}


struct StretchInformation::HSWFunction::maxStep {
// void operator()(const Range & i, const SubFunction & l, const SubFunction & r) {
//  // Check if subfunctions cross each other
//  if (l.a != r.a) {
//   // When l.a > r.a, l function is greater than r over the cut, and lower under it.
//   const StretchInformation::HSWFunction::SubFunction * maxUp = l.a > r.a ? &l : &r;
//   const StretchInformation::HSWFunction::SubFunction * maxDown = l.a > r.a ? &r : &l;
//   // Calculate the cut, and use the intersection algorithm to extract the resulting pieces.
//   double dcut = (l.c - r.c) / (l.a - r.a), ecut = (r.b - l.b) / (l.a - r.a);
//   // Intervals where maxUp is greater
//   i.intersection(Range(i.Ss, i.Se, dcut, ecut, infinity, infinity), intersections, numIntersections);
//   for (int j = 0; j < numIntersections; ++j)
//    insert(intersections[j], *maxUp, pieces);
//   // Intervals where maxDown is greater
//   i.intersection(Range(i.Ss, i.Se, 0, 0, dcut, ecut), intersections, numIntersections);
//   for (int j = 0; j < numIntersections; ++j)
//    insert(intersections[j], *maxDown, pieces);
//  } else if (l.b != r.b) {
//   // When l.b > r.b, l function is greater than r after the cut, and lower before it.
//   const StretchInformation::HSWFunction::SubFunction * maxAfter = l.b > r.b ? &l : &r;
//   const StretchInformation::HSWFunction::SubFunction * maxBefore = l.b > r.b ? &r : &l;
//   // Calculate the cut, and use the intersection algorithm to extract the resulting pieces.
//   double Scut = (l.c - r.c) / (l.b - r.b);
//   Range cut(i);
//   // Intervals where maxAfter is greater
//   cut.Ss = Scut;
//   insert(cut, *maxAfter, pieces);
//   // Intervals where maxBefore is greater
//   cut.Ss = i.Ss;
//   cut.Se = Scut;
//   insert(cut, *maxBefore, pieces);
//  } else {
//   // They do not cut each other
//   insert(i, l.c > r.c ? r : l, pieces);
//  }
// }

    vector<Piece> pieces;
// Range intersections[3];
    int numIntersections;
};


void StretchInformation::HSWFunction::max(const HSWFunction & l, const HSWFunction & r) {
    maxStep step;
// stepper(l, r, step);
    pieces.swap(step.pieces);
    minStretch = l.minStretch > r.minStretch ? r.minStretch : l.minStretch;
}


struct StretchInformation::HSWFunction::sqdiffStep {
    sqdiffStep(double _sh, double _wh) : sh(_sh), wh(_wh), result(0.0) {}

    void operator()(double Ss, double ds, double es, double Se, double de, double ee, const SubFunction & l, const SubFunction & r) {
        // Compute the double integral of the squared difference between l and r in the given range, that is:
        // integrate(integrate((S*w*l.a + S*l.b - l.c - S*w*r.a - S*r.b + r.c)^2, w, ds/S + es, de/S + ee), S, Ss, Se)
        double DA = l.a - r.a, DB = l.b - r.b, DC = l.c - r.c;
        double DAA = DA * DA, DAB = DA * DB, DAC = DA * DC, DBB = DB * DB, DBC = DB * DC, DCC = DC * DC;
        // Adjust horizons
        if (Se == infinity) Se = sh;
        if (de == infinity) de = 0;
        if (ee == infinity) ee = wh;
        double ds2 = ds * ds, de2 = de * de, es2 = es * es, ee2 = ee * ee, Ss2 = Ss * Ss, Se2 = Se * Se;
        double tmp = \
                     (DAA * (de * de2 - ds * ds2) - 3 * DAC * (de2 - ds2) + 3 * DCC * (de - ds)) * log(Se / Ss) / 3 + \
                     (DAA * (ee * de2 - es * ds2) + DAB * (de2 - ds2) - 2 * DAC * (de * ee - ds * es) - 2 * DBC * (de - ds) + DCC * (ee - es)) * (Se - Ss) + \
                     (DAA * (de * ee2 - ds * es2) - DAC * (ee2 - es2) + 2 * DAB * (de * ee - ds * es) - 2 * DBC * (ee - es) + DBB * (de - ds)) * (Se2 - Ss2) / 2 + \
                     (DAA * (ee * ee2 - es * es2) + 3 * DAB * (ee2 - es2) + 3 * DBB * (ee - es)) * (Se * Se2 - Ss * Ss2) / 9;
        result += tmp;
    }

    double sh, wh, result;
};


double StretchInformation::HSWFunction::sqdiff(const HSWFunction & r, double sh, double wh) const {
    sqdiffStep step(sh, wh);
    stepper(*this, r, step);
    return step.result;
}


struct StretchInformation::HSWFunction::meanLossStep {
    meanLossStep(unsigned int _lv, unsigned int _rv, double _sh, double _wh) : ss(_sh, _wh), lv(_lv), rv(_rv) {}

// void operator()(const Range & i, const SubFunction & l, const SubFunction & r) {
//  ss(i, l, r);
//  SubFunction mean((lv * l.a + rv * r.a) / (lv + rv), (lv * l.b + rv * r.b) / (lv + rv), (lv * l.c + rv * r.c) / (lv + rv));
//  insert(i, mean, pieces);
// }

    sqdiffStep ss;
    vector<Piece> pieces;
    unsigned int lv, rv;
};


double StretchInformation::HSWFunction::meanAndLoss(const HSWFunction & l, const HSWFunction & r, unsigned int lv, unsigned int rv, double sh, double wh) {
    meanLossStep step(lv, rv, sh, wh);
// stepper(l, r, step);
    minStretch = l.minStretch > r.minStretch ? r.minStretch : l.minStretch;
    pieces.swap(step.pieces);
    return step.ss.result * lv * rv / (double)(lv + rv);
}


//struct StretchInformation::HSWFunction::IntervalCluster {
// double sh, wh;
// unsigned int value;
// double accumHsq;
// Range r;
// SubFunction sf;
// std::list<std::pair<Range, SubFunction> > clusteredIntervals;
//
// IntervalCluster() : value(1), accumHsq(0.0) {}
//
// /// Comparison operator
// bool operator==(const IntervalCluster & o) const {
//  return value == o.value && r == o.r && sf == o.sf;
// }
//
// /// Distance operator for the clustering algorithm
// double distance(const IntervalCluster & o, IntervalCluster & sum) const {
//  if (far(o)) return std::numeric_limits<double>::infinity();
//  sum.aggregate(*this, o);
//  return sum.accumHsq / sum.value;
// }
//
// bool far(const IntervalCluster & o) const {
//  return !r.isExtensionOf(o.r) && !o.r.isExtensionOf(r);
// }
//
// /// Aggregation operator for the clustering algorithm
// void aggregate(const IntervalCluster & o) {
//  aggregate(*this, o);
// }
//
// /// Constructs a cluster from the aggregation of two. It is mainly useful for the distance operator.
// void aggregate(const IntervalCluster & l, const IntervalCluster & o) {
//  SubFunction newMeanH((l.value * l.sf.a + o.value * o.sf.a) / (l.value + o.value),
//    (l.value * l.sf.b + o.value * o.sf.b) / (l.value + o.value),
//    (l.value * l.sf.c + o.value * o.sf.c) / (l.value + o.value));
//  std::list<std::pair<Range, SubFunction> > newCl = l.clusteredIntervals;
//  newCl.insert(newCl.end(), o.clusteredIntervals.begin(), o.clusteredIntervals.end());
//  sqdiffStep step(sh, wh);
//  for (std::list<std::pair<Range, SubFunction> >::iterator it = newCl.begin();
//    it != newCl.end(); ++it)
//   step(it->first, it->second, newMeanH);
//  accumHsq = step.result;
//
//  sf = newMeanH;
//  if (l.r.Ss == o.r.Se)
//   r = Range(o.r.Ss, l.r.Se, l.r.ds, l.r.es, l.r.de, l.r.ee);
//  else if (o.r.Ss == l.r.Se)
//   r = Range(l.r.Ss, o.r.Se, l.r.ds, l.r.es, l.r.de, l.r.ee);
//  else if (l.r.ds == o.r.de && l.r.es == o.r.ee)
//   r = Range(l.r.Ss, l.r.Se, o.r.ds, o.r.es, l.r.de, l.r.ee);
//  else
//   r = Range(l.r.Ss, l.r.Se, l.r.ds, l.r.es, o.r.de, o.r.ee);
//  clusteredIntervals.swap(newCl);
//  value = l.value + o.value;
// }
//
// /// Dummy method
// friend std::ostream & operator<<(std::ostream & os, const IntervalCluster & o) { return os; }
//};


struct StretchInformation::HSWFunction::ReduceOption {
    struct ReducedPiece : public Piece {
        list<Piece *> joinedPieces;

        ReducedPiece(Piece * p) : Piece(*p), joinedPieces(1, p) {}
    };

    ReduceOption(vector<Piece> & p) : cost(0.0) {
        pieces.resize(p.size());
        for (unsigned int i = 0; i < p.size(); ++i)
            pieces[i].reset(new ReducedPiece(&p[i]));
    }
    ReduceOption(const ReduceOption & copy) : pieces(copy.pieces), cost(copy.cost) {}

    bool operator<(const ReduceOption & r) const {
        return cost < r.cost;
    }

    vector<boost::shared_ptr<ReducedPiece> > pieces;
    double cost;
};


double StretchInformation::HSWFunction::reduce(double sh, double wh, unsigned int quality) {
    double result = 0.0;
    if (pieces.size() > StretchInformation::numPieces) {
        list<ReduceOption> options;
        options.push_back(ReduceOption(pieces));
        while (options.front().pieces.size() > StretchInformation::numPieces) {
            vector<boost::shared_ptr<ReduceOption::ReducedPiece> > & b = options.front().pieces;
            // Calculate each immediate possibilities
            for (unsigned int i = 0; i < b.size(); ++i) {
                boost::shared_ptr<ReduceOption::ReducedPiece> p = b[i];
                // Check if it can be merged with upper piece
                if (p->nw != -1) {
                    boost::shared_ptr<ReduceOption::ReducedPiece> pp = b[p->nw];
                    if (p->s == pp->s && (p->ns == pp->ns || (p->ns != -1 && pp->ns != -1 && b[p->ns]->s == b[pp->ns]->s))) {
                        // Merge p with pp
                        options.push_back(options.front());
                        vector<boost::shared_ptr<ReduceOption::ReducedPiece> > & bb = options.back().pieces;
                        //bb[i].reset(new ReducedPiece)
                    }
                }
                // Check if it can be merged with rightmost piece
                if (p->ns != -1) {
                    boost::shared_ptr<ReduceOption::ReducedPiece> pp = b[p->ns];
                    if (p->d == pp->d && p->e == pp->e && (p->nw == pp->nw ||
                                                           (p->nw != -1 && pp->nw != -1 && b[p->nw]->d == b[pp->nw]->d && b[p->nw]->e == b[pp->nw]->e))) {
                        // Merge p with pp
                    }
                }
            }
            // If there are not enough options, check for stripping
            if (options.size() < quality) {
                for (unsigned int i = 0; i < b.size(); ++i) {
                }
            }
            options.sort();
        }

// // Use clusterization
// ClusteringVector<IntervalCluster> cl(pieces.size());
// for (unsigned int i = 0; i < pieces.size(); ++i) {
//  cl[i].r = pieces[i].first;
//  cl[i].sh = sh;
//  cl[i].wh = wh;
//  cl[i].sf = pieces[i].second;
//  cl[i].clusteredIntervals.push_back(make_pair(cl[i].r, cl[i].sf));
// }
// cl.clusterize(StretchInformation::numPieces);
// pieces.clear();
// for (unsigned int i = 0; i < cl.getSize(); ++i) {
//  insert(cl[i].r, cl[i].sf, pieces);
//  result += cl[i].accumHsq;
// }
    }
    return result;
}


uint64_t StretchInformation::HSWFunction::getAvailability(double S, double w) const {
    // Find piece that contains (S, w)
    if (S >= minStretch && w >= 0.0) {
        for (int i = 0; true;) {
            const Piece & p = pieces[i];
            if (p.ns != -1 && pieces[p.ns].s <= S)
                i = p.ns;
            else if (p.nw != -1 && pieces[p.nw].w(S) <= w)
                i = p.nw;
            else return p.f.value(S, w);
        }
    }
    return 0;
}


double StretchInformation::MDHCluster::distance(const MDHCluster & r, MDHCluster & sum) const {
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
            double loss = (sum.accumHsq / reference->availRange / sum.value);
            result += loss;
        }
    }
    return result;
}


bool StretchInformation::MDHCluster::far(const MDHCluster & r) const {
    if (reference->memRange) {
        if (((minM - reference->minM) * numIntervals / reference->memRange) != ((r.minM - reference->minM) * numIntervals / reference->memRange))
            return true;
    }
    if (reference->diskRange) {
        if (((minD - reference->minD) * numIntervals / reference->diskRange) != ((r.minD - reference->minD) * numIntervals / reference->diskRange))
            return true;
    }
// TODO
// if (minA.isFree() != r.minA.isFree()) return true;
// if (reference->availRange) {
//  if (floor(minA.sqdiff(reference->minA, reference->aggregationTime, reference->horizon) * numIntervals / reference->availRange)
//    != floor(r.minA.sqdiff(reference->minA, reference->aggregationTime, reference->horizon) * numIntervals / reference->availRange)) {
//   return true;
//  }
// }
    return false;
}


void StretchInformation::MDHCluster::aggregate(const MDHCluster & r) {
    aggregate(*this, r);
}


void StretchInformation::MDHCluster::aggregate(const MDHCluster & l, const MDHCluster & r) {
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

    HSWFunction newMeanH;
    accumHsq = l.accumHsq + r.accumHsq
               + newMeanH.meanAndLoss(l.meanH, r.meanH, l.value, r.value, reference->stretchHorizon, reference->lengthHorizon);

    minM = newMinM;
    minD = newMinD;
    meanH.swap(newMeanH);
    value = l.value + r.value;
    // Reduce the mean function to avoid piece explosion
    accumHsq += value * meanH.reduce(reference->stretchHorizon, reference->lengthHorizon);
}


//void StretchInformation::MDHCluster::reduce() {
// accumHsq += value * meanH.reduce(reference->stretchHorizon, reference->lengthHorizon);
//}


void StretchInformation::setAvailability(uint32_t m, uint32_t d, std::list<AppDesc> & apps, double power) {
    minM = maxM = m;
    minD = maxD = d;
    summary.clear();
    summary.pushBack(MDHCluster(this, m, d, apps, power));
    minH = maxH = summary[0].meanH;
    minH.getHorizon(stretchHorizon, lengthHorizon);
    minimumStretch = maximumStretch = minH.getMinStretch();
}


unsigned int StretchInformation::getAvailableSlots(const TaskDescription & req, double stretch) {
    unsigned int result = 0;

    unsigned int size = summary.getSize();
    for (unsigned int i = 0; i < size; ++i)
        if (summary[i].fulfills(req))
            result += (unsigned int)floor(summary[i].meanH.getAvailability(stretch, req.getAppLength()) / req.getLength());

    return result;
}


StretchInformation::SpecificAF::SpecificAF(HSWFunction & fi, unsigned int wi, unsigned int ai, unsigned int nodes)
        : k(0), func(fi), w(wi), a(ai), numNodes(nodes) {
    // For every range that cuts w = wi, add its function to the list, in order
    const vector<HSWFunction::Piece> & b = fi.getPieces();
    // Look for the first piece
    double minStretch = fi.getMinStretch();
    int i = 0;
    while (b[i].nw != -1 && b[b[i].nw].w(minStretch) <= w)
        i = b[i].nw;
    functions.push_back(make_pair(minStretch, b[i].f));
    while (true) {
        const HSWFunction::Piece & p = b[i];
        // Check if it goes out through the lower limit
        if (p.d <= 0.0 && p.e > w && (p.ns == -1 || p.d / (w - p.e) < b[p.ns].s)) {
            i = p.pw;
            functions.push_back(make_pair(p.d / (w - p.e), b[i].f));
        } else if (p.nw != -1) {
            // Check if it goes out through the upper limit
            const HSWFunction::Piece & u = b[p.nw];
            if (u.d > 0.0 && u.e < w && (p.ns == -1 || u.d / (w - u.e) < b[p.ns].s)) {
                i = p.nw;
                functions.push_back(make_pair(u.d / (w - u.e), u.f));
            }
        } else if (p.ns != -1) {
            // It goes right
            i = p.ns;
            functions.push_back(make_pair(b[i].s, b[i].f));
        } else break;
    }

    it = functions.begin();
    step();
}


void StretchInformation::getSpecificFunctions(const TaskDescription & req, std::list<SpecificAF> & specificFunctions) {
    unsigned int size = summary.getSize();
    for (unsigned int i = 0; i < size; ++i)
        if (summary[i].fulfills(req))
            specificFunctions.push_back(SpecificAF(summary[i].meanH, req.getAppLength(), req.getLength(), summary[i].value));
}


void StretchInformation::join(const StretchInformation & r) {
    if (!r.summary.isEmpty()) {
        LogMsg("Ex.RI.Aggr", DEBUG) << "Aggregating two summaries:";

        if (summary.isEmpty()) {
            // operator= forbidden
            minM = r.minM;
            maxM = r.maxM;
            minD = r.minD;
            maxD = r.maxD;
            minH.min(minH, r.minH);
            maxH.max(maxH, r.maxH);
            stretchHorizon = r.stretchHorizon;
            lengthHorizon = r.lengthHorizon;
            minimumStretch = r.minimumStretch;
            maximumStretch = r.maximumStretch;
        } else {
            if (minM > r.minM) minM = r.minM;
            if (maxM < r.maxM) maxM = r.maxM;
            if (minD > r.minD) minD = r.minD;
            if (maxD < r.maxD) maxD = r.maxD;
            minH.min(minH, r.minH);
            maxH.max(maxH, r.maxH);
            if (lengthHorizon < r.lengthHorizon)
                lengthHorizon = r.lengthHorizon;
            if (stretchHorizon < r.stretchHorizon)
                stretchHorizon = r.stretchHorizon;
            if (minimumStretch > r.minimumStretch)
                minimumStretch = r.minimumStretch;
            if (maximumStretch < r.maximumStretch)
                maximumStretch = r.maximumStretch;
        }
        summary.add(r.summary);
        unsigned int size = summary.getSize();
        for (unsigned int i = 0; i < size; i++)
            summary[i].reference = this;
    }
}


void StretchInformation::reduce() {
    // Set up clustering variables
    memRange = maxM - minM;
    diskRange = maxD - minD;
    availRange = maxH.sqdiff(minH, stretchHorizon, lengthHorizon);
    summary.clusterize(numClusters);
// unsigned int size = summary.getSize();
// for (unsigned int i = 0; i < size; i++)
//  summary[i].reduce();
}


//void StretchInformation::update(uint32_t n, uint64_t a) {
// uint32_t log = 0;
// for (double i = 1.0; i < a; i *= taskSizeBase, log++);
// for (vector<ReleaseDate>::iterator i = m.begin(); i != m.end(); i++) {
//  for (AppSize::iterator j = i->second.begin(); j != i->second.end(); j++) {
//   double nPrime = n * pow(taskSizeBase, log - taskSizeMin);
//   for (TaskSize::iterator k = j->begin(); k != j->end(); k++) {
//    k->update((uint32_t)ceil(nPrime));
//    nPrime /= taskSizeBase;
//   }
//  }
// }
//}


void StretchInformation::output(std::ostream & os) const {
    os << '(' << minimumStretch << ", " << maximumStretch << ") ";
    os << '(' << minM << "MB, " << maxM << "MB) ";
    os << '(' << minD << "MB, " << maxD << "MB) ";
    os << '(' << minH << ", " << maxH << ") (";
    for (unsigned int i = 0; i < summary.getSize(); i++)
        os << summary[i] << ',';
    os << ')';
}

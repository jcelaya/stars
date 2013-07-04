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

#include <fstream>
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include "LDeltaFunction.hpp"
#include "TestHost.hpp"
#include "Logger.hpp"
#include "../RandomQueueGenerator.hpp"
using namespace std;


namespace stars {

string plot(LDeltaFunction & f) {
    ostringstream os;
    if (f.getPoints().empty()) {
        os << "0,0" << endl;
        os << "100000000000," << (unsigned long)(f.getSlope() * 100000.0) << endl;
    } else {
        for (vector<pair<Time, double> >::const_iterator it = f.getPoints().begin(); it != f.getPoints().end(); it++)
            os << it->first.getRawDate() << ',' << it->second << endl;
    }
    return os.str();
}

struct LDeltaFunctionFixture {
    LDeltaFunctionFixture() {
        TestHost::getInstance().reset();
    }

    RandomQueueGenerator rqg;
};

BOOST_FIXTURE_TEST_SUITE(LDeltaFunctionTest, LDeltaFunctionFixture)

BOOST_AUTO_TEST_CASE(LDeltaFunction_create) {
    Time lastTime = Time::getCurrentTime();
    double lastAvail = 0.0;
    LDeltaFunction f(35, rqg.createNLengthQueue(20, 35));
    const LDeltaFunction::PieceVector & p = f.getPoints();
    for (auto & piece : p) {
        BOOST_CHECK_GE(piece.first, lastTime);
        lastTime = piece.first;
        BOOST_CHECK_GE(piece.second, lastAvail);
        lastAvail = piece.second;
    }
    LogMsg("Test.RI", INFO) << "Random Function: " << f;
}


BOOST_AUTO_TEST_CASE(LDeltaFunction_min) {
    double f1power = rqg.getRandomPower(), f2power = rqg.getRandomPower();
    LDeltaFunction f1(f1power, rqg.createNLengthQueue(20, f1power)),
            f2(f2power, rqg.createNLengthQueue(20, f2power)),
            r;
    r.min(f1, f2);
    BOOST_CHECK_GT(r.getPoints().size(), 0);
    BOOST_CHECK_GT(r.getSlope(), 0.0);
    for (auto & i : r.getPoints()) {
        BOOST_CHECK_LE(i.second * 0.99999, f1.getAvailabilityBefore(i.first));
        BOOST_CHECK_LE(i.second * 0.99999, f2.getAvailabilityBefore(i.first));
    }
}


BOOST_AUTO_TEST_CASE(LDeltaFunction_max) {
    double f1power = rqg.getRandomPower(), f2power = rqg.getRandomPower();
    LDeltaFunction f1(f1power, rqg.createNLengthQueue(20, f1power)),
            f2(f2power, rqg.createNLengthQueue(20, f2power)),
            r;
    r.max(f1, f2);
    BOOST_CHECK_GT(r.getPoints().size(), 0);
    BOOST_CHECK_GT(r.getSlope(), 0.0);
    for (auto & i : r.getPoints()) {
        BOOST_CHECK_GE(i.second * 1.00001, f1.getAvailabilityBefore(i.first));
        BOOST_CHECK_GE(i.second * 1.00001, f2.getAvailabilityBefore(i.first));
    }
}


BOOST_AUTO_TEST_CASE(LDeltaFunction_lc) {
    double f1power = rqg.getRandomPower(), f2power = rqg.getRandomPower();
    LDeltaFunction f1(f1power, rqg.createNLengthQueue(20, f1power)),
            f2(f2power, rqg.createNLengthQueue(20, f2power)),
            r;
    r.lc(f1, f2, 3, 4);
    BOOST_CHECK_GT(r.getPoints().size(), 0);
    BOOST_CHECK_GT(r.getSlope(), 0.0);
    for (auto & i : r.getPoints()) {
        BOOST_CHECK_CLOSE(i.second, 3*f1.getAvailabilityBefore(i.first) + 4*f2.getAvailabilityBefore(i.first), 0.000001);
    }
}


BOOST_AUTO_TEST_CASE(LDeltaFunction_operations) {
    Time ct = Time::getCurrentTime();
    Time h = ct + Duration(100000.0);
    LDeltaFunction::setNumPieces(8);

    // Min/max and sum of several functions
    LogMsg("Test.RI", INFO) << "";
    ofstream of("af_test.stat");
    for (int i = 0; i < 100; i++) {
        LogMsg("Test.RI", INFO) << "Functions " << i;

        double f11power = rqg.getRandomPower(),
               f12power = rqg.getRandomPower(),
               f13power = rqg.getRandomPower(),
               f21power = rqg.getRandomPower(),
               f22power = rqg.getRandomPower();
        LDeltaFunction
               f11(f11power, rqg.createRandomQueue(f11power)),
               f12(f12power, rqg.createRandomQueue(f12power)),
               f13(f13power, rqg.createRandomQueue(f13power)),
               f21(f21power, rqg.createRandomQueue(f21power)),
               f22(f22power, rqg.createRandomQueue(f22power));
        LDeltaFunction min, max;
        min.min(f11, f12);
        min.min(min, f13);
        min.min(min, f21);
        min.min(min, f22);
        max.max(f11, f12);
        max.max(max, f13);
        max.max(max, f21);
        max.max(max, f22);

        // Join f11 with f12
        LDeltaFunction f112;
        double accumAsq112 = f112.minAndLoss(f11, f12, 1, 1, LDeltaFunction(), LDeltaFunction(), ct, h);
        BOOST_CHECK_GE(accumAsq112 * 1.0001, f112.sqdiff(f11, ct, h) + f112.sqdiff(f12, ct, h));
        LDeltaFunction accumAln112;
        accumAln112.max(f11, f12);
        BOOST_CHECK_CLOSE(accumAsq112, f11.sqdiff(f12, ct, h), 0.0001);

        // join f112 with f13, and that is f1
        LDeltaFunction f1;
        double accumAsq1 = f1.minAndLoss(f112, f13, 2, 1, accumAln112, LDeltaFunction(), ct, h) + accumAsq112;
        BOOST_CHECK_GE(accumAsq1 * 1.0001, f1.sqdiff(f11, ct, h) + f1.sqdiff(f12, ct, h) + f1.sqdiff(f13, ct, h));
        LDeltaFunction accumAln1;
        accumAln1.max(accumAln112, f13);

        // join f21 with f22, and that is f2
        LDeltaFunction f2;
        double accumAsq2 = f2.minAndLoss(f21, f22, 1, 1, LDeltaFunction(), LDeltaFunction(), ct, h);
        BOOST_CHECK_GE(accumAsq2 * 1.0001, f2.sqdiff(f21, ct, h) + f2.sqdiff(f22, ct, h));
        LDeltaFunction accumAln2;
        accumAln2.max(f21, f22);

        // join f1 with f2, and that is f
        LDeltaFunction f;
        double accumAsq = f.minAndLoss(f1, f2, 3, 2, accumAln1, accumAln2, ct, h) + accumAsq1 + accumAsq2;
        BOOST_CHECK_GE(accumAsq * 1.0001, f.sqdiff(f11, ct, h) + f.sqdiff(f12, ct, h) + f.sqdiff(f13, ct, h) + f.sqdiff(f21, ct, h) + f.sqdiff(f22, ct, h));
        LDeltaFunction accumAln;
        accumAln.max(accumAln1, accumAln2);

        // Print functions
        of << "# Functions " << i << endl;
        //              of << "# minloss(a, b) = " << loss << "; a.loss(b) = " << a.loss(b, ct, ref) << "; b.loss(a) = " << b.loss(a, ct, ref) << "; a.loss(min) = " << a.loss(r, ct, ref)
        //                              << "; b.loss(min) = " << b.loss(r, ct, ref) << "; max.loss(min) = " << t.loss(r, ct, ref) << "; min.loss(max) = " << r.loss(t, ct, ref) << endl;
        //              // Take two random values j and k
        //              unsigned int j = uniform(2, 30, 1), k = uniform(2, 30, 1);
        //              of << "# " << j << "a.loss(min) = " << (j * a.loss(r, ct, ref)) << "; " << k << "b.loss(min) = " << (k * b.loss(r, ct, ref))
        //                              << "; sum " << (j * a.loss(r, ct, ref) + k * b.loss(r, ct, ref)) << "; a.loss(b, " << j << ", " << k << ") = " << min.minAndLoss(a, b, j, k, ct, ref) << endl;
        of << "# f11: " << f11 << endl << plot(f11) << endl;
        of << "# f12: " << f12 << endl << plot(f12) << endl;
        of << "# f112: " << f112 << endl << "# accumAsq112 " << accumAsq112 << " =? " << (f112.sqdiff(f11, ct, h) + f112.sqdiff(f12, ct, h)) << endl << plot(f112) << endl;
        of << "# accumAln112: " << accumAln112 << endl << plot(accumAln112) << endl;
        of << "# f13: " << f13 << endl << plot(f13) << endl;
        of << "# f1: " << f1 << endl << "# accumAsq1 " << accumAsq1 << " =? " << (f1.sqdiff(f11, ct, h) + f1.sqdiff(f12, ct, h) + f1.sqdiff(f13, ct, h)) << endl << plot(f1) << endl;
        of << "# accumAln1: " << accumAln1 << endl << plot(accumAln1) << endl;
        of << "# f21: " << f21 << endl << plot(f21) << endl;
        of << "# f22: " << f22 << endl << plot(f22) << endl;
        of << "# f2: " << f2 << endl << "# accumAsq2 " << accumAsq2 << " =? " << (f2.sqdiff(f21, ct, h) + f2.sqdiff(f22, ct, h)) << endl << plot(f2) << endl;
        of << "# accumAln2: " << accumAln2 << endl << plot(accumAln2) << endl;
        of << "# f: " << f << endl << "# accumAsq " << accumAsq << " =? " << (f.sqdiff(f11, ct, h) + f.sqdiff(f12, ct, h) + f.sqdiff(f13, ct, h) + f.sqdiff(f21, ct, h) + f.sqdiff(f22, ct, h)) << endl << plot(f) << endl;
        of << "# accumAln: " << accumAln << endl << plot(accumAln) << endl;
        accumAsq += f.reduceMin(5, accumAln, ct, h);
        BOOST_CHECK_GE(accumAsq * 1.0001, f.sqdiff(f11, ct, h) + f.sqdiff(f12, ct, h) + f.sqdiff(f13, ct, h) + f.sqdiff(f21, ct, h) + f.sqdiff(f22, ct, h));
        of << "# f reduced: " << f << endl << "# accumAsq " << accumAsq << " =? " << (f.sqdiff(f11, ct, h) + f.sqdiff(f12, ct, h) + f.sqdiff(f13, ct, h) + f.sqdiff(f21, ct, h) + f.sqdiff(f22, ct, h)) << endl << plot(f) << endl;
        accumAln.reduceMax(ct, h);
        of << "# accumAln reduced: " << accumAln << endl << plot(accumAln) << endl;
        of << endl;
    }
    of.close();
}
BOOST_AUTO_TEST_SUITE_END()

}

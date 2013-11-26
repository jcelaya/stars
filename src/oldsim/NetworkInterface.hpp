/*
 * NetworkInterface.hpp
 *
 *  Created on: 22/11/2013
 *      Author: javi
 */

#ifndef NETWORKINTERFACE_HPP_
#define NETWORKINTERFACE_HPP_

#include <ostream>
#include "Time.hpp"


namespace stars {

class NetworkInterface {
public:
    void setup(double in, double out) {
        inBW = in;
        outBW = out;
    }

    Duration getReceiveTime(unsigned int size) const {
        return Duration(size / inBW);
    }

    Duration getSendTime(unsigned int size) const {
        return Duration(size / outBW);
    }

    void updateInQueueEndTime(unsigned int size) {
        inQueueEndTime += Duration(size / inBW);
        if (inQueueEndTime < Time::getCurrentTime())
            inQueueEndTime = Time::getCurrentTime();
    }

    void updateOutQueueEndTime(unsigned int size) {
        if (outQueueEndTime < Time::getCurrentTime())
            outQueueEndTime = Time::getCurrentTime();
        outQueueEndTime += Duration(size / outBW);
    }

    Time getInQueueEndTime() const {
        return inQueueEndTime;
    }

    Time getOutQueueEndTime() const {
        return outQueueEndTime;
    }

    void accountRecvTraffic(unsigned int size) {
        received.addMessage(size, inBW, Time::getCurrentTime());
    }

    void accountSentTraffic(unsigned int size) {
        sent.addMessage(size, outBW, outQueueEndTime);
    }

    void output(std::ostream & os, double totalTime) const {
        sent.output(os, outBW, totalTime);
        os << ',';
        received.output(os, inBW, totalTime);
    }

private:
    struct LinkTrafficStats {
        unsigned long int bytes, dataBytes;
        unsigned long int maxBytes[2];
        unsigned long int lastBytes[2];
        std::list<std::pair<unsigned long int, Time> > lastSizes[2];
        LinkTrafficStats() : bytes(0), dataBytes(0), maxBytes{0, 0}, lastBytes{0, 0} {}
        void addMessage(unsigned int size, double bw, Time ref);
        void output(std::ostream & os, double bw, double totalTime) const;
    };

    static Duration samplingIntervals[2];

    LinkTrafficStats sent, received;
    Time inQueueEndTime, outQueueEndTime;
    double inBW, outBW;
};

} /* namespace stars */
#endif /* NETWORKINTERFACE_HPP_ */

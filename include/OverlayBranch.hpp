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

#ifndef OVERLAYBRANCH_HPP_
#define OVERLAYBRANCH_HPP_

#include "CommLayer.hpp"
#include "ZoneDescription.hpp"


class OverlayBranch;
class OverlayBranchObserver {
protected:
    friend class OverlayBranch;

    virtual ~OverlayBranchObserver() {}

    virtual void startChanges() = 0;

    virtual void commitChanges(bool fatherChanged, bool leftChanged, bool rightChanged) = 0;
};


class OverlayBranch : public Service {
public:
    virtual ~OverlayBranch() {}

    virtual bool inNetwork() const = 0;

    virtual const CommAddress & getFatherAddress() const = 0;

    virtual const CommAddress & getLeftAddress() const = 0;

    virtual double getLeftDistance(const CommAddress & src) const = 0;

    virtual bool isLeftLeaf() const = 0;

    virtual const CommAddress & getRightAddress() const = 0;

    virtual double getRightDistance(const CommAddress & src) const = 0;

    virtual bool isRightLeaf() const = 0;

    void registerObserver(OverlayBranchObserver * o) {
        observers.push_back(o);
    }

    void unregisterObserver(OverlayBranchObserver * o) {
        for (size_t i = 0; i < observers.size(); ++i)
            if (observers[i] == o) {
                observers[i] = observers.back();
                observers.pop_back();
                return;
            }
    }

protected:
    void fireStartChanges() {
        for (size_t i = 0; i < observers.size(); ++i)
            observers[i]->startChanges();
    }

    void fireCommitChanges(bool fatherChanged, bool leftChanged, bool rightChanged) {
        for (size_t i = 0; i < observers.size(); ++i)
            observers[i]->commitChanges(fatherChanged, leftChanged, rightChanged);
    }

private:
    std::vector<OverlayBranchObserver *> observers;   ///< Change observers.
};

#endif /* OVERLAYBRANCH_HPP_ */

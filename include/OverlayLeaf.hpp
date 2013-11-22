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

#ifndef OVERLAYLEAF_HPP_
#define OVERLAYLEAF_HPP_

#include "CommLayer.hpp"


class OverlayLeaf;
class OverlayLeafObserver {
public:
    virtual ~OverlayLeafObserver() {}

    virtual void fatherChanging() = 0;

    virtual void fatherChanged(bool changed) = 0;
};


class OverlayLeaf : public Service {
public:
    virtual ~OverlayLeaf() {}

    virtual const CommAddress & getFatherAddress() const = 0;

    void registerObserver(OverlayLeafObserver * o) {
        observers.push_back(o);
    }

    void unregisterObserver(OverlayLeafObserver * o) {
        for (size_t i = 0; i < observers.size(); ++i)
            if (observers[i] == o) {
                observers[i] = observers.back();
                observers.pop_back();
                return;
            }
    }

protected:
    void fireFatherChanging() {
        for (std::vector<OverlayLeafObserver *>::iterator it = observers.begin(); it != observers.end(); ++it)
            (*it)->fatherChanging();
    }

    void fireFatherChanged(bool changed) {
        for (std::vector<OverlayLeafObserver *>::iterator it = observers.begin(); it != observers.end(); ++it)
            (*it)->fatherChanged(changed);
    }

private:
    std::vector<OverlayLeafObserver *> observers;   ///< Change observers.
};


#endif /* OVERLAYLEAF_HPP_ */

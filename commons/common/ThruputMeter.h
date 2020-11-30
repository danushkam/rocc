//
// Copyright (C) 2005 Andras Varga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#ifndef __COMMONS_THRUPUTMETER_H
#define __COMMONS_THRUPUTMETER_H

#include "inet/common/INETDefs.h"

namespace commons {

/**
 * Measures and records network thruput
 */
// FIXME problem: if traffic suddenly stops, it'll show the last reading forever;
// (output vector will be correct though); would need a timer to handle this situation
class ThruputMeter
{
  public:
    // config
    simtime_t startTime;    // start time
    unsigned int batchSize;    // number of packets in a batch
    simtime_t maxInterval;    // max length of measurement interval (measurement ends
    // if either batchSize or maxInterval is reached, whichever
    // is reached first)

    // global statistics
    unsigned long numBits;

    // current measurement interval
    simtime_t intvlStartTime;
    unsigned long intvlNumPackets;
    unsigned long intvlNumBits;

    // statistics
    cOutVector bitpersecVector;

    void initialize(std::string chName);
    void handleMessage(cMessage *msg);

  protected:
    void updateStats(simtime_t now, unsigned long bits);
    void beginNewInterval(simtime_t now);
};

} // namespace commons

#endif // ifndef __COMMONS_THRUPUTMETER_H


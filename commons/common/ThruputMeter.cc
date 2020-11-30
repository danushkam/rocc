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

#include "ThruputMeter.h"

using namespace commons;
using namespace std;

void ThruputMeter::initialize(string chName)
{
    startTime = simTime();
    batchSize = 50;
    maxInterval = 1;

    numBits = 0;
    intvlStartTime = 0;
    intvlNumPackets = intvlNumBits = 0;

    bitpersecVector.setName((chName + "-thruput").c_str());
}

void ThruputMeter::handleMessage(cMessage *msg)
{
    updateStats(simTime(), PK(msg)->getBitLength());
}

void ThruputMeter::updateStats(simtime_t now, unsigned long bits)
{
    numBits += bits;

    // packet should be counted to new interval
    if (intvlNumPackets >= batchSize || now - intvlStartTime >= maxInterval)
        beginNewInterval(now);

    intvlNumPackets++;
    intvlNumBits += bits;
}

void ThruputMeter::beginNewInterval(simtime_t now)
{
    simtime_t duration = now - intvlStartTime;

    // record measurements
    double bitpersec = intvlNumBits / duration.dbl();

    bitpersecVector.recordWithTimestamp(intvlStartTime, bitpersec);

    // restart counters
    intvlStartTime = now;    // FIXME this should be *beginning* of tx of this packet, not end!
    intvlNumPackets = intvlNumBits = 0;
}

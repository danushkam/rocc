//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

namespace inet;

struct NodeINT
{
    int txBytes;
    int qLen;
    double ts;
    double B; // Bps
}

packet UDPReliableAppPacket
{
    // Common
    int sourceId; // App module
    uint32_t sequenceNumber; // Unique across flow(let)s
    
    // Flow
    uint32_t flowId;
    uint32_t flowSequenceNumber;
    uint32_t flowSize; // Set in the first pkt of a flow
    bool firstPkt;
    bool lastPkt;
    
    // HPCC specific
    int nHop;
    int pathId;
    NodeINT L[10];
}

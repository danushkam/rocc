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

packet UDPReliableAppPacketAck
{
    //uint32_t sequenceNumber;
    int appGateIndex; // Needed in transport layer to resolve app
    int protocolId; // Needed in IP layer to resolve transport module
    int sourceId; // App module
    int packetSize;
    bool firstPkt;
    bool lastPkt;
    bool ack;
    bool lastPktLeftQueue;
}

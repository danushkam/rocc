//
// Copyright (C) 2006 Alfonso Ariza
// Copyright (C) 2004 Andras Varga
// Copyright (C) 2000 Institut fuer Telematik, Universitaet Karlsruhe
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//

package common.applications;

import inet.applications.contract.IUDPApp;

//
// This traffic generator has evolved from UDPBasicBurst
//
simple UDPReliableApp like IUDPApp
{
    parameters:
        @class(commons::UDPReliableApp);
        volatile double sleepDuration @unit(ms); // ms
        int dataRate; // Mbps
        int startTime @unit(us) = default(0); // application start time (start of the first flow)
        int stopTime @unit(us) = default(0); // don't stop by default
        int localPort = default(-1); // local UDP port number (-1: use ephemeral port)
        int destPort; // remote UDP port number 
        int flowCount = default(-1); // -1: unlimited
        string destAddress;
        string flowSizeCDF;
        bool checkOrder = default(true);
        
        @signal[sentPk](type=cPacket);
        @signal[rcvdPk](type=cPacket);
        @signal[flowSize](type=long);
        @signal[sleepDuration](type=double);
        @statistic[sentPk](title="packets sent"; source=sentPk; record=count,"histogram(packetBytes)","sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[rcvdPk](title="packets received"; source=rcvdPk; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[flowSize](title="flow size"; record=vector,histogram; interpolationmode=none);
        @statistic[sleepDuration](title="sleep duration"; record=vector,histogram; interpolationmode=none);
        
        @display("i=block/app");
    gates:
        input udpIn @labels(UDPControlInfo/up);
        output udpOut @labels(UDPControlInfo/down);
}


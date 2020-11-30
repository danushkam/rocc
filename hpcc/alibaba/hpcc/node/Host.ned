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

package alibaba.hpcc.node;

import inet.applications.contract.ITCPApp;
import inet.applications.contract.IUDPApp;
import inet.transportlayer.contract.ITCP;
import inet.transportlayer.contract.IUDP;
import alibaba.hpcc.linklayer.ethernet.HostEthernetInterface;
import common.applications.UDPReliableApp;

//
// Definition of an IP node with a transport generator that connects
// to IP directly, without TCP or UDP.
//
module Host extends NodeBase
{
    parameters:
        @display("i=device/pc3");
        int numTcpApps = default(0);
        int numUdpApps = default(0);
        bool hasTcp = default(numTcpApps > 0);
        bool hasUdp = default(numUdpApps > 0);
        string tcpType = default("TCP");
        string udpType = default("UDP");
    submodules:
		tcpApp[numTcpApps]: <> like ITCPApp {
            parameters:
                @display("p=147,54,row,60");
        }
        tcp: <tcpType> like ITCP if hasTcp {
            parameters:
                @display("p=147,141");
        }
        udpApp[numUdpApps]: UDPReliableApp {
            parameters:
                @display("p=329,54,row,60");
        }
        udp: <udpType> like IUDP if hasUdp {
            parameters:
                @display("p=329,141");
        }
        eth[sizeof(ethg)]: HostEthernetInterface {
            parameters:
                @display("p=368,406,row,60;q=txQueue");
        }
    connections allowunconnected:
        // transport connections
		for i=0..numTcpApps-1 {
            tcpApp[i].tcpOut --> tcp.appIn++;
            tcpApp[i].tcpIn <-- tcp.appOut++;
        }
        tcp.ipOut --> networkLayer.transportIn++ if hasTcp;
        tcp.ipIn <-- networkLayer.transportOut++ if hasTcp;
        for i=0..numUdpApps-1 {
            udpApp[i].udpOut --> udp.appIn++;
            udpApp[i].udpIn <-- udp.appOut++;
        }
        udp.ipOut --> networkLayer.transportIn++ if hasUdp;
        udp.ipIn <-- networkLayer.transportOut++ if hasUdp;
        // connections to network outside
        for i=0..sizeof(ethg)-1 {
            ethg[i] <--> { @display("m=s"); } <--> eth[i].phys;
            eth[i].upperLayerOut --> networkLayer.ifIn++;
            eth[i].upperLayerIn <-- networkLayer.ifOut++;
            eth[i].oobL3Out --> networkLayer.ifOobIn++;
            eth[i].oobL3In <-- networkLayer.ifOobOut++;
        } 
}
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

import inet.common.lifecycle.NodeStatus;
import inet.common.packet.PcapRecorder;
import inet.linklayer.contract.IExternalNic;
import inet.linklayer.contract.ITunNic;
import inet.linklayer.contract.IWiredNic;
import inet.linklayer.contract.IWirelessNic;
import inet.linklayer.loopback.LoopbackInterface;
import inet.mobility.contract.IMobility;
import inet.networklayer.common.InterfaceTable;
import inet.networklayer.contract.INetworkLayer;
import inet.networklayer.contract.IRoutingTable;
import inet.power.contract.IEnergyGenerator;
import inet.power.contract.IEnergyManagement;
import inet.power.contract.IEnergyStorage;
import inet.node.inet.INetworkNode;
import alibaba.hpcc.networklayer.ipv4.IPv4NetworkLayerEx;

//
// Contains the common lower layers (linklayer and networklayer)
// of Router, StandardHost, WirelessHost etc.
//
// NOTE:
//    - forwarding is turned on by default.
//    - use this model only as a base type. In simulations use ~Router,
//      ~StandardHost, ~WirelessHost and ~AdhocHost
//
module NodeBase like INetworkNode
{
    parameters:
        @display("bgb=,448");
        @networkNode;
        @labels(node,ethernet-node,wireless-node);
        bool hasStatus = default(false);
        int numRadios = default(0);               // the number of radios in the router. by default no wireless
        int numPcapRecorders = default(0); // no of PcapRecorders.
        string osgModel = default(""); // 3D model for OSG visualization, no 3D model by default
        string osgModelColor = default(""); // tint color, no colorization by default
        string routingTableType = default("IPv4RoutingTable");
        bool forwarding = default(true);
        bool multicastForwarding = default(false);
        routingTable.forwarding = forwarding;
        routingTable.multicastForwarding = multicastForwarding;   // for IPv4, IPv6, Generic
        *.interfaceTableModule = default(absPath(".interfaceTable"));
        *.routingTableModule = default(routingTableType != "" ? absPath(".routingTable") : "");
        *.energySourceModule = default(energyStorageType != "" ? absPath(".energyStorage") : "");
    gates:
        input radioIn[numRadios] @directIn;
        inout pppg[] @labels(PPPFrame-conn);
        inout ethg[] @labels(EtherFrame-conn);
    submodules:
        status: NodeStatus if hasStatus {
            @display("p=50,50");
        }
        // network layer
        networkLayer: IPv4NetworkLayerEx {
            parameters:
                @display("p=329,287;q=queue");
        }
        // routing table
        routingTable: <routingTableType> like IRoutingTable if routingTableType != "" {
            parameters:
                @display("p=53,250;is=s");
        }
        // linklayer
        interfaceTable: InterfaceTable {
            parameters:
                @display("p=53,300;is=s");
        }
        pcapRecorder[numPcapRecorders]: PcapRecorder {
            @display("p=53,350,r,10");
        }
}
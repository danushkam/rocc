//
// Copyright (C) 2004 Andras Varga
//               2010 Zoltan Bojthe
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

// Note (Danushka): It was required to use an extended version of EtherQoSQueue,
// 		 		    hence this class, a clone + extension of EthernetInterface.		 

package alibaba.hpcc.linklayer.ethernet;

import inet.common.IHook;
import inet.linklayer.contract.IEtherMAC;
import inet.linklayer.contract.ITrafficConditioner;
import inet.linklayer.contract.IWiredNic;
import inet.linklayer.ethernet.EtherQoSQueue;
import inet.linklayer.ethernet.IEtherEncap;
import inet.common.misc.ThruputMeter;

//
// Ethernet network interface. Complements ~EtherMAC and ~EtherEncap
// with an output queue for QoS and RED support.
//
module HostEthernetInterface like IWiredNic
{
    parameters:
        string interfaceTableModule;
        bool csmacdSupport = default(false);  // by default CSMA/CD is turned off, so only point-to-point duplex links are supported.
        string macType = default(csmacdSupport ? "EtherMAC" : "EtherMACFullDuplex"); // ~EtherMAC or ~EtherMACFullDuplex
        string ingressTCType = default(""); // a module type implementing ~ITrafficConditioner for optional traffic conditioning of incoming traffic
        string egressTCType = default(""); // a module type implementing ~ITrafficConditioner for optional traffic conditioning of outgoing traffic
        string encapType = default("EtherEncap");   // module for encapsulation/decapsulation; use ~EtherEncapDummy for no encapsulation/decapsulation
        int numOutputHooks = default(0);
        int numInputHooks = default(0);
        @display("i=block/ifcard;bgl=2");
        *.interfaceTableModule = default(absPath(interfaceTableModule));
    gates:
        input upperLayerIn;
        output upperLayerOut;
        inout phys @labels(EtherFrame);
        input oobL3In;
        output oobL3Out;
    submodules:
        outputHook[numOutputHooks]: <default("Nop")> like IHook if numOutputHooks>0 {
            @display("p=80,98");
        }
        inputHook[numInputHooks]: <default("Nop")> like IHook if numInputHooks>0 {
            @display("p=265,83");
        }
        ingressTC: <ingressTCType> like ITrafficConditioner if ingressTCType != "" {
            @display("p=292,158");
        }
        egressTC: <egressTCType> like ITrafficConditioner if egressTCType != "" {
            @display("p=53,158");
        }
        queue: HostEtherQoSQueue {
            parameters:
                @display("p=107,263;q=l2queue");
        }
        mac: <macType> like IEtherMAC {
            parameters:
                queueModule = "^.queue"; // (queueType == "" ? "" : "^.queue");
                txQueueLimit = 1; // (queueType == "" ? 10000 : 1); // queue sends one packet at a time
                @display("p=182,321");
        }
        encap: <encapType> like IEtherEncap {
            parameters:
                @display("p=182,205");
        }
        offeredTP: ThruputMeter;
        deliveredTP: ThruputMeter;
    connections:
        mac.upperLayerOut --> encap.lowerLayerIn;
        mac.phys <--> { @display("m=s"); } <--> phys;

		oobL3In --> { @display("m=n"); } --> queue.oobL3In;
		oobL3Out <-- { @display("m=n"); } <-- queue.oobL3Out;

        //encap.lowerLayerOut --> queue.in;
        encap.lowerLayerOut --> offeredTP.in;
        offeredTP.out --> queue.in;
        //queue.out --> mac.upperLayerIn;
        queue.out --> deliveredTP.in;
        deliveredTP.out --> mac.upperLayerIn;

        // no input hooks, no ingressTC
        encap.upperLayerOut --> { @display("m=n"); } --> upperLayerOut if numInputHooks == 0 && ingressTCType == "";

        // no input hooks, there is ingressTC
        if numInputHooks == 0 && ingressTCType != "" {
            encap.upperLayerOut --> ingressTC.in;
            ingressTC.out --> { @display("m=n"); } --> upperLayerOut;
        }

        // there are input hooks
        if numInputHooks > 0 {
            inputHook[numInputHooks-1].out --> { @display("m=n"); } --> upperLayerOut;
        }
        for i=0..numInputHooks-2 {
            inputHook[i].out --> inputHook[i+1].in;
        }

        // there are input hooks, no ingressTC
        if numInputHooks > 0 && ingressTCType == "" {
            encap.upperLayerOut --> inputHook[0].in;
        }

        // there are input hooks, there is ingressTC
        if numInputHooks > 0 && ingressTCType != "" {
            encap.upperLayerOut --> ingressTC.in;
            ingressTC.out --> inputHook[0].in;
        }

        // chain output hooks
        for i=0..numOutputHooks-2 {
            outputHook[i].out --> outputHook[i+1].in;
        }

        // no output hooks, no egressTC
        upperLayerIn --> { @display("m=n"); } --> encap.upperLayerIn if numOutputHooks == 0 && egressTCType == "";

        // there are output hooks
        if numOutputHooks > 0 {
            upperLayerIn --> { @display("m=n"); } --> outputHook[0].in;
        }

        // no output hooks, there is egressTC
        if numOutputHooks == 0 && egressTCType != "" {
            upperLayerIn --> { @display("m=n"); } --> egressTC.in;
        }

        // there is egressTC
        if egressTCType != "" {
            egressTC.out --> encap.upperLayerIn;
        }

        // there are output hooks, no egressTC
        if numOutputHooks > 0 && egressTCType == "" {
            outputHook[numOutputHooks-1].out --> encap.upperLayerIn;
        }

        // there are output hooks, there is egressTC
        if numOutputHooks > 0 && egressTCType != "" {
            outputHook[numOutputHooks-1].out --> egressTC.in;
        }
}
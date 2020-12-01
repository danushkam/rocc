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

package cisco.rocc.node;

import cisco.rocc.linklayer.ethernet.SwitchEthernetInterface;

module Switch extends NodeBase
{
    submodules:
        eth[sizeof(ethg)]: SwitchEthernetInterface {
            parameters:
                @display("p=368,406,row,60;q=txQueue");
        }
    connections allowunconnected:
        // connections to network outside
        for i=0..sizeof(ethg)-1 {
            ethg[i] <--> { @display("m=s"); } <--> eth[i].phys;
            eth[i].upperLayerOut --> networkLayer.ifIn++;
            eth[i].upperLayerIn <-- networkLayer.ifOut++;
            eth[i].oobL3Out --> networkLayer.ifOobIn++;
            eth[i].oobL3In <-- networkLayer.ifOobOut++;
        }     
}
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

package inet.showcases.visualizer.styling;
import inet.node.ethernet.Eth100M;
import inet.node.inet.WirelessHost;
import inet.networklayer.configurator.ipv4.IPv4NetworkConfigurator;
import inet.node.ethernet.Eth10M;
import inet.node.inet.AdhocHost;
import inet.node.inet.StandardHost;
import inet.node.wireless.AccessPoint;
import inet.physicallayer.ieee80211.packetlevel.Ieee80211RadioMedium;
import inet.physicallayer.ieee80211.packetlevel.Ieee80211ScalarRadioMedium;
import inet.visualizer.integrated.IntegratedVisualizer;


//
// TODO documentation
//
network StylingShowcase
{
    @display("bgb=600,250");
    submodules:
        host1: AdhocHost {
            @display("p=205,150");
        }
        host2: AdhocHost {
            @display("p=465,150");
        }
        visualizer: IntegratedVisualizer {
            @display("p=50,50");
        }
        configurator: IPv4NetworkConfigurator {
            @display("p=50,100");
        }
        radioMedium: Ieee80211ScalarRadioMedium {
            @display("p=50,150");
        }
}

//
// TODO documentation
//
network AnnotationShowcase
{
    @display("bgb=600,250");
    submodules:
        host1: WirelessHost {
            @display("p=205,150");
        }
        host2: WirelessHost {
            @display("p=465,150");
        }
        visualizer: IntegratedVisualizer {
            @display("p=50,50");
        }
        configurator: IPv4NetworkConfigurator {
            @display("p=50,100");
        }
        radioMedium: Ieee80211ScalarRadioMedium {
            @display("p=50,150");
        }
        accessPoint: AccessPoint {
            @display("p=329,67");
        }
}

//
// TODO documentation
//
network TestShowcase
{
    @display("bgb=600,250");
    submodules:
        host1: WirelessHost {
            @display("p=200,143");
        }
        host2: WirelessHost {
            @display("p=325,143");
        }
        visualizer: IntegratedVisualizer {
            @display("p=50,50");
        }
        configurator: IPv4NetworkConfigurator {
            @display("p=50,100");
        }
        radioMedium: Ieee80211ScalarRadioMedium {
            @display("p=50,150");
        }
        accessPoint1: AccessPoint {
            @display("p=200,49");
        }
        accessPoint2: AccessPoint {
            @display("p=325,49");
        }
        accessPoint3: AccessPoint {
            @display("p=458,49");
        }
        host3: WirelessHost {
            @display("p=458,143");
        }
        standardHost: StandardHost {
            @display("p=243,208");
        }
        standardHost1: StandardHost {
            @display("p=530,208");
        }
    connections:
        standardHost.ethg++ <--> Eth100M <--> standardHost1.ethg++;
}


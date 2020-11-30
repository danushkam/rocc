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

package inet.showcases.wireless.coexistence;

import inet.networklayer.configurator.ipv4.IPv4NetworkConfigurator;
import inet.node.inet.AdhocHost;
import inet.physicallayer.common.packetlevel.RadioMedium;
import inet.physicallayer.ieee80211.packetlevel.Ieee80211DimensionalRadioMedium;
import inet.physicallayer.ieee80211.packetlevel.Ieee80211ScalarRadioMedium;
import inet.physicallayer.ieee802154.bitlevel.Ieee802154UWBIRRadioMedium;
import inet.visualizer.integrated.IntegratedCanvasVisualizer;


network CoexistenceShowcase
{
    submodules:
        visualizer: IntegratedCanvasVisualizer {
            @display("p=50,50");
        }
        configurator: IPv4NetworkConfigurator {
            @display("p=50,100");
        }
        radioMedium: RadioMedium {
            @display("p=50,150");
        }
        wifiHost1: AdhocHost {
            @display("p=407.91748,119.189995");
        }
        wifiHost2: AdhocHost {
            @display("p=407.91748,314.415");
        }
        wpanHost1: AdhocHost {
            @display("p=234.26999,215.775");
        }
        wpanHost2: AdhocHost {
            @display("p=511.695,215.775");
        }
}

network CoexistenceShowcase2
{
    submodules:
        visualizer: IntegratedCanvasVisualizer {
            @display("p=50,50");
        }
        configurator: IPv4NetworkConfigurator {
            @display("p=50,100");
        }
        radioMedium: Ieee80211ScalarRadioMedium {
            @display("p=50,150");
        }
        wifiHost1: AdhocHost {
            @display("p=407.91748,119.189995");
        }
        wifiHost2: AdhocHost {
            @display("p=407.91748,314.415");
        }
        wpanHost1: AdhocHost {
            @display("p=234.26999,215.775");
        }
        wpanHost2: AdhocHost {
            @display("p=511.695,215.775");
        }
        radioMedium2: Ieee80211ScalarRadioMedium {
            @display("p=50,202");
        }
        visualizer2: IntegratedCanvasVisualizer {
            @display("p=41,253");
        }
}

network CoexistenceShowcase3
{
    submodules:
        visualizer: IntegratedCanvasVisualizer {
            @display("p=50,50");
        }
        configurator: IPv4NetworkConfigurator {
            @display("p=50,100");
        }
        wifiHost1: AdhocHost {
            @display("p=407.91748,119.189995");
        }
        wifiHost2: AdhocHost {
            @display("p=407.91748,314.415");
        }
        wpanHost1: AdhocHost {
            @display("p=234.26999,215.775");
        }
        wpanHost2: AdhocHost {
            @display("p=511.695,215.775");
        }
        visualizer2: IntegratedCanvasVisualizer {
            @display("p=41,253");
        }
        radioMedium: Ieee80211DimensionalRadioMedium {
            @display("p=49,157");
        }
}

network CoexistenceShowcase4
{
    submodules:
        visualizer: IntegratedCanvasVisualizer {
            @display("p=50,50");
        }
        configurator: IPv4NetworkConfigurator {
            @display("p=50,100");
        }
        wifiHost1: AdhocHost {
            @display("p=407.91748,119.189995");
        }
        wifiHost2: AdhocHost {
            @display("p=407.91748,314.415");
        }
        wpanHost1: AdhocHost {
            @display("p=362,215");
        }
        wpanHost2: AdhocHost {
            @display("p=427,215");
        }
        visualizer2: IntegratedCanvasVisualizer {
            @display("p=41,253");
        }
        radioMedium: Ieee80211DimensionalRadioMedium {
            @display("p=49,157");
        }
}

network CoexistenceShowcase5
{
    submodules:
        visualizer: IntegratedCanvasVisualizer {
            @display("p=50,50");
        }
        configurator: IPv4NetworkConfigurator {
            @display("p=50,100");
        }
        radioMedium: Ieee802154UWBIRRadioMedium {
            @display("p=50,150");
        }
        wifiHost1: AdhocHost {
            @display("p=407.91748,119.189995");
        }
        wifiHost2: AdhocHost {
            @display("p=407.91748,314.415");
        }
        wpanHost1: AdhocHost {
            @display("p=234.26999,215.775");
        }
        wpanHost2: AdhocHost {
            @display("p=511.695,215.775");
        }
}

network CoexistenceShowcase6
{
    submodules:
        visualizer: IntegratedCanvasVisualizer {
            @display("p=50,50");
        }
        configurator: IPv4NetworkConfigurator {
            @display("p=50,100");
        }
        radioMedium: RadioMedium {
            @display("p=50,150");
        }
        wifiHost1: AdhocHost {
            @display("p=407.91748,119.189995");
        }
        wifiHost2: AdhocHost {
            @display("p=407.91748,314.415");
        }
        wpanHost1: AdhocHost {
            @display("p=234.26999,215.775");
        }
        wpanHost2: AdhocHost {
            @display("p=511.695,215.775");
        }
}

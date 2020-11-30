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

package inet.tutorials.configurator;

import inet.networklayer.configurator.ipv4.IPv4NetworkConfigurator;
import inet.node.aodv.AODVRouter;
import inet.physicallayer.ieee80211.packetlevel.Ieee80211ScalarRadioMedium;
import inet.visualizer.integrated.IntegratedCanvasVisualizer;


network ConfiguratorD
{
    @display("bgb=939.89996,581.1");
    submodules:
        host1: AODVRouter {
            @display("p=239.148,74.897995");
        }
        host2: AODVRouter {
            @display("p=483,221");
        }
        host3: AODVRouter {
            @display("p=513.774,345.582");
        }
        host4: AODVRouter {
            @display("p=304.848,182.646;b=7,5");
        }
        host5: AODVRouter {
            @display("p=650.43,436.248");
        }
        host6: AODVRouter {
            @display("p=361.35,294.336");
        }
        host7: AODVRouter {
            @display("p=778,462");
        }
        configurator: IPv4NetworkConfigurator {
            @display("p=50,50");
        }
        radioMedium: Ieee80211ScalarRadioMedium {
            @display("p=50,100");
        }
        visualizer: IntegratedCanvasVisualizer {
            @display("p=50,150");
        }
}

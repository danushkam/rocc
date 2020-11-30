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

package inet.showcases.visualizer.instrument;

import inet.networklayer.configurator.ipv4.IPv4NetworkConfigurator;
import inet.node.inet.AdhocHost;
import inet.node.inet.WirelessHost;
import inet.physicallayer.ieee80211.packetlevel.Ieee80211ScalarRadioMedium;
import inet.visualizer.integrated.IntegratedVisualizer;

network InstrumentShowcase
{
    @display("bgb=840,400");

    @figure[throughputGauge](type=gauge; pos=190,30; size=100,100; minValue=0; maxValue=25; tickSize=5; label="Application level throughput [Mbps]");
    @statistic[throughput](source=throughput(client.tcpApp[0].rcvdPk)/1000000; record=figure; targetFigure=throughputGauge);

    @figure[bitrateLinearGauge](type=linearGauge; pos=385,108; size=250,30; minValue=0; maxValue=54; tickSize=6; label="Wifi bit rate [Mbps]");
    @statistic[bitrate1](source=server.wlan[0].mac.dcf.rateControl.datarate/1000000; record=figure; targetFigure=bitrateLinearGauge);

    @figure[progressMeter](type=progressMeter; pos=65,330; size=300,20; text="%.4g%%"; label="Download progress");
    @statistic[progress](source=100 * sum(packetBytes(client.tcpApp[0].rcvdPk)) / 1048576; record=figure; targetFigure=progressMeter);

    @figure[numRcvdPkCounter](type=counter; pos=413,327; label="Packets received"; decimalPlaces=4);
    @statistic[numRcvdPk](source=count(client.tcpApp[0].rcvdPk); record=figure; targetFigure=numRcvdPkCounter);

    @figure[perThermometer](type=thermometer; pos=710,45; size=30,180; minValue=0; maxValue=1; tickSize=0.2; label="Packet error rate");
    @statistic[per1](source=client.wlan[0].radio.packetErrorRate; record=figure; targetFigure=perThermometer);

    @figure[ctnIndexedImage](type=indexedImage; pos=570,338; size=32,32; images=misc/idle,misc/listen,misc/clock; label="contention state"; labelOffset=0,25; interpolation=best);
    @statistic[ctn](source=server.wlan[0].mac.dcf.channelAccess.contention.stateChanged; record=figure; targetFigure=ctnIndexedImage); // note: indexedImage takes the value modulo the number of images
    
    @figure[perPlot](type=plot; pos=647,260; size=150,90; timeWindow=3; maxValue=1; valueTickSize=1; label="Packet Error Rate");
    @statistic[per2](source=client.wlan[0].radio.packetErrorRate; record=figure; targetFigure=perPlot);
    
    @figure[bitratePlot](type=plot; pos=385,25; size=250,40; timeWindow=3; maxValue=55; valueTickSize=54; label="Wifi bit rate [Mbps]"; numberSizeFactor=2.5);
    @statistic[bitrate2](source=server.wlan[0].mac.dcf.rateControl.datarate/1000000; record=figure; targetFigure=bitratePlot);
    
    submodules:
        configurator: IPv4NetworkConfigurator {
            @display("p=62,41");
        }
        radioMedium: Ieee80211ScalarRadioMedium {
            @display("p=61,110");
        }
        visualizer: IntegratedVisualizer {
            @display("p=62,170");
        }
        client: AdhocHost {
            @display("p=300,250");
        }
        server: WirelessHost {
            @display("p=250,250");
        }
}

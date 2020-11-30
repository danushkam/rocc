package alibaba.hpcc;

import inet.common.misc.ThruputMeteringChannel;
import inet.networklayer.configurator.ipv4.IPv4NetworkConfigurator;
import alibaba.hpcc.node.Host;
import alibaba.hpcc.node.Switch;

network AsymmetricFabric
{
    parameters:
        int numGens1 = default(3);
        int numGens2 = default(2);
        @display("bgb=800,475");
    types:
        channel ethChannel extends ThruputMeteringChannel
        {
            thruputDisplayFormat = "u";
        }
    submodules:
        configurator: IPv4NetworkConfigurator {
            @display("p=48,31");
        }
        sw1: Switch {
            @display("p=250,100");
        }
        sw2: Switch {
            @display("p=250,350");
        }
        sw3: Switch {
            @display("p=450,250");
        }
        gens1[numGens1]: Host {
            @display("p=100,100,m,2,100");
        }
        gens2[numGens2]: Host {
            @display("p=100,350,m,2,100");
        }
        sink: Host {
            @display("p=600,250,m,2,100");
        }
    connections:
        for i=0..numGens1-1 {
            gens1[i].ethg++ <--> ethChannel {  datarate = 40Gbps; delay = 1.5us; } <--> sw1.ethg++;
        }
        for i=0..numGens2-1 {
            gens2[i].ethg++ <--> ethChannel {  datarate = 100Gbps; delay = 1.5us; } <--> sw2.ethg++;
        }
        sw1.ethg++ <--> ethChannel {  datarate = 100Gbps; delay = 1.5us; } <--> sw3.ethg++;
        sw2.ethg++ <--> ethChannel {  datarate = 100Gbps; delay = 1.5us; } <--> sw3.ethg++;
        sw3.ethg++ <--> ethChannel {  datarate = 100Gbps; delay = 1.5us; } <--> sink.ethg++;
}

package alibaba.hpcc;

import inet.common.misc.ThruputMeteringChannel;
import inet.networklayer.configurator.ipv4.IPv4NetworkConfigurator;
import alibaba.hpcc.node.Host;
import alibaba.hpcc.node.Switch;

network DBFabric
{
    parameters:
        int numGenerators;
        int numSinks;
        @display("bgb=800,500");
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
            @display("p=350,250");
        }
        sw2: Switch {
            @display("p=500,250");
        }
        generators[numGenerators]: Host {
            @display("p=50,150,m,2,100");
        }
        sinks[numSinks]: Host {
            @display("p=650,150,m,2,100");
        }
    connections:
        sw1.ethg++ <--> ethChannel {  datarate = 40Gbps; delay = 1us; } <--> sw2.ethg++;
        for i=0..numGenerators-1 {
            generators[i].ethg++ <--> ethChannel {  datarate = 10Gbps; delay = 1us; } <--> sw1.ethg++;
        }
        for i=0..numSinks-1 {
            sinks[i].ethg++ <--> ethChannel {  datarate = 10Gbps; delay = 1us; } <--> sw2.ethg++;
        }
}

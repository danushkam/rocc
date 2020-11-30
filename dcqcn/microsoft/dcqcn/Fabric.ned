package microsoft.dcqcn;

import inet.common.misc.ThruputMeteringChannel;
import inet.networklayer.configurator.ipv4.IPv4NetworkConfigurator;
import microsoft.dcqcn.node.Host;
import microsoft.dcqcn.node.Switch;

network Fabric
{
    parameters:
        int numGenerators;
        @display("bgb=583,409");
    types:
        channel ethChannel extends ThruputMeteringChannel
        {
            thruputDisplayFormat = "u";
        }
    submodules:
        generators[numGenerators]: Host {
            @display("p=100,200,m,4,100");
        }
        sink: Host {
            @display("p=500,100");
        }
        switch: Switch {
            @display("p=350,100");
        }
        configurator: IPv4NetworkConfigurator {
            @display("p=48,31");
        }
    connections:
        for i=0..numGenerators-1 {
            generators[i].ethg++ <--> ethChannel {  datarate = 40Gbps; delay = 1.5us; } <--> switch.ethg++;
        }
        switch.ethg++ <--> ethChannel {  datarate = 40Gbps; delay = 1.5us; } <--> sink.ethg++;
}
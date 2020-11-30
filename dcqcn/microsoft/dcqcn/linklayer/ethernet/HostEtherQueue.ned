package microsoft.dcqcn.linklayer.ethernet;

import common.queue.BaseQueue;

simple HostEtherQueue extends BaseQueue
{
    parameters:
        @class(dcqcn::HostEtherQueue);
        bool enableLossRecovery = default(false);
        int channelCapacity = default(75); // KB
        int rtt @unit(us) = default(0); // us
        int linkBandwidth = default(40); // Gbps
        int networkDelay = default(0); // us; one-way network delay
    gates:
        input oobL3In;
        output oobL3Out;
}
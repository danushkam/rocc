package cisco.rocc.linklayer.ethernet;

import common.queue.BaseQueue;

simple HostEtherQueue extends BaseQueue
{
    parameters:
        @class(rocc::HostEtherQueue);
        bool enableLossRecovery = default(false);
        int channelCapacity = default(75); // KB
        int rtt @unit(us) = default(0); // us
        int maxRate = default(40000); // Mbps
    gates:
        input oobL3In;
        output oobL3Out;
}
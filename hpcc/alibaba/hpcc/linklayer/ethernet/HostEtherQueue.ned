package alibaba.hpcc.linklayer.ethernet;

import common.queue.BaseQueue;

simple HostEtherQueue extends BaseQueue
{
    parameters:
        @class(hpcc::HostEtherQueue);
        bool enableLossRecovery = default(false);
        int channelCapacity = default(75); // KB
        int rtt @unit(us) = default(0); // us
        int nicSpeed = default(40); // Gbps
        int maxStage = default(5);
        int N = default(2);
        double T @unit(s) = default(0); // per hop
        double n = default(0.95);
        
    gates:
        input oobL3In;
        output oobL3Out;
}
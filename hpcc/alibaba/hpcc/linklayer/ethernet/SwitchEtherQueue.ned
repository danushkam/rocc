package alibaba.hpcc.linklayer.ethernet;

import common.queue.BaseQueue;

simple SwitchEtherQueue extends BaseQueue
{
    parameters:
        @class(hpcc::SwitchEtherQueue);
        bool congCtrl = default(false); // Enable congestion control
        bool enablePFC = default(false);
        int pauseThreshold = default(500); // KB
        int unpauseThreshold = default(100); // KB
        int B = default(40); // Gbps
        
        @signal[queueSize](type=long);
        @statistic[queueSize](title="queue size"; record=max,timeavg,vector,histogram);
        
    gates:
        input oobL3In;
        output oobL3Out;
}
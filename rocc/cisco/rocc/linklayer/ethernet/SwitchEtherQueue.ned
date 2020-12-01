package cisco.rocc.linklayer.ethernet;

import common.queue.BaseQueue;

simple SwitchEtherQueue extends BaseQueue
{
    parameters:
        @class(rocc::SwitchEtherQueue);
        bool enablePFC = default(false);
        bool congCtrl = default(false); // Enable L3FCN flow control
        int linkBandwidth = default(0); // Gbps : deprecated
        int minRate = default(100); // Mbps
        int maxRate = default(40000); // Mbps; min(in bandwidth, out bandwidth)
        int refQueueSize = default(100); // KB
        int midQueueSize = default(200); // KB
        int maxQueueSize = default(300); // KB
        int pauseThreshold = default(500); // KB
        int unpauseThreshold = default(100); // KB
        double alphaBar = default(0.3);
        double betaBar = default(1.5);
        
        @signal[queueSize](type=long);
        @statistic[queueSize](title="queue size"; record=max,timeavg,vector,histogram);
        @signal[rate](type=long);
        @statistic[rate](title="rate"; record=max,timeavg,vector,histogram);
        @signal[rateMsgBw](type=long);
        @statistic[rateMsgBw](title="rate msg bandwidth"; record=max,timeavg,vector,histogram);
    gates:
        input oobL3In;
        output oobL3Out;
}
package microsoft.dcqcn.linklayer.ethernet;

import common.queue.BaseQueue;

simple SwitchEtherQueue extends BaseQueue
{
    parameters:
        @class(dcqcn::SwitchEtherQueue);
        bool congCtrl = default(false); // Enable flow control
        bool enablePFC = default(false);
        int pauseThreshold = default(500); // KB
        int unpauseThreshold = default(100); // KB
        
        @signal[queueSize](type=long);
        @statistic[queueSize](title="queue size"; record=max,timeavg,vector,histogram);
        @signal[markProbability](type=double);
        @statistic[markProbability](title="mark probability"; record=max,timeavg,vector,histogram);
        @signal[spotProbability](type=double);
        @statistic[spotProbability](title="spot probability"; record=max,timeavg,vector,histogram);
    gates:
        input oobL3In;
        output oobL3Out;
}
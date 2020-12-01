package cisco.rocc.networklayer.ipv4;

import common.networklayer.ipv4.L3RelayUnit;

simple L3RelayUnitEx extends L3RelayUnit
{
    parameters:
        @class(rocc::L3RelayUnitEx);
        bool enableLossRecovery = default(false);
        bool enablePFC = default(false);
        int pauseThreshold = default(500); // KB
        int unpauseThreshold = default(100); // KB
        int elephantThreshold = default(0); // KB
        int elephantInvldIntvl @unit(ms) = default(2ms);
        
        @signal[rateMsgFanout](type=long);
        @statistic[rateMsgFanout](title="rate msg fanout"; record=max,timeavg,vector,histogram);
}
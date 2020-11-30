package alibaba.hpcc.networklayer.ipv4;

import common.networklayer.ipv4.L3RelayUnit;

simple L3RelayUnitEx extends L3RelayUnit
{
    parameters:
        @class(hpcc::L3RelayUnitEx);
        bool enableLossRecovery = default(false);
        bool enablePFC = default(false);
        int pauseThreshold = default(500); // KB
        int unpauseThreshold = default(100); // KB
        
        @signal[rateMsgFanout](type=long);
        @statistic[rateMsgFanout](title="rate msg fanout"; record=max,timeavg,vector,histogram);
}
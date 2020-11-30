package microsoft.dcqcn.networklayer.ipv4;

import common.networklayer.ipv4.L3RelayUnit;

simple L3RelayUnitEx extends L3RelayUnit
{
    parameters:
        @class(dcqcn::L3RelayUnitEx);
        bool enableLossRecovery = default(false);
        bool enablePFC = default(false);
        bool destination = default(false);
        int pauseThreshold = default(500); // KB
        int unpauseThreshold = default(100); // KB
}
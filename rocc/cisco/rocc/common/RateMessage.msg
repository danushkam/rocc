cplusplus {{
#include "inet/common/INETDefs.h"
}}

namespace rocc;

packet RateMessage
{
    uint32_t srcAddress;
    uint32_t destAddress;
    uint32_t flowId;
    int rate;
}
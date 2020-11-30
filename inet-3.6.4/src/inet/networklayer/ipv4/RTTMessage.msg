//
// Copyright (C) 2018 Cisco Systems, Inc. All rights reserved.
//

cplusplus {{
#include "inet/common/INETDefs.h"
}}

namespace inet;

packet RTTMessage
{
    uint32_t flowId;
    uint32_t msgId;
}

packet RTTRequest extends RTTMessage
{
    uint32_t destAddress;
}

packet RTTResponse extends RTTMessage
{}
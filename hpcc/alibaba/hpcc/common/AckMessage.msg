namespace hpcc;

struct AckNodeINT
{
    int txBytes;
    int qLen;
    double ts;
    double B; // Bps
}

packet AckMessage
{
    uint32_t flowId;
    uint32_t destAddress;
    uint32_t expectedSeq;
    uint32_t sackNo;
    bool ack;
    bool lastPkt;
    bool dupAck; // local use only
    
    int nHop;
    AckNodeINT L[5];
}

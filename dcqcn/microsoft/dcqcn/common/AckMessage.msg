namespace dcqcn;

packet AckMessage
{
    uint32_t flowId;
    uint32_t destAddress;
    uint32_t expectedSeq;
    uint32_t sackNo;
    bool ack;
    bool lastPkt;
    bool dupAck; // local use only
}
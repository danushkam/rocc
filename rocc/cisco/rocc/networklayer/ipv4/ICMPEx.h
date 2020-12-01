#ifndef __ICMP_EX_H
#define __ICMP_EX_H

#include "../../../rocc/common/AckMessage_m.h"
#include "../../../rocc/common/RateMessage_m.h"
#include "inet/networklayer/ipv4/ICMP.h"

namespace rocc {

enum L3FCNICMPType {
    ICMP_L3FCN_RATE = 30,
    ICMP_L3FCN_ACK = 31
};

class INET_API ICMPEx : public inet::ICMP
{
  public:
    ICMPEx();

  protected:
    virtual void handleMessage(cMessage* msg) override;
    virtual void processICMPMessage(inet::ICMPMessage* icmpmsg) override;

    void sendRateMessage(RateMessage* rateMsg);
    void sendAck(AckMessage* ackMsg);

  private:
    int numRateMsgsSent;
    int numRateMsgsReceived;
    int numAcksSent;
    int numAcksReceived;
};

} // namespace rocc

#endif // #ifndef __ICMP_EX_H

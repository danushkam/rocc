#ifndef __ICMP_EX_H
#define __ICMP_EX_H

#include "inet/networklayer/ipv4/ICMP.h"
#include "microsoft/dcqcn/common/CNPMessage_m.h"
#include "microsoft/dcqcn/common/AckMessage_m.h"

namespace dcqcn {

enum DCQCNICMPType {
    ICMP_DCQCN_CNP = 30,
    ICMP_DCQCN_ACK = 31
};

class INET_API ICMPEx : public inet::ICMP
{
  public:
    ICMPEx();

  protected:
    virtual void handleMessage(cMessage* msg) override;
    virtual void processICMPMessage(inet::ICMPMessage* icmpmsg) override;

    void sendCNPMessage(CNPMessage* rateMsg);
    void sendAck(AckMessage* ackMsg);

  private:
    int numAcksSent;
    int numAcksReceived;
};

} // namespace dcqcn

#endif // #ifndef __ICMP_EX_H

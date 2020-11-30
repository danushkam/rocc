#ifndef __SWITCH_ETHER_QUEUE_H
#define __SWITCH_ETHER_QUEUE_H

#include "inet/linklayer/ethernet/EtherFrame.h"
#include "inet/networklayer/common/L3Address.h"
#include "common/queue/BaseQueue.h"
#include <set>
#include <list>

namespace hpcc {

class INET_API SwitchEtherQueue : public commons::BaseQueue
{
  public:
    SwitchEtherQueue();
    virtual ~SwitchEtherQueue();

  protected:
    // Statistics
    static simsignal_t queueSizeSignal;

    virtual void initialize() override;

  private:
    bool enablePFC;
    bool congCtrl;
    bool paused;

    int pauseThreshold; // Bytes
    int unpauseThreshold; // Bytes
    double B; // Bps

    int txBytes;
    int pauseCount;

    cGate* oobL3InGate;
    cGate* oobL3OutGate;

    void emitQueueSize();

    virtual bool isFull() override;

    virtual void processIngressMessage(const cMessage* msg) override;
    virtual void processEgressMessage(const cMessage* msg) override;

    void injectINT(cPacket* packet);

    void sendPause();
    void sendUnpause();

    void UpdateBufferUsage(const cMessage* msg, bool release = false);

    virtual void finish() override;
};

} // namespace hpcc

#endif // #ifndef __SWITCH_ETHER_QUEUE_H

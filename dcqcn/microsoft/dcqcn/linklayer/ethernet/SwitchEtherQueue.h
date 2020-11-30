#ifndef __SWITCH_ETHER_QUEUE_H
#define __SWITCH_ETHER_QUEUE_H

#include "inet/linklayer/ethernet/EtherFrame.h"
#include "inet/networklayer/common/L3Address.h"
#include "common/queue/BaseQueue.h"
#include <set>
#include <list>

namespace dcqcn {

class INET_API SwitchEtherQueue : public commons::BaseQueue
{
  public:
    SwitchEtherQueue();
    virtual ~SwitchEtherQueue();

  protected:
    // Statistics
    static simsignal_t queueSizeSignal;
    static simsignal_t markProbabilitySignal;
    static simsignal_t spotProbabilitySignal;

    virtual void initialize() override;
    virtual void handleMessage(cMessage* msg) override;

  private:
    bool enablePFC;
    bool congCtrl;
    bool paused;

    double wq;
    double maxp;
    int minth;
    int maxth;
    int pauseThreshold; // Bytes
    int unpauseThreshold; // Bytes

    double avg;
    int count;
    omnetpp::simtime_t qTime;

    int oldQueueSize;
    double currDropProb;

    int pauseCount;

    int probUpdateTickCounter;

    cGate* oobL3InGate;
    cGate* oobL3OutGate;

    cMessage* clockEvent;

    void handleClockEvent();

    void setClock();
    void onProbUpdateTimer();

    void emitQueueSize();

    virtual bool isFull() override;

    virtual void processIngressMessage(const cMessage* msg) override;
    virtual void processEgressMessage(const cMessage* msg) override;

    void sendPause();
    void sendUnpause();

    void UpdateBufferUsage(const cMessage* msg, bool release = false);

    bool shouldMarkPkt(cPacket* pkt);
    bool markRED();
    bool markPIE();

    std::stringstream ss;

    virtual void finish() override;
};

} // namespace dcqcn

#endif // #ifndef __SWITCH_ETHER_QUEUE_H

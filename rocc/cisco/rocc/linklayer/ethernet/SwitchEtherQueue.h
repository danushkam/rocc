#ifndef __SWITCH_ETHER_QUEUE_H
#define __SWITCH_ETHER_QUEUE_H

#include "inet/linklayer/ethernet/EtherFrame.h"
#include "inet/networklayer/common/L3Address.h"
#include "common/queue/BaseQueue.h"
#include <set>
#include <list>
#include <map>
#include "../../../rocc/common/RateMessage_m.h"

namespace rocc {

class INET_API SwitchEtherQueue : public commons::BaseQueue
{
  public:
    SwitchEtherQueue();
    virtual ~SwitchEtherQueue();

  protected:
    // Statistics
    static simsignal_t queueSizeSignal;
    static simsignal_t rateSignal; // Both switch and source
    static simsignal_t rateMsgBwSignal;

    virtual void initialize() override;
    virtual void handleMessage(cMessage* msg) override;

  private:
    bool enablePFC;
    bool congCtrl;
    bool paused;
    bool congested;

    int linkBandwidth; // Mbps
    int minRate; // Mbps
    int maxRate; // Mbps
    int refQueueSize; // Bytes
    int midQueueSize; // Bytes
    int maxQueueSize; // Bytes
    int pauseThreshold; // Bytes
    int unpauseThreshold; // Bytes
    double alphaBar;
    double betaBar;

    int currentRate;
    int oldQueueSize;

    int numSent;
    int rateMessageCount;
    int pauseCount;

    int congCtrlTickCounter;

    cGate* oobL3InGate;
    cGate* oobL3OutGate;

    cMessage* clockEvent;

    std::map<inet::uint32, inet::uint16> newFlowTable;

    void handleClockEvent();

    void setClock();
    void emitQueueSize();

    void onCongCtrlTimer();

    int recalculateFairRate();
    int getFairRateForAlgo1();
    void autoTune(double& a, double& b);

    virtual bool isFull() override;

    virtual void processIngressMessage(const cMessage* msg) override;
    virtual void processEgressMessage(const cMessage* msg) override;

    void sendPause();
    void sendUnpause();

    void UpdateBufferUsage(const cMessage* msg, bool release = false);

    virtual void finish() override;
};

} // namespace rocc

#endif // #ifndef __SWITCH_ETHER_QUEUE_H

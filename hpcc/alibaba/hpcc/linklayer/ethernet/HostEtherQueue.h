#ifndef __HOST_ETHER_QUEUE_H
#define __HOST_ETHER_QUEUE_H

#include "inet/linklayer/ethernet/EtherFrame.h"
#include "inet/networklayer/common/L3Address.h"
#include "inet/applications/udpapp/UDPReliableAppPacket_m.h"
#include "inet/applications/udpapp/UDPReliableAppPacketAck_m.h"
#include "common/queue/BaseQueue.h"
#include "common/ThruputMeter.h"
#include "alibaba/hpcc/common/AckMessage_m.h"
#include <map>

namespace hpcc {

/**
 * Flow channel
 */
class Channel
{
    struct AckPendingMsg
    {
        enum Status { ACK_PENDING = 0, RESEND };
        inet::uint32 seqNumber;
        cMessage* msg;
        Status status;
    };
    typedef std::map<inet::uint32, AckPendingMsg*> AckPendingMsgs;

    bool systemChannel;
    inet::uint32 flowId;
    bool enableLossRecovery;
    int capacity;
    int rtt; // us (end-to-end)
    int maxStage;
    int Winit;
    int WAI;
    double T; // sec
    double n;

    inet::uint32 nextSeq;
    inet::uint32 lastUpdateSeq;
    inet::NodeINT L[5];
    int incStage;
    int Wc;
    double U;
    double R;
    double bitsPerInterval;
    double bitsToSend;

    int depth;
    int numReceived;
    int numDropped;
    int numSent;
    int numResent;
    int numAckReceived;
    int numNackReceived;

    inet::uint32 lastAckSeq;
    inet::uint32 prevLastAckSeq; // lastAckSeq at previous timeout
    int timeoutCount;
    int ackTimeoutTickCounter;

    cQueue queue;
    cOutVector rateVector;
    cOutVector UVector;
    commons::ThruputMeter thruputMeter;

    AckPendingMsgs ackPendingMsgs;
    inet::UDPReliableAppPacketAck* lastPktAck;

    std::stringstream ss;

    Channel(inet::uint32 flwId, bool enableLossRec, int channelCapacity, int rtt_,
            int maxStage_, double T_, double n_, int Winit_, int WAI_);
    ~Channel();

    void initialize();
    void handleAckMessage(AckMessage* ackMsg); // Received from destination
    void onAckReceived(AckMessage* ackMsg);

    cMessage* addMessage(cMessage* msg);
    cMessage* getMessage();

    void onRateLimitTimer();
    void onAckClockTimer();

    double measureInflight(AckMessage* ack);
    int computeWind(double U, bool updateWc);
    void newAck(AckMessage* ack);

    void adjustRate(double rate);

    static inet::uint32 getSequence(cMessage* msg);
    static inet::UDPReliableAppPacket* getAppPkt(omnetpp::cMessage* msg);

    int getPendingResendCount();

    friend class HostEtherQueue;
};

/**
 * This is a multi-channel drop-tail queue that maintains a channel per flow.
 */
class INET_API HostEtherQueue : public commons::BaseQueue
{
  public:
    HostEtherQueue();
    virtual ~HostEtherQueue();

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage* msg) override;
    virtual void requestPacket() override;

  private:
    typedef std::map<inet::uint32, Channel*> Channels; // By flow

    bool enableLossRecovery;
    int channelCapacity;
    int rtt; // us (end-to-end)
    int maxStage;
    int Winit;
    int WAI;
    double T; // sec
    double n;

    int nextChannel;

    int rateLimitTickCounter;
    int ackClockTickCounter;

    Channels channels;

    cGate* oobL3InGate;
    cGate* oobL3OutGate;

    cMessage* clockEvent;

    void handleClockEvent();
    void handleMessageEx(cMessage* msg);
    void sendApplicationPacketAck(cMessage* msg, bool ack);
    inet::UDPReliableAppPacketAck* getLastPktAck(cMessage* msg);

    virtual cMessage* enqueue(cMessage* msg) override;
    virtual cMessage* dequeue() override;

    cMessage* addToChannel(cMessage* msg);

    void setClock();
    void onRateLimitTimer();
    void onAckClockTimer();

    void sendPackets(bool stale = false);

    int getQueueSize();
    bool hasNewMsgs();

    virtual bool isEmpty() override;
    virtual bool isFull() override;
    virtual void finish() override;

    std::stringstream ss;
};

} // namespace hpcc

#endif // #ifndef __HOST_ETHER_QUEUE_H

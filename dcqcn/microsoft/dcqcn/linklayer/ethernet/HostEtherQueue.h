#ifndef __HOST_ETHER_QUEUE_H
#define __HOST_ETHER_QUEUE_H

#include "inet/linklayer/ethernet/EtherFrame.h"
#include "inet/networklayer/common/L3Address.h"
#include "inet/applications/udpapp/UDPReliableAppPacket_m.h"
#include "inet/applications/udpapp/UDPReliableAppPacketAck_m.h"
#include "common/queue/BaseQueue.h"
#include "common/ThruputMeter.h"
#include "microsoft/dcqcn/common/CNPMessage_m.h"
#include "microsoft/dcqcn/common/AckMessage_m.h"
#include <set>
#include <list>
#include <map>

namespace dcqcn {

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
    int rtt;

    bool rateLimit;

    inet::uint32 nextSequence;
    int linkBandwidth;
    int alphaTickCount;

    double currentRate;
    double targetRate;
    double a;
    double bitsPerInterval;
    double bitsToSend;
    long byteCounter;
    int t;
    int bc;

    int depth;
    int numReceived;
    int numDropped;
    int numSent;
    int numResent;
    int numAckReceived;
    int numNackReceived;
    int cnpMsgCount;

    int alphaTickCounter;
    int tTickCounter;

    inet::uint32 lastAckSeq;
    inet::uint32 prevLastAckSeq; // lastAckSeq at previous timeout
    int timeoutCount;
    int ackTimeoutTickCounter;

    cQueue queue;
    cOutVector rateVector;
    commons::ThruputMeter thruputMeter;

    AckPendingMsgs ackPendingMsgs;
    inet::UDPReliableAppPacketAck* lastPktAck;

    std::stringstream ss;

    Channel(inet::uint32 flwId, bool enableLossRec, int channelCapacity, int linkBw, int alphaTC, int rtt);
    ~Channel();

    void initialize();
    void handleCnpMessage(CNPMessage* cnpMsg); // Received from the destination
    void handleAckMessage(AckMessage* ackMsg); // Received from destination
    void onAckReceived(AckMessage* ackMsg);

    cMessage* addMessage(cMessage* msg);
    cMessage* getMessage();

    void handleClockEvent();

    void onRateLimitTimer();
    void onRateIncreaseTimer();
    void onAlphaUpdateTimer();
    void onAckClockTimer();

    void increaseRate();
    void adjustRate(double rate);

    static inet::uint32 getSequence(cMessage* msg);
    static inet::UDPReliableAppPacket* getAppPkt(omnetpp::cMessage* msg);

    int getPendingResendCount();

    friend class HostEtherQueue;
};

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
    int linkBandwidth; // Mbps
    int alphaTickCount;
    int rtt;

    int cnpMsgCount;
    int nextChannel;

    int rateLimitTickCounter; // Source
    int ackClockTickCounter;

    Channels channels;

    cGate* oobL3InGate;
    cGate* oobL3OutGate;

    cMessage* clockEvent;

    void handleClockEvent();
    void handleCnpMessage(CNPMessage* cnpMsg); // Received from the destination
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
};

} // namespace dcqcn

#endif // #ifndef __HOST_ETHER_QUEUE_H

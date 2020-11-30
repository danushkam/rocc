#include "HostEtherQueue.h"
#include "inet/linklayer/ethernet/EtherFrame.h"
#include "inet/networklayer/ipv4/IPv4Datagram.h"
#include "inet/networklayer/common/IPProtocolId_m.h"
#include "common/messages/PauseMessage_m.h"
#include "common/FlowSensor.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <fstream>

using namespace std;
using namespace inet;
using namespace commons;
using namespace hpcc;

Define_Module(HostEtherQueue);

const static int CLOCK_TICK_LENGTH = 1; // ns

const static int RATE_LIMIT_INTERVAL = 10; // ns
const static int ACK_CLOCK_INTERVAL = 1000; // ns

const static int RATE_LIMIT_TICK_COUNT = RATE_LIMIT_INTERVAL / CLOCK_TICK_LENGTH;
const static int ACK_CLOCK_TICK_COUNT = ACK_CLOCK_INTERVAL / CLOCK_TICK_LENGTH;

Channel::Channel(uint32 flwId, bool enableLossRec, int channelCapacity, int rtt_,
        int maxStage_, double T_, double n_, int Winit_, int WAI_):
        systemChannel(flwId == SYSTEM_FLOW_ID),
        flowId(flwId),
        enableLossRecovery(enableLossRec),
        capacity(channelCapacity),
        rtt(rtt_),
        maxStage(maxStage_),
        T(T_),
        n(n_),
        Winit(Winit_),
        WAI(WAI_),
        nextSeq(0),
        lastUpdateSeq(0),
        incStage(0),
        U(0),
        R(0),
        Wc(0),
        bitsPerInterval(0),
        bitsToSend(0),
        depth(0),
        numReceived(0),
        numDropped(0),
        numSent(0),
        numResent(0),
        numAckReceived(0),
        numNackReceived(0),
        lastAckSeq(0),
        prevLastAckSeq(0),
        timeoutCount(0),
        ackTimeoutTickCounter(0),
        lastPktAck(nullptr)
{}

Channel::~Channel()
{}

void Channel::initialize()
{
    std::string chName = "ch-";
    chName += to_string(flowId);

    queue.setName((chName + "-queue").c_str());
    rateVector.setName((chName + "-rate").c_str());
    UVector.setName((chName + "-U").c_str());

    createWatch((chName + "-depth").c_str(), depth);
    createWatch((chName + "-numReceived").c_str(), numReceived);
    createWatch((chName + "-numDropped").c_str(), numDropped);
    createWatch((chName + "-numSent").c_str(), numSent);
    createWatch((chName + "-numResent").c_str(), numResent);
    createWatch((chName + "-numAckReceived").c_str(), numAckReceived);
    createWatch((chName + "-numNackReceived").c_str(), numNackReceived);

    thruputMeter.initialize(chName);

    memset(&L, 0, sizeof(L));

    adjustRate(Winit / T);
}

void Channel::handleAckMessage(AckMessage* ackMsg)
{
    EV_INFO << "Ack message received!" << endl;

//    ss << "ACK " << ackMsg->getAck() << " " << ackMsg->getSackNo() << " " << ackMsg->getExpectedSeq() << endl;

    auto ite = ackPendingMsgs.find(ackMsg->getExpectedSeq());
    if (ite != ackPendingMsgs.end()) {
        onAckReceived(ackMsg);

        lastAckSeq = ackMsg->getExpectedSeq();

        ackTimeoutTickCounter = 0;
        timeoutCount = 0;

        newAck(ackMsg);
    }

    delete ackMsg;
}

void Channel::onAckReceived(AckMessage* ackMsg)
{
    uint32 stopSeq = ackMsg->getAck() ? ackMsg->getExpectedSeq() + 1 : ackMsg->getExpectedSeq();

    while (!ackPendingMsgs.empty()) {
        AckPendingMsg* pendingMsg = ackPendingMsgs.begin()->second;
        if (stopSeq == pendingMsg->seqNumber) {
            break;
        }

        depth -= PK(pendingMsg->msg)->getByteLength();

        delete pendingMsg->msg;
        delete pendingMsg;
        ackPendingMsgs.erase(ackPendingMsgs.begin());
    }

    if (nextSeq < stopSeq) {
        nextSeq = stopSeq;
    }

    if (ackMsg->getAck()) { // Ack
        numAckReceived++;
    } else {
        numNackReceived++;
        nextSeq = stopSeq;

        for (auto ite = ackPendingMsgs.begin(); ite != ackPendingMsgs.end(); ite++) {
            ite->second->status = AckPendingMsg::RESEND;
        }
    }
}

cMessage* Channel::addMessage(cMessage* msg)
{
    numReceived++;

    if (capacity <= depth) { // Channel full
        numDropped++;
        return msg;
    }

    queue.insert(msg);
    depth += PK(msg)->getByteLength();

    return nullptr;
}

cMessage* Channel::getMessage()
{
    if (systemChannel && !queue.isEmpty()) {
        cMessage* msg = (cMessage*)queue.pop();
        depth -= PK(msg)->getByteLength();
        numSent++;
        return msg;
    }

    cMessage* msg = nullptr;

    if (enableLossRecovery) {
        if (!ackPendingMsgs.empty() && (nextSeq <= ackPendingMsgs.rbegin()->first)) { // Retransmit
            AckPendingMsg* pendingMsg = ackPendingMsgs.find(nextSeq)->second;
            bool go = PK(pendingMsg->msg)->getBitLength() <= bitsToSend;
            if (go) {
               msg = pendingMsg->msg->dup();
               pendingMsg->status = AckPendingMsg::ACK_PENDING;
               numResent++;
            }
        } else { // Transmit
            bool go = !queue.isEmpty() && (PK(queue.front())->getBitLength() <= bitsToSend);
            if (go) {
                msg = (cMessage*)queue.pop();
                if (getSequence(msg) != nextSeq) {
                    throw cRuntimeError("Next frame doesn't have the expected sequence number!");
                }
                AckPendingMsg* pendingMsg = new AckPendingMsg;
                pendingMsg->seqNumber = nextSeq;
                pendingMsg->msg = msg->dup();
                pendingMsg->status = AckPendingMsg::ACK_PENDING;
                ackPendingMsgs[nextSeq] = pendingMsg;
                numSent++;
            }
        }
    } else {
        bool go = !queue.isEmpty();// && (PK(queue.front())->getBitLength() <= bitsToSend);
        if (go) {
            msg = (cMessage*)queue.pop();
            numSent++;
        }
    }

    if (msg) {
        if (enableLossRecovery) {
            nextSeq++;
        } else {
            depth -= PK(msg)->getByteLength();
        }

        bitsToSend -= PK(msg)->getBitLength();

        thruputMeter.handleMessage(msg);
    }

    return msg;
}

double Channel::measureInflight(AckMessage* ack)
{
    double t = T;
    double u = 0;

    for (int i = 0; i < ack->getNHop(); i++) {
        double txRate = (ack->getL(i).txBytes - L[i].txBytes) / (ack->getL(i).ts - L[i].ts);
        double v1 = min(ack->getL(i).qLen, L[i].qLen) / (ack->getL(i).B * T);
        double v2 = txRate / ack->getL(i).B;
        double u1 = v1 + v2;
        if (u1 > u) {
            u = u1;
            t = ack->getL(i).ts - L[i].ts;
        }
    }

    t = min(t, T);
    U = (1 - (t / T)) * U + (t / T) * u;

    UVector.recordWithTimestamp(simTime(), U);

    return U;
}

int Channel::computeWind(double U, bool updateWc)
{
    int W = 0;

    if (U >= n || incStage >= maxStage) {
        W = Wc / (U / n) + WAI;
        if (updateWc) {
            incStage = 0;
            Wc = W;
        }
    } else {
        W = Wc + WAI;
        if (updateWc) {
            incStage++;
            Wc = W;
        }
    }

    return W;
}

void Channel::newAck(AckMessage* ack)
{
    int W = 0;

    if (ack->getSackNo() > lastUpdateSeq) {
        W = computeWind(measureInflight(ack), true);
        lastUpdateSeq = nextSeq;
    } else {
        W = computeWind(measureInflight(ack), false);
    }

    for (int i = 0; i < ack->getNHop(); i++) {
        NodeINT& nodeInt = L[i];
        AckNodeINT& ackNodeInt = ack->getL(i);
        nodeInt.B = ackNodeInt.B;
        nodeInt.qLen = ackNodeInt.qLen;
        nodeInt.ts = ackNodeInt.ts;
        nodeInt.txBytes = ackNodeInt.txBytes;
    }

    if (Winit < W)
        W = Winit;
        //throw cRuntimeError("-ve window size!");

    R = W / T;
    adjustRate(R);
}

void Channel::adjustRate(double rate) // Bps
{
    EV_INFO << "Rate adjusting!" << endl;
    rateVector.recordWithTimestamp(simTime(), (rate * 8) / ((double)(1000 * 1000 * 1000)));

    double bitsPerSec = rate * 8;//rate * 1000 * 1000;
    bitsPerInterval = (bitsPerSec * RATE_LIMIT_INTERVAL) / ((double)(1000 * 1000 * 1000));

    onRateLimitTimer(); // Rectify asap
}

void Channel::onRateLimitTimer()
{
    // Include carry over from previous interval
    bitsToSend += bitsPerInterval;
}

void Channel::onAckClockTimer()
{
    if (!ackPendingMsgs.empty()) {
        if (ackTimeoutTickCounter++ == rtt) {
            bool timeout = nextSeq - lastAckSeq < 3 ? true : false;
            if (!timeout && (prevLastAckSeq == lastAckSeq)) {
                timeoutCount++;
                if (timeoutCount == 5) {
                    timeout = true;
                    timeoutCount = 0;
                }
            } else {
                timeoutCount = 0;
            }

            if (timeout) {
                nextSeq = ackPendingMsgs.begin()->second->seqNumber;
                for (auto ite = ackPendingMsgs.begin(); ite != ackPendingMsgs.end(); ite++) {
                    ite->second->status = AckPendingMsg::RESEND;
                }
            }

            prevLastAckSeq = lastAckSeq;
            ackTimeoutTickCounter = 0;
        }
    } else {
        ackTimeoutTickCounter = 0;
    }
}

uint32 Channel::getSequence(cMessage* msg)
{
    UDPReliableAppPacket* appPacket = getAppPkt(msg);
    if (!appPacket) {
        return 0;
    }
    return appPacket->getSequenceNumber();
}

UDPReliableAppPacket* Channel::getAppPkt(cMessage* msg)
{
    EtherFrame* frame = dynamic_cast<EtherFrame*>(msg);
    if (!frame) {
        return nullptr;
    }
    cPacket* udpPacket = frame->getEncapsulatedPacket()->getEncapsulatedPacket();
    if (!udpPacket) {
        return nullptr;
    }
    return dynamic_cast<UDPReliableAppPacket*>(udpPacket->getEncapsulatedPacket());
}

int Channel::getPendingResendCount()
{
    int count = 0;
    for (auto ite = ackPendingMsgs.begin(); ite != ackPendingMsgs.end(); ite++) {
        AckPendingMsg* pendingMsg = ite->second;
        count += pendingMsg->status == AckPendingMsg::RESEND ? 1 : 0;
    }
    return count;
}

HostEtherQueue::HostEtherQueue():
        enableLossRecovery(false),
        channelCapacity(0),
        rtt(0),
        maxStage(0),
        T(0),
        n(0),
        Winit(0),
        WAI(0),
        nextChannel(0),
        rateLimitTickCounter(0),
        ackClockTickCounter(0),
        oobL3InGate(nullptr),
        oobL3OutGate(nullptr),
        clockEvent(nullptr)
{}

HostEtherQueue::~HostEtherQueue()
{
    cancelAndDelete(clockEvent);
    for (auto ite = channels.begin(); ite != channels.end(); ite++) {
        delete ite->second;
    }
}

void HostEtherQueue::initialize()
{
    BaseQueue::initialize();

    enableLossRecovery = par("enableLossRecovery");
    channelCapacity = par("channelCapacity").longValue() * 1000; // Bytes
    rtt = par("rtt"); // us
    maxStage = par("maxStage");
    T = par("T"); // sec
    n = par("n");
    double nicSpeed = par("nicSpeed").doubleValue() * 1000 * 1000 * 125; // Bps
    Winit = nicSpeed * T;
    WAI = (Winit * (1 - n)) / par("N").longValue();

    oobL3InGate = gate("oobL3In");
    oobL3OutGate = gate("oobL3Out");

    setClock();
}

void HostEtherQueue::handleMessage(cMessage* msg)
{
    if (msg->isSelfMessage()) { // Timers
        handleClockEvent();
    } else {
        cGate* arrivalGate = msg->getArrivalGate();
        if (!strcmp(arrivalGate->getName(), "oobL3In")) {
            AckMessage* ackMsg;
            if ((ackMsg = dynamic_cast<AckMessage*>(msg)) != nullptr) { // Ack
                EV_INFO << "Received " << ackMsg << " message from remote host";
                auto ite = channels.find(ackMsg->getFlowId());
                if (ite == channels.end()) {
                    throw cRuntimeError("No corresponding channel found for Ack!");
                }
                Channel* channel = ite->second;
                // Send app ack for last pkt
                if (ackMsg->getAck() && ackMsg->getLastPkt()) {
                    if (channel->lastPktAck) { // This should always be not null. Anyway ...
                        send(channel->lastPktAck, oobL3OutGate);
                        channel->lastPktAck = nullptr;
                    }
                }
                // Handle ack
                channel->handleAckMessage(ackMsg);
                if (channel->getPendingResendCount()) {
                    if (packetRequested) {
                        sendPackets(true); // There are messages in the queue; clear them first
                    }
                    if (!packetRequested && channel->getPendingResendCount()) {
                        notifyListeners();
                    }
                }
            }
        } else {
            handleMessageEx(msg);
        }
    }
}

void HostEtherQueue::handleClockEvent()
{
    if (++rateLimitTickCounter == RATE_LIMIT_TICK_COUNT) {
        onRateLimitTimer();
    }

    if (enableLossRecovery) {
        if (++ackClockTickCounter == ACK_CLOCK_TICK_COUNT) {
            onAckClockTimer();
        }
    }

    setClock();
}

void HostEtherQueue::onRateLimitTimer()
{
    for (auto ite = channels.begin(); ite != channels.end(); ite++) {
        ite->second->onRateLimitTimer();
    }

    // Start new cycle
    rateLimitTickCounter = 0;

    // Flush queue; packets stuck in the queue won't go until next packet arrives at the queue
    if (packetRequested) {
        sendPackets(true);
    }
}

void HostEtherQueue::onAckClockTimer()
{
    for (auto ite = channels.begin(); ite != channels.end(); ite++) {
        Channel* channel = ite->second;
        channel->onAckClockTimer();

        if (channel->getPendingResendCount()) {
            if (packetRequested) {
                sendPackets(true); // There are messages in the queue; clear them first
            }
            if (!packetRequested && channel->getPendingResendCount()) {
                notifyListeners();
            }
        }
    }

    ackClockTickCounter = 0;
}

void HostEtherQueue::handleMessageEx(cMessage* msg)
{
    // Important: Messages should always go through the queue to support rate limiting
    numQueueReceived++;

    emit(rcvdPkSignal, msg);

    msg->setArrivalTime(simTime());
    cMessage* droppedMsg = enqueue(msg);

    if (droppedMsg) {
        numQueueDropped++;
        emit(dropPkByQueueSignal, droppedMsg);
        if (packetRequested) {
            sendPackets(true); // There are messages in the queue; so try
        }
    } else {
        emit(enqueuePkSignal, msg);
        if (packetRequested) {
            sendPackets(true); // There are messages in the queue; clear them first
        }
        if (!packetRequested && hasNewMsgs()) {
            notifyListeners();
        }
    }

    sendApplicationPacketAck(msg, !droppedMsg);

    if (droppedMsg) {
        delete droppedMsg;
    }
}

void HostEtherQueue::sendApplicationPacketAck(cMessage* msg, bool ack)
{
    EtherFrame* frame = dynamic_cast<EtherFrame*>(msg);
    if (!frame) {
        return;
    }
    cPacket* udpPacket = frame->getEncapsulatedPacket()->getEncapsulatedPacket();
    if (!udpPacket) {
        return;
    }
    UDPReliableAppPacket* appPacket = dynamic_cast<UDPReliableAppPacket*>(udpPacket->getEncapsulatedPacket());
    if (!appPacket) {
        return;
    }

    UDPReliableAppPacketAck* appAck = new UDPReliableAppPacketAck("UDPReliableAppPacketAck");
    appAck->setProtocolId(IPProtocolId::IP_PROT_UDP);
    appAck->setSourceId(appPacket->getSourceId());
    appAck->setAppGateIndex(appPacket->par("appGateIndex"));
    appAck->setPacketSize(appPacket->getByteLength());
    appAck->setFirstPkt(appPacket->getFirstPkt());
    appAck->setLastPkt(appPacket->getLastPkt());
    appAck->setAck(ack);

    send(appAck, oobL3OutGate);
}

UDPReliableAppPacketAck* HostEtherQueue::getLastPktAck(cMessage* msg)
{
    UDPReliableAppPacketAck* appAck = nullptr;

    UDPReliableAppPacket* appPacket = Channel::getAppPkt(msg);
    if (appPacket && appPacket->getLastPkt()) {
        appAck = new UDPReliableAppPacketAck("UDPReliableAppPacketAck");
        appAck->setProtocolId(IPProtocolId::IP_PROT_UDP);
        appAck->setSourceId(appPacket->getSourceId());
        appAck->setAppGateIndex(appPacket->par("appGateIndex"));
        appAck->setLastPktLeftQueue(true);
    }

    return appAck;
}

cMessage* HostEtherQueue::enqueue(cMessage* msg)
{
    cMessage* droppedMsg = addToChannel(msg);
    if (!droppedMsg) {
        queueSize += PK(msg)->getByteLength();
    }
    return droppedMsg;
}

cMessage* HostEtherQueue::dequeue()
{
    cMessage* msg = nullptr;

    // Use round robin as scheduling scheme to pick a message from the channels
    int chCount = channels.size();
    auto ite = channels.begin();
    for (int i = 0; i < nextChannel; i++) {
        ite++;
    }

    for (int i = 0; i < chCount; i++) {
        Channel* channel = ite->second;
        msg = channel->getMessage();
        if (msg) {
            UDPReliableAppPacketAck* appAck = getLastPktAck(msg);
            if (appAck) {
                /*if (enableLossRecovery) {
                    if (!channel->lastPktAck) { // Don't set if it's already set
                        channel->lastPktAck = appAck; // Send up when the ack is received
                    } else { // Unlikely
                        delete appAck;
                    }
                } else*/ {
                    send(appAck, oobL3OutGate); // No ack
                }
            }
            queueSize = getQueueSize();
            break;
        }

        if (++ite == channels.end()) {
            ite = channels.begin();
        }

        nextChannel = (nextChannel + 1) % chCount;
    }

    nextChannel = (nextChannel + 1) % chCount;

    return msg;
}

cMessage* HostEtherQueue::addToChannel(cMessage* msg)
{
    Channel* channel;
    uint32 flowId = FlowSensor::getFlowId(dynamic_cast<EtherFrame*>(msg));
    auto ite = channels.find(flowId);
    if (ite != channels.end()) {
        channel = ite->second;
    } else {
        channel = new Channel(flowId, enableLossRecovery, channelCapacity, rtt,
                maxStage, T, n, Winit, WAI);
        channel->initialize();
        channels[flowId] = channel;
    }

    return channel->addMessage(msg);
}

void HostEtherQueue::sendPackets(bool stale)
{
    if (!stale) { // requestPacket()
        packetRequested++;
    }

    while (packetRequested) {
        cMessage* msg = dequeue();
        if (!msg) {
            break;
        }

        //sendLastPktAck(msg);

        packetRequested--;

        emit(dequeuePkSignal, msg);
        emit(queueingTimeSignal, simTime() - msg->getArrivalTime());
        sendOut(msg);
   }
}

void HostEtherQueue::setClock()
{
    if (!clockEvent) {
        clockEvent = new cMessage("ClockEvent");
    }
    scheduleAt(simTime() + SimTime(CLOCK_TICK_LENGTH, SIMTIME_NS), clockEvent);
}

void HostEtherQueue::requestPacket()
{
    Enter_Method("requestPacket()");

    sendPackets();
}

int HostEtherQueue::getQueueSize()
{
    int size = 0;
    for (auto ite = channels.begin(); ite != channels.end(); ite++) {
        size += ite->second->depth;
    }
    return size;
}

bool HostEtherQueue::hasNewMsgs()
{
    for (auto ite = channels.begin(); ite != channels.end(); ite++) {
        Channel* channel = ite->second;
        if (!channel->queue.isEmpty()) {
            return true;
        }
    }
    return false;
}

bool HostEtherQueue::isEmpty()
{
    for (auto ite = channels.begin(); ite != channels.end(); ite++) {
        Channel* channel = ite->second;
        if (!channel->queue.isEmpty() || !channel->getPendingResendCount()) {
            return false;
        }
    }
    return true;
}

bool HostEtherQueue::isFull()
{
    return queueCapacity <= queueSize;
}

void HostEtherQueue::finish()
{
//    ofstream myfile;
//    myfile.open(getFullPath() + ".hpcc");
//    for (auto ite = channels.begin(); ite != channels.end(); ite++)
//        myfile << ite->second->ss.str();
//    myfile.close();

    int sentCount = 0;
    int resentCount = 0;
    for (auto ite = channels.begin(); ite != channels.end(); ite++) {
        Channel* channel = ite->second;
        sentCount += channel->numSent;
        resentCount += channel->numResent;
    }

    recordScalar("sentCount", sentCount);
    recordScalar("resentCount", resentCount);

    PassiveQueueBase::finish();
}

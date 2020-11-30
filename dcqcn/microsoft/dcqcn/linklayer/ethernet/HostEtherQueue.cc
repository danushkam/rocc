#include "HostEtherQueue.h"

#include "inet/linklayer/ethernet/EtherFrame.h"
#include "inet/networklayer/ipv4/IPv4Datagram.h"
#include "inet/networklayer/common/IPProtocolId_m.h"
#include "inet/applications/udpapp/UDPReliableAppPacket_m.h"
#include "inet/applications/udpapp/UDPReliableAppPacketAck_m.h"
#include "common/messages/PauseMessage_m.h"
#include "microsoft/dcqcn/networklayer/ipv4/L3RelayUnitEx.h"
#include <cmath>
#include <iostream>
#include <fstream>

using namespace std;
using namespace inet;
using namespace dcqcn;
using namespace commons;

Define_Module(HostEtherQueue);

const static int CLOCK_TICK_LENGTH = 1; // ns

const static int RATE_LIMIT_INTERVAL = 10; // ns -- Source
const static long T_INTERVAL = 55000; // ns

const static double G = 1 / 256.0;
const static int F = 5;
const static int AI_RATE = 4; // Mbps -- Additive increase; NOTE: Default value is 40
const static int HAI_RATE = 400; // Mbps -- Hyper additive increase
const static long B = 10000000; // Bytes

const static int RATE_LIMIT_TICK_COUNT = RATE_LIMIT_INTERVAL / CLOCK_TICK_LENGTH;
const static long T_TICK_COUNT = T_INTERVAL / CLOCK_TICK_LENGTH;

const static int ACK_CLOCK_INTERVAL = 1000; // ns

Channel::Channel(uint32 flwId, bool enableLossRec, int cpcty, int linkBw, int alphaTC, int rttVal):
        systemChannel(flwId == SYSTEM_FLOW_ID),
        flowId(flwId),
        enableLossRecovery(enableLossRec),
        capacity(cpcty),
        linkBandwidth(linkBw),
        alphaTickCount(alphaTC),
        rtt(rttVal),
        rateLimit(false),
        nextSequence(0),
        currentRate(0),
        targetRate(0),
        a(0),
        bitsPerInterval(0),
        bitsToSend(0),
        byteCounter(0),
        t(0),
        bc(0),
        depth(0),
        numReceived(0),
        numDropped(0),
        numSent(0),
        numResent(0),
        numAckReceived(0),
        numNackReceived(0),
        cnpMsgCount(0),
        alphaTickCounter(0),
        tTickCounter(0),
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

    createWatch((chName + "-depth").c_str(), depth);
    createWatch((chName + "-numReceived").c_str(), numReceived);
    createWatch((chName + "-numDropped").c_str(), numDropped);
    createWatch((chName + "-numSent").c_str(), numSent);
    createWatch((chName + "-numResent").c_str(), numResent);
    createWatch((chName + "-numAckReceived").c_str(), numAckReceived);
    createWatch((chName + "-numNackReceived").c_str(), numNackReceived);
    createWatch((chName + "-cnpMsgCount").c_str(), cnpMsgCount);

    thruputMeter.initialize(chName);

    currentRate = targetRate = linkBandwidth;
    a = 1;
}

void Channel::handleCnpMessage(CNPMessage* cnpMsg)
{
    // Calculate new rate
    targetRate = currentRate;
    currentRate = currentRate * (1 - (a * 0.5));
    a = (1 - G) * a + G;

    if (linkBandwidth < currentRate) {
        currentRate = linkBandwidth;
    }

    adjustRate(currentRate);

    delete cnpMsg;

    byteCounter = 0;
    t = 0;
    bc = 0;
    tTickCounter = 0;
    alphaTickCounter = 0;

    cnpMsgCount++;
}

void Channel::handleAckMessage(AckMessage* ackMsg)
{
    EV_INFO << "Ack message received!" << endl;

    auto ite = ackPendingMsgs.find(ackMsg->getExpectedSeq());
    if (ite != ackPendingMsgs.end()) {
        onAckReceived(ackMsg);

        lastAckSeq = ackMsg->getExpectedSeq();

        ackTimeoutTickCounter = 0;
        timeoutCount = 0;
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

    if (nextSequence < stopSeq) {
        nextSequence = stopSeq;
    }

    if (ackMsg->getAck()) { // Ack
        numAckReceived++;
    } else {
        numNackReceived++;
        nextSequence = stopSeq;

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
//    cMessage* msg = nullptr;
//
//    if (systemChannel && !queue.isEmpty()) {
//        msg = (cMessage*)queue.pop();
//    } else {
//        bool go = !queue.isEmpty() && (!rateLimit || PK(queue.front())->getBitLength() <= bitsToSend);
//        if (go) {
//            msg = (cMessage*)queue.pop();
//            if (rateLimit) {
//                bitsToSend -= PK(msg)->getBitLength();
//
//                byteCounter += PK(msg)->getByteLength();
//                if (B <= byteCounter) {
//                    bc++;
//                    increaseRate();
//                    byteCounter = 0;
//                }
//            }
//        }
//    }
//
//    if (msg) {
//        depth -= PK(msg)->getByteLength();
//        numSent++;
//        thruputMeter.handleMessage(msg);
//    }
//
//    return msg;

    if (systemChannel && !queue.isEmpty()) {
        cMessage* msg = (cMessage*)queue.pop();
        depth -= PK(msg)->getByteLength();
        numSent++;
        return msg;
    }

    cMessage* msg = nullptr;

    if (enableLossRecovery) {
        if (!ackPendingMsgs.empty() && (nextSequence <= ackPendingMsgs.rbegin()->first)) { // Retransmit
            AckPendingMsg* pendingMsg = ackPendingMsgs.find(nextSequence)->second;
            bool go = !rateLimit || PK(pendingMsg->msg)->getBitLength() <= bitsToSend;
            if (go) {
               msg = pendingMsg->msg->dup();
               pendingMsg->status = AckPendingMsg::ACK_PENDING;
               numResent++;
            }
        } else { // Transmit
            bool go = !queue.isEmpty() && (!rateLimit || PK(queue.front())->getBitLength() <= bitsToSend);
            if (go) {
                msg = (cMessage*)queue.pop();
                if (getSequence(msg) != nextSequence) {
                    throw cRuntimeError("Next frame doesn't have the expected sequence number!");
                }
                AckPendingMsg* pendingMsg = new AckPendingMsg;
                pendingMsg->seqNumber = nextSequence;
                pendingMsg->msg = msg->dup();
                pendingMsg->status = AckPendingMsg::ACK_PENDING;
                ackPendingMsgs[nextSequence] = pendingMsg;
                numSent++;
            }
        }
    } else {
        bool go = !queue.isEmpty() && (!rateLimit || PK(queue.front())->getBitLength() <= bitsToSend);
        if (go) {
            msg = (cMessage*)queue.pop();
            numSent++;
        }
    }

    if (msg) {
        if (enableLossRecovery) {
            nextSequence++;
        } else {
            depth -= PK(msg)->getByteLength();
        }

        if (rateLimit) {
            bitsToSend -= PK(msg)->getBitLength();

            byteCounter += PK(msg)->getByteLength();
            if (B <= byteCounter) {
                bc++;
                increaseRate();
                byteCounter = 0;
            }
        }

        thruputMeter.handleMessage(msg);
    }

    return msg;
}

void Channel::handleClockEvent()
{
    if (++tTickCounter == T_TICK_COUNT) {
        onRateIncreaseTimer();
    }

    if (++alphaTickCounter == alphaTickCount) {
        onAlphaUpdateTimer();
    }
}

void Channel::onRateLimitTimer()
{
    // Include carry over from previous interval
    if (rateLimit) {
        bitsToSend += bitsPerInterval;
    }
}

void Channel::onRateIncreaseTimer()
{
    t++;
    increaseRate();
    tTickCounter = 0;
}

void Channel::onAlphaUpdateTimer()
{
    a = (1 - G) * a;
    alphaTickCounter = 0;
}

void Channel::onAckClockTimer()
{
    if (!ackPendingMsgs.empty()) {
        if (ackTimeoutTickCounter++ == rtt) {
            bool timeout = nextSequence - lastAckSeq < 3 ? true : false;
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
                nextSequence = ackPendingMsgs.begin()->second->seqNumber;
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

void Channel::increaseRate()
{
    if (max(t, bc) < F) { // Fast recovery
        targetRate += 0;
    } else if (F < min(t, bc)) { // Hyper increase
        targetRate += HAI_RATE;
    } else { // Additive increase
        targetRate += AI_RATE;
    }

    currentRate = (currentRate + targetRate) / 2;

    if (linkBandwidth < currentRate) {
        currentRate = linkBandwidth;
    }

    adjustRate(currentRate);
}

void Channel::adjustRate(double rate)
{
    EV_INFO << "Rate adjusting!" << endl;
    rateVector.recordWithTimestamp(simTime(), rate);

    double bitsPerSec = rate * 1000 * 1000;
    bitsPerInterval = (bitsPerSec * RATE_LIMIT_INTERVAL) / (1000 * 1000 * 1000);

    rateLimit = true; // Trigger rate limit

    onRateLimitTimer(); // Rectify asap
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
        linkBandwidth(0),
        cnpMsgCount(0),
        nextChannel(0),
        rateLimitTickCounter(0),
        alphaTickCount(0),
        rtt(0),
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
    linkBandwidth = par("linkBandwidth").longValue() * 1000; // Mbps
    int networkDelay = par("networkDelay").longValue() * 1000; // ns; one-way
    alphaTickCount = (CNP_INTERVAL + networkDelay) / CLOCK_TICK_LENGTH;
    rtt = par("rtt"); // us

    oobL3InGate = gate("oobL3In");
    oobL3OutGate = gate("oobL3Out");

    setClock();

    WATCH(cnpMsgCount);
}

void HostEtherQueue::handleMessage(cMessage* msg)
{
    if (msg->isSelfMessage()) { // Timers
        handleClockEvent();
    } else {
        cGate* arrivalGate = msg->getArrivalGate();
        if (!strcmp(arrivalGate->getName(), "oobL3In")) {
            CNPMessage* cnpMsg;
            AckMessage* ackMsg;
            if ((cnpMsg = dynamic_cast<CNPMessage*>(msg)) != nullptr) {
                EV_INFO << "Received " << cnpMsg << " message from destination.\n";
                handleCnpMessage(cnpMsg);
            } else if ((ackMsg = dynamic_cast<AckMessage*>(msg)) != nullptr) { // Ack
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
                        channel->rateLimit = false; // Don't rate-limit next flow
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
            handleMessageEx(msg); // Used by both source and destination
        }
    }
}

void HostEtherQueue::handleClockEvent()
{
    if (++rateLimitTickCounter == RATE_LIMIT_TICK_COUNT) {
        onRateLimitTimer();
    }

    if (enableLossRecovery) {
        if (++ackClockTickCounter == ACK_CLOCK_INTERVAL) {
            onAckClockTimer();
        }
    }

    for (auto ite = channels.begin(); ite != channels.end(); ite++) {
        ite->second->handleClockEvent();
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

void HostEtherQueue::handleCnpMessage(CNPMessage* cnpMsg)
{
    EV_INFO << "CNP message received!" << endl;

    auto ite = channels.find(cnpMsg->getFlowId());
    if (ite == channels.end()) {
        throw cRuntimeError("No corresponding channel found for CNP message!");
    }
    Channel* channel = ite->second;
    channel->handleCnpMessage(cnpMsg);

    cnpMsgCount++;
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
        msg = ite->second->getMessage();
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
            queueSize -= PK(msg)->getByteLength();
            break;
        }

        ite++; // TODO: Move this inside the if condition below; Use ++ite
        if (ite == channels.end()) {
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
        channel = new Channel(flowId, enableLossRecovery, channelCapacity, linkBandwidth, alphaTickCount, rtt);
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
//    return !hasNewMsgs();
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
//    myfile.open(getFullPath() + ".txt");
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
    recordScalar("cnpCount", cnpMsgCount);

    PassiveQueueBase::finish();
}

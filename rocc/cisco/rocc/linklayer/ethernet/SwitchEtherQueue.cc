#include "../../../rocc/linklayer/ethernet/SwitchEtherQueue.h"

#include "inet/linklayer/ethernet/EtherFrame.h"
#include "inet/networklayer/ipv4/IPv4Datagram.h"
#include "common/messages/PauseMessage_m.h"
#include "common/FlowSensor.h"
#include <cmath>
#include "../../../rocc/common/BufferUsage_m.h"

using namespace std;
using namespace inet;
using namespace commons;
using namespace rocc;

Define_Module(SwitchEtherQueue);

const static int CLOCK_TICK_LENGTH = 1; // ns

const static int T = 40000; // ns
const static int RATE_RESOLUTION = 10; // Mbps
const static int QUEUE_RESOLUTION = 600; // Bytes

simsignal_t SwitchEtherQueue::queueSizeSignal = registerSignal("queueSize");
simsignal_t SwitchEtherQueue::rateSignal = registerSignal("rate");
simsignal_t SwitchEtherQueue::rateMsgBwSignal = registerSignal("rateMsgBw");

SwitchEtherQueue::SwitchEtherQueue():
        enablePFC(false),
        congCtrl(false),
        paused(false),
        congested(false),
        linkBandwidth(0),
        minRate(0),
        maxRate(0),
        refQueueSize(0),
        midQueueSize(0),
        maxQueueSize(0),
        pauseThreshold(0),
        unpauseThreshold(0),
        alphaBar(0),
        betaBar(0),
        currentRate(0),
        oldQueueSize(0),
        numSent(0),
        rateMessageCount(0),
        pauseCount(0),
        congCtrlTickCounter(0),
        oobL3InGate(nullptr),
        oobL3OutGate(nullptr),
        clockEvent(nullptr)
{}

SwitchEtherQueue::~SwitchEtherQueue()
{
    cancelAndDelete(clockEvent);
}

void SwitchEtherQueue::initialize()
{
    BaseQueue::initialize();

    enablePFC = par("enablePFC");
    congCtrl = par("congCtrl");
    linkBandwidth = par("linkBandwidth").longValue() * 1000; // Mbps
    minRate = par("minRate"); // Mbps
    maxRate = par("maxRate"); // Mbps
    refQueueSize = par("refQueueSize").longValue() * 1000; // Bytes
    midQueueSize = par("midQueueSize").longValue() * 1000;
    maxQueueSize = par("maxQueueSize").longValue() * 1000;
    pauseThreshold = par("pauseThreshold").longValue() * 1000;
    unpauseThreshold = par("unpauseThreshold").longValue() * 1000;
    alphaBar = par("alphaBar");
    betaBar = par("betaBar");

    oobL3InGate = gate("oobL3In");
    oobL3OutGate = gate("oobL3Out");

    setClock();

    WATCH(rateMessageCount);
    WATCH(pauseCount);
    WATCH(paused);
}

void SwitchEtherQueue::handleMessage(cMessage* msg)
{
    if (msg->isSelfMessage()) { // Timers
        handleClockEvent();
    } else {
        PassiveQueueBase::handleMessage(msg);
    }
}

void SwitchEtherQueue::handleClockEvent()
{
    if (congCtrl) {
        if (++congCtrlTickCounter == T) {
            onCongCtrlTimer();
        }
    }

    setClock();
}

void SwitchEtherQueue::onCongCtrlTimer()
{
    // Send new fair rate
    currentRate = recalculateFairRate();
    emit(rateSignal, currentRate);

//    RateMessage* rateMsg = new RateMessage("RateMessage");
//    rateMsg->setRate(currentRate);
//
//    send(rateMsg, oobL3OutGate);
//    EV_INFO << "Sent " << rateMsg << " message from L3FCN engine.\n";
//
//    rateMessageCount++;

    for (auto ite = newFlowTable.begin(); ite != newFlowTable.end(); ite++) {
        RateMessage* rateMsg = new RateMessage("RateMessage");
        rateMsg->setRate(currentRate);
        rateMsg->setFlowId(ite->first);

        EV_INFO << "Sent " << rateMsg << " message from L3FCN engine.\n";
        send(rateMsg, oobL3OutGate);

        rateMessageCount++;
    }

    emit(rateMsgBwSignal, newFlowTable.size());

    // Start new cycle
    congCtrlTickCounter = 0;
    oldQueueSize = queueSize;
}

int SwitchEtherQueue::recalculateFairRate()
{
//    if (paused) {
//        return minRate;
//    }

    EV_INFO << "old: " << oldQueueSize << ", now: " << queueSize << endl;

    int fairRate = 0;

//    // MD
//    if ((maxQueueSize < queueSize) &&
//        (maxQueueSize < (queueSize - oldQueueSize))) {
//        fairRate = minRate;
//    } else if (midQueueSize < (queueSize - oldQueueSize)) {
//        fairRate = currentRate / 2;
//    } else { // PI
//        fairRate = getFairRateForAlgo1();
//    }
//
//    if (fairRate < minRate) {
//        fairRate = minRate;
//    }
//    if (maxRate < fairRate) {
//        fairRate = maxRate;
//    }

    // MD
    int qCur = queueSize / QUEUE_RESOLUTION;
    int qOld = oldQueueSize / QUEUE_RESOLUTION;

    int qRef = refQueueSize / QUEUE_RESOLUTION;
    int qMid = midQueueSize / QUEUE_RESOLUTION;
    int qMax = maxQueueSize / QUEUE_RESOLUTION;

    int fMin = minRate / RATE_RESOLUTION;
    int fMax = maxRate / RATE_RESOLUTION;

    if ((qCur >= qMax) && (currentRate > (fMax / 8))) {
        fairRate = fMin;
    } else if (((qCur - qOld) >= qMid) && (currentRate > (fMax / 8))) {
        fairRate = currentRate / 2;
    } else { // PI
        double a, b;
        autoTune(a, b);
        fairRate = currentRate - a * (qCur - qRef) - b * (qCur - qOld);
    }

    if (fairRate < fMin) {
        fairRate = fMin;
    }
    if (fMax < fairRate) {
        fairRate = fMax;
    }

    return fairRate;
}

void SwitchEtherQueue::autoTune(double& a, double& b)
{
    int fMax = maxRate / RATE_RESOLUTION;
    int level = 2;
    while ((currentRate < (fMax / level)) && (level < 64)) {
        level *= 2;
    }
    double ratio = level / 2.0;
    a = alphaBar/*0.3*//*0.8*/ / ratio;
    b = betaBar/*1.5*//*2.4*/ / ratio;
}

int SwitchEtherQueue::getFairRateForAlgo1()
{
    double A, B, coef;
    int queueOld = (oldQueueSize + queueSize) / 2;

    A = 0.7 / pow(2, 7);
    B = 0.7 / pow(2, 5);

    double ch = linkBandwidth / 2.0;
    if ((queueSize < (refQueueSize / 8)) && (queueOld < (refQueueSize / 8))) {
        coef = 1;
    } else {
        coef = currentRate / ch;
    }

    if (coef < (1/64.0)) {
        coef = 1/64.0;
    }

    if (1 < coef) {
        coef = 1;
    }

//        int ratio = 1;
//        if ((queueSize < (refQueueSize / 8)) && (queueOld < (refQueueSize / 8))) {
//            ratio = 1;
//        } else {
//            if (currentRate < (linkBandwidth / 128))
//                ratio = 64;
//            else if (currentRate < (linkBandwidth / 64))
//                ratio = 32;
//            else if (currentRate < (linkBandwidth / 32))
//                    ratio = 16;
//            else if (currentRate < (linkBandwidth / 16))
//                    ratio = 8;
//            else if (currentRate < (linkBandwidth / 8))
//                    ratio = 4;
//            else if (currentRate < (linkBandwidth / 4))
//                    ratio = 2;
//            else if (currentRate < (linkBandwidth / 2))
//                    ratio = 1;
//        }
//
//        coef = 1.0 / ratio;

    double a = A * coef;
    double b = B * coef;
    int correction = a * (queueSize - refQueueSize) + b * (queueSize - queueOld);
    int fairRate = currentRate - correction;

    return fairRate;
}

bool SwitchEtherQueue::isFull()
{
    return queueCapacity <= queueSize;
}

void SwitchEtherQueue::processIngressMessage(const cMessage* msg)
{
    BaseQueue::processIngressMessage(msg);

    emitQueueSize();
    UpdateBufferUsage(msg);

    if (enablePFC && !paused && (pauseThreshold <= queueSize)) {
        sendPause();
    }

    // Update flow table
    uint32 flowId = FlowSensor::getFlowId((const EtherFrame *)msg);
    auto ite = newFlowTable.find(flowId);
    if (ite == newFlowTable.end())
        newFlowTable[flowId] = 1;
    else
        ite->second++;
}

void SwitchEtherQueue::processEgressMessage(const cMessage* msg)
{
    BaseQueue::processEgressMessage(msg);

    emitQueueSize();
    UpdateBufferUsage(msg, true);

    if (enablePFC && paused && (queueSize <= unpauseThreshold)) {
        sendUnpause();
    }

    // Update flow table
    uint32 flowId = FlowSensor::getFlowId((const EtherFrame *)msg);
    auto ite = newFlowTable.find(flowId);
    if (--ite->second == 0)
        newFlowTable.erase(ite);
}

void SwitchEtherQueue::sendPause()
{
    // Send pause message
    PauseMessage* pauseMsg = new PauseMessage();
    pauseMsg->setPauseUnits(65535);

    send(pauseMsg, oobL3OutGate);

    paused = true;
    pauseCount++;
}

void SwitchEtherQueue::sendUnpause()
{
    // Send pause message
    PauseMessage* pauseMsg = new PauseMessage();
    pauseMsg->setPauseUnits(0);

    send(pauseMsg, oobL3OutGate);

    paused = false;
}

void SwitchEtherQueue::UpdateBufferUsage(const cMessage* msg, bool release)
{
    const cPacket* frame = dynamic_cast<const cPacket*>(msg);
    cPacket* datagram = frame->getEncapsulatedPacket();
    if (datagram && datagram->hasPar("IngressPort")) {
        BufferUsage* bufferUsage = new BufferUsage();
        bufferUsage->setPortNum(datagram->par("IngressPort").longValue());
        bufferUsage->setUsage(frame->getByteLength() * (release ? -1 : 1));
        send(bufferUsage, oobL3OutGate);
        if (release) {
            delete datagram->getParList().remove("IngressPort");
        }
    }
}

void SwitchEtherQueue::setClock()
{
    if (!clockEvent) {
        clockEvent = new cMessage("ClockEvent");
    }
    scheduleAt(simTime() + SimTime(CLOCK_TICK_LENGTH, SIMTIME_NS), clockEvent);
}

void SwitchEtherQueue::emitQueueSize()
{
    emit(queueSizeSignal, queueSize / 1000);
}

void SwitchEtherQueue::finish()
{
    recordScalar("pauseCount", pauseCount);
    recordScalar("dropCount", numQueueDropped);

    PassiveQueueBase::finish();
}

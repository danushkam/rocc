#include "SwitchEtherQueue.h"

#include "inet/linklayer/ethernet/EtherFrame.h"
#include "inet/networklayer/ipv4/IPv4Datagram.h"
#include "common/messages/PauseMessage_m.h"
#include "microsoft/dcqcn/common/BufferUsage_m.h"
#include <cmath>
#include <iostream>
#include <fstream>

using namespace std;
using namespace omnetpp;
using namespace inet;
using namespace dcqcn;
using namespace commons;

Define_Module(SwitchEtherQueue);

const static double WQ = 0.2; // 1; // 0.5; // 0.002;
const static double MAX_P = 0.01; // 0.1;
const static int MIN_TH = 5000; // Bytes
const static int MAX_TH = 200000; // Bytes
const static int ECN_CE = 3;

const static int PMAX = 500;
const static int PMIN = 0;
const static int CLOCK_TICK_LENGTH = 1; // ns
const static int PROB_UPDATE_INTERVAL = 10000; // ns
const static int PROB_UPDATE_TICK_COUNT = PROB_UPDATE_INTERVAL / CLOCK_TICK_LENGTH;

simsignal_t SwitchEtherQueue::queueSizeSignal = registerSignal("queueSize");
simsignal_t SwitchEtherQueue::markProbabilitySignal = registerSignal("markProbability");
simsignal_t SwitchEtherQueue::spotProbabilitySignal = registerSignal("spotProbability");

SwitchEtherQueue::SwitchEtherQueue():
        enablePFC(false),
        congCtrl(false),
        paused(false),
        wq(WQ),
        maxp(MAX_P),
        minth(MIN_TH),
        maxth(MAX_TH),
        pauseThreshold(0),
        unpauseThreshold(0),
        avg(0),
        count(-1),
        qTime(0),
        oldQueueSize(0),
        currDropProb(0),
        pauseCount(0),
        probUpdateTickCounter(0),
        clockEvent(nullptr),
        oobL3InGate(nullptr),
        oobL3OutGate(nullptr)
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
    pauseThreshold = par("pauseThreshold").longValue() * 1000;
    unpauseThreshold = par("unpauseThreshold").longValue() * 1000;

    oobL3InGate = gate("oobL3In");
    oobL3OutGate = gate("oobL3Out");

    setClock();

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
//    if (congCtrl) {
//        if (++probUpdateTickCounter == PROB_UPDATE_TICK_COUNT) {
//            onProbUpdateTimer();
//        }
//    }
//
//    setClock();
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

    if (shouldMarkPkt(PK((cMessage*)msg))) {
        IPv4Datagram* datagram = dynamic_cast<IPv4Datagram*>(PK((cMessage*)msg)->getEncapsulatedPacket());
        datagram->setExplicitCongestionNotification(ECN_CE);
    }
}

void SwitchEtherQueue::processEgressMessage(const cMessage* msg)
{
    BaseQueue::processEgressMessage(msg);

    emitQueueSize();
    UpdateBufferUsage(msg, true);

    if (congCtrl && (queueSize == 0)) {
        qTime = simTime();
    }

    if (enablePFC && paused && (queueSize <= unpauseThreshold)) {
        sendUnpause();
    }
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

void SwitchEtherQueue::onProbUpdateTimer()
{
    double qRef = 200;
    double qMax = 400;

    double qLen = queueSize / 624.0;
    double qOld = ((oldQueueSize + queueSize) / 2) / 624.0;

    double fairProb = 0;
    if (qMax < qLen) {
        fairProb = PMAX;
    } else {
        fairProb += 0.5 * (qLen -qRef) + 10 * (qLen - qOld);
        fairProb = PMAX < fairProb ? PMAX : fairProb;
        fairProb = fairProb < PMIN ? PMIN : fairProb;
    }

    currDropProb = fairProb / 1000;

    emit(markProbabilitySignal, currDropProb);

    probUpdateTickCounter = 0;
}

void SwitchEtherQueue::emitQueueSize()
{
    emit(queueSizeSignal, queueSize / 1000);
}

bool SwitchEtherQueue::shouldMarkPkt(cPacket* pkt)
{
    if (!congCtrl) {
        return false;
    }

    if (!pkt) {
        return false;
    }

    IPv4Datagram* datagram = dynamic_cast<IPv4Datagram*>(pkt->getEncapsulatedPacket());
    if (!datagram) {
        return false;
    }

    if (datagram->getExplicitCongestionNotification() == ECN_CE) {
        //throw cRuntimeError("Packet already ECN marked!");
        return false;
    }

    return markRED();
    //return markPIE();
}

bool SwitchEtherQueue::markRED()
{
    if (0 < queueSize) {
        avg = (1 - wq) * avg + wq * queueSize;
    } else {
        const double m = SIMTIME_DBL(simTime() - qTime);
        avg = pow(1 - wq, m) * avg;
    }

    if (minth <= avg && avg < maxth) {
        count++;
        const double pb = (maxp * (avg - minth)) / (maxth - minth);
        const double pa = pb / (1 - count * pb);
        double pc = dblrand();
        emit(markProbabilitySignal, pc);
        if (pc < pa) {
            count = 0;
            return true;
        }
    } else if (maxth <= avg || maxth <= queueSize) {
        count = 0;
        return true;
    } else {
        count = -1;
    }

    return false;
}

bool SwitchEtherQueue::markPIE()
{
    return dblrand() < currDropProb;
}

void SwitchEtherQueue::finish()
{
    recordScalar("pauseCount", pauseCount);
    recordScalar("dropCount", numQueueDropped);

    PassiveQueueBase::finish();
}


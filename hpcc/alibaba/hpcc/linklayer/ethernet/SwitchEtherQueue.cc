#include "SwitchEtherQueue.h"
#include "inet/linklayer/ethernet/EtherFrame.h"
#include "inet/networklayer/ipv4/IPv4Datagram.h"
#include "inet/applications/udpapp/UDPReliableAppPacket_m.h"
#include "common/messages/PauseMessage_m.h"
#include "alibaba/hpcc/common/BufferUsage_m.h"
#include <cmath>
#include <iostream>
#include <fstream>

using namespace std;
using namespace inet;
using namespace commons;
using namespace hpcc;

Define_Module(SwitchEtherQueue);

simsignal_t SwitchEtherQueue::queueSizeSignal = registerSignal("queueSize");

SwitchEtherQueue::SwitchEtherQueue():
        enablePFC(false),
        congCtrl(false),
        paused(false),
        B(0),
        pauseThreshold(0),
        unpauseThreshold(0),
        txBytes(0),
        pauseCount(0),
        oobL3InGate(nullptr),
        oobL3OutGate(nullptr)
{}

SwitchEtherQueue::~SwitchEtherQueue()
{}

void SwitchEtherQueue::initialize()
{
    BaseQueue::initialize();

    enablePFC = par("enablePFC");
    congCtrl = par("congCtrl");
    B = par("B").longValue() * 1000 * 1000 * 125.0; // Bps
    pauseThreshold = par("pauseThreshold").longValue() * 1000;
    unpauseThreshold = par("unpauseThreshold").longValue() * 1000;

    oobL3InGate = gate("oobL3In");
    oobL3OutGate = gate("oobL3Out");

    WATCH(pauseCount);
    WATCH(paused);
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
}

void SwitchEtherQueue::processEgressMessage(const cMessage* msg)
{
    BaseQueue::processEgressMessage(msg);

    emitQueueSize();
    UpdateBufferUsage(msg, true);

    if (congCtrl) {
        txBytes += PK((cMessage*)msg)->getByteLength();
        injectINT(PK((cMessage*)msg));
    }

    if (enablePFC && paused && (queueSize <= unpauseThreshold)) {
        sendUnpause();
    }
}

void SwitchEtherQueue::injectINT(cPacket* packet)
{
    cPacket* datagram = packet->getEncapsulatedPacket();
    if (!datagram) {
        return;
    }
    cPacket* udpPacket = datagram->getEncapsulatedPacket();
    if (!udpPacket) {
        return;
    }
    UDPReliableAppPacket* appPacket = dynamic_cast<UDPReliableAppPacket*>(udpPacket->getEncapsulatedPacket());
    if (!appPacket) {
        return;
    }

    NodeINT& nodeInt = appPacket->getL(appPacket->getNHop());
    nodeInt.B = B;
    nodeInt.qLen = queueSize;
    nodeInt.txBytes = txBytes;
    nodeInt.ts = simTime().dbl();

    appPacket->setNHop(appPacket->getNHop() + 1);
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

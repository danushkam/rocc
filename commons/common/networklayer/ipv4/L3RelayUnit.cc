#include "L3RelayUnit.h"
#include "inet/linklayer/common/Ieee802Ctrl.h"
#include "inet/applications/udpapp/UDPReliableAppPacketAck_m.h"
#include "common/messages/PauseMessage_m.h"

using namespace std;
using namespace inet;
using namespace commons;

Define_Module(L3RelayUnit);

L3RelayUnit::L3RelayUnit():
        flowSensor(nullptr)
{}

L3RelayUnit::~L3RelayUnit()
{
    delete flowSensor;
}

void L3RelayUnit::initialize()
{
    flowSensor = new FlowSensor;
}

void L3RelayUnit::handleMessage(cMessage* msg)
{
    cGate* arrivalGate = msg->getArrivalGate();

    if (!strcmp(arrivalGate->getName(), "icmpIn")) {
        handleMessageIcmp(msg);
    } else if (!strcmp(arrivalGate->getBaseName(), "ipIn")) {
        handleMessageIp(msg);
    } else if (!strcmp(arrivalGate->getBaseName(), "ifIn")) {
        handleMessageIf(msg);
    } else if (!strcmp(arrivalGate->getBaseName(), "ifOobIn")) {
        handleMessageIfOob(msg);
    }
}

void L3RelayUnit::handleMessageIp(cMessage* msg)
{
    cGate* arrivalGate = msg->getArrivalGate();
    int channelId = arrivalGate->getIndex();
    if (flowSensor) {
        const cPacket* packet = dynamic_cast<const cPacket*>(msg);
        flowSensor->onEgressData(packet);
    }

    send(msg, "ifOut", channelId);
}

void L3RelayUnit::handleMessageIf(cMessage* msg)
{
    cGate* arrivalGate = msg->getArrivalGate();
    int channelId = arrivalGate->getIndex();
    if (flowSensor) {
        const cPacket* packet = dynamic_cast<const cPacket*>(msg);
        flowSensor->onIngressData(packet);
    }
    send(msg, "ipOut", channelId);
}

void L3RelayUnit::handleMessageIfOob(cMessage* msg)
{
    cGate* arrivalGate = msg->getArrivalGate();
    UDPReliableAppPacketAck* appAck;
    PauseMessage* pauseMsg;

    if ((appAck = dynamic_cast<UDPReliableAppPacketAck*>(msg)) != nullptr) { // Host only
        int channelId = arrivalGate->getIndex();
        send(msg, "ipOut", channelId);
    } else if ((pauseMsg = dynamic_cast<PauseMessage*>(msg)) != nullptr) { // Switch only
        if (!flowSensor) {
            throw cRuntimeError("PAUSE triggered from lower layer when flow sensor is not active!");
        }
        // Find all the ingress flows that go through the egress interface the pause message
        // originated from and forward it to the source that corresponds to each ingress flow
        set<int> ingressIfIds;
        getIngressIfIds(arrivalGate->getIndex(), ingressIfIds);
        for (int ingressIfId : ingressIfIds) {
            if (!sendPauseMsg(ingressIfId, pauseMsg->getPauseUnits()))
                continue;
            Ieee802Ctrl* ctrl = new Ieee802Ctrl();
            ctrl->setPauseUnits(pauseMsg->getPauseUnits());
            cMessage* pauseCtrl = new cMessage("PauseCtrl", IEEE802CTRL_SENDPAUSE);
            pauseCtrl->setControlInfo(ctrl);
            send(pauseCtrl, "ifOut", ingressIfId);
        }
        delete msg;
    }
}

void L3RelayUnit::getIngressIfIds(int egressIfId, set<int>& ingressIfIds)
{
    list<const IngressFlow*> flows;
    flowSensor->getIngressFlows(flows, egressIfId);

    for (const IngressFlow* flow : flows)
        ingressIfIds.insert(flow->ingressIfId);
}

bool L3RelayUnit::sendPauseMsg(int ingressIfId, bool pause)
{
    auto ite = pauseMsgController.find(ingressIfId);
    if (pause) {
        if (ite == pauseMsgController.end()) {
            pauseMsgController[ingressIfId] = 1;
            return true;
        } else
            ite->second++;
    } else {
        if (--ite->second == 0) {
            pauseMsgController.erase(ite);
            return true;
        }
    }

    return false;
}

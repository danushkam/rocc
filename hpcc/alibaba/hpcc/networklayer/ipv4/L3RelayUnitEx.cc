#include "L3RelayUnitEx.h"

#include "inet/linklayer/common/Ieee802Ctrl.h"
#include "alibaba/hpcc/common/BufferUsage_m.h"

using namespace std;
using namespace inet;
using namespace commons;
using namespace hpcc;

Define_Module(L3RelayUnitEx);

L3RelayUnitEx::L3RelayUnitEx():
        L3RelayUnit(),
        enableLossRecovery(false),
        enablePFC(false),
        pauseThreshold(0),
        unpauseThreshold(0),
        numAckSent(0),
        numNackSent(0),
        numDropped(0)
{}

L3RelayUnitEx::~L3RelayUnitEx()
{
    for (auto ite = ingressPorts.begin(); ite != ingressPorts.end(); ite++) {
        delete ite->second;
    }
    ingressPorts.clear();
}

void L3RelayUnitEx::initialize()
{
    L3RelayUnit::initialize();

    enableLossRecovery = par("enableLossRecovery");
    enablePFC = par("enablePFC");
    pauseThreshold = par("pauseThreshold").longValue() * 1000; // Bytes
    unpauseThreshold = par("unpauseThreshold").longValue() * 1000;

    WATCH(numAckSent);
    WATCH(numNackSent);
    WATCH(numDropped);
}

void L3RelayUnitEx::handleMessageIcmp(cMessage* msg)
{
    // Source only
    AckMessage* ackMsg;
    if ((ackMsg = dynamic_cast<AckMessage*>(msg)) != nullptr) {
        EV_INFO << "Received " << ackMsg << " message from remote host";
        const EgressFlow* flow = flowSensor->getEgressFlow(ackMsg->getFlowId());
        send(ackMsg, "ifOobOut", flow->egressIfId);
    }
}

void L3RelayUnitEx::handleMessageIf(cMessage* msg)
{
    // PFC
    if (enablePFC && !FlowSensor::systemFlow(FlowSensor::getFlowId((const IPv4Datagram*)msg))) {
        int portNum = msg->getArrivalGate()->getIndex();
        // Update port table
        auto ite = ingressPorts.find(portNum);
        if (ite == ingressPorts.end()) {
            IngressPort* port = new IngressPort(portNum);
            port->initialize();
            ingressPorts[portNum] = port;
        }
        // Mark ingress port number in the message
        msg->addPar("IngressPort").setLongValue(portNum);
    }

    // HPCC
    if (enableLossRecovery && ReceiveHostAdaptor::appPkt(msg)) { // Only recover app pkts
        AckMessage* ackMsg = hostAdaptor.receive(msg);
        bool drop = true;
        if (ackMsg) { // In order (ori/dup) or first nack
            if (ackMsg->getAck()) { // In order
                if (!ackMsg->getDupAck()) {
                    L3RelayUnit::handleMessageIf(msg);
                    drop = false;
                    numAckSent++;
                }
            } else { // First nack
                numNackSent++;
            }
            send(ackMsg, "icmpOut");
        }

        if (drop) {
            delete msg;
            numDropped++;
        }
    } else {
        L3RelayUnit::handleMessageIf(msg);
    }
}

void L3RelayUnitEx::handleMessageIfOob(cMessage* msg)
{
    if (dynamic_cast<BufferUsage*>(msg) != nullptr) {
        BufferUsage* bufferUsage = (BufferUsage*)msg;
        IngressPort* port;
        auto ite = ingressPorts.find(bufferUsage->getPortNum());
        if (ite == ingressPorts.end()) {
            throw cRuntimeError("Buffer usage received for invalid port number!");
        }
        port = ite->second;
        port->updateBuffSize(bufferUsage->getUsage());
        delete msg;
        // Trigger PAUSE
        if (enablePFC) {
            if (port->pause(pauseThreshold)) {
                sendPause(port);
            } else if (port->unpause(unpauseThreshold)) {
                sendPause(port, false);
            }
        }
    } else {
        L3RelayUnit::handleMessageIfOob(msg);
    }
}

void L3RelayUnitEx::sendPause(IngressPort* port, bool pause)
{
    Ieee802Ctrl* ctrl = new Ieee802Ctrl();
    ctrl->setPauseUnits(pause ? 65535 : 0);
    cMessage* pauseCtrl = new cMessage("PauseCtrl", IEEE802CTRL_SENDPAUSE);
    pauseCtrl->setControlInfo(ctrl);
    send(pauseCtrl, "ifOut", port->portNum);

    if (pause) {
        port->paused = true;
        port->numPauseFramesSent++;
    } else {
        port->paused = false;
        port->numUnpauseFramesSent++;
    }
}

void L3RelayUnitEx::finish()
{
    hostAdaptor.finish(getFullPath());
}

#include "L3RelayUnitEx.h"

#include "inet/linklayer/common/Ieee802Ctrl.h"
#include "microsoft/dcqcn/common/BufferUsage_m.h"
#include "microsoft/dcqcn/common/CNPMessage_m.h"

using namespace std;
using namespace inet;
using namespace dcqcn;
using namespace commons;

Define_Module(L3RelayUnitEx);

const static int ECN_CE = 3;

L3RelayUnitEx::L3RelayUnitEx():
        L3RelayUnit(),
        enableLossRecovery(false),
        enablePFC(false),
        pauseThreshold(0),
        unpauseThreshold(0),
        numAckSent(0),
        numNackSent(0),
        numDropped(0),
        ecnCount(0),
        cnpCount(0),
        clockEvent(nullptr)
{}

L3RelayUnitEx::~L3RelayUnitEx()
{
    cancelAndDelete(clockEvent);

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
    destination = par("destination");
    pauseThreshold = par("pauseThreshold").longValue() * 1000; // Bytes
    unpauseThreshold = par("unpauseThreshold").longValue() * 1000;

    setClock();

    WATCH(numAckSent);
    WATCH(numNackSent);
    WATCH(numDropped);

    WATCH(ecnCount);
    WATCH(cnpCount);
}

void L3RelayUnitEx::handleMessage(cMessage* msg)
{
    if (msg->isSelfMessage()) { // Timers
        handleClockEvent();
    } else {
        L3RelayUnit::handleMessage(msg);
    }
}

void L3RelayUnitEx::handleMessageIcmp(cMessage* msg)
{
    // Source only
    CNPMessage* cnpMsg;
    AckMessage* ackMsg;
    if ((cnpMsg = dynamic_cast<CNPMessage*>(msg)) != nullptr) {
        const EgressFlow* flow = flowSensor->getEgressFlow(cnpMsg->getFlowId());
        send(msg, "ifOobOut", flow->egressIfId);
    } else if ((ackMsg = dynamic_cast<AckMessage*>(msg)) != nullptr) {
        EV_INFO << "Received " << ackMsg << " message from remote host";
        const EgressFlow* flow = flowSensor->getEgressFlow(ackMsg->getFlowId());
        send(ackMsg, "ifOobOut", flow->egressIfId);
    }
}

void L3RelayUnitEx::handleMessageIf(cMessage* msg)
{
    if (destination) {
        handleEcnMarkedPacket(PK(msg));
    }

    // PFC (on switch)
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

    // Loss recovery
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

void L3RelayUnitEx::handleEcnMarkedPacket(const cPacket* packet)
{
    const IPv4Datagram* datagram = dynamic_cast<const IPv4Datagram*>(packet);
    if (!datagram) {
        return;
    }

    if (datagram->getExplicitCongestionNotification() == ECN_CE) {
        uint32 flowId = FlowSensor::getFlowId(datagram);
        if (cnpSentFlows.find(flowId) == cnpSentFlows.end()) { // CNP not sent for the flow in this time window
            // Send CNP
            CNPMessage* cnpMsg = new CNPMessage();
            cnpMsg->setFlowId(flowId);
            cnpMsg->setDestAddress(FlowSensor::getSrcAddress(datagram));
            send(cnpMsg, "icmpOut");

            cnpSentFlows.insert(flowId);
            cnpCount++;
        }
        ecnCount++;
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

void L3RelayUnitEx::handleClockEvent()
{
    cnpSentFlows.clear();

    setClock();
}

void L3RelayUnitEx::setClock()
{
    if (!clockEvent) {
        clockEvent = new cMessage("ClockEvent");
    }
    scheduleAt(simTime() + SimTime(CNP_INTERVAL, SIMTIME_NS), clockEvent);
}

void L3RelayUnitEx::finish()
{
    hostAdaptor.finish(getFullPath());

//    std::ofstream myfile;
//    myfile.open(getFullPath() + ".fbr"); // Feedback ratio
//    myfile << ss.str();
//    myfile.close();
}

#include "../../../rocc/networklayer/ipv4/L3RelayUnitEx.h"

#include "inet/linklayer/common/Ieee802Ctrl.h"
#include <iostream>
#include <fstream>
#include "../../../rocc/common/BufferUsage_m.h"
#include "../../../rocc/common/RateMessage_m.h"
#include "../../../rocc/networklayer/ipv4/ETFlowSensor.h"

using namespace std;
using namespace inet;
using namespace commons;
using namespace rocc;

Define_Module(L3RelayUnitEx);

simsignal_t L3RelayUnitEx::rateMsgFanoutSignal = registerSignal("rateMsgFanout");

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
    flowSensor = new ETFlowSensor(par("elephantThreshold").longValue() * 1000);
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
    RateMessage* rateMsg;
    AckMessage* ackMsg;
    if ((rateMsg = dynamic_cast<RateMessage*>(msg)) != nullptr) {
        EV_INFO << "Received " << rateMsg << " message from remote switch (Rate: " << rateMsg->getRate() << ").\n";
        const EgressFlow* flow = flowSensor->getEgressFlow(rateMsg->getFlowId());
        send(rateMsg, "ifOobOut", flow->egressIfId);
    } else if ((ackMsg = dynamic_cast<AckMessage*>(msg)) != nullptr) {
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
    // Switch only
    if (dynamic_cast<RateMessage*>(msg) != nullptr) {
        // Find all the ingress flows that go through the egress interface the rate/pause message
        // originated from and forward it to the source that corresponds to each ingress flow
//        cGate* arrivalGate = msg->getArrivalGate();
//        int egressIfId = arrivalGate->getIndex();
//        list<const IngressFlow*> flows;
//        int totFlowCount;
//        ((ETFlowSensor*)flowSensor)->getElephantFlows(flows, egressIfId, totFlowCount);
//        for (const IngressFlow* flow : flows) {
//            RateMessage* rateMsgClone = rateMsg->dup();
//            rateMsgClone->setDestAddress(flow->srcAddress);
//            rateMsgClone->setFlowId(flow->flowId);
//            send(rateMsgClone, "icmpOut");
//        }
//        lastSentRate = rateMsg->getRate();
        RateMessage* rateMsg = (RateMessage*)msg;
        const IngressFlow* flow = flowSensor->getIngressFlow(rateMsg->getFlowId());
        if (flow) {
            rateMsg->setDestAddress(flow->srcAddress);
            send(rateMsg, "icmpOut");
        }
//        emit(rateMsgFanoutSignal, flows.size());
//        delete msg;
//        double flowCount = flows.size();
//        double percent = (totFlowCount != 0) ? (flowCount / totFlowCount) * 100 : 0;
//        ss << "FBR: Sent:" << flowCount << ", Tot:" << totFlowCount << " (" << percent << ")"<< endl;
    } else if (dynamic_cast<BufferUsage*>(msg) != nullptr) {
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

//    std::ofstream myfile;
//    myfile.open(getFullPath() + ".fbr"); // Feedback ratio
//    myfile << ss.str();
//    myfile.close();
}

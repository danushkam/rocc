#include "ICMPEx.h"
#include "inet/networklayer/contract/ipv4/IPv4ControlInfo.h"

using namespace inet;
using namespace hpcc;

Define_Module(ICMPEx);

ICMPEx::ICMPEx():
        ICMP(),
        numAcksSent(0),
        numAcksReceived(0)
{
    WATCH(numAcksSent);
    WATCH(numAcksReceived);
}

void ICMPEx::handleMessage(cMessage* msg)
{
    cGate* arrivalGate = msg->getArrivalGate();

    if (!strcmp(arrivalGate->getName(), "l3RelayIn")) {
        AckMessage* ackMsg;
        if ((ackMsg = dynamic_cast<AckMessage*>(msg)) != nullptr) { // Destination only
            EV_INFO << "Received " << ackMsg << " message from IRN host handler.\n";
            sendAck(ackMsg);
        }
    } else {
        ICMP::handleMessage(msg);
    }
}

void ICMPEx::sendAck(AckMessage* ackMsg)
{
    IPv4ControlInfo* ipv4Ctrl = new IPv4ControlInfo();
    ipv4Ctrl->setProtocol(IP_PROT_ICMP);
    ipv4Ctrl->setDestinationAddress(L3Address(IPv4Address(ackMsg->getDestAddress())));

    ICMPMessage* icmpMsg = new ICMPMessage("L3FCNAck");
    icmpMsg->setType(ICMP_L3FCN_ACK);
    icmpMsg->setControlInfo(ipv4Ctrl);
    icmpMsg->encapsulate(ackMsg);

    sendToIP(icmpMsg);

    numAcksSent++;

    EV_INFO << "Sent " << icmpMsg << " message to network layer.\n";
}

void ICMPEx::processICMPMessage(ICMPMessage* icmpmsg)
{
    // Source only
    if (icmpmsg->getType() == ICMP_L3FCN_ACK) {
        IPv4ControlInfo* ipv4Ctrl = dynamic_cast<IPv4ControlInfo*>(icmpmsg->getControlInfo());
        AckMessage* ackMsg = dynamic_cast<AckMessage*>(icmpmsg->decapsulate());
        if (!ackMsg) {
            throw cRuntimeError("Unexpected ICMP message received!");
        }
        if (ipv4Ctrl->getDestinationAddress().toIPv4().getInt() != ackMsg->getDestAddress()) {
            throw cRuntimeError("Ack message received at wrong destination!");
        }
        EV_INFO << "Received " << ackMsg << " message from network.\n";
        delete icmpmsg;
        send(ackMsg, "l3RelayOut");
        numAcksReceived++;
    } else {
        ICMP::processICMPMessage(icmpmsg);
    }
}

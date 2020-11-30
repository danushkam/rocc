#include "inet/networklayer/contract/ipv4/IPv4ControlInfo.h"
#include "ICMPEx.h"

using namespace inet;
using namespace dcqcn;

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

    // Destination only
    if (!strcmp(arrivalGate->getName(), "l3RelayIn")) {
        CNPMessage* cnpMsg;
        AckMessage* ackMsg;
        if ((cnpMsg = dynamic_cast<CNPMessage*>(msg)) != nullptr) {
            EV_INFO << "Received " << cnpMsg << " message from relay unit.\n";
            sendCNPMessage(cnpMsg);
        } else if ((ackMsg = dynamic_cast<AckMessage*>(msg)) != nullptr) { // Destination only
            EV_INFO << "Received " << ackMsg << " message from host handler.\n";
            sendAck(ackMsg);
        }
    } else {
        ICMP::handleMessage(msg);
    }
}

void ICMPEx::sendCNPMessage(CNPMessage* cnpMsg)
{
    IPv4ControlInfo* ipv4Ctrl = new IPv4ControlInfo();
    ipv4Ctrl->setProtocol(IP_PROT_ICMP);
    ipv4Ctrl->setDestinationAddress(L3Address(IPv4Address(cnpMsg->getDestAddress())));

    ICMPMessage* icmpMsg = new ICMPMessage("CNP");
    icmpMsg->setType(ICMP_DCQCN_CNP);
    icmpMsg->setControlInfo(ipv4Ctrl);
    icmpMsg->encapsulate(cnpMsg);

    sendToIP(icmpMsg);

    EV_INFO << "Sent " << icmpMsg << " message to network layer.\n";
}

void ICMPEx::sendAck(AckMessage* ackMsg)
{
    IPv4ControlInfo* ipv4Ctrl = new IPv4ControlInfo();
    ipv4Ctrl->setProtocol(IP_PROT_ICMP);
    ipv4Ctrl->setDestinationAddress(L3Address(IPv4Address(ackMsg->getDestAddress())));

    ICMPMessage* icmpMsg = new ICMPMessage("L3FCNAck");
    icmpMsg->setType(ICMP_DCQCN_ACK);
    icmpMsg->setControlInfo(ipv4Ctrl);
    icmpMsg->encapsulate(ackMsg);

    sendToIP(icmpMsg);

    numAcksSent++;

    EV_INFO << "Sent " << icmpMsg << " message to network layer.\n";
}


void ICMPEx::processICMPMessage(ICMPMessage* icmpmsg)
{
    // Source only
    if (icmpmsg->getType() == ICMP_DCQCN_CNP) {
        IPv4ControlInfo* ipv4Ctrl = dynamic_cast<IPv4ControlInfo*>(icmpmsg->getControlInfo());
        CNPMessage* cnpMsg = dynamic_cast<CNPMessage*>(icmpmsg->decapsulate());
        if (!cnpMsg) {
            throw cRuntimeError("Unexpected ICMP message received!");
        }
        if (ipv4Ctrl->getDestinationAddress().toIPv4().getInt() != cnpMsg->getDestAddress()) {
            throw cRuntimeError("CNP message received at wrong destination!");
        }
        EV_INFO << "Received " << cnpMsg << " message from network.\n";
        delete icmpmsg;
        send(cnpMsg, "l3RelayOut");
    } else if (icmpmsg->getType() == ICMP_DCQCN_ACK) {
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

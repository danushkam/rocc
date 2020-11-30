#include "ReceiveHostAdaptor.h"
#include "inet/networklayer/ipv4/IPv4Datagram.h"
#include "inet/transportlayer/udp/UDPPacket.h"
#include "inet/applications/udpapp/UDPReliableAppPacket_m.h"
#include "common/FlowSensor.h"
#include <iostream>
#include <fstream>

using namespace std;
using namespace inet;
using namespace dcqcn;
using namespace commons;

bool ReceiveHostAdaptor::appPkt(cMessage* msg)
{
    UDPPacket* udpPkt = dynamic_cast<UDPPacket*>(PK(msg)->getEncapsulatedPacket());
    if (!udpPkt) {
        return false;
    }

    return dynamic_cast<UDPReliableAppPacket*>(udpPkt->getEncapsulatedPacket());
}

AckMessage* ReceiveHostAdaptor::receive(cMessage* msg)
{
    IPv4Datagram* datagram = (IPv4Datagram*)msg;
    UDPReliableAppPacket* appPkt =
            (UDPReliableAppPacket*)datagram->getEncapsulatedPacket()->getEncapsulatedPacket();

    uint32 flowId = FlowSensor::getFlowId(datagram);
    Channel* channel;
    auto ite = channels.find(flowId);
    if (ite != channels.end()) {
        channel = ite->second;
    } else {
        channel = new Channel;
        channel->nextSequence = 0;
        channel->inRecovery = false;
        channels[flowId] = channel;
    }

    AckMessage* ackMsg = nullptr;

    bool sendAck = true;
    bool ack = true;
    bool dupAck = false;
    uint32 expectedSeq = channel->nextSequence;

//    ss << flowId << " " << appPkt->getSequenceNumber() << " " << expectedSeq << endl;

    if (appPkt->getSequenceNumber() == expectedSeq) { // In order
        channel->inRecovery = false;
        channel->nextSequence++;
    } else if (expectedSeq < appPkt->getSequenceNumber()) { // OOO
        ack = false;
        sendAck = !channel->inRecovery; // Send nack for first ooo pkt
        channel->inRecovery = true;
    } else { // Lost ack
        dupAck = true;
        expectedSeq = appPkt->getSequenceNumber();
    }

    if (sendAck) {
        ackMsg = new AckMessage;
        ackMsg->setFlowId(flowId);
        ackMsg->setDestAddress(FlowSensor::getSrcAddress(datagram));
        ackMsg->setExpectedSeq(expectedSeq);
        ackMsg->setSackNo(appPkt->getSequenceNumber());
        ackMsg->setAck(ack);
        ackMsg->setLastPkt(appPkt->getLastPkt());
        ackMsg->setDupAck(dupAck);
    }

    return ackMsg;
}

void ReceiveHostAdaptor::finish(string path)
{
//    ofstream file;
//    file.open(path + ".rtt");
//    file << ss.str();
//    file.close();
}

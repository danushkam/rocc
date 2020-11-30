#include "FlowSensor.h"
#include "inet/transportlayer/udp/UDPPacket.h"
#include "inet/applications/udpapp/UDPReliableAppPacket_m.h"

using namespace std;
using namespace inet;
using namespace commons;

FlowSensor::~FlowSensor()
{
    for (auto ite = ingressFlowById.begin(); ite != ingressFlowById.end(); ite++) {
        delete ite->second;
    }
    for (auto ite = egressFlowById.begin(); ite != egressFlowById.end(); ite++) {
        delete ite->second;
    }
    ingressFlowById.clear();
    egressFlowById.clear();
    egressFlowByInterface.clear();
}

IngressFlow* FlowSensor::onIngressData(const cPacket* packet)
{
    const IPv4Datagram* datagram = dynamic_cast<const IPv4Datagram*>(packet);
    if (!datagram) {
        return nullptr;
    }

    IngressFlow* flow;
    uint32 flowId = getFlowId(datagram);
    auto it = ingressFlowById.find(flowId);
    if (it != ingressFlowById.end()) { // Flow already created
        flow = it->second;
    } else {
        flow = createIngressFlow(flowId, datagram);
        ingressFlowById[flowId] = flow;
    }

    return flow;
}

IngressFlow* FlowSensor::createIngressFlow(uint32 flowId, const IPv4Datagram* datagram)
{
    IngressFlow* flow = new IngressFlow(flowId);
    flow->ingressIfId = datagram->getArrivalGate()->getIndex();
    flow->srcAddress = getSrcAddress(datagram);

    return flow;
}

EgressFlow* FlowSensor::onEgressData(const cPacket* packet)
{
    const IPv4Datagram* datagram = dynamic_cast<const IPv4Datagram*>(packet);
    if (!datagram) {
        return nullptr;
    }

    EgressFlow* flow;
    uint32 flowId = getFlowId(datagram);
    const auto it = egressFlowById.find(flowId);
    if (it != egressFlowById.end()) { // Flow already created
        flow = it->second;
    } else {
        flow = new EgressFlow(flowId);
        flow->egressIfId = datagram->getArrivalGate()->getIndex(); // ipIn:ifOut = 1:1
        flow->destAddress = getDestAddress(datagram);

        egressFlowById[flowId] = flow;
        egressFlowByInterface.insert(make_pair(flow->egressIfId, flow));
    }

    return flow;
}

const IngressFlow* FlowSensor::getIngressFlow(uint32 flowId) const
{
    auto it = ingressFlowById.find(flowId);
    if (it != ingressFlowById.end()) {
        return it->second;
    }

    return nullptr;
}

const EgressFlow* FlowSensor::getEgressFlow(uint32 flowId) const
{
    auto it = egressFlowById.find(flowId);
    if (it != egressFlowById.end()) {
        return it->second;
    }

    return nullptr;
}

void FlowSensor::getIngressFlows(list<const IngressFlow*>& flows, int egressIfId) const
{
    auto range = egressFlowByInterface.equal_range(egressIfId);
    for (auto it = range.first; it != range.second; it++) {
        const EgressFlow* egressFlow = it->second;

        const auto it2 = ingressFlowById.find(egressFlow->flowId);
        if (it2 == ingressFlowById.end()) {
            continue;
        }

        flows.push_back(it2->second);
    }
}

void FlowSensor::getEgressFlows(list<const EgressFlow*>& flows, int egressIfId) const
{
    auto range = egressFlowByInterface.equal_range(egressIfId);
    for (auto it = range.first; it != range.second; it++) {
        flows.push_back(it->second);
    }
}

uint32 FlowSensor::getFlowId(const IPv4Datagram* datagram)
{
//    uint32 flowId = datagram->getSrcAddress().getInt() + datagram->getDestAddress().getInt();
//    const UDPPacket* udpPkt = dynamic_cast<const UDPPacket*>(datagram->getEncapsulatedPacket());
//    if (udpPkt) {
//        flowId += udpPkt->getSrcPort() + udpPkt->getDestPort();
//    }
//    return flowId;

    uint32 flowId;

    const UDPPacket* udpPkt = dynamic_cast<const UDPPacket*>(datagram->getEncapsulatedPacket());
    if (udpPkt) { // Usually application data
        flowId = datagram->getSrcAddress().getInt() + datagram->getDestAddress().getInt() + udpPkt->getSrcPort() + udpPkt->getDestPort();
        /*UDPReliableAppPacket* appPkt = dynamic_cast<UDPReliableAppPacket*>(udpPkt->getEncapsulatedPacket());
        flowId = appPkt->getFlowId();*/

//        stringstream ss;
//        ss << datagram->getSrcAddress().getInt() << datagram->getDestAddress().getInt();
//        ss << udpPkt->getSrcPort() << udpPkt->getDestPort();
//
//        // Algorithm: djb2; http://www.cse.yorku.ca/~oz/hash.html
//        flowId = 5381;
//        int c;
//        const char* str = ss.str().c_str();
//
//        while ((c = *str++)) {
//            flowId = ((flowId << 5) + flowId) + c; /* flowId * 33 + c */
//        }
    } else {
        flowId = SYSTEM_FLOW_ID;
    }

    return flowId;
}

uint32 FlowSensor::getFlowId(const EtherFrame* frame)
{
    uint32 flowId = SYSTEM_FLOW_ID;
    const IPv4Datagram* datagram = dynamic_cast<const IPv4Datagram*>(frame->getEncapsulatedPacket());
    if (datagram) {
        flowId = getFlowId(datagram);
    }
    return flowId;
}

uint32 FlowSensor::getSrcAddress(const IPv4Datagram* datagram)
{
    return datagram->getSrcAddress().getInt();
}

uint32 FlowSensor::getSrcAddress(const EtherFrame* frame)
{
    return getSrcAddress((const IPv4Datagram*)frame->getEncapsulatedPacket());
}

uint32 FlowSensor::getDestAddress(const IPv4Datagram* datagram)
{
    return datagram->getDestAddress().getInt();
}

uint32 FlowSensor::getDestAddress(const EtherFrame* frame)
{
    return getDestAddress((const IPv4Datagram*)frame->getEncapsulatedPacket());
}

bool FlowSensor::systemFlow(uint32 flowId)
{
    return (flowId == SYSTEM_FLOW_ID);
}

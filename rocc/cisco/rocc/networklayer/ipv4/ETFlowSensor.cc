#include "../../../rocc/networklayer/ipv4/ETFlowSensor.h"

using namespace std;
using namespace inet;
using namespace commons;
using namespace rocc;

ETFlowSensor::ETFlowSensor(int eleThreshold) :
        elephantThreshold(eleThreshold)
{}

IngressFlow* ETFlowSensor::onIngressData(const cPacket* packet)
{
    ETIngressFlow* flow = (ETIngressFlow*)FlowSensor::onIngressData(packet);

    if (elephantThreshold && flow) {
        if (flow->byteCount < elephantThreshold) { // Not quite there!
            flow->byteCount += packet->getByteLength();

            if ((elephantThreshold <= flow->byteCount) &&
                (activeFlows.find(flow->flowId) == activeFlows.end())) {
                activeFlows[flow->flowId] = flow;
            }
        }
        flow->aged = false;
    }

    return flow;
}

IngressFlow* ETFlowSensor::createIngressFlow(uint32 flowId, const IPv4Datagram* datagram)
{
    ETIngressFlow* flow = new ETIngressFlow(flowId);
    flow->ingressIfId = datagram->getArrivalGate()->getIndex();
    flow->srcAddress = getSrcAddress(datagram);

    return flow;
}

void ETFlowSensor::getElephantFlows(list<const IngressFlow*>& flows, int egressIfId, int& totFlowCount) const
{
    if (elephantThreshold) {
        totFlowCount = 0;
        auto range = egressFlowByInterface.equal_range(egressIfId);
        for (auto it = range.first; it != range.second; it++) {
            const EgressFlow* egressFlow = it->second;
            totFlowCount++;

            const auto it2 = activeFlows.find(egressFlow->flowId);
            if (it2 == activeFlows.end()) {
                continue;
            }

            flows.push_back(it2->second);
        }
    } else {
        FlowSensor::getIngressFlows(flows, egressIfId);
        totFlowCount = flows.size();
    }
}

void ETFlowSensor::invalidateElephants()
{
    for (auto ite = activeFlows.begin(); ite != activeFlows.end(); ) {
        ETIngressFlow* flow = ite->second;
        if (flow->aged) {
            flow->aged = false;
            flow->byteCount = 0;
            activeFlows.erase(ite++);
        } else {
            flow->aged = true;
            ite++;
        }
    }
}

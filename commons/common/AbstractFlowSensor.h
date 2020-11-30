#ifndef __ABSTRACT_FLOW_SENSOR_H
#define __ABSTRACT_FLOW_SENSOR_H

#include "inet/linklayer/ethernet/EtherFrame.h"
#include "inet/networklayer/contract/ipv4/IPv4Address.h"
#include <list>

namespace commons
{

struct Flow
{
    Flow(inet::uint32 id):
        flowId(id) {}

    inet::uint32 flowId;
};

struct IngressFlow : Flow
{
    IngressFlow(inet::uint32 id):
        Flow(id), ingressIfId(-1), srcAddress(0) {}

    int ingressIfId;
    inet::uint32 srcAddress;
};

struct EgressFlow : Flow
{
    EgressFlow(inet::uint32 id):
        Flow(id), egressIfId(-1), destAddress(0) {}

    int egressIfId;
    inet::uint32 destAddress;
};

class AbstractFlowSensor
{
  public:
    AbstractFlowSensor() {}
    virtual ~AbstractFlowSensor() {}

    virtual IngressFlow* onIngressData(const cPacket* packet) = 0;
    virtual EgressFlow* onEgressData(const cPacket* packet) = 0;

    /**
     * Required to deliver rate messages
     */
    virtual const IngressFlow* getIngressFlow(inet::uint32 flowId) const = 0;

    /**
     * Required to deliver incoming rate messages
     */
    virtual const EgressFlow* getEgressFlow(inet::uint32 flowId) const = 0;

    /**
     * Required to send rate, pause messages. Get set of ingress flows that eventually egress through a given interface.
     */
    virtual void getIngressFlows(std::list<const IngressFlow*>& flows, int egressIfId) const = 0;

    /**
     * Required to send something downstream.
     */
    virtual void getEgressFlows(std::list<const EgressFlow*>& flows, int egressIfId) const = 0;
};

} // namespace commons

#endif // #ifndef __ABSTRACT_FLOW_SENSOR_H

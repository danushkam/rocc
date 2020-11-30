#ifndef __FLOW_SENSOR_H
#define __FLOW_SENSOR_H

#include "AbstractFlowSensor.h"
#include "inet/networklayer/ipv4/IPv4Datagram.h"
#include <vector>
#include <map>

namespace commons
{

const static inet::uint32 SYSTEM_FLOW_ID = 0;

class FlowSensor : public AbstractFlowSensor
{
  public:
    virtual ~FlowSensor();

    virtual IngressFlow* onIngressData(const cPacket* packet);
    virtual EgressFlow* onEgressData(const cPacket* packet);

    virtual const IngressFlow* getIngressFlow(inet::uint32 flowId) const;
    virtual const EgressFlow* getEgressFlow(inet::uint32 flowId) const;
    virtual void getIngressFlows(std::list<const IngressFlow*>& flows, int egressIfId) const;
    virtual void getEgressFlows(std::list<const EgressFlow*>& flows, int egressIfId) const;

    static inet::uint32 getFlowId(const inet::IPv4Datagram* datagram);
    static inet::uint32 getFlowId(const inet::EtherFrame* frame);
    static inet::uint32 getSrcAddress(const inet::IPv4Datagram* datagram);
    static inet::uint32 getSrcAddress(const inet::EtherFrame* frame);
    static inet::uint32 getDestAddress(const inet::IPv4Datagram* datagram);
    static inet::uint32 getDestAddress(const inet::EtherFrame* frame);

    static bool systemFlow(inet::uint32 flowId);

  protected:
    std::map<inet::uint32, IngressFlow*> ingressFlowById;
    std::map<inet::uint32, EgressFlow*> egressFlowById;
    std::multimap<int, EgressFlow*> egressFlowByInterface;

    virtual IngressFlow* createIngressFlow(inet::uint32 flowId, const inet::IPv4Datagram* datagram);
};

} // namespace commons

#endif // #ifndef __FLOW_SENSOR_H

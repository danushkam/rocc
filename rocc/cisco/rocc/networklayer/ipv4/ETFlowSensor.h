#ifndef __ET_FLOW_SENSOR_H
#define __ET_FLOW_SENSOR_H

#include "common/FlowSensor.h"

namespace rocc
{

struct ETIngressFlow : commons::IngressFlow
{
    ETIngressFlow(inet::uint32 id):
        commons::IngressFlow(id), aged(false), byteCount(0) {}

    bool aged;
    inet::uint32 byteCount;
};

class ETFlowSensor : public commons::FlowSensor
{
  public:
    ETFlowSensor(int eleThreshold);

    virtual commons::IngressFlow* onIngressData(const cPacket* packet) override;

    void getElephantFlows(std::list<const commons::IngressFlow*>& flows, int egressIfId, int& totFlowCount) const;

    void invalidateElephants();

  private:
    int elephantThreshold;

    std::map<inet::uint32, ETIngressFlow*> activeFlows;

    virtual commons::IngressFlow* createIngressFlow(inet::uint32 flowId, const inet::IPv4Datagram* datagram) override;
};

} // namespace rocc

#endif // #ifndef __ET_FLOW_SENSOR_H

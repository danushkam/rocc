#ifndef __RECEIVE_HOST_ADAPTOR_H
#define __RECEIVE_HOST_ADAPTOR_H

#include "inet/common/Compat.h"
#include "inet/applications/udpapp/UDPReliableAppPacket_m.h"
#include <map>
#include "../../../rocc/common/AckMessage_m.h"

namespace rocc {

class ReceiveHostAdaptor
{
  public:
    static bool appPkt(omnetpp::cMessage* msg);

    AckMessage* receive(omnetpp::cMessage* msg);

    void finish(std::string path);

  private:
    struct Channel
    {
        inet::uint32 nextSequence;
        bool inRecovery;
    };
    typedef std::map<inet::uint32, Channel*> Channels;

    Channels channels; // By flow

    std::stringstream ss;
};

} // namespace rocc

#endif // #ifndef __RECEIVE_HOST_ADAPTOR_H

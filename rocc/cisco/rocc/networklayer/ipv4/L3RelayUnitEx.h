#ifndef __L3_RELAY_UNIT_EX_H
#define __L3_RELAY_UNIT_EX_H

#include "common/networklayer/ipv4/L3RelayUnit.h"
#include <map>
#include "../../../rocc/networklayer/ipv4/ReceiveHostAdaptor.h"

namespace rocc {

class INET_API L3RelayUnitEx : public commons::L3RelayUnit
{
    struct IngressPort
    {
        IngressPort(int pn):
            portNum(pn),
            paused(false),
            numPauseFramesSent(0),
            numUnpauseFramesSent(0),
            buffSize(0)
        {}

        void initialize()
        {
            std::string portName = "ingressPort-";
            portName += std::to_string(portNum);

            createWatch((portName + "-numPauseFramesSent").c_str(), numPauseFramesSent);
            createWatch((portName + "-numUnpauseFramesSent").c_str(), numUnpauseFramesSent);
            createWatch((portName + "-ingressQueueSize").c_str(), buffSize); // Virtual ingress queue
        }

        void updateBuffSize(int buffUsage)
        {
            buffSize += buffUsage;
        }

        bool pause(const int& pauseTh) const
        {
            return !paused && (pauseTh <= buffSize);
        }

        bool unpause(const int& unpauseTh) const
        {
            return paused && (buffSize <= unpauseTh);
        }

        // Configuration
        int portNum;

        // State
        bool paused;
        int numPauseFramesSent;
        int numUnpauseFramesSent;
        int buffSize;
    };

  public:
    L3RelayUnitEx();
    virtual ~L3RelayUnitEx();

  protected:
    // Statistics
    static simsignal_t rateMsgFanoutSignal;

    virtual void initialize() override;

    virtual void handleMessageIcmp(cMessage* msg) override;
    virtual void handleMessageIf(cMessage* msg) override;
    virtual void handleMessageIfOob(cMessage* msg) override;

    void sendPause(IngressPort* port, bool pause = true);

  private:
    // Configuration
    bool enableLossRecovery;
    bool enablePFC;
    int pauseThreshold; // Bytes
    int unpauseThreshold; // Bytes

    int numAckSent;
    int numNackSent;
    int numDropped;

    ReceiveHostAdaptor hostAdaptor;

    std::map<int, IngressPort*> ingressPorts;

    std::stringstream ss;

    virtual void finish() override;
};

} // namespace rocc

#endif // #ifndef __L3_RELAY_UNIT_EX_H

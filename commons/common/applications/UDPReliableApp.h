//
// Copyright (C) 2004 Andras Varga
// Copyright (C) 2000 Institut fuer Telematik, Universitaet Karlsruhe
// Copyright (C) 2011 Zoltan Bojthe
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//

//
// This traffic generator has been evolved from UDPBasicBurst
//

#ifndef __UDP_RELIABLE_APP_H
#define __UDP_RELIABLE_APP_H

#include "inet/common/INETDefs.h"
#include "inet/applications/base/ApplicationBase.h"
#include "inet/applications/udpapp/UDPReliableAppPacket_m.h"
#include "inet/applications/udpapp/UDPReliableAppPacketAck_m.h"
#include "inet/transportlayer/contract/udp/UDPSocket.h"
#include "ranvar.h"
#include <mutex>
#include <map>
#include <set>

namespace commons {

const static int FLOW_COUNT = 50000;

class FlowAllocator
{
  public:
    static FlowAllocator* flowAllocator;
    static FlowAllocator* getInstance();

//    bool startFlow(); // Return true if a flow can start or false otherwise
    bool finishFlow(); // Return true if the last flow to finish or false otherwise
    inet::uint32 getNewFlowId();

  private:
    FlowAllocator();

    std::mutex mtxRem;
    std::mutex mtxCom;
//    int remainingFlowCount;
    int completedFlowCount;
    std::set<inet::uint32> flowIdPool;
};

/**
 * UDP application. See NED for more info.
 */
class UDPReliableApp : public inet::ApplicationBase
{
  protected:
    // --- Common ---
    int localPort = -1, destPort = -1;

    inet::UDPSocket socket;
    inet::L3Address destAddr;

    bool isSource = false;

    // statistics:
    int numPktSent = 0;
    int numPktReceived = 0;
    int numAppNack = 0;

    static simsignal_t sentPkSignal;
    static simsignal_t rcvdPkSignal;
    static simsignal_t flowSizeSignal;
    static simsignal_t sleepDurationSignal;

    // --- Sender ---
    // parameters
    simtime_t startTime;
    simtime_t stopTime;
    bool canStop;
    int flowCount;
    double dataRate;
    long flowSize;
    EmpiricalRandomVariable* flowSizeVar = nullptr;

    // volatile parameters
    cPar* sleepDurationPar;

    bool checkOrder;

    cMessage* timerNext = nullptr;
    simtime_t nextPkt;

    bool newFlow = true;
    bool lastAckRcvd = true; // By NIC
    inet::uint32 flowId = 0;
    inet::uint32 pktSequence = 0;
    inet::uint32 flowSequence = 0;
    inet::uint32 pktSubSequence = 0;
    int sentFlowCount;
    double sendInterval;

    // --- Receiver ---
    struct Flow
    {
        // Flows
        int sourceId;
        int rcvdFlowCount;

        // Current flow
        long flowSize; // Bytes
        long flowSeq;
        long rcvdFlowSize;
        long nextPktSeq;
    };
    enum SelfMsgKinds { START = 1, NEW, SEND, STOP, IDLE, SLEEP };
    typedef std::map<int, Flow*> SourceFlow; // Flow by source

    int rcvdFlowCount = 0;

    SourceFlow sourceFlow;
    std::stringstream flowStartTimes, flowCompTimes;

  protected:
    // chooses random destination address
    virtual inet::UDPReliableAppPacket* createPacket(long size);
    virtual void processPacket(cMessage* msg);

    virtual int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage *msg) override;
    virtual void finish() override;
    virtual void refreshDisplay() const override;

    virtual void processStart();
    virtual void processNew();
    virtual void processSend();
    virtual void processStop();
    virtual void processIdle();
    virtual void processSleep();

    virtual bool handleNodeStart(inet::IDoneCallback *doneCallback) override;
    virtual bool handleNodeShutdown(inet::IDoneCallback *doneCallback) override;
    virtual void handleNodeCrash() override;

    void handlePacketAck(inet::UDPReliableAppPacketAck* ack);
    void updateFlow(inet::UDPReliableAppPacket* pkt);
    void onFlowCompletion(Flow* flow);

    long getFlowSize() const;

    std::stringstream ss;

  public:
    UDPReliableApp() {}
    ~UDPReliableApp();
};

} // namespace commons

#endif // ifndef __UDP_RELIABLE_APP_H


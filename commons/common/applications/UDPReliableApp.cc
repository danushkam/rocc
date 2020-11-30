//
// Copyright (C) 2000 Institut fuer Telematik, Universitaet Karlsruhe
// Copyright (C) 2007 Universidad de Málaga
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

#include "UDPReliableApp.h"

#include "inet/transportlayer/contract/udp/UDPControlInfo_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/common/ModuleAccess.h"
#include <iostream>
#include <fstream>

using namespace inet;
using namespace commons;

Define_Module(UDPReliableApp);

simsignal_t UDPReliableApp::sentPkSignal = registerSignal("sentPk");
simsignal_t UDPReliableApp::rcvdPkSignal = registerSignal("rcvdPk");
simsignal_t UDPReliableApp::flowSizeSignal = registerSignal("flowSize");
simsignal_t UDPReliableApp::sleepDurationSignal = registerSignal("sleepDuration");

const static int TOT_HEADER_SIZE = 18 + 20 + 8; // Ethernet: 18; IPv4: 20; UDP: 8
const static int MIN_ETH_FRAME_SIZE = 64;
//const static int MAX_ETH_FRAME_SIZE = 450; // 760; // 1518;
const static int MAX_ETH_FRAME_SIZE = 1518;

UDPReliableApp::~UDPReliableApp()
{
    cancelAndDelete(timerNext);
    delete flowSizeVar;
}

void UDPReliableApp::initialize(int stage)
{
    ApplicationBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        numPktSent = 0;
        numPktReceived = 0;

        startTime = par("startTime").doubleValue() / (1000 * 1000); // Sec
        double stopTimeVal = par("stopTime").doubleValue();
        if (stopTimeVal) {
            stopTime =  ((double)stopTimeVal) / (1000 * 1000);
            canStop = true;
        } else {
            canStop = false;
        }

        sleepDurationPar = &par("sleepDuration");

        flowCount = par("flowCount");
        dataRate = par("dataRate").doubleValue(); // Mbps
        isSource = (strcmp(par("destAddress"), "") != 0);

        WATCH(numPktSent);
        WATCH(numPktReceived);
        WATCH(numAppNack);

        localPort = par("localPort");
        destPort = par("destPort");

        timerNext = new cMessage("UDPReliableAppTimer");

        if (isSource) {
            flowSizeVar = new EmpiricalRandomVariable(INTER_DISCRETE, 0);
            if (!flowSizeVar->loadCDF(par("flowSizeCDF"))) {
                throw cRuntimeError("Failed to load flowSize CDF file!");
            }

            sentFlowCount = 0;

            WATCH(sentFlowCount);
        } else {
            WATCH(rcvdFlowCount);
            checkOrder = par("checkOrder");
        }
    }
}

UDPReliableAppPacket* UDPReliableApp::createPacket(long size)
{
    char msgName[64];
    sprintf(msgName, "UDPReliableAppData-%d-%d-%d", getId(), flowSequence, pktSubSequence++);
    UDPReliableAppPacket* payload = new UDPReliableAppPacket(msgName);
    payload->setByteLength(size);
    payload->setFlowId(flowId);
    payload->setSequenceNumber(pktSequence);
    payload->setFlowSequenceNumber(flowSequence);
    payload->setSourceId(getId());
    payload->setFlowSize(0);
    payload->setFirstPkt(false);
    payload->setLastPkt(false);
    // HPCC specific
    payload->setNHop(0);
    payload->setPathId(0);

    return payload;
}

void UDPReliableApp::handleMessageWhenUp(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        switch (msg->getKind()) {
            case START:
                processStart();
                break;
            case NEW:
                processNew();
                break;
            case SEND:
                processSend();
                break;
            case STOP:
                processStop();
                break;
            case IDLE:
                processIdle();
                break;
            case SLEEP:
                processSleep();
                break;
            default:
                throw cRuntimeError("Invalid kind %d in self message", (int)msg->getKind());
        }
    } else if (dynamic_cast<UDPReliableAppPacketAck*>(msg)) {
        handlePacketAck((UDPReliableAppPacketAck*)msg);
    } else if (msg->getKind() == UDP_I_DATA) {
        // process incoming packet
        processPacket(msg);
    } else if (msg->getKind() == UDP_I_ERROR) {
        EV_WARN << "Ignoring UDP error report\n";
        delete msg;
    } else {
        throw cRuntimeError("Unrecognized message (%s)%s", msg->getClassName(), msg->getName());
    }
}

void UDPReliableApp::processStart()
{
    socket.setOutputGate(gate("udpOut"));
    socket.bind(localPort);

    if (isSource) {
        destAddr = L3AddressResolver().resolve(par("destAddress"));
        processNew();
    } else {
        std::remove((getFullPath() + ".fct").c_str()); // Remove previous results file
    }
}

void UDPReliableApp::processNew()
{
//    if (FlowAllocator::getInstance()->startFlow()) {
        flowSize = getFlowSize();
        if (flowSize <= 0) {
            throw cRuntimeError("The flowSize must be positive!");
        }
        emit(flowSizeSignal, flowSize);

        double flowDuration = (flowSize * 8) / (dataRate * 1000 * 1000);
        sendInterval = (flowSize <= MAX_ETH_FRAME_SIZE) ? flowDuration : (flowDuration / ceil(flowSize / (double)MAX_ETH_FRAME_SIZE));

        //flowId = FlowAllocator::getInstance()->getNewFlowId();
        //pktSequence = 0;
        newFlow = true;
        flowSequence++;
        pktSubSequence = 0;
        flowStartTimes << getId() << ":" << flowSequence << ":" << flowSize << ":" << simTime() << "\n";

        std::ofstream file(getFullPath() + ".fst");
        file << flowStartTimes.str();
        file.close();

        timerNext->setKind(SEND);

        processSend();
//    } else {
//        cancelEvent(timerNext);
//        processStop();
//    }
}

void UDPReliableApp::processSend()
{
    simtime_t now = simTime();

    if (nextPkt < now) {
        nextPkt = now;
    }

    if (lastAckRcvd && (MIN_ETH_FRAME_SIZE <= flowSize)) {
        // Generate traffic to match link throughput (not actual application throughput)
        long frameSize = flowSize < MAX_ETH_FRAME_SIZE ? flowSize : MAX_ETH_FRAME_SIZE;

        UDPReliableAppPacket* payload = createPacket(frameSize - TOT_HEADER_SIZE);
        if (newFlow) {
            payload->setFlowSize(flowSize);
            payload->setFirstPkt(true);
        }
        if (flowSize - frameSize < MIN_ETH_FRAME_SIZE) { // No next pkt
            payload->setLastPkt(true);
        }

        emit(sentPkSignal, payload);
        payload->setTimestamp();
        lastAckRcvd = false;
        socket.sendTo(payload, destAddr, destPort);
    }

    // Next timer
    nextPkt += sendInterval;

    if (canStop && (stopTime <= nextPkt)) {
        timerNext->setKind(STOP);
        nextPkt = std::max(stopTime, nextPkt);
    }

    scheduleAt(nextPkt, timerNext);
}

void UDPReliableApp::processStop()
{
    socket.close();

//    canStop = false;
//
//    simtime_t nextFlow = simTime() + 0.005;
//    timerNext->setKind(NEW);
//    scheduleAt(nextFlow, timerNext);
}

void UDPReliableApp::processSleep()
{
    double sleepDuration = sleepDurationPar->doubleValue() / 1000; // Sec
    if (sleepDuration < 0.0) {
        throw cRuntimeError("The sleepDuration mustn't be smaller than 0");
    }
    emit(sleepDurationSignal, sleepDuration * 1000);

    simtime_t nextFlow = simTime() + sleepDuration;
    timerNext->setKind(NEW);
    scheduleAt(nextFlow, timerNext);
}

void UDPReliableApp::processIdle()
{
    nextPkt += sendInterval;
    scheduleAt(nextPkt, timerNext);
}

void UDPReliableApp::handlePacketAck(UDPReliableAppPacketAck* ack)
{
    if (ack->getSourceId() != getId()) {
        throw cRuntimeError("Ack received by wrong application");
    }

    if (ack->getLastPktLeftQueue()) {
        if ((0 < flowCount) && (flowCount == sentFlowCount)) {
            timerNext->setKind(STOP);
        } else {
            timerNext->setKind(SLEEP);
        }
    } else {
        if (ack->getAck()) { // Acked
    //        ss << "sent: " << pktSequence << ", flowSize: " << flowSize << ", last: " << ack->getLastPkt() << endl;

            numPktSent++;
            pktSequence++;
            flowSize -= ack->getPacketSize() + TOT_HEADER_SIZE;

            if (ack->getFirstPkt()) {
                ASSERT(newFlow);
                newFlow = false;
            }

            if (ack->getLastPkt()) { // Last pkt of flow
                sentFlowCount++;
                if (timerNext->getKind() == SEND) {
                    timerNext->setKind(IDLE);
                }
            }
        } else {
            numAppNack++;
        }

        lastAckRcvd = true;
    }

    delete ack;
}

long UDPReliableApp::getFlowSize() const
{
    long flowSize = flowSizeVar->value(); // Bytes
    int maxSizedPkts = flowSize / MAX_ETH_FRAME_SIZE;
    long lastPktSize = flowSize - (maxSizedPkts * MAX_ETH_FRAME_SIZE);

    if (lastPktSize < MIN_ETH_FRAME_SIZE) {
        flowSize -= lastPktSize;
    }

    return flowSize;
}

void UDPReliableApp::refreshDisplay() const
{
    char buf[100];
    sprintf(buf, "rcvd: %d pks\nsent: %d pks", numPktReceived, numPktSent);
    getDisplayString().setTagArg("t", 0, buf);
}

void UDPReliableApp::processPacket(cMessage* msg)
{
    UDPReliableAppPacket* pkt = dynamic_cast<UDPReliableAppPacket*>(msg);
    if (!pkt) {
        throw cRuntimeError("UDPReliableAppPacket expected!");
    }

    EV_INFO << "Received packet: " << UDPSocket::getReceivedPacketInfo(pkt) << endl;
    emit(rcvdPkSignal, pkt);
    numPktReceived++;

    updateFlow(pkt);
    delete pkt;
}

void UDPReliableApp::updateFlow(UDPReliableAppPacket* pkt)
{
    int sourceId = pkt->getSourceId();
    Flow* flow;
    auto it = sourceFlow.find(sourceId);
    if (it != sourceFlow.end()) {
        flow = it->second;
    } else { // New flow
        flow = new Flow;
        flow->sourceId = sourceId;
        flow->rcvdFlowCount = 0;
        flow->nextPktSeq = 0;
        sourceFlow[sourceId] = flow;
    }

    if (pkt->getFirstPkt()) { // First pkt of flow
        if (pkt->getFlowSize() == 0) {
            throw cRuntimeError("First pkt received without flow size!");
        }
        flow->flowSize = pkt->getFlowSize();
        flow->flowSeq = pkt->getFlowSequenceNumber();
        flow->rcvdFlowSize = 0;
        //flow->nextPktSeq = 0;
    }

    if (checkOrder && pkt->getSequenceNumber() != flow->nextPktSeq) {
        throw cRuntimeError("OOO packet received!");
    }
    flow->rcvdFlowSize += pkt->getByteLength() + TOT_HEADER_SIZE;
    flow->nextPktSeq++;

    if (pkt->getLastPkt()) { // Last pkt of flow
        if (checkOrder && flow->rcvdFlowSize != flow->flowSize) {
            throw cRuntimeError("Flow received partially!");
        }
        flow->rcvdFlowCount++;
        onFlowCompletion(flow);
    }

//    ss << "source: " << sourceId << ", rcvd: " << pkt->getSequenceNumber() << ", last: " << pkt->getLastPkt() << endl;
}

void UDPReliableApp::onFlowCompletion(Flow* flow)
{
    flowCompTimes << flow->sourceId << ":" << flow->flowSeq << ":" << flow->flowSize << ":" << simTime() << "\n";

    rcvdFlowCount++;

    std::ofstream file(getFullPath() + ".fct");
    file << flowCompTimes.str();
    file.close();

    if (FlowAllocator::getInstance()->finishFlow() || (flowCount == rcvdFlowCount)) {
        endSimulation();
    }
}

void UDPReliableApp::finish()
{
//    std::ofstream myfile;
//    myfile.open(getFullPath() + ".app");
//    myfile << ss.str();
//    myfile.close();

    recordScalar("Total sent", numPktSent);
    recordScalar("Total received", numPktReceived);

    ApplicationBase::finish();
}

bool UDPReliableApp::handleNodeStart(IDoneCallback *doneCallback)
{
    simtime_t start = std::max(startTime, simTime());

    timerNext->setKind(START);
    scheduleAt(start, timerNext);

    return true;
}

bool UDPReliableApp::handleNodeShutdown(IDoneCallback *doneCallback)
{
    if (timerNext)
        cancelEvent(timerNext);
    //TODO if(socket.isOpened()) socket.close();
    return true;
}

void UDPReliableApp::handleNodeCrash()
{
    if (timerNext)
        cancelEvent(timerNext);
}

// ------------------------------------------------------ //

FlowAllocator* FlowAllocator::flowAllocator = new FlowAllocator; // nullptr;

FlowAllocator* FlowAllocator::getInstance()
{
//    if (!flowAllocator) {
//        flowAllocator = new FlowAllocator;
//    }
    return flowAllocator;
}

FlowAllocator::FlowAllocator():
//        remainingFlowCount(FLOW_COUNT),
        completedFlowCount(0)
{
    // Init flow id pool
//    srand(time(nullptr));
//    while (flowIdPool.size() != FLOW_COUNT * 2) {
//        flowIdPool.insert(rand());
//    }
}

//bool FlowAllocator::startFlow()
//{
//    bool status = false;
//
//    mtxRem.lock();
//    {
//        if (remainingFlowCount) {
//            remainingFlowCount--;
//            status = true;
//        }
//    }
//    mtxRem.unlock();
//
//    return status;
//}

bool FlowAllocator::finishFlow()
{
    bool status;

    mtxCom.lock();
    {
        completedFlowCount++;
        status = (FLOW_COUNT <= completedFlowCount);
    }
    mtxCom.unlock();

    return status;
}

uint32 FlowAllocator::getNewFlowId()
{
    uint32 flowId = 0;

    mtxCom.lock();
    {
        if (flowIdPool.empty()) {
            throw cRuntimeError("Flow id pool exhausted!");
        }
        flowId = *flowIdPool.begin();
        flowIdPool.erase(flowIdPool.begin());
    }
    mtxCom.unlock();

    return flowId;
}

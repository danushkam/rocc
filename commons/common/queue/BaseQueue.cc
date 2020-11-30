#include "BaseQueue.h"

using namespace inet;
using namespace commons;

Define_Module(BaseQueue);

void BaseQueue::initialize()
{
    PassiveQueueBase::initialize();

    // configuration
    queueCapacity = par("queueCapacity").longValue() * 1000; // Bytes

    queueSize = 0;

    queue.setName(par("queueName"));
    outGate = gate("out");

    WATCH(queueSize);
}

cMessage* BaseQueue::enqueue(cMessage* msg)
{
    if (isFull()) {
        EV << "Queue full, dropping packet.\n";
        return msg;
    } else {
        queue.insert(msg);
        processIngressMessage(msg);
        return nullptr;
    }
}

cMessage* BaseQueue::dequeue()
{
    if (queue.isEmpty())
        return nullptr;

    cMessage* msg = (cMessage*)queue.pop();
    processEgressMessage(msg);

    return msg;
}

void BaseQueue::processIngressMessage(const cMessage* msg)
{
    queueSize += dynamic_cast<const cPacket*>(msg)->getByteLength();
}

void BaseQueue::processEgressMessage(const cMessage* msg)
{
    queueSize -= dynamic_cast<const cPacket*>(msg)->getByteLength();
}

void BaseQueue::sendOut(cMessage* msg)
{
    send(msg, outGate);
}

bool BaseQueue::isEmpty()
{
    return queue.isEmpty();
}

bool BaseQueue::isFull()
{
    return (queueCapacity <= queueSize);
}


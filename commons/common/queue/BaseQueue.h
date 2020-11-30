#ifndef __BASE_QUEUE_H
#define __BASE_QUEUE_H

#include "inet/common/queue/PassiveQueueBase.h"

namespace commons {

class INET_API BaseQueue : public inet::PassiveQueueBase
{
  protected:
    // configuration
    int queueCapacity; // Bytes

    // state
    int queueSize; // Bytes

    cQueue queue;
    cGate* outGate;

  protected:
    virtual void initialize() override;

    virtual cMessage* enqueue(cMessage* msg) override;
    virtual cMessage* dequeue() override;

    virtual void processIngressMessage(const cMessage* msg);
    virtual void processEgressMessage(const cMessage* msg);

    virtual void sendOut(cMessage* msg) override;

    virtual bool isEmpty() override;
    virtual bool isFull();
};

} // namespace commons

#endif // ifndef __BASE_QUEUE_H


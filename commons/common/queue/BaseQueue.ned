package common.queue;

import inet.common.queue.IOutputQueue;

simple BaseQueue like IOutputQueue
{
    parameters:
        @class(commons::BaseQueue);
        int queueCapacity = default(75); // KB
        string queueName = default("l2queue"); // name of the inner cQueue object, used in the 'q' tag of the display string
        
        @display("i=block/queue");
        @signal[rcvdPk](type=cPacket);
        @signal[enqueuePk](type=cPacket);
        @signal[dequeuePk](type=cPacket);
        @signal[dropPkByQueue](type=cPacket);
        @signal[queueingTime](type=simtime_t; unit=s);
        @statistic[rcvdPk](title="received packets"; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[dropPk](title="dropped packets"; source=dropPkByQueue; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[queueingTime](title="queueing time"; record=histogram,vector; interpolationmode=none);
    gates:
        input in;
        output out;
}


//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

// Note (Danushka): It was required to use an extended version of EtherQoSQueue,
// 		 		    hence this class, a clone + extension of EtherQoSQueue.

package alibaba.hpcc.linklayer.ethernet;

import inet.common.queue.DropTailQueue;
import inet.common.queue.IOutputQueue;
import inet.common.queue.PriorityScheduler;
import inet.linklayer.ethernet.EtherFrameClassifier;

//
// Queue module that gives the PAUSE frames a higher priority,
// and can be parametrized with an ~IOutputQueue for serving the
// other frames.
//
module HostEtherQoSQueue like IOutputQueue
{
    parameters:
        @display("i=block/queue");
    gates:
        input in;
        output out;
        input oobL3In;
        output oobL3Out;
    submodules:
        classifier: EtherFrameClassifier {
            @display("p=46,145");
        }
        pauseQueue: DropTailQueue {
            queueName = "pauseQueue";
            @display("p=187,91");
        }
        dataQueue: HostEtherQueue {
            parameters:
                @display("p=187,192;q=l2queue");
        }
        scheduler: PriorityScheduler {
            @display("p=318,145");
        }
    connections:
        in --> classifier.in;
        classifier.pauseOut --> pauseQueue.in;
        classifier.defaultOut --> dataQueue.in;
        pauseQueue.out --> scheduler.in++;
        dataQueue.out --> scheduler.in++;
        scheduler.out --> out;
        oobL3In --> dataQueue.oobL3In;
        oobL3Out <-- dataQueue.oobL3Out;
}
#ifndef CONNPOOL_H
#define CONNPOOL_H 

#include "queue.h"
#include "window.h"

// Added DATA and ACK Wait Queues + Sender Window
typedef struct connUp {
    queue *dataQueue;
    queue *ackWaitQueue;
    sendWindow sw;
} connUp;

// Added GET, ACK & TIMEOUT queues + Receiver Winow
typedef struct connDown {
   // 0 = Ready, 1 = Waiting, 2 = Downloading
   int state; 
   int curChunkID;
   queue *getQueue;
   queue *timeoutQueue;
   queue *ackSendQueue;
   recvWindow rw;
} connDown;


#endif
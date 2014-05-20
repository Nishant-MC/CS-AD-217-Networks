#ifndef CONNPOOL_H
#define CONNPOOL_H 

#include "queue.h"
#include "window.h"
#include "sortedPacketCache.h"

// Added DATA and ACK Wait Queues + Sender Window
typedef struct connUp {
   sendWindow sw;
   struct timeval startTime;
   queue *dataQueue;
   queue *ackWaitQueue;
   uint32_t connID;
   uint8_t connected;
   uint8_t timeoutCount; // Assert a connection loss after 3 timeouts!
} connUp;

// Added GET, ACK & TIMEOUT queues + Receiver Winow
typedef struct connDown {
   // 0 = Ready, 1 = Waiting, 2 = Downloading
   recvWindow rw;
   int state; 
   int curChunkID;
   queue *getQueue;
   queue *timeoutQueue;
   queue *ackSendQueue;
   sortedPacketCache* cache;
   uint8_t connected;
   uint8_t timeoutCount;
} connDown;

void cleanUpConnUp(connUp *conn);
void cleanUpConnDown(connDown *conn);

#endif

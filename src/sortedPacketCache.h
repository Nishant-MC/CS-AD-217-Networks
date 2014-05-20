#ifndef _CACHE_H_
#define _CACHE_H_

#include "packet.h"
#include "queue.h"

typedef struct sortedPacketCache{
  int seq;
  Packet *pkt;
  struct sortedPacketCache* next;
} sortedPacketCache;

sortedPacketCache * newCache(Packet* pkt, int seq);
void insertInOrder(sortedPacketCache **head, Packet* pkt, int seq);
Packet * removeHead(sortedPacketCache **head);
int flushCache(int expected, queue* queue, sortedPacketCache **cache);
void clearCache(sortedPacketCache **cache);

#endif

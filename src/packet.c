#include "packet.h"

void freePacket(Packet *pkt)
{
    free(pkt);
}

int verifyPacket(Packet *pkt)
{
    uint16_t magic = getPacketMagic(pkt);
    uint8_t version = getPacketVersion(pkt);

    if(magic == 15441 && version == 1) {
        return 1;
    } else {
        printf("Invalid magic=%d, version=%d\n", magic, version);
        return 0;
    }
}

Packet *newPacketDefault()
{
    Packet *pkt = calloc(sizeof(Packet), 1);
    uint8_t *ptr = pkt->payload;
    *((uint16_t *)ptr) = 15441; //magic number
    *(ptr + 2) = 1; //version
    *((uint16_t *)(ptr + 4)) = 16; //Header size
    *((uint16_t *)(ptr + 6)) = 16; //Packet size
    return pkt;
}

void setPacketType(Packet *pkt, char *type)
{
    uint8_t *ptr = pkt->payload;
    uint8_t numType = 6;
    if(strcmp(type, "WHOHAS") == 0) {
        numType = 0;
    } else if(strcmp(type, "IHAVE") == 0) {
        numType = 1;
    } else if(strcmp(type, "GET") == 0) {
        numType = 2;
    } else if(strcmp(type, "DATA") == 0) {
        numType = 3;
    } else if(strcmp(type, "ACK") == 0) {
        numType = 4;
    } else if(strcmp(type, "DENIED") == 0) {
        numType = 5;
    }
    *(ptr + 3) = numType;
}
uint16_t getPacketSize(Packet *pkt)
{
    uint8_t *ptr = (pkt->payload);
    return *((uint16_t *)(ptr + 6));
}

uint16_t getPacketMagic(Packet *pkt)
{
    uint8_t *ptr = (pkt->payload);
    return *((uint16_t *)ptr);
}

uint8_t getPacketVersion(Packet *pkt)
{
    uint8_t *ptr = (pkt->payload);
    return *((uint8_t *)(ptr + 3));
}

uint8_t getPacketType(Packet *pkt)
{
   uint8_t *ptr = (pkt->payload);
   return *((uint8_t *)(ptr + 4));
}

void setPacketSize(Packet *pkt, uint16_t size)
{
    uint8_t *ptr = (pkt->payload);
    *((uint16_t *)(ptr + 6)) = size; //Packet size
}

void incPacketSize(Packet *pkt, uint16_t size)
{
    uint8_t *ptr = (pkt->payload);
    *((uint16_t *)(ptr + 6)) += size; //Packet size
}

uint32_t getPacketSeq(Packet *pkt)
{
    uint8_t *ptr = (pkt->payload);
    return *((uint32_t *)(ptr + 8));
}

uint32_t getPacketAck(Packet *pkt)
{
    uint8_t *ptr = (pkt->payload);
    return *((uint32_t *)(ptr + 12));
}

void setPacketSeq(Packet *pkt, uint32_t seqNo)
{
    uint8_t *ptr = (pkt->payload);
    *((uint32_t *)(ptr + 8)) = seqNo;
}

void setPacketAck(Packet *pkt, uint32_t ackNo)
{
    uint8_t *ptr = (pkt->payload);
    *((uint32_t *)(ptr + 12)) = ackNo;
}

void setPacketTime(Packet *pkt)
{
    gettimeofday(&(pkt->timestamp), NULL);
}

void newPacketWHOHAS(queue *sendQueue)
{
    int i, j;
    int pktIndex = 0;
    int numPacket = getChunk.numChunk / MAX_HASH_PER_PACKET;
    if(getChunk.numChunk % MAX_HASH_PER_PACKET > 0) {
        numPacket++;
    }

    for(i = 0; i < numPacket; i++) {
        int num;
        // uint8_t *ptr;
        Packet *thisObj = newPacketDefault();
        incPacketSize(thisObj, 4);
        setPacketType(thisObj, "WHOHAS");
        if(i < numPacket - 1) {
            num = MAX_HASH_PER_PACKET;
        } else {
            num = getChunk.numChunk % MAX_HASH_PER_PACKET;
        }
        for(j = 0; j < num; j++) {
            while(getChunk.list[pktIndex].fetchState) {
                pktIndex++;
            }
            insertPacketHash(thisObj, getChunk.list[pktIndex].hash);
	    pktIndex++;
        }
        enqueue(sendQueue, (void *)thisObj);
    }
}

void newPacketACK(Packet *pkt, queue *ackSendQueue)
{
    Packet *thisObj = newPacketDefault();
    setPacketType(thisObj, "ACK");
    setPacketAck(thisObj, getPacketSeq(pkt));
    enqueue(ackSendQueue, (void *)thisObj);
}

void newPacketDATA(Packet *pkt, queue *dataQueue)
{
    uint8_t *hash = pkt->payload + 16;
    int idx = searchHash(hash, &masterChunk, -1);
    Packet *newPkt;
    if(idx >= 0) {
        int i = 0;
        int numPacket = BT_CHUNK_SIZE / PACKET_DATA_SIZE;
        if(BT_CHUNK_SIZE % PACKET_DATA_SIZE > 0) {
            numPacket++;
        }
        for(i = 0; i < numPacket; i++) {
            if(i == numPacket - 1) {
                newPkt = newPacketSingleDATA(i + 1, idx, BT_CHUNK_SIZE % PACKET_DATA_SIZE);
            } else {
                newPkt = newPacketSingleDATA(i + 1, idx, PACKET_DATA_SIZE);
            }
            enqueue(dataQueue, newPkt);
        }
    }
}

Packet *newPacketSingleDATA(int seqNo, int seqChunk, size_t size)
{
    size_t retSize;
    long offset = seqChunk * BT_CHUNK_SIZE + (seqNo - 1) * PACKET_DATA_SIZE;
    Packet *pkt = newPacketDefault();

    setPacketType(pkt, "DATA");
    incPacketSize(pkt, size);
    setPacketSeq(pkt, seqNo);

    FILE *mf = masterChunk.filePtr;

    rewind(mf);
    fseek(mf, offset, SEEK_SET);
    retSize = fread(pkt->payload + 16, sizeof(uint8_t), size, mf);
    if(retSize != size) {
        printf("IO Error reading chunk\n");
        freePacket(pkt);
        return NULL;
    } else {
        //printf("DataOut %d [%ld-%ld]\n", seqNo, offset, offset + size);
        return pkt;
    }

}

void newPacketGET(Packet *pkt, queue *getQueue)
{
    uint8_t numHash = getPacketNumHash(pkt);
    int i, idx;
    uint8_t *hash;
    for(i = 0; i < numHash; i++) {
        hash = getPacketHash(pkt, i);
	printHash(hash);
        idx = searchHash(hash, &getChunk, 0);
        //Only GET when chunk hasn't been fetched
        if(getChunk.list[idx].fetchState == 0) {
            getChunk.list[idx].fetchState = 2;
	    Packet *thisObj = newPacketSingleGET(hash);
	    enqueue(getQueue, (void *)thisObj);
        }
    }
}

Packet *newPacketSingleGET(uint8_t *hash)
{
    Packet *thisObj = newPacketDefault();
    incPacketSize(thisObj, 20);
    setPacketType(thisObj, "GET");
    memcpy(thisObj->payload + 16, hash, SHA1_HASH_SIZE);
    return thisObj;
}


Packet *newPacketIHAVE(Packet *pktWHOHAS)
{
    uint8_t numHash = getPacketNumHash(pktWHOHAS);
    int i = 0;
    int idx;
    uint8_t *hash;
    Packet *thisObj = newPacketDefault();
    incPacketSize(thisObj, 4);
    setPacketType(thisObj, "IHAVE");
    for(i = 0; i < numHash; i++) {
        hash = getPacketHash(pktWHOHAS, i);
        idx = searchHash(hash, &hasChunk, -1);
        if(idx >= 0) {
            printf("Has[%d]", i);
            insertPacketHash(thisObj, hash);
        }
    }

    if(i == 0 || getPacketSize(thisObj) == 20) {
        freePacket(thisObj);
        return NULL;
    } else {
        return thisObj;
    }
}

uint8_t getPacketNumHash(Packet *pkt)
{
    uint8_t *ptr = (pkt->payload);
    return *((uint8_t *)(ptr + 16));
}

uint8_t *getPacketHash(Packet *pkt, int i)
{
    int type = getPacketType(pkt);
    if(type == 0 || type == 1 || type == 2) {
        // uint8_t *pPtr = (pkt->payload);
        return pkt->payload + 20 + SHA1_HASH_SIZE * i;
    } else {
        return NULL;
    }
}

void insertPacketHash(Packet *pkt, uint8_t *hash)
{
    uint8_t *ptr = pkt->payload;
    uint8_t numHash = *(ptr + 16);
    memcpy(ptr + 20 + numHash * SHA1_HASH_SIZE, hash, SHA1_HASH_SIZE);
    *(ptr + 16) = numHash + 1;//increment numHash
    incPacketSize(pkt, SHA1_HASH_SIZE);
}

/*
Packet *headerPacketfromPacket(Packet *pkt)
{
    Packet *newObj = malloc(sizeof(Packet));
    if(pkt->src == NULL) {
        newObj->src = NULL;
    } else {
        newObj->src = malloc(strlen(pkt->src) + 1);
        strcpy(newObj->src, pkt->src);
    }
    newObj->dest = NULL;
    newObj->srcPort = pkt->srcPort;
    newObj->destPort = 0;
    //Payload
    newObj->version = pkt->version;
    newObj->TTL = pkt->TTL;
    newObj->type = pkt->type;
    newObj->senderID = pkt->senderID;
    newObj->seqNo = pkt->seqNo;
    newObj->numLink = 0;
    newObj->numObj = 0;
    newObj->listLink = NULL;
    newObj->listObj = NULL;
    //Meta
    gettimeofday(&(newObj->timestamp), NULL);
    newObj->hasRetran = 0;
    newObj->isDown = 0;
    newObj->isExpired = 0;
    return newObj;
}
*/

/*
Packet *PacketfromPacket(Packet *pkt)
{
    int size = sizeof(uint32_t) * pkt->numLink;
    Packet *newObj = headerPacketfromPacket(pkt);
    newObj->numLink = pkt->numLink;
    newObj->numObj = pkt->numObj;
    newObj->listLink = malloc(size);
    memcpy(newObj->listLink, pkt->listLink, size);
    newObj->listObj = copyList(pkt->listObj);

    return newObj;
}
*/

Packet *newPacketFromBuffer(char *buf) 
{
    // unsigned int i;
    // char *ptr;
    // uint8_t version, TTL;

    Packet *newObj = malloc(sizeof(Packet));
    memcpy(newObj->payload, buf, 1500);
    // printPacket(newObj);
    return newObj;
}

/*
void printPacket(Packet *pkt)
{
    printf("==Packet== ");
    printf("src=%s:%d;dest=%s:%d;", pkt->src, pkt->srcPort, pkt->dest, pkt->destPort);
    printf("version=%d;TTL=%d;type=%d;", pkt->version, pkt->TTL, pkt->type);
    printf("senderID=%d;seqNo=%d;", pkt->senderID, pkt->seqNo);
    printf("numLink=%d;numObj=%d;", pkt->numLink, pkt->numObj);
    printf("hasRetran=%d;isDown=%d", pkt->hasRetran, pkt->isDown);

}
*/

/*
void PackettoBuffer(Packet *pkt, char *buffer, ssize_t *bufSize)
{
    unsigned int i;
    uint16_t val16;
    uint32_t val32;
    ssize_t packetSize;
    char *ptr;
    memcpy(buffer, &(pkt->version), 1);
    memcpy(buffer + 1, &(pkt->TTL), 1);
    val16 = htons(pkt->type);
    memcpy(buffer + 2, &val16, 2);
    val32 = htonl(pkt->senderID);
    memcpy(buffer + 4, &val32, 4);
    val32 = htonl(pkt->seqNo);
    memcpy(buffer + 8, &val32, 4);
    val32 = htonl(pkt->numLink);
    memcpy(buffer + 12, &val32, 4);
    val32 = htonl(pkt->numObj);
    memcpy(buffer + 16, &val32, 4);
    ptr = buffer + 20;
    for(i = 0; i < pkt->numLink; i++) {
        val32 = htonl(pkt->listLink[i]);
        memcpy(ptr + i * 4, &val32, 4);
    }
    ptr += i * 4;

    for(i = 0; i < pkt->numObj; i++) {
        char *objPtr = getNodeDataAt(pkt->listObj, i);
        strcpy(ptr, objPtr);
        ptr += strlen(objPtr) + 1;;
    }
    printPacket(pkt);
    packetSize = ptr - buffer;
    if(packetSize > *bufSize) {
        printf("Packet too big, %zu / %zu\n", packetSize, *bufSize);
        *bufSize = -1;
    } else {
        printf("Size: %zu bytes\n", packetSize);
        *bufSize = packetSize;
    }
}
*/

/*
void insertPacketLink(Packet *pkt, uint32_t nodeID)
{
    uint32_t numLink = pkt->numLink;
    pkt->listLink = realloc(pkt->listLink, sizeof(uint32_t) * (numLink + 1));
    pkt->listLink[numLink] = nodeID;
    pkt->numLink++;
}
*/

/*
void insertPacketObj(Packet *pkt, char *objName)
{
    if(pkt->numObj == 0) {
        pkt->listObj = malloc(sizeof(DLL));
        initList(pkt->listObj, compareString, freeString, NULL, copyString);
    }

    insertNode(pkt->listObj, pkt->listObj->copyData(objName));
    pkt->numObj++;

}
*/

int searchHash(uint8_t *hash, chunkList *chunkPool, int fetchState)
{
    int i;
    for(i = 0; i < chunkPool->numChunk; i++) {
        chunkLine *line = &(chunkPool->list[i]);
        int matched = sameHash(line->hash, hash, SHA1_HASH_SIZE);
        if(fetchState >= 0) {
            matched = matched && line->fetchState == fetchState;
        }
        if(matched) {
            return i;
        }
    }
    return -1;
}

int sameHash(uint8_t *hash1, uint8_t *hash2, int size)
{
    int i;
    for(i = 0; i < size; i++) {
        if(hash1[i] != hash2[i]) {
            return 0;
        }
    }
    return 1;
}

void printHash(uint8_t *hash)
{
    char buf[50];
    bzero(buf, 50);
    binary2hex(hash, 20, buf);
    printf("%s\n", buf);
}
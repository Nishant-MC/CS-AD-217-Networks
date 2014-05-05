#include "peer.h"

void peer_run(bt_config_t *config);

int main(int argc, char **argv)
{

    bt_config_t config;

    bt_init(&config, argc, argv);

    DPRINTF(DEBUG_INIT, "peer.c main beginning\n");

    config.identity = 1; // your group number here
    strcpy(config.chunk_file, "chunkfile");
    strcpy(config.has_chunk_file, "haschunks");

    bt_parse_command_line(&config);

    bt_dump_config(&config);

    init(&config);
    peer_run(&config);
    return 0;
}

void init(bt_config_t *config)
{

    int i;
    fillChunkList(&masterChunk, MASTER, config->chunk_file);
    fillChunkList(&hasChunk, HAS, config->has_chunk_file);

    fillPeerList(config);

    maxConn = config->max_conn;
    nonCongestQueue = newqueue();
    for(i = 0; i < peerInfo.numPeer; i++) {
        int peerID = peerInfo.peerList[i].peerID;
        uploadPool[peerID].dataQueue = newqueue();
        uploadPool[peerID].ackWaitQueue = newqueue();
        downloadPool[peerID].getQueue = newqueue();
        downloadPool[peerID].timeoutQueue = newqueue();
        downloadPool[peerID].ackSendQueue = newqueue();

        initSendWindow(&(uploadPool[peerID].sw));
        initRecvWindow(&(downloadPool[peerID].rw));
    }
    printInit();

}

void printInit()
{
    printChunk(&masterChunk);
    printChunk(&hasChunk);
}

void printChunk(chunkList *list)
{
    int i;
    printf("ListType: %d\n", list->type);
    for(i = 0; i < list->numChunk; i++) {
        char buf[50];
        bzero(buf, 50);
        chunkLine *line = &(list->list[i]);
        binary2hex(line->hash, 20, buf);
        printf("%d: %s\n", line->seq, buf);
    }
}

void fillChunkList(chunkList *list, enum chunkType type, char *filename)
{
    FILE *fPtr = NULL;
    char lineBuf[MAX_LINE_SIZE];
    char *linePtr;
    int numChunk = 0;
    int chunkIdx = 0;
    bzero(list, sizeof(list));
    list->type = type;

    switch (type) {
    case MASTER:
        if((fPtr = fopen(filename, "r")) == NULL) {
            printf("Open file %s failed\n", filename);
            exit(1);
        }
        fgets(lineBuf, MAX_LINE_SIZE, fPtr);
        if(strncmp(lineBuf, "File: ", 6) != 0) {
            printf("Error parsing masterchunks\n");
            exit(1);
        } else {
            FILE *masterFile;

            char *newline = strrchr(lineBuf, '\n');
            if(newline) {
                *newline = '\0';
            }

            linePtr = &(lineBuf[6]);
            if((masterFile = fopen(linePtr, "r")) == NULL) {
                printf("Error opening master data file: <%s>\n", linePtr);
                exit(1);
            }
            list->filePtr = masterFile;
        }
        //Skip "Chunks:" line
        fgets(lineBuf, MAX_LINE_SIZE, fPtr);
    
    case GET:
        list->getChunkFile=calloc(strlen(filename)+1,1);
        strcpy(list->getChunkFile, filename);   
 
    case HAS:
        if(fPtr == NULL) {
            if((fPtr = fopen(filename, "r")) == NULL) {
                fprintf(stderr, "Open file %s failed\n", filename);
                exit(-1);
            }
        }
        while(!feof(fPtr)) {
            char *hashBuf;
            if(fgets(lineBuf, MAX_LINE_SIZE, fPtr) == NULL) {
                break;
            }
            if(2 != sscanf(lineBuf, "%d %ms", &chunkIdx, &hashBuf)) {
                printf("Error parsing hash\n");
                exit(1);
            }
            chunkLine *cPtr = &(list->list[numChunk]);
            cPtr->seq = chunkIdx;
            hex2binary(hashBuf, 2 * SHA1_HASH_SIZE, cPtr->hash);
            free(hashBuf);
            numChunk++;
        }
        list->numChunk = numChunk;
        break;
    
    default:
        printf("WTF!!\n");
        exit(1);
    }
    fclose(fPtr);
}


void fillPeerList(bt_config_t *config)
{
    bt_peer_t *peer = config->peers;
    peerList_t *peerList = peerInfo.peerList;
    int numPeer = 0;
    while(peer != NULL) {
        if(peer->id == config->identity) {
            peerList[numPeer].isMe = 1;
        } else {
            peerList[numPeer].isMe = 0;

        }
        peerList[numPeer].peerID = peer->id;
        memcpy(&(peerList[numPeer].addr), &(peer->addr), sizeof(struct sockaddr_in));
        peer = peer->next;
        numPeer++;
    }
    peerInfo.numPeer = numPeer;
}

void handlePacket(Packet *pkt)
{
    if(verifyPacket(pkt)) {
        int type = getPacketType(pkt);
        switch(type) {
        case 0: { //WHOHAS
            printf("->WHOHAS\n");
            Packet *pktIHAVE = newPacketIHAVE(pkt);
            enqueue(nonCongestQueue, (void *)pktIHAVE);
            break;
        }
        case 1: { //IHAVE
            printf("->IHAVE\n");
            int peerIndex = searchPeer(&(pkt->src));
            int peerID = peerInfo.peerList[peerIndex].peerID;
            newPacketGET(pkt, downloadPool[peerID].getQueue);
            break;
        }
        
	case 2: { //GET
            printf("->GET\n");
            int peerIndex = searchPeer(&(pkt->src));
            int peerID = peerInfo.peerList[peerIndex].peerID;
            newPacketDATA(pkt, uploadPool[peerID].dataQueue);

            break;
        }
        
	case 3: { //DATA
            printf("->DATA");
            int peerIndex = searchPeer(&(pkt->src));
            int peerID = peerInfo.peerList[peerIndex].peerID;
            newPacketACK(pkt, downloadPool[peerID].ackSendQueue);
            if(1 == updateGetSingleChunk(pkt, peerID)) {
                updateGetChunk();
            }
            break;
        }

        case 4: { //ACK
            printf("->ACK\n");
            int peerIndex = searchPeer(&(pkt->src));
            int peerID = peerInfo.peerList[peerIndex].peerID;
            updateUploadPool(pkt, peerID);
            break;
        }

        case 5://DENIED not used
        default:
            printf("Type=WTF\n");
        }
    } else {
        printf("Invalid packet\n");
        return;
    }
    freePacket(pkt);
    return;
}

void updateUploadPool(Packet *pkt, int peerID)
{
    uint32_t ack = getPacketAck(pkt);
    queue *ackWaitQueue = uploadPool[peerID].ackWaitQueue;
    Packet *ackWait = dequeue(ackWaitQueue);
    if(ackWait != NULL && ack == getPacketSeq(ackWait)) {
        freePacket(ackWait);
    } else {
        printf("Got ACK for something didn't send?\n");
    }
}

int updateGetSingleChunk(Packet *pkt, int peerID)
{
    int dataSize = getPacketSize(pkt) - 16;
    uint8_t *dataPtr = pkt->payload + 16;
    int seq = getPacketSeq(pkt);
    int curChunk = downloadPool[peerID].curChunkID;
    long offset = (seq - 1) * PACKET_DATA_SIZE + BT_CHUNK_SIZE * curChunk;
    FILE *of = getChunk.filePtr;
    printf("DataIn %d [%ld-%ld]\n", seq, offset, offset + dataSize);
    fseek(of, offset, SEEK_SET);
    fwrite(dataPtr, sizeof(uint8_t), dataSize, of);

    /*Check if this GET finished */
    //This is a hack, should be change in CP2
    if(seq == BT_CHUNK_SIZE / PACKET_DATA_SIZE + 1) {
        getChunk.list[curChunk].fetchState = 1;
        downloadPool[peerID].state = 0;
        Packet *clearPkt = dequeue(downloadPool[peerID].timeoutQueue);
        while(clearPkt != NULL) {
            freePacket(clearPkt);
            clearPkt = dequeue(downloadPool[peerID].timeoutQueue);
        }
        printf("Chunk %d fetched\n", curChunk);
        printf("%d More GETs in queue\n", downloadPool[peerID].getQueue->size);
        return 1;//this GET is done
    } else {
        return 0;
    }

}

/* Check if all GETs are done */
void updateGetChunk()
{
    int i = 0;
    int done = 1;
    for(i = 0; i < getChunk.numChunk; i++) {
        if(getChunk.list[i].fetchState != 1) {
            done = 0;
            printf("Still missing chunk %d\n", i);
        }
    }
    if(done) {
        fclose(getChunk.filePtr);
        printf("GOT %s\n", getChunk.getChunkFile);
        free(getChunk.getChunkFile);
        bzero(&getChunk, sizeof(getChunk));
        idle = 1;
    }
}

int searchPeer(struct sockaddr_in *src)
{
    int i = 0;
    for(i = 0; i < peerInfo.numPeer; i++) {
        struct sockaddr_in *entry = &(peerInfo.peerList[i].addr);
        //Compare sin_port & sin_addr.s_addr
        int isSame = entry->sin_port == src->sin_port &&
                     entry->sin_addr.s_addr == src->sin_addr.s_addr;
        if(isSame) {
            return i;
        }
    }
    return -1;
}

void process_inbound_udp(int sock)
{
#define BUFLEN 1500
    struct sockaddr_in from;
    socklen_t fromlen;
    char buf[BUFLEN];

    fromlen = sizeof(from);
    spiffy_recvfrom(sock, buf, BUFLEN, 0, (struct sockaddr *) &from, &fromlen);

    /*
    printf("Incoming message from %s:%d\n",
           inet_ntoa(from.sin_addr),
           ntohs(from.sin_port));
    */

    Packet *newPkt = newPacketFromBuffer(buf);
    memcpy(&(newPkt->src), &from, fromlen);
    handlePacket(newPkt);

}


void process_get(char *chunkfile, char *outputfile)
{
    printf("Handle GET (%s, %s)\n", chunkfile, outputfile);

    fillChunkList(&getChunk, GET, chunkfile);
    if((getChunk.filePtr = fopen(outputfile, "w")) == NULL) {
        fprintf(stderr, "Open file %s failed\n", outputfile);
        exit(-1);
    }
    //TODO:only get chunks that I don't have
    printChunk(&getChunk);
    newPacketWHOHAS(nonCongestQueue);

}

void handle_user_input(char *line, void *cbdata)
{
    char chunkf[128], outf[128];
    cbdata = cbdata;
    bzero(chunkf, sizeof(chunkf));
    bzero(outf, sizeof(outf));

    if (sscanf(line, "GET %120s %120s", chunkf, outf)) {
        if (strlen(outf) > 0) {
            process_get(chunkf, outf);
            idle = 0;
        }
    }
}

void peer_run(bt_config_t *config)
{
    int sock;
    struct sockaddr_in myaddr;
    fd_set readfds;
    struct user_iobuf *userbuf;

    if ((userbuf = create_userbuf()) == NULL) {
        perror("peer_run could not allocate userbuf");
        exit(-1);
    }

    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) == -1) {
        perror("peer_run could not create socket");
        exit(-1);
    }

    bzero(&myaddr, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = htons(config->myport);

    if (bind(sock, (struct sockaddr *) &myaddr, sizeof(myaddr)) == -1) {
        perror("peer_run could not bind socket");
        exit(-1);
    }

    spiffy_init(config->identity, (struct sockaddr *)&myaddr, sizeof(myaddr));
    struct timeval timeout;
    while (1) {
        int nfds;
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sock, &readfds);

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        nfds = select(sock + 1, &readfds, NULL, NULL, &timeout);

        if (nfds > 0) {
            if (FD_ISSET(sock, &readfds)) {
                process_inbound_udp(sock);
            }

            if (FD_ISSET(STDIN_FILENO, &readfds) && idle == 1) {
                process_user_input(STDIN_FILENO,
                                   userbuf,
                                   handle_user_input,
                                   "Currently unused");
            }
        }

        flushQueue(sock, nonCongestQueue);
        flushUpload(sock);
        flushDownload(sock);
    }
}


void flushUpload(int sock)
{
    int i = 0;
    Packet *pkt;
    connUp *pool = uploadPool;
    for(i = 0; i < peerInfo.numPeer; i++) {
        int peerID = peerInfo.peerList[i].peerID;
        Packet *ack = dequeue(pool[peerID].ackWaitQueue);
        if(ack == NULL) { //Send DATA at a time
            pkt = dequeue(pool[peerID].dataQueue);
            if(pkt != NULL) {
                peerList_t *p = &(peerInfo.peerList[i]);
                int retVal = spiffy_sendto(sock,
                                           pkt->payload,
                                           getPacketSize(pkt),
                                           0,
                                           (struct sockaddr *) & (p->addr),
                                           sizeof(p->addr));
                if(retVal == -1) {
                    printf("Data lost\n");
                }
                enqueue(pool[peerID].ackWaitQueue, pkt);
            }
        } else { //Wait if there is outstanding ACK
            enqueue(pool[peerID].ackWaitQueue, ack);
        }
    }
}

void flushDownload(int sock)
{
    int i = 0;
    Packet *pkt;
    connDown *pool = downloadPool;
    for(i = 0; i < peerInfo.numPeer; i++) {
        int peerID = peerInfo.peerList[i].peerID;
        /* Send ACK */
        Packet *ack = dequeue(pool[peerID].ackSendQueue);
        if(ack != NULL) {
            peerList_t *p = &(peerInfo.peerList[i]);
            int retVal = spiffy_sendto(sock,
                                       ack->payload,
                                       getPacketSize(ack),
                                       0,
                                       (struct sockaddr *) & (p->addr),
                                       sizeof(p->addr));
            if(retVal == -1) {
                printf("Sending ACK failed\n");
            }
            freePacket(ack);
        }
        /* Send GET */
        switch(pool[peerID].state) {
        case 0://Ready for next
            pkt = dequeue(pool[peerID].getQueue);
            if(pkt != NULL) {
                printf("Sending a GET\n");
                peerList_t *p = &(peerInfo.peerList[i]);
                uint8_t *hash = pkt->payload + 16;
                char buf[50];
                bzero(buf, 50);
                binary2hex(hash, 20, buf);
                printf("GET hash:%s\n", buf);
                pool[peerID].curChunkID = searchHash(hash, &getChunk, -1);
                //Send get
                int retVal = spiffy_sendto(sock,
                                           pkt->payload,
                                           getPacketSize(pkt),
                                           0,
                                           (struct sockaddr *) & (p->addr),
                                           sizeof(p->addr));

                if(retVal == -1) {
                    Packet *pktIHAVE = newPacketIHAVE(pkt);
                    enqueue(nonCongestQueue, (void *)pktIHAVE);
                    freePacket(pkt);
                    return;
                }
                //Mark time
                setPacketTime(pkt);
                //Put it in timeoutQueue
                enqueue(pool[peerID].timeoutQueue, pkt);
                pool[peerID].state = 1;
            }
            break;
        case 1: {//Waiting
            pkt = dequeue(pool[peerID].timeoutQueue);
            struct timeval curTime;
            gettimeofday(&curTime, NULL);
            long dt = diffTimeval(&curTime, &(pkt->timestamp));
            if(dt > GET_TIMEOUT_SEC) {
                Packet *pktIHAVE = newPacketIHAVE(pkt);
                enqueue(nonCongestQueue, (void *)pktIHAVE);
                freePacket(pkt);
            } else {
                enqueue(pool[peerID].timeoutQueue, pkt);
            }
            break;
        }
        case 2: {
            break;
        }
        default:
            break;
        }

    }
}


void flushQueue(int sock, queue *sendQueue)
{
    int i = 0;
    int retVal = -1;
    Packet *pkt = dequeue(sendQueue);
    if(pkt == NULL) {
        return;
    }
    peerList_t *list = peerInfo.peerList;
    while(pkt != NULL) {
        printf("Sending Packet\n");
        for(i = 0; i < peerInfo.numPeer; i++) {
            if(list[i].isMe == 0) {
                retVal = spiffy_sendto(sock,
                                       pkt->payload,
                                       getPacketSize(pkt),
                                       0,
                                       (struct sockaddr *) & (list[i].addr),
                                       sizeof(list[i].addr));
                if(retVal == -1) {
                    break;
                }
            }
        }

        if(retVal == -1) {
            break;
        }
        freePacket(pkt);
        pkt = dequeue(sendQueue);
    }

    if(retVal == -1) {
        if(errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
            printf("Error EAGAIN\n");
            enqueue(sendQueue, (void *)pkt);
        } else {
            printf("Fail sending\n");
            close(sock);
        }
    } else {
        printf("All packets flushed\n");
    }

}

long diffTimeval(struct timeval *t1, struct timeval *t2)
{
    return t1->tv_sec - t2->tv_sec;
}
/*
 * Utility.h
 *
 *  Created on: Dec 3, 2015
 *      Author: hero
 */

#ifndef UTILITY_H_
#define UTILITY_H_

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <string>
#include <errno.h>
#include <map>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fstream>
#include <vector>
#include <sstream>
#include <ctime>
using namespace std;
/* Ack-only packets are only 8 bytes */
struct ack_packet {
    uint16_t cksum; /* Optional bonus part */
    uint16_t len;
    uint32_t ackno;
};

struct packet {
    /* Header */
    uint16_t cksum; /* Optional bonus part */
    uint16_t len;
    uint32_t seqno;
    /* Data */
    char data[500]; /* Not always 500 bytes, can be less */
};


/* External error handling function */
void DieWithError(string errorMessage);
void set_failure_rate(double new_rate);
double get_failure_rate();
void set_seed(int seed);
double get_seed();

int send_udp(int socket, const void *msg, unsigned int msgLength, struct sockaddr_in & destAddr);
bool validateCheckSum(ack_packet ack);
uint16_t setCheckSum(void* file_pck,bool from_client);

#endif /* UTILITY_H_ */










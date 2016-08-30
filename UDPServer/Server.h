/*
 * Server.h
 *
 *  Created on: Dec 3, 2015
 *      Author: hero
 */

#ifndef SERVER_H_
#define SERVER_H_


#include "Utility.h"
#include <errno.h>
#include <signal.h>
#include <pthread.h>

void start_server(int port,int window,int seed,int loss_probability);
int send_to_client(sockaddr_in cli,int sock,packet pck,int seqno,bool ignore_timeout);
void* send_file(sockaddr_in cli,packet pck);


#endif /* SERVER_H_ */

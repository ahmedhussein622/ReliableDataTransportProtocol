/*
 * Client.h
 *
 *  Created on: Dec 3, 2015
 *      Author: hero
 */

#ifndef CLIENT_H_
#define CLIENT_H_

#include "Utility.h"
#include <errno.h>
#include <signal.h>
double start_client(int port,string server_ip, string file_name,int rcv_w);
void receive_first_packet(int sock,string file_name);
void start_rcv_file(int sock);


#endif /* CLIENT_H_ */

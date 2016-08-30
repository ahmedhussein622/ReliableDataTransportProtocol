#include "Utility.h"



/* External error handling function */
void DieWithError(string errorMessage) {
	perror (errorMessage.c_str());
	exit(1);
}


double failure_rate = 0;
void set_failure_rate(double new_rate) {
	failure_rate = new_rate;
}
double get_failure_rate() {
	return failure_rate;
}

int send_udp(int socket, const void *msg, unsigned int msgLength, struct sockaddr_in & destAddr) {

	double r = rand() / (double) RAND_MAX;
	if(r <= failure_rate) {
		printf("sending udp failed %f\n",r);
		return msgLength;
	}

	return sendto(socket, msg, msgLength, 0,(struct sockaddr *)& destAddr, sizeof(destAddr));
}









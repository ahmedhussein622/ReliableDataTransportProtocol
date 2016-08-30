#include "Utility.h"



/* External error handling function */
void DieWithError(string errorMessage) {
	perror (errorMessage.c_str());
	exit(1);
}

int count_pckts=0;
int count_losses=0;
double failure_rate = 0;
double seed=0;
void set_failure_rate(double new_rate) {
	failure_rate = new_rate;
}
void set_seed(int seed){
//srand(seed);
}
double get_failure_rate() {
	return failure_rate;
}

int send_udp(int socket, const void *msg, unsigned int msgLength, struct sockaddr_in & destAddr) {
    count_pckts++;
	int prob=(rand()%(100)+1);


	if(prob > 100-failure_rate) {
	count_losses++;
		//printf("sending udp failed %d total losses:%d\n",prob,count_losses);
		return msgLength;
	}

	return sendto(socket, msg, msgLength, 0,(struct sockaddr *)& destAddr, sizeof(destAddr));
}









#include "Server.h"
#include "Utility.h"
std::ifstream file_temp;
int main(int argc, char *argv[]) {

    std::string server_port;
    std::string send_window;
    std::string random_seed;
    std::string loss_probability; //from 0% to 100%


    file_temp.open("server.in");

	std::getline(file_temp,server_port);
	std::getline(file_temp,send_window);//not needed
	std::getline(file_temp,random_seed);
	std::getline(file_temp,loss_probability);

    start_server(atoi(server_port.c_str()),atoi(send_window.c_str()),atoi(random_seed.c_str()),atoi(loss_probability.c_str()));

	return 0;
}



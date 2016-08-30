#include "Client.h"
#include "Utility.h"
std::ifstream file;



int main(int argc, char *argv[]) {
    std::string server_ip;
    std::string server_port;
    std::string client_port;
    std::string file_name;
    std::string receive_window;

    file.open("client.in");

	std::getline(file,server_ip);
	std::getline(file,server_port);
	std::getline(file,client_port);//not needed
	std::getline(file,file_name);
	std::getline(file,receive_window);

  //  start_client(atoi(server_port.c_str()), server_ip, file_name, atoi(receive_window.c_str()));

    // start test
    int n = 1;
    double times[n];
    double average = 0;
    for(int i = 0; i < n; i++) {
        printf("----------starting test %d--------\n", i);
        times[i] = start_client(atoi(server_port.c_str()), server_ip, file_name, atoi(receive_window.c_str()));
        average += times[i];
        sleep(2);
    }
    average /= n;

    printf("test result for %d times is  average : %f\n",n, average);
    for(int i = 0;i < n; i++) {
        printf("%f ", times[i]);
    }
    printf("\n");
    //end of test


	return 0;
}

#include "Client.h"
#include <ctime>
#define ECHOMAX 512

#define TIMEOUT_SECS 2

struct sockaddr_in echoServAddr; /* Echo server address */
ofstream myfile;
string requestString;
int count=0;

void CatchAlarm(int ignored)
{

}

bool validateCheckSum(packet file_pck)
{
    packet new_pck;

    memset(&new_pck,0,sizeof(packet));
    memcpy(new_pck.data,file_pck.data,500);

    new_pck.len=file_pck.len;
    new_pck.seqno=file_pck.seqno;

//    printf("packet length: %d checksum :%" PRIu16 "seqno :%d validated checksum :%" PRIu16 "equal:%d\n",file_pck.len,file_pck.cksum,file_pck.seqno, setCheckSum(&new_pck,false) ,setCheckSum(&new_pck,false)==file_pck.cksum);

    return setCheckSum(&new_pck,false)==file_pck.cksum;
}
uint16_t setCheckSum(void * file_pck,bool from_client)
{
    //return 0;
    int cnt;
    if(from_client)
    {
        cnt=sizeof(ack_packet);
    }

    else
    {
        cnt=sizeof(packet);
    }

    uint16_t  * addr=(uint16_t *) file_pck;
    register long sum = 0;

    while( cnt > 1 )
    {
        /*  This is the inner loop */
        sum += (uint16_t) *addr++;
        cnt -= 2;
    }

    /*  Add left-over byte, if any */
    if( cnt > 0 )
        sum += * (unsigned char *) addr;

    /*  Fold 32-bit sum to 16 bits */
    while (sum>>16)
        sum = (sum & 0xffff) + (sum >> 16);

    uint16_t checksum;
    checksum = ~sum;

    return checksum;
}


double time_receive=0.0;
double start_client(int port,string server_ip, string file_name,int receive_window)
{
    time_receive=0.0;
    printf("starting client of stop and wait\n");
    int sock;/* Socket descriptor */
    struct sockaddr_in fromAddr;/* Source address of echo */
    unsigned short echoServPort  = port; /*Echo server port */
    unsigned int fromSize; /*In-out of address size for recvfrom() */
    string servIP = server_ip ;	/*IP address of server */
    requestString = file_name; /*String to send to echo server */
    char echoBuffer[ECHOMAX]; /*Buffer for receiving ack of request */
    int respStringLen; /* Length of received response */
    struct sigaction myAction;
    /* Create a datagram/UDP socket */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        DieWithError( "socket () failed") ;
    }
    myAction.sa_handler = CatchAlarm;
    if (sigfillset(&myAction.sa_mask) < 0) /* block everything in handler */
        DieWithError( "sigfillset () failed") ;
    myAction.sa_flags = 0;
    if (sigaction(SIGALRM, &myAction, 0) < 0)
        DieWithError("sigaction() failed for SIGALP~") ;
    /* Construct the server address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr)); /* Zero out structure */
    echoServAddr.sin_family = AF_INET; /* Internet addr family */
    echoServAddr.sin_addr.s_addr = inet_addr(servIP.c_str()); /* Server IP address */
    echoServAddr.sin_port = htons(echoServPort); /* Server port */
    /* Send the request to the server */

    receive_first_packet(sock,file_name);
    close(sock);
    return time_receive;
}
void receive_first_packet(int sock, string requestString)
{
    //Send eqquest
    packet pck;
    memset(&pck,0,sizeof(packet));
    struct sockaddr_in fromAddr;/* Source address of echo */
    for(int i=0; i<requestString.length(); i++)
    {
        pck.data[i]=requestString[i];
    }
    pck.data[requestString.length()]='\0';
    pck.len=requestString.length()+1;
    printf("requested file: %s length: %d\n",pck.data,pck.len);
    pck.seqno=0;
    pck.cksum=setCheckSum(&pck,false);
    int seqno=0;
    int x = send_udp(sock, &pck, sizeof(pck), echoServAddr);
    /* Recv a response */
    unsigned int fromSize = sizeof(echoServAddr) ;
    int recvd_seq=!seqno;
    int respStringLen=0;
    /* Recv a response */
    packet pck_file;

        alarm(TIMEOUT_SECS);
        while((respStringLen = recvfrom(sock, &pck_file, sizeof(pck_file), 0,(struct sockaddr *) &echoServAddr, &fromSize)) <0)
        {
            if (errno==EINTR)
            {
                x = send_udp(sock, &pck, sizeof(pck), echoServAddr);
                alarm(TIMEOUT_SECS);
                printf("client timed out can't find server!\n");
            }
            else
            {
                DieWithError("recvfrom() failed");
            }
        }

    alarm(0);
    start_rcv_file(sock);
}
void start_rcv_file(int sock)
{
    printf("received 1st packet\n");

    int seqno=0;
    unsigned int fromSize=sizeof(echoServAddr);
    int respStringLen;
    vector <packet> buffer;
    clock_t before = clock();
    while(true)
    {
        packet pck_file;
        ack_packet ack;
        ack.len=0;
        memset(&ack,0,sizeof(ack_packet));
        memset(&pck_file,0,sizeof(packet));
        if((respStringLen = recvfrom(sock, &pck_file, sizeof(pck_file), 0,
                                     (struct sockaddr *) &echoServAddr, &fromSize)) <0)
        {
            continue;
        }
        if(pck_file.len==0)
        {
            ack.cksum=setCheckSum(&ack,true);
            send_udp(sock,&ack,sizeof(ack),echoServAddr);
            break;
        }
        else if(pck_file.seqno!=seqno || !validateCheckSum(pck_file))
        {
            ack.ackno=!seqno;
        }
        else
        {
            ack.ackno=seqno;
            seqno=!seqno;
            buffer.push_back(pck_file);
        }
        ack.cksum=setCheckSum(&ack,true);
        send_udp(sock,&ack,sizeof(ack),echoServAddr);
        printf("sent ack \n");
    }
    clock_t after = clock();
    time_receive = (after- before) / (double) CLOCKS_PER_SEC * 1000;
    myfile.open(requestString.c_str());
 printf("file recived \n");
    printf("# of packets :%d",buffer.size());
    int cnt=0;
    for(int i=0; i<buffer.size(); i++)
        for(int j=0; j<buffer[i].len; j++)
        {
            myfile << buffer[i].data[j];
            cnt++;
        }
    printf("bytes read: %d\n",cnt);
    printf("file written\n");
    myfile.close();
}

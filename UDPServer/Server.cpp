#include "Server.h"
#define ECHOMAX 512 /* Longest string to echo */
#define TIMEOUT_SECS 2

struct thread_arg
{
    sockaddr_in cli;
    packet pck_file;
};

int tries=0;
vector<sockaddr_in> serving;
pthread_mutex_t mutexsum;
void *send_file(void * args)
{

    thread_arg* arg=(thread_arg* )args;
    sockaddr_in cli=arg->cli;
    packet pck=arg->pck_file;
    //printf("hello from thread\n");
    printf("starting thread client requested: %s\n",pck.data);
    int sock;
    int seqno=0;
    struct sockaddr_in echoServAddr; /* local address */
    /* Create socket for sending/receiving datagrams */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        DieWithError("socket() failed");
    }
    struct timeval tv;
    tv.tv_usec = 200;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0)
    {
        perror("Error");
    }
    string file_name(pck.data);
    std::ifstream file;
    file.open(file_name.c_str());
    int fileSize;
    file.seekg(0, ios::end);
    fileSize = file.tellg();
    file.seekg(0, ios::beg);
    int losses=0;
    clock_t before=clock();
    if(fileSize > 0)
    {

        printf("Trying file read size:%d!\n",fileSize);
        char data[500];

        printf("file read!\n");
        packet file_pck;
        memset(&file_pck,0,sizeof(packet));
        int size=0;
        int prev_percent = 0;
        before=clock();
        for(int i=0; i<fileSize; i=i+size)
        {

            size=min(500,fileSize-i);
            file.read(data, size);
            for(int j=0; j<size; j++)
            {
                file_pck.data[j]=data[j];
            }
            file_pck.len=size;

            losses+=send_to_client(cli,sock, file_pck,seqno,false);
            printf("packet no %d sent !! \n",i);
            seqno=!seqno;
            int percent = (int)(i / (double)fileSize * 100);
            if(percent % 10 == 0 && percent > prev_percent) {
                prev_percent = percent;
                printf("sent : %d \n", percent);
            }
        }
    }

    for(int i=0; i < 1000; i++)
    {
        packet end_pck;
        memset(&end_pck,0,sizeof(end_pck));
        end_pck.len=0;
        end_pck.seqno=!seqno;
        send_udp(sock,&end_pck,sizeof(end_pck),cli);
    }
    clock_t after=clock();
    printf("time :%f\n" ,((after-before)/ (double) CLOCKS_PER_SEC) * 1000);

   // printf("finished loop\n");
    double loss= (double)losses/((double)fileSize/500);
    loss*=100;
    printf("losses detected from sender %f\n",loss);

    printf("requestig mutex\n");
    pthread_mutex_lock (&mutexsum);
    for(int i=0; i<serving.size(); i++)
    {
        if(serving[i].sin_addr.s_addr==cli.sin_addr.s_addr && serving[i].sin_port==cli.sin_port)
        {
            serving.erase (serving.begin()+i);
        }
    }
    pthread_mutex_unlock (&mutexsum);
    printf("unlocking mutex\n");
    close(sock);
    file.close();
    printf("file closed\n");
    pthread_exit(NULL);
}

void start_server(int port,int window,int seed,int loss_probability)
{
    pthread_mutex_init(&mutexsum, NULL);
    printf("starting server of stop and wait protocol\n");

    int sock; /* socket */
    struct sockaddr_in echoServAddr; /* local address */
    struct sockaddr_in echoClntAddr; /* client address */
    unsigned int cliAddrLen; /* Length of incoming message */
    char echoBuffer[ECHOMAX]; /* Buffer for echo string */
    unsigned short echoServPort; /* Server port */
    int recvMsgSize; /* Size of received message */

    echoServPort = port; /* First arg' local port */

    /* Create socket for sending/receiving datagrams */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        DieWithError("socket() failed");
    }
    set_failure_rate(loss_probability);
    set_seed(seed);
    /* Construct local address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr)); /* Zero out structure */
    echoServAddr.sin_family = AF_INET; /* Internet address family */
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    echoServAddr.sin_port = htons(echoServPort); /* Local port */

    /* Bind to the local address */
    if (bind(sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0)
        DieWithError("bind() failed");

    printf("server is running (y)\n");
    while(true)   /* Run forever */
    {
        /* Set the size of the in-out parameter */
        cliAddrLen = sizeof(echoClntAddr);
        /* Block until receive message from a client */
        packet recvd;
        if ((recvMsgSize = recvfrom(sock, &recvd, sizeof(recvd), 0,(struct sockaddr *) &echoClntAddr, &cliAddrLen)) < 0)
        {
            DieWithError("recvfrom() failed") ;
        }
        printf("Handling client %s\n", inet_ntoa(echoClntAddr.sin_addr)) ;

        printf("%s",recvd.data);

        pthread_mutex_lock (&mutexsum);
        printf("lock aqcuired\n");
        bool existing=false;
        for(int i=0; i<serving.size(); i++)
        {
            if(serving[i].sin_addr.s_addr==echoClntAddr.sin_addr.s_addr && serving[i].sin_port==echoClntAddr.sin_port)
            {
                existing=true;
                break;
            }
        }
        //printf("finished check\n");
        if(existing)
        {
            printf("client already exists!\n");
            pthread_mutex_unlock (&mutexsum);
            continue;
        }

        serving.push_back(echoClntAddr);
        pthread_mutex_unlock (&mutexsum);
        printf("unlocked mutex\n");
        pthread_t temp;
        thread_arg *args=new thread_arg();
        args->cli=echoClntAddr;
        args->pck_file=recvd;
        //printf("before thread creation\n");
        int rc = pthread_create(&temp, NULL, send_file, (void *) args);
    }
}
int send_to_client(sockaddr_in cli,int sock,packet pck,int seqno,bool ignore_timeout)
{

    pck.seqno=seqno;
    pck.cksum=setCheckSum(&pck,true);

    int x = send_udp(sock, &pck, sizeof(pck), cli);

    int respStringLen; /* Length of receiv

    ed response */

    unsigned int fromSize=sizeof(cli);
    int recvd_seq=!seqno;
    int losses=0;
    /* Recv a response */
    while(recvd_seq != seqno)
    {
        ack_packet ack;
        memset(&ack,0,sizeof(ack_packet));

        if((respStringLen = recvfrom(sock, &ack, sizeof(ack), 0,(struct sockaddr *) &cli, &fromSize)) <0)
        {
            if(ignore_timeout)
            {
                return 0;
            }
            else if (errno==EWOULDBLOCK)
            {
                x = send_udp(sock, &pck, sizeof(pck), cli);
                printf("timeout send again with seqno: %d and checksum:%d !\n",seqno,pck.cksum);
                losses++;
                continue;
            }
            else
            {
                DieWithError("recvfrom() failed");
            }
        }
        else
        {
            recvd_seq = ack.ackno;
            if(!validateCheckSum(ack))
            {
                recvd_seq = !seqno;
            }
            else {
            printf("ack received !\n");
            }
        }

    }
    return losses;
}
bool validateCheckSum(ack_packet file_pck)
{
    ack_packet new_pck;

    memset(&new_pck,0,sizeof(ack_packet));

    new_pck.len=file_pck.len;
    new_pck.ackno=file_pck.ackno;

    //printf("ack packet seqno: %d with checksum :%d\n",file_pck.ackno,file_pck.cksum);
    return setCheckSum(&new_pck,false)==file_pck.cksum;
    //return true;
}
uint16_t setCheckSum(void * file_pck,bool from_server)
{
    //return 0;
    int cnt;
    if(from_server)
    {
        cnt=sizeof(packet);
    }

    else
    {
        cnt=sizeof(ack_packet);
    }

    uint16_t * addr=(uint16_t *) file_pck;
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

    packet *pck=(packet *)file_pck;
    //printf("checksum :%d seqno :%d\n",checksum,pck->seqno);
    return checksum;
}

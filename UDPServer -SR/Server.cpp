#include "Server.h"
#include <iostream>
#include <fstream>
#define ECHOMAX 512 /* Longest string to echo */
#define TIMEOUT_USECS 300
ofstream result("congestion.txt");

char tmp[100];

struct thread_arg
{
    sockaddr_in cli;
    packet pck_file;

};


struct timer_handler_args
{

    pthread_mutex_t * thread_timer;

    priority_queue<Pair, vector<Pair>, PairCompare>* my_queue;
    int packets;
    bool * acknowledged;
    bool * finished;
    sockaddr_in cli;
    int socket;



    pthread_mutex_t * state_mutex;
    double * window_size;
    int * current_state;
    double * ssthresh;
    int * send_base;
};
struct ack_receiver
{
    bool * finished;
    bool * acknowledged;
    int sock;
    sockaddr_in cli;



    pthread_mutex_t * state_mutex;
    double * window_size;
    int * current_state;
    double * ssthresh;

};
double global_window_size=0;
double global_ssthresh=0;
clock_t timeout=clock_t(TIMEOUT_USECS * (pow(10,-6))*CLOCKS_PER_SEC);
int tries=0;
vector<sockaddr_in> serving;
pthread_mutex_t mutexsum;

void * receiveAcks(void * args)
{
    ack_receiver * arg=(ack_receiver *) args;
    sockaddr_in cli=arg->cli;
    int sock=arg->sock;
    bool * acknowleged=arg->acknowledged;
    bool * finished=arg->finished;

    pthread_mutex_t * state_mutex=  (arg->state_mutex);

    double * window_size=arg->window_size;
    int * state=arg->current_state;
    double * st=arg->ssthresh;
    while (! *(finished))
    {
        ack_packet ack;
        int recvMsgSize;
        unsigned int cliAddrLen=sizeof(cli);
        if ((recvMsgSize = recvfrom(sock, &ack, sizeof(ack), 0,(struct sockaddr *) &cli, &cliAddrLen)) < 0)
        {
            DieWithError("recvfrom() failed") ;
        }
        else
        {
            if(!acknowleged[ack.ackno] && validateCheckSum(ack))
            {
                acknowleged[ack.ackno]=true;
                pthread_mutex_lock (state_mutex);
                if(*(state)==0)
                {

                    *(window_size)=*(window_size)+1.0;
                    if(*(window_size)>*(st))
                    {
                        *(state)=!(*(state));
                    }
                }
                else
                {
                    *(window_size)=(*(window_size))+1.0 / (*(window_size));
                }
                result<<*(state)<<","<< *(window_size)<<"," <<*(st)<<endl;

                pthread_mutex_unlock (state_mutex);
            }
        }
    }
}
void *packet_timer_handler(void * args)
{
    printf("timer thread started!!\n");
    timer_handler_args * arg=(timer_handler_args *)args;
    int sock=(arg->socket);
    sockaddr_in cli=arg->cli;
    double * window_size=arg->window_size;
    double * st=arg->ssthresh;
    pthread_mutex_t * state_mutex=  (arg->state_mutex);
    int * state=arg->current_state;
    pthread_mutex_t * thread_timer=  (arg->thread_timer);
    priority_queue<Pair, vector<Pair>, PairCompare>* my_queue=arg->my_queue;
    bool * acknowledged= (arg->acknowledged);
    bool * finished= (arg->finished);
    int packets=arg->packets;
    int * send_base=arg->send_base;
    clock_t previous=clock();
    int lost=0;
    clock_t my_timeout=timeout/10;
    while(! (*finished))
    {
       // printf("inside timer loop thread\n");
        clock_t current=clock();

        if(current-previous>my_timeout && (*my_queue).size()>0)
        {
            previous=current;
            bool has_loss=false;
            pthread_mutex_lock (thread_timer);
         //   printf("timer thread acquired packet timer mutex\n");
        //    printf("queue size is :%d\n",(*my_queue).size());
            Pair p = (*my_queue).top();
            int max_seq= *(send_base)+ (int) *(window_size) +1;
        //    printf("max_seq %d\n", max_seq);
            while(p.time<current )
            {
              //  printf("time:%lld seqno:%d current:%lld checksum:%d \n",p.time,p.pck.seqno,current,p.pck.cksum);

                (*my_queue).pop();
                if(!acknowledged[p.pck.seqno])
                {
                    if(p.pck.seqno <= max_seq) {
                        send_udp(sock,&p.pck,sizeof(p.pck),cli);
                         lost++;
                    }
                    p.time=current+timeout;
                    (*my_queue).push(p);
                    has_loss=true;

                }
                if((*my_queue).size()>0)
                    p = (*my_queue).top();
                else
                    break;
            }
          //  printf("timer thread unlocked packet timer mutex\n");
            pthread_mutex_unlock (thread_timer);
            if(has_loss)
            {
                pthread_mutex_lock (state_mutex);
                if(*(state)==0)
                {
                    *(st)=(*(window_size))/2;
                    *(window_size)=1.0;
                }
                else
                {
                    *(st)=(*(window_size))/2;
                    *(window_size)=1.0;
                    *(state)=!(*(state));
                }
                result<<*(state)<<","<< *(window_size)<<"," <<*(st)<<endl;

                pthread_mutex_unlock (state_mutex);
            }
        }
    }
    double loss_db=( (double)lost/(double)(packets +lost));

    printf("packets: %d lost: %d\n",packets,lost);
    printf("losses rate:%f %\n",loss_db*100.0);
}
void *send_file(void * args)
{

    thread_arg* arg=(thread_arg* )args;
    sockaddr_in cli=arg->cli;
    packet pck=arg->pck_file;
   // printf("hello from thread\n");
    printf("starting thread client requested: %s\n",pck.data);
    int sock;
    int seqno=0;


    struct sockaddr_in echoServAddr; /* local address */
    /* Create socket for sending/receiving datagrams */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        DieWithError("socket() failed");
    }
    string file_name(pck.data);
    std::ifstream file;
    file.open(file_name.c_str());
    int fileSize;
    file.seekg(0, ios::end);
    fileSize = file.tellg();
    file.seekg(0, ios::beg);
    int losses=0;
    clock_t before_send=clock();
    before_send=clock();
    if(fileSize > 0)
    {


        printf("Trying file read size:%d!\n",fileSize);
        char data[PACKET_SIZE];
        //std::vector<char> data(fileSize, 0);
        //file.read(&data[0], fileSize);
      //  printf("file read!\n");

        priority_queue<Pair, vector<Pair>, PairCompare>* my_queue=new priority_queue<Pair, vector<Pair>, PairCompare>();
        bool finished=false;

        pthread_mutex_t packet_timer_mutex;
        pthread_mutex_init(&packet_timer_mutex, NULL);

        pthread_mutex_t state_mutex;
        pthread_mutex_init(&state_mutex, NULL);

        int current_state=0;
        double window_size_thread=global_window_size;
        double ssthresh_thread=global_ssthresh;

        int local_window_size=(int)window_size_thread;

        int packets=(int)ceil(fileSize / (double) PACKET_SIZE);

        printf("number of pakcets: %d\n",packets);
        bool acknowledged[packets];
        memset(acknowledged,0,sizeof(acknowledged));
      //  printf("initialized acknowledge boolean\n");
        int send_base=0;
        int seqno=0;
        int current_byte=0;

        int prev_percent = 0;
        timer_handler_args * args=new timer_handler_args();

        pthread_t temp;

        args->cli=cli;
        args->acknowledged=acknowledged;
        args->finished=&finished;
        args->my_queue=my_queue;
        args->socket=sock;
        args->thread_timer=&packet_timer_mutex;
        args->packets=packets;
        args->current_state=&current_state;
        args->state_mutex=&state_mutex;
        args->ssthresh=&ssthresh_thread;
        args->window_size=&window_size_thread;
        args->send_base=&send_base;

     //   printf("before timer thread creation!!\n");

        pthread_t temp2;

        ack_receiver * args2=new ack_receiver();
        args2->cli=cli;
        args2->acknowledged=acknowledged;
        args2->sock=sock;
        args2->finished=&finished;
        args2->current_state=&current_state;
        args2->state_mutex=&state_mutex;
        args2->ssthresh=&ssthresh_thread;
        args2->window_size=&window_size_thread;


        int rc = pthread_create(&temp, NULL, packet_timer_handler , (void *) args);

        int rc2 = pthread_create(&temp2, NULL,  receiveAcks, (void *) args2);

        while(send_base<packets)
        {

            while(acknowledged[send_base]==true && send_base <packets)
                send_base++;

            if(send_base == seqno){
                //sprintf(tmp, "%10d    %10.2f", local_window_size, ssthresh_thread);
                //result<<tmp<<endl;
            }

            vector <packet> to_send;

            pthread_mutex_lock (&state_mutex);

            local_window_size=(int)ceil(window_size_thread);
           // seqno = min(seqno, send_base + local_window_size);

            pthread_mutex_unlock (&state_mutex);
            //printf("local window!!!!!!!!!!!!!!!!!!!: %d\n",local_window_size);
            while(seqno<packets && seqno < send_base+local_window_size)
            {
                if(acknowledged[seqno])
                {
                seqno++;
                continue;
                }

                file.read(data, min(PACKET_SIZE,fileSize-current_byte));

                packet pck_file;
                memset(&pck_file,0,sizeof(packet));
                int i;
                for(i=0; i<min(PACKET_SIZE,fileSize-current_byte); i++)
                {
                    pck_file.data[i]=data[i];
                }
                current_byte+=i;
                pck_file.seqno=seqno++;
                pck_file.len=i;
                pck_file.cksum=setCheckSum(&pck_file,true);

                send_udp(sock,&pck_file,sizeof(pck_file),cli);
                to_send.push_back(pck_file);
            }
            if(to_send.size()!=0)
            {

                pthread_mutex_lock (&packet_timer_mutex);
            //    printf("acquired packet timer mutex\n");
                clock_t current=clock()+timeout;
            //    printf("before if\n");
                for(int i=0; i<to_send.size(); i++)
                {
                    Pair p(current,to_send[i]);
                    (*my_queue).push(p);
                }
                pthread_mutex_unlock (&packet_timer_mutex);
           //     printf("unlocked packet timer mutex\n");
            }

            int percent = (int)(send_base / (double)packets * 100);
            if(percent % 10 == 0 && percent > prev_percent) {
                prev_percent = percent;
                printf("sent : %d\n", percent);
            }

        }
        printf("well done .\n");
        finished=true;
        result.close();

    }

    for(int i=0; i<1000; i++)
    {
        packet end_pck;
        end_pck.len=0;
        end_pck.seqno=0;
        //setCheckSum(&end_pck);
        send_udp(sock,&end_pck,sizeof(end_pck),cli);
    }

    clock_t after_send=clock();
    printf("time :%f\n" ,((after_send-before_send)/ (double) CLOCKS_PER_SEC) * 1000);
  //  printf("finished loop\n");
  //  printf("requestig mutex\n");
    pthread_mutex_lock (&mutexsum);
    for(int i=0; i<serving.size(); i++)
    {
        if(serving[i].sin_addr.s_addr==cli.sin_addr.s_addr && serving[i].sin_port==cli.sin_port)
        {
            serving.erase (serving.begin()+i);
        }
    }
    pthread_mutex_unlock (&mutexsum);
   // printf("unlocking mutex\n");
    close(sock);
    file.close();
    printf("file closed\n");
    pthread_exit(NULL);
}

void start_server(int port,int window,int seed,int loss_probability)
{
    pthread_mutex_init(&mutexsum, NULL);
    global_window_size=(double)window;
    global_ssthresh=global_window_size/2.0;
    printf("starting server of selective repeat protocol\n");

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
        printf("Handling client %s\n and port is %d", inet_ntoa(echoClntAddr.sin_addr),echoClntAddr.sin_port) ;

        printf("%s",recvd.data);

        pthread_mutex_lock (&mutexsum);
//printf("lock aqcuired\n");
        bool existing=false;
        for(int i=0; i<serving.size(); i++)
        {
            if(serving[i].sin_addr.s_addr==echoClntAddr.sin_addr.s_addr && serving[i].sin_port==echoClntAddr.sin_port)
            {
                existing=true;
                break;
            }
        }
       // printf("finished check\n");
        if(existing)
        {
            printf("client already exists!\n");
            pthread_mutex_unlock (&mutexsum);
            continue;
        }

        serving.push_back(echoClntAddr);
        pthread_mutex_unlock (&mutexsum);
       // printf("unlocked mutex\n");
        pthread_t temp;
        thread_arg *args=new thread_arg();
        args->cli=echoClntAddr;
        args->pck_file=recvd;
       // printf("before thread creation\n");
        int rc = pthread_create(&temp, NULL, send_file, (void *) args);
    }
}

bool validateCheckSum(ack_packet file_pck)
{
    ack_packet new_pck;

    memset(&new_pck,0,sizeof(ack_packet));

    new_pck.len=file_pck.len;
    new_pck.ackno=file_pck.ackno;

   // printf("ack packet seqno: %d with checksum :%d\n",file_pck.ackno,file_pck.cksum);
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



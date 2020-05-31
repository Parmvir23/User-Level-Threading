
/* Parmvir Singh               */
/* CSC139, Assignment 3        */
/* User Level Threading        */
/* 4 December  2019            */

#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>

#define SECOND 1000000
#define TIME_QUANTUM 1*SECOND
#define S_SIZE 4096
#define Max_Threads 100

#define RR 1
#define LOTTERY 2

int weight_sum = 0;
int num = 0;
int lottery[Max_Threads];

int scheduling;
typedef struct itimeinterval tv;
enum status{
    READY,
    RUNNING,
    SUSPENDED,
    SLEEPING,
    FINISHED,
};

typedef struct TCB{
    int id;
    int weight;
    enum status state;
    sigjmp_buf jbuf; 
    void* arg;
    void* (*f)(void* );
}TCB;

char *stacks[Max_Threads];
struct TCB *threads[Max_Threads];
int sleeping_time[Max_Threads];
int requested_sleeping_time[Max_Threads];
int running_time[Max_Threads];
enum status thread_states[Max_Threads][60*10];
int total = 0;

typedef struct statictics{
    int tid;
    enum status states[60*10];
    int execution_time;
    int requested_sleeping;
}statistics;
typedef struct statistics *thread_stats[Max_Threads];

typedef struct linked_list{
    struct TCB thread_TCB;
    struct linked_list *next;
} readyQ;
readyQ *ready_qhead = NULL;
readyQ *ready_qtail = NULL;

typedef struct linked_list suspendedQ, sleepingQ;
suspendedQ *suspended_qhead = NULL;
suspendedQ *suspended_qtail = NULL;
sleepingQ *sleeping_qhead = NULL;
sleepingQ *sleeping_qtail = NULL;

int new_thread(void (*f) (void));
void Go();
int GetMyId();
enum status GetStatus(int thread_id);
//int DeleteThread(int thread_id);
//void Dispatch(int sig);
void YieldCPU();
//int SuspendThread(int thread_id);
//int ResumeThread(int thraed_id);
//void SleepThread(int sec);
//void CleanUp();
int new_thread_arg1(void* (*f)(void* arg), void* par);
int new_thread_arg2(void* (*f)(void* arg), void* par);
int get_stats(int thread_id);

#ifdef __x86_64__
// code for 64 bit Intel arch

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

//A translation required when using an address of a variable
//Use this as a black box in your code.
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
		"rol    $0x11,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#else
// code for 32 bit Intel arch

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5 

//A translation required when using an address of a variable
//Use this as a black box in your code.
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
		"rol    $0x9,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#endif

void switchThreads();


void f( void ) {
    printf("My ID is: %d\n", GetMyID()); 
    int i=0;

    while(1) {
        ++i;
        if (i % 99999555 == 0) {
	  printf("f: switching\n");
        }
	int j;
	while(j<99995555) j++;
    }
}

void g( void )
{
    printf("My ID is: %d\n", GetMyID()); 
    int i=0;

    CleanUp(); //for test
    while(1){
        ++i;
        if (i % 99999555 == 0) {
	   printf("g: switching\n");
        }
	int j;
        while(j<99995555) j++;
    }
}

void h( void )
{
    printf("My ID is: %d\n", GetMyID()); 
    int i=0;

    while(1){
        ++i;
        if (i % 99999555 == 0) {
           printf("h: switching\n");
        }
	int j;
        while(j<99995555) j++;
    }
}


void AddToQ(int item, node *&front, node *&rear)
{
	node *newNode = new node;
	newNode->item = item;
	newNode->next = NULL;
	if (rear) {

		rear->next = newNode;
		rear = newNode;
	}
	else
		front = rear = newNode;
}

void RemoveFromQ(node *&front, node *&rear)
{
	node *temp;
	if (front) {
		temp = front;//to empty out space for a new front
        if(front->next){
            front = front->next; //assign the next node into front
            delete temp; //get rid of old front
		//if (front)
			//rear = NULL;
        }
        else
            printf("QueueEmpty\n");
	}
}

int new_thread(void (*func)(void)){
    int id, i = 0;
    while(threads[i] != NULL)
    {i++;}
    id = i;
    char *stack = (char*)malloc(S_SIZE);
    stack[i] = stack;
    threads[i] = (struct TCB *)malloc(sizeof(struct TCB));
    threads[i]->id = id;
    threads[i]->weight = rand()%10+1;
    threads[i]->state = READY;

    address_t sp,pc;
    sp = (address_t)stack + S_SIZE - sizeof(address_t);
    pc = (address_t)func;

    sigsetjmp(threads[i]->jbuf,1);
    (threads[i]->jbuf->__jmpbuf)[JB_SP] = translate_address(sp);
    (threads[i]->jbuf->__jmpbuf)[JB_SP] = translate_address(pc);

    sigemptyset(&(threads[i]->jbuf->__saved_mask));
    AppendReadyQ((readyQ *)threads[i]);
    if(id >= 0){
        printf("A new thread created and appended to the ready Q:(%d)\t(0x%0161x)\t(%d)!!\n", id,(unsigned long)func,threads[i]->weight);
        return id;
    }
    else{
        return -1;
    }
}
int new_thread_arg1(void *(*f)(void*arg), void * par){
    int id,i=0;
    while(threads[i] != NULL){
        i++;
    }
    id = i;
    char *stack = (char*)malloc(S_SIZE);
    stack[i] = stack;
    threads[i] = (struct TCB *)malloc(sizeof(struct TCB));
    threads[i]->id = id;
    threads[i]->weight = rand()%10+1;
    threads[i]->state = READY;
    threads[i]->arg = par;
    threads[i]->f = f;

    address_t sp,pc;
    sp = (address_t)stack + S_SIZE - sizeof(address_t);
    pc = (address_t)f;

    int ret_val = sigsetjmp(threads[i]->jbuf,1);
    if(ret_val == 0){
        (threads[i]->jbuf->__jmpbuf)[JB_SP] = translate_address(sp);
        (threads[i]->jbuf->__jmpbuf)[JB_PC] = translate_address(pc);
        sigemptyset(&(threads[i]->jbuf->__saved_mask));

        AppendReadyQ((readyQ*)threads[i]);
        if(id >= 0){
            printf("A new thread created and appended to the ready Q:(%d)\t(0x%0161x)\t(%d)!!\n", id,(unsigned long)f,threads[i]->weight);
            return id;
        }
        else{
            return -1;
        }
    }
    else if(ret_val == 1){
        int current = GetMyID();
        threads[current]->f(threads[current]->arg);
    }
    else exit (-1);
}
void procedure(void){
    printf("The thread with arguments is returned to be here!\n");
    fflush(stdout);
}
int new_thread_arg2(void* (*f)(void*arg), void*par){
    int id, i = 0;
    while(threads[i] != NULL)
    {i++;}
    id = i;
    char *stack = (char*)malloc(S_SIZE);
    stack[i] = stack;
    threads[i] = (struct TCB *)malloc(sizeof(struct TCB));
    threads[i]->id = id;
    threads[i]->weight = rand()%10+1;
    threads[i]->state = READY;

    address_t sp,pc;
    sp = (address_t)stack + S_SIZE - sizeof(address_t);
    pc = (address_t)f;

    sigsetjmp(threads[i]->jbuf,1);
    (threads[i]->jbuf->__jmpbuf)[JB_SP] = translate_address(sp);
    (threads[i]->jbuf->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&(threads[i]->jbuf->__saved_mask));

    AppendReadyQ((readyQ*)threads[i]);

    if(id >= 0){
        printf("A new thread created and appended to the ready Q:(%d)\t(0x%0161x)\t(%d)!!\n", id,(unsigned long)f,threads[i]->weight);
        return id;
    }
    else{
        return -1;
    }
}
int GetMyId(){
    int i = 0;
    int ID;
    for(i=0;i<Max_Threads;i++){
        if(threads[i] != NULL && threads[i]->state == RUNNING)
        {
            ID = threads[i]->id;
            break;
        }
    }
    return ID;
}
enum status GetStatus(int thread_id){
    enum status stat = threads[thread_id]->state;
    if(stat == READY || stat == RUNNING || stat == SUSPENDED || stat == FINISHED || stat == SLEEPING)
        return stat;
    else
        return -1;
}


void setup()
{
    address_t sp, pc;
    
    sp = (address_t)stack1 + STACK_SIZE - sizeof(address_t);
    pc = (address_t)f;
    
    
    sigsetjmp(jbuf[0],1);
    (jbuf[0]->__jmpbuf)[JB_SP] = translate_address(sp);
    (jbuf[0]->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&jbuf[0]->__saved_mask);     


    sp = (address_t)stack2 + STACK_SIZE - sizeof(address_t);
    pc = (address_t)g;
    
    sigsetjmp(jbuf[1],1);
    (jbuf[1]->__jmpbuf)[JB_SP] = translate_address(sp);
    (jbuf[1]->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&jbuf[1]->__saved_mask);         
}

int RRSchedule(){
    int next;
    readyQ *head;
    head = getHeadReadyQ();
    next = head ->thread_TCB.id;

    head->thread_TCB.state = RUNNING;
    printf("Round robin scheduling performed\n");
    return next;
}
int LotteryHandler(){
    readyQ *p = ready_qhead;
    if(ready_qtail==NULL){
        printf("Oh the ready Q is empty\n");
        return -1;
    }
    else{
        while(p!=NULL){
            weight_sum = weight_sum + p->thread_TCB.weight;
            p = p->next;
            num++;
        }
    }
    printf("The number and sum of threads weights is: (%d)\t(%d)\n", num, weight_sum);

    p = ready_qhead;
    int i = 0;
    while(p != NULL){
        lottery[i] = (int)(((p->thread_TCB.weight)*100)/weight_sum);
        printf("The lottery tickets: %d\t", lottery[i]);
        i++;
        p = p->next;
    }
    printf("\n");

    printf("The lottery intervals: %d\t", lottery[0]);
    for(i=1;i<num;i++){
        lottery[i]=lottery[i]+lottery[i-1];
        printf("The lottery intervals: %d\t", lottery[i]);
    }
    printf("\n");
    return 0;
}
int Lottery(){
    int next;
    int ready_qlocation = 0;
    int i = 0;

    num = 0;
    weight_sum = 0;
    if(LotteryHandler()!=0){
        printf("Error in lottery initialization\n");
        return -1;
    }
    time_t t;
    srand((unsigned)time(&t));

    int random = rand()%100;
    printf("The random number: %d\n",random);

    for(i=0;i<num-1;i++){
        if(lottery[i] < random && random <= lottery[i+1]){
            ready_qlocation = i+1;
            break;
        }
    }
    readyQ *p = GetMiddleReadyQ(ready_qlocation);
    next = p->thread_TCB.state = RUNNING;

    return(next);
}
void Go(){
    int next;
    printf("Please indicate the scheduling algorithm you want to use: \n 1 for round robin \n 2 for lottery scheduling\t");
    scanf("%d", &scheduling);

    if(ready_qtail == NULL){
        printf("Oh please create threads The ready Q is empty\n");
        exit(-1);
    }
    if(scheduling == 1)
        next = RRSchedule();
    else if (scheduling == 2)
        next = Lottery();
    else{
        printf("Alert: scheduling not implemented yet");
        exit(-1);
    }
    total = total + 1;
    thread_states[next][total] = RUNNING;
    running_time[next] = running_time[next] + 1;
    siglongjmp(threads[next]->jbuf,1);
}
int DeleteThread(int thread_id){

}
void Dispatch(int sig){

}

void YieldCPU(){
    int current = GetMyId();
    int ret_val = sigsetjmp(threads[current]->jbuf,1);

    if(ret_val == 0){
        printf("Yield CPU to other threads: ret_val = %d\n",ret_val);

        thread_states[current][total] = SUSPENDED;
        threads[current]->state = SUSPENDED;
        AppendSuspendedQ((suspendedQ*)threads[current]);
        if(scheduling == 1)
            current = RRSchedule();
        else if(scheduling == 2)
            current = Lottery();
        printf("Yield to thread ID: %d\n", current);
        siglongjmp(threads[current]->jbuf,1);
    }

    if(ret_val == 1){
        return;
    }
}

int SuspendThread(int thread_id){

}
int ResumeThread(int thraed_id){

}
void SleepThread(int sec){

}
void CleanUp(){

}
int get_stats(int thread_id){
    thread_stats[thread_id]->tid = thread_id;
    int i;
    for(i=0;i<total;i++)
        thread_stats[thread_id]->states[i] = thread_states[thread_id][i];
    thread_stats[thread_id]->execution_time = running_time[thread_id];
    thread_stats[thread_id]->requested_sleeping = requested_sleeping_time[thread_id];
    return (thread_id);
}
void CreateStatictics(){
    int i;
    for(i=0;i<Max_Threads;i++)
        thread_stats[i] = (struct statistics*)malloc(sizeof(struct statictics));
}
void switchThreads()
{
    static int currentThread = 0;
    
    int ret_val = sigsetjmp(jbuf[currentThread],1);
    printf("SWITCH: ret_val=%d\n", ret_val); 
    if (ret_val == 1) {
        return;
    }
    
    currentThread = 1 - currentThread;
    siglongjmp(jbuf[currentThread],1);
}

int main()
{
  InitiateThreads();

  // for test
  int i = 0;
  for(i=0;i<20;i++) {
    CreateThread(f);
    CreateThread(h);
  }
  new_thread(f);
  new_thread(g);

  CreateStatistics();

  signal(SIGVTALRM, Dispatch);

  // Time to the next timer expiration.
  tv.it_value.tv_sec = TIME_QUANTUM/SECOND; //time of first timer
  tv.it_value.tv_usec = 0; //time of first timer

  // Value to put into "it_value" when the timer expires.
  tv.it_interval.tv_sec = TIME_QUANTUM/SECOND; //time of all timers but the first one
  tv.it_interval.tv_usec = 0; //time of all timers but the first one
  
  setitimer(ITIMER_VIRTUAL, &tv, NULL);	
	
  Go();
  
  return 0;
}



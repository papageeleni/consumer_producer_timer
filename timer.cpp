#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>

#define N_OF_ARGS 10

#define P_MAX 1
#define C_MAX 4 
#define QUEUESIZE 4 

// the files to store the statistics  
FILE * WaitingTime;
FILE * drift;
FILE * actualPeriods;

struct workFunction {
  void * (*work)(void *);
  void * arg;
}; // workFunction struct



void *producer (void *args);
void *consumer (void *args);


typedef struct {
  struct workFunction * buf;
  long head, tail;
  int full, empty;
  pthread_mutex_t *mut;
  pthread_cond_t *notFull, *notEmpty;
} queue; // queue struct

queue *queueInit (int capacity);
void queueDelete (queue *q);

void queueAdd (queue *q, struct workFunction in);
void queueExec ( queue *q,struct workFunction  workFunc,int currHead);

void *func(void *arg){
  
  double x;
  for (int i=0; i<11; i++){
    x = sin(*(int *)(arg)*M_PI / 180);
  }
  return (NULL);
}

typedef struct {
  unsigned int Period; //Period given in milliseconds
  unsigned int TasksToExecute; //How many times the workFunc will by executed by times
  unsigned int StartDelay;// StartDellay is in seconds, the initial dellay of workFunc execution
  pthread_t producer_tid; // the producer thread id correspoding to timer

  struct workFunction * TimerFcn;// TimerFcn

  void * (* StartFcn)(void *);
  void * (* StopFcn)(void *);
  void * (* ErrorFcn)(void *);

  void * userData;

  queue *Q;
} Timer; // Timer struct

Timer * timerInit (unsigned int t_Period,unsigned int t_TasksToExecute,
  unsigned int t_StartDelay,struct workFunction * t_TimerFcn);
void timerDelete(Timer * t);
void * def_StartFcn(void * arg);
void * def_StopFcn(void * arg);
void * def_ErrorFcn(void * arg);

void start(Timer * t);
void startat(Timer * t,int y,int m,int d,int h,int min,int sec);

void * (* functions)(void *)= func;
int argument = 10;
int* argumentsPtr = &argument;

struct timeval * startWaitingTime ;
struct timespec * startWaitingTime2 ;

long functionsCounter ;
double meanWaitingTime;
int TotalDrift=0;
int terminationStatus;

int main (int argc, char* argv[])
{
  int test_case=2;
  int nOftimers;
  if (test_case == 0 or test_case == 1 or test_case == 2)
    nOftimers = 1;
  else
    nOftimers = 3; 

  int timerIndex=0;

  if(!argc || argc ==1 ){ //if no arguments are given
    test_case = 0;
    printf("wrong case, max value 3 and min 0 \n");
  }
  else{
    test_case = atoi(argv[1]); // get the test case from the arguments 
  }

  FILE  *dataFileMean;  

  char filename1[sizeof "WaitingTimeQUEUESIZEXX_QXXX_caseX.csv"];
  sprintf(filename1, "WaitingTimeQUEUESIZE%02d_Q%03d_case%d.csv", QUEUESIZE, C_MAX, test_case);

  char filename2[sizeof "driftQUEUESIZEXX_QXXX_caseX.csv"];
  sprintf(filename2, "driftQUEUESIZE%02d_Q%03d_case%d.csv", QUEUESIZE, C_MAX, test_case);

  char filename3[sizeof "actualPeriodsQUEUESIZEXX_QXXX_caseX.csv"];
  sprintf(filename3, "actualPeriodsQUEUESIZE%02d_Q%03d_case%d.csv", QUEUESIZE, C_MAX, test_case);

  WaitingTime = fopen(filename1,"a");
  drift = fopen(filename2,"a");
  actualPeriods = fopen(filename3,"a");

  queue *fifo; 
  pthread_t producers[P_MAX], consumers[C_MAX];//threads declaration
  Timer * timers[nOftimers];

  functionsCounter=0;
  meanWaitingTime=0;

  terminationStatus=0;

  fifo = queueInit (QUEUESIZE); //queue initialization
  if (fifo ==  NULL) {
    fprintf (stderr, "main: Queue Init failed.\n");
    exit (1);
  }

  startWaitingTime = (struct timeval *) malloc (sizeof(struct timeval)*QUEUESIZE);
  startWaitingTime2 = (struct timespec *) malloc (sizeof(struct timespec)*QUEUESIZE);
  if(  (!startWaitingTime) || (!startWaitingTime2)  ){
    printf("Couldn't allocate Memory \n" );
    exit(1);
  }

  for (int i = 0; i < C_MAX; i++)
    pthread_create (&consumers[i], NULL, consumer, fifo);

  double t_periods[]={1e3,1e2,10}; // milliseconds
  double t_TasksToExecute[]={3600,36000,360000};
  unsigned int t_StartDelay=0;
  int argIndex, funcIndex; 

  for(int i=0; i<nOftimers; i++){
    argIndex=1+i;
    funcIndex=1;

    struct workFunction t_TimerFcn;
    t_TimerFcn.work = functions;
    t_TimerFcn.arg = (void *) argumentsPtr;

    timers[i]= timerInit(t_periods[test_case], t_TasksToExecute[test_case], t_StartDelay, &t_TimerFcn);
    (timers[i] -> Q)= fifo;
    (timers[i] -> producer_tid)=producers[i];
    start(timers[i]);

  }

  for(int i=0; i<nOftimers; i++){
    pthread_join ((timers[i]-> producer_tid), NULL);
    (timers[i]-> StopFcn)(NULL);
    timerDelete(timers[i]);
  }


  for (int i = 0; i < C_MAX; i++)
    pthread_join (consumers[i], NULL);


  queueDelete (fifo);

  printf("\n FINISHED. \n");

  printf("For Producers=%d, and Customer=%d ,QUEUESIZE=%d the mean waiting-time is : %lf usec \n \n",P_MAX,C_MAX,QUEUESIZE,meanWaitingTime);
  
  char filename4[sizeof "meanWaitingtime_caseX.csv"];

  sprintf(filename4, "meanWaitingtime_case%d.csv", test_case);

  dataFileMean=fopen(filename4,"a");

  fprintf(dataFileMean,"%d,%d,%lf\n",QUEUESIZE,C_MAX,meanWaitingTime);

  fclose(WaitingTime);
  fclose(drift);
  fclose(actualPeriods);

  free(startWaitingTime);
  free(startWaitingTime2);

  return 0;
}

//producers' threads function
void *producer (void * t)
{
  struct timeval assignTimestamp;
  struct timeval previousAssignTimestamp;
  unsigned int actualPeriod;

  int drifterror=0;

  Timer * timer; 

  timer = (Timer *)t;
  unsigned int executions= (timer -> TasksToExecute);
  unsigned int initialPeriod=(timer->Period)*1e3;
  int fixedPeriod= initialPeriod; 

  if(timer->StartDelay){
    sleep(timer->StartDelay);
    printf("A timer is starting now as scheduled, with the respect to its startDelay!\n");
  }

  while (executions)
  {
    pthread_mutex_lock ((timer-> Q)->mut);
    while ((timer-> Q)->full) {
      timer->ErrorFcn(NULL);
      pthread_cond_wait ((timer-> Q)->notFull, (timer-> Q)->mut);

    }

    gettimeofday(&assignTimestamp,NULL);
    queueAdd ((timer-> Q), *(timer-> TimerFcn));
    pthread_mutex_unlock ((timer-> Q)->mut);
    pthread_cond_signal ((timer-> Q)->notEmpty);

    executions--;
    if(executions< (timer-> TasksToExecute -1)){

      actualPeriod= (assignTimestamp.tv_sec*1e6 -previousAssignTimestamp.tv_sec*1e6);
      actualPeriod+= (assignTimestamp.tv_usec-previousAssignTimestamp.tv_usec);
      drifterror= actualPeriod- initialPeriod;
      fixedPeriod= (fixedPeriod - drifterror)> 0 ?fixedPeriod - drifterror :fixedPeriod; // calulcating the sleep time to manage the drift
      fprintf(drift,"%f\n ",(float)(drifterror/1e6));
      fprintf(actualPeriods,"%f\n ",(float)(actualPeriod/1e6));
    }
    previousAssignTimestamp=assignTimestamp;

    if(!executions)
      break;
    usleep((fixedPeriod)); 

  }
  
  terminationStatus++;
  pthread_cond_signal ((timer-> Q)->notEmpty);
  return (NULL);
}

//consumers' threads function
void *consumer (void *q)
{
  queue *fifo;
  fifo = (queue *)q;

  while (1) {

    pthread_mutex_lock (fifo->mut);

    while (fifo->empty ) {

      if(terminationStatus ==P_MAX){

        pthread_mutex_unlock (fifo->mut);
        pthread_cond_signal (fifo->notEmpty);
        return NULL;
      }

      pthread_cond_wait (fifo->notEmpty, fifo->mut);

    }
    struct workFunction execFunc;
    long currHead;
    currHead=fifo->head;
    execFunc=fifo->buf[currHead];

    queueExec (fifo,execFunc,currHead);
  }
  return (NULL);
}

queue *queueInit (int capacity)
{
  queue *q;

  q = (queue *)malloc (sizeof (queue));
  if (q == NULL) return (NULL);

  q->buf=(struct workFunction *) malloc (sizeof(struct workFunction)*capacity);
  if(!(q->buf)){
    printf("Couldn't allocate Memory!\n" );
    exit(1);
  }
  q->empty = 1;
  q->full = 0;
  q->head = 0;
  q->tail = 0;
  q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
  pthread_mutex_init (q->mut, NULL);
  q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notFull, NULL);
  q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notEmpty, NULL);

  return (q);
}

void queueDelete (queue *q)
{
  pthread_mutex_destroy (q->mut);
  free (q->mut);
  pthread_cond_destroy (q->notFull);
  free (q->notFull);
  pthread_cond_destroy (q->notEmpty);
  free (q->notEmpty);
  free (q->buf);
  free (q);
}

void queueAdd (queue *q, struct workFunction in)
{

  q->buf[q->tail] = in;

  gettimeofday(&startWaitingTime[q->tail],NULL); // start wait time
  clock_gettime(CLOCK_MONOTONIC, &startWaitingTime2[q->tail]);

  q->tail++;
  if (q->tail == QUEUESIZE)
    q->tail = 0;
  if (q->tail == q->head)
    q->full = 1;
  q->empty = 0;

  return;
}

void queueExec ( queue *q,struct workFunction  workFunc,int currHead)
{


  ////TIME CALCULATIONS/////
  long currWaitingTime =0 ;
  long currWaitingTime2=0 ;
  struct timeval endTime;
  struct timespec endTime2;

  gettimeofday(&endTime,NULL);
  clock_gettime(CLOCK_MONOTONIC, &endTime2);
   
  currWaitingTime= (endTime.tv_sec*1e6 -(startWaitingTime[currHead] ).tv_sec*1e6); // from adding to excecuting time
  currWaitingTime+= (endTime.tv_usec-(startWaitingTime[currHead] ).tv_usec);

  currWaitingTime2=(endTime2.tv_sec-(startWaitingTime2[currHead ]).tv_sec) * 1e9  ;
  currWaitingTime2+=(endTime2.tv_nsec-(startWaitingTime2[currHead ]).tv_nsec  );

  if(currWaitingTime>=5)
    fprintf(WaitingTime,"%ld\n ",currWaitingTime);
  else
    // nanoseconds precision
    fprintf(WaitingTime, "%lf\n", ((float)currWaitingTime2)/1000);

  ++functionsCounter;


  q->head++;
  if (q->head == QUEUESIZE)
    q->head = 0;
  if (q->head == q->tail)
    q->empty = 1;
  q->full = 0;

  meanWaitingTime= (meanWaitingTime*(functionsCounter-1) + (double)currWaitingTime )/(functionsCounter) ;

  pthread_mutex_unlock (q->mut);
  pthread_cond_signal (q->notFull);

  (workFunc.work)((workFunc.arg));
  return;
}

Timer * timerInit (  unsigned int t_Period,
  unsigned int t_TasksToExecute,
  unsigned int t_StartDelay,
  struct workFunction * t_TimerFcn){

  Timer *t;

  t = (Timer *)malloc (sizeof (Timer));
  if (!t){
    printf("Failed to allocate memory for the timer!");
    return (NULL);
  }

  t->Period = t_Period;
  t->TasksToExecute = t_TasksToExecute;
  t->StartDelay = t_StartDelay;

  t->TimerFcn = t_TimerFcn;

  t->StartFcn = &def_StartFcn ;
  t->StopFcn = &def_StopFcn;
  t->ErrorFcn = &def_ErrorFcn;

  t->userData = NULL;

  return (t);
}

void timerDelete(Timer * t){
  free( t );
}

void start(Timer * t){
  (t-> StartFcn)(NULL);
  pthread_t t_producer;
  pthread_create (&(t->producer_tid) , NULL, producer, t);
}

void startat(Timer * t,int y,int m,int d,int h,int min,int sec){
  int totalDelayInSeconds=0;

  struct timeval tv;
  time_t nowtime;
  struct tm *nowtm;

  gettimeofday(&tv, NULL);
  nowtime = tv.tv_sec;
  nowtm = localtime(&nowtime);

  printf("The time now is: %d-%02d-%02d %02d:%02d:%02d\n", nowtm->tm_year + 1900, nowtm->tm_mon + 1, nowtm->tm_mday, nowtm->tm_hour, nowtm->tm_min, nowtm->tm_sec);
  printf("A timer is set to execute at: %d-%02d-%02d %02d:%02d:%02d\n", y, m, d, h, min, sec);
  struct tm * timerExecStart = (struct tm *)malloc(sizeof(struct tm));
  if(!timerExecStart){
    printf("Failed to allocate memory!\n");
    exit(1);
  }
  timerExecStart->tm_year = y -1900;
  timerExecStart->tm_mon = m -1;
  timerExecStart->tm_mday = d;
  timerExecStart->tm_hour = h;
  timerExecStart->tm_min = min;
  timerExecStart->tm_sec = sec;
  totalDelayInSeconds=(int) difftime( mktime(timerExecStart),mktime(nowtm));
  if(totalDelayInSeconds>0)
    t-> StartDelay = totalDelayInSeconds;

  pthread_t t_producer;
  pthread_create (&(t->producer_tid) , NULL, producer, t);
  free(timerExecStart);
}

void * def_StartFcn(void * arg){
  printf("A timer started.\n");
}

void * def_StopFcn(void * arg){
  printf("A timer finished.\n");
}

void * def_ErrorFcn(void * arg){
  printf("Queue full.\n");

}
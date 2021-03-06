
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>
#include <iostream>
#include <string.h>

#include "SimpleIndicationWrapper.h"
#include "SimpleRequestProxy.h"
#include "GeneratedTypes.h"

using namespace std;

//bool flag = false;

FILE *fp0, *fp1;

//pthread_mutex_t mutex;
sem_t done_sem;

double timespec_diff_sec( timespec start, timespec end ) {
  double t = end.tv_sec - start.tv_sec;
  t += ((double)(end.tv_nsec - start.tv_nsec)/1000000000L);
  return t;
}


class SimpleIndication : public SimpleIndicationWrapper
{  
public:
  int dtaCnt;
  int cmdCnt;
  virtual void finish(uint64_t v) {
    fprintf(stderr, "FPGA finishes in %d cycles.\n", v);
    //sleep(1);
    /*
    pthread_mutex_lock(&mutex);
    flag = true;
    pthread_mutex_unlock(&mutex);
    */
    sem_post(&done_sem);
    //exit(0);
  }

  virtual void dumpReqs(MemReqType_dummy v){
    fprintf(stderr, "Cmd[%d]\topcode = %d\tkeylen = %d\tvallen = %d\thv=%d\n", cmdCnt, v.opcode, v.keylen, v.vallen, v.hv);
    fprintf(fp0, "Cmd[%d]\topcode = %d\tkeylen = %d\tvallen = %d\thv=%d\n", cmdCnt++, v.opcode, v.keylen, v.vallen, v.hv);
  }

  
  virtual void dumpDta(uint64_t v){
    fprintf(stderr, "Data[%d] = %016lx\n", dtaCnt, v);
    fprintf(fp1, "Data[%d] = %016lx\n", dtaCnt++, v);
  }

  timespec aurorastart;
  virtual void hexDump(uint32_t v) {
    printf( "%x--\n", v );
    timespec now;
    clock_gettime(CLOCK_REALTIME, & now);
    printf( "aurora data! %f\n", timespec_diff_sec(aurorastart, now) );
    //fflush(stdout);
    }
    
  SimpleIndication(unsigned int id) : SimpleIndicationWrapper(id), cmdCnt(0),dtaCnt(0){}
};

SimpleIndication *indication = 0;
SimpleRequestProxy *device = 0;



void setAuroraRouting2(int myid, int src, int dst, int port1, int port2) {
  if ( myid != src ) return;

  for ( int i = 0; i < 8; i ++ ) {
    if ( i % 2 == 0 ) { 
      device->setAuroraExtRoutingTable(dst,port1, i);
    } else {
      device->setAuroraExtRoutingTable(dst,port2, i);
    }
  }
}

void auroraifc_start(int myid) {
  device->setNetId(myid);
  device->auroraStatus(0);

  //This is not strictly required
  for ( int i = 0; i < 8; i++ ) 
    device->setAuroraExtRoutingTable(myid,0,i);
  /*
  // This is set up such that all nodes can one day 
  // read the same routing file and apply it
  //setAuroraRouting2(myid, 0,1, 0,1);
  setAuroraRouting2(myid, 0,1, 0,0);
  setAuroraRouting2(myid, 0,2, 2,3);
  //setAuroraRouting2(myid, 0,3, 2,3);
  setAuroraRouting2(myid, 0,3, 3,3);
  
  setAuroraRouting2(myid, 1,0, 0,0);
  //setAuroraRouting2(myid, 1,0, 0,1);
  setAuroraRouting2(myid, 1,2, 0,1);
  setAuroraRouting2(myid, 1,3, 0,1);
  
  setAuroraRouting2(myid, 2,0, 0,3);
  setAuroraRouting2(myid, 2,1, 0,3);
  setAuroraRouting2(myid, 2,3, 0,3);
  
  setAuroraRouting2(myid, 3,0, 1,2);
  setAuroraRouting2(myid, 3,1, 1,2);
  setAuroraRouting2(myid, 3,2, 0,3);
  */
  
  for ( int i = 0; i < 10; i++ ) {
    if ( myid > i ) {
      setAuroraRouting2(myid, myid, i, 2,3);
    } else {
      setAuroraRouting2(myid,myid, i, 0,1);
    }
  }
  /*
  setAuroraRouting2(myid, 0,1, 0,2);
  setAuroraRouting2(myid, 0,2, 1,3);
  setAuroraRouting2(myid, 0,3, 1,3);

  setAuroraRouting2(myid, 1,0, 0,1);
  setAuroraRouting2(myid, 1,2, 0,1);
  setAuroraRouting2(myid, 1,3, 0,1);
  
  setAuroraRouting2(myid, 2,0, 0,3);
  setAuroraRouting2(myid, 2,1, 0,3);
  setAuroraRouting2(myid, 2,3, 0,3);

  setAuroraRouting2(myid, 3,0, 1,2);
  setAuroraRouting2(myid, 3,1, 1,2);
  setAuroraRouting2(myid, 3,2, 0,3);
  */
  usleep(100);

}

int main(int argc, const char **argv)
{
  //mutex = PTHREAD_MUTEX_INITIALIZER;
  if (sem_init(&done_sem, 1, 0)){
    fprintf(stderr, "failed to init done_sem\n");
    exit(1);
  }

  char hostname[32];
  gethostname(hostname,32);
  
  //FIXME "lightning" is evaluated to 0,
  // so when bdbm00 is returned to the cluster,
  // code needs to be modified
  if ( strstr(hostname, "bdbm") == NULL 
       && strstr(hostname, "umma") == NULL
       && strstr(hostname, "lightning") == NULL ) {
    
    fprintf(stderr, "ERROR: hostname should be bdbm[idx] or lightning\n");
    return 1;
  }

  int myid = atoi(hostname+strlen("bdbm"));

  indication = new SimpleIndication(IfcNames_SimpleIndication);
  device = new SimpleRequestProxy(IfcNames_SimpleRequest);

  portalExec_start();

  fp0 = fopen("cmdLog.txt", "w");
  fp1 = fopen("dtaLog.txt", "w");
  if ( !fp0 || !fp1){
    perror("File opening failed\n");
    exit(0);
  }

  std::cout << "Input Node number: ";
  std::cin >> myid;

  printf( "initializing aurora with node id %d\n", myid ); fflush(stdout);
  /*if ( myid == 0 )
    auroraifc_start(1);
  else
  auroraifc_start(0);*/
  
  auroraifc_start(myid);

  int numTests;
  std::cout << "Input Number of Tests: ";
  std::cin >> numTests;
  device->start(numTests);

  fprintf(stderr, "Main::about to go to sleep\n");
  int cnt = 0;
  /*  while(true){
    pthread_mutex_lock(&mutex);
    bool doneflag = flag;
    pthread_mutex_unlock(&mutex);
    if (doneflag || cnt++ == 5000) break;
    usleep(100);
    }*/
  sem_wait(&done_sem);
  fprintf(stderr, "Main::starting dumping aurora history\n");
  device->dumpStart();
  sleep(100);
  fclose(fp0);
  fclose(fp1);
}

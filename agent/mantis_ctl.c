#define _GNU_SOURCE
#include <dlfcn.h> 
#include <pipe_mgr/pipe_mgr_intf.h>
#include <unistd.h>
#include <sys/time.h> 
#include <sys/ioctl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <bf_switchd/bf_switchd.h>
#ifdef __cplusplus
}
#endif

#define MAX_HDL_SIZE 10000

bool sleep_before = true;
bool loop_flag = false;
void sigusr1_handler(int signum) {
  printf("Signal %d\n", signum);
  sleep_before = false;
  if(loop_flag) {
    loop_flag = false;
  }
  else {
    loop_flag = true;
  }
}

char* p4rlib_name = "./context/p4r.so";
char* pd_prologue_name = "pd_prologue";
char* pd_dialogue_name = "pd_dialogue";

void * p4rlib = NULL;
int (*pd_prologue_hdl)(uint32_t sess_hdl, dev_target_t pipe_mgr_dev_tgt, uint32_t* handlers);
int (*pd_dialogue_hdl)(uint32_t sess_hdl, dev_target_t pipe_mgr_dev_tgt, uint32_t* handlers);

void load_p4r(){
  printf("Loading...\n");
  if (p4rlib != NULL){
    dlclose(p4rlib);
  }  
  p4rlib = dlopen(p4rlib_name, RTLD_LAZY);
  if (!p4rlib){
      printf(".so not loaded\n");
      printf("Tried to open %s\n", p4rlib_name);
      char * errorstr = dlerror();
      printf("--%s--\n", errorstr);
      exit(1);            
  }
  pd_prologue_hdl = dlsym(p4rlib, pd_prologue_name);
  if (!pd_prologue_hdl){
      printf("pd_prologue not loaded\n");
      exit(1);            
  }
  pd_dialogue_hdl = dlsym(p4rlib, pd_dialogue_name);
  if (!pd_dialogue_hdl){
      printf("pd_dialogue not loaded\n");
      exit(1);
  }
}

bool redefine_loop = false;
void sigusr2_handler(int signum) {
  printf("Signal %d\n", signum);
  redefine_loop = true;
}

void* mantis_thd_func() {

  printf("Launch mantis thread... [PID: %ld]\n", (long)getpid());
  printf("---------------------\n");

  while(sleep_before) {
    usleep(100000);
  }

  struct sched_param params;

  params.sched_priority = sched_get_priority_max(SCHED_FIFO);
  if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &params) != 0) {
    printf("Failed to set priority\n");
  }

  uint32_t sess_hdl = 0;
  uint32_t status_tmp = 0;
  status_tmp = pipe_mgr_init();
  if(status_tmp!=0) {
    printf("ERROR: Status code: %u\n", status_tmp);
    exit(1);
  }
  status_tmp = pipe_mgr_client_init(&sess_hdl);
  if(status_tmp!=0) {
    printf("ERROR: Status code: %u\n", status_tmp);
    exit(1);
  }
  dev_target_t pipe_mgr_dev_tgt;
  pipe_mgr_dev_tgt.device_id = 0;
  pipe_mgr_dev_tgt.dev_pipe_id = 0xffff;

  uint32_t* handlers = (uint32_t*)malloc(sizeof(uint32_t) * MAX_HDL_SIZE);
  memset(handlers, 0, sizeof(uint32_t) * MAX_HDL_SIZE);  

  load_p4r();
  if(!pd_prologue_hdl(sess_hdl, pipe_mgr_dev_tgt, handlers)) {
    printf("Failed initialization\n");
  }

  printf("=== Reaction starts === \n");

  while(loop_flag) {
    if(redefine_loop) {
      load_p4r();
      redefine_loop = false;
    }
    if(!pd_dialogue_hdl(sess_hdl, pipe_mgr_dev_tgt, handlers)) {
      printf("Failed dialogue\n");
      break;
    }
  }

  if (handlers) free(handlers);

  return 0;
}

int main(int argc, char **argv) {

  struct sigaction sa_usr1;
  sa_usr1.sa_handler=&sigusr1_handler;
  sa_usr1.sa_flags=0;
  if(sigaction(SIGUSR1, &sa_usr1, NULL)!=0) {
    exit(1);
  }
  struct sigaction sa_usr2;
  sa_usr2.sa_handler=&sigusr2_handler;
  sa_usr2.sa_flags=0;
  if(sigaction(SIGUSR2, &sa_usr2, NULL)!=0) {
    exit(1);
  }

  bf_switchd_context_t *switchd_ctx;
  if ((switchd_ctx = (bf_switchd_context_t *) calloc(1, sizeof(bf_switchd_context_t))) == NULL) {
    printf("Cannot Allocate switchd context\n");
    exit(1);
  }
  switchd_ctx->install_dir = argv[1];
  switchd_ctx->conf_file = argv[2];
  // Set to false in case one prefers interactive commands
  switchd_ctx->running_in_background = false;

  bf_switchd_lib_init(switchd_ctx);

  pthread_t mantis_thd;
  pthread_create(&mantis_thd, NULL, &mantis_thd_func, NULL);

  pthread_join(mantis_thd, NULL);

  return 0;
}

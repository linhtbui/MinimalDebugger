#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syscall.h>
#include <sys/reg.h>
#include <sys/user.h>
#include <errno.h>
#include <stdarg.h>
#include "breakpoints.h"
#include <stdint.h>

void* print_lines(char* file, int line_num);
void segfault_handler(pid_t pid, char* filepath);

// in the parent process (Debugger)
void run_debugger(pid_t pid, char* filepath){
  sleep(1);    

  char* path = (char*)malloc(sizeof(char)*18 + 1);
  strcpy(path, "/proc/");
  char pid_str[7];
  sprintf(pid_str, "%ld", (long) pid); 
  strcat(path, pid_str);
  strcat(path, "/maps");
  FILE *proc_maps = fopen(path, "r");
  if(proc_maps == NULL){
    fprintf(stderr, "file open failed: %s\n", strerror(errno));
    exit(2);
  }

  char offset[10];
  if(fgets(offset, 10, proc_maps) == NULL){
    fprintf(stderr, "fgets failed\n");
    exit(2);  
  }
  wait(0);

  procmsg("child now at EIP = 0x%08x\n", get_child_eip(pid));
    
  printf("Set a breakpoint\n");
  int target = 0;
  scanf("%d", &target);
  void* line_addr = print_lines(filepath, target);
  char line[128];
  sprintf(line, "%p\n", line_addr);
  memmove(line,line+1, strlen(line));
  memmove(line,line+1, strlen(line));
    
  char full_addr[50];
  sprintf(full_addr,"0x%s%s",offset,line);
  printf ("full: %s\n", full_addr);
  unsigned long addr;
  sscanf(full_addr, "%lx", &addr);
  void* final_addr = (void*) (uintptr_t) addr;
  debug_breakpoint_t* bp = create_breakpoint(pid, final_addr); 
  ptrace(PTRACE_CONT, pid, 0, 0);
  int wait_status;
  wait(&wait_status);
  siginfo_t data;
  if (WIFSTOPPED(wait_status)){
    printf("Program has stopped\n");
    ptrace(PTRACE_GETSIGINFO, pid, 0, &data) ;
    if (data.si_signo == SIGSEGV){
      printf("It has stopped because of a segmentation fault\n");
      segfault_handler(pid, filepath);
    }
  }
  procmsg("child stopped at breakpoint. EIP = 0x%08X\n", get_child_eip(pid));
  printf("Type c to continue\n");
  char buffer = 'a';
  scanf("%c\n", &buffer); 
  while(1){
    procmsg("resuming...\n");
    int rc = resume_from_breakpoint(pid, bp, filepath);
    printf("rc: %d\n", rc);
    if(rc == 0){
      procmsg("Child exited\n");
      break;
    }
    else if (rc == 1){
      continue;
    }
    else {
      procmsg("unexpected: %d\n", rc);
      break;
    }
  }
    
  cleanup_breakpoint(bp);
}

int main(int argc, char** argv){
  
  pid_t pid;

  if(argc < 2){
    fprintf(stderr, "Help: ./grinnellDBG $PROGRAM $PARAMS p_1, p_2, ..., p_n\n");
    exit(1);
  }

  pid = fork();
  if(pid == 0){
    run_target(argv[1], argv + 1);
  }else if(pid > 0){
    run_debugger(pid, argv[1]);
  }else{
    fprintf(stderr, "Fork failed.");
    exit(2);
  }

  return 0;
}

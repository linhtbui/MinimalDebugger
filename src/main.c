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
void run_breakpoint(pid_t pid, char* filepath);
// Function to run the debugger loop and create breakpoints and check for segmentation faults
void run_debugger(pid_t pid, char* filepath){
  sleep(1);    
 
  printf("Press b to set a breakpoint or c to continue\n");
  char target = 0;
  scanf("%c", &target);
  // if the user sets a breakpoint call the breakpoint function otherwise continue the tracked process as usual 
  if(target == 'b'){
    run_breakpoint(pid, filepath);
  }else{
    ptrace(PTRACE_CONT, pid, 0, 0);
    int wait_status;
    wait(&wait_status);
    siginfo_t data;
    // check if the process has stopped 
    if (WIFSTOPPED(wait_status)){
      printf("Program has stopped\n");
      // if it has stopped at a seg fault call the segfault handler
      ptrace(PTRACE_GETSIGINFO, pid, 0, &data) ;
      if (data.si_signo == SIGSEGV){
        printf("It has stopped because of a segmentation fault\n");
        segfault_handler(pid, filepath);
      }
    }
  }
}

// function to handle the breakpoint creation
void run_breakpoint(pid_t pid, char* filepath){
  // finding the offset to be added to the line address
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
  printf("Set a breakpoint\n");
  int target = 0;
  scanf("%d", &target);
  // Get the line address for the breakpoint
  void* line_addr = print_lines(filepath, target);
  char line[128];
  // Add the offset to the line address and convert it to a void*
  sprintf(line, "%p\n", line_addr);
  memmove(line,line+1, strlen(line));
  memmove(line,line+1, strlen(line));
    
  char full_addr[50];
  sprintf(full_addr,"0x%s%s",offset,line);;
  unsigned long addr;
  sscanf(full_addr, "%lx", &addr);
  void* final_addr = (void*) (uintptr_t) addr;
  // create a breakpoint at the address of the line
  debug_breakpoint_t* bp = create_breakpoint(pid, final_addr);
  // continue the process
  ptrace(PTRACE_CONT, pid, 0, 0);
  int wait_status;
  wait(&wait_status);
  siginfo_t data;
  // check if the program has stopped
  if (WIFSTOPPED(wait_status)){
    printf("Program has stopped\n");
    ptrace(PTRACE_GETSIGINFO, pid, 0, &data) ;
    // if it has stopped at a segfault call the segfault handler
    if (data.si_signo == SIGSEGV){
      printf("It has stopped because of a segmentation fault\n");
      segfault_handler(pid, filepath);
    }
  }
  printf("program has stopped at breakpoint\n");
  printf("Type c to continue\n");
  char buffer = 'a';
  scanf("%c\n", &buffer);
  // otherwise the program has stopped at a breakpoint
  while(1){
    printf("resuming...\n");
    // continue execution from the breakpoint
    int rc = resume_from_breakpoint(pid, bp, filepath);
    // check if the process has finished excecuting
    if(rc == 0){
      procmsg("Child exited\n");
      break;
    }
    // otherwise check for other breakpoints and continue execution
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
    // In the child process
    run_target(argv[1], argv + 1);
  }else if(pid > 0){
    // In the parent process
    run_debugger(pid, argv[1]);
  }else{
    fprintf(stderr, "Fork failed.");
    exit(2);
  }

  return 0;
}

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

int main(int argc, char** argv){
  
  pid_t pid;

  if(argc < 2){
    fprintf(stderr, "Help: ./grinnellDBG $PROGRAM $PARAMS p_1, p_2, ..., p_n\n");
    exit(1);
  }

  pid = fork();
  if(pid == 0){
    // in the child process (Debuggee)
    
    char *path = argv[1];
    char **args = argv + 1;

    ptrace(PTRACE_TRACEME, NULL, NULL, NULL);
    if(execvp(path, args) == -1){
      fprintf(stderr, "exec failed");
      exit(2);
    }
  }else if(pid > 0){
    // in the parent process (Debugger)

    sleep(1);    

    char* path = (char*)malloc(sizeof(char)*18 + 1);
    strcpy(path, "/proc/");
    char pid_str[7];
    sprintf(pid_str, "%ld", (long) pid); 
    strcat(path, pid_str);
    strcat(path, "/maps");
    printf("path: %s\n", path);
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
    printf("offset: %s\n", offset);
    wait(0);

    procmsg("child now at RIP = 0x%08x\n", get_child_eip(pid));
    
    struct user_regs_struct *regs = (struct user_regs_struct*)malloc(sizeof(struct user_regs_struct));
    ptrace(PTRACE_GETREGS, pid, 0, regs);
    
    printf("main: 0x%08x\n", main);
        
    printf("RIP: 0x%08x\n", regs->rip);
    printf("Set a breakpoint");

    int target = (int)getchar() - 48;
    void* line_addr = print_lines(argv[1], target);
    char line[128];
    sprintf(line, "%p\n", line_addr);
    memmove(line,line+1, strlen(line));
    memmove(line,line+1, strlen(line));
    printf ("line: %s\n", line);
    
    char full_addr[50];
    sprintf(full_addr,"0x%s%s",offset,line);
    printf ("full: %s\n", full_addr);
    
    unsigned long addr;
    sscanf(full_addr, "%lx", &addr);
    void* final_addr = (void*) (uintptr_t) addr;
    
    debug_breakpoint_t* bp = create_breakpoint(pid, final_addr); 
    procmsg("breakpoint created at %lx\n", final_addr);
    ptrace(PTRACE_CONT, pid, 0, 0);
    wait(0);  

    while(1){
      procmsg("child stopped at breakpoint. EIP = 0x%08X\n", get_child_eip(pid));
      procmsg("resuming...\n");
      int rc = resume_from_breakpoint(pid, bp);
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

    /*
    int status;
    wait(&status);
    while(WIFSTOPPED(status) && !WIFEXITED(status)){
      //unsigned instr = ptrace(PTRACE_PEEKTEXT, pid, regs->rip, 0);

      printf("[*] RIP = 0x%08x.  instr = 0x%08x\n", regs->rip, instr);

      //set breakpoints
      //debug_breakpoint_t* bp = create_breakpoint(pid, (void*)regs->rip);
      
      char input[1];

      ptrace(PTRACE_CONT, pid, NULL, NULL);
      wait(&status);
    }
    */
  }else{
    fprintf(stderr, "Fork failed.");
    exit(2);
  }

  return 0;
}

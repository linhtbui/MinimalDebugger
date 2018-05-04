#include <stdbool.h>
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

void print_lines(int argc, char** argv);

int main(int argc, char** argv){
  
  pid_t pid;
  print_lines(argc, argv);

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
    wait(0);
    procmsg("child now at RIP = 0x%08x\n", get_child_eip(pid));
    
    struct user_regs_struct *regs = (struct user_regs_struct*)malloc(sizeof(struct user_regs_struct));
    ptrace(PTRACE_GETREGS, pid, 0, regs);

    debug_breakpoint_t* bp = create_breakpoint(pid, (void*)0x6f0);
    procmsg("breakpoint created.\n");
    ptrace(PTRACE_CONT, pid, 0, 0);
    wait(0);  

    while(1){
      procmsg("child stopped at breakpoint. EIP = 0x%08X\n", get_child_eip(pid));
      procmsg("resuming...\n");
      int rc = resume_from_breakpoint(pid, bp);

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

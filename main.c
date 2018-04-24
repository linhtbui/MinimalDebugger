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
    int status;
    wait(&status);
    while(WIFSTOPPED(status)){
      struct user_regs_struct *regs = (struct user_regs_struct*)malloc(sizeof(struct user_regs_struct));
      ptrace(PTRACE_GETREGS, pid, 0, regs);
      unsigned instr = ptrace(PTRACE_PEEKTEXT, pid, regs->rip, 0);

      printf("[*] RIP = 0x%08x.  instr = 0x%08x\n", regs->rip, instr);
      
      ptrace(PTRACE_CONT, pid, NULL, NULL);
      wait(&status);
    }
    /*
    while(true){
      int status;
      waitpid(pid, &status, 0);
      if(WIFSTOPPED(status) && WSTOPSIG(status) == SIGSEGV) {
        printf("SEG FAULT\n");
        ptrace(PTRACE_CONT, pid, NULL, NULL);
      }else if(WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
        printf("SIGTRAP\n");
        ptrace(PTRACE_CONT, pid, NULL, NULL);
      }else if(WIFEXITED(status)) {
        printf("inferior exited - debugger terminating...\n");
        exit(0);
      }
    }
    */
  }else{
    fprintf(stderr, "Fork failed.");
    exit(2);
  }

  return 0;
}

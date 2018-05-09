#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/reg.h>
#include <sys/user.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

int get_line(char* file,  unsigned address);

void segfault_handler(pid_t pid, char* filepath){
  struct user_regs_struct regs;
  ptrace(PTRACE_GETREGS, pid, 0, &regs);
  printf("segmentation fault occured on line number: %d\n", get_line(filepath, (regs.rip & 0x000000000FFF)));
  unsigned data = ptrace(PTRACE_PEEKTEXT, pid, regs.rip, 0);
  char line[500];
  sprintf(line, "%s", data);
  printf("LINE: %s\n", line);
  kill(pid, SIGKILL);
  exit(1);
}

void run_target(char* path, char** args)
{

    ptrace(PTRACE_TRACEME, NULL, NULL, NULL);
    
    if(execvp(path, args) == -1){
      fprintf(stderr, "exec failed");
      exit(2);
    }
}

/* Print a message to stdout, prefixed by the process ID
*/
void procmsg(const char* format, ...)
{
    va_list ap;
    fprintf(stdout, "[%d] ", getpid());
    va_start(ap, format);
    vfprintf(stdout, format, ap);
    va_end(ap);
}


long get_child_eip(pid_t pid)
{
    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS, pid, 0, &regs);
    return regs.rip;
}


/* Encapsulates a breakpoint. Holds the address at which the BP was placed
** and the original data word at that address (prior to int3) insertion.
*/
typedef struct debug_breakpoint {
    void* addr;
    unsigned long orig_data;
} debug_breakpoint_t;


/* Enable the given breakpoint by inserting the trap instruction at its 
** address, and saving the original data at that location.
*/
 void enable_breakpoint(pid_t pid, debug_breakpoint_t* bp)
{
    assert(bp);
    ptrace(PTRACE_POKETEXT, pid, bp->addr, (bp->orig_data & 0xFFFFFF00) | 0xCC);
}

 void enable_breakpoint_2(pid_t pid, debug_breakpoint_t* bp)
{
    assert(bp);
}

/* Disable the given breakpoint by replacing the byte it points to with
** the original byte that was there before trap insertion.
*/
void disable_breakpoint(pid_t pid, debug_breakpoint_t* bp)
{
  assert(bp);
  unsigned data = ptrace(PTRACE_PEEKTEXT, pid, bp->addr, 0);
  assert((data & 0xFF) == 0xCC);
  ptrace(PTRACE_POKETEXT, pid, bp->addr, bp->orig_data);
  data = ptrace(PTRACE_PEEKTEXT, pid, bp->addr, 0);
  assert((data & 0xFF) != 0xCC);
}


debug_breakpoint_t* create_breakpoint(pid_t pid, void* addr)
{
    debug_breakpoint_t* bp = malloc(sizeof(*bp));
    bp->addr = addr;
    bp->orig_data = ptrace(PTRACE_PEEKTEXT, pid, bp->addr, 0);
    enable_breakpoint(pid, bp);
    return bp;
}


void cleanup_breakpoint(debug_breakpoint_t* bp)
{
    free(bp);
}


int resume_from_breakpoint(pid_t pid, debug_breakpoint_t* bp, char* filepath)
{
    struct user_regs_struct regs;
    int wait_status;
    ptrace(PTRACE_GETREGS, pid, 0, &regs);
    /* Make sure we indeed are stopped at bp */
    assert(regs.rip == (long) bp->addr + 1);

    /* Now disable the breakpoint, rewind EIP back to the original instruction
    ** and single-step the process. This executes the original instruction that
    ** was replaced by the breakpoint.
    */
    regs.rip = (long) bp->addr;
    ptrace(PTRACE_SETREGS, pid, 0, &regs);
    disable_breakpoint(pid, bp);
    if (ptrace(PTRACE_SINGLESTEP, pid, 0, 0) < 0) {
      perror("ptrace");
      return -1;
    }
    wait(&wait_status);
    siginfo_t data;

    if (WIFEXITED(wait_status)) {
       return 0;
    }
    // Re-enable the breakpoint and let the process run.
     enable_breakpoint_2(pid, bp);
    if (ptrace(PTRACE_CONT, pid, 0, 0) < 0) {
        perror("ptrace");
        return -1;
    }
    wait(&wait_status);

    if (WIFSTOPPED(wait_status)){
      printf("Program has stopped\n");
      ptrace(PTRACE_GETSIGINFO, pid, 0, &data) ;
      if (data.si_signo == SIGSEGV){
        printf("It has stopped because of a segmentation fault\n");
        segfault_handler(pid, filepath);
      }
    }
    if (WIFEXITED(wait_status))
      return 0;
    else if (WIFSTOPPED(wait_status)) {
      return 1;
    }
    else
      return -1;
}



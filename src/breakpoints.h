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


/* Run a target process in tracing mode by exec()-ing the given program name.
*/
void run_target(const char* programname)
{
    procmsg("target started. will run '%s'\n", programname);

    /* Allow tracing of this process */
    if (ptrace(PTRACE_TRACEME, 0, 0, 0) < 0) {
        perror("ptrace");
        return;
    }

    /* Replace this process's image with the given program */
    execl(programname, programname, 0);
}


long get_child_eip(pid_t pid)
{
    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS, pid, 0, &regs);
    return regs.rip;
}


void dump_process_memory(pid_t pid, unsigned from_addr, unsigned to_addr)
{
    procmsg("Dump of %d's memory [0x%08X : 0x%08X]\n", pid, from_addr, to_addr);
    for (unsigned addr = from_addr; addr <= to_addr; ++addr) {
        unsigned word = ptrace(PTRACE_PEEKTEXT, pid, addr, 0);
        printf("  0x%08X:  %02x\n", addr, word & 0xFF);
    }
}

typedef struct debug_breakpoint{
  void* addr;
  unsigned orig_data;
}debug_breakpoint_t;

void enable_breakpoint(pid_t pid, debug_breakpoint_t* bp){
  assert(bp);
  bp->orig_data = ptrace(PTRACE_PEEKTEXT, pid, bp->addr, 0);
  ptrace(PTRACE_POKETEXT, pid, bp->addr, (bp->orig_data & 0xFFFFFF00) | 0xCC);
}

void disable_breakpoint(pid_t pid, debug_breakpoint_t* bp){
  assert(bp);
  unsigned data = ptrace(PTRACE_PEEKTEXT, pid, bp->addr, 0);
  assert((data & 0XCC) == 0xCC);
  ptrace(PTRACE_POKETEXT, pid, bp->addr, (data & 0xFFFFFF00) | (bp->orig_data & 0xFF));
}

debug_breakpoint_t* create_breakpoint(pid_t pid, void* addr){
  debug_breakpoint_t* bp = malloc(sizeof(*bp));
  bp->addr = addr;
  enable_breakpoint(pid, bp);
  return bp;
}

void cleanup_breakpoint(debug_breakpoint_t* bp){
  free(bp);
}

int resume_from_breakpoint(pid_t pid, debug_breakpoint_t* bp)
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

    if (WIFEXITED(wait_status)) {
        return 0;
    }

    /* Re-enable the breakpoint and let the process run.
    */
    enable_breakpoint(pid, bp);

    if (ptrace(PTRACE_CONT, pid, 0, 0) < 0) {
        perror("ptrace");
        return -1;
    }
    wait(&wait_status);

    if (WIFEXITED(wait_status))
        return 0;
    else if (WIFSTOPPED(wait_status)) {
        return 1;
    }
    else
        return -1;
}
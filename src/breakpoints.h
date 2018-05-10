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
#include <stdint.h>

typedef struct addr_info{
  char* min_addr;
  char* max_addr;
  int read_bit;
  int write_bit;
  int execute_bit; 
}addr_info_t;

typedef struct proc_maps{
  addr_info_t* data;
  struct proc_maps *next;
}proc_maps_t;

int get_line(char* file,  unsigned address);

void segfault_handler(pid_t pid, char* filepath){
  
  char* path = (char*)malloc(sizeof(char)*18 + 1);
  strcpy(path, "/proc/");
  char pid_str[7];
  sprintf(pid_str, "%ld", (long) pid);
  strcat(path, pid_str);
  strcat(path, "/maps");
  FILE *proc_file = fopen(path, "r");
  if(proc_file == NULL){
    fprintf(stderr, "file open failed: %s\n", strerror(errno));
    exit(2); 
  }

  char *line = NULL; 
  proc_maps_t* current = NULL;
  size_t *line_size;
  *line_size = 31;
  proc_maps_t* proc_info;
  while(getline(&line, line_size, proc_file) != -1){
    addr_info_t* mapped_range = (addr_info_t*)malloc(sizeof(addr_info_t));
    proc_maps_t* maps = (proc_maps_t*)malloc(sizeof(proc_maps_t));
    
    if(current != NULL) {
      current->next = maps;
    } else {
      proc_info = maps;
    }
    maps->next = NULL;
    maps->data = mapped_range;
    current = maps;
    
    mapped_range->min_addr = strtok(line, "-"); 
    mapped_range->max_addr = strtok(NULL, " ");
    char* permissions = strtok(NULL, " ");

    printf("permissions: %s\n", permissions);
    
    if(permissions[0] == 'r'){
      mapped_range->read_bit = 1;
    }
    if(permissions[1] == 'w'){
      mapped_range->write_bit = 1;
    }
    if(permissions[2] == 'x'){
      mapped_range->execute_bit = 1;  
    }

  }
      
  siginfo_t data;
  ptrace(PTRACE_GETSIGINFO, pid, 0, &data);
  if(data.si_addr == NULL){
    printf("Dereferenced a NULL pointer!\n");
  }
  struct user_regs_struct regs;
  ptrace(PTRACE_GETREGS, pid, 0, &regs);
  printf("segmentation fault occured on line number: %d\n", get_line(filepath, (regs.rip & 0x000000000FFF)));
  
  proc_maps_t* cur = proc_info;
  int map_bit = 0;
  // Check if in mapped memory
  while(cur->next != NULL){
    void* min;
    void* max;
    char full_addr[16];
    unsigned long min_addr;
    unsigned long max_addr;
    sprintf(full_addr, "0x%s", cur->data->min_addr);
    sscanf(full_addr, "%lx", &min_addr);
    sprintf(full_addr, "0x%s", cur->data->max_addr);
    sscanf(full_addr, "%lx", &max_addr);
    
    if(regs.rip >= min_addr && regs.rip <= max_addr){
      map_bit = 1;
      break; 
    }
    cur = cur->next;
  }

  printf("map_bit: %d\n", map_bit);
  if(map_bit){
    printf("readable: %d\n", cur->data->read_bit);
    printf("writeable: %d\n", cur->data->write_bit);
    printf("executable: %d\n", cur->data->execute_bit);
  }
      // Check permissions (if in mapped mem)
  
  printf("RIP: %p\n", regs.rip);
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



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

// This struct holds the information for each line within
// the /proc/<PID>/maps file. That way, we can find out 
// which regions of mapped memory have which permissions.
typedef struct addr_info{
  char* min_addr;
  char* max_addr;
  int read_bit;
  int write_bit;
  int execute_bit; 
}addr_info_t;

// This struct allows us to create a linked list for data
// in the previous struct. 
typedef struct proc_maps{
  addr_info_t* data;
  struct proc_maps *next;
}proc_maps_t;

// This function returns the line number in the source code,
// given the address.
int get_line(char* file,  unsigned address);

// This function pushes values onto our /proc/<PID>/maps
// linked list.
void push(proc_maps_t** node, addr_info_t* data){
  proc_maps_t* temp = (proc_maps_t*)malloc(sizeof(void*) * 2);
  temp->data = data;
  temp->next = *node;
  *node = temp;
}

// This function is called by the debugging process when the
// child throws a SIGSEGV signal. It does all the reporting of
// segfault information, then terminates the entire program 
// (both parent and child).
void segfault_handler(pid_t pid, char* filepath){

// This code loads the /proc/<PID>/maps file into 'proc_file'
// for processing.
  char* path = (char*)malloc(sizeof(char)*18 + 1);
  strcpy(path, "/proc/");
  char pid_str[7];
  sprintf(pid_str, "%ld", (long) pid);
  strcat(path, pid_str);
  strcat(path, "/maps");
  FILE *proc_file = fopen(path, "r");
  
  // error check
  if(proc_file == NULL){
    fprintf(stderr, "file open failed: %s\n", strerror(errno));
    exit(2); 
  }

  // These two lines load the register values of the child
  // at the segfault into 'regs', using PTRACE_GETREGS. 
  // From here, we can extract the instruction pointer to find
  // the line number that caused the segfault.
  struct user_regs_struct regs;
  ptrace(PTRACE_GETREGS, pid, 0, &regs);
  
  // This line prints the line number that caused the segfault,
  // by passing the instruction pointer into the aforementioned
  // function, 'get_line()'
  printf("segmentation fault occured on line number: %d\n", get_line(filepath, (regs.rip & 0x000000000FFF)));
  
  // These two lines load the signal info into 'data', so that we
  // may find out if the segfault was triggered because of unmapped
  // memory, dereferencing a NULL pointer, or otherwise.
  siginfo_t data;
  ptrace(PTRACE_GETSIGINFO, pid, 0, &data);

  // This 'if' statement first checks if the segfault was triggered
  // by an access to unmapped memory.
  if(data.si_code == SEGV_MAPERR){
    
    // If it was triggered by unmapped memory, report this to the user.
    printf("You tried to access unmapped memory");
    if(data.si_addr == NULL){
      // Additionally, if the unmapped memory was a NULL pointer, report this
      // AND the line number with 'get_line()'
      printf(", specifically, you dereferenced a NULL pointer on line %d!\n", get_line(filepath, (regs.rip & 0x000000000FFF)));
    }else{
      // Otherwise, simply report to the user that the program tried
      // access unmapped memory on the line number.
      printf(" on line %d and address %p.\n", get_line(filepath, (regs.rip & 0x000000000FFF)), data.si_addr);
    }
  }else{

    // If the segfault was NOT caused by an access to unmapped memory, then
    // this 'else' block executes

    // first, we create a linked list of all regions of mapped memory and their
    // permissions with the aforementioned 'proc_maps_t' and 'addr_info_t' structs.
    // the purpose of this is to report to the user the permissions,
    // so that they may understand which permissions violation they made
    char *line = NULL; 
    proc_maps_t* proc_maps = NULL; 
    size_t *line_size = (size_t*)malloc(sizeof(size_t));
    *line_size = 31;
    while(getline(&line, line_size, proc_file) != -1){
      addr_info_t* mapped_range = (addr_info_t*)malloc(sizeof(char*)*2 + sizeof(int) * 3);
      
      // extracts minimum and maximum bounds for each region in mapped memory
      mapped_range->min_addr = strtok(line, "-"); 
      mapped_range->max_addr = strtok(NULL, " ");
      char* permissions = strtok(NULL, " ");
      
      // adds permissions to the 'mapped_range' struct for region
      // in mapped memory
      if(permissions[0] == 'r'){
        mapped_range->read_bit = 1;
      }else{
        mapped_range->read_bit = 0;
      }
      if(permissions[1] == 'w'){
        mapped_range->write_bit = 1;
      }else{
        mapped_range->write_bit = 0;
      }
      if(permissions[2] == 'x'){
        mapped_range->execute_bit = 1;  
      }else{
        mapped_range->execute_bit = 0;
      }
      
      // add 'mapped_range' to 'proc_maps'
      push(&proc_maps, mapped_range);
      line = NULL;
    }
 
    // Next, we compare the address that triggered the segfault
    // to each region in mapped memory (through our 'proc_maps' 
    // linked list), to find out the permissions of that address.
    proc_maps_t* cur = proc_maps;
    while(cur != NULL){
      void* min;
      void* max;
      char full_addr[16];
      unsigned long min_addr;
      unsigned long max_addr;
      sprintf(full_addr, "0x%s", cur->data->min_addr);
      sscanf(full_addr, "%lx", &min_addr);
      sprintf(full_addr, "0x%s", cur->data->max_addr);
      sscanf(full_addr, "%lx", &max_addr);
      min = (void*) (uintptr_t) min_addr;
      max = (void*) (uintptr_t) max_addr;

         // if the memory address that triggered the segfault
         // lies within 'cur's memory bracket, then break
      if(data.si_addr >= min && data.si_addr <= max){
        break; 
      }
      cur = cur->next;
    }

    // Next, we report the permissions of 'cur's memory bracket
    printf("The permissions of the memory you tried to access are: '");
    if(cur->data->read_bit){
      printf("r");
    }else{
      printf("-");
    }
    if(cur->data->write_bit){
      printf("w");
    }else{

      printf("-");
    }
    if(cur->data->execute_bit){
      printf("x");
    }else{
      printf("-");
    }
    printf("'. Look at line %d to see what you did.\n", get_line(filepath, (regs.rip & 0x000000000FFF)));
  }

  // Kill child process
  kill(pid, SIGKILL);

  // Exit the program
  exit(1);
}


// This command executes the debuggee code, while allowing
// the parent to trace it with PTRACE_TRACEME
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

// Returns the instruction pointer of the child
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
** address
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

// This function creates a breakpoint in the child <PID> at
// 'addr' and stores the original data for the breakpoint
debug_breakpoint_t* create_breakpoint(pid_t pid, void* addr)
{
    debug_breakpoint_t* bp = malloc(sizeof(*bp));
    bp->addr = addr;
    bp->orig_data = ptrace(PTRACE_PEEKTEXT, pid, bp->addr, 0);
    enable_breakpoint(pid, bp);
    return bp;
}

// This function deletes the breakpoint, freeing the memory.
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
    // check if the program finished executing
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
    // check if the program has stopped
    if (WIFSTOPPED(wait_status)){
      printf("Program has stopped\n");
      ptrace(PTRACE_GETSIGINFO, pid, 0, &data) ;
      // check if it has stopped because of a segmentation fault
      if (data.si_signo == SIGSEGV){
        printf("It has stopped because of a segmentation fault\n");
        // call the segfault handler
        segfault_handler(pid, filepath);
      }
    }
    // if it has finished executing return 0 otherwise 1 and -1 if an error occured
    if (WIFEXITED(wait_status))
      return 0;
    else if (WIFSTOPPED(wait_status)) {
      return 1;
    }
    else
      return -1;
}



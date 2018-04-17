int main(int argc, char** argv){
  
  pid_t child_pid;

  if(argc < 2){
    fprintf(stderr, "Help: ./grinnellDBG $PROGRAM $PARAMS p_1, p_2, ..., p_n");
    exit(1);
  }

  child_pid = fork();
  if(child_pid == 0){
    // in the child process (Debuggee)
  }else if(child_pid > 0){
    // in the parent process (Debugger)
  }else{
    fprintf(stderr, "Fork failed.");
    exit(2);
  }

  return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>



int main(){
  printf("1\n");
  printf("2\n");
  char *string = "hello world\n";
  printf("3\n");
  string[2] = 'd';
  printf("4\n");
}

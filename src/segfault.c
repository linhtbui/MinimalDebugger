#include <stdio.h>
#include <stdlib.h>
#include <signal.h>



int main(){
  printf("1\n");
  printf("2\n");
  int *p = NULL;
  printf("3\n");
  *p = 3;
  printf("4\n");
}

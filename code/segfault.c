#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

void test(){
  printf("in TEST\n");
}

int main(){
  printf("function at %p\n", test);
}

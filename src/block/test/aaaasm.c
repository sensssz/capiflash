#include <stdio.h>
int main () {
int array[] = { 1, 2, 3 };
 printf ( "array = [ %d, %d, %d ]\n", array[0], array[1], array[2]);
   asm ("ABS %0, %1 \n" 
   :"+r"(array[1]) 
   :"r"(array[2]) );
   printf ( "array = [ %d, %d, %d ]\n", array[0], array[1], array[2]);
return 0;
}



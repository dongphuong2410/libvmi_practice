#include <stdio.h>
#include <stdlib.h>

#ifdef __unix__
#  include <unistd.h>
#elif define _WIN32
#  include <windows.h>
#define sleep(x) Sleep(1000 * x)
#endif

void main(void) {
    int *test = malloc(sizeof(int));
    *test = 1;

    do {
        printf("My PID is %i. The test pointer is at 0x%1x on the stack, it points to ??? on the heap where the value is %i.\n"
                , getpid(), &test, *test);
        sleep(5);
    } while (1);
    free(test);
}

#include <stdio.h>
#include <unistd.h>

int main() {
    printf("From 1 to 21 counter starts, but hits alarm at 10.\n");
    int i;
    for (i = 1; i < 21; i++) {
        printf("Background process Timer: %d\n", i);
        sleep(1); 
    }
    return 0;
}
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void sieve(int left[2]) {
    close(left[1]);

    int start, right[2], buf[1];
    // read the first number from the left pipe, which is the first prime
    // if read return 0, it means the left pipe is closed, exit
    if (read(left[0], &start, sizeof(int)) == 0) {
        close(left[0]);
        exit(0);
    }
    fprintf(1, "prime %d\n", start);

    // crate new pipe to pass values to right
    pipe(right);
    int pid = fork();
    if (pid == 0) {
        sieve(right);
        exit(0);
    } else if (pid > 0) {
        close(right[0]);
        while (read(left[0], &buf, sizeof(int))) {
            if (buf[0] % start != 0) {
                write(right[1], &buf, sizeof(int));
            }
        }
        close(left[0]);
        close(right[1]);
        wait(0);
        exit(0);
    } else {
        fprintf(2, "fork failed\n");
        exit(1);
    }
}

int main(int argc, char **argv) {
    int p[2];
    pipe(p);

    int pid = fork();
    if (pid == 0) {
        sieve(p);
        exit(0);
    } else if (pid > 0) {
        close(p[0]);
        int i;
        for (i = 2; i <= 35; i ++) {
            write(p[1], &i, sizeof(int));
        }
        close(p[1]);
        wait(0);
        exit(0);
    } else {
        fprintf(2, "fork failed\n");
        exit(1);
    }
}

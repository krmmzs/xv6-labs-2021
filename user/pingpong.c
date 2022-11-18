#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void) {
    int c2p[2], p2c[2];
    pipe(c2p);
    pipe(p2c);
    char buf[1];

    int pid = fork();
    if (pid == 0) {
        close(p2c[1]);
        close(c2p[0]);
        read(p2c[0], buf, 4);
        fprintf(1, "%d: received %s\n", getpid(), buf);
        close(p2c[0]);
        write(c2p[1], "pong", 4);
        close(c2p[1]);
        exit(0);
    } else if(pid > 0) {
        close(p2c[0]);
        close(c2p[1]);
        write(p2c[1], "ping", 4);
        close(p2c[1]);
        read(c2p[0], buf, 5);
        fprintf(1, "%d: received %s\n", getpid(), buf);
        close(c2p[0]);
        exit(0);
    } else {
        fprintf(2, "fork failed\n");
        exit(1);
    }
}

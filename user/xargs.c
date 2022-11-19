#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

int main(int argc, char **argv) {
    int buf_idx = 0;
    char buf, arg[MAXARG];
    // A list of all parameters, an array of string pointers,
    // including the parameters passed in by argv and the parameters
    // read by stdin
    char *args[MAXARG];
    if (argc < 2) {
        fprintf(2, "Usage: xargs command\n");
        exit(1);
    }
    if (argc > MAXARG) {
        fprintf(2, "xargs: too many arguments\n");
        exit(1);
    }
    // Copy the parameters passed in by argv to args
    // except the first one (the command name)
    for (int i = 0; i < argc; i ++) {
        args[i] = argv[i + 1];
    }
    args[argc - 1] = arg;
    args[argc] = 0;
    // To read individual lines of input, read a character at a time until a newline ('\n') appears.
    while (read(0, &buf, sizeof(buf)) > 0) {
        if (buf == '\n' || buf == ' ') {
            arg[buf_idx] = '\0';

            int pid = fork();
            if (pid == 0) {
                exec(args[0], args);
            } else if (pid > 0) {
                wait(0);
                buf_idx = 0;
            } else {
                fprintf(2, "xargs: fork failed\n");
                exit(1);
            }
        } else {
            arg[buf_idx ++] = buf;
        }
    }
    exit(0);
}

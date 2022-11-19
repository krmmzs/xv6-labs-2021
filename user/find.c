#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char *fmtname(char* path) {
    char* p;

    // Find first character after last slash.
    for (p = path + strlen(path); p >= path && *p != '/'; p --);
    return p + 1;
}

// find all the files in a directory tree with a specific name
void find(char *path, char *file) {
    char buf[512], *p;
    // get the file descriptor of the directory
    int fd = open(path, 0);
    struct stat st;
    struct dirent de;

    if (fd < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    // get the stat of the directory
    if(fstat(fd, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type) {
        // Determine if the path is a file, if it is a file,
        // directly determine whether the file name is the same
        // as the target file name.
        case 2:
            if(strcmp(fmtname(path), file) == 0) {
                fprintf(1, "%s\n", path);
            }
        break;
        // Determine if the path is a directory, if it is a directory,
        // append the file name to the path and recursively call the
        // find function.
        case 1:
            strcpy(buf, path);
            p = buf + strlen(buf);
            *p++ = '/';
            while (read(fd, &de, sizeof(de)) == sizeof(de)) {
                if (de.inum == 0 || !strcmp(de.name, ".") || !strcmp(de.name, "..")) continue;
                memmove(p, de.name, DIRSIZ);
                if(stat(buf, &st) < 0){
                    printf("find: cannot stat %s\n", buf);
                    continue;
                }
                find(buf, file);
            }
        break;
    }
    close(fd);
}

int main(int argc, char **argv) {
    if(argc < 3){
        fprintf(2, "Usage: find path file\n");
        exit(1);
    }
    find(argv[1], argv[2]);
    exit(0);
}

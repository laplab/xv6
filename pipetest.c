#include "types.h"
#include "user.h"
#include "pipe.h"

enum
{
    FD_WRITE = 1,
    FD_READ = 0,
    PROC_COUNT = 50 // should be less than NPROC == 64
};

void close_pipe(int fd[2]) {
    close(fd[FD_READ]);
    close(fd[FD_WRITE]);
}

int main(int argc, char *argv[]) {
    int fd[2];
    pipe(fd);

    int pids[PROC_COUNT];
    for (int i = 0; i < PROC_COUNT; i++) {
        pids[i] = fork();
        if (!pids[i]) {
            char buf[PIPESIZE];
            for (int j = 0; j < PIPESIZE; j++) {
                buf[j] = 'a' + i;
            }

            write(fd[FD_WRITE], buf, sizeof(buf));
            
            close_pipe(fd);
            exit();
        }
    }

    printf(1, "INFO: started forks\n");

    int success = 1;
    char buf[PIPESIZE];
    for (int i = 0 ; i < PROC_COUNT; i++) {
        read(fd[FD_READ], buf, sizeof(buf));
        for (int j = 0; j < PIPESIZE && success; j++) {
            success = buf[0] == buf[j];
        }
    }

    printf(1, "INFO: finished reading\n");

    while(wait() >= 0);

    printf(1, "INFO: waited for all forks\n");

    if (success) {
        printf(1, "PASS: no data interleave found in pipe\n");
    } else {
        printf(1, "FAIL: pipe data interleave detected");
    }

    close_pipe(fd);
    exit();
}


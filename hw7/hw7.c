#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#define READ 1
#define WRITE 2
#define ERROR 4
#define OCLOSE 5
#define ICLOSE 6
#define WOPEN 7
#define ROPEN 8
#define DUP 9
#define TIME 10
#define EXEC 11
#define CHDIR 12
#define PID 13
#define GETLINE 14

int NPROC, NICE, WAIT;

void print_time_info(int proc, struct rusage *ru);

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: hw7 NPROC NICE WAIT\n");
        exit(EXIT_FAILURE);
    }

    NPROC = atoi(argv[1]);
    NICE = atoi(argv[2]);
    WAIT = atoi(argv[3]);

    int i;
    long j;
    int wstatus;
    struct rusage rus[NPROC];
    long long times[NPROC];
    long long total_time = 0;
    memset(rus, 0, sizeof(rus));
    pid_t child, childs[NPROC];

    for (i = 0; i < NPROC; i++) {
        switch ((child = fork())) {
            case -1:
                perror("Failed to fork");
                exit(EXIT_FAILURE);
                break;
            case 0:
                if (i == 0) {
                    if (nice(NICE) < 0) {
                        perror("Failed to adjust nice");
                        exit(EXIT_FAILURE);
                    }
                }
                for (j = 0; j < (1LL << 50); j++) {}
                exit(EXIT_SUCCESS);
                break;
            default:
                childs[i] = child;
                break;
        }
    }
    sleep(WAIT);
    signal(SIGINT, SIG_IGN);
    kill(0, SIGINT);
    for (i = 0; i < NPROC; i++) {
        do {
            if (wait4(childs[i], &wstatus, 0, rus + i) < 0) {
                perror("Failed to wait");
                exit(EXIT_FAILURE);
            }
        } while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));
    }

    for (i = 0; i < NPROC; i++) {
        times[i] = (rus[i].ru_utime.tv_sec + rus[i].ru_stime.tv_sec) * 1e6
                   + (rus[i].ru_utime.tv_usec + rus[i].ru_stime.tv_usec);
        total_time += times[i];
        print_time_info(i, rus + i);
    }
    printf("Spawning %d processes and waiting %d seconds, first child process will have nice %d\n", NPROC, WAIT, NICE);
    printf("Total CPU time was %lld us\n", total_time);
    printf("Task 0 CPU time was %lld us\n", times[0]);
    printf("Task 0 received %f%% of total CPU\n", ((double)times[0]/total_time)*100);
}

void print_time_info(int proc, struct rusage *ru) {
    fprintf(stdout, "%d, %ld.%06ld, %ld.%06ld\n", proc,
            ru->ru_utime.tv_sec, ru->ru_utime.tv_usec, ru->ru_stime.tv_sec, ru->ru_stime.tv_usec);
}
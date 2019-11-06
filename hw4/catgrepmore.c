#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUF_SIZE 4096

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
#define PIPE 15
#define PCLOSE 16
#define PWRITE 17
#define WPID 18
#define SIGACT 19

#define BUF_SIZE 4096

void check_error(int fd, int n, char *s, int type, int return_code);
void run_grep(pid_t* grep_pid, char* cmd[]);
void run_more(pid_t* more_pid, char* cmd[]);
void int_handler(int sig);
void cat_grep_more(char* pattern, char* filename);

int cat_grep_pipe[2];
int grep_more_pipe[2];
sigjmp_buf jb;

int totalFileCnt;
long long totalBytes;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: catgrepmore pattern infile1 [...infile2...]\n");
        exit(EXIT_FAILURE);
    }
    struct sigaction sa;
    sa.sa_handler = int_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags=0;
    check_error(sigaction(SIGINT, &sa, NULL), 0, "SIGINT", SIGACT, 0);
    check_error(sigaction(SIGPIPE, &sa, NULL), 0, "SIGPIPE", SIGACT, 0);
    int idx;
    for (idx = 2; idx < argc; idx++) {
        totalFileCnt++;
        cat_grep_more(argv[1], argv[idx]);
    }
}

void cat_grep_more(char* pattern, char* filename) {
    int fd_r, bytes_read, bytes_written, total_bytes_written;
    char buf[BUF_SIZE];
    bool skip = false;
    pid_t grep_pid, more_pid;
    int grep_status, more_status;

    char* grep_cmd[] = {"grep", pattern, NULL};
    char* more_cmd[] = {"more", NULL};

    check_error(pipe(cat_grep_pipe), 0, "GREP", PIPE, 0);
    check_error(pipe(grep_more_pipe), 0, "MORE", PIPE, 0);

    run_grep(&grep_pid, grep_cmd);
    run_more(&more_pid, more_cmd);

    check_error(close(cat_grep_pipe[0]), 0, "GREP", PCLOSE, 0);
    check_error(close(grep_more_pipe[0]), 0, "MORE", PCLOSE, 0);
    check_error(close(grep_more_pipe[1]), 0, "MORE", PCLOSE, 0);

    // write to pipe here
    check_error((fd_r = open(filename, O_RDONLY)), 0, filename, ROPEN, 0);
    while ((bytes_read = read(fd_r, buf, BUF_SIZE)) != 0) {
        if (sigsetjmp(jb, 1) != 0) break;
        check_error(fd_r, bytes_read, filename, READ, 0);
        bytes_written = 0, total_bytes_written = 0;
        while (bytes_written < bytes_read) {
            bytes_written = write(cat_grep_pipe[1], buf + total_bytes_written, bytes_read);
            check_error(cat_grep_pipe[1], bytes_written, "", PWRITE, 0);
            bytes_read -= bytes_written;
            total_bytes_written += bytes_written;
        }
        totalBytes += total_bytes_written;
    }

    if (!skip) {
        if (fd_r) check_error(fd_r, close(fd_r), filename, OCLOSE, 0);
        check_error(close(cat_grep_pipe[1]), 0, "GREP", PCLOSE, 0);
        skip = true;
        check_error(waitpid(grep_pid, &grep_status, 0), 0, "GREP", WPID, 0);
        check_error(waitpid(more_pid, &more_status, 0), 0, "MORE", WPID, 0);
    }
}

void run_grep(pid_t* grep_pid, char* cmd[]) {
    pid_t pid;
    switch(pid = fork()) {
        case -1:
            perror("Failed to fork GREP");
            exit(EXIT_FAILURE);
        case 0:
            check_error(dup2(cat_grep_pipe[0], STDIN_FILENO), 0, "", DUP, 0);
            check_error(dup2(grep_more_pipe[1], STDOUT_FILENO), 0, "", DUP, 0);
            check_error(close(cat_grep_pipe[0]), 0, "GREP", PCLOSE, 0);
            check_error(close(cat_grep_pipe[1]), 0, "GREP", PCLOSE, 0);
            check_error(close(grep_more_pipe[0]), 0, "MORE", PCLOSE, 0);
            check_error(close(grep_more_pipe[1]), 0, "MORE", PCLOSE, 0);
            execvp(cmd[0], cmd);
            fprintf(stderr, "Failed to run grep execvp: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        default:
            *grep_pid = pid;
            break;
    }
}

void run_more(pid_t* more_pid, char* cmd[]) {
    pid_t pid;
    switch(pid = fork()) {
        case -1:
            perror("Failed to fork MORE");
            exit(EXIT_FAILURE);
        case 0:
            check_error(dup2(grep_more_pipe[0], STDIN_FILENO), 0, "", DUP, 0);
            check_error(close(cat_grep_pipe[0]), 0, "GREP", PCLOSE, 0);
            check_error(close(cat_grep_pipe[1]), 0, "GREP", PCLOSE, 0);
            check_error(close(grep_more_pipe[0]), 0, "MORE", PCLOSE, 0);
            check_error(close(grep_more_pipe[1]), 0, "MORE", PCLOSE, 0);
            execvp(cmd[0], cmd);
            fprintf(stderr, "Failed to run more execvp: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        default:
            *more_pid = pid;
            break;
    }
}

void int_handler(int sig) {
    if (sig == SIGINT) fprintf(stderr, "%d files and %lld bytes processed.\n", totalFileCnt, totalBytes);
    siglongjmp(jb, 1);
}

void check_error(int fd, int n, char *s, int type, int return_code) {
    if (fd < 0 || n < 0) {
        switch (type) {
            case WRITE:
                fprintf(stderr, "Can't write file %s: %s\n", s, strerror(errno));
                break;
            case READ:
                fprintf(stderr, "Can't read file %s: %s\n", s, strerror(errno));
                break;
            case OCLOSE:
                fprintf(stderr, "Can't close output file %s: %s\n", s, strerror(errno));
                break;
            case ICLOSE:
                fprintf(stderr, "Can't close input file %s: %s\n", s, strerror(errno));
                break;
            case WOPEN:
                fprintf(stderr, "Can't open file %s for writing: %s\n", s, strerror(errno));
                break;
            case ROPEN:
                fprintf(stderr, "Can't open file %s for reading: %s\n", s, strerror(errno));
                break;
            case DUP:
                fprintf(stderr, "Can't duplicate file %s to fd table: %s\n", s, strerror(errno));
                break;
            case TIME:
                fprintf(stderr, "Can't retrieve current time: %s\n", strerror(errno));
                break;
            case EXEC:
                fprintf(stderr, "Can't exec %s: %s\n", s, strerror(errno));
                break;
            case CHDIR:
                fprintf(stderr, "Can't change current directory to %s: %s\n", s, strerror(errno));
                break;
            case PID:
                fprintf(stderr, "Failed to wait child PID: %s\n", strerror(errno));
                break;
            case GETLINE:
                fprintf(stderr, "Failed to getline: %s\n", strerror(errno));
                break;
            case PIPE:
                fprintf(stderr, "Failed to create %s pipe: %s\n", s, strerror(errno));
                break;
            case PCLOSE:
                fprintf(stderr, "Can't close %s pipe: %s\n", s, strerror(errno));
                break;
            case PWRITE:
                fprintf(stderr, "Can't write to pipe: %s\n", strerror(errno));
                break;
            case WPID:
                fprintf(stderr, "Failed to wait %s pid: %s\n", s, strerror(errno));
                break;
            case SIGACT:
                fprintf(stderr, "Failed to attach handler to signal %s: %s\n", s, strerror(errno));
                break;
            default:
                break;
        }
        if (return_code != 0) exit(return_code);
        exit(EXIT_FAILURE);
    }
}
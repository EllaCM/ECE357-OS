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

int run_cd(char *path);

void check_error(int fd, int n, char *s, int type, int return_code);

bool check_pound(const char *line);

char **parse_cmd(char *line, mode_t **redirIn, mode_t **redirOut, int **mode, int *cmdLength, int *redirLoc);

int run_cmd(char **parsedCmd, mode_t *redirIn, mode_t *redirOut, int *mode, int cmdLength, int redirLoc);

int run_pwd();

void run_exit(char *code, int return_code);

void print_time_info(struct timeval *result, struct rusage *ru);

void print_info(pid_t pid, int wstatus, int *return_code);

void get_time(struct timeval *time);

int shell_redirect_fd = -1;
char* shell_redirect;

int main(int argc, char *argv[]) {
    FILE *infile;
    if (argc > 1) {
        shell_redirect_fd = open(argv[1], O_RDONLY);
        shell_redirect = argv[1];
        check_error(shell_redirect_fd, 0, argv[1], ROPEN, 0);
        infile = fdopen(shell_redirect_fd, "r");
    } else {
        infile = fdopen(STDIN_FILENO, "r");
    }
    if (infile == NULL) {
        perror("Unable to open file");
        exit(EXIT_FAILURE);
    }
    char *line = NULL;
    size_t lineLength = 0;
    ssize_t byteRead = 0;
    int return_code = 0;
    while ((byteRead = getline(&line, &lineLength, infile)) != -1) {
        check_error(byteRead, 0, "", GETLINE, 0);
        if (check_pound(line)) continue;
        int *mode, redirLoc = 0, cmdLength = 0;
        mode_t *redirIn, *redirOut;
        char **parsedCmd = parse_cmd(line, &redirIn, &redirOut, &mode, &cmdLength, &redirLoc);
        if (parsedCmd == NULL) continue;
        if (strcmp(parsedCmd[0], "cd") == 0) {
            return_code = run_cd(parsedCmd[1]);
            free(redirIn); free(redirOut); free(mode); free(parsedCmd);
            continue;
        }
        if (redirLoc == 0 && strcmp(parsedCmd[0], "pwd") == 0) {
            return_code = run_pwd();
            free(redirIn); free(redirOut); free(mode); free(parsedCmd);
            continue;
        }
        if (strcmp(parsedCmd[0], "exit") == 0) {
            run_exit(parsedCmd[1], return_code);
        }
        return_code = run_cmd(parsedCmd, redirIn, redirOut, mode, cmdLength, redirLoc);
        free(redirIn); free(redirOut); free(mode); free(parsedCmd);
    }
    return 0;
}

int run_cmd(char **parsedCmd, mode_t *redirIn, mode_t *redirOut, int *mode, int cmdLength, int redirLoc) {
    pid_t w;
    struct rusage ru;
    struct timeval start, end, result;
    int fd = -1, return_code = 0;
    int wstatus, i;
    int redirLength = (redirLoc != 0) ? cmdLength - redirLoc : 0;

    get_time(&start);
    switch (fork()) {
        case -1: // fail
            perror("fork failed");
            exit(EXIT_FAILURE);
        case 0: // child
            if (shell_redirect_fd != -1) check_error(close(shell_redirect_fd), 0, shell_redirect, ICLOSE, 0);
            for (i = 0; i < redirLength; i++) {
                if (mode[i] & READ) {
                    fd = open(parsedCmd[redirLoc + i], redirIn[i]);
                    check_error(fd, 0, parsedCmd[redirLoc + i], ROPEN, 1);
                    check_error(dup2(fd, STDIN_FILENO), 0, parsedCmd[redirLoc + i], DUP, 1);
                    check_error(close(fd), 0, parsedCmd[redirLoc + i], ICLOSE, 1);
                }
                if (mode[i] & WRITE) {
                    fd = open(parsedCmd[redirLoc + i], redirOut[i], 0666);
                    check_error(fd, 0, parsedCmd[redirLoc + i], WOPEN, 1);
                    if (mode[i] & ERROR) check_error(dup2(fd, STDERR_FILENO), 0, parsedCmd[redirLoc + i], DUP, 1);
                    else check_error(dup2(fd, STDOUT_FILENO), 0, parsedCmd[redirLoc + i], DUP, 1);
                    check_error(close(fd), 0, parsedCmd[redirLoc + i], OCLOSE, 1);
                }
            }
            if (strcmp(parsedCmd[0], "pwd") == 0) {
                run_pwd();
                exit(EXIT_SUCCESS);
            } else {
                if (redirLoc) parsedCmd[redirLoc] = NULL;
                check_error(execvp(parsedCmd[0], parsedCmd), 0, parsedCmd[0], EXEC, 127);
            }
            break;
        default: // parent
            w = wait3(&wstatus, 0, &ru);
            check_error(w, 0, "", PID, 0);
            get_time(&end);
            timersub(&end, &start, &result);
            print_info(w, wstatus, &return_code);
            print_time_info(&result, &ru);
            break;
    }
    return return_code;
}

char **parse_cmd(char *line, mode_t **redirIn, mode_t **redirOut, int **mode, int *cmdLength, int *redirLoc) {
    char **parsedCmd = malloc(BUFSIZ * sizeof(char));
    if (parsedCmd == NULL) {
        fprintf(stderr, "Failed to allocate memory: %s\n", strerror(errno));
        return parsedCmd;
    }
    char *token, *delim = " \r\n";
    int i = 0, m = 0, cnt = 0;
    char *tmp = malloc(sizeof(line));
    char *tmp_addr = tmp;
    if (tmp == NULL) {
        fprintf(stderr, "Failed to allocate memory: %s\n", strerror(errno));
        return NULL;
    }
    strcpy(tmp, line);
    while ((strtok_r(tmp, delim, &tmp))) cnt++;
    free(tmp_addr);
    *redirIn = malloc(cnt * sizeof(mode_t));
    *redirOut = malloc(cnt * sizeof(mode_t));
    *mode = malloc(cnt * sizeof(int));
    if (*redirIn == NULL || *redirOut == NULL || *mode == NULL) {
        fprintf(stderr, "Failed to allocate memory: %s\n", strerror(errno));
        return NULL;
    }
    cnt = 0;
    while ((token = strtok_r(line, delim, &line))) {
        if (token[0] == '<') { // <
            (*redirIn)[cnt] = O_RDONLY;
            m |= READ;
            if (*redirLoc == 0) *redirLoc = i;
            parsedCmd[i++] = token + 1;
            (*mode)[cnt] = m;
        } else if (token[0] == '>') {
            if (token[1] == '>') { // >>
                (*redirOut)[cnt] = O_WRONLY | O_CREAT | O_APPEND;
                parsedCmd[i++] = token + 2;
            } else { // >
                (*redirOut)[cnt] = O_WRONLY | O_CREAT | O_TRUNC;
                parsedCmd[i++] = token + 1;
            }
            if (*redirLoc == 0) *redirLoc = i - 1;
            m |= WRITE;
            (*mode)[cnt] = m;
        } else if (token[0] == '2') {
            if (token[2] == '>') { // 2>>
                (*redirOut)[cnt] = O_WRONLY | O_CREAT | O_APPEND;
                parsedCmd[i++] = token + 3;
            } else { // 2>
                (*redirOut)[cnt] = O_WRONLY | O_CREAT | O_TRUNC;
                parsedCmd[i++] = token + 2;
            }
            if (*redirLoc == 0) *redirLoc = i - 1;
            m |= ERROR;
            (*mode)[cnt] = m;
        } else {
            parsedCmd[i++] = token;
            cnt--;
        }
        cnt++;
    }
    parsedCmd[i] = NULL;
    *cmdLength = i;
    return parsedCmd;
}

int run_cd(char *path) {
    if (path == NULL) path = getenv("HOME");
    check_error(chdir(path), 0, path, CHDIR, 0);
    return 0;
}

int run_pwd() {
    char buf[BUFSIZ];
    if (getcwd(buf, BUFSIZ) == NULL) {
        perror("Can't get current working directory");
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "%s\n", buf);
    return 0;
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
            default:
                break;
        }
        if (return_code != 0) exit(return_code);
        exit(EXIT_FAILURE);
    }
}

void run_exit(char *code, int return_code) {
    if (code == NULL) return exit(return_code);
    // https://stackoverflow.com/questions/8871711/atoi-how-to-identify-the-difference-between-zero-and-error/18544436
    char *tmp;
    long num = strtol(code, &tmp, 10);
    if (tmp == code) {
        fprintf(stderr, "Can't convert string %s to number\n", code);
        exit(EXIT_FAILURE);
    }
    if ((num > INT_MAX || num < INT_MIN) && errno == ERANGE) {
        fprintf(stderr, "Number out of range %s\n", optarg);
        exit(EXIT_FAILURE);
    }
    return exit((int) num);
}

bool check_pound(const char *line) {
    if (*line == '#') return true;
    return false;
}

void print_info(pid_t pid, int wstatus, int *return_code) {
    if (wstatus == 0) fprintf(stdout, "Child process %d exited normally\n", pid);
    else {
        if (WIFSIGNALED(wstatus)) {
            fprintf(stdout, "Child process %d exited with signal %d\n", pid, WTERMSIG(wstatus));
        } else {
            fprintf(stdout, "Child process %d exited with return value %d\n", pid, WEXITSTATUS(wstatus));
        }
    }
    *return_code = wstatus;
}

void print_time_info(struct timeval *result, struct rusage *ru) {
    fprintf(stdout, "Real: %ld.%06lds User: %ld.%06lds Sys: %ld.%06lds\n",
            result->tv_sec, result->tv_usec, ru->ru_utime.tv_sec,
            ru->ru_utime.tv_usec, ru->ru_stime.tv_sec, ru->ru_stime.tv_usec);
}

void get_time(struct timeval *time) {
    check_error(gettimeofday(time, NULL), 0, NULL, TIME, 0);
}
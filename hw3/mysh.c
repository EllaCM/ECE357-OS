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

void run_cd(char *path);

void check_error(int fd, int n, char *s, int type);

bool check_pound(const char *line);

char **parse_cmd(char *line, mode_t **redirIn, mode_t **redirOut, int **mode, int *cmdLength, int *redirLoc);

void run_cmd(char **parsedCmd, mode_t *redirIn, mode_t *redirOut, int *mode, int cmdLength, int redirLoc);

void run_pwd();

void run_exit(char *code);

void print_time_info(struct timeval *result, struct rusage *ru);

void print_info(pid_t pid);

void get_time(struct timeval *time);

int status;

int main(int argc, char *argv[]) {
    FILE *infile;

    if (argc > 1) {
        infile = fopen(argv[1], "r");
        if (infile == NULL) {
            perror("unable to open file");
            exit(EXIT_FAILURE);
        }
    } else {
        infile = stdin;
    }

    char *line = NULL;
    size_t lineLength = 0;
    ssize_t byteRead = 0;
    int pos;

    while ((byteRead = getline(&line, &lineLength, infile)) != -1) {
        // byteRead;
        if (check_pound(line)) continue;
        int *mode, redirLoc = 0, cmdLength = 0;
        mode_t *redirIn, *redirOut;
        char **parsedCmd = parse_cmd(line, &redirIn, &redirOut, &mode, &cmdLength, &redirLoc);
        // printf("redirLoc: %d\n", redirLoc);
        if (strcmp(parsedCmd[0], "cd") == 0) {
            run_cd(parsedCmd[1]);
            continue;
        }
        if (redirLoc == 0 && strcmp(parsedCmd[0], "pwd") == 0) {
            run_pwd();
            continue;
        }
        if (strcmp(parsedCmd[0], "exit") == 0) {
            run_exit(parsedCmd[1]);
        }
        run_cmd(parsedCmd, redirIn, redirOut, mode, cmdLength, redirLoc);
        free(redirIn);
        free(redirOut);
        free(mode);
        free(parsedCmd);
    }

    // close file
    if (argc > 1 && fclose(infile) < 0) {
        perror("unable to close file");
        exit(EXIT_FAILURE);
    }
    return 0;
}

void run_cmd(char **parsedCmd, mode_t *redirIn, mode_t *redirOut, int *mode, int cmdLength, int redirLoc) {
    pid_t cpid, w;
    struct rusage ru;
    struct timeval start, end, result;
    int fd = -1;
    int wstatus, i;
    int redirLength = (redirLoc != 0) ? cmdLength - redirLoc : 0;

    get_time(&start);
    switch (cpid = fork()) {
        case -1: // fail
            perror("fork failed");
            exit(EXIT_FAILURE);
            break;
        case 0: // child
            for (i = 0; i < redirLength; i++) {
                if (mode[i] & READ) {
                    fd = open(parsedCmd[redirLoc + i], redirIn[i]);
                    check_error(fd, 0, parsedCmd[redirLoc + i], ROPEN);
                    check_error(dup2(fd, STDIN_FILENO), 0, parsedCmd[redirLoc + i], DUP);
                }
                if (mode[i] & WRITE) {
                    fd = open(parsedCmd[redirLoc + i], redirOut[i], 0666);
                    check_error(fd, 0, parsedCmd[redirLoc + i], WOPEN);
                    if (mode[i] & ERROR) check_error(dup2(fd, STDERR_FILENO), 0, parsedCmd[redirLoc + i], DUP);
                    else check_error(dup2(fd, STDOUT_FILENO), 0, parsedCmd[redirLoc + i], DUP);
                }
            }
            if (strcmp(parsedCmd[0], "pwd") == 0) {
                run_pwd();
            } else {
                if (redirLoc) parsedCmd[redirLoc] = NULL;
                check_error(execvp(parsedCmd[0], parsedCmd), 0, parsedCmd[0], EXEC);
            }
            
            // change to check_error
            if (fd != -1 && close(fd) < 0) {
                fprintf(stderr, "can not close file\n");
                exit(EXIT_FAILURE);
            }
            break;
        default: // parent
            w = wait3(&wstatus, WUNTRACED | WCONTINUED, &ru);
                        
            if (w == -1) {
                perror("waitpid failed");
                exit(EXIT_FAILURE);
            }
            get_time(&end);
            timersub(&end, &start, &result);
            print_info(w);
            print_time_info(&result, &ru);
            break;
    }
}

char **parse_cmd(char *line, mode_t **redirIn, mode_t **redirOut, int **mode, int *cmdLength, int *redirLoc) {
    char **parsedCmd = malloc(BUFSIZ * sizeof(char));
    char *token, *delim = " \r\n";
    int i = 0, m = 0, cnt = 0;
    char *tmp = malloc(sizeof(line));
    strcpy(tmp, line);
    while ((strtok_r(tmp, delim, &tmp))) cnt++;
    *redirIn = malloc(cnt * sizeof(mode_t));
    *redirOut = malloc(cnt * sizeof(mode_t));
    *mode = malloc(cnt * sizeof(int));
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

void run_cd(char *path) {
    if (path == NULL) path = getenv("HOME");
    check_error(chdir(path), 0, path, CHDIR);
}

void run_pwd() {
    char buf[BUFSIZ];
    if (getcwd(buf, BUFSIZ) == NULL) {
        perror("Can't get current working directory");
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "%s\n", buf);
}

void check_error(int fd, int n, char *s, int type) {
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
                fprintf(stderr, "Can't change current directory to %s: %s", s, strerror(errno));
                break;
            default:
                break;
        }
        exit(EXIT_FAILURE);
    }
}

void run_exit(char *code) {
    if (code == NULL) return exit(0);
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

void print_info(pid_t pid) {
    fprintf(stdout, "Child process %d exited normally\n", pid);
}

void print_time_info(struct timeval *result, struct rusage *ru) {
    fprintf(stdout, "Real: %ld.%06lds User: %ld.%06lds Sys: %ld.%06lds\n",
            result->tv_sec, result->tv_usec, ru->ru_utime.tv_sec,
            ru->ru_utime.tv_usec, ru->ru_stime.tv_sec, ru->ru_stime.tv_usec);
}

void get_time(struct timeval *time) {
    check_error(gettimeofday(time, NULL), 0, NULL, TIME);
}
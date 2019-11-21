#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

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
#define RWOPEN 20
#define LSEEK 21
#define REMOVE 22
#define MMAP 23
#define UNMAP 24

#define TESTFILE "test"

void check_error(int fd, int n, char *s, int type, int return_code);
int test1();
int test2_3(int test);
int test4();
void test_handler(int sig);
void set_signal();

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: hw5 test_case_number\n");
        exit(EXIT_FAILURE);
    }
    int test_case = atoi(argv[1]);
    set_signal();
    int return_code;
    switch (test_case){
        case 1:
            return_code = test1();
            break;
        case 2:
            return_code = test2_3(2);
            break;
        case 3:
            return_code = test2_3(3);
            break;
        case 4:
            return_code = test4();
            break;
        default:
            fprintf(stderr, "undefined test case number\n");
            exit(EXIT_FAILURE);
    }
    check_error(remove(TESTFILE), 0, TESTFILE, REMOVE, 0);
    return return_code;
}

int test1() {
    int fd;
    char *tmp = "AAAAAAAAAA";

    fprintf(stderr, "Executing Test #1 (write to r/o mmap):\n");
    fprintf(stderr, "creating temporary file\n");
    check_error((fd=open(TESTFILE, O_CREAT|O_TRUNC|O_RDWR, 0666)), 0, TESTFILE, RWOPEN, 0);
    check_error(write(fd, tmp, 10), 0, TESTFILE, WRITE, 0);
    fprintf(stderr, "mapping with PROT_READ and MAP_SHARED\n");
    char* addr = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        fprintf(stderr, "Failed to map file %s to memory: %s", TESTFILE, strerror(errno));
        exit(255);
    }
    fprintf(stderr, "map[%d]=='%c'\n", 3, addr[3]);
    fprintf(stderr, "writing a '%c'\n", 'B');
    addr[3] = 'B';
    if (addr[3] != 'B') return 255;
    fprintf(stderr, "unmapping previously mapped region\n");
    check_error(munmap(addr, 10), 0, "", UNMAP, 0);
    fprintf(stderr, "closing temporary file\n");
    check_error(fd, close(fd), TESTFILE, OCLOSE, 0);
    return 0;
}

int test2_3(int test) {
    int fd;
    char buf[10];
    char *tmp = "AAAAAAAAAA";

    if (test == 2) fprintf(stderr, "Executing Test #2 (write to shared map):\n");
    else if (test == 3) fprintf(stderr, "Executing Test #3 (write to private map):\n");
    fprintf(stderr, "creating temporary file\n");
    check_error((fd = open(TESTFILE, O_CREAT | O_TRUNC | O_RDWR, 0666)), 0,
                TESTFILE, RWOPEN, 0);
    check_error(write(fd, tmp, 10), 0, TESTFILE, WRITE, 0);
    char* addr;
    if (test == 2) {
        fprintf(stderr, "mapping with PROT_READ|PROT_WRITE and MAP_SHARED\n");
        addr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    }
    else if(test == 3) {
        fprintf(stderr, "mapping with PROT_READ|PROT_WRITE and MAP_PRIVATE\n");
        addr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    }
    if (addr == MAP_FAILED) {
        fprintf(stderr, "Failed to map file %s to memory: %s", TESTFILE, strerror(errno));
        exit(255);
    }
    fprintf(stderr, "map[%d]=='%c'\n", 3, addr[3]);
    fprintf(stderr, "writing a '%c'\n", 'B');
    addr[3] = 'B';
    fprintf(stderr, "map[%d]=='%c'\n", 3, addr[3]);
    check_error(lseek(fd, 3, SEEK_SET), 0, TESTFILE, LSEEK, 0);
    check_error(fd, read(fd, buf, 1), TESTFILE, READ, 0);
    fprintf(stderr, "file[%d]=='%c'\n", 3, buf[0]);
    if (buf[0] == addr[3]) return 0;
    fprintf(stderr, "unmapping previously mapped region\n");
    check_error(munmap(addr, 4096), 0, "", UNMAP, 255);
    fprintf(stderr, "closing temporary file\n");
    check_error(fd, close(fd), TESTFILE, OCLOSE, 0);
    return 1;
}

int test4() {
    int fd;
    char buf[10];
    char *tmp = "AAAAAAAAAA";
    fprintf(stderr, "Executing Test #4 (create hole):\n");
    fprintf(stderr, "creating temporary file\n");
    check_error((fd=open(TESTFILE, O_CREAT|O_TRUNC|O_RDWR, 0666)), 0, TESTFILE, RWOPEN, 0);
    check_error(write(fd, tmp, 10), 0, TESTFILE, WRITE, 0);
    fprintf(stderr, "mapping with PROT_READ|PROT_WRITE and MAP_SHARED\n");
    char* addr = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        fprintf(stderr, "Failed to map file %s to memory: %s", TESTFILE, strerror(errno));
        exit(255);
    }
    addr[10] = 'B';
    fprintf(stderr, "writing a '%c' to map[%d]\n", 'B', 10);
    check_error(lseek(fd, 26, SEEK_SET), 0, TESTFILE, LSEEK, 0);
    check_error(write(fd, "B", 1), 0, TESTFILE, WRITE, 0);
    fprintf(stderr, "map[%d]=='%c'\n", 26, addr[26]);

    check_error(lseek(fd, 10, SEEK_SET), 0, TESTFILE, LSEEK, 0);
    check_error(fd, read(fd, buf, 1), TESTFILE, READ, 1);
    fprintf(stderr, "file[%d]=='%c'\n", 10, buf[0]);
    if (buf[0] != 'B') return 1;
    fprintf(stderr, "unmapping previously mapped region\n");
    check_error(munmap(addr, 4096), 0, "", UNMAP, 255);
    fprintf(stderr, "closing temporary file\n");
    check_error(fd, close(fd), TESTFILE, OCLOSE, 0);
    return 0;
}

void test_handler(int sig) {
    fprintf(stderr, "Signal %d received!\n", sig);
    exit(sig);
}

void set_signal() {
    struct sigaction sa;
    sa.sa_handler = test_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    for (int i = 1; i < 32; i++) {
        if (i == SIGKILL || i == SIGSTOP) continue;
        check_error(sigaction(i, &sa, NULL), 0, "", SIGACT, 0);
    }
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
            case RWOPEN:
                fprintf(stderr, "Can't open file %s for reading and writing: %s\n", s, strerror(errno));
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
            case LSEEK:
                fprintf(stderr, "Can't open file %s for lseek: %s\n", s, strerror(errno));
                break;
            case REMOVE:
                fprintf(stderr, "Can't remove file %s: %s\n", s, strerror(errno));
                break;
            case UNMAP:
                fprintf(stderr, "Failed to unmap file %s: %s", TESTFILE, strerror(errno));
                break;
            default:
                break;
        }
        if (return_code != 0) exit(return_code);
        exit(EXIT_FAILURE);
    }
}
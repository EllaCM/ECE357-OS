#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 4096

#define READ 0
#define WRITE 1
#define OCLOSE 2
#define ICLOSE 3
#define WOPEN 4
#define ROPEN 5

void report(char* infile, char* outfile, int bytes, int r_cnt, int w_cnt, bool is_binary) {
    fprintf(stderr, "From %s to %s, %d bytes transferred, %d read system calls were made, and %d write system calls were made.\n", infile, outfile, bytes, r_cnt, w_cnt);
    if (is_binary) fprintf(stderr, "Warning: %s contains binary.\n", infile);
}

void check_error(int fd, int n, char* filename, int type) {
    if (fd < 0 || n < 0) {
        switch (type) {
            case WRITE: fprintf(stderr, "Can't write file %s: %s\n", filename, strerror(errno));
                break;
            case READ:  fprintf(stderr, "Can't read file %s: %s\n", filename, strerror(errno));
                break;
            case OCLOSE: fprintf(stderr, "Can't close output file %s: %s\n", filename, strerror(errno));
                break;
            case ICLOSE: fprintf(stderr, "Can't close input file %s: %s\n", filename, strerror(errno));
                break;
            case WOPEN:  fprintf(stderr, "Can't open file %s for writing: %s\n", filename, strerror(errno));
                break;
            case ROPEN:  fprintf(stderr, "Can't open file %s for reading: %s\n", filename, strerror(errno));
                break;
            default:
                break;
        }
        exit(-1);
    }
}

bool check_binary(char buf[], int n) {
    for (int i = 0; i < n; i++) {
        if (!(isprint(buf[i]) || isspace(buf[i]))) return true;
    }
    return false;
}

int main(int argc, char *argv[]) {
    char* outfile = NULL;
    char buf[BUF_SIZE];
    
    int opt, i, fd_w, fd_r, bytes_written, bytes_read, opt_id, r_cnt, w_cnt, total_bytes, total_bytes_written;
    bool is_binary = false;
    
    opterr = 0;
    if (getopt(argc, argv, "o:") != -1) {
        if (optarg == NULL) {
            fprintf(stderr, "No value provided for flag o\n");
            exit(-1);
        }
        outfile = optarg;
    }
    opt_id = optind;
    
    fd_w = STDOUT_FILENO;
    if (outfile) {
        fd_w = open(outfile, O_WRONLY|O_CREAT|O_TRUNC, 0666);
        check_error(fd_w, 0, outfile, WOPEN);
    }
    
    if (opt_id == argc) argv[--opt_id] = "-";
    
    for (i = opt_id; i < argc; i++) {
        if (strcmp(argv[i], "-") == 0) fd_r = STDIN_FILENO;
        else fd_r = open(argv[i], O_RDONLY);
        check_error(fd_r, 0, argv[i], ROPEN);
        r_cnt = 1, w_cnt = 0, total_bytes = 0, is_binary = false;
        while ((bytes_read = read(fd_r, buf, BUF_SIZE)) != 0) {
            r_cnt++;
            check_error(fd_r, bytes_read, argv[i], READ);
            if (!is_binary) is_binary = check_binary(buf, bytes_read);
            bytes_written = 0, total_bytes_written = 0;
            while (bytes_written < bytes_read) {
                w_cnt++;
                bytes_written = write(fd_w, buf+total_bytes_written, bytes_read-total_bytes_written);
                check_error(fd_w, bytes_written, outfile, WRITE);
                total_bytes_written += bytes_written;
            }
            total_bytes += total_bytes_written;
        }
        if (!fd_r && !outfile) report("<standard input>", "<standard output>", total_bytes, r_cnt, w_cnt, is_binary);
        else if (!fd_r) report("<standard input>", outfile, total_bytes, r_cnt, w_cnt, is_binary);
        else if (!outfile) report(argv[i], "<standard output>", total_bytes, r_cnt, w_cnt, is_binary);
        else report(argv[i], outfile, total_bytes, r_cnt, w_cnt, is_binary);
        if (fd_r) check_error(fd_r, close(fd_r), argv[i], OCLOSE);
    }
    if (outfile) check_error(fd_w, close(fd_w), outfile, ICLOSE);
    return 0;
}
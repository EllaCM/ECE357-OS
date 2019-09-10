#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define MAX 10
#define MAX_SIZE 20


int main(int argc, char *argv[]) {
    char* outfile = NULL;
    char* infiles[MAX_SIZE];
    char buf[4096];
    int opt, cnt, i, fd_w, fd_r, n;

    opterr = 0;
    if (getopt(argc, argv, "o:") != -1) outfile = optarg;

    cnt = 0;
    for (i = optind; i < argc; i++) infiles[cnt++] = argv[i];

    fd_w = 1;
    if (outfile) fd_w = open(outfile, O_WRONLY|O_CREAT|O_TRUNC, 0666); 
    if (fd_w != -1) {
        if (cnt == 0) {
            // no input file
            fd_r = 0;
        } else {
            for (i = 0; i < cnt; i++) {
                fd_r = open(infiles[i], O_RDONLY);
                if (fd_r == -1) {
                    // read open error
                    continue;
                }
                if (errno == ENOENT) {
                    // file does not appear to be the name of a valid file\n
                    continue;
                }
                while (n = read(fd_r, buf, sizeof(buf))) {
                    write(fd_w, buf, n);
                }
                close(fd_r);
            }
        }
    } 
    // else {
    //     // write open error
    // }
    if (outfile) close(fd_w);

    if (outfile) printf("Outfile: %s\n", outfile);
    for (i = 0; i < cnt; i++) printf("Infile%d: %s\n", i+1, infiles[i]);
    return 0;
}

// int main() {
//     char* infile = "test.txt";
//     char buf[4096];
//     int fd = open(infile, O_RDONLY);
//     int j;
//     while (j = read(fd, buf, sizeof(buf))) {
//         //write(1, buf, j);
//         printf("%d\n", j);
//     }
//     return 0;
// }
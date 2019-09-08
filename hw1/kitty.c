#include <stdio.h>
#include <unistd.h>

#define MAX 10
#define MAX_SIZE 20


int main(int argc, char *argv[]) {
    char* outfile = NULL;
    char* infiles[MAX_SIZE];
    int opt, cnt, i;

    opterr = 0;
    if (getopt(argc, argv, "o:") != -1) outfile = optarg;

    cnt = 0;
    for (i = optind; i < argc; i++) infiles[cnt++] = argv[i];






    if (outfile) printf("Outfile: %s\n", outfile);
    for (i = 0; i < cnt; i++) printf("Infile%d: %s\n", i+1, infiles[i]);
    return 0;
}
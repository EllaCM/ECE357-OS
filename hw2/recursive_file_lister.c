#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define SEP "/"

bool init = true;

void recursive_travese(char *curPath);
char *pathJoin(char *curPath, char *childPath);
void parse_info(struct dirent *direntp, struct stat *statbuf, struct passwd *pwd, struct group *grp, char tmp[]);
void print_inode(__ino_t inode);
void print_block_num(__blkcnt_t block_num);
void print_permission(__mode_t mode);
void print_nlinks(__nlink_t nlinks);
void print_id(char *name);
void print_size(struct stat *statbuf);
void print_fname(char tmp[], __mode_t mode);

int main(int argc, char *argv[])
{
    bool mtimeFlag = false, volumeFlag = false;
    int opt, mtime;
    char *curPath;
    while ((opt = getopt(argc, argv, "m:v")) != -1)
    {
        switch (opt)
        {
        case 'm':
            mtimeFlag = true;
            if (optarg == NULL)
            {
                fprintf(stderr, "No value provided for flag m\n");
                exit(EXIT_FAILURE);
            }
            mtime = atoi(optarg);
            // check error
            break;
        case 'v':
            volumeFlag = true;
            break;
        default:
            printf("Unfound flag\n");
            break;
        }
    }

    if (optind >= argc)
    {
        fprintf(stderr, "Expected path after options\n");
        exit(EXIT_FAILURE);
    }
    curPath = argv[optind];
    recursive_travese(curPath);
    return 0;
}

char *pathJoin(char *curPath, char *childPath)
{
    if (strcmp(childPath, ".") == 0 && init)
    {
        init = false;
        return childPath;
    }
    return strcat(strcat(curPath, SEP), childPath);
}

void recursive_travese(char *curPath)
{
    char tmp[1024];
    DIR *dirp;
    struct dirent *direntp;
    struct stat statbuf;
    struct group *grp;
    struct passwd *pwd;

    dirp = opendir(curPath);
    if (dirp == NULL)
    {
        fprintf(stderr, "open directory error %s", strerror(errno));
        return;
    }
    while (direntp = readdir(dirp))
    {
        if (direntp == NULL)
        {
            fprintf(stderr, "error %s", strerror(errno));
            break;
        }
        if (strcmp(direntp->d_name, "..") == 0 || (init || strcmp(direntp->d_name, ".")) == 0)
            continue;
        strcpy(tmp, curPath);
        pathJoin(tmp, direntp->d_name);
        if (lstat(tmp, &statbuf) < 0)
        {
            fprintf(stderr, "error getting stat %s: %s", curPath, strerror(errno));
        }

        if ((pwd = getpwuid(statbuf.st_uid)) == NULL)
        {
            fprintf(stderr, "user not found");
            continue;
        }

        if ((grp = getgrgid(statbuf.st_gid)) == NULL)
        {
            fprintf(stderr, "group not found");
            continue;
        }

        parse_info(direntp, &statbuf, pwd, grp, tmp);

        if (direntp->d_type == DT_DIR && strcmp(direntp->d_name, ".") != 0)
        {
            recursive_travese(tmp);
        }
    }
    if (closedir(dirp) < 0)
    {
        fprintf(stderr, "error closing directory %s: %s", curPath, strerror(errno));
    }
}

void print_inode(__ino_t inode)
{
    printf("%lu\t", inode);
}

void print_block_num(__blkcnt_t block_num)
{
    printf("%ld\t", block_num);
}

// switch case inspiration from
// https://stackoverflow.com/questions/10323060/printing-file-permissions-like-ls-l-using-stat2-in-c/44580683#44580683
void print_permission(__mode_t mode)
{
    char permission[10];
    memset(permission, '-', sizeof(permission));
    char c;
    switch (mode & S_IFMT)
    {
    case S_IFBLK:
        c = 'b';
        break;
    case S_IFCHR:
        c = 'c';
        break;
    case S_IFDIR:
        c = 'd';
        break;
    case S_IFIFO:
        c = 'f';
        break;
    case S_IFLNK:
        c = 'l';
        break;
    case S_IFREG:
        c = '-';
        break;
    case S_IFSOCK:
        c = 's';
        break;
    default:
        c = '?';
        break;
    }
    permission[0] = c;
    if (mode & S_IRUSR)
        permission[1] = 'r';
    if (mode & S_IWUSR)
        permission[2] = 'w';
    if (mode & S_IXUSR)
        permission[3] = 'x';
    if (mode & S_IRGRP)
        permission[4] = 'r';
    if (mode & S_IWGRP)
        permission[5] = 'w';
    if (mode & S_IXGRP)
        permission[6] = 'x';
    if (mode & S_IROTH)
        permission[7] = 'r';
    if (mode & S_IWOTH)
        permission[8] = 'w';
    if (mode & S_IROTH)
        permission[9] = 'x';
    if (mode & S_ISVTX)
        permission[9] = 't';
    printf("%s\t", permission);
}

void print_nlinks(__nlink_t nlinks)
{
    printf("%ld\t", (long) nlinks);
}

void print_id(char *name)
{
    printf("%s\t", name);
}

void print_size(struct stat *statbuf)
{   
    if (S_ISBLK(statbuf->st_mode) || S_ISCHR(statbuf->st_mode)) {
        printf("%lx, %lx\t", (long) major(statbuf->st_dev), (long) minor(statbuf->st_dev));
    } else {
        printf("%ld\t", statbuf->st_size);
    }
}

void print_mtime(__time_t *mtime) {
    struct tm* mt = localtime(mtime);
    char buf[1024];
    strftime(buf, sizeof(buf), "%b %d %R", mt);
    printf("%s\t", buf);
}

void print_fname(char tmp[], __mode_t mode) {
    char buf[1024];
    printf("%s", tmp);
    if (S_ISLNK(mode)) {
        readlink(tmp, buf, sizeof(buf));
        printf(" -> %s", buf);
    }
    printf("\n");
}

void parse_info(struct dirent *direntp, struct stat *statbuf, struct passwd *pwd, struct group *grp, char tmp[])
{
    print_inode(direntp->d_ino);
    print_block_num(statbuf->st_blocks / 2);
    print_permission(statbuf->st_mode);
    print_nlinks(statbuf->st_nlink);
    print_id(pwd->pw_name);
    print_id(grp->gr_name);
    print_size(statbuf);
    print_mtime(&statbuf->st_mtime);
    print_fname(tmp, statbuf->st_mode);
}

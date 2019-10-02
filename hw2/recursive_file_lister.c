#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define SEP "/"

bool mtimeFlag = false, volumeFlag = false, userFlag = false,
     curUserNotFound = false, curUserNameNotFound = false;
int nGroups = 0, curGroupsCnt;
double mtimeLimit;
char *curUser;
char startPath[4096];
dev_t devNum;
time_t currentTime;
__uid_t curUid;
gid_t *curGroups;
struct passwd *curUserp;

bool check_uid_or_user(char *curUser);
bool check_user(struct stat *statbuf);
bool check_mtime(__time_t mtime);
char *path_join(char *curPath, char *childPath);
void recursive_travese(char *curPath);
void parse_info(struct dirent *direntp, struct stat *statbuf,
                struct passwd *pwd, struct group *grp, char tmp[],
                bool userNotFound, bool groupNotFound);
void print_inode(__ino_t inode);
void print_block_num(long block_num);
void print_permission(__mode_t mode);
void print_nlinks(__nlink_t nlinks);
void print_name(char *name);
void print_id(long id);
void print_size(struct stat *statbuf);
void print_fname(char tmp[], __mode_t mode);
void print_mtime(__time_t *mtime);

int main(int argc, char *argv[]) {
  time(&currentTime);
  int opt;
  char *curPath;
  while ((opt = getopt(argc, argv, "m:vu:")) != -1) {
    switch (opt) {
    case 'm':
      mtimeFlag = true;
      // https://stackoverflow.com/questions/8871711/atoi-how-to-identify-the-difference-between-zero-and-error/18544436
      char *end;
      long num = strtol(optarg, &end, 10);
      if (end == optarg) {
        fprintf(stderr, "Can't convert string %s to number\n", optarg);
        exit(EXIT_FAILURE);
      }
      if ((num == LONG_MAX || num == LONG_MIN) && errno == ERANGE) {
        fprintf(stderr, "Number out of range %s\n", optarg);
        exit(EXIT_FAILURE);
      }
      mtimeLimit = num;
      break;
    case 'v':
      volumeFlag = true;
      break;
    case 'u':
      userFlag = true;
      curUser = optarg;
      if (check_uid_or_user(optarg)) {
        curUserp = getpwuid(atoi(optarg));
        if (curUserp == NULL) {
          curUserNotFound = true;
          curUid = atoi(optarg);
        }
      } else {
        curUserp = getpwnam(optarg);
        if (curUserp == NULL) {
          curUserNameNotFound = true;
          // fprintf(stderr, "No user named %s found\n", curUser);
          // exit(EXIT_FAILURE);
        }
      }
      if (curUserp) {
        getgrouplist(curUser, curUserp->pw_gid, curGroups, &nGroups);
        curGroups = malloc(nGroups * sizeof(gid_t));
        curGroupsCnt = getgrouplist(curUser, curUserp->pw_gid, curGroups, &nGroups);
      }
      break;
    default:
      printf("Unfound flag\n");
      break;
    }
  }

  if (optind >= argc) {
    fprintf(stderr, "Expected path after options\n");
    exit(EXIT_FAILURE);
  }
  curPath = argv[optind];

  if (volumeFlag) {
    struct stat statbuf;
    if (lstat(curPath, &statbuf) < 0) {
      fprintf(stderr, "Error getting stat %s: %s\n", curPath, strerror(errno));
      exit(EXIT_FAILURE);
    }
    devNum = statbuf.st_dev;
  }

  memcpy(startPath, curPath, strlen(curPath));
  recursive_travese(curPath);
  return 0;
}

char *path_join(char *curPath, char *childPath) {
  if (strcmp(curPath, startPath) == 0 && strcmp(childPath, ".") == 0) return curPath;
  return strcat(strcat(curPath, SEP), childPath);
}

void recursive_travese(char *curPath) {
  bool userNotFound, groupNotFound;
  char tmp[4096];
  DIR *dirp;
  struct dirent *direntp;
  struct stat statbuf;
  struct group *grp;
  struct passwd *pwd;

  dirp = opendir(curPath);
  if (dirp == NULL) {
    fprintf(stderr, "Can't open directory %s: %s\n", tmp, strerror(errno));
    return;
  }
  while ((direntp = readdir(dirp))) {
    userNotFound = false;
    groupNotFound = false;
    if (direntp == NULL) break;
    if (strcmp(direntp->d_name, "..") == 0 ||
        (strcmp(direntp->d_name, ".") == 0 && strcmp(curPath, startPath) != 0))
      continue;
    strcpy(tmp, curPath);
    path_join(tmp, direntp->d_name);

    if (lstat(tmp, &statbuf) < 0) {
      fprintf(stderr, "Can't retrieve stat for %s: %s\n", tmp, strerror(errno));
      continue;
    }
    if (userFlag && !check_user(&statbuf)) continue;
    if ((mtimeFlag && check_mtime(statbuf.st_mtime))) continue;

    if ((pwd = getpwuid(statbuf.st_uid)) == NULL)userNotFound = true;
    if ((grp = getgrgid(statbuf.st_gid)) == NULL) groupNotFound = true;

    parse_info(direntp, &statbuf, pwd, grp, tmp, userNotFound, groupNotFound);

    if (direntp->d_type == DT_DIR && strcmp(direntp->d_name, ".") != 0) {
      if (!volumeFlag) recursive_travese(tmp);
      else if (devNum == statbuf.st_dev) recursive_travese(tmp);
      else fprintf(stderr, "note: not crossing mount point at %s\n", tmp);
    }
  }
  if (closedir(dirp) < 0) fprintf(stderr, "error closing directory %s: %s\n", curPath, strerror(errno));
}

void parse_info(struct dirent *direntp, struct stat *statbuf,
                struct passwd *pwd, struct group *grp, char tmp[],
                bool userNotFound, bool groupNotFound) {
  print_inode(direntp->d_ino);
  print_block_num(statbuf->st_blocks / 2 + statbuf->st_blocks % 2);
  print_permission(statbuf->st_mode);
  print_nlinks(statbuf->st_nlink);
  if (userNotFound) print_id(statbuf->st_uid);
  else print_name(pwd->pw_name);
  if (groupNotFound) print_id(statbuf->st_gid);
  else print_name(grp->gr_name);
  print_size(statbuf);
  print_mtime(&statbuf->st_mtime);
  print_fname(tmp, statbuf->st_mode);
}

bool check_user(struct stat *statbuf) {
  if (!curUserNameNotFound && statbuf->st_mode & S_IRUSR &&
      ((curUserNotFound && curUid == statbuf->st_uid) ||
       (!curUserNotFound && curUserp->pw_uid == statbuf->st_uid)))
    return true;

  bool sameGroup = false;
  if (curGroupsCnt != 0) {
    struct group *gr;
    for (int i = 0; i < curGroupsCnt; i++) {
      gr = getgrgid(curGroups[i]);
      if (gr->gr_gid == statbuf->st_gid) {
        sameGroup = true;
        break;
      }
    }
  }

  if (!curUserNameNotFound && sameGroup && statbuf->st_mode & S_IRGRP) return true;
  if (!sameGroup && statbuf->st_mode & S_IROTH) return true;
  return false;
}

bool check_uid_or_user(char *curUser) { return isdigit(*curUser); }

bool check_mtime(__time_t mtime) {
  double timeDiff = difftime(currentTime, mtime);
  // printf("%f %f\n", mtimeLimit, timeDiff);
  if (mtimeLimit >= 0) {
    if (timeDiff >= mtimeLimit)
      return false;
  } else {
    if (timeDiff <= -mtimeLimit)
      return false;
  }
  return true;
}

void print_inode(__ino_t inode) { printf("%lu\t", inode); }

void print_block_num(long block_num) { printf("%ld\t", block_num); }

// switch case inspiration from
// https://stackoverflow.com/questions/10323060/printing-file-permissions-like-ls-l-using-stat2-in-c/44580683#44580683
void print_permission(__mode_t mode) {
  char permission[11];
  char c;
  switch (mode & S_IFMT) {
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
  permission[1] = (mode & S_IRUSR) ? 'r' : '-';
  permission[2] = (mode & S_IWUSR) ? 'w' : '-';
  permission[3] = (mode & S_IXUSR) ? 'x' : '-';
  permission[4] = (mode & S_IRGRP) ? 'r' : '-';
  permission[5] = (mode & S_IWGRP) ? 'w' : '-';
  permission[6] = (mode & S_IXGRP) ? 'x' : '-';
  permission[7] = (mode & S_IROTH) ? 'r' : '-';
  permission[8] = (mode & S_IWOTH) ? 'w' : '-';
  permission[9] = (mode & S_IXOTH) ? 'x' : '-';
  if (mode & S_ISUID) permission[3] = (mode & S_IXUSR) ? 's' : 'S';
  if (mode & S_ISGID) permission[6] = (mode & S_IXGRP) ? 's' : 'S';
  if (mode & S_ISVTX) permission[9] = (mode & S_IXOTH) ? 't' : 'T';
  permission[10] = '\0';
  printf("%s\t", permission);
}

void print_nlinks(__nlink_t nlinks) { printf("%ld\t", (long)nlinks); }

void print_id(long id) { printf("%ld\t", id); }

void print_name(char *name) { printf("%s\t", name); }

void print_size(struct stat *statbuf) {
  if (S_ISBLK(statbuf->st_mode) || S_ISCHR(statbuf->st_mode)) {
    printf("%ld, %ld\t", (long)major(statbuf->st_rdev),
           (long)minor(statbuf->st_rdev));
  } else {
    printf("%lld\t", (long long)statbuf->st_size);
  }
}

void print_mtime(__time_t *mtime) {
  struct tm *mt = localtime(mtime);
  char buf[4096];
  strftime(buf, sizeof(buf), "%b %d %R", mt);
  printf("%s\t", buf);
}

void print_fname(char tmp[], __mode_t mode) {
  char buf[4096];
  printf("%s", tmp);
  if (S_ISLNK(mode)) {
    readlink(tmp, buf, sizeof(buf));
    printf(" -> %s", buf);
  }
  printf("\n");
}
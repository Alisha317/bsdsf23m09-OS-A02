/*
* Programming Assignment 02: lsv1.1.0
* Version 1.1.0: adds the -l option (long listing)
* Usage:
*       $ ./bin/ls
*       $ ./bin/ls -l
*       $ ./bin/ls -l /home /etc
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>

extern int errno;

void do_ls(const char *dir, int long_flag);
void print_long(const char *dir, const char *name);
void print_permissions(mode_t mode);

int main(int argc, char *argv[])
{
    int opt;
    int long_flag = 0;          /* 0 = normal listing, 1 = long listing */

    while ((opt = getopt(argc, argv, "l")) != -1)
    {
        switch (opt)
        {
        case 'l':
            long_flag = 1;
            break;
        default:
            fprintf(stderr, "Usage: %s [-l] [directory...]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind == argc)
    {
        do_ls(".", long_flag);
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s : \n", argv[i]);
            do_ls(argv[i], long_flag);
            puts("");
        }
    }
    return 0;
}

void do_ls(const char *dir, int long_flag)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }
    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        if (long_flag)
            print_long(dir, entry->d_name);
        else
            printf("%s\n", entry->d_name);
    }

    if (errno != 0)
    {
        perror("readdir failed");
    }

    closedir(dp);
}

/* Print ONE line of the long listing for file "name" inside directory "dir" */
void print_long(const char *dir, const char *name)
{
    char path[PATH_MAX];
    struct stat st;

    /* lstat needs the full path: "dir/name", not just "name" */
    snprintf(path, sizeof(path), "%s/%s", dir, name);

    /* lstat (not stat) so that a symbolic link describes itself */
    if (lstat(path, &st) == -1)
    {
        perror(path);
        return;
    }

    /* 1. type + permissions, e.g. -rwxr-xr-x */
    print_permissions(st.st_mode);

    /* 2. number of hard links */
    printf(" %3lu", (unsigned long)st.st_nlink);

    /* 3. owner and group NAMES (st_uid / st_gid are only numbers) */
    struct passwd *pw = getpwuid(st.st_uid);
    struct group  *gr = getgrgid(st.st_gid);
    printf(" %-8s", pw ? pw->pw_name : "?");
    printf(" %-8s", gr ? gr->gr_name : "?");

    /* 4. size in bytes */
    printf(" %8lld", (long long)st.st_size);

    /* 5. modification time: ctime gives "Wed Oct 14 18:29:42 2026\n"
          we skip the first 4 chars (day name) and keep 12 chars: "Oct 14 18:29" */
    time_t t = st.st_mtime;
    printf(" %.12s", ctime(&t) + 4);

    /* 6. file name */
    printf(" %s", name);

    /* 7. for a symbolic link also show where it points: name -> target */
    if (S_ISLNK(st.st_mode))
    {
        char target[PATH_MAX];
        ssize_t n = readlink(path, target, sizeof(target) - 1);
        if (n != -1)
        {
            target[n] = '\0';
            printf(" -> %s", target);
        }
    }
    printf("\n");
}

/* Convert st_mode (a number) into a string like "drwxr-xr-x" */
void print_permissions(mode_t mode)
{
    char perm[11];

    /* first character = file type */
    if (S_ISREG(mode))       perm[0] = '-';
    else if (S_ISDIR(mode))  perm[0] = 'd';
    else if (S_ISLNK(mode))  perm[0] = 'l';
    else if (S_ISCHR(mode))  perm[0] = 'c';
    else if (S_ISBLK(mode))  perm[0] = 'b';
    else if (S_ISFIFO(mode)) perm[0] = 'p';
    else if (S_ISSOCK(mode)) perm[0] = 's';
    else                     perm[0] = '?';

    /* next 9 characters = rwx for owner, group, others (bitwise AND) */
    perm[1] = (mode & S_IRUSR) ? 'r' : '-';
    perm[2] = (mode & S_IWUSR) ? 'w' : '-';
    perm[3] = (mode & S_IXUSR) ? 'x' : '-';
    perm[4] = (mode & S_IRGRP) ? 'r' : '-';
    perm[5] = (mode & S_IWGRP) ? 'w' : '-';
    perm[6] = (mode & S_IXGRP) ? 'x' : '-';
    perm[7] = (mode & S_IROTH) ? 'r' : '-';
    perm[8] = (mode & S_IWOTH) ? 'w' : '-';
    perm[9] = (mode & S_IXOTH) ? 'x' : '-';
    perm[10] = '\0';

    /* special bits: SUID -> s/S, SGID -> s/S, sticky -> t/T
       (small letter if execute is also set, capital letter if not) */
    if (mode & S_ISUID) perm[3] = (mode & S_IXUSR) ? 's' : 'S';
    if (mode & S_ISGID) perm[6] = (mode & S_IXGRP) ? 's' : 'S';
    if (mode & S_ISVTX) perm[9] = (mode & S_IXOTH) ? 't' : 'T';

    printf("%s", perm);
}

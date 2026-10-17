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

#define SIX_MONTHS 15778476   /* seconds; older files show the year, not the time */

/* widest value of each column, so that all columns line up */
struct widths
{
    int nlink;
    int owner;
    int group;
    int size;
};

void do_ls(const char *dir, int long_flag);
void compute_widths(DIR *dp, const char *dir, struct widths *w, long long *blocks);
void print_long(const char *dir, const char *name, const struct widths *w);
void print_permissions(mode_t mode);
void print_time(time_t t);

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

    struct widths w = {1, 1, 1, 1};

    if (long_flag)
    {
        /* PASS 1: read the whole directory once, only to find the widest
           value of every column and the total number of blocks */
        long long blocks = 0;
        compute_widths(dp, dir, &w, &blocks);

        /* blocks are counted in 512-byte units, "total" is shown in 1K units */
        printf("total %lld\n", (blocks + 1) / 2);

        rewinddir(dp);          /* go back to the start for the second pass */
    }

    /* PASS 2 (or the only pass for a normal listing): print */
    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        if (long_flag)
            print_long(dir, entry->d_name, &w);
        else
            printf("%s\n", entry->d_name);
    }

    if (errno != 0)
    {
        perror("readdir failed");
    }

    closedir(dp);
}

/* number of digits of n, e.g. 4096 -> 4 */
static int num_width(unsigned long long n)
{
    int digits = 1;
    while (n >= 10)
    {
        n /= 10;
        digits++;
    }
    return digits;
}

/* owner / group name; if there is no name for the number, print the number */
static void owner_name(uid_t uid, char *buf, size_t len)
{
    struct passwd *pw = getpwuid(uid);
    if (pw)
        snprintf(buf, len, "%s", pw->pw_name);
    else
        snprintf(buf, len, "%u", (unsigned)uid);
}

static void group_name(gid_t gid, char *buf, size_t len)
{
    struct group *gr = getgrgid(gid);
    if (gr)
        snprintf(buf, len, "%s", gr->gr_name);
    else
        snprintf(buf, len, "%u", (unsigned)gid);
}

/* PASS 1: find the widest value of each column and add up the blocks */
void compute_widths(DIR *dp, const char *dir, struct widths *w, long long *blocks)
{
    struct dirent *entry;
    struct stat st;
    char path[PATH_MAX];
    char buf[64];
    int n;

    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        if (lstat(path, &st) == -1)
            continue;

        n = num_width(st.st_nlink);
        if (n > w->nlink) w->nlink = n;

        owner_name(st.st_uid, buf, sizeof(buf));
        n = (int)strlen(buf);
        if (n > w->owner) w->owner = n;

        group_name(st.st_gid, buf, sizeof(buf));
        n = (int)strlen(buf);
        if (n > w->group) w->group = n;

        n = num_width((unsigned long long)st.st_size);
        if (n > w->size) w->size = n;

        *blocks += st.st_blocks;
    }
}

/* Print ONE line of the long listing for file "name" inside directory "dir" */
void print_long(const char *dir, const char *name, const struct widths *w)
{
    char path[PATH_MAX];
    char owner[64], group[64];
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

    /* 2. number of hard links (right aligned, width = widest value) */
    printf(" %*lu", w->nlink, (unsigned long)st.st_nlink);

    /* 3. owner and group names (left aligned) */
    owner_name(st.st_uid, owner, sizeof(owner));
    group_name(st.st_gid, group, sizeof(group));
    printf(" %-*s", w->owner, owner);
    printf(" %-*s", w->group, group);

    /* 4. size in bytes (right aligned) */
    printf(" %*llu", w->size, (unsigned long long)st.st_size);

    /* 5. modification time */
    print_time(st.st_mtime);

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

/* Recent file: "Oct 14 18:29"   Old (or future) file: "Oct 14  2024" */
void print_time(time_t t)
{
    time_t now = time(NULL);
    char *s = ctime(&t);        /* "Wed Oct 14 18:29:42 2026\n" */

    if (t > now || now - t > SIX_MONTHS)
        printf(" %.7s %.4s", s + 4, s + 20);
    else
        printf(" %.12s", s + 4);
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

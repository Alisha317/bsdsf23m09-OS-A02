/*
* Programming Assignment 02: lsv1.4.0
* Version 1.4.0: names are now sorted alphabetically (qsort)
* Usage:
*       $ ./bin/ls           (down then across)
*       $ ./bin/ls -l        (long listing)
*       $ ./bin/ls -x        (across, left to right)
*       $ ./bin/ls /home /etc
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
#include <sys/ioctl.h>

extern int errno;

#define SIX_MONTHS 15778476   /* seconds; older files show the year, not the time */
#define SPACING    2          /* spaces between two columns */

/* the three ways to display a directory */
enum display_mode
{
    MODE_DEFAULT,             /* columns, down then across */
    MODE_LONG,                /* -l */
    MODE_HORIZONTAL           /* -x */
};

/* widest value of each column of the long listing, so that columns line up */
struct widths
{
    int nlink;
    int owner;
    int group;
    int size;
};

void do_ls(const char *dir, enum display_mode mode);
int  read_entries(const char *dir, char ***names, int *count, int *maxlen);
void free_entries(char **names, int count);
int  compare_names(const void *a, const void *b);
int  get_terminal_width(void);
void print_columns(char **names, int count, int maxlen);
void print_horizontal(char **names, int count, int maxlen);
void compute_widths(const char *dir, char **names, int count, struct widths *w, long long *blocks);
void print_long(const char *dir, const char *name, const struct widths *w);
void print_permissions(mode_t mode);
void print_time(time_t t);

int main(int argc, char *argv[])
{
    int opt;
    enum display_mode mode = MODE_DEFAULT;   /* which display to use */

    /* "lx": -l and -x are valid options; if both are given, the last one wins */
    while ((opt = getopt(argc, argv, "lx")) != -1)
    {
        switch (opt)
        {
        case 'l':
            mode = MODE_LONG;
            break;
        case 'x':
            mode = MODE_HORIZONTAL;
            break;
        default:
            fprintf(stderr, "Usage: %s [-l | -x] [directory...]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind == argc)
    {
        do_ls(".", mode);
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s : \n", argv[i]);
            do_ls(argv[i], mode);
            puts("");
        }
    }
    return 0;
}

void do_ls(const char *dir, enum display_mode mode)
{
    char **names = NULL;        /* dynamic array of file names */
    int count = 0;              /* how many names are in the array */
    int maxlen = 0;             /* length of the longest name */

    /* Step 1: read ALL names first (we must know the longest one) */
    if (read_entries(dir, &names, &count, &maxlen) == -1)
        return;

    /* Step 2: sort the names alphabetically (all display modes use this order) */
    qsort(names, count, sizeof(char *), compare_names);

    /* Step 3: display, using the function that belongs to the chosen mode */
    if (mode == MODE_LONG)
    {
        struct widths w = {1, 1, 1, 1};
        long long blocks = 0;

        compute_widths(dir, names, count, &w, &blocks);
        printf("total %lld\n", (blocks + 1) / 2);

        for (int i = 0; i < count; i++)
            print_long(dir, names[i], &w);
    }
    else if (mode == MODE_HORIZONTAL)
    {
        print_horizontal(names, count, maxlen);
    }
    else
    {
        print_columns(names, count, maxlen);
    }

    /* Step 4: give the memory back */
    free_entries(names, count);
}

/* Read all (non-hidden) names of directory "dir" into a dynamic array.
   Returns 0 on success, -1 on error. */
int read_entries(const char *dir, char ***names, int *count, int *maxlen)
{
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return -1;
    }

    int capacity = 16;
    char **arr = malloc(capacity * sizeof(char *));
    if (arr == NULL)
    {
        perror("malloc");
        closedir(dp);
        return -1;
    }

    int n = 0;
    struct dirent *entry;
    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        /* array is full: double its size */
        if (n == capacity)
        {
            capacity *= 2;
            char **tmp = realloc(arr, capacity * sizeof(char *));
            if (tmp == NULL)
            {
                perror("realloc");
                free_entries(arr, n);
                closedir(dp);
                return -1;
            }
            arr = tmp;
        }

        arr[n] = strdup(entry->d_name);     /* our own copy of the name */
        if (arr[n] == NULL)
        {
            perror("strdup");
            free_entries(arr, n);
            closedir(dp);
            return -1;
        }

        int len = (int)strlen(arr[n]);
        if (len > *maxlen)
            *maxlen = len;
        n++;
    }

    if (errno != 0)
        perror("readdir failed");

    closedir(dp);
    *names = arr;
    *count = n;
    return 0;
}

/* Comparator for qsort. qsort passes pointers to the array ELEMENTS; each element
   is a char *, so the arguments are really char ** (pointer to a string pointer). */
int compare_names(const void *a, const void *b)
{
    const char *s1 = *(const char * const *)a;
    const char *s2 = *(const char * const *)b;
    return strcmp(s1, s2);
}

void free_entries(char **names, int count)
{
    for (int i = 0; i < count; i++)
        free(names[i]);
    free(names);
}

/* Width of the terminal in characters; 80 if it cannot be found */
int get_terminal_width(void)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
        return 80;
    return ws.ws_col;
}

/* Print names in columns, "down then across":
        a  d  g
        b  e  h
        c  f
   The array is filled column by column. */
void print_columns(char **names, int count, int maxlen)
{
    if (count == 0)
        return;

    int width  = get_terminal_width();
    int colw   = maxlen + SPACING;              /* width of one column */
    int cols   = width / colw;                  /* how many columns fit */
    if (cols < 1)
        cols = 1;
    int rows   = (count + cols - 1) / cols;     /* ceiling of count / cols */

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            int idx = r + c * rows;             /* the important formula */
            if (idx >= count)
                break;

            /* pad with spaces only if another name follows in this row */
            int next = r + (c + 1) * rows;
            if (c + 1 < cols && next < count)
                printf("%-*s", colw, names[idx]);
            else
                printf("%s", names[idx]);
        }
        printf("\n");
    }
}

/* Print names "across": left to right, and start a new line when the next
   column would not fit on the current line:
        a  b  c  d
        e  f  g  h
   No row/column calculation is needed, only the current position. */
void print_horizontal(char **names, int count, int maxlen)
{
    int width = get_terminal_width();
    int colw  = maxlen + SPACING;       /* width of one column */
    int pos   = 0;                      /* characters already used on this line */

    for (int i = 0; i < count; i++)
    {
        printf("%s", names[i]);
        pos += colw;

        /* if another name follows AND it still fits on this line:
           pad with spaces up to the column width; otherwise end the line */
        if (i + 1 < count && pos + colw <= width)
        {
            printf("%*s", colw - (int)strlen(names[i]), "");
        }
        else
        {
            printf("\n");
            pos = 0;
        }
    }
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

/* find the widest value of each column and add up the blocks */
void compute_widths(const char *dir, char **names, int count, struct widths *w, long long *blocks)
{
    struct stat st;
    char path[PATH_MAX];
    char buf[64];
    int n;

    for (int i = 0; i < count; i++)
    {
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
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

    snprintf(path, sizeof(path), "%s/%s", dir, name);

    if (lstat(path, &st) == -1)
    {
        perror(path);
        return;
    }

    print_permissions(st.st_mode);
    printf(" %*lu", w->nlink, (unsigned long)st.st_nlink);

    owner_name(st.st_uid, owner, sizeof(owner));
    group_name(st.st_gid, group, sizeof(group));
    printf(" %-*s", w->owner, owner);
    printf(" %-*s", w->group, group);

    printf(" %*llu", w->size, (unsigned long long)st.st_size);
    print_time(st.st_mtime);
    printf(" %s", name);

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

    if (S_ISREG(mode))       perm[0] = '-';
    else if (S_ISDIR(mode))  perm[0] = 'd';
    else if (S_ISLNK(mode))  perm[0] = 'l';
    else if (S_ISCHR(mode))  perm[0] = 'c';
    else if (S_ISBLK(mode))  perm[0] = 'b';
    else if (S_ISFIFO(mode)) perm[0] = 'p';
    else if (S_ISSOCK(mode)) perm[0] = 's';
    else                     perm[0] = '?';

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

    if (mode & S_ISUID) perm[3] = (mode & S_IXUSR) ? 's' : 'S';
    if (mode & S_ISGID) perm[6] = (mode & S_IXGRP) ? 's' : 'S';
    if (mode & S_ISVTX) perm[9] = (mode & S_IXOTH) ? 't' : 'T';

    printf("%s", perm);
}

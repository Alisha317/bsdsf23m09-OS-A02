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
#include <sys/stat.h>

extern int errno;

/* do_ls now also receives long_flag: 1 if user typed -l, else 0 */
void do_ls(const char *dir, int long_flag);

int main(int argc, char *argv[])
{
    int opt;
    int long_flag = 0;          /* 0 = normal listing, 1 = long listing */

    /* getopt reads options like -l one by one; "l" means only -l is valid */
    while ((opt = getopt(argc, argv, "l")) != -1)
    {
        switch (opt)
        {
        case 'l':
            long_flag = 1;      /* user typed -l */
            break;
        default:                /* any unknown option, e.g. -z */
            fprintf(stderr, "Usage: %s [-l] [directory...]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    /* optind = index of the first argument that is NOT an option */
    if (optind == argc)
    {
        do_ls(".", long_flag);  /* no directory given: use current one */
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
            printf("LONG: %s\n", entry->d_name);   /* temporary, replaced in Stage 2 */
        else
            printf("%s\n", entry->d_name);
    }

    if (errno != 0)
    {
        perror("readdir failed");
    }

    closedir(dp);
}

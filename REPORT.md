# Operating Systems - PA-02 Report (ls utility)

---

## Feature 2: ls-v1.1.0 - Complete Long Listing Format (-l)

**Q1. What is the crucial difference between the stat() and lstat() system calls? In the context of the ls command, when is it more appropriate to use lstat()?**

Both calls fill a `struct stat` with the metadata of a file stored in its inode. The difference appears when the path is a symbolic link:

- `stat()` **follows** the link and returns the information of the file the link points to (the target).
- `lstat()` does **not** follow the link; it returns the information of the link itself (its own inode, its own type, its own size).

In `ls` it is more appropriate to use `lstat()`. `ls -l` has to show a symbolic link as a link: the type character must be `l`, the size is the length of the stored path, and the name is followed by `-> target`. With `stat()` a link would be shown as a normal file or directory, and a broken link (target deleted) would make `stat()` fail with an error. My implementation uses `lstat()` for every entry and then `readlink()` to print the target.

**Q2. The st_mode field is an integer that contains both the file type and the permission bits. How can bitwise operators (&) and macros (S_IFDIR, S_IRUSR) be used to extract this information?**

`st_mode` packs several things into different bits: the upper bits hold the file type, the next 3 bits are the special bits (SUID, SGID, sticky), and the lowest 9 bits are the permissions of owner, group and others (3 bits each: r, w, x).

- **File type:** AND the mode with the mask `S_IFMT` and compare with a type constant: `(st_mode & S_IFMT) == S_IFDIR` means directory. The macros `S_ISDIR()`, `S_ISREG()`, `S_ISLNK()`, etc. do exactly this.
- **Permissions:** AND the mode with a permission mask. If the result is non-zero, that bit is set. For example `st_mode & S_IRUSR` tells whether the owner can read, `st_mode & S_IWGRP` whether the group can write, `st_mode & S_IXOTH` whether others can execute.
- **Special bits:** `st_mode & S_ISUID`, `S_ISGID`, `S_ISVTX`. When set, the execute position shows `s` or `t` (capital `S` or `T` if the execute bit itself is not set).

In my code `print_permissions()` checks each of the nine bits with `&` and builds the string such as `-rwxr-xr-x`.

---

## Feature 3: ls-v1.2.0 - Column Display (Down Then Across)

**Q1. Explain the general logic for printing items in a "down then across" columnar format. Why is a simple single loop through the list of filenames insufficient?**

First all file names are read into a dynamic array and the length of the longest name (`maxlen`) is found. Then the layout is calculated:

- column width = `maxlen + 2` (two spaces between columns)
- number of columns = `terminal_width / column_width` (at least 1)
- number of rows = `ceil(count / columns)`

The names are stored in the array in order, but they must be printed column by column: the first column holds names 0 .. rows-1, the second column holds the next `rows` names, and so on. So the program prints row by row, and for row `r` and column `c` it prints `names[r + c * rows]`. Each name is padded with spaces to the column width so the columns line up.

A single loop is not enough because it would print the names in array order from left to right, which gives the "across" order. In "down then across" the neighbours of a name in the same row are `rows` positions apart in the array, so we need nested loops (rows and columns) and an index calculation, and we need to know the number of rows before printing the first line.

**Q2. What is the purpose of the ioctl system call in this context? What would be the limitations of your program if you only used a fixed-width fallback (e.g., 80 columns) instead of detecting the terminal size?**

`ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)` asks the terminal driver for the current window size. It fills a `struct winsize`, and `ws.ws_col` is the number of columns (characters per line) of the terminal. The program uses it to decide how many columns of names fit on one line.

With a fixed width of 80:

- on a wide terminal, space is wasted because fewer columns are used than could fit;
- on a narrow terminal (or after the window is resized), lines are longer than the screen and wrap, which breaks the alignment and makes the output hard to read;
- the output never adapts when the user resizes the window.

The fixed value of 80 is only used as a fallback when `ioctl` fails, for example when the output is redirected to a file or a pipe and there is no terminal.

---

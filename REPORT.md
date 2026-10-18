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
## Feature 4: ls-v1.3.0 - Horizontal Column Display (-x)

**Q1. Compare the implementation complexity of the "down then across" (vertical) printing logic versus the "across" (horizontal) printing logic. Which one requires more pre-calculation and why?**

The horizontal ("across") logic is simpler. It only needs the terminal width, the column width (`longest name + 2`) and a running counter of how many characters are already used on the current line. The names are printed in array order, left to right, and a newline is printed when the next column would not fit. It uses a single loop, and no index arithmetic or knowledge of the total number of rows is needed.

The vertical ("down then across") logic needs more pre-calculation. Before printing anything, the program must calculate the number of columns (`width / column width`) and then the number of rows (`ceil(count / columns)`), because the position of every name depends on the total count of names. The printing needs two nested loops and the index formula `names[r + c * rows]`, and it has to handle the last column, which may be incomplete (index >= count), and pad only when another name follows in the row. So the vertical layout requires more pre-calculation, because a name's place is only known after the number of rows is known.

**Q2. Describe the strategy you used in your code to manage the different display modes (-l, -x, and default). How did your program decide which function to call for printing?**

I used an enumeration `enum display_mode { MODE_DEFAULT, MODE_LONG, MODE_HORIZONTAL }` and one variable `mode` in `main()`, initialised to `MODE_DEFAULT`. The `getopt()` loop (option string `"lx"`) sets `mode = MODE_LONG` for `-l` and `mode = MODE_HORIZONTAL` for `-x`; if both are given, the last one wins. The chosen mode is passed to `do_ls(dir, mode)`.

Inside `do_ls()` all file names are first read into a dynamic array. Then the mode is checked with an if / else-if / else chain: `MODE_LONG` calls `compute_widths()` and `print_long()` for every entry, `MODE_HORIZONTAL` calls `print_horizontal()`, and anything else (the default) calls `print_columns()` (down then across). Because the mode is a single variable and the reading of names is shared, a new display mode only needs a new enum value, a new option in `getopt()` and a new print function.

---
## Feature 5: ls-v1.4.0 - Alphabetical Sort

**Q1. Why is it necessary to read all directory entries into memory before sorting? What are the potential drawbacks of this approach for directories containing millions of files?**

`readdir()` returns the entries in the order in which the file system stores them, which is not alphabetical, and it can only be read once from start to end. A sort needs to compare any entry with any other entry, so the smallest name could be the very last one returned. Therefore the program cannot print a name until it has seen all of them. For this reason all the names are first stored in a dynamic array (`malloc`/`realloc`), then sorted with `qsort()`, and only then displayed. The same array is also needed to find the longest name for the column layout.

The drawback is memory and time. Memory use grows with the number of entries (the array of pointers plus a copy of every name made with `strdup`), so a directory with millions of files may need hundreds of megabytes and the program can become slow or even fail if `malloc` returns NULL. Nothing can be printed until reading and sorting are finished, so the user waits and sees no output at first. Sorting also costs O(n log n) time. A real `ls` has the same limitation when sorting; an unsorted listing (like `ls -f`) can print every entry immediately while reading, using constant memory.

**Q2. Explain the purpose of the comparison function required by `qsort()`. How does it work, and why does it take `const void *` arguments?**

`qsort()` is a generic function: it can sort an array of any type, but it does not know how to compare two elements. The comparison function is supplied by the programmer and tells `qsort()` the order of two elements. It must return a negative value if the first element should come before the second, zero if they are equal, and a positive value if the first should come after the second. `qsort()` calls it many times while sorting.

The arguments are `const void *` because `qsort()` must work with any element type, and a `void *` can point to anything. The `const` promises that the comparison will not modify the elements. Inside the function the pointers must be cast back to the real type. In my program the array elements are strings (`char *`), and `qsort()` passes the address of each element, so the arguments really point to a `char *`. I cast each argument to `const char * const *`, dereference it to get the string, and return `strcmp(s1, s2)`, which already returns a negative, zero or positive value.

---
## Feature 6: ls-v1.5.0 - Colorized Output

**Q1. How do ANSI escape codes work for terminal colors? Show the specific sequence for printing text in green.**

ANSI escape codes are special byte sequences that are written to the terminal like normal text, but the terminal does not display them; it interprets them as commands. A sequence starts with the ESC character (ASCII 27, written `\033` in octal in C), followed by `[`, then one or more numeric codes separated by `;`, and ends with the letter `m` (the "select graphic rendition" command): `ESC [ <code> m`. Codes 30-37 set the text color (31 red, 32 green, 34 blue, 35 magenta/pink), 1 means bold and 7 means reverse video (swap foreground and background). The color stays active until it is changed, so the program must print the reset sequence `\033[0m` after the file name; otherwise all the following text would also be coloured.

To print text in green: `printf("\033[0;32m%s\033[0m", name);` The part `\033[0;32m` switches to the normal (not bold) style with green text, `%s` is the file name, and `\033[0m` resets the terminal back to the default color. The escape characters take no space on the screen, but they are counted by `printf` width formats such as `%-10s`, so in my code the padding is calculated from the real name length and printed separately to keep the columns aligned.

**Q2. To color an executable file differently, you need to check its permission bits. Explain which bits in the `st_mode` field you need to check and how.**

The `st_mode` field returned by `lstat()` contains the file type in its upper bits and the permission bits in its lower 9 bits (owner, group, others, each with read, write and execute). To know whether a file is executable I test the three execute bits using the macros `S_IXUSR` (owner execute, 0100), `S_IXGRP` (group execute, 0010) and `S_IXOTH` (others execute, 0001), combined with the bitwise AND operator: `if (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))`. The expression is non-zero if at least one of the three execute bits is set, and the file is then printed in green. This is checked only after the file has been identified as an ordinary file (the type is tested first with macros like `S_ISDIR` and `S_ISLNK`, which compare the type bits with `S_IFMT`), because directories also have execute bits (search permission) but must be shown in blue.

---

## Feature 7: ls-v1.6.0 - Recursive Listing (-R)

**Q1. In a recursive function, what is a "base case"? Identify the base case in your recursive `ls` implementation.**

A base case is the condition under which a recursive function stops calling itself and returns directly. Without it the function would call itself forever, and the program would crash with a stack overflow. Every recursive call must move closer to the base case.

In my `do_ls()` the recursion happens inside a loop over the entries of the current directory: for every entry that is a directory, `do_ls()` is called again for it. The base case is a directory that contains no sub-directories (for example a directory with only ordinary files, or an empty directory, or one that cannot be opened). In such a directory the loop does not find any entry for which `S_ISDIR()` is true, so no further recursive call is made, the function prints its entries, frees its memory and returns to the caller. Each recursive call works on a sub-directory, which is one level deeper in the directory tree, and a tree is finite, so the recursion always reaches the base case. The entries `.` and `..` are explicitly skipped, and symbolic links are not followed (`lstat` is used), because otherwise the recursion could go in circles and never end.

**Q2. Explain why it is essential to construct a full path (e.g., "parent_dir/subdir") before making a recursive call. What would happen if you simply tried to call `do_ls()` with just the sub-directory name?**

`readdir()` returns only the name of an entry (for example `src`), not its location. System calls such as `opendir()` and `lstat()` interpret a relative name starting from the current working directory of the program, not from the directory that is being listed. So after listing `/home/me/project`, calling `do_ls("src")` would look for `./src` in the directory from which the program was started. If it does not exist, the program prints "Cannot open directory", and if a different directory with the same name exists there, the program would list the wrong directory without any error. The `lstat()` calls used for the long listing and for the colors would also fail or give wrong information for the same reason.

Therefore, before every recursive call I build the full path by joining the parent directory, a `/` and the entry name (`build_path()` gives `/home/me/project/src`), and I pass this path to `do_ls()`. The same path is used to check with `lstat()` whether the entry is a directory, and it is printed as the header of the sub-directory (`/home/me/project/src:`), so the user can see which directory is listed. Deeper levels simply extend the path further (`.../src/lib`), so every call always has an exact location.

---

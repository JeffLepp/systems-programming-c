# unix-filesystem-sim

A simplified Unix-style filesystem simulator written in C.

The filesystem is represented as a tree using child and sibling pointers. Each
node represents either a directory or a file. The program runs as an
interactive shell that accepts filesystem-style commands.

Build:
gcc -o fssim *.c

Run:
./fssim

Usage:
Enter commands interactively after startup.

Supported commands:
mkdir <pathname>
rmdir <pathname>
creat <pathname>
rm <pathname>
cd [pathname]
ls [pathname]
pwd
save <file>
reload <file>
quit

Both absolute and relative paths are supported.

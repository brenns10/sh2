sh2
===

sh2 is a work-in-progress shell implementation in C. It aims to demonstrate
concepts behind a shell beyond the basics. While my original [lsh][]
demonstrated the basics, it left a lot of room for more: piping, environment
variables & expansions, conditions, loops, functions, etc. This shell aims to
bridge that gap. It's not necessarily intending to be 100% POSIX-compliant, but
rather to implement most of the features present there in a way that is
accessible to learners.

[lsh]: https://github.com/brenns10/lsh

Building & Running
==================

sh2 requires meson, ninja, and a C compiler. The following commands will build
and run it:

``` sh
meson setup build
ninja -C build
build/sh2
```

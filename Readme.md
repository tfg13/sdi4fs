# sdi4fs
A simple, (mostly) log-structured file system

sdi4fs was written in 2015 for a university project.
The primary target was to design and implement a flash-friendly file system that runs as a userspace driver server for systems based on [L4Re](https://os.inf.tu-dresden.de/L4Re/) and the [FIASCO.OC](https://os.inf.tu-dresden.de/L4Re/fiasco/) microkernel.

## Design
* Mostly log-structured for flash-friendliness (for devices with a basic flash translation layer (FTL), like SD cards)
* Fast mounting via snapshots (after correct unmount)
* Automatic block map reconstruction if snapshot is missing
* Full POSIX-like support for files, directories and hardlinks
* Reasonable size limits:
 * 16TiB max fs size
 * 4GiB max file size
 * 129413 hardlinks per directory (>2x ext4!)
 
See [sdi4fs_spec](https://github.com/tfg13/sdi4fs/blob/master/sdi4fs_spec) for details.

## Scope & Linux support
This repository contains everything I wrote and was able to trivially
release as FOSS. The full system also featured an SD card driver,
a block device server, a stream-like IPC mechanism and an L4Re compatible
frontend server for the file system.

This repository contains the full fs code without any L4 dependencies,
which allows for compiling and running sdi4fs on Linux without any modifications.

Since sdi4fs was designed to be accessed through a server on the mircokernel system,
this repository only contains "libsdi4fs" - the file system itself without any frontend.
To use sdi4fs on Linux, one needs to `#include FS.h` and use the API directly.
In absence of the original SD card driver, sdi4fs can use a regular file as device.
See [linux_main.cc](https://github.com/tfg13/sdi4fs/blob/master/linux_main.cc) as an example.

## Code & Compiling
Written in C++, mostly conforms to the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).
Running `make` should work on any half-modern Linux machine with support for C++11.

## License
Apache 2.0, see [LICENSE](https://github.com/tfg13/sdi4fs/blob/master/LICENSE)

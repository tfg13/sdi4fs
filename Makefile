# Default target.
all:

CC = g++
OPT = -O0
#DEV_LINUX leaves all L4 dependencies out and uses std::iostream
CFLAGS = -DDEV_LINUX -Wall -std=c++11 -g $(OPT)

FS.o: FS.cc FS.h StreamUtils.inc Constants.inc PathUtils.inc
	$(CC) $(CFLAGS) $(XFLAGS) -c FS.cc -o $@

Block.o: Block.cc Block.h StreamUtils.inc
	$(CC) $(CFLAGS) $(XFLAGS) -c Block.cc -o $@

INode.o: INode.cc INode.h StreamUtils.inc
	$(CC) $(CFLAGS) $(XFLAGS) -c INode.cc -o $@

DirectoryINode.o: DirectoryINode.cc DirectoryINode.h
	$(CC) $(CFLAGS) $(XFLAGS) -c DirectoryINode.cc -o $@

Directory.o: Directory.cc Directory.h
	$(CC) $(CFLAGS) $(XFLAGS) -c Directory.cc -o $@

DirectoryEntryList.o: DirectoryEntryList.cc DirectoryEntryList.h
	$(CC) $(CFLAGS) $(XFLAGS) -c DirectoryEntryList.cc -o $@

Hardlink.o: Hardlink.cc Hardlink.h
	$(CC) $(CFLAGS) $(XFLAGS) -c Hardlink.cc -o $@

FileINode.o: FileINode.cc FileINode.h StreamUtils.inc
	$(CC) $(CFLAGS) $(XFLAGS) -c FileINode.cc -o $@

File.o: File.cc File.h StreamUtils.inc
	$(CC) $(CFLAGS) $(XFLAGS) -c File.cc -o $@

DataBlockList.o: DataBlockList.cc DataBlockList.h StreamUtils.inc
	$(CC) $(CFLAGS) $(XFLAGS) -c DataBlockList.cc -o $@

DataBlock.o: DataBlock.cc DataBlock.h StreamUtils.inc
	$(CC) $(CFLAGS) $(XFLAGS) -c DataBlock.cc -o $@

linux_main.o: linux_main.cc
	$(CC) $(CFLAGS) $(XFLAGS) -c $< -o $@

linux_main:  linux_main.o FS.o Block.o INode.o DirectoryINode.o Directory.o DirectoryEntryList.o Hardlink.o FileINode.o File.o DataBlockList.o DataBlock.o
	$(CC) $(LDFLAGS) $(XFLAGS) $^ -o $@

mkfs.sdi4fs.linux.o: mkfs.sdi4fs.linux.cc
	$(CC) $(CFLAGS) $(XFLAGS) -c $< -o $@

mkfs.sdi4fs:  mkfs.sdi4fs.linux.o Block.o INode.o DirectoryINode.o Directory.o DirectoryEntryList.o Hardlink.o
	$(CC) $(LDFLAGS) $(XFLAGS) $^ -o $@

all: linux_main mkfs.sdi4fs

clean:
	rm -f *.o

.PHONY: all clean
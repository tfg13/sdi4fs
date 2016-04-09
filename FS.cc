/*
 * File:   FS.cpp
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on August 14, 2015, 5:03 PM
 */

#include "FS.h"

#include <cmath>
#include <iostream>
#include <list>
#include <memory>
#include <unordered_map>
#include <sstream>

#include "Constants.inc"
#include "PathUtils.inc"
#include "StreamUtils.inc"
#include "INode.h"
#include "IDataBlockListCreator.h"
#include "IDirectoryEntryListCreator.h"
#include "IPrimaryINodeHolder.h"
#include "DirectoryINode.h"
#include "Directory.h"
#include "FileINode.h"
#include "File.h"
#include "ListUtils.inc"
#include "TimeUtils.inc"

namespace SDI4FS {

FS::FS(STREAM &dev)
: dev(dev), bmapStart_bptr(SDI4FS_HEADER_SIZE),
dev_bmap_valid(false) {
    std::cout << "fs: accessing block device..." << std::endl;
    // read header
    if (!readHeader()) {
        std::cout << "fs: error - invalid header" << std::endl;
        return;
    }
    // calc positions of imap etc
    calcLayout();
    // more sanity checks, now that the positions are known
    if (usedBlocks > logSize || write_ptr > logSize) {
        std::cout << "fs: error - invalid header (step 2)" << std::endl;
        return;
    }

    // callbacks (block creators)
    initCallbacks();

    // alloc memory for bmap
    std::cout << "fs: alloc " << bmapSize_b << " bytes of memory for block map" << std::endl;
    bmap = (uint32_t*) calloc(1, bmapSize_b);
    if (bmap == NULL) {
        std::cout << "fs: error - cannot allocate memory for bmap, ERRNO " << bmap << std::endl;
        return;
    }

    // load or reconstruct bmap
    if (dev_bmap_valid) {
        // load bmap
        if (!loadBMap()) {
            std::cout << "fs: error - cannot load bmap" << std::endl;
            return;
        }
#ifndef DEV_LINUX
        // for systems without rtc
        dev.seekg(32);
        read32(dev, &pseudoTime);
#endif // DEV_LINUX
    } else {
        // reconstruct bmap
        std::cout << "fs: detected invalid previous unmount. bmap reconstruction required, please stand by..." << std::endl;
        reconstructBMap();
    }

    // mark bmap dirty (fs mounted)
    dev.seekp(20);
    write32(dev, 0);

    // all ok
    std::cout << "fs: " << size_b << "B total, " << usedBlocks << " of " << logSize << " blocks in use" << std::endl;
    std::cout << "fs: ready." << std::endl;
}

void FS::umount() {
    saveBMap();
    // delete bmap
    free(bmap);
    bmap = 0;
    // save write_ptr
    dev.seekp(16);
    write32(dev, write_ptr);
    // save next blockID
    dev.seekp(24);
    write32(dev, nextBlockID);
    // save number of used blocks
    write32(dev, usedBlocks);
    // umount time
#ifdef DEV_LINUX
    write32(dev, now());
#else
    write32(dev, pseudoTime++);
#endif
    dev.seekp(20);
    // mark unmount complete
    write32(dev, 1);
    // make sure changes were written to disk
    dev.flush();
    std::cout << "fs: unmount ok." << std::endl;
}

FS::~FS() {
    delete dirEntryListCreator;
}

bool FS::readHeader() {
    dev.seekg(0);
    uint32_t magic;
    read32(dev, &magic);
    // verify magic
    if (magic != SDI4FS_MAGIC) {
        std::cout << "error, wrong magic, expected " << SDI4FS_MAGIC << ", got " << magic << std::endl;
        return false;
    }
    // fs size
    dev.seekg(8);
    read64(dev, &size_b);
    // sanity check
    if (size_b < SDI4FS_FS_MIN_SIZE || size_b > SDI4FS_FS_MAX_SIZE) {
        std::cout << "error, invalid size, got " << size_b << std::endl;
        return false;
    }
    // next write pos
    read32(dev, &write_ptr);
    // sanity check
    if (write_ptr == 0) {
        std::cout << "error, invalid next write pos, got " << write_ptr << std::endl;
        return false;
    }

    // last umount ok?
    uint32_t bmap_valid;
    read32(dev, &bmap_valid);
    if (bmap_valid == 1) {
        dev_bmap_valid = true;
    }

    // nextBlockID
    read32(dev, &nextBlockID);

    // used blocks
    read32(dev, &usedBlocks);
    // sanity check
    if (usedBlocks == 0) {
        std::cout << "error, invalid number of used blocks, got zero" << std::endl;
        return false;
    }

    return true;
}

void FS::calcLayout() {
    // bmap requires min 1/1024 of total size (rounded up to 4K blocks)

    bmapSize_b = ceil(((size_b - SDI4FS_HEADER_SIZE) / 1024) / 4096.0) * 4096;
    // log starts after bmap
    logStart_bptr = bmapStart_bptr + bmapSize_b;
    // log fills up rest
    logSize = (size_b - SDI4FS_HEADER_SIZE - bmapSize_b) / 4096;
}

bool FS::loadBMap() {
    dev.seekg(bmapStart_bptr);
    dev.read((char*) bmap, bmapSize_b);

    return !dev.fail();
}

void FS::saveBMap() {
    dev.seekp(bmapStart_bptr);
    dev.write((char*) bmap, bmapSize_b);
    if (dev.fail()) {
        std::cout << "fs: error - cannot save bmap" << std::endl;
    }
}

uint32_t FS::lookupBlockAddress(uint32_t id) {
    // sanity checks:
    // zero is not a valid block id
    // prevent buffer overrun (logSize is lower bound for size of bmap in blocks)
    if (id == 0 || id > logSize) {
        std::cout << "fs: error - request for invalid block: " << id << std::endl;

        return 0;
    }
    return bmap[id - 1];
}

std::unique_ptr<Directory> FS::loadDirectory(uint32_t id) {
    // get pos in log
    uint32_t logPtr = lookupBlockAddress(id);
    if (logPtr == 0 || logPtr > logSize) {
        std::cout << "fs: error - inode not found: " << id << std::endl;
        return std::unique_ptr<Directory>(nullptr);
    }
    // seek to pos
    dev.seekg(logStart_bptr + ((logPtr - 1) * SDI4FS_BLOCK_SIZE));
    // read inode
    std::unique_ptr<DirectoryINode> inode(new DirectoryINode(dev));
    // sanity checks
    if (inode->getId() != id) {
        std::cout << "fs: error - inconsistency, tried to load inode " << id << ", but got " << inode->getId() << std::endl;
        return std::unique_ptr<Directory>(nullptr);
    }
    if (inode->getType() != SDI4FS_INODE_TYPE_DIR) {
        std::cout << "fs: error - inconsistency, tried to load inode with type " << SDI4FS_INODE_TYPE_DIR << ", but got " << inode->getType() << std::endl;
        return std::unique_ptr<Directory>(nullptr);
    }

    std::list<uint32_t> entryListIDs;
    std::unique_ptr<Directory> dir(new Directory(dirEntryListCreator, std::move(inode), &entryListIDs));
    // dir constructor fills list if additional blocks must be loaded
    if (!entryListIDs.empty()) {
        std::vector<std::unique_ptr < DirectoryEntryList>> initList;
        for (auto &list : entryListIDs) {
            std::unique_ptr<DirectoryEntryList> newDirEntryList = loadDirEntryList(list);
            if (!newDirEntryList) {
                std::cout << "fs: error - unable to load directory " << dir->getPrimaryINode().getId() << ", requested dirEntryList " << list << " not found" << std::endl;
                return std::unique_ptr<Directory>(nullptr);
            }
            // block ok
            initList.push_back(std::move(newDirEntryList));
        }
        dir->init(initList);
    }
    return dir;
}

std::unique_ptr<DirectoryEntryList> FS::loadDirEntryList(uint32_t id) {
    // get pos in log
    uint32_t logPtr = lookupBlockAddress(id);
    if (logPtr == 0 || logPtr > logSize) {
        std::cout << "fs: error - dirEntryList not found: " << id << std::endl;
        return std::unique_ptr<DirectoryEntryList>(nullptr);
    }
    // seek to pos
    dev.seekg(logStart_bptr + ((logPtr - 1) * SDI4FS_BLOCK_SIZE));
    std::unique_ptr<DirectoryEntryList> newDirEntryList(new DirectoryEntryList(dev));
    // sanity checks
    if (newDirEntryList->getId() != id) {
        std::cout << "fs: error - inconsistency, tried to load dirEntryList " << id << ", but got " << newDirEntryList->getId() << std::endl;
        return std::unique_ptr<DirectoryEntryList>(nullptr);
    }
    return newDirEntryList;
}

std::unique_ptr<File> FS::loadFile(uint32_t id) {
    // get pos in log
    uint32_t logPtr = lookupBlockAddress(id);
    if (logPtr == 0 || logPtr > logSize) {
        std::cout << "fs: error - inode not found: " << id << std::endl;
        return std::unique_ptr<File>(nullptr);
    }
    // seek to pos
    dev.seekg(logStart_bptr + ((logPtr - 1) * SDI4FS_BLOCK_SIZE));
    // read inode
    std::unique_ptr<FileINode> inode(new FileINode(dev));
    // sanity checks
    if (inode->getId() != id) {
        std::cout << "fs: error - inconsistency, tried to load inode " << id << ", but got " << inode->getId() << std::endl;
        return std::unique_ptr<File>(nullptr);
    }
    if (inode->getType() != SDI4FS_INODE_TYPE_REGULARFILE) {
        std::cout << "fs: error - inconsistency, tried to load inode with type " << SDI4FS_INODE_TYPE_REGULARFILE << ", but got " << inode->getType() << std::endl;
        return std::unique_ptr<File>(nullptr);
    }

    std::list<uint32_t> dataBlockListIDs;
    std::unique_ptr<File> file(new File(dataBlockListCreator, std::move(inode), &dataBlockListIDs));
    // dir constructor fills list if additional blocks must be loaded
    if (!dataBlockListIDs.empty()) {
        std::vector<std::unique_ptr < DataBlockList>> initList;
        for (auto &list : dataBlockListIDs) {
            std::unique_ptr<DataBlockList> newFileDataBlockList = loadDataBlockList(list);
            if (!newFileDataBlockList) {
                std::cout << "fs: error - unable to load file " << file->getPrimaryINode().getId() << ", requested dataBlockList " << list << " not found" << std::endl;
                return std::unique_ptr<File>(nullptr);
            }
            // block ok
            initList.push_back(std::move(newFileDataBlockList));
        }
        file->init(initList);
    }
    return file;
}

std::unique_ptr<DataBlockList> FS::loadDataBlockList(uint32_t id) {
    // get pos in log
    uint32_t logPtr = lookupBlockAddress(id);
    if (logPtr == 0 || logPtr > logSize) {
        std::cout << "fs: error - dataBlockList not found: " << id << std::endl;
        return std::unique_ptr<DataBlockList>(nullptr);
    }
    // seek to pos
    dev.seekg(logStart_bptr + ((logPtr - 1) * SDI4FS_BLOCK_SIZE));
    std::unique_ptr<DataBlockList> newDataBlockList(new DataBlockList(dev));
    // sanity checks
    if (newDataBlockList->getId() != id) {
        std::cout << "fs: error - inconsistency, tried to load DataBlockList " << id << ", but got " << newDataBlockList->getId() << std::endl;
        return std::unique_ptr<DataBlockList>(nullptr);
    }
    return newDataBlockList;
}

std::unique_ptr<DataBlock> FS::loadDataBlock(uint32_t id) {
    // get pos in log
    uint32_t logPtr = lookupBlockAddress(id);
    if (logPtr == 0 || logPtr > logSize) {
        std::cout << "fs: error - dataBlock not found: " << id << std::endl;
        return std::unique_ptr<DataBlock>(nullptr);
    }
    // seek to pos
    dev.seekg(logStart_bptr + ((logPtr - 1) * SDI4FS_BLOCK_SIZE));
    std::unique_ptr<DataBlock> newDataBlock(new DataBlock(dev));
    // sanity
    if (newDataBlock->getId() != id) {
        std::cout << "fs: error - inconsistency, tried to load DataBlock " << id << ", but got " << newDataBlock->getId() << std::endl;
        return std::unique_ptr<DataBlock>(nullptr);
    }
    return newDataBlock;
}

void FS::reconstructBMap() {
    // STEP 1: reconstruct/estimate some header values:
    // - write_ptr (next write pos in log)
    // - nextBlockID
    // for the write_ptr,  search the log for the block with the latest write date
    uint32_t lastWritePtr = 0;
    uint32_t latestWriteTime = 0;
    nextBlockID = 0;
    for (uint32_t i = 0; i < logSize; ++i) {
        // read block
        dev.seekg(logStart_bptr + (i * SDI4FS_BLOCK_SIZE));
        uint32_t id;
        uint32_t writeTime;
        read32(dev, &id);
        read32(dev, &writeTime);
        // check valid, newer (write_ptr)
        if (id != 0 && writeTime >= latestWriteTime) {
            latestWriteTime = writeTime;
            lastWritePtr = i + 1;
        }
        // search biggest blockID (nextBlockID)
        if (id != 0 && nextBlockID < id) {
            nextBlockID = id;
        }
    }

    // search done, nextBlockID is biggest Block + 1 (wrap-around is ok)
    // (wrong values make gc slower, but are no problem for fs consistency)
    ++nextBlockID;
    std::cout << "fs: recovered last nextBlockID: " << nextBlockID << " (estimated)" << std::endl;

    // save recovered write_ptr (slightly off values here are not critical for fs operation, only for device wear leveling)
    write_ptr = lastWritePtr + 1;
    if (write_ptr > logSize) {
        write_ptr = 1;
    }
    std::cout << "fs: recovered last write_ptr: " << write_ptr << " (estimated)" << std::endl;

#ifndef DEV_LINUX
    // for systems without rtc
    pseudoTime = latestWriteTime + 1;
    std::cout << "fs: set next pseudo timestamp to " << pseudoTime << std::endl;
#endif // DEV_LINUX

    // STEP 2: now that the write_ptr is estimated, we iterate the log again, but starting from the write_ptr
    // by doing this, blocks with the same id AND date that are found later are almost certainly newer
    // Also, count the number of valid blocks
    usedBlocks = 0;

    // required to resolve conflicts
    uint32_t *latestWriteTimes = (uint32_t*) calloc(1, logSize * sizeof (uint32_t));

    for (uint32_t i = 0; i < logSize; ++i) {
        // shift + wrap-around
        uint32_t j = i + lastWritePtr;
        if (j >= logSize) {
            j -= logSize;
        }
        // read block
        dev.seekg(logStart_bptr + (j * SDI4FS_BLOCK_SIZE));
        // read id, writeTime
        uint32_t id;
        uint32_t lastWriteTime;
        read32(dev, &id);
        read32(dev, &lastWriteTime);
        // valid block?
        if (id == 0) {
            continue;
        }
        std::cout << "fs: found block with id " << id << " at pos " << j + 1 << " writeTime " << lastWriteTime; // no std::endl
        // seen before?
        if (bmap[id - 1] == 0) {
            ++usedBlocks;
        }
        // newer block (less *and* equal!)
        if (latestWriteTimes[id - 1] <= lastWriteTime) {
            std::cout << " -> saved" << std::endl;
            // put in bmap
            bmap[id - 1] = j + 1;
            latestWriteTimes[id - 1] = lastWriteTime;
        } else {
            std::cout << " -> outdated" << std::endl;
        }
    }

    free(latestWriteTimes);

    // STEP 3: depth-first traversal, to filter out unreachable INodes/Blocks
    bool *bmapFilter = new bool[logSize];
    for (uint32_t i = 0; i < logSize; ++i) {
        bmapFilter[i] = false;
    }

    std::unique_ptr<Directory> rootDir = loadDirectory(1);
    recursiveRecovery(bmapFilter, *rootDir.get());

    for (uint32_t i = 0; i < logSize; ++i) {
        if (!bmapFilter[i]) {
            // block not reachable, remove if previously found
            if (bmap[i]) {
                std::cout << "fs: unreachable block @ " << i << " removed from bmap" << std::endl;
                bmap[i] = 0;
                --usedBlocks;
            }
        }
    }

    delete bmapFilter;

    // sanity checks for recovery results
    if (usedBlocks == 0) {
        std::cout << "fs: error - recovery failed, zero blocks found" << std::endl;
    }
}

void FS::recursiveRecovery(bool *bmapFilter, Directory &dir) {
    // mark dir + all dirEntryLists reachable
    bmapFilter[dir.getPrimaryINode().getId() - 1] = true;
    for (DirectoryEntryList *block : dir.blocks()) {
        bmapFilter[block->getId() - 1] = true;
    }
    // handle links
    std::list<std::string> links;
    dir.ls(links);
    for (std::string link : links) {
        // do nothing for "." and ".."
        if (link.compare(".") == 0 || link.compare("..") == 0) {
            continue;
        }
        uint32_t linkID = dir.searchHardlink(link);
        std::unique_ptr<Directory> childDir;
        std::unique_ptr<File> file;
        std::list<uint32_t> fileBlockIDs;
        switch (peekINodeType(linkID)) {
            case SDI4FS_INODE_TYPE_DIR:
                childDir = loadDirectory(linkID);
                recursiveRecovery(bmapFilter, *childDir.get());
                break;
            case SDI4FS_INODE_TYPE_REGULARFILE:
                // load file, mark all blocks reachable
                file = loadFile(linkID);
                file->blocks(fileBlockIDs);
                for (uint32_t blockID : fileBlockIDs) {
                    bmapFilter[blockID - 1] = true;
                }
                break;
            default:
                // huh?
                std::cout << "fs: traversal found unknown INode type, id " << linkID << std::endl;
        }
    }
}

void FS::initCallbacks() {
    // anon impl for IDirectoryEntryListCreator

    class DirEntryListAllocator : public IDirectoryEntryListCreator {
    public:

        DirEntryListAllocator(FS *fs) : fs(fs) {
        }

        virtual DirectoryEntryList* alloc() {
            // sanity check
            if (fs->usedBlocks + 1 > fs->logSize) {
                // should never happen, FS checks free space before calling methods in dir object
                std::cout << "fs: cannot create new DirEntryList, fs is full" << std::endl;
                return NULL;
            }
            // alloc
            DirectoryEntryList *newDirEntryList = new DirectoryEntryList(fs->getNextBlockID());
            // do *not* store, this is the responsibility of the dir object
            return newDirEntryList;
        }

        virtual void dealloc(DirectoryEntryList *block) {
            fs->freeBlock(block->getId());
            delete block;
        }

        virtual ~DirEntryListAllocator() {
        }
    private:
        FS *fs;
    };
    dirEntryListCreator = new DirEntryListAllocator(this);

    // anon impl for IDataBlockListCreator

    class DataBlockListCreator : public IDataBlockListCreator {
    public:

        DataBlockListCreator(FS *fs) : fs(fs) {
        }

        virtual DataBlockList* alloc() {
            // sanity check
            if (fs->usedBlocks + 1 > fs->logSize) {
                // full, user will have to stop writing to this file
                return NULL;
            }
            // alloc
            DataBlockList *newDataBlockList = new DataBlockList(fs->getNextBlockID());
            // do *not* store, this is the responsibility of the file object
            return newDataBlockList;
        }

        virtual void dealloc(DataBlockList *block) {
            fs->freeBlock(block->getId());
            delete block;
        }

        virtual ~DataBlockListCreator() {
        }
    private:
        FS *fs;
    };
    dataBlockListCreator = new DataBlockListCreator(this);
}

bool FS::mkdir(std::string absolutePath) {
    absolutePath = normalizePath(absolutePath);
    if (absolutePath.find_first_of("/") != 0) {
        // not an absolute path
        std::cout << "fs: mkdir: cannot create dir with path \"" << absolutePath << "\", path is not absolute" << std::endl;
        return false;
    }
    // this requires at least 4 free blocks (1 for new dir, 1 for updated parent, (rare:) 2 for parent switching from inline to non-inline)
    if (usedBlocks + 4 > logSize) {
        std::cout << "fs: mkdir: cannot create new directory, fs is full" << std::endl;
        return false;
    }
    // find parent node
    std::unique_ptr<Directory> parent = searchParent(absolutePath);
    // parent exists?
    if (!parent) {
        std::cout << "fs: mkdir: cannot create dir with path \"" << absolutePath << "\", parent does not exist" << std::endl;
        return false;
    }

    // child already existing?
    if (parent->searchHardlink(lastName(absolutePath)) != 0) {
        std::cout << "fs: mkdir: cannot create dir with path \"" << absolutePath << "\", dir exists" << std::endl;
        return false;
    }

    // make sure parent can handle one more child
    if (parent->getChildCount() == SDI4FS_MAX_HARDLINKS_PER_DIR) {
        std::cout << "fs: mkdir: cannot create new directory, max # of links in parent dir reached, parent " << parent->getPrimaryINode().getId() << std::endl;
        return false;
    }
    // prevent link counter overflow in parent
    if (parent->getPrimaryINode().getLinkCounter() == SDI4FS_MAX_NUMBER_OF_LINKS_TO_INODE) {
        std::cout << "fs: mkdir: cannot create new directory, max # of links to parent reachedm, parent " << parent->getPrimaryINode().getId() << std::endl;
        return false;
    }

    // get id for new block
    uint32_t newBlockID = getNextBlockID();
    // alloc new block
    std::unique_ptr<DirectoryINode> newDirINode(new DirectoryINode(newBlockID));
    // create directory object for it
    std::unique_ptr<Directory> newDir(new Directory(dirEntryListCreator, std::move(newDirINode), parent));
    // create link from parent to new child (this cannot overflow the link counter in the child since it is brand new)
    std::list<Block*> changedBlocks = parent->addHardlink(newDir->getPrimaryINode(), lastName(absolutePath));
    // save
    for (auto &block : changedBlocks) {
        saveBlock(*block);
    }

    // done!
    return true;
}

bool FS::rmdir(std::string absolutePath) {
    absolutePath = normalizePath(absolutePath);
    if (absolutePath.find_first_of("/") != 0) {
        // not an absolute path
        std::cout << "fs: rmdir: cannot remove dir with path \"" << absolutePath << "\", path is not absolute" << std::endl;
        return false;
    }
    // this is a bit counter-intuitive, but removing a dir requires up to 2 blocks (for re-writing the list in parent, child or both)
    if (usedBlocks + 2 > logSize) {
        std::cout << "fs: rmdir: cannot remove directory, fs is full (2 blocks buffer required)" << std::endl;
        return false;
    }
    // find parent node
    std::unique_ptr<Directory> parent = searchParent(absolutePath);
    // parent exists?
    if (!parent) {
        std::cout << "fs: rmdir: cannot remove dir with path \"" << absolutePath << "\", parent does not exist" << std::endl;
        return false;
    }
    // dir exists?
    uint32_t id = parent->searchHardlink(lastName(absolutePath));
    if (id == 0) {
        std::cout << "fs: rmdir: cannot remove dir with path \"" << absolutePath << "\", dir does not exist" << std::endl;
        return false;
    }
    if (id == 1) {
        std::cout << "fs: rmdir: cannot remove root directory" << std::endl;
        return false;
    }
    // is this even a dir?
    if (peekINodeType(id) != SDI4FS_INODE_TYPE_DIR) {
        std::cout << "fs: rmdir: cannot remove \"" << absolutePath << "\", not a directory" << std::endl;
        return false;
    }
    // load dir
    std::unique_ptr<Directory> dir = loadDirectory(id);
    if (!dir) {
        // should never happen
        std::cout << "fs: fatal error - inconsistency - unable to load dir with primary inode id " << id << std::endl;
        return false;
    }
    // dir must be empty
    if (dir->getChildCount() > 2) { // 2 = empty, "." and ".." are always required
        std::cout << "fs: rmdir: cannot remove dir with path \"" << absolutePath << "\", dir is not empty" << std::endl;
        return false;
    }

    // all requirements ok, delete hardlink from parent
    std::list<Block*> changedBlocks = parent->rmHardlink(dir->getPrimaryINode(), lastName(absolutePath));
    // delete ".." link from child, since it affects parents link counter
    addUnique<Block*>(changedBlocks, dir->rmHardlink(parent->getPrimaryINode(), ".."));
    for (auto &block : changedBlocks) {
        saveBlock(*block);
    }

    // since directory hardlinks are not allowed (other than "." and ".."),
    // the previously removed hardlink was the only one pointing to this
    // dir, so it can be deleted

    // start by deleting all DirEntryLists (disk)
    if (!dir->getPrimaryINode().isInlined()) {
        const std::list<DirectoryEntryList*> entryLists = dir->blocks();
        for (auto &list : entryLists) {
            freeBlock(list->getId());
        }
    }
    // dealloc primary INode (disk)
    freeBlock(dir->getPrimaryINode().getId());

    return true;
}

bool FS::rename(std::string sourcePath, std::string destPath) {
    sourcePath = normalizePath(sourcePath);
    destPath = normalizePath(destPath);
    if (sourcePath.find_first_of("/") != 0 || destPath.find_first_of("/") != 0) {
        // not an absolute path
        std::cout << "fs: rename: cannot rename from path \"" << sourcePath << "\" to \"" << destPath << "\", both paths must be absolute" << std::endl;
        return false;
    }
    // rename requires up to 5 blocks (up to 2 to rm in old, up to 3 for new hardlink)
    if (usedBlocks + 5 > logSize) {
        std::cout << "fs: rename: cannot rename, fs is full (5 blocks buffer required)" << std::endl;
        return false;
    }

    // new link cannot be child of current link
    if (destPath.find(sourcePath) == 0 && destPath.length() > sourcePath.length()) {
        std::cout << "fs: rename: cannot rename, new path cannot be child of old" << std::endl;
        return false;
    }

    // check old hardlink exists
    std::unique_ptr<Directory> oldParent = searchParent(sourcePath);
    if (!oldParent) {
        std::cout << "fs: rename: cannot rename, parent of source path \"" << sourcePath << "\" does not exist" << std::endl;
        return false;
    }
    int targetID = oldParent->searchHardlink(lastName(sourcePath));
    if (targetID == 0) {
        std::cout << "fs: rename: cannot rename, source path \"" << sourcePath << "\" does not exist" << std::endl;
        return false;
    }
    // check parent of new hardlink exists, but hardlink itself not
    std::unique_ptr<Directory> newParent = searchParent(destPath);
    if (!newParent) {
        std::cout << "fs: rename: cannot rename, parent of dest path \"" << destPath << "\" does not exist" << std::endl;
        return false;
    }
    if (newParent->searchHardlink(lastName(destPath))) {
        std::cout << "fs: rename: cannot rename, target \"" << destPath << "\" exists" << std::endl;
        return false;
    }

    // get move target INode
    std::unique_ptr<IPrimaryINodeHolder> moveTarget;
    bool directory = false;
    switch (peekINodeType(targetID)) {
        case SDI4FS_INODE_TYPE_DIR:
            moveTarget = std::move(loadDirectory(targetID));
            directory = true;
            break;
        case SDI4FS_INODE_TYPE_REGULARFILE:
            moveTarget = std::move(loadFile(targetID));
            break;
        default:
            std::cout << "fs: rename: cannot move target with unknown INode type " << peekINodeType(targetID) << std::endl;
            return false;
    }

    // check if same parent, then actually move
    if (oldParent->getPrimaryINode().getId() == newParent->getPrimaryINode().getId()) {
        // same dir
        // beware: there are now 2 directory objects for the same dir!
        // from this point on, only oldParent is used!
        std::list<Block*> changes = oldParent->rmHardlink(moveTarget->getPrimaryINode(), lastName(sourcePath));
        addUnique<Block*>(changes, oldParent->addHardlink(moveTarget->getPrimaryINode(), lastName(destPath)));
        // save all returned dirs
        for (Block *block : changes) {
            saveBlock(*block);
        }
    } else {
        // different parents ("normal" case)
        // make sure new parent can handle one more child
        if (newParent->getChildCount() == SDI4FS_MAX_HARDLINKS_PER_DIR) {
            std::cout << "fs: rename: cannot rename, max # of links in new parent dir reached" << std::endl;
            return false;
        }
        // also make sure the new parent can handle one more link pointing to it
        if (newParent->getPrimaryINode().getLinkCounter() == SDI4FS_MAX_NUMBER_OF_LINKS_TO_INODE) {
            std::cout << "fs: rename: cannot rename, max # of links pointing to new parent dir reached" << std::endl;
            return false;
        }
        // move target
        std::list<Block*> changes = oldParent->rmHardlink(moveTarget->getPrimaryINode(), lastName(sourcePath));
        addUnique<Block*>(changes, newParent->addHardlink(moveTarget->getPrimaryINode(), lastName(destPath)));
        if (directory) {
            // also need to take care of ".." link
            addUnique<Block*>(changes, static_cast<Directory*> (moveTarget.get())->rmHardlink(oldParent->getPrimaryINode(), ".."));
            addUnique<Block*>(changes, static_cast<Directory*> (moveTarget.get())->addHardlink(newParent->getPrimaryINode(), ".."));
        }
        // save old + new parent, link target + all returned blocks
        for (Block *block : changes) {
            saveBlock(*block);
        }
    }

    return true;
}

bool FS::touch(std::string absolutePath) {
    absolutePath = normalizePath(absolutePath);
    if (absolutePath.find_first_of("/") != 0) {
        // not an absolute path
        std::cout << "fs: touch: cannot create file with path \"" << absolutePath << "\", path is not absolute" << std::endl;
        return false;
    }
    // this requires at least 4 free blocks (1 for new file, 1 for updated parent, (rare:) 2 for parent switching from inline to non-inline)
    if (usedBlocks + 4 > logSize) {
        std::cout << "fs: touch: cannot create new file, fs is full" << std::endl;
        return false;
    }
    // find parent node
    std::unique_ptr<Directory> parent = searchParent(absolutePath);
    // parent exists?
    if (!parent) {
        std::cout << "fs: touch: cannot create file with path \"" << absolutePath << "\", parent does not exist" << std::endl;
        return false;
    }

    // child already existing?
    if (parent->searchHardlink(lastName(absolutePath)) != 0) {
        std::cout << "fs: touch: cannot create file with path \"" << absolutePath << "\", file exists" << std::endl;
        return false;
    }

    // make sure parent can handle one more child
    if (parent->getChildCount() == SDI4FS_MAX_HARDLINKS_PER_DIR) {
        std::cout << "fs: touch: cannot create new file, max # of links in parent dir reached, parent " << parent->getPrimaryINode().getId() << std::endl;
        return false;
    }

    // get id for new block
    uint32_t newBlockID = getNextBlockID();
    // alloc new block
    std::unique_ptr<FileINode> newFileINode(new FileINode(newBlockID));
    // create file object for it
    std::unique_ptr<File> newFile(new File(dataBlockListCreator, std::move(newFileINode)));
    // create link from parent to new child (cannot overflow child link counter since child is brand new)
    std::list<Block*> changedBlocks = parent->addHardlink(newFile->getPrimaryINode(), lastName(absolutePath));
    // save
    for (auto &block : changedBlocks) {
        saveBlock(*block);
    }

    // done!
    return true;
}

bool FS::ls(std::string absolutePath, std::list<std::string> &result) {
    absolutePath = normalizePath(absolutePath);
    if (absolutePath.find_first_of("/") != 0) {
        // not an absolute path
        std::cout << "fs: ls: cannot list dir with path \"" << absolutePath << "\", path is not absolute" << std::endl;
        return false;
    }
    // find parent node
    std::unique_ptr<Directory> parent = searchParent(absolutePath);
    // parent exists?
    if (!parent) {
        std::cout << "fs: ls: cannot list dir with path \"" << absolutePath << "\", parent does not exist" << std::endl;
        return false;
    }
    uint32_t id = 1; // default to root
    // root dir is its own parent
    if (absolutePath.compare("/") != 0) {
        // dir exists?
        id = parent->searchHardlink(lastName(absolutePath));
        if (id == 0) {
            std::cout << "fs: ls: cannot list dir with path \"" << absolutePath << "\", dir does not exist" << std::endl;
            return false;
        }
        // is this even a dir?
        if (peekINodeType(id) != SDI4FS_INODE_TYPE_DIR) {
            std::cout << "fs: ls: cannot list dir with path \"" << absolutePath << "\", not a directory" << std::endl;
            return false;
        }
    }
    // load dir
    std::unique_ptr<Directory> dir = loadDirectory(id);
    if (!dir) {
        // should never happen
        std::cout << "fs: fatal error - inconsistency - unable to load dir with primary inode id " << id << std::endl;
        return false;
    }
    // get content, then augment
    std::list<std::string> list;
    dir->ls(list);
    for (std::string linkName : list) {
        // try accessing the child
        uint32_t childID = dir->searchHardlink(linkName);
        std::unique_ptr<IPrimaryINodeHolder> child;
        bool directory = false;
        switch (peekINodeType(childID)) {
            case SDI4FS_INODE_TYPE_DIR:
                child = std::move(loadDirectory(childID));
                directory = true;
                break;
            case SDI4FS_INODE_TYPE_REGULARFILE:
                child = std::move(loadFile(childID));
                break;
            default:
                std::cout << "fs: ls: cannot list child with unknown INode type " << peekINodeType(childID) << std::endl;
                continue;
        }
        INode *childINode = &child->getPrimaryINode();
        // output format: TYPE (d/f) SIZE SIZE_ON_DISK T_CREATED T_MODIFIED
        std::stringstream ss;
        if (directory) {
            ss << "d ";
        } else {
            ss << "f ";
        }
        ss << childINode->getLinkCounter() << " ";
        ss << childINode->getInternalSize_b() << " ";
        ss << childINode->getUserVisibleSize_b() << " ";
        ss << childINode->getCreationTime() << " ";
        ss << childINode->getLastWriteTime() << " ";
        ss << linkName;
        result.push_back(ss.str());
    }
    // header if not empty
    if (!list.empty()) {
        result.push_front("t #links size disksize t_created t_mod name");
    }
    return true;
}

bool FS::rm(std::string absolutePath) {
    absolutePath = normalizePath(absolutePath);
    if (absolutePath.find_first_of("/") != 0) {
        // not an absolute path
        std::cout << "fs: rm: cannot remove file with path \"" << absolutePath << "\", path is not absolute" << std::endl;
        return false;
    }
    // this is a bit counter-intuitive, but removing a file requires up to 2 free block (for re-writing the parent)
    if (usedBlocks + 2 > logSize) {
        std::cout << "fs: rm: cannot remove file, fs is full (2 blocks buffer required)" << std::endl;
        return false;
    }
    // find parent node
    std::unique_ptr<Directory> parent = searchParent(absolutePath);
    // parent exists?
    if (!parent) {
        std::cout << "fs: rm: cannot remove file with path \"" << absolutePath << "\", parent does not exist" << std::endl;
        return false;
    }
    // file exists?
    uint32_t id = parent->searchHardlink(lastName(absolutePath));
    if (id == 0) {
        std::cout << "fs: rm: cannot remove file with path \"" << absolutePath << "\", file does not exist" << std::endl;
        return false;
    }
    // is this even a file?
    if (peekINodeType(id) != SDI4FS_INODE_TYPE_REGULARFILE) {
        std::cout << "fs: rm: cannot remove \"" << absolutePath << "\", not a file" << std::endl;
        return false;
    }
    // load file
    std::unique_ptr<File> file = loadFile(id);
    if (!file) {
        // should never happen
        std::cout << "fs: fatal error - inconsistency - unable to load file with primary inode id " << id << std::endl;
        return false;
    }

    // all requirements ok, delete hardlink from parent
    std::list<Block*> changedBlocks = parent->rmHardlink(file->getPrimaryINode(), lastName(absolutePath));
    // save changes to parent
    for (auto &block : changedBlocks) {
        saveBlock(*block);
    }

    // if link counter is now zero, the file can no longer be reached and must also be deleted
    if (file->getPrimaryINode().getLinkCounter() == 0) {
        // rm all blocks
        std::list<uint32_t> blocks;
        file->blocks(blocks);
        for (uint32_t id : blocks) {
            freeBlock(id);
        }
    }

    return true;
}

bool FS::link(std::string sourcePath, std::string targetPath) {
    sourcePath = normalizePath(sourcePath);
    targetPath = normalizePath(targetPath);
    if (sourcePath.find_first_of("/") != 0 || targetPath.find_first_of("/") != 0) {
        // not an absolute path
        std::cout << "fs: link: cannot link from path \"" << sourcePath << "\" to \"" << targetPath << "\", both paths must be absolute" << std::endl;
        return false;
    }
    // link requires up to 3 new/buffer blocks (all in link parent)
    if (usedBlocks + 3 > logSize) {
        std::cout << "fs: link: cannot create link, fs is full (3 blocks buffer required)" << std::endl;
        return false;
    }
    // find parent node of future link
    std::unique_ptr<Directory> parent = searchParent(sourcePath);
    // parent exists?
    if (!parent) {
        std::cout << "fs: link: cannot create link with path \"" << sourcePath << "\", parent does not exist" << std::endl;
        return false;
    }

    // child already existing?
    if (parent->searchHardlink(lastName(sourcePath)) != 0) {
        std::cout << "fs: link: cannot create link with path \"" << sourcePath << "\", file exists" << std::endl;
        return false;
    }

    // make sure parent can handle one more child
    if (parent->getChildCount() == SDI4FS_MAX_HARDLINKS_PER_DIR) {
        std::cout << "fs: link: cannot create link, max # of links in parent dir reached, parent " << parent->getPrimaryINode().getId() << std::endl;
        return false;
    }
    // now find target
    std::unique_ptr<Directory> targetParent = searchParent(targetPath); // this may now exist more than once, changes are strictly forbidden!
    // parent exists?
    if (!targetParent) {
        std::cout << "fs: link: cannot create link with target \"" << targetPath << "\", parent does not exist" << std::endl;
        return false;
    }
    // target must exist
    uint32_t targetID = parent->searchHardlink(lastName(targetPath));
    if (targetID == 0) {
        std::cout << "fs: link: cannot create link with target \"" << targetPath << "\", file does not exist" << std::endl;
        return false;
    }
    // target must be a file
    if (peekINodeType(targetID) != SDI4FS_INODE_TYPE_REGULARFILE) {
        std::cout << "fs: link: cannot create link with target \"" << targetPath << "\", not a file" << std::endl;
        return false;
    }
    std::unique_ptr<File> file = loadFile(targetID);
    // prevent target link counter overflow
    if (file->getPrimaryINode().getLinkCounter() == SDI4FS_MAX_NUMBER_OF_LINKS_TO_INODE) {
        std::cout << "fs: link: cannot create link with target \"" << targetPath << "\", max # of links pointing to this target reached " << std::endl;
        return false;
    }
    // now finally add the link
    std::list<Block*> changedBlocks = parent->addHardlink(file->getPrimaryINode(), lastName(sourcePath));
    for (Block *block : changedBlocks) {
        saveBlock(*block);
    }
    return true;
}

uint32_t FS::gc() {
    // full?
    if (usedBlocks == logSize) {
        std::cout << "fs: warning - cannot alloc new block, fs full" << std::endl;
        return 0;
    }
    uint32_t result = 0;
    // search a reusable block (limit of for loop prevents endless loops in inconsistent fs)
    for (uint32_t i = 0; i < logSize; ++i) {
        // read id of block at write_ptr
        dev.seekg(logStart_bptr + ((write_ptr - 1) * SDI4FS_BLOCK_SIZE));
        uint32_t id;
        read32(dev, &id);
        // sanity
        if (id > logSize) {
            std::cout << "fs: fatal error - inconsistency, invalid id " << id << " at write_ptr " << write_ptr << std::endl; 
        }
        // free or reclaimable?
        if (id == 0) {
            // free
            result = write_ptr;
            break;
        } else if (bmap[id - 1] != write_ptr) {
            // reclaimable
            // delete block (null id)
            dev.seekp(logStart_bptr + ((write_ptr - 1) * SDI4FS_BLOCK_SIZE));
            write32(dev, 0);
        } else {
            // continue search at next block
            ++write_ptr;
            if (write_ptr > logSize) {
                write_ptr = 1;
            }
        }
    }
    // sanity check
    if (result == 0) {
        // this should never happen, the full-check at the beginning should catch these cases
        std::cout << "fs: fatal error - inconsistency - unable to find a useable block in gc" << std::endl;
        return 0;
    }
    return result;
}

uint32_t FS::getNextBlockID() {
    // full?
    if (usedBlocks == logSize) {
        std::cout << "fs: warning - cannot alloc id for new block, fs full" << std::endl;
        return 0;
    }
    // search a free id, start at nextBlockID
    for (uint32_t i = 0; i < logSize; ++i) { // (limit of for loop prevents endless loops in inconsistend fs)
        // calc bmap position from search start (nextBlockID) + i, wrap result
        uint32_t bmapIndex = nextBlockID + i;
        if (bmapIndex > logSize) {
            bmapIndex -= logSize;
        }
        // is this id available?
        if (bmap[bmapIndex - 1] == 0) {
            // yes, return
            nextBlockID = bmapIndex + 1;
            if (nextBlockID > logSize) {
                nextBlockID -= logSize;
            }
            return bmapIndex;
        }
        // else continue search
    }

    // nothing found
    return 0;
}

void FS::saveBlock(Block &block) {
    // get log address for this block
    uint32_t log_ptr = gc();
    if (log_ptr == 0) {
        return; // gc() already prints a message
    }
    // jump to adr
    dev.seekp(logStart_bptr + ((log_ptr - 1) * SDI4FS_BLOCK_SIZE));
    // write bĺock
    block.save(dev);
    // new block (= never written before)
    if (bmap[block.getId() - 1] == 0) {
        usedBlocks++;
    }
    // update bmap
    bmap[block.getId() - 1] = log_ptr;
    // advance write_ptr
    ++write_ptr;
    if (write_ptr > logSize) {
        write_ptr = 1;
    }
}

void FS::freeBlock(uint32_t id) {
    // sanity check
    if (id == 0) {
        std::cout << "fs: cannot free block with id zero" << std::endl;
        return;
    }
    // never remove root
    if (id == 1) {
        std::cout << "fs: cannot free block with id 1 (root node!)" << std::endl;
        return;
    }
    // remove registration in bmap
    bmap[id - 1] = 0;
    --usedBlocks;
}

std::unique_ptr<Directory> FS::searchParent(std::string absolutePath) {
    // standard sanity checks
    if (absolutePath.find_first_of("/") != 0) {
        // not an absolute path
        std::cout << "fs: searchParent: cannot traverse path \"" << absolutePath << "\", path is not absolute" << std::endl;
        return std::unique_ptr<Directory>(nullptr);
    }

    std::list<std::string> dirs;
    split(dirs, absolutePath, '/');

    // remove leading empty string and trailing filename for path traversal
    dirs.pop_front();
    dirs.pop_back();

    // start path traversal with root dir (id 1)
    std::unique_ptr<Directory> currentDir = loadDirectory(1);
    for (auto iter = dirs.begin(); iter != dirs.end(); ++iter) {
        uint32_t nextDirID = currentDir->searchHardlink(*iter);
        if (nextDirID == 0) {
            // no such file or dir
            currentDir.reset(nullptr);
            break;
        }
        // make sure this is a directory
        if (peekINodeType(nextDirID) != SDI4FS_INODE_TYPE_DIR) {
            std::cout << "fs: path traversal impossible, item " << *iter << " is not a directory" << std::endl;
            currentDir.reset(nullptr);
            break;
        }
        currentDir = loadDirectory(nextDirID);
        if (!currentDir) {
            // cannot load dir for given inode, should never happen
            currentDir.reset(nullptr);
            break;
        }
        // loading worked, continue with next traversal step
    }

    if (!currentDir) {
        // no such file or dir
        return std::unique_ptr<Directory>(nullptr);
    }

    return currentDir;
}

uint8_t FS::peekINodeType(uint32_t id) {
    // sanity check
    if (id == 0) {
        std::cout << "fs: error - tried to peek at INode with id zero" << std::endl;
        return 0;
    }
    // get pos in log
    uint32_t logPtr = lookupBlockAddress(id);
    if (logPtr == 0 || logPtr > logSize) {
        std::cout << "fs: error - peeking INode type failed - not found: " << id << std::endl;
        return 0;
    }
    // seek to block start + offset of type field
    dev.seekg(logStart_bptr + ((logPtr - 1) * SDI4FS_BLOCK_SIZE) + 16);

    // read type (4 bits) + inlined (1bit)
    uint8_t typeAndInline;
    read8(dev, &typeAndInline);
    return (typeAndInline >> 4) & 0xF;
}

uint32_t FS::fileSize(std::string absolutePath) {
    absolutePath = normalizePath(absolutePath);
    if (absolutePath.find_first_of("/") != 0) {
        // not an absolute path
        std::cout << "fs: fileSize: cannot stat file with path \"" << absolutePath << "\", path is not absolute" << std::endl;
        return 0;
    }
    // find parent node
    std::unique_ptr<Directory> parent = searchParent(absolutePath);
    // parent exists?
    if (!parent) {
        std::cout << "fs: fileSize: cannot stat file with path \"" << absolutePath << "\", parent does not exist" << std::endl;
        return 0;
    }
    // file exists?
    uint32_t id = parent->searchHardlink(lastName(absolutePath));
    if (id == 0) {
        std::cout << "fs: fileSize: cannot stat file with path \"" << absolutePath << "\", file does not exist" << std::endl;
        return 0;
    }
    // is this even a file?
    if (peekINodeType(id) != SDI4FS_INODE_TYPE_REGULARFILE) {
        std::cout << "fs: fileSize: cannot stat \"" << absolutePath << "\", not a file" << std::endl;
        return 0;
    }
    // load file
    std::unique_ptr<File> file = loadFile(id);
    if (!file) {
        // should never happen
        std::cout << "fs: fatal error - inconsistency - unable to load file with primary inode id " << id << std::endl;
        return 0;
    }
    return file->getPrimaryINode().getInternalSize_b();
}

uint32_t FS::openFile(std::string absolutePath) {
    absolutePath = normalizePath(absolutePath);
    if (absolutePath.find_first_of("/") != 0) {
        // not an absolute path
        std::cout << "fs: openFile: cannot open file with path \"" << absolutePath << "\", path is not absolute" << std::endl;
        return 0;
    }
    // find parent node
    std::unique_ptr<Directory> parent = searchParent(absolutePath);
    // parent exists?
    if (!parent) {
        std::cout << "fs: openFile: cannot open file with path \"" << absolutePath << "\", parent does not exist" << std::endl;
        return 0;
    }
    // file exists?
    uint32_t id = parent->searchHardlink(lastName(absolutePath));
    if (id == 0) {
        std::cout << "fs: openFile: cannot open file with path \"" << absolutePath << "\", file does not exist" << std::endl;
        return 0;
    }
    // is this even a file?
    if (peekINodeType(id) != SDI4FS_INODE_TYPE_REGULARFILE) {
        std::cout << "fs: openFile: cannot open \"" << absolutePath << "\", not a file" << std::endl;
        return 0;
    }
    // file already open?
    if (openFiles.find(id) != openFiles.end()) {
        std::cout << "fs: openFile: cannot open \"" << absolutePath << "\", already opened" << std::endl;
        return 0;
    }
    // load file
    std::unique_ptr<File> file = loadFile(id);
    if (!file) {
        // should never happen
        std::cout << "fs: fatal error - inconsistency - unable to load file with primary inode id " << id << std::endl;
        return 0;
    }
    openFiles[id] = std::move(file);
    return id;
}

void FS::closeFile(uint32_t handle) {
    flushFile(handle);
    openFiles.erase(handle);
}

void FS::flushFile(uint32_t handle) {
    if (openFiles.find(handle) != openFiles.end()) {
        // save metadata (INode)
        saveBlock(openFiles[handle]->getPrimaryINode());
        if (openFiles[handle]->cachedDataBlockIsDirty()) {
            saveBlock(*(openFiles[handle]->releaseCachedDataBlock().get()));
        }
        // force flush in caching layer/block device server
        dev.flush();
    }
}

bool FS::read(uint32_t fileHandle, char* target, uint32_t pos, std::size_t n) {
    // sanity checks
    if (n < 1) {
        std::cout << "fs: read failed, must read at least 1 byte" << std::endl;
        return false;
    }
    if (openFiles.find(fileHandle) == openFiles.end()) {
        std::cout << "fs: read failed, unknown handle " << fileHandle << std::endl;
        return false;
    }
    File *file = openFiles[fileHandle].get();
    uint32_t fileSize = file->getPrimaryINode().getInternalSize_b();
    if (pos >= fileSize || (pos + n > fileSize)) {
        std::cout << "fs: read failed, invalid byte range specified (from " << pos << ", n " << n << ", fileSize " << fileSize << ")" << std::endl;
        return false;
    }
    // special case: inline file
    if (file->getPrimaryINode().isInlined()) {
        return file->getPrimaryINode().readInline(target, pos, n);
    }

    const uint32_t endPos = pos + n;
    uint32_t currentPos_b = pos;
    uint32_t bytesCopied = 0;
    while (currentPos_b < endPos) {
        uint32_t bytesLeft = endPos - currentPos_b; // absolute
        // find block, then copy
        uint32_t dataBlockNo = currentPos_b / SDI4FS_MAX_BYTES_PER_DATABLOCK;
        uint32_t dataBlockId = file->getDataBlockID(dataBlockNo);
        // calc copy pos in this block
        uint32_t blockStart = currentPos_b - (dataBlockNo * SDI4FS_MAX_BYTES_PER_DATABLOCK);
        uint32_t blockBytes = SDI4FS_MAX_BYTES_PER_DATABLOCK - blockStart;
        if (blockBytes > bytesLeft) {
            blockBytes = bytesLeft;
        }
        if (file->getCachedDataBlockID() != dataBlockId) {
            // datablock must be loaded from disk
            // before, save changes to current block, if any
            if (file->cachedDataBlockIsDirty()) {
                std::unique_ptr<DataBlock> dataBlock = file->releaseCachedDataBlock();
                saveBlock(*dataBlock.get());
            }
            file->setCachedDataBlock(loadDataBlock(dataBlockId));
        }
        if (!file->readFromCachedDataBlock(&target[bytesCopied], blockStart, blockBytes)) {
            std::cout << "fs: read error in block " << dataBlockId << " file " << file->getPrimaryINode().getId() << std::endl;
            return false;
        } // else continue while loop
        currentPos_b += blockBytes;
        bytesCopied += blockBytes;
    }
    return true;
}

bool FS::write(uint32_t fileHandle, const char* source, uint32_t pos, std::size_t n) {
    // sanity
    if (n < 1) {
        std::cout << "fs: write failed, must write at least 1 byte" << std::endl;
        return false;
    }
    if (openFiles.find(fileHandle) == openFiles.end()) {
        std::cout << "fs: write failed, unknown handle " << fileHandle << std::endl;
        return false;
    }
    File *file = openFiles[fileHandle].get();
    FileINode &primaryINode = file->getPrimaryINode();
    uint32_t fSize = primaryINode.getInternalSize_b();
    if (pos > fSize) {
        std::cout << "fs: write failed, write start position must be smaller than or equal to the file size" << std::endl;
        return false;
    }
    if (pos + n >= SDI4FS_MAX_FILE_SIZE) {
        std::cout << "fs: write failed, max file size (" << SDI4FS_MAX_FILE_SIZE << ") exceeded, file " << primaryINode.getId() << std::endl;
        return false;
    }
    // check if this is doable inline-only
    if (primaryINode.isInlined() && (pos + n) <= SDI4FS_MAX_BYTES_PER_INODE) {
        if (primaryINode.writeInline(source, pos, n)) {
            primaryINode.setInternalSize_b(pos + n);
            return true;
        } else {
            return false;
        }
    }
    // from here on everything is done with non-inlined INodes, check if the INode is in that state yet and convert if required
    if (primaryINode.isInlined()) {
        switchNonInline(file);
    }
    // copy blockwise, add blocks as required
    const uint32_t endPos = pos + n;
    uint32_t currentPos_b = pos;
    uint32_t bytesCopied = 0;
    std::unordered_map<uint32_t, Block*> changedMetaBlocks;
    while (currentPos_b < endPos) {
        uint32_t bytesLeft = endPos - currentPos_b; // absolute
        uint32_t dataBlockNo = currentPos_b / SDI4FS_MAX_BYTES_PER_DATABLOCK;
        // make sure the correct DataBlock is cached
        if (file->getNumberOfDataBlocks() == dataBlockNo) {
            // new block, this method creats one, saves the old (if required) and sets the new one as cached
            addDataBlock(file, changedMetaBlocks);
        } else {
            // block was allocated previously, loading required?
            if (file->getDataBlockID(dataBlockNo) != file->getCachedDataBlockID()) {
                // save old cached block, if any
                if (file->cachedDataBlockIsDirty()) {
                    saveBlock(*(file->releaseCachedDataBlock().get()));
                }
                file->setCachedDataBlock(loadDataBlock(file->getDataBlockID(dataBlockNo)));
            }
        }
        // calc copy pos in this block
        uint32_t blockStart = currentPos_b - (dataBlockNo * SDI4FS_MAX_BYTES_PER_DATABLOCK);
        uint32_t blockBytes = SDI4FS_MAX_BYTES_PER_DATABLOCK - blockStart;
        if (blockBytes > bytesLeft) {
            blockBytes = bytesLeft;
        }
        if (!file->writeToCachedDataBlock(&source[bytesCopied], blockStart, blockBytes)) {
            std::cout << "fs: write error in block " << file->getCachedDataBlockID() << " file " << primaryINode.getId() << std::endl;
            return false;
        }
        currentPos_b += blockBytes;
        bytesCopied += blockBytes;
    }

    primaryINode.setInternalSize_b(pos + n);

    for (std::pair<const uint32_t, SDI4FS::Block*> &block: changedMetaBlocks) {
        saveBlock(*(block.second));
    }
    return true;
}

bool FS::truncate(uint32_t fileHandle, uint32_t size) {
    // sanity
    if (openFiles.find(fileHandle) == openFiles.end()) {
        std::cout << "fs: truncate failed, unknown handle " << fileHandle << std::endl;
        return false;
    }
    File *file = openFiles[fileHandle].get();
    uint32_t fSize = file->getPrimaryINode().getInternalSize_b();
    if (size >= fSize) {
        std::cout << "fs: truncate failed, new size (" << size << ") must be smaller than old size (" << fSize << ")" << std::endl;
        return false;
    }
    uint32_t newNumberOfBlocks = (size / SDI4FS_MAX_BYTES_PER_DATABLOCK) + 1;
    if (size % SDI4FS_MAX_BYTES_PER_DATABLOCK == 0) {
        --newNumberOfBlocks;
    }
    uint32_t oldNumberOfBlocks = (fSize / SDI4FS_MAX_BYTES_PER_DATABLOCK) + 1;
    if (fSize % SDI4FS_MAX_BYTES_PER_DATABLOCK == 0) {
        --oldNumberOfBlocks;
    }
    // clear file cache before truncate
    if (file->cachedDataBlockIsDirty()) {
        saveBlock(*(file->releaseCachedDataBlock().get()));
    }
    removeDataBlocks(file, oldNumberOfBlocks - newNumberOfBlocks);
    file->getPrimaryINode().setInternalSize_b(size);
    saveBlock(file->getPrimaryINode());
    return true;
}

void FS::switchNonInline(File *file) {
    // switching requires 1 new Inode, 1 new DataBlockList, 1 new DataBlock
    if (usedBlocks + 3 > logSize) {
        std::cout << "fs: write: cannot write, fs is too full for non-inline switch of file " << file->getPrimaryINode().getId() << std::endl;
        return;
    }
    // this block will hold the currently inlined data
    std::unique_ptr<DataBlock> newDataBlock(new DataBlock(getNextBlockID()));
    std::list<Block*> changedBlocks = file->convertToNonInline(std::move(newDataBlock));
    // save blocks
    for (Block *block : changedBlocks) {
        saveBlock(*block);
    }
}

bool FS::addDataBlock(File *file, std::unordered_map<uint32_t, Block*> &changedMetaBlocks) {
    // before adding a DataBlock, check the file can tolerate one more + enough blocks are free (for inode, new block, new list)
    if (usedBlocks + 3 > logSize) {
        std::cout << "fs: write: cannot write, fs is too full to add one additional data block to file " << file->getPrimaryINode().getId() << std::endl;
        return false;
    }
    if (file->getNumberOfDataBlocks() == SDI4FS_MAX_DATABLOCKS_PER_FILE) {
        // file full!
        std::cout << "fs: write: cannot write, max size of file " << file->getPrimaryINode().getId() << " reached" << std::endl;
        return false;
    }
    // handle previously cached block first
    if (file->cachedDataBlockIsDirty()) {
        saveBlock(*(file->releaseCachedDataBlock().get()));
    }
    // alloc, then save
    std::unique_ptr<DataBlock> newDataBlock(new DataBlock(getNextBlockID()));

    std::list<Block*> changedBlocks = file->addDataBlock(std::move(newDataBlock)); // this also sets DataBlock as cached
    for (Block *block : changedBlocks) {
        if (changedMetaBlocks.find(block->getId()) == changedMetaBlocks.end()) {
            changedMetaBlocks[block->getId()] = block;
        }
    }
    return true;
}

void FS::removeDataBlocks(File *file, std::size_t n) {
    // this requires at least 1 free block (updated INode or DataBlockList)
    if (usedBlocks + 1 > logSize) {
        std::cout << "fs: cannot remove DataBlock, this requires at least 1 free block as buffer, file " << file->getPrimaryINode().getId() << std::endl;
        return;
    }
    // do not remove last datablock of file
    if (file->getNumberOfDataBlocks() <= n) {
        std::cout << "fs: error - invalid number of datablocks to remove, requested " << n << ", present in file " << file->getNumberOfDataBlocks()
                << " file " << file->getPrimaryINode().getId() << std::endl;
        return;
    }
    std::list<Block*> changedBlocks;
    for (std::size_t i = 0; i < n; ++i) {
        addUnique<Block*>(changedBlocks, file->removeDataBlock());
    }
    for (Block *block : changedBlocks) {
        saveBlock(*block);
    }
}

} // SDI4FS

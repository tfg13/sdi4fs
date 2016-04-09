/* 
 * File:   Formatter.inc
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on November 3, 2015, 11:44 AM
 */

#ifndef FORMATTER_INC
#define	FORMATTER_INC

#include <cmath>
#include <iostream>
#include <memory>

#include "Directory.h"
#include "DirectoryINode.h"

#include "Constants.inc"
#include "StreamSelectorHeader.inc"
#include "StreamUtils.inc"


inline void createSDI4FS(STREAM &dev, uint64_t fsSize_b) {
    
    // fsSize_b zero means use everything
    if (fsSize_b == 0) {
        dev.seekg(0, dev.end);
        fsSize_b = dev.tellg();
    }
    
    dev.seekg(0, dev.beg);
    
    // calc some values
    uint64_t bmapSize_b = ceil(((fsSize_b - SDI4FS_HEADER_SIZE) / 1024) / 4096.0) * 4096;
    uint64_t logStart_bptr = SDI4FS_HEADER_SIZE + bmapSize_b;

    // null file, alloc block buffer for faster formatting
    void *zeros = calloc(1, SDI4FS_BLOCK_SIZE);
    if (zeros == NULL) {
        // this really should not happen
        std::cout << "error, cannot alloc " << SDI4FS_BLOCK_SIZE << " bytes for nulling the device. Aborting." << std::endl;
        return;
    }
    dev.seekp(0, dev.beg);
    for (uint64_t i = 0; i < fsSize_b; ++i) {
        if (i + SDI4FS_BLOCK_SIZE <= fsSize_b) {
            // null whole block
            i += SDI4FS_BLOCK_SIZE - 1;
            dev.write((char*) zeros, SDI4FS_BLOCK_SIZE);
            if (dev.fail()) {
                std::cout << "error writing to stream at " << dev.tellg() << std::endl;
            }
        } else {
            // byte after byte
            write8(dev, 0);
        }
    }
    free(zeros);

    // HEADER
    // magic
    dev.seekp(0, dev.beg);
    write32(dev, SDI4FS_MAGIC);
    dev.seekp(8);
    // fs size
    write64(dev, fsSize_b);
    // write_ptr
    write32(dev, 2);
    // bmap_valid
    write32(dev, 1);
    // nextBlockID
    write32(dev, 2);
    // number of used blocks
    write32(dev, 1);
    // umount-time (zero for now)
    write32(dev, 0);

    // BMAP
    dev.seekp(SDI4FS_HEADER_SIZE);
    // put in entry for root dir (blockID 1)
    write32(dev, 1); // position 1 in log

    // LOG
    // root INode (dir "/")
    std::unique_ptr<SDI4FS::DirectoryINode> rootINode(new SDI4FS::DirectoryINode(1));
    SDI4FS::Directory rootDir(NULL, std::move(rootINode)); // give NULL as block allocator, this tool does not call any methods that use it
    dev.seekp(logStart_bptr);
    rootDir.getPrimaryINode().save(dev);

    // override block 1 write time (set to zero)
    dev.seekp(logStart_bptr + 4);
    write32(dev, 0);

    dev.flush();
}



#endif	/* FORMATTER_INC */


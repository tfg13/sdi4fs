/*
 * File:   FileINode.cpp
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on September 16, 2015, 11:11 PM
 */

#include "FileINode.h"
#include "INode.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include "Constants.inc"
#include "DataBlockList.h"
#include "StreamUtils.inc"
#include "DataBlock.h"

namespace SDI4FS {

FileINode::FileINode(STREAM &input) : INode(input), entries() {
    // verify INode type
    if (getType() != SDI4FS_INODE_TYPE_REGULARFILE) {
        std::cout << "fs: fatal error - inconsistency - reading FileINode from INode of different type: " << getType() << std::endl;
        return;
    }
    if (isInlined()) {
        // read stored content
        readN(input, &data[0], getInternalSize_b());
    } else {
        // read stored ids of DataBlockLists
        for (int i = 0; i < SDI4FS_MAX_DATABLOCKLISTS_PER_FILE; ++i) {
            uint32_t linkTarget;
            read32(input, &linkTarget);
            if (linkTarget != 0) {
                entries.push_back(linkTarget);
            } else {
                // no gaps allowed
                break;
            }
        }
    }
}

FileINode::FileINode(uint32_t id) : INode(id, SDI4FS_INODE_TYPE_REGULARFILE), entries() {
}

void FileINode::convertToNonInline(DataBlockList *blockList, DataBlock &dataBlock) {
    pushDataBlockList(blockList->getId());
    dataBlock.write((char*) &data[0], 0, getInternalSize_b());
    setInlined(false);
}

bool FileINode::pushDataBlockList(uint32_t id) {
    if (entries.size() == SDI4FS_MAX_DATABLOCKLISTS_PER_FILE) {
        // full
        return false;
    }
    entries.push_back(id);
    return true;
}

uint32_t FileINode::popDataBlockList() {
    // cannot empty below 1
    if (entries.size() == 1) {
        return 0;
    }
    uint32_t result = entries[entries.size() - 1];
    entries.pop_back();
    return result;
}

uint32_t FileINode::getDataBlockList(size_t index) {
    if (entries.size() <= index) {
        return 0;
    }
    return entries[index];
}

bool FileINode::readInline(char *target, uint32_t pos, std::size_t n) {
    if (!isInlined()) {
        std::cout << "fs: error - attempting to read inline data from non-inline FileINode: " << getId() << std::endl;
        return false;
    }
    if (pos > SDI4FS_MAX_BYTES_PER_INODE || (pos + n) > SDI4FS_MAX_BYTES_PER_INODE) {
        std::cout << "fs: error - attempting to read inline data with out-of-bound positions:" << getId() << " " << pos << " " << n << std::endl;
        return false;
    }
    memcpy(target, &data[pos], n);
    return true;
}

bool FileINode::writeInline(const char *source, uint32_t pos, std::size_t n) {
    if (!isInlined()) {
        std::cout << "fs: error - attempting to write inline data to non-inline FileINode: " << getId() << std::endl;
        return false;
    }
    if (pos > SDI4FS_MAX_BYTES_PER_INODE || (pos + n) > SDI4FS_MAX_BYTES_PER_INODE) {
        std::cout << "fs: error - attempting to write inline data with out-of-bound positions:" << getId() << " " << pos << " " << n << std::endl;
        return false;
    }
    memcpy(&data[pos], source, n);
    return true;
}

uint32_t FileINode::getUserVisibleSize_b() {
    if (isInlined()) {
        return SDI4FS_BLOCK_SIZE;
    } else {
        size_t numberOfDataBlockLists = entries.size();
        size_t numberOfDataBlocks = ceil(getInternalSize_b() / ((float) SDI4FS_MAX_BYTES_PER_DATABLOCK));
        // 1 FileINode + #DataBlockLists + #DataBlocks
        return ((1 + numberOfDataBlockLists + numberOfDataBlocks) * SDI4FS_BLOCK_SIZE);
    }
}

void FileINode::setInternalSize_b(uint32_t size_b) {
    // verify range
    if (size_b <= SDI4FS_MAX_FILE_SIZE) {
        INode::setInternalSize_b(size_b);
    } else {
        std::cout << "fs: cannot set internal size of file " << getId() << ", file size limit exceeded (" << size_b << ")" << std::endl;
    }
}

void FileINode::save(STREAM &output) {
    INode::save(output);
    if (isInlined()) {
        // write content
        writeN(output, &data[0], getInternalSize_b());
    } else {
        // write entries
        for (auto iter = entries.begin(); iter != entries.end(); ++iter) {
            // write id
            write32(output, *iter);
        }
        // null rest
        for (int i = entries.size(); i < SDI4FS_MAX_DATABLOCKLISTS_PER_FILE; ++i) {
            // write 0 (zero)
            write32(output, 0);
        }
    }
}

FileINode::~FileINode() {
}

} // SDI4FS


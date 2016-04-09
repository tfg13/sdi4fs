/*
 * File:   File.cpp
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on September 16, 2015, 11:26 PM
 */

#include "File.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <list>
#include <memory>
#include <vector>

#include "DataBlockList.h"
#include "Directory.h"
#include "FileINode.h"
#include "IDataBlockListCreator.h"
#include "DataBlock.h"


namespace SDI4FS {

File::File(IDataBlockListCreator *blockListCreator, std::unique_ptr<FileINode> primary, std::list<uint32_t> *blockListIDs) : blockListCreator(blockListCreator), inode(std::move(primary)), blockLists(), numberOfDataBlocks(0) {
    if (!inode->isInlined()) {
        numberOfDataBlocks = ceil(inode->getInternalSize_b() / ((float) SDI4FS_MAX_BYTES_PER_DATABLOCK));
        // copy list of DataBlockLists
        size_t numberOfDataBlockLists = ceil(numberOfDataBlocks / ((float) SDI4FS_MAX_DATABLOCKS_PER_DATABLOCKLIST));
        for (size_t i = 0; i < numberOfDataBlockLists; ++i) {
            blockListIDs->push_back(inode->getDataBlockList(i));
        }
        // copying the ids requests them to be loaded. caller will (=must) call init before using this object to complete construction
    }
}

File::File(IDataBlockListCreator *blockListCreator, std::unique_ptr<FileINode> empty) : blockListCreator(blockListCreator), inode(std::move(empty)), blockLists(), numberOfDataBlocks(0) {
    // nothing else to do, file has size 0 and is empty
}

void File::init(std::vector<std::unique_ptr<DataBlockList>> &blockLists) {
    // sanity check
    if (inode->isInlined()) {
        std::cout << "fs: error - wrong initialization of file object" << std::endl;
    }
    // copy to internal list
    for (auto &list : blockLists) {
        this->blockLists.push_back(list.release());
    }
}

FileINode& File::getPrimaryINode() {
    return *inode.get();
}

uint32_t File::getNumberOfDataBlocks() {
    return numberOfDataBlocks;
}

std::list<Block*> File::convertToNonInline(std::unique_ptr<DataBlock> dataBlock) {
    std::list<Block*> changedBlocks;
    // sanity check
    if (!inode->isInlined()) {
        std::cout << "fs: error - cannot convert non-inlined inode to non-inlined mode again" << std::endl;
        return changedBlocks;
    }
    // create first DataBlockList (caller guarantees this will never return null)
    DataBlockList *newList = blockListCreator->alloc();
    // set first DataBlock, then save list
    newList->pushDataBlock(dataBlock->getId());
    inode->convertToNonInline(newList, *dataBlock.get());
    blockLists.push_back(newList);
    ++numberOfDataBlocks;
    // caller must save inode, new list, datablock
    changedBlocks.push_back(newList);
    changedBlocks.push_back(&getPrimaryINode());
    changedBlocks.push_back(dataBlock.get());
    // set cached
    setCachedDataBlock(std::move(dataBlock));
    return changedBlocks;
}

std::list<Block*> File::addDataBlock(std::unique_ptr<DataBlock> dataBlock) {
    std::list<Block*> changedBlocks;
    // sanity check
    if (inode->isInlined()) {
        std::cout << "fs: error - cannot add DataBlock to inline-mode file " << inode->getId() << std::endl;
        return changedBlocks;
    }
    // new DataBlockList required?
    if (numberOfDataBlocks % SDI4FS_MAX_DATABLOCKS_PER_DATABLOCKLIST == 0) {
        // alloc new list, put in inode (caller guarantees this will never return null)
        DataBlockList *newList = blockListCreator->alloc();
        if (!inode->pushDataBlockList(newList->getId())) {
            std::cout << "fs: error - cannot save new DataBlockList, file " << inode->getId() << " is full" << std::endl;
            return changedBlocks;
        }
        blockLists.push_back(newList);
        changedBlocks.push_back(&getPrimaryINode());
    }
    // free DataBlockList slot is guaranteed now
    DataBlockList *last = blockLists[blockLists.size() - 1];
    last->pushDataBlock(dataBlock->getId());
    changedBlocks.push_back(last);
    ++numberOfDataBlocks;
    setCachedDataBlock(std::move(dataBlock));
    return changedBlocks;
}

std::list<Block*> File::removeDataBlock() {
    std::list<Block*> changedBlocks;
    // sanity check
    if (inode->isInlined()) {
        std::cout << "fs: error - cannot remove a DataBlock from inline-mode file " << inode->getId() << std::endl;
        return changedBlocks;
    }
    // remove last DataBlock first
    DataBlockList *last = blockLists[blockLists.size() - 1];
    last->popDataBlock();
    --numberOfDataBlocks;
    // last DataBlockList now empty?
    if (numberOfDataBlocks % SDI4FS_MAX_DATABLOCKS_PER_DATABLOCKLIST == 0 && numberOfDataBlocks > SDI4FS_MAX_DATABLOCKS_PER_DATABLOCKLIST) { // not if only 1 list left
        // yes, remove now-empty DataBlockList
        blockListCreator->dealloc(last);
        blockLists.pop_back();
        inode->popDataBlockList();
        // inode was modified
        changedBlocks.push_back(&getPrimaryINode());
    } else {
        // modified DataBlockList still contains elements, save
        changedBlocks.push_back(last);
    }
    return changedBlocks;
}

uint32_t File::getDataBlockID(uint32_t blockNo) {
    if (inode->isInlined()) {
        std::cout << "fs: error - cannot retrieve a DataBlock from inline-mode file " << inode->getId() << std::endl;
        return 0;
    }
    if (blockNo >= numberOfDataBlocks) {
        return 0;
    }
    // get list index, then ask the list
    uint32_t listNo = blockNo / SDI4FS_MAX_DATABLOCKS_PER_DATABLOCKLIST;
    DataBlockList *blockList = blockLists[listNo];
    return blockList->getDataBlock(blockNo % SDI4FS_MAX_DATABLOCKS_PER_DATABLOCKLIST);
}

void File::blocks(std::list<uint32_t> &result) {
    result.push_back(inode->getId());
    if (!inode->isInlined()) {
        for (DataBlockList *list : blockLists) {
            result.push_back(list->getId());
            list->blocks(result);
        }
    }
}

void File::setCachedDataBlock(std::unique_ptr<DataBlock> dataBlock) {
    cachedDataBlock = std::move(dataBlock);
}

uint32_t File::getCachedDataBlockID() {
    if (cachedDataBlock) {
        return cachedDataBlock->getId();
    } else {
        return 0;
    }
}

bool File::cachedDataBlockIsDirty() {
    if (cachedDataBlock) {
        return cachedDataBlock->isDirty();
    }
    return false;
}

std::unique_ptr<DataBlock> File::releaseCachedDataBlock() {
    return std::move(cachedDataBlock);
}

bool File::readFromCachedDataBlock(char *target, uint32_t pos, std::size_t n) {
    if (!cachedDataBlock) {
        return false;
    }
    return cachedDataBlock->read(target, pos, n);
}

bool File::writeToCachedDataBlock(const char *source, uint32_t pos, std::size_t n) {
    if (!cachedDataBlock) {
        return false;
    }
    return cachedDataBlock->write(source, pos, n);
}

File::~File() {
    for (auto iter = blockLists.begin(); iter != blockLists.end(); ++iter) {
        delete *iter;
    }
}

} // SDI4FS


/*
 * File:   File.h
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on September 16, 2015, 11:26 PM
 */

#ifndef SDI4FS_FILE_H
#define	SDI4FS_FILE_H

#include "IPrimaryINodeHolder.h"

#include <cstdint>
#include <list>
#include <memory>
#include <vector>

#include "DataBlockList.h"
#include "Directory.h"
#include "FileINode.h"
#include "IDataBlockListCreator.h"
#include "DataBlock.h"

namespace SDI4FS {

/**
 * Represents a regular file.
 */
class File : public IPrimaryINodeHolder {
public:
    File();
    /**
     * Creates a new File Object for EXISTING (on disk) files with the given primary INode.
     * This is a two-stage process. Calling this constructor results
     * in a list of blockIDs of DataBlockList blocks that need to be loaded
     * by the caller and fed into this class by calling init()
     * @param blockListCreator used to create new DataBlockList blocks, if required
     * @param blockCreator used to create new DataBlock blocks, if required
     * @param primary the primary INode
     * @param blockListIDs pointer to empty list, will be filled with required block list ids.
     */
    File(IDataBlockListCreator *blockListCreator, std::unique_ptr<FileINode> primary, std::list<uint32_t> *blockListIDs);

    /**
     * Creates a new File.
     * It is required to save() the FileINode and its parent after calling this constructor.
     * @param blockListCreator used to create new DataBlockList blocks, if required
     * @param blockCreator used to create new DataBlock blocks, if required
     * @param empty a fresh FileINode
     */
    File(IDataBlockListCreator *blockListCreator, std::unique_ptr<FileINode> empty);

    /**
     * Finishes the initialization of a non-inlined File read from disk.
     * This must be called iff the Constructor that reads from disk returns at least one blockID of FileBlockLists.
     * @param blockList list of requested (by the constructor) blocks
     */
    void init(std::vector<std::unique_ptr<DataBlockList>> &blockList);

    /**
     * Returns the primary FileINode of this directory.
     * @return the primary INode
     */
    virtual FileINode& getPrimaryINode();

    /**
     * Returns the current number of used DataBlocks.
     * @return the current number of used DataBlocks
     */
    uint32_t getNumberOfDataBlocks();

    /**
     * Converts this File to non-inlined data storage mode.
     * Before calling this method, a new DataBlock must be allocated.
     * This method copies all currently inlined data into the given DataBlock.
     * The given DataBlock is set as the currently cached DataBlock.
     * Afterwards, all Blocks returned by this
     * method must be saved. (this will include the primary INode and the DataBlock)
     * @param dataBlock a new DataBlock
     * @return list of Blocks to save
     */
    std::list<Block*> convertToNonInline(std::unique_ptr<DataBlock> dataBlock);

    /**
     * Adds a DataBlock to this file.
     * The given DataBlock is also set as cached.
     * @param dataBlock the new data block.
     * @return list of changed blocks, including the given data block
     */
    std::list<Block*> addDataBlock(std::unique_ptr<DataBlock> dataBlock);

    /**
     * Removes one DataBlock from this file.
     * After calling this, all returned blocks must be saved.
     * @return list of changed blocks, not including the now deleted block
     */
    std::list<Block*> removeDataBlock();

    /**
     * Gets the ID of the nths DataBlock of this file.
     * @param blockNo number of the data block, starts at zero
     * @return blockID or zero
     */
    uint32_t getDataBlockID(uint32_t blockNo);

    /**
     * Fills the given list with the blockIDs of all blocks currently
     * used to manage this file and its contents.
     * @param result the list to fill
     */
    void blocks(std::list<uint32_t> &result);
    
    /**
     * Sets the given DataBlocks as the currently cached DataBlock.
     * This overwrites any old cached DataBlock.
     * @param dataBlock
     */
    void setCachedDataBlock(std::unique_ptr<DataBlock> dataBlock);
    
    /**
     * Returns the ID of the currently cached DataBlock or zero.
     * @return blockID of cached DataBlock or zero (iff cache empty)
     */
    uint32_t getCachedDataBlockID();
    
    /**
     * Returns the dirty bit of the currently cached DataBlock.
     * Always returns false, if there is no cached DataBlock.
     * @return dirty bit, or false
     */
    bool cachedDataBlockIsDirty();
    
    /**
     * Returns the cached DataBlock, if any.
     * Afterwards, the DataBlock is no longer held by this File.
     * @return the cached DataBlock, or nullptr
     */
    std::unique_ptr<DataBlock> releaseCachedDataBlock();
    
    /**
     * Reads n bytes from position in the currently cached DataBlock and writes them to target.
     * Fails, if the cache is empty or the request is out-of-bounds.
     * @param target content is copied to target
     * @param pos start position in block in bytes
     * @param n number of bytes
     * @return true, iff successful
     */
    bool readFromCachedDataBlock(char *target, uint32_t pos, std::size_t n);
    
    /**
     * Reads n bytes from source and writes them to position in the currently cached DataBlock.
     * Fails, if the cache is empty or the request is out-of-bounds.
     * @param source content is read from source
     * @param pos target position in cache
     * @param n number of bytes
     * @return true, iff successful
     */
    bool writeToCachedDataBlock(const char *source, uint32_t pos, std::size_t n);

    virtual ~File();
private:
    /**
     * Callback to main impl, used to create new DataBlockLists, if required.
     */
    IDataBlockListCreator *blockListCreator;

    /**
     * The primary FileINode, holds important values
     * like the file size.
     */
    std::unique_ptr<FileINode> inode;

    /**
     * List of currently used DataBlockLists.
     */
    std::vector<DataBlockList*> blockLists;

    /**
     * Number of datablocks currently in use (non-inlined mode only).
     */
    uint32_t numberOfDataBlocks;
    
    /**
     * The currently loaded DataBlock.
     * Its content are not necessarily backed by the disk.
     * Prior to destorying this File object, the cached DataBlock must be synced to disk.
     * May be null.
     */
    std::unique_ptr<DataBlock> cachedDataBlock;

};

} // SDI4FS

#endif	// SDI4FS_FILE_H


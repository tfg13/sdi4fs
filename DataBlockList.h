/*
 * File:   DataBlockList.h
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on September 16, 2015, 11:44 PM
 */

#ifndef SDI4FS_DATABLOCKLIST_H
#define	SDI4FS_DATABLOCKLIST_H

#include "Block.h"

#include <cstdint>
#include <list>
#include <vector>

#include "StreamSelectorHeader.inc"

namespace SDI4FS {

/**
 * A DataBlockList is a block that contains links to file contents (DataBlocks).
 * A DataBlockList must always be referenced by exactly one FileINode.
 */
class DataBlockList : public Block {
public:
    /**
     * Creates a DataBlockList by reading contents from the stream.
     * Will read up to SDI4FS_BLOCK_SIZE bytes from the stream.
     * The caller must call seekg beforehand.
     * @param input the stream to read from
     */
    DataBlockList(STREAM &input);

    /**
     * Creates a new DataBlockList with the given parameters.
     * @param id the new unique block id
     */
    DataBlockList(uint32_t id);

    /**
     * Adds the given blockID to this DataBlockList.
     * @param id the blockID to add
     * @return true, iff successful (DataBlockList not full)
     */
    bool pushDataBlock(uint32_t id);

    /**
     * Removes and returns the last stored blockID.
     * @return the now removed blockID
     */
    uint32_t popDataBlock();

    /**
     * Returns the stored DataBlock blockID with the given index.
     * @param index the requested index
     * @return stored blockID or zero
     */
    uint32_t getDataBlock(size_t index);

    /**
     * Fills the given list with the blockIDs of all blocks currently
     * stored in this DataBlockList.
     * @param result the list to fill
     */
    void blocks(std::list<uint32_t> &result);

    virtual void save(STREAM &output);
    virtual ~DataBlockList();
private:
    /**
     * Content of this DataBlockList.
     */
    std::vector<uint32_t> entries;
};

} // SDI4FS

#endif	// SDI4FS_DATABLOCKLIST_H


/*
 * File:   Block.h
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on August 16, 2015, 3:54 PM
 */

#ifndef SDI4FS_BLOCK_H
#define	SDI4FS_BLOCK_H

#include <iostream>
#include <cstdint>

#include "StreamSelectorHeader.inc"

namespace SDI4FS {

/**
 * Represents a single Block on disk.
 * Superclass for all other Blocks, holds common values (id, last write time).
 */
class Block {
public:
    /**
     * Creates a block by reading contents from the stream.
     * Will read up to SDI4FS_BLOCK_SIZE bytes from the stream.
     * The caller must call seekg beforehand.
     * @param input the stream to read from
     */
    Block(STREAM &input);

    /**
     * Creates a new Block with the given id.
     * @param id new unique id for this block
     */
    Block(uint32_t id);

    virtual ~Block();

    /**
     * Returns the unique id of this block.
     * @return the id of this block
     */
    uint32_t getId();

    /**
     * Returns the last write time of this block.
     * @return UNIX-timestamp, last write time of this block
     */
    uint32_t getLastWriteTime();

    /**
     * Writes disk block to disk.
     * Writes up to SDI4FS_BLOCK_SIZE bytes.
     * Caller must call seekp beforehand.
     * Subclasses should override this, but still call Superclass::save().
     * @param output the stream to write into
     */
    virtual void save(STREAM &output);
private:
    /**
     * Unique id of this block.
     */
    uint32_t id;

    /**
     * UNIX-timestamp, last time this block was written.
     */
    uint32_t lastWriteTime;
};

} // SDI4FS

#endif	// SDI4FS_BLOCK_H

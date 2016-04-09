/*
 * File:   DataBlock.h
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on September 17, 2015, 6:22 PM
 */

#ifndef SDI4FS_DATABLOCK_H
#define	SDI4FS_DATABLOCK_H

#include "Block.h"

#include "StreamSelectorHeader.inc"
#include "Constants.inc"

namespace SDI4FS {

/**
 * Represents a DataBlock on disk.
 * File content is buffered in fs memory.
 */
class DataBlock : public Block {
public:
    /**
     * Creates a DataBlock by reading contents from the stream.
     * Will read up to SDI4FS_BLOCK_SIZE bytes from the stream.
     * The caller must call seekg beforehand.
     * @param input the stream to read from
     */
    DataBlock(STREAM &input);

    /**
     * Creates a new DataBlock with the given parameters.
     * @param id the new unique block id
     */
    DataBlock(uint32_t id);
    
    /**
     * Reads n bytes from position pos of the content and writes them to target.
     * Fails if the read is out-of-bounds.
     * @param target content is copied to target
     * @param pos position, in bytes
     * @param n number of bytes to read
     * @return true, iff succcessful
     */
    bool read(char *target, uint32_t pos, std::size_t n);

    /**
     * Writes n bytes from source to position pos of the content.
     * Fails if the write is out-of-bounds.
     * Sets the dirty bit.
     * @param source content is read from source
     * @param pos position, in bytes
     * @param n number of bytes to read
     * @return true, iff successful
     */
    bool write(const char *source, uint32_t pos, std::size_t n);
    
    /**
     * Returns the dirty bit.
     * @return the dirty bit
     */
    bool isDirty();

    virtual void save(STREAM &output);
    virtual ~DataBlock();
private:
    /**
     * Content (raw data)
     */
    uint8_t data[SDI4FS_MAX_BYTES_PER_DATABLOCK];
    
    /**
     * The dirty bit.
     */
    bool dirty;
};

} // SDI4FS

#endif	// SDI4FS_DATABLOCK_H


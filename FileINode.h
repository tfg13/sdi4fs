/*
 * File:   FileINode.h
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on September 16, 2015, 11:11 PM
 */

#ifndef SDI4FS_FILEINODE_H
#define	SDI4FS_FILEINODE_H

#include "INode.h"

#include <cstdint>
#include <iostream>
#include <vector>

#include "DataBlock.h"
#include "DataBlockList.h"
#include "StreamSelectorHeader.inc"
#include "Constants.inc"

namespace SDI4FS {

/**
 * Represents the primary INode of a regular file.
 */
class FileINode : public INode {
public:
    /**
     * Creates a FileINode by reading contents from the stream.
     * Will read up to SDI4FS_BLOCK_SIZE bytes from the stream.
     * The caller must call seekg beforehand.
     * @param input the stream to read from
     */
    FileINode(STREAM &input);

    /**
     * Creates a new, empty FileINode.
     * @param id the new unique block id
     */
    FileINode(uint32_t id);

    /**
     * Converts this FileINode to non-inlined form.
     * Irreversible, cannot be done twice.
     * Copies all inline data to the given DataBlock
     * @param blockList DataBlockList
     * @param dataBlock DataBlock that will hold the data in the future
     */
    void convertToNonInline(DataBlockList *blockList, DataBlock &dataBlock);

    /**
     * Adds the given blockID to this FileINode (non-inlined mode only).
     * @param id the blockID to add
     * @return true, iff successful (INode not full)
     */
    bool pushDataBlockList(uint32_t id);

    /**
     * Removes and returns the last stored blockID (non-inlined mode only).
     * @return the now removed blockID
     */
    uint32_t popDataBlockList();

    /**
     * Returns the stored DataBlockList blockID with the given index (non-inlined mode only).
     * @param index the requested index
     * @return stored blockID or zero
     */
    uint32_t getDataBlockList(size_t index);

    /**
     * Reads n bytes from position pos of the inlined content and writes them to target.
     * Fails if the FileINode is in non-inlined mode or the read is out-of-bounds.
     * @param target content is copied to target
     * @param pos position, in bytes
     * @param n number of bytes to read
     * @return true, iff succcessful
     */
    bool readInline(char *target, uint32_t pos, std::size_t n);

    /**
     * Writes n bytes from source to position pos of the inlined content.
     * Fails if the FileINode is in non-inlined mode or the write is out-of-bounds.
     * Does NOT modify the internal size.
     * @param source content is read from source
     * @param pos position, in bytes
     * @param n number of bytes to read
     * @return true, iff successful
     */
    bool writeInline(const char *source, uint32_t pos, std::size_t n);

    virtual uint32_t getUserVisibleSize_b();
    virtual void setInternalSize_b(uint32_t size_b);
    virtual void save(STREAM &output);

    virtual ~FileINode();
private:
    /**
     * Non-Inlined content (blockIDs of DataBlockLists)
     */
    std::vector<uint32_t> entries;

    /**
     * Inlined content (raw data)
     */
    uint8_t data[SDI4FS_MAX_BYTES_PER_INODE];
};

} // SDI4FS

#endif	// SDI4FS_FILEINODE_H


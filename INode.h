/*
 * File:   INode.h
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on August 16, 2015, 4:30 PM
 */

#ifndef SDI4FS_INODE_H
#define	SDI4FS_INODE_H

#define SDI4FS_INODE_TYPE_DIR 1
#define SDI4FS_INODE_TYPE_REGULARFILE 2
#define SDI4FS_INODE_TYPE_SYMLINK 3

#include "Block.h"

#include <cstdint>
#include <iostream>

#include "StreamSelectorHeader.inc"

namespace SDI4FS {

/**
 * Superclass for INodes, holds common values (creation time, size, type, inline flag, link counter)
 */
class INode : public Block {
public:
    /**
     * Creates an INode by reading contents from the stream.
     * Will read up to SDI4FS_BLOCK_SIZE bytes from the stream.
     * The caller must call seekg beforehand.
     * @param input the stream to read from
     */
    INode(STREAM &input);

    /**
     * Creates a new INode with the given parameters.
     * @param id the new unique block id
     * @param type the type of this INode
     */
    INode(uint32_t id, uint8_t type);

    /**
     * Returns the creation time of this block.
     * @return UNIX-timestamp, creation of this block
     */
    uint32_t getCreationTime();

    /**
     * Returns the type of this INode.
     * On of the TYPE defines above.
     * Attention: Only uint4_t on disk!
     * @return the type of this INode
     */
    uint8_t getType();

    /**
     * Returns the size in bytes of this INode or its content.
     * Exact meaning depends on type/subclass.
     * Example: Dirs usually return the size the directory data structures use on disk,
     * files return the number of bytes of the actual content (omitting fs overhead).
     * @return user visible size in bytes
     */
    virtual uint32_t getUserVisibleSize_b() = 0;

    /**
     * Returns the size of the actual content of this INode in bytes.
     * @return the size of the actual content in bytes
     */
    uint32_t getInternalSize_b();

    /**
     * Sets the size of the actual content of this INode in bytes.
     * @param size_b new size in bytes
     */
    virtual void setInternalSize_b(uint32_t size_b);

    /**
     * Returns true, iff the content of this INode is inlined.
     * @return true, iff inlined
     */
    bool isInlined();

    /**
     * Returns the number of links to this INode.
     * @return the number of links to this INode
     */
    uint16_t getLinkCounter();

    /**
     * Increments the link counter by one, if possible.
     * @return true, iff successful
     */
    bool incrementLinkCounter();

    /**
     * Decrements the link counter by one.
     */
    void decrementLinkCounter();

    virtual ~INode();
    virtual void save(STREAM &output);

protected:

    /**
     * Sets the inlined-flag to the given value.
     * @param inlined the new inline flag
     */
    void setInlined(bool inlined);

private:
    /**
     * UNIX-timestamp, time of INode creation.
     */
    uint32_t creationTime;

    /**
     * Size of the content of this INode.
     * Exact meaning depends on type/subclass.
     */
    uint32_t size_b;

    /**
     * Type of this INode,
     * one of the TYPE defines above.
     * Attention: Only uint4_t on disk!
     */
    uint8_t type;

    /**
     * True, iff content is inlined.
     * This means everything is saved within the data part of this INode.
     */
    bool inlined;

    /**
     * Number of other INodes pointing to this INode.
     */
    uint16_t linkCounter;
};

} // SDI4FS

#endif	// SDI4FS_INODE_H


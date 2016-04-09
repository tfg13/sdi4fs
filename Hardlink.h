/*
 * File:   Hardlink.h
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on August 21, 2015, 3:09 PM
 */

#ifndef SDI4FS_HARDLINK_H
#define	SDI4FS_HARDLINK_H

#include <cstdint>
#include <string>

#include "Constants.inc"

namespace SDI4FS {

/**
 * Represents a Hardlink (only holds the data).
 */
class Hardlink {
public:
    /**
     * Creates a new Hardlink with the given parameters.
     * @param linkName the link name, max 27 chars (+\0)
     * @param targetINodeBlockID the link target INode (blockID)
     */
    Hardlink(const std::string linkName, uint32_t targetINodeBlockID);

    /**
     * Returns the target this hardlink points to (blockID of an INode).
     * @return link target
     */
    uint32_t getTarget();

    /**
     * Returns the link name of this hardlink.
     * @returns the link name
     */
    const std::string getLinkName();

    virtual ~Hardlink();
private:
    /**
     * The name of this link.
     * 27 chars max (+ \0).
     */
    const std::string linkName;
    /**
     * Link target INode (its blockID).
     */
    const uint32_t targetINodeBlockID;
};

} // SDI4FS

#endif	// SDI4FS_HARDLINK_H


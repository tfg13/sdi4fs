/*
 * File:   DirectoryEntryList.h
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on August 20, 2015, 3:33 PM
 */

#ifndef SDI4FS_DIRECTORYENTRYLIST_H
#define	SDI4FS_DIRECTORYENTRYLIST_H

#include "Block.h"

#include <iostream>
#include <list>

#include "Hardlink.h"
#include "StreamSelectorHeader.inc"

namespace SDI4FS {

/**
 * DirectoryEntryLists are a block that contains directory contents (hardlinks).
 * A DirectoryEntryList must always be referenced by exactly one DirectoryINode.
 */
class DirectoryEntryList : public Block {
public:
    /**
     * Creates a DirectoryEntryList by reading contents from the stream.
     * Will read up to SDI4FS_BLOCK_SIZE bytes from the stream.
     * The caller must call seekg beforehand.
     * @param input the stream to read from
     */
    DirectoryEntryList(STREAM &input);

    /**
     * Creates a new DirectoryEntryList with the given parameters.
     * @param id the new unique block id
     */
    DirectoryEntryList(uint32_t id);

    /**
     * Tries to save a reference to the given Hardlink inline in this DirEntryList.
     * The Hardlink must be located on the heap. If add is successful, this
     * class will take the responsibility to delete the hardlink.
     * @param link a pointer to the heap-allocated link to save
     * @return true, if successful, false if full
     */
    bool addLink(Hardlink *link);

    /**
     * Removes a link from this DirEntryList.
     * The caller gains responsibility to delete the hardlink.
     * @param linkName the link name
     * @return the hardlink, or NULL
     */
    Hardlink* removeLink(std::string linkName);

    /**
     * Finds a Hardlink by its link name within the contents of this DirEntryList.
     * @param linkName the link name
     * @return the hardlink, or NULL
     */
    Hardlink* findLink(std::string linkName);

    /**
     * Returns the number of hardlinks stored in this DirEntryList.
     * @return the number of hardlinks stored
     */
    uint32_t getNumberOfHardlinks();

    /**
     * Fills the given list with the names of all hardlinks stored in this DirEntryList.
     * Dotfiles included.
     * @param result will be filled with hardlink names
     */
    void ls(std::list<std::string> &result);

    virtual ~DirectoryEntryList();
    virtual void save(STREAM &output);
private:
    /**
     * Content of this DirEntryList.
     */
    std::list<Hardlink*> entries;
};

} // SDI4FS

#endif	// SDI4FS_DIRECTORYENTRYLIST_H


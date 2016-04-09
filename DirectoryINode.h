/*
 * File:   Directory.h
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on August 17, 2015, 6:06 PM
 */

#ifndef SDI4FS_DIRECTORYINODE_H
#define	SDI4FS_DIRECTORYINODE_H

#include "INode.h"

#include <cstdint>
#include <iostream>
#include <list>

#include "Hardlink.h"
#include "DirectoryEntryList.h"
#include "StreamSelectorHeader.inc"

namespace SDI4FS {

/**
 * Represents one Directory INode on disk.
 * Does *not* represent a directory, this is what
 * the @link Directory is for.
 * This class represents the primary INode of a Directory.
 */
class DirectoryINode : public INode {
public:
    /**
     * Creates a Directory by reading contents from the stream.
     * Will read up to SDI4FS_BLOCK_SIZE bytes from the stream.
     * The caller must call seekg beforehand.
     * @param input the stream to read from
     */
    DirectoryINode(STREAM &input);

    /**
     * Creates a new, empty DirectoryINode.
     * @param id the new unique block id
     */
    DirectoryINode(uint32_t id);

    virtual ~DirectoryINode();
    virtual void setInternalSize_b(uint32_t size_b);

    /**
     * Tries to save a reference to the given Hardlink inline in this INode.
     * The Hardlink must be located on the heap. If add is successful, this
     * class will take the responsibility to delete the hardlink.
     * @param link a pointer to the heap-allocated link to save
     * @return true, if successful, false if full
     */
    bool addLink(Hardlink *link);

    /**
     * Removes a inline-stored link from this INode.
     * The caller gains responsibility to delete the hardlink.
     * @param linkName the link name
     * @return the hardlink, or NULL
     */
    Hardlink* removeLink(std::string linkName);

    /**
     * Finds a Hardlink by its link name within the inline-contents of this inode.
     * @param linkName the link name
     * @return the hardlink, or NULL
     */
    Hardlink* findLink(std::string linkName);

    /**
     * Returns the number of hardlinks stored inline in this DirectoryINode.
     * @return the number of hardlinks stored inline
     */
    uint32_t getNumberOfHardlinks();

    /**
     * Fills the given list with the names of all hardlinks stored inline in this DirectoryINode.
     * Dotfiles included.
     * @param result will be filled with hardlink names
     */
    void ls(std::list<std::string> &result);

    /**
     * Converts this DirectoryINode to non-inlined form.
     * Irreversible, cannot be done twice.
     * @param empty DirEntryList to reference the returned hardlinks in the future
     */
    void convertToNonInline(DirectoryEntryList *entryList);

    /**
     * Returns a list of blocksIDs pointing to DirEntryLists,
     * which contain the contents of this directory (non-inlined only)
     * @return list of blockIDs for DirEntryLists
     */
    const std::list<uint32_t>& getDirEntryListIDs();

    /**
     * Tries to save the given blockID in the list of known DirEntryLists.
     * @param blockID blockID of the DirEntryList
     * @return true, if successful, false if full
     */
    bool addDirEntryList(uint32_t blockID);

    /**
     * Removes the given blockID from the list of known DirEntryLists.
     * @param blockID blockID of the DirEntryList
     * @return true, if successful, false if not found
     */
    bool removeDirEntryList(uint32_t blockID);

    virtual void save(STREAM &output);
    virtual uint32_t getUserVisibleSize_b();
private:
    /**
     * Content of this INode, if data is inline.
     */
    std::list<Hardlink*> entries;

    /**
     * Content of this INode, if data is non-inline.
     * (Content is a list of IDs pointing to DirEntryLists)
     */
    std::list<uint32_t> dirEntryListIDs;
};

} // SDI4FS

#endif	// SDI4FS_DIRECTORYINODE_H


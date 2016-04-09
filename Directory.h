/*
 * File:   Directory.h
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on August 20, 2015, 3:24 PM
 */

#ifndef SDI4FS_DIRECTORY_H
#define	SDI4FS_DIRECTORY_H

#include "IPrimaryINodeHolder.h"

#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "Constants.inc"
#include "DirectoryINode.h"
#include "DirectoryEntryList.h"
#include "IDirectoryEntryListCreator.h"

namespace SDI4FS {

/**
 * Represents a "real" Directory.
 */
class Directory : public IPrimaryINodeHolder {
public:
    /**
     * Creates a new Directory Object for EXISTING (on disk) directorys with the given primary INode.
     * This is a two-stage process. Calling this constructor results
     * in a list of blockIDs of DirectoryEntryList blocks that need to be loaded
     * by the caller and fed into this class by calling init()
     * @param blockCreator used to create new DirectoryEntryList blocks, if required
     * @param primary the primary INode
     * @param entryListIDs pointer to empty list, will be filled with required entry list ids.
     */
    Directory(IDirectoryEntryListCreator *blockCreator, std::unique_ptr<DirectoryINode> primary, std::list<uint32_t> *entryListIDs);

    /**
     * Creates a new Directory as a children of the given parent.
     * It is required to save() the DirectoryINode and its parent after calling this constructor.
     * @param blockCreator used to create new DirectoryEntryList blocks, if required
     * @param empty a fresh DirectoryINode
     * @param parent the parent dir
     */
    Directory(IDirectoryEntryListCreator *blockCreator, std::unique_ptr<DirectoryINode> empty, std::unique_ptr<Directory> &parent);

    /**
     * Creates a new root Directory.
     * It is required to save() the DirectoryINode after calling this constructor.
     * @param blockCreator used to create new DirectoryEntryList blocks, if required
     * @param empty a fresh DirectoryINode
     */
    Directory(IDirectoryEntryListCreator *blockCreator, std::unique_ptr<DirectoryINode> empty);

    /**
     * Finishes the initialization of a non-inlined Directory read from disk.
     * This must be called iff the Constructor that reads from disk returns at least one blockID of DirectoryEntryLists.
     * @param entryList list of requested (by the constructor) blocks
     */
    void init(std::vector<std::unique_ptr<DirectoryEntryList>> &entryLists);

    /**
     * Returns the primary DirectoryINode of this directory.
     * @return the primary INode
     */
    virtual DirectoryINode& getPrimaryINode();

    /**
     * Searches for an existing hardlink with the given name.
     * @param name the name of the link
     * @return blockID of the link target or zero
     */
    uint32_t searchHardlink(std::string name);

    /**
     * Returns the number of children this directory has.
     * @return the number of children
     */
    uint32_t getChildCount();

    /**
     * Internal method to add hardlinks.
     * After calling this method, all returned Blocks must be saved.
     * @param target the link target (any INode)
     * @param name link name (max 27 chars + \0, no / or \0 (unchecked!)
     * @return list of modified blocks that need to be saved
     */
    std::list<Block*> addHardlink(INode &target, std::string name);

    /**
     * Internal method to remove Hardlinks.
     * After calling this method, all returned Blocks must be saved.
     * If given Hardlink does not exists, nothing is changed.
     * @param target the link target (any INode)
     * @param name link name to remove
     * @return list of modified blocks that need to be saved
     */
    std::list<Block*> rmHardlink(INode &target, std::string name);

    /**
     * Fills the given list with the names of all hardlinks in this directory.
     * Dotfiles included.
     * @param result will be filled with hardlink names
     */
    void ls(std::list<std::string> &result);

    /**
     * Returns a list of pointers to all currently used DirEntryLists.
     * @return a list of pointers to all DirEntryLists
     */
    const std::list<DirectoryEntryList*> blocks();

    virtual ~Directory();
private:
    /**
     * Callback to main impl, used to create new DirEntryLists, if required.
     */
    IDirectoryEntryListCreator *blockCreator;

    /**
     * The primary DirectoryINode, holds important values
     * like the link counter.
     */
    std::unique_ptr<DirectoryINode> inode;

    /**
     * Number of children this directory has
     */
    uint32_t childCount;

    /**
     * List of currently used DirectoryEntryLists.
     */
    std::list<DirectoryEntryList*> entryLists;

};

} // SDI4FS

#endif	// SDI4FS_DIRECTORY_H

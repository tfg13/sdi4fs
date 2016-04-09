/*
 * File:   Directory.cpp
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on August 20, 2015, 3:24 PM
 */

#include "Directory.h"

#include <cstdint>
#include <list>
#include <memory>
#include <string>

#include "DirectoryINode.h"
#include "DirectoryEntryList.h"

namespace SDI4FS {

Directory::Directory(IDirectoryEntryListCreator *blockCreator, std::unique_ptr<DirectoryINode> primary, std::list<uint32_t> *entryListIDs) : blockCreator(blockCreator), inode(std::move(primary)), entryLists() {
    if (!inode->isInlined()) {
        // copy list of dirEntryLists
        for (auto iter = inode->getDirEntryListIDs().begin(); iter != inode->getDirEntryListIDs().end(); ++iter) {
            entryListIDs->push_back(*iter);
        }
        // copying the ids requests them to be loaded. caller will (=must) call init before using this object to complete construction
    } else {
        childCount = inode->getNumberOfHardlinks();
    }
}

Directory::Directory(IDirectoryEntryListCreator *blockCreator, std::unique_ptr<DirectoryINode> empty, std::unique_ptr<Directory> &parent) : blockCreator(blockCreator), inode(std::move(empty)), childCount(0), entryLists() {
    // create . and .. links
    // this never fails and never allocates new blocks, so ignore return values
    addHardlink(*inode.get(), ".");
    addHardlink(parent->getPrimaryINode(), "..");
}

Directory::Directory(IDirectoryEntryListCreator *blockCreator, std::unique_ptr<DirectoryINode> empty) : blockCreator(blockCreator), inode(std::move(empty)), childCount(0), entryLists() {
    // create . and .. links
    // this never fails and never allocates new blocks, so ignore return values
    addHardlink(*inode.get(), ".");
    addHardlink(*inode.get(), "..");
}

void Directory::init(std::vector<std::unique_ptr<DirectoryEntryList>> &entryLists) {
    // sanity check
    if (inode->isInlined()) {
        std::cout << "fs: error - wrong initialization of directory object" << std::endl;
    }
    // copy to internal list (and count entrys)
    for (auto &list : entryLists) {
        childCount += list->getNumberOfHardlinks();
        this->entryLists.push_back(list.release());
    }
}

Directory::~Directory() {
    for (auto iter = entryLists.begin(); iter != entryLists.end(); ++iter) {
        delete *iter;
    }
}

DirectoryINode& Directory::getPrimaryINode() {
    return *inode.get();
}

uint32_t Directory::getChildCount() {
    return childCount;
}

std::list<Block*> Directory::addHardlink(INode &target, std::string name) {
    std::list<Block*> changedBlocks;
    // already present?
    if (searchHardlink(name) != 0) {
        std::cout << "fs: warning - cannot add hardlink with name \"" << name << "\", already present." << std::endl;
        return changedBlocks;
    }
    // capacity check
    if (childCount == SDI4FS_MAX_HARDLINKS_PER_DIR) {
        std::cout << "fs: warning - cannot add hardlink, max # of links in dir " << inode->getId() << " reached." << std::endl;
        return changedBlocks;
    }
    // increment linkcounter in target
    if (!target.incrementLinkCounter()) {
        std::cout << "fs: warning - cannot add hardlink, max # of links to INode " << target.getId() << " reached." << std::endl;
        return changedBlocks;
    }
    changedBlocks.push_back(&target);

    Hardlink *link = new Hardlink(name, target.getId());
    bool successInline = false;
    if (inode->isInlined()) {
        // try putting it in here
        if (inode->addLink(link)) {
            successInline = true;
            changedBlocks.push_back(&getPrimaryINode());
        }
    }
    if (!successInline && inode->isInlined()) {
        // alloc first DirEntryList
        DirectoryEntryList *newDirEntryList = blockCreator->alloc();
        // sanity check, FS guarantees that this will never happen
        if (newDirEntryList == NULL) {
            std::cout << "fs: error - cannot alloc DirectoryEntryList" << std::endl;
            target.decrementLinkCounter();
            delete link;
            return changedBlocks;
        }
        // convert primary INode
        inode->convertToNonInline(newDirEntryList);
        changedBlocks.push_back(&getPrimaryINode());
        // save new list
        entryLists.push_back(newDirEntryList);
        // remember this changed block
        changedBlocks.push_back(newDirEntryList);
        // (now jump into next if to also save the new hardlink)
    }
    if (!successInline) {
        // try to save to existing dirEntryList
        bool createNewList = true;
        for (auto iter = entryLists.begin(); iter != entryLists.end(); ++iter) {
            if ((*iter)->addLink(link)) {
                // ok!
                createNewList = false;
                changedBlocks.push_back((*iter));
                break;
            }
        }
        // already saved in old lists?
        if (createNewList) {
            // all old lists were full, create a new one
            DirectoryEntryList *newDirEntryList = blockCreator->alloc();
            // sanity check, FS guarantees that this will never happen
            if (newDirEntryList == NULL) {
                std::cout << "fs: error - cannot alloc DirectoryEntryList(2)" << std::endl;
                target.decrementLinkCounter();
                delete link;
                return changedBlocks;
            }
            // push new link there
            newDirEntryList->addLink(link);
            // save new list
            entryLists.push_back(newDirEntryList);
            inode->addDirEntryList(newDirEntryList->getId());
            // remember for caller
            changedBlocks.push_back(&getPrimaryINode());
            changedBlocks.push_back(newDirEntryList);
        }
    }
    ++childCount;
    return changedBlocks;
}

uint32_t Directory::searchHardlink(std::string name) {
    // sanity check
    if (name.empty() || name.find_first_of('/') != name.npos) {
        std::cout << "fs: error - cannot search for hardlink with name \"" << name << "\", not a valid link name" << std::endl;
        return 0;
    }
    if (inode->isInlined()) {
        // search in primary inode only
        Hardlink *link = inode->findLink(name);
        if (link == NULL) {
            // not found
            return 0;
        }
        return link->getTarget();
    } else {
        // search in DirEntryLists
        for (auto iter = entryLists.begin(); iter != entryLists.end(); ++iter) {
            Hardlink *link = (*iter)->findLink(name);
            if (link != NULL) {
                // found!
                return link->getTarget();
            }
        }
        // not found
        return 0;
    }
}

std::list<Block*> Directory::rmHardlink(INode &target, std::string name) {
    std::list<Block*> changedBlocks;
    // sanity check
    if (name.empty() || name.find_first_of('/') != name.npos) {
        std::cout << "fs: error - cannot remove hardlink with name \"" << name << "\", not a valid link name" << std::endl;
        return changedBlocks;
    }
    target.decrementLinkCounter();
    changedBlocks.push_back(&target);
    if (inode->isInlined()) {
        delete inode->removeLink(name);
        changedBlocks.push_back(&getPrimaryINode());
    } else {
        // search in DirEntryLists
        for (auto iter = entryLists.begin(); iter != entryLists.end(); ++iter) {
            Hardlink *link = (*iter)->removeLink(name);
            if (link != NULL) {
                delete link;
                // removed, list now empty?
                if ((*iter)->getNumberOfHardlinks() == 0) {
                    blockCreator->dealloc(*iter);
                    inode->removeDirEntryList((*iter)->getId());
                    entryLists.erase(iter);
                    changedBlocks.push_back(&getPrimaryINode());
                } else {
                    // block still valid
                    changedBlocks.push_back(*iter);
                }
                break;
            }
        }
    }

    --childCount;
    return changedBlocks;
}

void Directory::ls(std::list<std::string> &result) {
    if (inode->isInlined()) {
        inode->ls(result);
    } else {
        for (DirectoryEntryList *list : entryLists) {
            list->ls(result);
        }
    }
}

const std::list<DirectoryEntryList*> Directory::blocks() {
    return entryLists;
}

} // SDI4FS

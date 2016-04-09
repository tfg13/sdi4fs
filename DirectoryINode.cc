/*
 * File:   Directory.cpp
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on August 17, 2015, 6:07 PM
 */

#include "DirectoryINode.h"
#include "INode.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

#include "Constants.inc"
#include "DirectoryEntryList.h"
#include "StreamUtils.inc"

namespace SDI4FS {

DirectoryINode::DirectoryINode(STREAM &input) : INode(input), entries(), dirEntryListIDs() {
    // verify INode type
    if (getType() != SDI4FS_INODE_TYPE_DIR) {
        std::cout << "fs: fatal error - inconsistency - reading DirectoryINode from INode of different type: " << getType() << std::endl;
        return;
    }
    if (isInlined()) {
        // skip unused space
        input.seekg(12, input.cur);
        // read entries
        for (int i = 0; i < SDI4FS_MAX_LINKS_PER_DIRENTRYLIST; ++i) {
            uint32_t id;
            char cname[SDI4FS_MAX_LINK_NAME_LENGTH];
            read32(input, &id);
            if (id != 0) {
                // read name
                read64(input, (uint64_t*) & cname[0]);
                read64(input, (uint64_t*) & cname[8]);
                read64(input, (uint64_t*) & cname[16]);
                read32(input, (uint32_t*) & cname[24]);
                // save
                Hardlink *link = new Hardlink(std::string(cname), id);
                entries.push_back(link);
            } else {
                // skip name
                input.seekg(SDI4FS_MAX_LINK_NAME_LENGTH, input.cur);
            }
        }
    } else {
        for (int i = 0; i < SDI4FS_MAX_DIRENTRYLISTS_PER_DIR; ++i) {
            uint32_t linkTarget;
            read32(input, &linkTarget);
            if (linkTarget != 0) {
                // save
                dirEntryListIDs.push_back(linkTarget);
            }
        }
    }
}

DirectoryINode::DirectoryINode(uint32_t id) : INode(id, SDI4FS_INODE_TYPE_DIR), entries(), dirEntryListIDs() {
    // nothing to do
}

DirectoryINode::~DirectoryINode() {
    // clean up
    for (auto iter = entries.begin(); iter != entries.end(); ++iter) {
        delete *iter;
    }
}

void DirectoryINode::setInternalSize_b(uint32_t size_b) {
    // ignore, size field is not used for dirs
    (void) size_b;
}

void DirectoryINode::save(STREAM &output) {
    INode::save(output);
    if (isInlined()) {
        // skip unused space
        output.seekp(12, output.cur);
        // write entries
        for (auto iter = entries.begin(); iter != entries.end(); ++iter) {
            Hardlink *link = *iter;
            // write target (id))
            write32(output, link->getTarget());
            // write name
            char cname[SDI4FS_MAX_LINK_NAME_LENGTH];
            strncpy(cname, link->getLinkName().c_str(), SDI4FS_MAX_LINK_NAME_LENGTH);
            // better save than sorry
            cname[SDI4FS_MAX_LINK_NAME_LENGTH - 1] = 0;
            for (int i = 0; i < SDI4FS_MAX_LINK_NAME_LENGTH; ++i) {
                write8(output, (uint8_t) cname[i]);
            }
        }
        // null ids of rest
        for (int i = entries.size(); i < SDI4FS_MAX_LINKS_PER_DIRENTRYLIST; ++i) {
            // null id
            write32(output, 0);
            // skip name
            output.seekp(SDI4FS_MAX_LINK_NAME_LENGTH, output.cur);
        }
    } else {
        // write entries
        for (auto iter = dirEntryListIDs.begin(); iter != dirEntryListIDs.end(); ++iter) {
            // write id
            write32(output, *iter);
        }
        // null rest
        for (int i = dirEntryListIDs.size(); i < SDI4FS_MAX_DIRENTRYLISTS_PER_DIR; ++i) {
            // write 0 (zero)
            write32(output, 0);
        }
    }
}

uint32_t DirectoryINode::getUserVisibleSize_b() {
    return (dirEntryListIDs.size() + 1) * SDI4FS_BLOCK_SIZE; // +1 is self
}

bool DirectoryINode::addLink(Hardlink *link) {
    // sanity check
    if (!isInlined()) {
        std::cout << "fs: error - cannot push hardlink to non-inlined INode, id " << getId() << std::endl;
        return false;
    }
    // size check
    if (entries.size() >= SDI4FS_MAX_LINKS_PER_DIRENTRYLIST) {
        // full
        return false;
    }
    // all ok
    entries.push_back(link);
    return true;
}

Hardlink* DirectoryINode::removeLink(std::string linkName) {
    // sanity check
    if (!isInlined()) {
        std::cout << "fs: error - cannot remove hardlink in non-inlined INode, id " << getId() << std::endl;
        return NULL;
    }
    for (auto iter = entries.begin(); iter != entries.end(); ++iter) {
        if (!(*iter)->getLinkName().compare(linkName)) {
            Hardlink *link = *iter;
            entries.erase(iter);
            return link;
        }
    }

    // not found, should never happen
    std::cout << "fs: error - cannot remove hardlink from non-inlined INode, id " << getId() << ", link \"" << linkName << "\" not found" << std::endl;
    return NULL;
}

Hardlink* DirectoryINode::findLink(std::string linkName) {
    // sanity check
    if (!isInlined()) {
        std::cout << "fs: error - cannot find hardlink in non-inlined INode, id " << getId() << std::endl;
        return NULL;
    }
    for (auto iter = entries.begin(); iter != entries.end(); ++iter) {
        if (!(*iter)->getLinkName().compare(linkName)) {
            // found
            return *iter;
        }
    }
    // not found
    return NULL;
}

uint32_t DirectoryINode::getNumberOfHardlinks() {
    return entries.size();
}

void DirectoryINode::ls(std::list<std::string> &result) {
    for (Hardlink *link : entries) {
        result.push_back(link->getLinkName());
    }
}

const std::list<uint32_t>& DirectoryINode::getDirEntryListIDs() {
    // sanity check
    if (isInlined()) {
        std::cout << "fs: warning - returning dirEntryList for inlined INode, id " << getId() << std::endl;
    }
    return dirEntryListIDs;
}

bool DirectoryINode::addDirEntryList(uint32_t blockID) {
    // sanity check
    if (isInlined()) {
        std::cout << "fs: error - cannot push dirEntryList for inlined INode, id " << getId() << std::endl;
        return false;
    }
    // size check
    if (dirEntryListIDs.size() == SDI4FS_MAX_DIRENTRYLISTS_PER_DIR) {
        std::cout << "fs: error - cannot push dirEntryList, INode full, id " << getId() << std::endl;
        return false;
    }
    dirEntryListIDs.push_back(blockID);
    return true;
}

bool DirectoryINode::removeDirEntryList(uint32_t blockID) {
    // sanity check
    if (isInlined()) {
        std::cout << "fs: error - cannot remove dirEntryList for inlined INode, id " << getId() << std::endl;
        return false;
    }
    for (auto iter = dirEntryListIDs.begin(); iter != dirEntryListIDs.end(); ++iter) {
        if (*iter == blockID) {
            dirEntryListIDs.erase(iter);
            return true;
        }
    }
    // not found
    return false;
}

void DirectoryINode::convertToNonInline(DirectoryEntryList *entryList) {
    // sanity check
    if (!isInlined()) {
        std::cout << "fs: error - cannot convert non-inlined INode to non-inlined form again, id " << getId() << std::endl;
        return;
    }
    if (entryList->getNumberOfHardlinks() != 0) {
        std::cout << "fs: error - cannot convert non-inlined INode, given list is not empty " << std::endl;
        return;
    }

    // save entries in new list (external block)
    for (auto iter = entries.begin(); iter != entries.end(); ++iter) {
        if (!entryList->addLink(*iter)) {
            // should never happen
            std::cout << "fs: error - (while converting to non-inline dir) cannot store a hardlink " << std::endl;
            return;
        }
    }
    // delete all entries (old inline list)
    entries.clear();
    // set to non-inline
    setInlined(false);
    // remember given dirEntryList
    dirEntryListIDs.push_back(entryList->getId());
}

} //SDI4FS


/*
 * File:   DirectoryEntryListINode.cpp
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on August 20, 2015, 3:33 PM
 */

#include "DirectoryEntryList.h"
#include "Block.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <iostream>

#include "Constants.inc"
#include "StreamUtils.inc"
#include "bits/stl_list.h"

namespace SDI4FS {

DirectoryEntryList::DirectoryEntryList(STREAM &input) : Block(input), entries() {
    // skip unused space
    input.seekg(24, input.cur);
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
}

DirectoryEntryList::DirectoryEntryList(uint32_t id) : Block(id), entries() {
    // nothing to do here
}

DirectoryEntryList::~DirectoryEntryList() {
    // clean up
    for (auto iter = entries.begin(); iter != entries.end(); ++iter) {
        delete *iter;
    }
}

void DirectoryEntryList::save(STREAM &output) {
    Block::save(output);
    // skip unused space
    output.seekp(24, output.cur);
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
}

bool DirectoryEntryList::addLink(Hardlink *link) {
    // size check
    if (entries.size() >= SDI4FS_MAX_LINKS_PER_DIRENTRYLIST) {
        // full
        return false;
    }
    // all ok
    entries.push_back(link);
    return true;
}

Hardlink* DirectoryEntryList::removeLink(std::string linkName) {
    for (auto iter = entries.begin(); iter != entries.end(); ++iter) {
        if (!(*iter)->getLinkName().compare(linkName)) {
            Hardlink *link = *iter;
            entries.erase(iter);
            return link;
        }
    }

    // not found
    return NULL;
}

Hardlink* DirectoryEntryList::findLink(std::string linkName) {
    for (auto iter = entries.begin(); iter != entries.end(); ++iter) {
        if (!(*iter)->getLinkName().compare(linkName)) {
            // found
            return *iter;
        }
    }
    // not found
    return NULL;
}

uint32_t DirectoryEntryList::getNumberOfHardlinks() {
    return entries.size();
}

void DirectoryEntryList::ls(std::list<std::string> &result) {
    for (Hardlink *link : entries) {
        result.push_back(link->getLinkName());
    }
}

} // SDI4FS

/*
 * File:   DataBlockList.cpp
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on September 16, 2015, 11:44 PM
 */

#include "DataBlockList.h"
#include "Block.h"

#include <cstdint>
#include <vector>
#include <list>

#include "Constants.inc"
#include "StreamUtils.inc"
#include "bits/stl_list.h"

namespace SDI4FS {

DataBlockList::DataBlockList(STREAM &input) : Block(input), entries() {
    // read entries
    for (int i = 0; i < SDI4FS_MAX_DATABLOCKS_PER_DATABLOCKLIST; ++i) {
        uint32_t id;
        read32(input, &id);
        if (id != 0) {
            // save
            entries.push_back(id);
        } else {
            // no gaps allowed
            break;
        }
    }
}

DataBlockList::DataBlockList(uint32_t id) : Block(id), entries() {
    // nothing to do here
}

DataBlockList::~DataBlockList() {
}

void DataBlockList::save(STREAM &output) {
    Block::save(output);
    // write entries
    for (auto iter = entries.begin(); iter != entries.end(); ++iter) {
        write32(output, *iter);
    }
    // null ids of rest
    for (int i = entries.size(); i < SDI4FS_MAX_DATABLOCKS_PER_DATABLOCKLIST; ++i) {
        // null id
        write32(output, 0);
    }
}

bool DataBlockList::pushDataBlock(uint32_t id) {
    if (entries.size() == SDI4FS_MAX_DATABLOCKS_PER_DATABLOCKLIST) {
        // full
        return false;
    }
    entries.push_back(id);
    return true;
}

uint32_t DataBlockList::popDataBlock() {
    if (entries.empty()) {
        return 0;
    }
    uint32_t result = entries[entries.size() - 1];
    entries.pop_back();
    return result;
}

uint32_t DataBlockList::getDataBlock(size_t index) {
    if (entries.size() <= index) {
        return 0;
    }
    return entries[index];
}

void DataBlockList::blocks(std::list<uint32_t> &result) {
    for (uint32_t id : entries) {
        result.push_back(id);
    }
}

} // SDI4FS


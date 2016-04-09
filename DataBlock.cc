/*
 * File:   DataBlock.cpp
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on September 17, 2015, 6:22 PM
 */

#include "DataBlock.h"
#include "Block.h"

#include <cstring>

#include "StreamUtils.inc"

namespace SDI4FS {

DataBlock::DataBlock(STREAM &input) : Block(input), dirty(false) {
    // read stored content
    readN(input, &data[0], SDI4FS_MAX_BYTES_PER_DATABLOCK);
}

DataBlock::DataBlock(uint32_t id) : Block(id), dirty(false) {
    // nothing to do here besides superclass constructor
}

DataBlock::~DataBlock() {
    // empty
}

bool DataBlock::read(char *target, uint32_t pos, std::size_t n) {
    if (pos > SDI4FS_MAX_BYTES_PER_DATABLOCK || (pos + n) > SDI4FS_MAX_BYTES_PER_DATABLOCK) {
        std::cout << "fs: error - attempting to read data with out-of-bound positions:" << getId() << " " << pos << " " << n << std::endl;
        return false;
    }
    memcpy(target, &data[pos], n);
    return true;
}

bool DataBlock::write(const char *source, uint32_t pos, std::size_t n) {
    if (pos > SDI4FS_MAX_BYTES_PER_DATABLOCK || (pos + n) > SDI4FS_MAX_BYTES_PER_DATABLOCK) {
        std::cout << "fs: error - attempting to write data with out-of-bound positions:" << getId() << " " << pos << " " << n << std::endl;
        return false;
    }
    memcpy(&data[pos], source, n);
    dirty = true;
    return true;
}

void DataBlock::save(STREAM &output) {
    Block::save(output);
    // write content
    writeN(output, &data[0], SDI4FS_MAX_BYTES_PER_DATABLOCK);
}

bool DataBlock::isDirty() {
    return dirty;
}

} // SDI4FS

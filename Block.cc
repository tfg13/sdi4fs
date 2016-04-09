/*
 * File:   Block.cpp
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on August 16, 2015, 3:54 PM
 */

#include "Block.h"

#include <cstdint>
#include <iostream>

#include "StreamUtils.inc"
#include "TimeUtils.inc"

namespace SDI4FS {

Block::Block(STREAM &input) {
    read32(input, &id);
    if (id == 0) {
        std::cout << "fs: error - found block with id zero" << std::endl;
        return;
    }
    read32(input, &lastWriteTime);
}

Block::Block(uint32_t id) {
    this->id = id;
}

Block::~Block() {
}

uint32_t Block::getId() {
    return id;
}

uint32_t Block::getLastWriteTime() {
    return lastWriteTime;
}

void Block::save(STREAM &output) {
    // all blocks have: id, lastWriteTime
    write32(output, id);
    lastWriteTime = now();
    write32(output, lastWriteTime);
}

} // SDI4FS

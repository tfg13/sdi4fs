/*
 * File:   INode.cpp
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on August 16, 2015, 4:30 PM
 */

#include "INode.h"
#include "Block.h"

#include <cstdint>
#include <iostream>

#include "Constants.inc"
#include "StreamUtils.inc"
#include "TimeUtils.inc"

namespace SDI4FS {

INode::INode(STREAM &input) : Block::Block(input) {
    // read creationTime and size_b
    read32(input, &creationTime);
    read32(input, &size_b);
    // read type (4 bits) + inlined (1bit)
    uint8_t typeAndInline;
    read8(input, &typeAndInline);
    type = (typeAndInline >> 4) & 0xF;
    inlined = (typeAndInline & 0x08) != 0 ? true : false;
    // jump 1 byte forward
    input.seekg(1, input.cur);
    // read linkCounter
    read16(input, &linkCounter);
}

INode::INode(uint32_t id, uint8_t type) : Block::Block(id),
size_b(0), inlined(true), linkCounter(0) {
    creationTime = now();
    // only 4bits on disk
    if (type > 0xF) {
        std::cout << "fs: error - illegal INode type: " << type << std::endl;
    }
    this->type = type;
}

INode::~INode() {
}

uint32_t INode::getCreationTime() {
    return creationTime;
}

uint32_t INode::getInternalSize_b() {
    return size_b;
}

void INode::setInternalSize_b(uint32_t size_b) {
    this->size_b = size_b;
}

bool INode::isInlined() {
    return inlined;
}

void INode::setInlined(bool inlined) {
    this->inlined = inlined;
}

uint8_t INode::getType() {
    return type;
}

uint16_t INode::getLinkCounter() {
    return linkCounter;
}

void INode::save(STREAM &output) {
    // call super first (*cough* anitpattern *cough*)
    Block::save(output);
    // write creationTime, size, type, inline, link counter
    write32(output, creationTime);
    write32(output, size_b);
    uint8_t typeAndInline = type << 4 | inlined << 3;
    write8(output, typeAndInline);
    output.seekp(1, output.cur);
    write16(output, linkCounter);
}

bool INode::incrementLinkCounter() {
    if (linkCounter != SDI4FS_MAX_NUMBER_OF_LINKS_TO_INODE) {
        ++linkCounter;
        return true;
    }
    return false;
}

void INode::decrementLinkCounter() {
    --linkCounter;
}

} // SDI4FS

/*
 * File:   Hardlink.cpp
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on August 21, 2015, 3:09 PM
 */

#include "Hardlink.h"

#include <cstdint>
#include <iostream>
#include <string>

#include "Constants.inc"

namespace SDI4FS {

Hardlink::Hardlink(const std::string linkName, uint32_t targetINodeBlockID) :
linkName(linkName), targetINodeBlockID(targetINodeBlockID) {
    if (linkName.empty() || linkName.size() >= SDI4FS_MAX_LINK_NAME_LENGTH) {
        std::cout << "fs: error - hardlink name exceeds limits, min 1, max " << (SDI4FS_MAX_LINK_NAME_LENGTH - 1) << ", got " << linkName.size() << std::endl;
    }
}

Hardlink::~Hardlink() {
}

uint32_t Hardlink::getTarget() {
    return targetINodeBlockID;
}

const std::string Hardlink::getLinkName() {
    return linkName;
}

} // SDI4FS

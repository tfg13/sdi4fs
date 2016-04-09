/*
 * File:   StreamUtils.inc
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on August 15, 2015, 4:37 PM
 */

#ifndef SDI4FS_STREAMUTILS_INC
#define	SDI4FS_STREAMUTILS_INC

#include <iostream>
#include <cstdint>

#include "StreamSelectorHeader.inc"

/*
 * Convenience functions
 */

inline void read8(STREAM &stream, uint8_t *data) {
    stream.read((char*) data, 1);
    if (stream.fail()) {
        std::cout << "error reading stream at " << stream.tellg() << std::endl;
    }
}

inline void read16(STREAM &stream, uint16_t *data) {
    stream.read((char*) data, 2);
    if (stream.fail()) {
        std::cout << "error reading stream at " << stream.tellg() << std::endl;
    }
}

inline void read32(STREAM &stream, uint32_t *data) {
    stream.read((char*) data, 4);
    if (stream.fail()) {
        std::cout << "error reading stream at " << stream.tellg() << std::endl;
    }
}

inline void read64(STREAM &stream, uint64_t *data) {
    stream.read((char*) data, 8);
    if (stream.fail()) {
        std::cout << "error reading stream at " << stream.tellg() << std::endl;
    }
}

inline void readN(STREAM &stream, void *data, std::size_t n) {
    stream.read((char*) data, n);
    if (stream.fail()) {
        std::cout << "error reading stream at " << stream.tellg() << std::endl;
    }
}

inline void write8(STREAM &stream, uint8_t data) {
    stream.write((char*) &data, 1);
    if (stream.fail()) {
        std::cout << "error writing to stream at " << stream.tellg() << std::endl;
    }
}

inline void write16(STREAM &stream, uint16_t data) {
    stream.write((char*) &data, 2);
    if (stream.fail()) {
        std::cout << "error writing to stream at " << stream.tellg() << std::endl;
    }
}

inline void write32(STREAM &stream, uint32_t data) {
    stream.write((char*) &data, 4);
    if (stream.fail()) {
        std::cout << "error writing to stream at " << stream.tellg() << std::endl;
    }
}

inline void write64(STREAM &stream, uint64_t data) {
    stream.write((char*) &data, 8);
    if (stream.fail()) {
        std::cout << "error writing to stream at " << stream.tellg() << std::endl;
    }
}

inline void writeN(STREAM &stream, void *data, std::size_t n) {
    stream.write((char*) data, n);
    if (stream.fail()) {
        std::cout << "error writing to stream at " << stream.tellg() << std::endl;
    }
}

#endif	// SDI4FS_STREAMUTILS_INC


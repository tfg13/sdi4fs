/*
 * File:   StreamSelectorHeader.inc
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on September 26, 2015, 6:00 PM
 */

/**
 * This file defines the header STREAM to match the stream implementation of the target platform
 */
#ifndef STREAM

#ifdef DEV_LINUX
#define STREAM std::iostream
#else
#include "../../../sdi4stream/include/fs_iostream_client.h"
#define STREAM SDI4FS::shared_iostream_client
#endif // DEV_LINUX

#endif //STREAM

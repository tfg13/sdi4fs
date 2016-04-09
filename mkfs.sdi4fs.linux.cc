/*
 * File:   mkfs.sdi4fs.cpp
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on August 18, 2015, 6:24 PM
 */

#include <cstdlib>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>

#include "Directory.h"
#include "DirectoryINode.h"

#include "Constants.inc"
#include "Formatter.inc"
#include "StreamUtils.inc"

using namespace std;

/*
 * Creates (formats) a sdi4fs filesystem.
 */
int main(int argc, char** argv) {

    if (argc != 2) {
        std::cout << "please specifiy exactly one target (file)" << std::endl;
        return 1;
    }

    fstream iofile(argv[1], ios::in | ios::out | ios::binary);

    if (!iofile) {
        // Print an error and exit
        std::cout << "error, cannot open " << argv[1] << std::endl;
        return 2;
    }

    createSDI4FS(iofile, 0);

    std::cout << "done." << std::endl;

}


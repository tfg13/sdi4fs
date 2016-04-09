/*
 * File:   linux_main.cpp
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on August 15, 2015, 3:03 PM
 */

#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>

#include "FS.h"

using namespace std;

/*
 * Main class for developing and debugging SDI4FS on linux.
 * Creates an instance of sdi4fs that uses a normal file as background "device".
 */
int main(int argc, char** argv) {

    fstream iofile("dev.dat", ios::in | ios::out | ios::binary);

    if (!iofile) {
        cerr << "Error, cannot open dev.dat" << endl;
        return 1;
    }

    // Open fs
    SDI4FS::FS fs(iofile);

    // try mkdir
    //    for (int i = 0; i < 125; ++i) {
    //        std::stringstream s;
    //        s << "/" << i;
    //        fs.mkdir(s.str());
    //    }
    //fs.touch("/bla/foo");
    std::list<std::string> ls;
    fs.ls("/", ls);
    for (std::string entry : ls) {
        cout << entry << endl;
    }
    ls.clear();
    fs.mkdir("/bla");
    fs.ls("/bla", ls);
    for (std::string entry : ls) {
        cout << entry << endl;
    }
    ls.clear();
    fs.mkdir("/bla/foo");
    fs.touch("/bla/foofile");
    fs.link("/bla/foofile2", "/bla/foofile");
    fs.ls("/bla", ls);
    for (std::string entry : ls) {
        cout << entry << endl;
    }
    ls.clear();
    fs.rmdir("/bla/foo");
    fs.rm("/bla/foofile");
    fs.ls("/bla", ls);
    for (std::string entry : ls) {
        cout << entry << endl;
    }
    //    fs.mkdir("/bla");
    //    fs.mkdir("/bla/foo");
    //    fs.touch("/bla/foobar");
    //    fs.rmdir("/bla/foo");
    //    fs.mkdir("/bla/foo");
    //fs.rmdir("/bla/foo");
    //fs.rmdir("/../././bla/./../bla/.");

    // umount
    fs.umount();

    // done
    iofile.close();
    return 0;
}


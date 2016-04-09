/*
 * File:   PathUtils.inc
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on August 29, 2015, 4:41 PM
 */

#ifndef SDI4FS_PATHUTILS_INC
#define	SDI4FS_PATHUTILS_INC

#include <string>
#include <sstream>
#include <list>

/*
 * Convenience functions
 */

/**
 * Splits the given string using the given seperator.
 * Based on http://stackoverflow.com/a/7408245
 * @param tokens found tokens (result)
 * @param text the string to split
 * @param sep the seperator
 */
inline void split(std::list<std::string> &tokens, const std::string &text, char sep) {
    size_t start = 0, end = 0;
    while ((end = text.find(sep, start)) != std::string::npos) {
        tokens.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    tokens.push_back(text.substr(start));
}

/**
 * Extracts the last name from an absolute path.
 * @param text the absolute path
 * @return the last file/dir/link name
 */
inline std::string lastName(const std::string &text) {
    return text.substr(text.find_last_of('/') + 1);
}

/**
 * Resolves all "." and ".." hardlinks in a given absolute path.
 * @param path an absolute path, potentially containing "." and ".."
 * @return an absolute path without "." or ".."
 */
inline std::string normalizePath(const std::string path) {
    // sanity checks
    if (path.find_first_of("/") != 0) {
        // not an absolute path
        std::cout << "fs: error - normalizePath: \"" << path << "\" is not absolute" << std::endl;
        return path;
    }

    // beware of ugly iterator juggling

    std::list<std::string> dirs;
    split(dirs, path, '/');

    // remove "."
    for (auto iter = dirs.begin(); iter != dirs.end(); ++iter) {
        if (!(*iter).compare(".")) {
            auto previous = iter;
            --previous;
            dirs.erase(iter);
            // fix iterator
            iter = previous;
        }
    }
    // resolve ".."
    for (auto iter = dirs.begin(); iter != dirs.end(); ++iter) {
        if (!(*iter).compare("..")) {
            // ".." either has a preceding link (then delete both), or is the first element in the path (then it is treated like "." = deleted alone)
            auto previous = iter;
            --previous;
            // previous = begin means ".." is the first element in the path (ex: "/..")
            if (previous == dirs.begin()) {
                // treat ".." like "." - delete it
                dirs.erase(iter);
                // fix iterator
                iter = previous;
            } else {
                // there is a valid preceding link, delete it along with ".."
                dirs.erase(iter);
                // fix iterator
                iter = previous;
                --iter;

                dirs.erase(previous);
            }
        }
    }

    // re-assemble path (remove leading empty string first)
    dirs.pop_front();
    std::stringstream s;

    if (!dirs.empty()) {
        for (auto iter = dirs.begin(); iter != dirs.end(); ++iter) {
            s << "/" << *iter;
        }
    } else {
        s << "/";
    }


    return s.str();
}

#endif	// SDI4FS_PATHUTILS_INC


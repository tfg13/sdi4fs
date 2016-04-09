/*
 * File:   IDirectoryEntryListCreator.h
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on September 1, 2015, 5:54 PM
 */

#ifndef SDI4FS_IDIRECTORYENTRYLISTCREATOR_H
#define	SDI4FS_IDIRECTORYENTRYLISTCREATOR_H

#include "DirectoryEntryList.h"

namespace SDI4FS {

/**
 * Anonymously implemented callback for fs operations that may require the creation of one or more DirectoryEntryLists.
 */
class IDirectoryEntryListCreator {
public:
    /**
     * Allocate a new DirectoryEntryList.
     * Caller is responsible for cleaning up the created object.
     * @return pointer to new DirectoryEntryList or NULL
     */
    virtual DirectoryEntryList* alloc() = 0;

    /**
     * Frees a DirectoryEntryList.
     * Caller should drop all references to the given object, callee will clean it up.
     * @param block pointer to the DirectoryEntryList to free
     */
    virtual void dealloc(DirectoryEntryList* block) = 0;

    virtual ~IDirectoryEntryListCreator() {
        // this desctructor has a body, because otherwise, gcc (linker) emits the infamous "undefined reference to vtable" error.
    }
};

}

#endif	// IDIRECTORYENTRYLISTCREATOR_H


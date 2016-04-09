/*
 * File:   IDataBlockListCreator.h
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on September 16, 2015, 11:29 PM
 */

#ifndef SDI4FS_IDATABLOCKLISTCREATOR_H
#define	SDI4FS_IDATABLOCKLISTCREATOR_H

#include "DataBlockList.h"

namespace SDI4FS {

/**
 * Anonymously implemented callback for fs operations that may require the creation of one or more DataBlockLists.
 */
class IDataBlockListCreator {
public:
    /**
     * Allocate a new DataBlockList.
     * Caller is responsible for cleaning up the created object.
     * @return pointer to new DataBlockList or NULL
     */
    virtual DataBlockList* alloc() = 0;

    /**
     * Frees a DataBlockList.
     * Caller should drop all references to the given object, callee will clean it up.
     * @param block pointer to the DataBlockList to free
     */
    virtual void dealloc(DataBlockList* block) = 0;

    virtual ~IDataBlockListCreator() {
        // this desctructor has a body, because otherwise, gcc (linker) emits the infamous "undefined reference to vtable" error.
    }
};

}

#endif	// SDI4FS_IDATABLOCKLISTCREATOR_H


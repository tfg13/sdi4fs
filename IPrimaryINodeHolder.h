/*
 * File:   IPrimaryINodeHolder.h
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on September 17, 2015, 4:21 PM
 */

#ifndef SDI4FS_IPRIMARYINODEHOLDER_H
#define	SDI4FS_IPRIMARYINODEHOLDER_H

#include "INode.h"

namespace SDI4FS {

/**
 * Interface for fs internal objects that hold a primary INode.
 */
class IPrimaryINodeHolder {
public:
    /**
     * Returns the primary INode.
     * @return the primary INode
     */
    virtual INode& getPrimaryINode() = 0;

    virtual ~IPrimaryINodeHolder() {

    }
};

}

#endif	// SDI4FS_IPRIMARYINODEHOLDER_H


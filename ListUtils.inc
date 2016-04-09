/*
 * File:   ListUtils.inc
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on September 19, 2015, 2:04 PM
 */

#ifndef SDI4FS_LISTUTILS_INC
#define	SDI4FS_LISTUTILS_INC

#include <list>

/*
 * Convenience functions
 */

// yes, you could use a set for this, but the lists are very short, so a set is barely worth the effort
// also, I don't want to change the collection type for this

template <class T>
inline void addUnique(std::list<T> &baseList, const std::list<T> &additions) {
    for (const T &elem : additions) {
        // only add if not already present
        bool found = false;
        for (T &innerElem : baseList) {
            if (innerElem == elem) {
                found = true;
                break;
            }
        }
        if (!found) {
            baseList.push_back(elem);
        }
    }
}

#endif	// SDI4FS_LISTUTILS_INC


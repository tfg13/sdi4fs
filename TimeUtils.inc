/*
 * File:   TimeUtils.inc
 * Author: Tobias Fleig <tobifleig@gmail.com>
 *
 * Created on August 17, 2015, 6:19 PM
 */

#ifndef SDI4FS_TIMEUTILS_INC
#define	SDI4FS_TIMEUTILS_INC

#include <ctime>

#ifndef DEV_LINUX
namespace SDI4FS {
    // pseudo-time (linear increasing counter) for systems without rtc
    static uint32_t pseudoTime = 0;
}
#endif // DEV_LINUX

/*
 * Convenience functions
 */

inline uint32_t now() {
#ifdef DEV_LINUX
    return std::time(NULL);
#else
    return SDI4FS::pseudoTime++;
#endif // DEV_LINUX
}

#endif	// SDI4FS_TIMEUTILS_INC


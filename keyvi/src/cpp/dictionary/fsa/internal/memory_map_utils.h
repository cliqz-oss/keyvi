/* * keyvi - A key value store.
 *
 * Copyright 2015 Hendrik Muhs<hendrik.muhs@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * memory_map_utils.h
 *
 *  Created on: May 17, 2016
 *      Author: hendrik
 */

#ifndef KEYVI_MEMORY_MAP_UTILS_H_
#define KEYVI_MEMORY_MAP_UTILS_H_

#ifdef _Win32
// no windows yet, maybe PrefetchVirtualMemory, windows.h

#else
#include <sys/mman.h>
#endif

//#define ENABLE_TRACING
#include "dictionary/util/trace.h"

namespace keyvi {
namespace dictionary {
namespace fsa {
namespace internal {

class MemoryMapUtils final{
public:
   /**
    * Tell the kernel to prefetch memory at the given address
    *
    * @addr memory address
    * @length length
    *
    */
   static void PrefetchMmap(const char* addr, size_t length) {
#ifdef _Win32
     // just return
     return;
#else // not _Win32
     size_t page_size = sysconf(_SC_PAGESIZE);
     void* addr_aligned = (void*) (((size_t) addr / page_size) * page_size);
     if (madvise(addr_aligned, length, MADV_WILLNEED) != 0) {
       TRACE("madvise failed");
     }
     return;
#endif // not _Win32
   }
};

} /* namespace internal */
} /* namespace fsa */
} /* namespace dictionary */
} /* namespace keyvi */


#endif /* KEYVI_MEMORY_MAP_UTILS_H_ */

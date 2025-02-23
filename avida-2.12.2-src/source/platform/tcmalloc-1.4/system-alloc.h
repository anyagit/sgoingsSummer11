/*
 *  system-alloc.h
 *  Avida
 *
 *  Added by David on 10/14/09.
 *  Copyright 2009-2010 Michigan State University. All rights reserved.
 *
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; version 2
 *  of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

// Copyright (c) 2005, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// ---
// Author: Sanjay Ghemawat
//
// Routine that uses sbrk/mmap to allocate memory from the system.
// Useful for implementing malloc.

#ifndef TCMALLOC_SYSTEM_ALLOC_H_
#define TCMALLOC_SYSTEM_ALLOC_H_

#include "tcmalloc-platform.h"
#include "internal_logging.h"

// REQUIRES: "alignment" is a power of two or "0" to indicate default alignment
//
// Allocate and return "N" bytes of zeroed memory.
//
// If actual_bytes is NULL then the returned memory is exactly the
// requested size.  If actual bytes is non-NULL then the allocator
// may optionally return more bytes than asked for (i.e. return an
// entire "huge" page if a huge page allocator is in use).
//
// The returned pointer is a multiple of "alignment" if non-zero.
//
// Returns NULL when out of memory.
extern void* TCMalloc_SystemAlloc(size_t bytes, size_t *actual_bytes,
                                  size_t alignment = 0);

// This call is a hint to the operating system that the pages
// contained in the specified range of memory will not be used for a
// while, and can be released for use by other processes or the OS.
// Pages which are released in this way may be destroyed (zeroed) by
// the OS.  The benefit of this function is that it frees memory for
// use by the system, the cost is that the pages are faulted back into
// the address space next time they are touched, which can impact
// performance.  (Only pages fully covered by the memory region will
// be released, partial pages will not.)
extern void TCMalloc_SystemRelease(void* start, size_t length);

// Interface to a pluggable system allocator.
class SysAllocator {
 public:
  SysAllocator() 
    : usable_(true), 
      failed_(false) {
  };
  virtual ~SysAllocator() {};

  virtual void* Alloc(size_t size, size_t *actual_size, size_t alignment) = 0;

  // Populate the map with whatever properties the specified allocator finds
  // useful for debugging (such as number of bytes allocated and whether the
  // allocator has failed).  The callee is responsible for any necessary
  // locking (and avoiding deadlock).
  virtual void DumpStats(TCMalloc_Printer* printer) = 0;

  // So the allocator can be turned off at compile time
  bool usable_;

  // Did this allocator fail? If so, we don't need to retry more than twice.
  bool failed_;
};

// Register a new system allocator. The priority determines the order in
// which the allocators will be invoked. Allocators with numerically lower
// priority are tried first. To keep things simple, the priority of various
// allocators is known at compile time.
//
// Valid range of priorities: [0, kMaxDynamicAllocators)
//
// Please note that we can't use complex data structures and cause
// recursive calls to malloc within this function. So all data structures
// are statically allocated.
//
// Returns true on success. Does nothing on failure.
extern PERFTOOLS_DLL_DECL bool RegisterSystemAllocator(SysAllocator *allocator,
                                                       int priority);

// Number of SysAllocators known to call RegisterSystemAllocator
static const int kMaxDynamicAllocators = 2;

// Retrieve the current state of various system allocators.
extern PERFTOOLS_DLL_DECL void DumpSystemAllocatorStats(TCMalloc_Printer* printer);

#endif /* TCMALLOC_SYSTEM_ALLOC_H_ */

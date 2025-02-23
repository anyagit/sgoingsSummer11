/*
 *  common.h
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

// Copyright (c) 2008, Google Inc.
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
// Author: Sanjay Ghemawat <opensource@google.com>
//
// Common definitions for tcmalloc code.

#ifndef TCMALLOC_COMMON_H_
#define TCMALLOC_COMMON_H_

#include "tcmalloc-platform.h"
#include <stddef.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdarg.h>
#include "commandlineflags.h"
#include "internal_logging.h"

// Type that can hold a page number
typedef uintptr_t PageID;

// Type that can hold the length of a run of pages
typedef uintptr_t Length;

//-------------------------------------------------------------------
// Configuration
//-------------------------------------------------------------------

// Not all possible combinations of the following parameters make
// sense.  In particular, if kMaxSize increases, you may have to
// increase kNumClasses as well.
static const size_t kPageShift  = 12;
static const size_t kPageSize   = 1 << kPageShift;
static const size_t kMaxSize    = 8u * kPageSize;
static const size_t kAlignment  = 8;
static const size_t kNumClasses = 61;

// Maximum length we allow a per-thread free-list to have before we
// move objects from it into the corresponding central free-list.  We
// want this big to avoid locking the central free-list too often.  It
// should not hurt to make this list somewhat big because the
// scavenging code will shrink it down when its contents are not in use.
static const int kMaxDynamicFreeListLength = 8192;

static const Length kMaxValidPages = (~static_cast<Length>(0)) >> kPageShift;

namespace tcmalloc {

// Convert byte size into pages.  This won't overflow, but may return
// an unreasonably large value if bytes is huge enough.
inline Length pages(size_t bytes) {
  return (bytes >> kPageShift) +
      ((bytes & (kPageSize - 1)) > 0 ? 1 : 0);
}

// Size-class information + mapping
class SizeMap {
 private:
  // Number of objects to move between a per-thread list and a central
  // list in one shot.  We want this to be not too small so we can
  // amortize the lock overhead for accessing the central list.  Making
  // it too big may temporarily cause unnecessary memory wastage in the
  // per-thread free list until the scavenger cleans up the list.
  int num_objects_to_move_[kNumClasses];

  //-------------------------------------------------------------------
  // Mapping from size to size_class and vice versa
  //-------------------------------------------------------------------

  // Sizes <= 1024 have an alignment >= 8.  So for such sizes we have an
  // array indexed by ceil(size/8).  Sizes > 1024 have an alignment >= 128.
  // So for these larger sizes we have an array indexed by ceil(size/128).
  //
  // We flatten both logical arrays into one physical array and use
  // arithmetic to compute an appropriate index.  The constants used by
  // ClassIndex() were selected to make the flattening work.
  //
  // Examples:
  //   Size       Expression                      Index
  //   -------------------------------------------------------
  //   0          (0 + 7) / 8                     0
  //   1          (1 + 7) / 8                     1
  //   ...
  //   1024       (1024 + 7) / 8                  128
  //   1025       (1025 + 127 + (120<<7)) / 128   129
  //   ...
  //   32768      (32768 + 127 + (120<<7)) / 128  376
  static const int kMaxSmallSize = 1024;
  unsigned char class_array_[377];
  
  // Compute index of the class_array[] entry for a given size
  static inline int ClassIndex(int s) {
    ASSERT(s <= static_cast<int>(kMaxSize));
    const bool big = (s > kMaxSmallSize);
    const int add_amount = big ? (127 + (120<<7)) : 7;
    const int shift_amount = big ? 7 : 3;
    return (s + add_amount) >> shift_amount;
  }

  int NumMoveSize(size_t size);

  // Mapping from size class to max size storable in that class
  size_t class_to_size_[kNumClasses];

  // Mapping from size class to number of pages to allocate at a time
  size_t class_to_pages_[kNumClasses];

 public:
  // Constructor should do nothing since we rely on explicit Init()
  // call, which may or may not be called before the constructor runs.
  SizeMap() { }

  // Initialize the mapping arrays
  void Init();

  inline unsigned int SizeClass(int size) {
    return class_array_[ClassIndex(size)];
  }

  // Get the byte-size for a specified class
  inline size_t ByteSizeForClass(size_t cl) {
    return class_to_size_[cl];
  }

  // Mapping from size class to max size storable in that class
  inline size_t class_to_size(size_t cl) {
    return class_to_size_[cl];
  }

  // Mapping from size class to number of pages to allocate at a time
  inline size_t class_to_pages(size_t cl) {
    return class_to_pages_[cl];
  }

  // Number of objects to move between a per-thread list and a central
  // list in one shot.  We want this to be not too small so we can
  // amortize the lock overhead for accessing the central list.  Making
  // it too big may temporarily cause unnecessary memory wastage in the
  // per-thread free list until the scavenger cleans up the list.
  inline int num_objects_to_move(size_t cl) {
    return num_objects_to_move_[cl];
  }

  // Dump contents of the computed size map
  void Dump(TCMalloc_Printer* out);
};

// Allocates "bytes" worth of memory and returns it.  Increments
// metadata_system_bytes appropriately.  May return NULL if allocation
// fails.  Requires pageheap_lock is held.
void* MetaDataAlloc(size_t bytes);

// Returns the total number of bytes allocated from the system.
// Requires pageheap_lock is held.
uint64_t metadata_system_bytes();

}  // namespace tcmalloc

#endif  // TCMALLOC_COMMON_H_

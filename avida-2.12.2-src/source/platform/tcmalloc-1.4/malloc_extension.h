/*
 *  malloc_extension.h
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
// Author: Sanjay Ghemawat <opensource@google.com>
//
// Extra extensions exported by some malloc implementations.  These
// extensions are accessed through a virtual base class so an
// application can link against a malloc that does not implement these
// extensions, and it will get default versions that do nothing.
//
// NOTE FOR C USERS: If you wish to use this functionality from within
// a C program, see malloc_extension_c.h.

#ifndef BASE_MALLOC_EXTENSION_H_
#define BASE_MALLOC_EXTENSION_H_

#include <stddef.h>
#include <string>

// Annoying stuff for windows -- makes sure clients can import these functions
#ifndef PERFTOOLS_DLL_DECL
# ifdef _WIN32
#   define PERFTOOLS_DLL_DECL  __declspec(dllimport)
# else
#   define PERFTOOLS_DLL_DECL
# endif
#endif

static const int kMallocHistogramSize = 64;

// One day, we could support other types of writers (perhaps for C?)
typedef std::string MallocExtensionWriter;

// The default implementations of the following routines do nothing.
// All implementations should be thread-safe; the current one
// (TCMallocImplementation) is.
class PERFTOOLS_DLL_DECL MallocExtension {
 public:
  virtual ~MallocExtension();

  // Call this very early in the program execution -- say, in a global
  // constructor -- to set up parameters and state needed by all
  // instrumented malloc implemenatations.  One example: this routine
  // sets environemnt variables to tell STL to use libc's malloc()
  // instead of doing its own memory management.  This is safe to call
  // multiple times, as long as each time is before threads start up.
  static void Initialize();

  // See "verify_memory.h" to see what these routines do
  virtual bool VerifyAllMemory();
  virtual bool VerifyNewMemory(void* p);
  virtual bool VerifyArrayNewMemory(void* p);
  virtual bool VerifyMallocMemory(void* p);
  virtual bool MallocMemoryStats(int* blocks, size_t* total,
                                 int histogram[kMallocHistogramSize]);

  // Get a human readable description of the current state of the malloc
  // data structures.  The state is stored as a null-terminated string
  // in a prefix of "buffer[0,buffer_length-1]".
  // REQUIRES: buffer_length > 0.
  virtual void GetStats(char* buffer, int buffer_length);


  // -------------------------------------------------------------------
  // Control operations for getting and setting malloc implementation
  // specific parameters.  Some currently useful properties:
  //
  // generic
  // -------
  // "generic.current_allocated_bytes"
  //      Number of bytes currently allocated by application
  //      This property is not writable.
  //
  // "generic.heap_size"
  //      Number of bytes in the heap ==
  //            current_allocated_bytes +
  //            fragmentation +
  //            freed memory regions
  //      This property is not writable.
  //
  // tcmalloc
  // --------
  // "tcmalloc.max_total_thread_cache_bytes"
  //      Upper limit on total number of bytes stored across all
  //      per-thread caches.  Default: 16MB.
  //
  // "tcmalloc.current_total_thread_cache_bytes"
  //      Number of bytes used across all thread caches.
  //      This property is not writable.
  //
  // "tcmalloc.slack_bytes"
  //      Number of bytes allocated from system, but not currently
  //      in use by malloced objects.  I.e., bytes available for
  //      allocation without needing more bytes from system.
  //      This property is not writable.
  //
  // TODO: Add more properties as necessary
  // -------------------------------------------------------------------

  // Get the named "property"'s value.  Returns true if the property
  // is known.  Returns false if the property is not a valid property
  // name for the current malloc implementation.
  // REQUIRES: property != NULL; value != NULL
  virtual bool GetNumericProperty(const char* property, size_t* value);

  // Set the named "property"'s value.  Returns true if the property
  // is known and writable.  Returns false if the property is not a
  // valid property name for the current malloc implementation, or
  // is not writable.
  // REQUIRES: property != NULL
  virtual bool SetNumericProperty(const char* property, size_t value);

  // Mark the current thread as "idle".  This routine may optionally
  // be called by threads as a hint to the malloc implementation that
  // any thread-specific resources should be released.  Note: this may
  // be an expensive routine, so it should not be called too often.
  //
  // Also, if the code that calls this routine will go to sleep for
  // a while, it should take care to not allocate anything between
  // the call to this routine and the beginning of the sleep.
  //
  // Most malloc implementations ignore this routine.
  virtual void MarkThreadIdle();

  // Mark the current thread as "busy".  This routine should be
  // called after MarkThreadIdle() if the thread will now do more
  // work.  If this method is not called, performance may suffer.
  //
  // Most malloc implementations ignore this routine.
  virtual void MarkThreadBusy();

  // Try to free memory back to the operating system for reuse.  Only
  // use this extension if the application has recently freed a lot of
  // memory, and does not anticipate using it again for a long time --
  // to get this memory back may require faulting pages back in by the
  // OS, and that may be slow.  (Currently only implemented in
  // tcmalloc.)
  virtual void ReleaseFreeMemory();

  // Sets the rate at which we release unused memory to the system.
  // Zero means we never release memory back to the system.  Increase
  // this flag to return memory faster; decrease it to return memory
  // slower.  Reasonable rates are in the range [0,10].  (Currently
  // only implemented in tcmalloc).
  virtual void SetMemoryReleaseRate(double rate);

  // Gets the release rate.  Returns a value < 0 if unknown.
  virtual double GetMemoryReleaseRate();

  // Returns the estimated number of bytes that will be allocated for
  // a request of "size" bytes.  This is an estimate: an allocation of
  // SIZE bytes may reserve more bytes, but will never reserve less.
  // (Currently only implemented in tcmalloc, other implementations
  // always return SIZE.)
  virtual size_t GetEstimatedAllocatedSize(size_t size);

  // Returns the actual number N of bytes reserved by tcmalloc for the
  // pointer p.  The client is allowed to use the range of bytes
  // [p, p+N) in any way it wishes (i.e. N is the "usable size" of this
  // allocation).  This number may be equal to or greater than the number
  // of bytes requested when p was allocated.
  // p must have been allocated by this malloc implementation,
  // must not be an interior pointer -- that is, must be exactly
  // the pointer returned to by malloc() et al., not some offset
  // from that -- and should not have been freed yet.  p may be NULL.
  // (Currently only implemented in tcmalloc; other implementations
  // will return 0.)
  virtual size_t GetAllocatedSize(void* p);

  // The current malloc implementation.  Always non-NULL.
  static MallocExtension* instance();

  // Change the malloc implementation.  Typically called by the
  // malloc implementation during initialization.
  static void Register(MallocExtension* implementation);
};

#endif  // BASE_MALLOC_EXTENSION_H_

// Copyright 2017, VIXL authors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of ARM Limited nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdexcept>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h> // For VirtualAlloc/VirtualProtect
#else
    extern "C" {
    #include <sys/mman.h>
    }
#endif

#include "code-buffer-vixl.h"
#include "utils-vixl.h"

namespace vixl {


CodeBuffer::CodeBuffer(size_t capacity)
    : buffer_(NULL),
      managed_(true),
      cursor_(NULL),
      dirty_(false),
      capacity_(capacity) {
  if (capacity_ == 0) {
    return;
  }
#ifdef VIXL_CODE_BUFFER_MALLOC
  buffer_ = reinterpret_cast<byte*>(malloc(capacity_));
#elif defined(VIXL_CODE_BUFFER_MMAP) // Use mmap on Linux/BSD/MacOS, VirtualProtect on Windows
    #ifdef _WIN32
        buffer = reinterpret_cast<byte*>(VirtualAlloc(nullptr, capacity, MEM_COMMIT, PAGE_READWRITE));
    #else
        buffer_ = reinterpret_cast<byte*>(mmap(NULL,
                                            capacity,
                                            PROT_READ | PROT_WRITE,
                                            MAP_PRIVATE | MAP_ANONYMOUS,
                                            -1,
                                            0));
    #endif
#else
#error Unknown code buffer allocator.
#endif
  VIXL_CHECK(buffer_ != NULL);
  // Aarch64 instructions must be word aligned, we assert the default allocator
  // always returns word align memory.
  VIXL_ASSERT(IsWordAligned(buffer_));

  cursor_ = buffer_;
}


CodeBuffer::CodeBuffer(byte* buffer, size_t capacity)
    : buffer_(reinterpret_cast<byte*>(buffer)),
      managed_(false),
      cursor_(reinterpret_cast<byte*>(buffer)),
      dirty_(false),
      capacity_(capacity) {
  VIXL_ASSERT(buffer_ != NULL);
}


CodeBuffer::~CodeBuffer() VIXL_NEGATIVE_TESTING_ALLOW_EXCEPTION {
  VIXL_ASSERT(!IsDirty());
  if (managed_) {
#ifdef VIXL_CODE_BUFFER_MALLOC
    free(buffer_);
#elif defined(VIXL_CODE_BUFFER_MMAP)
    #ifdef _WIN32
        VirtualFree(buffer_, capacity_, MEM_RELEASE);
    #else
        munmap(buffer_, capacity_);
    #endif
#else
#error Unknown code buffer allocator.
#endif
  }
}


void CodeBuffer::SetExecutable() {
#ifdef VIXL_CODE_BUFFER_MMAP
    #ifdef _WIN32
        DWORD oldProtect; // Unused, but VirtualProtect wants it anyways
        const auto ret = VirtualProtect(buffer_, capacity_, PAGE_EXECUTE_READ, &oldProtect);
        VIXL_CHECK(ret != 0);
    #else
        int ret = mprotect(buffer_, capacity_, PROT_READ | PROT_EXEC);
        VIXL_CHECK(ret == 0);
    #endif
#else
  // This requires page-aligned memory blocks, which we can only guarantee with
  // mmap.
  VIXL_UNIMPLEMENTED();
#endif
}


void CodeBuffer::SetWritable() {
#ifdef VIXL_CODE_BUFFER_MMAP
    #ifdef _WIN32
        DWORD oldProtect; // Unused, but VirtualProtect wants it anyways
        const auto ret = VirtualProtect(buffer_, capacity_, PAGE_READWRITE, &oldProtect);
        VIXL_CHECK(ret != 0);
    #else
        int ret = mprotect(buffer_, capacity_, PROT_READ | PROT_WRITE);
        VIXL_CHECK(ret == 0);
    #endif
#else
  // This requires page-aligned memory blocks, which we can only guarantee with
  // mmap.
  VIXL_UNIMPLEMENTED();
#endif
}


void CodeBuffer::EmitString(const char* string) {
  VIXL_ASSERT(HasSpaceFor(strlen(string) + 1));
  VIXL_UNIMPLEMENTED(); // Old version used stpcpy. Reimplement in a portable manner if necessary.
}


void CodeBuffer::EmitData(const void* data, size_t size) {
  VIXL_ASSERT(HasSpaceFor(size));
  dirty_ = true;
  memcpy(cursor_, data, size);
  cursor_ = cursor_ + size;
}


void CodeBuffer::UpdateData(size_t offset, const void* data, size_t size) {
  dirty_ = true;
  byte* dst = buffer_ + offset;
  VIXL_ASSERT(dst + size <= cursor_);
  memcpy(dst, data, size);
}


void CodeBuffer::Align() {
  byte* end = AlignUp(cursor_, 4);
  const size_t padding_size = end - cursor_;
  VIXL_ASSERT(padding_size <= 4);
  EmitZeroedBytes(static_cast<int>(padding_size));
}

void CodeBuffer::EmitZeroedBytes(int n) {
  EnsureSpaceFor(n);
  dirty_ = true;
  memset(cursor_, 0, n);
  cursor_ += n;
}

void CodeBuffer::Reset() {
#ifdef VIXL_DEBUG
  if (managed_) {
    // Fill with zeros (there is no useful value common to A32 and T32).
    memset(buffer_, 0, capacity_);
  }
#endif
  cursor_ = buffer_;
  SetClean();
}


void CodeBuffer::Grow(size_t new_capacity) {
    throw std::runtime_error("[Vixl] Grow function was called somehow");
}


}  // namespace vixl


// TODO make this multithreaded

#ifndef _SWAP_ALLOCATOR_H_
#define _SWAP_ALLOCATOR_H_

#include "push_allocator.h"

struct SwapAllocatorReference {
  void *memory;
  uint32_t buffer_idx;
};

struct SwapBufferHead {
  uint32_t ref_count;
  uint32_t bytes_allocated;
};

#ifndef SWAP_ALLOCATOR_BUFFER_COUNT
#define SWAP_ALLOCATOR_BUFFER_COUNT 2
#endif

struct SwapAllocator {
  uint8_t *memory;
  SwapBufferHead buffers[SWAP_ALLOCATOR_BUFFER_COUNT];
  uint32_t active_buffer;
  uint32_t buffer_size; // NOTE : Size of each individual buffer
};

static uint32_t calc_swap_allocator_memory_size(uint32_t buffer_size) { return buffer_size * SWAP_ALLOCATOR_BUFFER_COUNT; }

static bool is_initialized(SwapAllocator *allocator) {
  if (!allocator) return false;
  if (!allocator->memory) return false;
  if (!allocator->buffer_size) return false;
  return true;
}

static void clear(SwapAllocator *allocator) {
  auto mem = allocator->memory;
  auto size = allocator->buffer_size;
  *allocator = {};
  allocator->memory = mem;
  allocator->buffer_size = size;
}

static SwapAllocator new_swap_allocator(PushAllocator *allocator, uint32_t buffer_size) {
  // TODO choose a good alignment here
  uint8_t *mem = (uint8_t *) alloc_size(allocator, calc_swap_allocator_memory_size(buffer_size), 8);
  if (!mem) return {};

  SwapAllocator result = {};
  result.memory = mem;
  result.buffer_size = buffer_size;

  return result;
}

static SwapAllocatorReference alloc_size(SwapAllocator *allocator, uint32_t size, uint64_t alignment = 1) {
  assert(is_initialized(allocator));

  for (int i = 0; i < SWAP_ALLOCATOR_BUFFER_COUNT; i++) {
    uint32_t buffer_idx = (allocator->active_buffer + i) % SWAP_ALLOCATOR_BUFFER_COUNT;
    auto active_buffer = allocator->buffers + buffer_idx;
    auto active_memory = allocator->memory + allocator->buffer_size * buffer_idx; 

    uint32_t bytes_allocated = active_buffer->bytes_allocated;

    auto alignment_offset = get_alignment_offset(active_memory, bytes_allocated, alignment);

    if (bytes_allocated + size + alignment_offset > allocator->buffer_size) {
      continue;
    }

    // Allocation Success
    allocator->active_buffer = buffer_idx;
    active_buffer->bytes_allocated += size + alignment_offset;
    active_buffer->ref_count++;

    SwapAllocatorReference result;
    result.memory = active_memory + bytes_allocated + alignment_offset;
    result.buffer_idx = buffer_idx;
    assert(((uint64_t)result.memory) % alignment == 0);
    return result;
  }

  // Allocation Failed
  //assert(!"SwapAllocator out of memory.");
  return {};
}

static void free(SwapAllocator *allocator, SwapAllocatorReference ref) {
  assert(is_initialized(allocator));
  assert(ref.buffer_idx < SWAP_ALLOCATOR_BUFFER_COUNT);
  auto buf = allocator->buffers + ref.buffer_idx;
  buf->ref_count--;
  if (buf->ref_count == 0) {
    buf->bytes_allocated = 0;
  }
}

static void free(SwapAllocator *allocator, void *ptr) {
  assert(is_initialized(allocator));
  uint32_t buffer_idx = ((uint8_t *)ptr - allocator->memory) / allocator->buffer_size;
  free(allocator, {ptr, buffer_idx});
}

#endif


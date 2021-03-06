/**
 * memorypool.cc
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "memorypool.hh"

#define USE_MEMORY_POOL

MemoryPool::MemoryPool() {
#ifdef USE_MEMORY_POOL
	apr_initialize();
	apr_allocator_create(&alloc);
	apr_allocator_max_free_set(alloc, POOL_MAX_FREE_SIZE);
	apr_pool_create_ex(&pool, NULL, NULL, alloc);
	balloc = apr_bucket_alloc_create(pool);
#endif
}

MemoryPool::~MemoryPool() {
#ifdef USE_MEMORY_POOL
	apr_bucket_alloc_destroy(balloc);
	apr_pool_destroy(pool);
	apr_allocator_destroy(alloc);
	apr_terminate();
#endif
}

char* MemoryPool::poolMalloc(uint32_t length) {
#ifdef USE_MEMORY_POOL
	{
		std::lock_guard<std::mutex> lk(memoryPoolMutex);
		char* buf = (char *)apr_bucket_alloc((apr_size_t)length, balloc);
		memset (buf, 0, length);
		return buf;
	}
#else
	return (char*) calloc(length, 1);
#endif
}

void MemoryPool::poolFree(char* ptr) {
#ifdef USE_MEMORY_POOL
	{
		std::lock_guard<std::mutex> lk(memoryPoolMutex);
		apr_bucket_free(ptr);
	}
#else
	free(ptr);
#endif

}

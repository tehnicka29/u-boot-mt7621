/*
 * Based on Q3A 1.32b GPL release and Quake3e source modification
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#include "common.h"

#define M_ALLOC(s)     malloc(s)
#define M_FREE(p)      free(p)
#define M_REALLOC(p,s) realloc(p,s)
#define M_ALIGN(a,s)   memalign(a,s)
#define M_INIT(b,s)    m_init(b,s)

#ifndef PAD
#define PAD(base,align) (((base)+(align)-1)&~((align)-1))
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef enum {
	TAG_FREE = 0,
	TAG_USED
} memtag_t;

typedef struct memblock_s {
	struct memblock_s *next, *prev;
	size_t			size;
	memtag_t		tag;
} memblock_t;

typedef struct freeblock_s {
	struct freeblock_s *prev;
	struct freeblock_s *next;
} freeblock_t;

typedef struct memzone_s {
	memblock_t blocklist;
	freeblock_t	freelist;
	size_t size;
} memzone_t;

#define MINFRAGMENT	PAD(sizeof(memblock_t)+sizeof(freeblock_t),sizeof(void*))

static memzone_t *zone;

static void remove_free(memblock_t *block)
{
	freeblock_t *fb = (freeblock_t*)(block + 1);
	freeblock_t *prev;
	freeblock_t *next;

	prev = fb->prev;
	next = fb->next;

	prev->next = next;
	next->prev = prev;
}

static void insert_free(memblock_t *block)
{
	freeblock_t *fb = (freeblock_t*)(block + 1);
	freeblock_t *prev, *next;

	prev = &zone->freelist;
	next = prev->next;

	prev->next = fb;
	next->prev = fb;

	fb->prev = prev;
	fb->next = next;

	block->tag = TAG_FREE;
}

static void merge_block(memblock_t *curr, const memblock_t *next)
{
	curr->size += next->size;
	curr->next = next->next;
	curr->next->prev = curr;
}

static memblock_t *split_block(memblock_t *base, size_t base_size, size_t fragment_size)
{
	memblock_t *fragment = (memblock_t *)((unsigned char *)base + base_size);

	fragment->size = fragment_size;
	fragment->prev = base;
	fragment->next = base->next;
	fragment->next->prev = fragment;

	base->next = fragment;
	base->size = base_size;

	return fragment;
}

static memblock_t *search_free(size_t size)
{
	const freeblock_t *fb;
	memblock_t *block;

	fb = zone->freelist.next;

	for ( ;; ) {
		if (fb == &zone->freelist) {
			break;
		}
		block = (memblock_t*)((unsigned char*)fb - sizeof(*block));
		if (block->size >= size) {
			remove_free(block);
			return block;
		}
		fb = fb->next;
	}

	return NULL;
}

static memblock_t *search_free_aligned(size_t alignment, size_t size)
{
	const freeblock_t *fb;
	memblock_t *base;

	fb = zone->freelist.next;
	for ( ;; ) {
		size_t diff;
		if (fb == &zone->freelist) {
			break;
		}
		base = (memblock_t*)((unsigned char*)fb - sizeof(*base));
		diff = (size_t)fb & (alignment-1);
		if (diff == 0) {
			if (base->size >= size) {
				remove_free(base);
				return base;
			}
		} else if (base->size > size) {
			const size_t extra = base->size - size;
			const size_t split_base = (alignment - diff) + PAD(MINFRAGMENT, alignment);
			if (extra >= split_base) {
				memblock_t *fragment;
				remove_free(base);
				fragment = split_block(base, split_base, base->size - split_base);
				insert_free(base);
				return fragment;
			}
		}
		fb = fb->next;
	}

	return NULL;
}

void M_INIT(void *buf, size_t size)
{
	memblock_t	*block;

	zone = (memzone_t*) buf;

	memset(zone, 0x00, size);

	zone->blocklist.next = zone->blocklist.prev = block = (memblock_t *)(zone + 1);
	zone->blocklist.tag = TAG_USED;
	zone->blocklist.size = 0;

	block->prev = block->next = &zone->blocklist;
	block->size = size - sizeof(*zone);

	zone->size = block->size;

	zone->freelist.next = zone->freelist.prev = &zone->freelist;

	insert_free(block);
}

static int is_used_block(const memblock_t *block)
{
	if (block->tag == TAG_FREE) {
		return 0;
	}

	if (block->size == 0 || block->size > zone->size) {
		return 0;
	}

	return 1;
}

void M_FREE(void *ptr)
{
	memblock_t *block, *other;

	if (!ptr) {
		return;
	}

	block = ((memblock_t *)ptr) - 1;

	if (!is_used_block(block)) {
		return;
	}

	other = block->prev;
	if (other->tag == TAG_FREE) {
		remove_free(other);
		merge_block(other, block);
		block = other;
	}

	other = block->next;
	if (other->tag == TAG_FREE) {
		remove_free(other);
		merge_block(block, other);
	}

	insert_free(block);
}

void *M_ALLOC(size_t size)
{
	memblock_t *block;
	size_t extra;

	if (size < sizeof(freeblock_t)) {
		size = sizeof(freeblock_t);
	}

	size = PAD(size + sizeof(*block), sizeof(void*));

	block = search_free(size);
	if (block == NULL) {
		return NULL;
	}

	extra = block->size - size;
	if (extra >= MINFRAGMENT) {
		insert_free(split_block(block, size, extra));
	}

	block->tag = TAG_USED;

	return (void *)(block + 1);
}

void *M_REALLOC(void *ptr, size_t size)
{
	memblock_t *base;

	if (ptr == NULL) {
		return M_ALLOC(size);
	}

	if (size == 0) {
		M_FREE(ptr);
		return NULL;
	}
	base = ((memblock_t *)ptr) - 1;

	if (!is_used_block(base)) {
		return NULL;
	}

	if (size < sizeof(freeblock_t)) {
		size = sizeof(freeblock_t);
	}

	size = PAD(size + sizeof(*base), sizeof(void*));

	if (base->size >= size) {
		size_t extra = base->size - size;
		if (extra < MINFRAGMENT) {
			/* keep existing */
		} else {
			if (base->next->tag == TAG_FREE) {
				remove_free(base->next);
				extra += base->next->size;
				base->next = base->next->next;
			}
			insert_free(split_block(base, size, extra));
		}
		return ptr;
	} else {
		if (base->next->tag == TAG_FREE && (base->size + base->next->size) >= size) {
			remove_free(base->next);
			merge_block(base, base->next);
			if (base->size - size >= MINFRAGMENT) {
				insert_free(split_block(base, size, base->size - size));
			}
			return ptr;
		} else {
			void *ptr_new;
			ptr_new = M_ALLOC(size);
			if (ptr_new) {
				memcpy(ptr_new, ptr, size);
				M_FREE(ptr);
				return ptr_new;
			} else {
				return NULL;
			}
		}
	}
}

void *M_ALIGN(size_t alignment, size_t size)
{
	memblock_t *block;
	size_t extra;

	if (size < sizeof(freeblock_t)) {
		size = sizeof(freeblock_t);
	}

	size = PAD(size + sizeof(*block), sizeof(void*));

	block = search_free_aligned(alignment, size);

	if (block == NULL) {
		return NULL;
	}

	extra = block->size - size;
	if (extra >= MINFRAGMENT) {
		insert_free(split_block(block, size, extra));
	}

	block->tag = TAG_USED;
	return (void *)(block + 1);
}

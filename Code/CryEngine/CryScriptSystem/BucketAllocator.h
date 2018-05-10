// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   BucketAllocator.h
//  Version:     v1.00
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __BucketAllocator_h__
#define __BucketAllocator_h__
#pragma once

#define BUCKETQUANT 512

// Linux
#ifdef PTRBITS
	#undef PTRBITS
#endif

class PageBucketAllocatorForLua
{
	/*
	   Wouter,
	   Generic allocator that combines bucket allocation with reference counted 1 size object pages.
	   manages to perform well along each axis:
	   - very fast for small objects: only a few instructions in the general case for alloc/dealloc,
	   up to several orders of magnitude faster than traditional best fit allocators
	   - low per object memory overhead: 0 bytes overhead on small objects, small overhead for
	   pages that are partially in use (still significantly lower than other allocators).
	   - almost no fragmentation, reuse of pages is close to optimal
	   - very good cache locality (page aligned, same size objects)
	 */
	enum { EPAGESIZE = 4096 };
	enum { PAGEMASK = (~(EPAGESIZE - 1)) };
	//enum { PAGESATONCE = 64 };
	enum { PAGESATONCE = 32 };
	enum { PAGEBLOCKSIZE = EPAGESIZE * PAGESATONCE };
	enum { PTRSIZE = sizeof(char*) };
	enum { MAXBUCKETS = BUCKETQUANT / 4 + 1 }; // meaning up to size 512 on 32bit pointer systems
	enum { MAXREUSESIZE = MAXBUCKETS * PTRSIZE - PTRSIZE };
	enum { PTRBITS = PTRSIZE == 2 ? 1 : PTRSIZE == 4 ? 2 : 3 };

	ILINE int  bucket(int s)  { return (s + PTRSIZE - 1) >> PTRBITS; };
	ILINE int* ppage(void* p) { return (int*)(((INT_PTR)p) & PAGEMASK); };

	void*  reuse[MAXBUCKETS];
	void** pages;
	size_t nAllocatedSize;

	void put_in_buckets(char* start, char* end, int bsize)
	{
		int size = bsize * PTRSIZE;
		for (end -= size; start <= end; start += size)
		{
			*((void**)start) = reuse[bsize];
			reuse[bsize] = start;
		}
		;
	};

	void new_page_blocks()
	{
		char* b = (char*)malloc(PAGEBLOCKSIZE);  // if we could get page aligned memory here, that would be even better
		char* first = ((char*)ppage(b)) + EPAGESIZE;
		for (int i = 0; i < PAGESATONCE - 1; i++)
		{
			void** p = (void**)(first + i * EPAGESIZE);
			*p = pages;
			pages = p;
		}
		;
	};

	void* new_page(unsigned int bsize)
	{
		if (!pages) new_page_blocks();
		void** page = pages;
		pages = (void**)*pages;
		*page = 0;
		put_in_buckets((char*)(page + 1), ((char*)page) + EPAGESIZE, bsize);
		return alloc(bsize * PTRSIZE);
	};

	void free_page(int* page, int bsize) // worst case if very large amounts of objects get deallocated in random order from when they were allocated
	{
		for (void** r = &reuse[bsize]; *r; )
		{
			if (page == ppage(*r)) *r = *((void**)*r);
			else r = (void**)*r;
		}
		;
		void** p = (void**)page;
		*p = pages;
		pages = p;
	};

public:

	PageBucketAllocatorForLua()
	{
		nAllocatedSize = 0;
		pages = NULL;
		for (int i = 0; i < MAXBUCKETS; i++) reuse[i] = NULL;
	};

	void* alloc(size_t size)
	{
		nAllocatedSize += size;
		if (size == 0)
			return 0;

		if (size > MAXREUSESIZE)
		{
			return malloc(size);
		}
		size = bucket(size);
		void** r = (void**)reuse[size];
		if (!r)
		{
			return new_page(size);
		}
		reuse[size] = *r;
		int* page = ppage(r);
		(*page)++;
		return (void*)r;
	};

	void dealloc(void* p, size_t size)
	{
		nAllocatedSize -= size;
		if (size > MAXREUSESIZE)
		{
			free(p);
		}
		else
		{
			size = bucket(size);
			*((void**)p) = reuse[size];
			reuse[size] = p;
			int* page = ppage(p);
			if (!--(*page)) free_page(page, size);
		};
	};

	void* re_alloc(void* ptr, size_t osize, size_t nsize)
	{
		if (NULL == ptr)
		{
			return alloc(nsize);
		}
		else if (osize > MAXREUSESIZE && nsize > MAXREUSESIZE)
		{
			nAllocatedSize += nsize;
			nAllocatedSize -= osize;
			return realloc(ptr, nsize);
		}
		else
		{
			void* nptr = 0;
			if (nsize)
			{
				nptr = (char*)alloc(nsize);// + g_nPrecaution;
				memcpy(nptr, ptr, nsize > osize ? osize : nsize);
			}
			dealloc(ptr, osize);
			return nptr;
		}
	}

	size_t get_alloc_size() const { return nAllocatedSize; }

	void   stats()
	{
		int totalwaste = 0;
		for (int i = 0; i < MAXBUCKETS; i++)
		{
			int n = 0;
			for (void** r = (void**)reuse[i]; r; r = (void**)*r) n++;
			if (n)
			{
				int waste = i * 4 * n / 1024;
				totalwaste += waste;
				CryLogAlways("bucket %d -> %d (%d k)\n", i * 4, n, waste);
			}
			;
		}
		;
		CryLogAlways("totalwaste %d k\n", totalwaste);
	};
};

#endif //__BucketAllocator_h__

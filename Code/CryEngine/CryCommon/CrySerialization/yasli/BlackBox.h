// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once 
#include <stdlib.h> // for malloc and free

#include <CrySerialization/yasli/Config.h>

namespace yasli
{

// Black box is used to store opaque data blobs in a format internal to
// specific Archive. For example it can be used to store sections of the JSON
// or binary archive.
//
// This is useful for the Editor to store portions of files with unfamiliar
// structure.
//
// We store deallocation function here so we can safely pass the blob
// across DLLs with different memory allocators.
struct BlackBox
{
	const char* format;
	void* data;
	size_t size;
	int xmlVersion;
	typedef void* (*Allocator)(size_t, const void*);
	Allocator allocator;

	BlackBox()
	: format("")
	, data(0)
	, size(0)
	, allocator(0)
	, xmlVersion(0)
	{
	}

	BlackBox(const BlackBox& rhs)
	: format("")
	, data(0)
	, size(0)
	, allocator(0)
	, xmlVersion(rhs.xmlVersion)
	{
		*this = rhs;
	}

	void set(const char* format, const void* data, size_t size, Allocator allocator = &defaultAllocator)
	{
		YASLI_ASSERT(data != this->data);
		if(data != this->data)
		{
			release();
			this->format = format;
			if (data && size) 
			{
				this->data = allocator(size, data);
				this->size = size;
				this->allocator = allocator;
			}
		}
	}

	template<typename Type>
	void set(const char* format, const Type& data)
	{
		this->set(format, &data, sizeof(Type), &templateAllocator<Type>);
	}

	void release()
	{
		if (data && allocator) {
			allocator(0, data);
		}
		format = 0;
		data = 0;
		size = 0;
		allocator = 0;
		xmlVersion = 0;
	}

	BlackBox& operator=(const BlackBox& rhs)
	{
		set(rhs.format, rhs.data, rhs.size, rhs.allocator);
		xmlVersion = rhs.xmlVersion;
		return *this;
	}

	~BlackBox()
	{
		release();
	}

	static void* defaultAllocator(size_t size, const void* ptr)
	{
		if (size) {
			return memcpy(malloc(size), ptr, size);
		}
		else {
			free(const_cast<void*>(ptr));
			return 0;
		}
	}

	template<typename Type>
	static void* templateAllocator(size_t size, const void* ptr)
	{
		if (size) {
			return new Type(*static_cast<const Type*>(ptr));
		}
		else {
			delete static_cast<const Type*>(ptr);
			return 0;
		}
	}
};


}

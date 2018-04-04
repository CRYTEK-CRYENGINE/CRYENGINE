/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once

#include <cstddef>
#include <CrySerialization/yasli/Config.h>
#include "Pointers.h"

namespace yasli{

	class MemoryWriter : public RefCounter {
public:
	YASLI_INLINE MemoryWriter(std::size_t size = 128, bool reallocate = true);
	YASLI_INLINE ~MemoryWriter();

	const char* c_str() { return memory_; };
	const wchar_t* w_str() { return (wchar_t*)memory_; };
	char* buffer() { return memory_; }
	const char* buffer() const { return memory_; }
	std::size_t size() const{ return size_; }
	void clear() { position_ = memory_; }

	// String interface (after this calls '\0' is always written)
	YASLI_INLINE MemoryWriter& operator<<(i8 value);
	YASLI_INLINE MemoryWriter& operator<<(u8 value);
	YASLI_INLINE MemoryWriter& operator<<(i32 value);
	YASLI_INLINE MemoryWriter& operator<<(u32 value);
	YASLI_INLINE MemoryWriter& operator<<(i64 value);
	YASLI_INLINE MemoryWriter& operator<<(u64 value);
	YASLI_INLINE MemoryWriter& operator<<(float value) { return (*this) << double(value); }
	YASLI_INLINE MemoryWriter& operator<<(double value);
	YASLI_INLINE MemoryWriter& operator<<(char value);
	YASLI_INLINE MemoryWriter& operator<<(const char* value);
	YASLI_INLINE MemoryWriter& operator<<(const wchar_t* value);
	YASLI_INLINE void appendAsString(double, bool allowTrailingPoint);

	// Binary interface (does not writes trailing '\0')
	template<class T>
	void write(const T& value){
		write(&value, sizeof(value));
	}
	YASLI_INLINE void write(char c);
	YASLI_INLINE void write(const char* str);
	YASLI_INLINE bool write(const void* data, std::size_t size);

	std::size_t position() const{ return position_ - memory_; }
	YASLI_INLINE void setPosition(std::size_t pos);

	MemoryWriter& setDigits(int digits) { digits_ = (unsigned char)digits; return *this; }

private:
	YASLI_INLINE void alloc(std::size_t initialSize);
	YASLI_INLINE void reallocate(std::size_t newSize);

	std::size_t size_;
	char* position_;
	char* memory_;
	bool reallocate_;
	unsigned char digits_;
};

}

#if YASLI_INLINE_IMPLEMENTATION
#include "MemoryWriterImpl.h"
#endif
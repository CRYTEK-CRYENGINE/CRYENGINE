/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once
#include <CrySerialization/yasli/Assert.h>
#include <CrySerialization/yasli/Config.h>

#include <stdlib.h>
#include <cstring>
#include <math.h>
#ifdef _MSC_VER
# include <float.h>
#else
# include <wchar.h>
#endif

#include "MemoryWriter.h"

namespace yasli{

MemoryWriter::MemoryWriter(size_t size, bool reallocate)
: size_(size)
, reallocate_(reallocate)
, digits_(5)
{
    alloc(size);
}

MemoryWriter::~MemoryWriter()
{
    position_ = 0;
    free(memory_);
}

void MemoryWriter::alloc(size_t initialSize)
{
    memory_ = (char*)malloc(initialSize + 1);
    position_ = memory_;
}

void MemoryWriter::reallocate(size_t newSize)
{
    YASLI_ASSERT(newSize > size_);
    size_t pos = position();
    // Supressing the warning as we generally don't handle malloc errors.
    // cppcheck-suppress memleakOnRealloc
    memory_ = (char*)::realloc(memory_, newSize + 1);
    YASLI_ASSERT(memory_ != 0);
    position_ = memory_ + pos;
    size_ = newSize;
}

MemoryWriter& MemoryWriter::operator<<(i32 value)
{
    char buffer[12];
    sprintf(buffer, "%i", value);
    return operator<<((const char*)buffer);
}

MemoryWriter& MemoryWriter::operator<<(u32 value)
{
    char buffer[12];
    sprintf(buffer, "%u", value);
    return operator<<((const char*)buffer);
}

MemoryWriter& MemoryWriter::operator<<(i64 value)
{
    char buffer[24];
#ifdef _MSC_VER
    sprintf(buffer, "%I64i", value);
#else
    sprintf(buffer, "%lli", (long long)value);
#endif
    return operator<<((const char*)buffer);
}

MemoryWriter& MemoryWriter::operator<<(u64 value)
{
    char buffer[24];
#ifdef _MSC_VER
    sprintf(buffer, "%I64u", value);
#else
    sprintf(buffer, "%llu", (unsigned long long)value);
#endif
    return operator<<((const char*)buffer);
}

MemoryWriter& MemoryWriter::operator<<(char value)
{
    char buffer[12];
    sprintf(buffer, "%i", int(value));
    return operator<<((const char*)buffer);
}

MemoryWriter& MemoryWriter::operator<<(u8 value)
{
    char buffer[12];
    sprintf(buffer, "%i", int(value));
    return operator<<((const char*)buffer);
}

MemoryWriter& MemoryWriter::operator<<(i8 value)
{
    char buffer[12];
    sprintf(buffer, "%i", int(value));
    return operator<<((const char*)buffer);
}

inline void cutTrailingZeros(const char* str)
{
	for(char* p = (char*)str + strlen(str) - 1; p >= str; --p)
		if(*p == '0')
			*p = 0;
		else
			return;
}

MemoryWriter& MemoryWriter::operator<<(double value)
{
	appendAsString(value, false);
	return *this;
}

void MemoryWriter::appendAsString(double value, bool allowTrailingPoint)
{
#if YASLI_NO_FCVT
	char buf[64] = { 0 };
	sprintf(buf, "%f", value);
	operator<<(buf);
#else

	int point = 0;
	int sign = 0;

#ifdef _MSC_VER
	char buf[_CVTBUFSIZE];
	_fcvt_s(buf, value, digits_, &point, &sign);
#else
    const char* buf = fcvt(value, digits_, &point, &sign);
#endif

    if(sign != 0)
        write("-");
    if(point <= 0){
		cutTrailingZeros(buf);
		if (strlen(buf)){
			write("0.");
			while (point < 0){
				write("0");
				++point;
			}
			write(buf);
		}
		else
			write(allowTrailingPoint ? "0" : "0.0");
		*position_ = '\0';
    }
    else{
        write(buf, point);
        write(".");
		if (allowTrailingPoint)
			cutTrailingZeros(buf + point);
		else if (buf[point] != '\0')
			cutTrailingZeros(buf + point + 1);
		operator<<(buf + point);
    }
#endif
}

MemoryWriter& MemoryWriter::operator<<(const char* value)
{
    write((void*)value, strlen(value));
    YASLI_ASSERT(position() < size());
    *position_ = '\0';
    return *this;
}

MemoryWriter& MemoryWriter::operator<<(const wchar_t* value)
{
#if defined(ANDROID_NDK) || defined(NACL)
	return *this;
#else
    write((void*)value, wcslen(value) * sizeof(wchar_t));
    YASLI_ASSERT(position() < size());
    *position_ = '\0';
    return *this;
#endif
}

void MemoryWriter::setPosition(size_t pos)
{
    YASLI_ASSERT(pos < size_);
    YASLI_ASSERT(memory_ + pos <= position_);
    position_ = memory_ + pos;
}

void MemoryWriter::write(const char* value)
{
    write((void*)value, strlen(value));
}

bool MemoryWriter::write(const void* data, size_t size)
{
    YASLI_ASSERT(memory_ <= position_);
    YASLI_ASSERT(position() < this->size());
    if(size_ - position() > size){
        memcpy(position_, data, size);
        position_ += size;
    }
    else{
        if(!reallocate_)
            return false;

        reallocate(size_ * 2);
        write(data, size);
    }
    YASLI_ASSERT(position() < this->size());
    return true;
}

void MemoryWriter::write(char c)
{
    if(size_ - position() > 1){
        *(char*)(position_) = c;
        ++position_;
    }
    else{
		YASLI_ESCAPE(reallocate_, return);
        reallocate(size_ * 2);
        write(c);
    }
    YASLI_ASSERT(position() < this->size());
}

}

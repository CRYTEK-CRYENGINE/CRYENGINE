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
#include "MemoryReader.h"
#include <cstdlib>

namespace yasli{

MemoryReader::MemoryReader()
: size_(0)
, position_(0)
, memory_(0)
, ownedMemory_(false)
{
}

MemoryReader::MemoryReader(const char* fileName)
{
    FILE* file = fopen(fileName, "rb");
    YASLI_ASSERT(file != 0);
    if(file){
        fseek(file, 0, SEEK_END);
        std::size_t len = ftell(file);
        fseek(file, 0, SEEK_SET);
        memory_ = new char[len];
        std::size_t count = fread((void*)memory_, 1, len, file);
        YASLI_ASSERT(count == len);
        ownedMemory_ = true;
        position_ = memory_;
        size_ = len;
    }
}

MemoryReader::MemoryReader(const void* memory, std::size_t size, bool ownAndFree)
: size_(size)
, position_((const char*)(memory))
, memory_((const char*)(memory))
, ownedMemory_(ownAndFree)
{

}

MemoryReader::~MemoryReader()
{
    if(ownedMemory_){
        delete[] memory_;
        memory_ = 0;
        size_ = 0;
    }
}

void MemoryReader::setPosition(const char* position)
{
    position_ = position;
}

void MemoryReader::read(void* data, std::size_t size)
{
    YASLI_ASSERT(memory_ && position_);
    YASLI_ASSERT(position_ - memory_ + size <= size_);
    ::memcpy(data, position_, size);
    position_ += size;
}

bool MemoryReader::checkedRead(void* data, std::size_t size)
{
    if(!memory_ || !position_)
        return false;
    if(position_ - memory_ + size > size_)
        return false;

    ::memcpy(data, position_, size);
    position_ += size;
    return true;
}

bool MemoryReader::checkedSkip(std::size_t size)
{
    if(!memory_ || !position_)
        return false;
    if(position_ - memory_ + size > size_)
        return false;

    position_ += size;
    return true;
}

}

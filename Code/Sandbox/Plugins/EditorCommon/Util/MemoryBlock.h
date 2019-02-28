// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  Created:     10/10/2002 by Timur.
//  Description: Memory block helper used with ZLib
//
////////////////////////////////////////////////////////////////////////////
#include "EditorCommonAPI.h"
#include <CryCore/smartptr.h>

class EDITOR_COMMON_API CMemoryBlock : public _i_reference_target_t
{
public:
	CMemoryBlock();
	CMemoryBlock(const CMemoryBlock& mem);
	~CMemoryBlock();

	CMemoryBlock& operator=(const CMemoryBlock& mem);

	//! Allocate or reallocate memory for this block.
	//! @param size Amount of memory in bytes to allocate.
	//! @return true if the allocation succeeded.
	bool Allocate(int size, int uncompressedSize = 0);

	//! Frees memory allocated in this block.
	void Free();

	//! Attach memory buffer to this block.
	void Attach(void* buffer, int size, int uncompressedSize = 0);
	//! Detach memory buffer that was previously attached.
	void Detach();

	//! Returns amount of allocated memory in this block.
	int GetSize() const { return m_size; }

	//! Returns amount of allocated memory in this block.
	int   GetUncompressedSize() const { return m_uncompressedSize; }

	void* GetBuffer() const           { return m_buffer; };

	//! Copy memory range to memory block.
	void Copy(void* src, int size);

	//! Compress this memory block to specified memory block.
	//! @param toBlock target memory block where compressed result will be stored.
	void Compress(CMemoryBlock& toBlock) const;

	//! Uncompress this memory block to specified memory block.
	//! @param toBlock target memory block where compressed result will be stored.
	void Uncompress(CMemoryBlock& toBlock) const;

	//! Serialize memory block to archive.
	void Serialize(CArchive& ar);

	//! Is MemoryBlock is empty.
	bool IsEmpty() const { return m_buffer == 0; }

private:
	void* m_buffer;
	int   m_size;
	//! If not 0, memory block is compressed.
	int   m_uncompressedSize;
	//! True if memory block owns its memory.
	bool  m_owns;
};


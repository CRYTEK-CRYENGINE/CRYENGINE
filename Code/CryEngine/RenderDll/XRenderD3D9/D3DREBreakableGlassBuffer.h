// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _CRE_BREAKABLE_GLASS_BUFFER_
#define _CRE_BREAKABLE_GLASS_BUFFER_
#pragma once

#include <CryRenderer/RenderElements/CREBreakableGlassConfig.h>

//==================================================================================================
// Name: CREBreakableGlassBuffer
// Desc: Breakable glass geometry buffer management
// Author: Chris Bunner
//==================================================================================================
class CREBreakableGlassBuffer
{
public:
	CREBreakableGlassBuffer();
	~CREBreakableGlassBuffer();

	// Geometry buffer types
	enum EBufferType
	{
		EBufferType_Plane = 0,
		EBufferType_Crack,
		EBufferType_Frag,
		EBufferType_Num
	};

	// Singleton access from Render Thread
	static CREBreakableGlassBuffer& RT_Instance();
	static void                     RT_ReleaseInstance();

	// Buffer allocation
	uint32 RT_AcquireBuffer(const bool isFrag);
	bool   RT_IsBufferValid(const uint32 id, const EBufferType buffType) const;
	bool   RT_ClearBuffer(const uint32 id, const EBufferType buffType);

	// Buffer updates
	bool RT_UpdateVertexBuffer(const uint32 id, const EBufferType buffType, SVF_P3F_C4B_T2F* pVertData, const uint32 vertCount);
	bool RT_UpdateIndexBuffer(const uint32 id, const EBufferType buffType, uint16* pIndData, const uint32 indCount);
	bool RT_UpdateTangentBuffer(const uint32 id, const EBufferType buffType, SPipTangents* pTanData, const uint32 tanCount);

	// Buffer drawing
	bool RT_DrawBuffer(const uint32 id, const EBufferType buffType);

	// Buffer state
	static const uint32 NoBuffer = 0;
	static const buffer_handle_t InvalidStreamHdl = ~0u;
	static const uint32 NumBufferSlots = GLASSCFG_MAX_NUM_ACTIVE_GLASS;

private:
	// Internal allocation
	void InitialiseBuffers();
	void ReleaseBuffers();

	// Internal drawing
	void DrawBuffer(const uint32 cyclicId, const EBufferType buffType, const uint32 indCount);

	// Common buffer state/data
	struct SBuffer
	{
		SBuffer()
			: verticesHdl(InvalidStreamHdl)
			, indicesHdl(InvalidStreamHdl)
			, tangentStreamHdl(InvalidStreamHdl)
			, lastIndCount(0)
		{
		}

		// Geometry buffers
		buffer_handle_t verticesHdl;
		buffer_handle_t indicesHdl;
		buffer_handle_t tangentStreamHdl;

		// State
		uint32 lastIndCount;
	};

	// Buffer data
	ILINE bool IsVBufferValid(const SBuffer& buffer) const
	{
		return buffer.verticesHdl != InvalidStreamHdl && buffer.indicesHdl != InvalidStreamHdl && buffer.tangentStreamHdl != InvalidStreamHdl;
	}

	SBuffer m_buffer[EBufferType_Num][NumBufferSlots];
	uint32  m_nextId[EBufferType_Num];

	// Singleton instance
	static CREBreakableGlassBuffer* s_pInstance;
};

#endif // _CRE_BREAKABLE_GLASS_BUFFER_

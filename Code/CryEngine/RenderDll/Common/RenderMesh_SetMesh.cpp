// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RenderMesh.h"
#include <Cry3DEngine/IIndexedMesh.h>

#define TRANSFER_ALIGNMENT 1

static void transfer_writecombined(void* pDst, const void* pSrc, size_t size)
{
	cryMemcpy(pDst, pSrc, size, MC_CPU_TO_GPU);
}

template<EStreamIDs stream, size_t Size>
struct StreamCompactor;

template<EStreamIDs stream, size_t Size>
uint32 CompactStream(uint8 (&buffer)[Size], SSetMeshIntData& data, CMesh& mesh, uint32 beg, uint32 end)
{
	return StreamCompactor<stream, Size>::Compact(buffer, data, mesh, beg, end);
}

// Explicit specializations have to be at namespace scope
namespace StreamCompactorDetail
{
template<typename T>
void CompactPositions(T* pVBuff, SSetMeshIntData& data, CMesh& mesh, const Vec3& posOffset, uint32 beg, uint32 end)
{
	if (mesh.m_pPositionsF16)
		for (size_t i = 0; i < end; ++i)
			pVBuff[i].xyz = mesh.m_pPositionsF16[beg + i];
	else
		for (size_t i = 0; i < end; ++i)
			pVBuff[i].xyz = mesh.m_pPositions[beg + i] - posOffset;
}

template<>
void CompactPositions(SVF_P3F_C4B_T2F* pVBuff, SSetMeshIntData& data, CMesh& mesh, const Vec3& posOffset, uint32 beg, uint32 end)
{
	if (mesh.m_pPositionsF16)
		for (size_t i = 0; i < end; ++i)
			pVBuff[i].xyz = mesh.m_pPositionsF16[beg + i].ToVec3();
	else
		for (size_t i = 0; i < end; ++i)
			pVBuff[i].xyz = mesh.m_pPositions[beg + i] - posOffset;
}

template<typename T>
void CompactUVs(T* pVBuff, SSetMeshIntData& data, CMesh& mesh, uint32 beg, uint32 end)
{
	if (mesh.m_pTexCoord)
		for (size_t i = 0; i < end; ++i)
			mesh.m_pTexCoord[beg + i].ExportTo(pVBuff[i].st);
	// Value less than -1 tells vertex program to use usual (old) terrain tex gen (temporary)
	// Real coordinates from vertex are used only in case of 3d terrain research (CVoxTerrain)
	else
		for (uint32 i = 0; i < end; ++i)
			pVBuff[i].st = Vec2f16(-100.f, -100.f);
}

template<>
void CompactUVs(SVF_P3F_C4B_T2F* pVBuff, SSetMeshIntData& data, CMesh& mesh, uint32 beg, uint32 end)
{
	if (mesh.m_pTexCoord)
		for (size_t i = 0; i < end; ++i)
			mesh.m_pTexCoord[beg + i].GetUV(pVBuff[i].st);
}
}

template<size_t Size>
struct StreamCompactor<VSF_GENERAL, Size>
{
	template<typename T>
	static void CompactPositions(T* pVBuff, SSetMeshIntData& data, CMesh& mesh, const Vec3& posOffset, uint32 beg, uint32 end)
	{
		return StreamCompactorDetail::CompactPositions<T>(pVBuff, data, mesh, posOffset, beg, end);
	}

	static void CompactNormals(SVF_P3S_N4B_C4B_T2S* pVBuff, SSetMeshIntData& data, CMesh& mesh, uint32 beg, uint32 end)
	{
		if (mesh.m_pNorms)
			for (size_t i = 0; i < end; ++i)
			{
				Vec3 n = mesh.m_pNorms[beg + i].GetN();

				pVBuff[i].normal.bcolor[0] = (byte)(n[0] * 127.5f + 128.0f);
				pVBuff[i].normal.bcolor[1] = (byte)(n[1] * 127.5f + 128.0f);
				pVBuff[i].normal.bcolor[2] = (byte)(n[2] * 127.5f + 128.0f);

				SwapEndian(pVBuff[i].normal.dcolor);
			}
	}

	template<typename T>
	static void CompactColors0(T* pVBuff, SSetMeshIntData& data, CMesh& mesh, uint32 beg, uint32 end)
	{
		if (mesh.m_pColor0)
			for (size_t i = 0; i < end; ++i)
			{
				ColorB c = mesh.m_pColor0[beg + i].GetRGBA();

				pVBuff[i].color.bcolor[0] = c.b;
				pVBuff[i].color.bcolor[1] = c.g;
				pVBuff[i].color.bcolor[2] = c.r;
				pVBuff[i].color.bcolor[3] = c.a;

				SwapEndian(pVBuff[i].color.dcolor);
			}
		else
			for (size_t i = 0; i < end; ++i)
				pVBuff[i].color.dcolor = ~0;
	}

	static void CompactColors1(SVF_P2S_N4B_C4B_T1F* pVBuff, SSetMeshIntData& data, CMesh& mesh, uint32 beg, uint32 end)
	{
		if (mesh.m_pColor1)
			for (size_t i = 0; i < end; ++i)
			{
				ColorB c = mesh.m_pColor1[beg + i].GetRGBA();

				pVBuff[i].normal.bcolor[3] = c.b;
			}
	}

	template<typename T>
	static void CompactUVs(T* pVBuff, SSetMeshIntData& data, CMesh& mesh, uint32 beg, uint32 end)
	{
		StreamCompactorDetail::CompactUVs<T>(pVBuff, data, mesh, beg, end);
	}

	static uint32 Compact(uint8 (&buffer)[Size], SSetMeshIntData& data, CMesh& mesh, uint32 beg, uint32 end)
	{
		Vec3 posOffset = data.m_pPosOffset ? *data.m_pPosOffset : Vec3(0, 0, 0);

		if (data.m_pVBuff)
		{
			uint32 amount = 0;

			if (data.m_pMesh->m_pPositions)
			{
				size_t dstPad = (size_t)&data.m_pVBuff[beg * sizeof(SVF_P3F_C4B_T2F)] & (TRANSFER_ALIGNMENT - 1);
				SVF_P3F_C4B_T2F* pVBuff = alias_cast<SVF_P3F_C4B_T2F*>(&buffer[dstPad]);
				amount = min((uint32)(end - beg), (uint32)(Size / sizeof(pVBuff[0])));

				CompactPositions(pVBuff, data, mesh, posOffset, beg, amount);
				CompactUVs(pVBuff, data, mesh, beg, amount);
				CompactColors0(pVBuff, data, mesh, beg, amount);

				transfer_writecombined(&data.m_pVBuff[beg * sizeof(pVBuff[0])], &buffer[dstPad], amount * sizeof(pVBuff[0]));
			}
			else
			{
				size_t dstPad = (size_t)&data.m_pVBuff[beg * sizeof(SVF_P3S_C4B_T2S)] & (TRANSFER_ALIGNMENT - 1);
				SVF_P3S_C4B_T2S* pVBuff = alias_cast<SVF_P3S_C4B_T2S*>(&buffer[dstPad]);
				amount = min((uint32)(end - beg), (uint32)(Size / sizeof(pVBuff[0])));

				if (mesh.m_pP3S_C4B_T2S)
				{
					memcpy(pVBuff, &mesh.m_pP3S_C4B_T2S[beg], sizeof(pVBuff[0]) * amount);
				}
				else
				{
					CompactPositions(pVBuff, data, mesh, posOffset, beg, amount);
					CompactUVs(pVBuff, data, mesh, beg, amount);
					CompactColors0(pVBuff, data, mesh, beg, amount);
				}

				transfer_writecombined(&data.m_pVBuff[beg * sizeof(pVBuff[0])], &buffer[dstPad], amount * sizeof(pVBuff[0]));
			}

			return amount;
		}

		CryFatalError("CRenderMesh::SetMesh_Impl: invalid vertex format for general stream");
		return 0;
	}
};

template<size_t Size>
struct StreamCompactor<VSF_TANGENTS, Size>
{
	static uint32 Compact(uint8 (&buffer)[Size], SSetMeshIntData& data, CMesh& mesh, uint32 beg, uint32 end)
	{
		if (mesh.m_pTangents == NULL || data.m_pTBuff == NULL)
			return end;

		uint32 dstPad = 0;

		SPipTangents* pTBuff = alias_cast<SPipTangents*>(&buffer[dstPad]);
		uint32 amount = min((uint32)(end - beg), (uint32)(Size / sizeof(pTBuff[0])));

		for (size_t i = 0; i < amount; ++i)
			mesh.m_pTangents[beg + i].ExportTo(pTBuff[i]);

		transfer_writecombined(&data.m_pTBuff[beg], &buffer[dstPad], amount * sizeof(data.m_pTBuff[0]));

		return amount;
	}
};

template<size_t Size>
struct StreamCompactor<VSF_QTANGENTS, Size>
{
	static uint32 Compact(uint8 (&buffer)[Size], SSetMeshIntData& data, CMesh& mesh, uint32 beg, uint32 end)
	{
		if (mesh.m_pQTangents == NULL || data.m_pQTBuff == NULL)
			return end;

		uint32 dstPad = 0;

		SPipQTangents* pQTBuff = alias_cast<SPipQTangents*>(&buffer[dstPad]);
		uint32 amount = min((uint32)(end - beg), (uint32)(Size / sizeof(pQTBuff[0])));

		for (size_t i = 0; i < amount; ++i)
			mesh.m_pQTangents[beg + i].ExportTo(pQTBuff[i]);

		transfer_writecombined(&data.m_pQTBuff[beg], &buffer[dstPad], amount * sizeof(pQTBuff[0]));

		return amount;
	}
};

#if ENABLE_NORMALSTREAM_SUPPORT
template<size_t Size>
struct StreamCompactor<VSF_NORMALS, Size>
{
	static uint32 Compact(uint8 (&buffer)[Size], SSetMeshIntData& data, CMesh& mesh, uint32 beg, uint32 end)
	{
		if (mesh.m_pNorms == NULL || data.m_pNormalsBuff == NULL)
			return end;

		uint32 dstPad = 0;

		uint32 amount = min((uint32)(end - beg), (uint32)(Size / sizeof(data.m_pNormalsBuff[0])));
		memcpy(&buffer[dstPad], &mesh.m_pNorms[beg], amount * sizeof(data.m_pNormalsBuff[0]));
		transfer_writecombined(&data.m_pNormalsBuff[beg], &buffer[dstPad], amount * sizeof(data.m_pNormalsBuff[0]));

		return amount;
	}
};
#endif

template<size_t Size>
struct StreamCompactor<VSF_VERTEX_VELOCITY, Size>
{
	static uint32 Compact(uint8 (&buffer)[Size], SSetMeshIntData& data, CMesh& mesh, uint32 beg, uint32 end)
	{
		if (data.m_pVelocities == NULL)
			return end;

		uint32 dstPad = 0;

		uint32 amount = min((uint32)(end - beg), (uint32)(Size / sizeof(data.m_pVelocities[0])));
		memset(&buffer[dstPad], 0x0, amount * sizeof(data.m_pVelocities[0]));
		transfer_writecombined(&data.m_pVelocities[beg], &buffer[dstPad], amount * sizeof(data.m_pVelocities[0]));

		return amount;
	}
};

template<size_t Size>
uint32 CompactIndices(uint8 (&buffer)[Size], SSetMeshIntData& data, CMesh& mesh, uint32 beg, uint32 end)
{
	if (mesh.m_pIndices == NULL || data.m_pInds == NULL)
		return end;

	uint32 dstPad = 0;

	uint32 amount = min((uint32)(end - beg), (uint32)(Size / sizeof(mesh.m_pIndices[0])));
	memcpy(&buffer[dstPad], &mesh.m_pIndices[beg], amount * sizeof(mesh.m_pIndices[0]));
	transfer_writecombined(&data.m_pInds[beg], &buffer[dstPad], amount * sizeof(data.m_pInds[0]));
	return amount;
}

typedef CRY_ALIGN (128) uint8 AlignedStagingBufferT[(8 << 10) + 128];

void CRenderMesh::SetMesh_IntImpl(SSetMeshIntData data)
{
	CMesh& mesh = *data.m_pMesh;
	AlignedStagingBufferT stagingBuffer;

	//////////////////////////////////////////////////////////////////////////
	// Compact the seperate streams from the CMesh instance into a general
	//////////////////////////////////////////////////////////////////////////
	for (uint32 iter = 0; iter < data.m_nVerts; iter += CompactStream<VSF_GENERAL>(stagingBuffer, data, mesh, iter, data.m_nVerts))
		;
	for (uint32 iter = 0; iter < data.m_nVerts; iter += CompactStream<VSF_TANGENTS>(stagingBuffer, data, mesh, iter, data.m_nVerts))
		;
	for (uint32 iter = 0; iter < data.m_nVerts; iter += CompactStream<VSF_QTANGENTS>(stagingBuffer, data, mesh, iter, data.m_nVerts))
		;
#if ENABLE_NORMALSTREAM_SUPPORT
	for (uint32 iter = 0; iter < data.m_nVerts; iter += CompactStream<VSF_NORMALS>(stagingBuffer, data, mesh, iter, data.m_nVerts))
		;
#endif
	for (uint32 iter = 0; iter < data.m_nVerts; iter += CompactStream<VSF_VERTEX_VELOCITY>(stagingBuffer, data, mesh, iter, data.m_nVerts))
		;
	for (uint32 iter = 0; iter < data.m_nInds; iter += CompactIndices(stagingBuffer, data, mesh, iter, data.m_nInds))
		;
}

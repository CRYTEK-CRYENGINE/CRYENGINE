// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

/*=============================================================================
   CREMeshImpl.cpp : implementation of OcLeaf Render Element.

   Revision history:
* Created by Honitch Andrey

   =============================================================================*/

#include "StdAfx.h"
#include "CREMeshImpl.h"
#include <CryAnimation/ICryAnimation.h>

#include "XRenderD3D9/DriverD3D.h"

void CREMeshImpl::mfCenter(Vec3& Pos, CRenderObject* pObj)
{
	Vec3 Mins = m_pRenderMesh->m_vBoxMin;
	Vec3 Maxs = m_pRenderMesh->m_vBoxMax;
	Pos = (Mins + Maxs) * 0.5f;
	if (pObj)
		Pos += pObj->GetTranslation();
}

void CREMeshImpl::mfGetBBox(Vec3& vMins, Vec3& vMaxs)
{
	vMins = m_pRenderMesh->_GetVertexContainer()->m_vBoxMin;
	vMaxs = m_pRenderMesh->_GetVertexContainer()->m_vBoxMax;
}

///////////////////////////////////////////////////////////////////


TRenderChunkArray* CREMeshImpl::mfGetMatInfoList()
{
	return &m_pRenderMesh->m_Chunks;
}

int CREMeshImpl::mfGetMatId()
{
	return m_pChunk->m_nMatID;
}

CRenderChunk* CREMeshImpl::mfGetMatInfo()
{
	return m_pChunk;
}

bool CREMeshImpl::mfUpdate(InputLayoutHandle eVertFormat, int Flags, bool bTessellation)
{
	DETAILED_PROFILE_MARKER("CREMeshImpl::mfUpdate");
	FUNCTION_PROFILER_RENDER_FLAT
	IF (m_pRenderMesh == NULL, 0)
		return false;

	CRenderer* rd = gRenDev;
	const int threadId = gRenDev->GetRenderThreadID();

	bool bSucceed = true;

	CRenderMesh* pVContainer = m_pRenderMesh->_GetVertexContainer();

	m_pRenderMesh->m_nFlags &= ~FRM_SKINNEDNEXTDRAW;

	if (m_pRenderMesh->m_Modified[threadId].linked() || bTessellation) // TODO: use the modified list also for tessellated meshes
	{
		m_pRenderMesh->SyncAsyncUpdate(gRenDev->GetRenderThreadID());

		bSucceed = m_pRenderMesh->RT_CheckUpdate(pVContainer, eVertFormat, Flags | VSM_MASK, bTessellation);
		if (bSucceed)
		{
			AUTO_LOCK(CRenderMesh::m_sLinkLock);
			m_pRenderMesh->m_Modified[threadId].erase();
		}
	}

	if (!bSucceed || !pVContainer->_HasVBStream(VSF_GENERAL))
		return false;

	m_Flags &= ~FCEF_DIRTY;

	//int nFrameId = gEnv->pRenderer->GetFrameID(false); { char buf[1024]; cry_sprintf(buf, "    RE: %p : frame(%d) Clear FCEF_DIRTY\r\n", this, nFrameId); OutputDebugString(buf); }

	return true;
}

void* CREMeshImpl::mfGetPointer(ESrcPointer ePT, int* Stride, EParamType Type, ESrcPointer Dst, int Flags)
{
	DETAILED_PROFILE_MARKER("CREMeshImpl::mfGetPointer");
	CRenderMesh* pRM = m_pRenderMesh->_GetVertexContainer();
	byte* pD = NULL;
	IRenderMesh::ThreadAccessLock lock(pRM);

	switch (ePT)
	{
	case eSrcPointer_Vert:
		pD = pRM->GetPosPtr(*Stride, FSL_READ);
		break;
	case eSrcPointer_Tex:
		pD = pRM->GetUVPtr(*Stride, FSL_READ);
		break;
	case eSrcPointer_Normal:
		pD = pRM->GetNormPtr(*Stride, FSL_READ);
		break;
	case eSrcPointer_Tangent:
		pD = pRM->GetTangentPtr(*Stride, FSL_READ);
		break;
	case eSrcPointer_Color:
		pD = pRM->GetColorPtr(*Stride, FSL_READ);
		break;
	default:
		assert(false);
		break;
	}
	if (m_nFirstVertId && pD)
		pD += m_nFirstVertId * (*Stride);
	return pD;
}

void CREMeshImpl::mfGetPlane(Plane& pl)
{
	CRenderMesh* pRM = m_pRenderMesh->_GetVertexContainer();

	// fixme: plane orientation based on biggest bbox axis
	Vec3 pMin, pMax;
	mfGetBBox(pMin, pMax);
	Vec3 p0 = pMin;
	Vec3 p1 = Vec3(pMax.x, pMin.y, pMin.z);
	Vec3 p2 = Vec3(pMin.x, pMax.y, pMin.z);
	pl.SetPlane(p2, p0, p1);
}

//////////////////////////////////////////////////////////////////////////
InputLayoutHandle CREMeshImpl::GetVertexFormat() const
{
	if (m_pRenderMesh)
		return m_pRenderMesh->_GetVertexContainer()->_GetVertexFormat();
	return InputLayoutHandle::Unspecified;
}

//////////////////////////////////////////////////////////////////////////
bool CREMeshImpl::Compile(CRenderObject* pObj,CRenderView *pRenderView)
{
	if (!m_pRenderMesh)
		return false;

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CREMeshImpl::GetGeometryInfo(SGeometryInfo& geomInfo, bool bSupportTessellation)
{
	if (!m_pRenderMesh)
		return false;

	CRenderMesh* pVContainer = m_pRenderMesh->_GetVertexContainer();

	/* TheoM: This check breaks merged meshes: The streams are only finalized in mfUpdate, inside DrawBatch
	   if (!pVContainer->_HasVBStream(VSF_GENERAL))
	    return false;
	 */

	geomInfo.nFirstIndex = m_nFirstIndexId;
	geomInfo.nFirstVertex = m_nFirstVertId;
	geomInfo.nNumVertices = m_nNumVerts;
	geomInfo.nNumIndices = m_nNumIndices;
	geomInfo.nTessellationPatchIDOffset = m_nPatchIDOffset;
	geomInfo.eVertFormat = pVContainer->_GetVertexFormat();
	geomInfo.primitiveType = pVContainer->_GetPrimitiveType();

	{
		// Check if needs updating.
		//TODO Fix constant | 0x80000000
		uint32 streamMask = 0;
		uint16 nFrameId = gRenDev->GetRenderFrameID();
		if (!mfCheckUpdate(geomInfo.eVertFormat, (uint32)streamMask | 0x80000000, (uint16)nFrameId, bSupportTessellation))
		{
			// Force initial update on fail
			m_pRenderMesh->SyncAsyncUpdate(gRenDev->GetRenderThreadID());
			if (!m_pRenderMesh->RT_CheckUpdate(pVContainer, geomInfo.eVertFormat, (uint32)streamMask | 0x80000000, bSupportTessellation, true))
				return false;
			m_Flags &= ~FCEF_DIRTY;

			//int nFrameId = gEnv->pRenderer->GetFrameID(false); { char buf[1024]; cry_sprintf(buf, "    RE: %p : frame(%d) Clear2 FCEF_DIRTY\r\n", this, nFrameId); OutputDebugString(buf); }
		}
	}

	if (!m_pRenderMesh->FillGeometryInfo(geomInfo))
		return false;

	return true;
}

void CREMeshImpl::DrawToCommandList(CRenderObject* pObj, const SGraphicsPipelinePassContext& ctx)
{
	//@TODO: implement

	//if (!pRE->mfCheckUpdate(rRP.m_CurVFormat, rRP.m_FlagsStreams_Stream | 0x80000000, nFrameID, bTessEnabled))
	//continue;
}

CREMesh::~CREMesh()
{
	//int nFrameId = gEnv->pRenderer->GetFrameID(false); { char buf[1024]; cry_sprintf(buf,"~CREMesh: %p : frame(%d) \r\n", this, nFrameId); OutputDebugString(buf); }
}

// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   CREMeshImpl.cpp : implementation of OcLeaf Render Element.

   Revision history:
* Created by Honitch Andrey

   =============================================================================*/

#include "StdAfx.h"
#include "CREMeshImpl.h"
#include <CryAnimation/ICryAnimation.h>

#include "XRenderD3D9/DriverD3D.h"

void CREMeshImpl::mfReset()
{
}

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

void CREMeshImpl::mfPrepare(bool bCheckOverflow)
{
	DETAILED_PROFILE_MARKER("CREMeshImpl::mfPrepare");
	CRenderer* rd = gRenDev;

	if (bCheckOverflow)
		rd->FX_CheckOverflow(0, 0, this);

	IF (!m_pRenderMesh, 0)
		return;

	rd->m_RP.m_CurVFormat = m_pRenderMesh->_GetVertexFormat();

	{
		rd->m_RP.m_pRE = this;

		rd->m_RP.m_FirstVertex = m_nFirstVertId;
		rd->m_RP.m_FirstIndex = m_nFirstIndexId;
		rd->m_RP.m_RendNumIndices = m_nNumIndices;
		rd->m_RP.m_RendNumVerts = m_nNumVerts;
	}
}

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

void CREMeshImpl::mfPrecache(const SShaderItem& SH)
{
	DETAILED_PROFILE_MARKER("CREMeshImpl::mfPrecache");
	CShader* pSH = (CShader*)SH.m_pShader;
	IF (!pSH, 0)
		return;
	IF (!m_pRenderMesh, 0)
		return;
	IF (m_pRenderMesh->_HasVBStream(VSF_GENERAL), 0)
		return;

	mfCheckUpdate(pSH->m_eVertexFormat, VSM_TANGENTS, gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nFillThreadID].m_nFrameUpdateID);
}

bool CREMeshImpl::mfUpdate(InputLayoutHandle eVertFormat, int Flags, bool bTessellation)
{
	DETAILED_PROFILE_MARKER("CREMeshImpl::mfUpdate");
	FUNCTION_PROFILER_RENDER_FLAT
	IF (m_pRenderMesh == NULL, 0)
		return false;

	CRenderer* rd = gRenDev;
	const int threadId = rd->m_RP.m_nProcessThreadID;

	bool bSucceed = true;

	CRenderMesh* pVContainer = m_pRenderMesh->_GetVertexContainer();

	if (rd->m_RP.m_pShader && (rd->m_RP.m_pShader->m_Flags2 & EF2_DEFAULTVERTEXFORMAT))
		eVertFormat = pVContainer->_GetVertexFormat();

	m_pRenderMesh->m_nFlags &= ~FRM_SKINNEDNEXTDRAW;

	if (m_pRenderMesh->m_Modified[threadId].linked() || bTessellation) // TODO: use the modified list also for tessellated meshes
	{
		m_pRenderMesh->SyncAsyncUpdate(gRenDev->m_RP.m_nProcessThreadID);

		bSucceed = m_pRenderMesh->RT_CheckUpdate(pVContainer, eVertFormat, Flags | VSM_MASK, bTessellation);
		if (bSucceed)
		{
			m_pRenderMesh->m_Modified[threadId].erase();
		}
	}

	if (!bSucceed || !pVContainer->_HasVBStream(VSF_GENERAL))
		return false;

	bool bSkinned = (m_pRenderMesh->m_nFlags & (FRM_SKINNED | FRM_SKINNEDNEXTDRAW)) != 0;
	if ((Flags | VSM_MASK) & VSM_TANGENTS)
	{
		if (bSkinned && pVContainer->_HasVBStream(VSF_QTANGENTS))
		{
			rd->m_RP.m_FlagsStreams_Stream &= ~VSM_TANGENTS;
			rd->m_RP.m_FlagsStreams_Decl &= ~VSM_TANGENTS;
			rd->m_RP.m_FlagsStreams_Stream |= (1 << VSF_QTANGENTS);
			rd->m_RP.m_FlagsStreams_Decl |= (1 << VSF_QTANGENTS);
		}
	}

	rd->m_RP.m_CurVFormat = pVContainer->_GetVertexFormat();
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
bool CREMeshImpl::Compile(CRenderObject* pObj)
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
		uint16 nFrameId = gRenDev->m_RP.m_TI[gRenDev->m_RP.m_nProcessThreadID].m_nFrameUpdateID;
		if (!mfCheckUpdate(geomInfo.eVertFormat, (uint32)streamMask | 0x80000000, (uint16)nFrameId, bSupportTessellation))
		{
			// Force initial update on fail
			m_pRenderMesh->SyncAsyncUpdate(gRenDev->m_RP.m_nProcessThreadID);
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

bool CREMeshImpl::BindRemappedSkinningData(uint32 guid)
{

	CD3D9Renderer* rd = gcpRendD3D;

	SStreamInfo streamInfo;
	CRenderMesh* pRM = m_pRenderMesh->_GetVertexContainer();

	if (pRM->m_RemappedBoneIndices.empty())
		return true;

	if (pRM->GetRemappedSkinningData(guid, streamInfo))
	{
		buffer_size_t offset;
		void* buffer = gRenDev->m_DevBufMan.GetD3D(streamInfo.hStream, &offset);

		rd->FX_SetVStream(VSF_HWSKIN_INFO, buffer, offset, streamInfo.nStride);
		return true;
	}

	return false;
}

bool CREMeshImpl::mfPreDraw(SShaderPass* sl)
{
	DETAILED_PROFILE_MARKER("CREMeshImpl::mfPreDraw");
	IF (!m_pRenderMesh, 0)
		return false;

	//PROFILE_LABEL_SHADER(m_pRenderMesh->GetSourceName() ? m_pRenderMesh->GetSourceName() : "Unknown mesh-resource name");

#if ENABLE_NORMALSTREAM_SUPPORT
	if (CHWShader_D3D::s_pCurInstHS)
	{
		//assert(m_pRenderMesh->_HasVBStream(VSF_NORMALS));
	}
#endif

	CRenderMesh* pRM = m_pRenderMesh->_GetVertexContainer();
	pRM->PrefetchVertexStreams();

	// Should never happen. Video buffer is missing
	if (!pRM->_HasVBStream(VSF_GENERAL) || !m_pRenderMesh->_HasIBStream())
		return false;
	CD3D9Renderer* rd = gcpRendD3D;

	m_pRenderMesh->BindStreamsToRenderPipeline();

	m_Flags |= FCEF_PRE_DRAW_DONE;

	return true;
}

#if !defined(_RELEASE)
inline bool CREMeshImpl::ValidateDraw(EShaderType shaderType)
{
	bool ret = true;

	if (shaderType != eST_General &&
	    shaderType != eST_PostProcess &&
	    shaderType != eST_FX &&
	    shaderType != eST_Vegetation &&
	    shaderType != eST_Terrain &&
	    shaderType != eST_Glass &&
	    shaderType != eST_Water)
	{
		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR, "Incorrect shader set for mesh type: %s : %d", m_pRenderMesh->GetSourceName(), shaderType);
		ret = false;
	}

	if (!(m_Flags & FCEF_PRE_DRAW_DONE))
	{
		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR, "PreDraw not called for mesh: %s", m_pRenderMesh->GetSourceName());
		ret = false;
	}

	return ret;
}
#endif

bool CREMeshImpl::mfDraw(CShader* ef, SShaderPass* sl)
{
	DETAILED_PROFILE_MARKER("CREMeshImpl::mfDraw");
	FUNCTION_PROFILER_RENDER_FLAT
	CD3D9Renderer* r = gcpRendD3D;

#if !defined(_RELEASE)
	if (!ValidateDraw(ef->m_eShaderType))
	{
		return false;
	}
#endif

	CRenderMesh* pRM = m_pRenderMesh;
	if (ef->m_HWTechniques.Num() && pRM->CanRender())
	{
		r->FX_DrawIndexedMesh(r->m_RP.m_RendNumGroup >= 0 ? eptHWSkinGroups : pRM->_GetPrimitiveType());
	}
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

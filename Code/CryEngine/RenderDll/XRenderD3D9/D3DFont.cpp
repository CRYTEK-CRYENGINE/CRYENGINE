// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "DriverD3D.h"

//=========================================================================================

#include "../CryFont/FBitmap.h"

bool CD3D9Renderer::FontUpdateTexture(int nTexId, int nX, int nY, int USize, int VSize, byte* pSrcData)
{
	CTexture* tp = CTexture::GetByID(nTexId);
	assert(tp);

	if (tp)
	{
		tp->UpdateTextureRegion(pSrcData, nX, nY, 0, USize, VSize, 1, eTF_A8);

		return true;
	}
	return false;
}

bool CD3D9Renderer::FontUploadTexture(class CFBitmap* pBmp, ETEX_Format eSrcFormat)
{
	if (!pBmp)
	{
		return false;
	}

	unsigned int* pSrcData = new unsigned int[pBmp->GetWidth() * pBmp->GetHeight()];

	if (!pSrcData)
	{
		return false;
	}

	pBmp->Get32Bpp(&pSrcData);

	char szName[128];
	cry_sprintf(szName, "$AutoFont_%d", m_TexGenID++);

	int iFlags = FT_TEX_FONT | FT_DONT_STREAM | FT_DONT_RELEASE;
	CTexture* tp = CTexture::GetOrCreate2DTexture(szName, pBmp->GetWidth(), pBmp->GetHeight(), 1, iFlags, (unsigned char*)pSrcData, eSrcFormat);

	SAFE_DELETE_ARRAY(pSrcData);

	pBmp->SetRenderData((void*)tp);

	return true;
}

int CD3D9Renderer::FontCreateTexture(int Width, int Height, byte* pSrcData, ETEX_Format eSrcFormat, bool genMips)
{
	if (!pSrcData)
		return -1;

	char szName[128];
	cry_sprintf(szName, "$AutoFont_%d", m_TexGenID++);

	int iFlags = FT_TEX_FONT | FT_DONT_STREAM | FT_DONT_RELEASE;
	if (genMips)
		iFlags |= FT_FORCE_MIPS;
	CTexture* tp = CTexture::GetOrCreate2DTexture(szName, Width, Height, 1, iFlags, (unsigned char*)pSrcData, eSrcFormat);

	return tp->GetID();
}

void CD3D9Renderer::FontReleaseTexture(class CFBitmap* pBmp)
{
	if (!pBmp)
	{
		return;
	}

	CTexture* tp = (CTexture*)pBmp->GetRenderData();

	SAFE_RELEASE(tp);
}

void CD3D9Renderer::FontSetTexture(class CFBitmap* pBmp, int nFilterMode)
{
	if (pBmp)
	{
		CTexture* tp = (CTexture*)pBmp->GetRenderData();
		if (tp)
		{
			SSamplerState::SetDefaultFilterMode(nFilterMode);
			tp->Apply(0);
		}
	}
}

void CD3D9Renderer::FontSetTexture(int nTexId, int nFilterMode)
{
	if (nTexId <= 0)
		return;
	CTexture* tp = CTexture::GetByID(nTexId);
	assert(tp);
	if (!tp)
		return;

	SSamplerState::SetDefaultFilterMode(nFilterMode);
	tp->Apply(0);
	//CTexture::m_Text_NoTexture->Apply(0);
}

int s_InFontState = 0;

void CD3D9Renderer::FontSetRenderingState(unsigned int nVPWidth, unsigned int nVPHeight)
{
	assert(!s_InFontState);
	assert(m_pRT->IsRenderThread());

	// setup various d3d things that we need
	FontSetState(false);

	s_InFontState++;

	Matrix44A* m;
	m_RP.m_TI[m_RP.m_nProcessThreadID].m_matProj->Push();
	m_RP.m_TI[m_RP.m_nProcessThreadID].m_matProj->LoadIdentity();
	m = (Matrix44A*)m_RP.m_TI[m_RP.m_nProcessThreadID].m_matProj->GetTop();

	if (m_NewViewport.nWidth != 0 && m_NewViewport.nHeight != 0)
	{
		const bool bReverseDepth = (m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) != 0;
		const float zn = bReverseDepth ? 1.0f : -1.0f;
		const float zf = bReverseDepth ? -1.0f : 1.0f;

		mathMatrixOrthoOffCenter(m, 0.0f, (float)m_NewViewport.nWidth, (float)m_NewViewport.nHeight, 0.0f, zn, zf);
	}
	//GetDevice().SetTransform(D3DTS_PROJECTION, m);

	m_RP.m_TI[m_RP.m_nProcessThreadID].m_matView->Push();
	m_RP.m_TI[m_RP.m_nProcessThreadID].m_matView->LoadIdentity();
	//GetDevice().SetTransform(D3DTS_VIEW, (D3DXMATRIX*)m_RP.m_TI[nThreadID].m_matView->GetTop());
}

void CD3D9Renderer::FontRestoreRenderingState()
{
	Matrix44A* m;

	assert(m_pRT->IsRenderThread());
	assert(s_InFontState == 1);
	s_InFontState--;

	m_RP.m_TI[m_RP.m_nProcessThreadID].m_matProj->Pop();
	m = (Matrix44A*)m_RP.m_TI[m_RP.m_nProcessThreadID].m_matProj->GetTop();
	//GetDevice().SetTransform(D3DTS_PROJECTION, m);

	m_RP.m_TI[m_RP.m_nProcessThreadID].m_matView->Pop();

	FontSetState(true);
}

void CD3D9Renderer::FontSetBlending(int blendSrc, int blendDest)
{
	assert(m_pRT->IsRenderThread());
	m_fontBlendMode = blendSrc | blendDest;

	if (eTF_R16G16F == CTexture::s_eTFZ || CTexture::s_eTFZ == eTF_R32F)
		FX_SetState(m_fontBlendMode | GS_DEPTHFUNC_LEQUAL);
	else
		FX_SetState(m_fontBlendMode | GS_DEPTHFUNC_LEQUAL | GS_ALPHATEST, 0);
}

void CD3D9Renderer::FontSetState(bool bRestore)
{
	static DWORD wireframeMode;
	static bool bMatColor;
	static uint32 nState, nForceState;

	assert(m_pRT->IsRenderThread());

	// grab the modes that we might need to change
	if (!bRestore)
	{
		D3DSetCull(eCULL_None);

		nState = m_RP.m_CurState;
		nForceState = m_RP.m_StateOr;
		wireframeMode = m_wireframe_mode;

		EF_SetVertColor();

		if (wireframeMode > R_SOLID_MODE)
		{
			SStateRaster RS = gcpRendD3D->m_StatesRS[gcpRendD3D->m_nCurStateRS];
			RS.Desc.FillMode = D3D11_FILL_SOLID;
			gcpRendD3D->SetRasterState(&RS);
		}

		m_RP.m_FlagsPerFlush = 0;
		if (eTF_R16G16F == CTexture::s_eTFZ || CTexture::s_eTFZ == eTF_R32F)
			FX_SetState(m_fontBlendMode | GS_DEPTHFUNC_LEQUAL);
		else
			FX_SetState(m_fontBlendMode | GS_DEPTHFUNC_LEQUAL | GS_ALPHATEST, 0);

		EF_SetColorOp(eCO_REPLACE, eCO_MODULATE, eCA_Diffuse | (eCA_Diffuse << 3), DEF_TEXARG0);
		//GetDevice().SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		//GetDevice().SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	}
	else
	{
		if (wireframeMode == R_WIREFRAME_MODE)
		{
			SStateRaster RS = gcpRendD3D->m_StatesRS[gcpRendD3D->m_nCurStateRS];
			RS.Desc.FillMode = D3D11_FILL_WIREFRAME;
			gcpRendD3D->SetRasterState(&RS);
		}
		if (wireframeMode == R_POINT_MODE)
		{
		}
		m_RP.m_StateOr = nForceState;

		//FX_SetState(State);
	}
}

void CD3D9Renderer::DrawDynVB(SVF_P3F_C4B_T2F* pBuf, uint16* pInds, int nVerts, int nInds, const PublicRenderPrimitiveType nPrimType)
{
	PROFILE_FRAME(Draw_IndexMesh_Dyn);

	if (!pBuf)
		return;
	if (m_bDeviceLost)
		return;

	//if (CV_d3d9_forcesoftware)
	//  return;
	if (!nVerts || (pInds && !nInds))
		return;

	m_pRT->RC_DrawDynVB(pBuf, pInds, nVerts, nInds, nPrimType);
}

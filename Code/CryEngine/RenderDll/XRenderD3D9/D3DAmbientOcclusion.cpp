// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "DriverD3D.h"
#include <Cry3DEngine/I3DEngine.h>
#include "D3DPostProcess.h"
#include "../Common/Textures/TextureHelpers.h"

// TODO: Unify with other deferred primitive implementation
const t_arrDeferredMeshVertBuff& CD3D9Renderer::GetDeferredUnitBoxVertexBuffer() const
{
	return m_arrDeferredVerts;
}

const t_arrDeferredMeshIndBuff& CD3D9Renderer::GetDeferredUnitBoxIndexBuffer() const
{
	return m_arrDeferredInds;
}

void CD3D9Renderer::CreateDeferredUnitBox(t_arrDeferredMeshIndBuff& indBuff, t_arrDeferredMeshVertBuff& vertBuff)
{
	SVF_P3F_C4B_T2F vert;
	Vec3 vNDC;

	indBuff.clear();
	indBuff.reserve(36);

	vertBuff.clear();
	vertBuff.reserve(8);

	//Create frustum
	for (int i = 0; i < 8; i++)
	{
		//Generate screen space frustum (CCW faces)
		vNDC = Vec3((i == 0 || i == 1 || i == 4 || i == 5) ? 0.0f : 1.0f,
		            (i == 0 || i == 3 || i == 4 || i == 7) ? 0.0f : 1.0f,
		            (i == 0 || i == 1 || i == 2 || i == 3) ? 0.0f : 1.0f
		            );
		vert.xyz = vNDC;
		vert.st = Vec2(0.0f, 0.0f);
		vert.color.dcolor = -1;
		vertBuff.push_back(vert);
	}

	//CCW faces
	uint16 nFaces[6][4] = {
		{ 0, 1, 2, 3 },
		{ 4, 7, 6, 5 },
		{ 0, 3, 7, 4 },
		{ 1, 5, 6, 2 },
		{ 0, 4, 5, 1 },
		{ 3, 2, 6, 7 }
	};

	//init indices for triangles drawing
	for (int i = 0; i < 6; i++)
	{
		indBuff.push_back((uint16)  nFaces[i][0]);
		indBuff.push_back((uint16)  nFaces[i][1]);
		indBuff.push_back((uint16)  nFaces[i][2]);

		indBuff.push_back((uint16)  nFaces[i][0]);
		indBuff.push_back((uint16)  nFaces[i][2]);
		indBuff.push_back((uint16)  nFaces[i][3]);
	}
}

#if defined(USE_NV_API)
	#include NV_API_HEADER
#endif
#if defined(USE_AMD_API)
	#include <AMD/AGS Lib/inc/amd_ags.h>
#endif
#if defined(USE_AMD_EXT)
	#include <AMD/AMD_Extensions/AmdDxExtDepthBoundsApi.h>
#endif

void CD3D9Renderer::SetDepthBoundTest(float fMin, float fMax, bool bEnable)
{
	if (!m_bDeviceSupports_NVDBT)
		return;
#if DURANGO_ENABLE_ASYNC_DIPS
	WaitForAsynchronousDevice();
#endif

	m_bDepthBoundsEnabled = bEnable;
	if (bEnable)
	{
		m_fDepthBoundsMin = fMin;
		m_fDepthBoundsMax = fMax;
#if CRY_RENDERER_OPENGL && !DXGL_FULL_EMULATION
		DXGLSetDepthBoundsTest(true, fMin, fMax);
#elif CRY_RENDERER_GNM
		ORBIS_TO_IMPLEMENT;
#elif (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && defined(USE_NV_API) //transparent execution without NVDB
		NvAPI_Status status = NvAPI_D3D11_SetDepthBoundsTest(GetDevice().GetRealDevice(), bEnable, fMin, fMax);
		assert(status == NVAPI_OK);
#endif
	}
	else // disable depth bound test
	{
		m_fDepthBoundsMin = 0;
		m_fDepthBoundsMax = 1.0f;
#if CRY_RENDERER_OPENGL && !DXGL_FULL_EMULATION
		DXGLSetDepthBoundsTest(false, 0.0f, 1.0f);
#elif CRY_RENDERER_GNM
		ORBIS_TO_IMPLEMENT;
#elif (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && defined(USE_NV_API)
		NvAPI_Status status = NvAPI_D3D11_SetDepthBoundsTest(GetDevice().GetRealDevice(), bEnable, fMin, fMax);
		assert(status == NVAPI_OK);
#endif
	}
}

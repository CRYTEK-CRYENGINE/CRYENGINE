// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DeviceObjects.h"
#include "DriverD3D.h"
#include "xxhash.h"
#include "Common/ReverseDepth.h"
#include "Common/Textures/TextureHelpers.h"
#include "../GraphicsPipeline/Common/GraphicsPipelineStateSet.h"

#define REDUNDANCY_ASSERT(...)

extern uint8 g_StencilFuncLookup[8];
extern uint8 g_StencilOpLookup[8];

CDeviceObjectFactory CDeviceObjectFactory::m_singleton;

CDeviceObjectFactory::~CDeviceObjectFactory()
{
	if (m_fence_handle != DeviceFenceHandle() && FAILED(ReleaseFence(m_fence_handle)))
	{
		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING, "could not release sync fence");
	}
}

// Local helper function to erase items with refcount 1 from some cache of shared pointers
template<typename TCache>
static void EraseUnusedEntriesFromCache(TCache& cache)
{
	for (auto it = cache.begin(), itEnd = cache.end(); it != itEnd; )
	{
		auto itCurrentEntry = it++;
		if (itCurrentEntry->second.use_count() == 1)
			cache.erase(itCurrentEntry->first);
	}
}

// Local helper function to erase expired-items from some cache of weak pointers
template<typename TCache>
static void EraseExpiredEntriesFromCache(TCache& cache)
{
	for (auto it = cache.begin(), itEnd = cache.end(); it != itEnd; )
	{
		auto itCurrentEntry = it++;
		if (itCurrentEntry->second.expired())
			cache.erase(itCurrentEntry->first);
	}
}

bool CDeviceObjectFactory::CanUseCoreCommandList()
{
	return gcpRendD3D->m_pRT->IsRenderThread() && !gcpRendD3D->m_pRT->IsRenderLoadingThread();
}

////////////////////////////////////////////////////////////////////////////
// Fence API (TODO: offload all to CDeviceFenceHandle)

void CDeviceObjectFactory::SyncToGPU()
{
	if (CRenderer::CV_r_enable_full_gpu_sync)
	{
		if (m_fence_handle == DeviceFenceHandle() && FAILED(CreateFence(m_fence_handle)))
		{
			CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING, "could not create sync fence");
		}
		if (m_fence_handle)
		{
			IssueFence(m_fence_handle);
			SyncFence(m_fence_handle, true);
		}
	}
}

void CDeviceObjectFactory::IssueFrameFences()
{
	static_assert(CRY_ARRAY_COUNT(m_frameFences) == MAX_FRAMES_IN_FLIGHT, "Unexpected size for m_frameFences");

	if (!m_frameFences[0])
	{
		m_frameFenceCounter = MAX_FRAMES_IN_FLIGHT;
		for (uint32 i = 0; i < CRY_ARRAY_COUNT(m_frameFences); i++)
		{
			HRESULT hr = CreateFence(m_frameFences[i]);
			assert(hr == S_OK);
		}
		return;
	}

	HRESULT hr = IssueFence(m_frameFences[m_frameFenceCounter % MAX_FRAMES_IN_FLIGHT]);
	assert(hr == S_OK);

	if (CRenderer::CV_r_SyncToFrameFence)
	{
		// Stall render thread until GPU has finished processing previous frame (in case max frame latency is 1)
		PROFILE_FRAME("WAIT FOR GPU");
		HRESULT hr = SyncFence(m_frameFences[(m_frameFenceCounter - (MAX_FRAMES_IN_FLIGHT - 1)) % MAX_FRAMES_IN_FLIGHT], true, true);
		assert(hr == S_OK);
	}
	m_completedFrameFenceCounter = m_frameFenceCounter - (MAX_FRAMES_IN_FLIGHT - 1);
	m_frameFenceCounter += 1;
}

void CDeviceObjectFactory::ReleaseFrameFences()
{
	for (uint32 i = 0; i < CRY_ARRAY_COUNT(m_frameFences); i++)
		ReleaseFence(m_frameFences[i]);
}

////////////////////////////////////////////////////////////////////////////
// SamplerState API

CStaticDeviceObjectStorage<SamplerStateHandle, SSamplerState, CDeviceSamplerState, false, CDeviceObjectFactory::CreateSamplerState> CDeviceObjectFactory::s_SamplerStates;

void CDeviceObjectFactory::AllocatePredefinedSamplerStates()
{
	ReserveSamplerStates(300); // this likes to expand, so it'd be nice if it didn't; 300 => ~6Kb, there were 171 after one level

	// *INDENT-OFF*
	SamplerStateHandle a = GetOrCreateSamplerStateHandle(SSamplerState(FILTER_POINT,     eSamplerAddressMode_Clamp,  eSamplerAddressMode_Clamp,  eSamplerAddressMode_Clamp,   0      )); assert(a == EDefaultSamplerStates::PointClamp           );
	SamplerStateHandle b = GetOrCreateSamplerStateHandle(SSamplerState(FILTER_POINT,     eSamplerAddressMode_Wrap,   eSamplerAddressMode_Wrap,   eSamplerAddressMode_Wrap,    0      )); assert(b == EDefaultSamplerStates::PointWrap            );
	SamplerStateHandle c = GetOrCreateSamplerStateHandle(SSamplerState(FILTER_POINT,     eSamplerAddressMode_Border, eSamplerAddressMode_Border, eSamplerAddressMode_Border,  0      )); assert(c == EDefaultSamplerStates::PointBorder_Black    );
	SamplerStateHandle d = GetOrCreateSamplerStateHandle(SSamplerState(FILTER_POINT,     eSamplerAddressMode_Border, eSamplerAddressMode_Border, eSamplerAddressMode_Border, ~0      )); assert(d == EDefaultSamplerStates::PointBorder_White    );
	SamplerStateHandle e = GetOrCreateSamplerStateHandle(SSamplerState(FILTER_POINT,     eSamplerAddressMode_Wrap,   eSamplerAddressMode_Wrap,   eSamplerAddressMode_Wrap,    0, true)); assert(e == EDefaultSamplerStates::PointCompare         );
	SamplerStateHandle f = GetOrCreateSamplerStateHandle(SSamplerState(FILTER_LINEAR,    eSamplerAddressMode_Clamp,  eSamplerAddressMode_Clamp,  eSamplerAddressMode_Clamp,   0      )); assert(f == EDefaultSamplerStates::LinearClamp          );
	SamplerStateHandle g = GetOrCreateSamplerStateHandle(SSamplerState(FILTER_LINEAR,    eSamplerAddressMode_Wrap,   eSamplerAddressMode_Wrap,   eSamplerAddressMode_Wrap,    0      )); assert(g == EDefaultSamplerStates::LinearWrap           );
	SamplerStateHandle h = GetOrCreateSamplerStateHandle(SSamplerState(FILTER_LINEAR,    eSamplerAddressMode_Border, eSamplerAddressMode_Border, eSamplerAddressMode_Border,  0      )); assert(h == EDefaultSamplerStates::LinearBorder_Black   );
	SamplerStateHandle i = GetOrCreateSamplerStateHandle(SSamplerState(FILTER_LINEAR,    eSamplerAddressMode_Clamp,  eSamplerAddressMode_Clamp,  eSamplerAddressMode_Clamp,   0, true)); assert(i == EDefaultSamplerStates::LinearCompare        );
	SamplerStateHandle j = GetOrCreateSamplerStateHandle(SSamplerState(FILTER_BILINEAR,  eSamplerAddressMode_Clamp,  eSamplerAddressMode_Clamp,  eSamplerAddressMode_Clamp,   0      )); assert(j == EDefaultSamplerStates::BilinearClamp        );
	SamplerStateHandle k = GetOrCreateSamplerStateHandle(SSamplerState(FILTER_BILINEAR,  eSamplerAddressMode_Wrap,   eSamplerAddressMode_Wrap,   eSamplerAddressMode_Wrap,    0      )); assert(k == EDefaultSamplerStates::BilinearWrap         );
	SamplerStateHandle l = GetOrCreateSamplerStateHandle(SSamplerState(FILTER_BILINEAR,  eSamplerAddressMode_Border, eSamplerAddressMode_Border, eSamplerAddressMode_Border,  0      )); assert(l == EDefaultSamplerStates::BilinearBorder_Black );
	SamplerStateHandle m = GetOrCreateSamplerStateHandle(SSamplerState(FILTER_BILINEAR,  eSamplerAddressMode_Wrap,   eSamplerAddressMode_Wrap,   eSamplerAddressMode_Wrap,    0, true)); assert(m == EDefaultSamplerStates::BilinearCompare      );
	SamplerStateHandle n = GetOrCreateSamplerStateHandle(SSamplerState(FILTER_TRILINEAR, eSamplerAddressMode_Clamp,  eSamplerAddressMode_Clamp,  eSamplerAddressMode_Clamp,   0      )); assert(n == EDefaultSamplerStates::TrilinearClamp       );
	SamplerStateHandle o = GetOrCreateSamplerStateHandle(SSamplerState(FILTER_TRILINEAR, eSamplerAddressMode_Wrap,   eSamplerAddressMode_Wrap,   eSamplerAddressMode_Wrap,    0      )); assert(o == EDefaultSamplerStates::TrilinearWrap        );
	SamplerStateHandle p = GetOrCreateSamplerStateHandle(SSamplerState(FILTER_TRILINEAR, eSamplerAddressMode_Border, eSamplerAddressMode_Border, eSamplerAddressMode_Border,  0      )); assert(p == EDefaultSamplerStates::TrilinearBorder_Black);
	SamplerStateHandle q = GetOrCreateSamplerStateHandle(SSamplerState(FILTER_TRILINEAR, eSamplerAddressMode_Border, eSamplerAddressMode_Border, eSamplerAddressMode_Border, ~0      )); assert(q == EDefaultSamplerStates::TrilinearBorder_White);
	// *INDENT-ON*
}

void CDeviceObjectFactory::TrimSamplerStates()
{
	s_SamplerStates.Release(EDefaultSamplerStates::PreAllocated);
}

void CDeviceObjectFactory::ReleaseSamplerStates()
{
	s_SamplerStates.Release(EDefaultSamplerStates::PointClamp);
}

////////////////////////////////////////////////////////////////////////////
// InputLayout API

CStaticDeviceObjectStorage<InputLayoutHandle, SInputLayout, CDeviceInputLayout, true, CDeviceObjectFactory::CreateInputLayout> CDeviceObjectFactory::s_InputLayouts;

std::vector<InputLayoutHandle> CDeviceObjectFactory::s_InputLayoutPermutations[1 << VSF_NUM][3]; // [StreamMask][Morph][VertexFmt]

static const D3D11_INPUT_ELEMENT_DESC VertexDecl_Empty[] = { {} }; // Empty

static const D3D11_INPUT_ELEMENT_DESC VertexDecl_P3F_C4B_T2F[] = 
{
	{ "POSITION"    , 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0, offsetof(SVF_P3F_C4B_T2F     , xyz  ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR"       , 0, DXGI_FORMAT_R8G8B8A8_UNORM    , 0, offsetof(SVF_P3F_C4B_T2F     , color), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD"    , 0, DXGI_FORMAT_R32G32_FLOAT      , 0, offsetof(SVF_P3F_C4B_T2F     , st   ), D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const D3D11_INPUT_ELEMENT_DESC VertexDecl_P3S_C4B_T2S[] =
{
	{ "POSITION"    , 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 0, offsetof(SVF_P3S_C4B_T2S     , xyz  ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR"       , 0, DXGI_FORMAT_R8G8B8A8_UNORM    , 0, offsetof(SVF_P3S_C4B_T2S     , color), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD"    , 0, DXGI_FORMAT_R16G16_FLOAT      , 0, offsetof(SVF_P3S_C4B_T2S     , st   ), D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const D3D11_INPUT_ELEMENT_DESC VertexDecl_P3S_N4B_C4B_T2S[] =
{
	{ "POSITION"    , 0, DXGI_FORMAT_R16G16B16A16_FLOAT, 0, offsetof(SVF_P3S_N4B_C4B_T2S, xyz   ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "NORMAL"      , 0, DXGI_FORMAT_R8G8B8A8_UNORM    , 0, offsetof(SVF_P3S_N4B_C4B_T2S, normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR"       , 0, DXGI_FORMAT_R8G8B8A8_UNORM    , 0, offsetof(SVF_P3S_N4B_C4B_T2S, color ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD"    , 0, DXGI_FORMAT_R16G16_FLOAT      , 0, offsetof(SVF_P3S_N4B_C4B_T2S, st    ), D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const D3D11_INPUT_ELEMENT_DESC VertexDecl_P3F_C4B_T4B_N3F2[] = // ParticleVT.cfi: app2vertParticleGeneral
{
	{ "POSITION"    , 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0, offsetof(SVF_P3F_C4B_T4B_N3F2, xyz  ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR"       , 0, DXGI_FORMAT_R8G8B8A8_UNORM    , 0, offsetof(SVF_P3F_C4B_T4B_N3F2, color), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD"    , 0, DXGI_FORMAT_R8G8B8A8_UNORM    , 0, offsetof(SVF_P3F_C4B_T4B_N3F2, st   ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "AXIS"        , 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0, offsetof(SVF_P3F_C4B_T4B_N3F2, xaxis), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "AXIS"        , 1, DXGI_FORMAT_R32G32B32_FLOAT   , 0, offsetof(SVF_P3F_C4B_T4B_N3F2, yaxis), D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const D3D11_INPUT_ELEMENT_DESC VertexDecl_TP3F_C4B_T2F[] =
{
	{ "POSITION"    , 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(SVF_TP3F_C4B_T2F    , pos  ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR"       , 0, DXGI_FORMAT_R8G8B8A8_UNORM    , 0, offsetof(SVF_TP3F_C4B_T2F    , color), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD"    , 0, DXGI_FORMAT_R32G32_FLOAT      , 0, offsetof(SVF_TP3F_C4B_T2F    , st   ), D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const D3D11_INPUT_ELEMENT_DESC VertexDecl_P3F_T3F[] =
{
	{ "POSITION"    , 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0, offsetof(SVF_P3F_T3F         , p    ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD"    , 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0, offsetof(SVF_P3F_T3F         , st   ), D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const D3D11_INPUT_ELEMENT_DESC VertexDecl_P3F_T2F_T3F[] =
{
	{ "POSITION"    , 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0, offsetof(SVF_P3F_T2F_T3F     , p    ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD"    , 0, DXGI_FORMAT_R32G32_FLOAT      , 0, offsetof(SVF_P3F_T2F_T3F     , st0  ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD"    , 1, DXGI_FORMAT_R32G32B32_FLOAT   , 0, offsetof(SVF_P3F_T2F_T3F     , st1  ), D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const D3D11_INPUT_ELEMENT_DESC VertexDecl_P3F[] =
{
	{ "POSITION"    , 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0, offsetof(SVF_P3F             , xyz  ), D3D11_INPUT_PER_VERTEX_DATA, 0 },

	// TODO: remove this from the definition, it's a workaround for ZPass shaders expecting COLOR/TEXCOORD
	{ "COLOR"       , 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0, offsetof(SVF_P3F             , xyz  ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD"    , 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0, offsetof(SVF_P3F             , xyz  ), D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const D3D11_INPUT_ELEMENT_DESC VertexDecl_P2S_N4B_C4B_T1F[] =
{
	{ "POSITION"    , 0, DXGI_FORMAT_R16G16_FLOAT      , 0, offsetof(SVF_P2S_N4B_C4B_T1F, xy    ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "NORMAL"      , 0, DXGI_FORMAT_R8G8B8A8_UNORM    , 0, offsetof(SVF_P2S_N4B_C4B_T1F, normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR"       , 0, DXGI_FORMAT_R8G8B8A8_UNORM    , 0, offsetof(SVF_P2S_N4B_C4B_T1F, color ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD"    , 0, DXGI_FORMAT_R32_FLOAT         , 0, offsetof(SVF_P2S_N4B_C4B_T1F, z     ), D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const D3D11_INPUT_ELEMENT_DESC VertexDecl_P3F_C4B_T2S[] =
{
	{ "POSITION"    , 0, DXGI_FORMAT_R32G32B32_FLOAT   , 0, offsetof(SVF_P3F_C4B_T2S     , xyz  ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR"       , 0, DXGI_FORMAT_R8G8B8A8_UNORM    , 0, offsetof(SVF_P3F_C4B_T2S     , color), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD"    , 0, DXGI_FORMAT_R16G16_FLOAT      , 0, offsetof(SVF_P3F_C4B_T2S     , st   ), D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const D3D11_INPUT_ELEMENT_DESC VertexDecl_T4F_B4F[] =
{
	{ "TANGENT"     , 0, DXGI_FORMAT_R32G32B32A32_FLOAT, VSF_TANGENTS,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "BITANGENT"   , 0, DXGI_FORMAT_R32G32B32A32_FLOAT, VSF_TANGENTS, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const D3D11_INPUT_ELEMENT_DESC VertexDecl_T4S_B4S[] =
{
	{ "TANGENT"     , 0, DXGI_FORMAT_R16G16B16A16_SNORM, VSF_TANGENTS,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "BITANGENT"   , 0, DXGI_FORMAT_R16G16B16A16_SNORM, VSF_TANGENTS,  8, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const D3D11_INPUT_ELEMENT_DESC VertexDecl_Q4F[] =
{
	{ "TANGENT"     , 0, DXGI_FORMAT_R32G32B32A32_FLOAT, VSF_QTANGENTS, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const D3D11_INPUT_ELEMENT_DESC VertexDecl_Q4S[] =
{
	{ "TANGENT"     , 0, DXGI_FORMAT_R16G16B16A16_SNORM, VSF_QTANGENTS, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const D3D11_INPUT_ELEMENT_DESC VertexDecl_N3F[] =
{
	{ "NORMAL"      , 0, DXGI_FORMAT_R32G32B32_FLOAT   , VSF_NORMALS  , 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const D3D11_INPUT_ELEMENT_DESC VertexDecl_W4B_I4S[] =
{
	{ "BLENDWEIGHT" , 0, DXGI_FORMAT_R8G8B8A8_UNORM    , VSF_HWSKIN_INFO, offsetof(SVF_W4B_I4S, weights), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "BLENDINDICES", 0, DXGI_FORMAT_R16G16B16A16_SINT , VSF_HWSKIN_INFO, offsetof(SVF_W4B_I4S, indices), D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const D3D11_INPUT_ELEMENT_DESC VertexDecl_V3F[] =
{
	{ "POSITION"    , 3, DXGI_FORMAT_R32G32B32_FLOAT   , VSF_VERTEX_VELOCITY, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const D3D11_INPUT_ELEMENT_DESC VertexDecl_W2F[] =
{
	{ "BLENDWEIGHT" , 1, DXGI_FORMAT_R32G32_FLOAT      , VSF_MORPHBUDDY_WEIGHTS, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

static const D3D11_INPUT_ELEMENT_DESC VertexDecl_V4Fi[] =
{
	{ "TEXCOORD"    , 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 3, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 }
};

static const struct
{
	size_t numDescs;
	const D3D11_INPUT_ELEMENT_DESC* inputDescs;
}
VertexDecls[EDefaultInputLayouts::PreAllocated] =
{
	{ 0, VertexDecl_Empty            },

	// Base stream
	{ CRY_ARRAY_COUNT(VertexDecl_P3F_C4B_T2F     ), VertexDecl_P3F_C4B_T2F      },
	{ CRY_ARRAY_COUNT(VertexDecl_P3S_C4B_T2S     ), VertexDecl_P3S_C4B_T2S      },
	{ CRY_ARRAY_COUNT(VertexDecl_P3S_N4B_C4B_T2S ), VertexDecl_P3S_N4B_C4B_T2S  },

	{ CRY_ARRAY_COUNT(VertexDecl_P3F_C4B_T4B_N3F2), VertexDecl_P3F_C4B_T4B_N3F2 },
	{ CRY_ARRAY_COUNT(VertexDecl_TP3F_C4B_T2F    ), VertexDecl_TP3F_C4B_T2F     },
	{ CRY_ARRAY_COUNT(VertexDecl_P3F_T3F         ), VertexDecl_P3F_T3F          },
	{ CRY_ARRAY_COUNT(VertexDecl_P3F_T2F_T3F     ), VertexDecl_P3F_T2F_T3F      },

	{ CRY_ARRAY_COUNT(VertexDecl_P3F             ), VertexDecl_P3F              },

	{ CRY_ARRAY_COUNT(VertexDecl_P2S_N4B_C4B_T1F ), VertexDecl_P2S_N4B_C4B_T1F  },
	{ CRY_ARRAY_COUNT(VertexDecl_P3F_C4B_T2S     ), VertexDecl_P3F_C4B_T2S      },

	// Additional streams
	{ CRY_ARRAY_COUNT(VertexDecl_T4F_B4F         ), VertexDecl_T4F_B4F          },
	{ CRY_ARRAY_COUNT(VertexDecl_T4S_B4S         ), VertexDecl_T4S_B4S          },
	{ CRY_ARRAY_COUNT(VertexDecl_Q4F             ), VertexDecl_Q4F              },
	{ CRY_ARRAY_COUNT(VertexDecl_Q4S             ), VertexDecl_Q4S              },
	{ CRY_ARRAY_COUNT(VertexDecl_N3F             ), VertexDecl_N3F              },
	{ CRY_ARRAY_COUNT(VertexDecl_W4B_I4S         ), VertexDecl_W4B_I4S          },
	{ CRY_ARRAY_COUNT(VertexDecl_V3F             ), VertexDecl_V3F              },
	{ CRY_ARRAY_COUNT(VertexDecl_W2F             ), VertexDecl_W2F              },

	{ CRY_ARRAY_COUNT(VertexDecl_V4Fi            ), VertexDecl_V4Fi             }
};

void CDeviceObjectFactory::AllocatePredefinedInputLayouts()
{
	ReserveInputLayouts(EDefaultInputLayouts::PreAllocated);

	for (InputLayoutHandle nFormat = EDefaultInputLayouts::Empty; nFormat < EDefaultInputLayouts::PreAllocated; nFormat = InputLayoutHandle::ValueType(uint16(nFormat) + 1U))
	{
		SInputLayout out;
		for (size_t n = 0; n < VertexDecls[uint32(nFormat)].numDescs; n++)
			out.m_Declaration.push_back(VertexDecls[uint32(nFormat)].inputDescs[n]);

		// General vertex stream stride
		out.CalculateStride();
		out.CalculateOffsets();
		out.m_pVertexShader = nullptr;

		InputLayoutHandle r = GetOrCreateInputLayoutHandle(out);
		assert(r == nFormat);
	}

	for (int n = 0; n <= MASK(VSF_NUM); ++n)
	{
		s_InputLayoutPermutations[n][0].resize(EDefaultInputLayouts::PreAllocated);
		s_InputLayoutPermutations[n][1].resize(EDefaultInputLayouts::PreAllocated);
	}

	// NOTE: s_InputLayoutPermutations is entirely empty in the beginning. Subsequent calls to
	// GetOrCreateInputLayoutHandle() with a valid shader blob will retry creating the input layout objects
}

void CDeviceObjectFactory::TrimInputLayouts()
{
	s_InputLayouts.Release(EDefaultInputLayouts::PreAllocated);

	// for simplicity sake, clear lookup table
	for (auto& permutationsForMask : s_InputLayoutPermutations)
		for (auto& permutationsForMorph : permutationsForMask)
			permutationsForMorph.clear();
}

void CDeviceObjectFactory::ReleaseInputLayouts()
{
	s_InputLayouts.Release(EDefaultInputLayouts::Empty);

	// for simplicity sake, clear lookup table
	for (auto& permutationsForMask : s_InputLayoutPermutations)
		for (auto& permutationsForMorph : permutationsForMask)
			permutationsForMorph.clear();
}

InputLayoutHandle CDeviceObjectFactory::GetOrCreateInputLayoutHandle(const SShaderBlob* pVertexShader, int StreamMask, int InstAttrMask, uint32 nUsedAttr, byte Attributes[], const InputLayoutHandle VertexFormat)
{
	bool bMorph = (StreamMask & VSM_MORPHBUDDY) != 0;
	bool bInstanced = (StreamMask & VSM_INSTANCED) != 0;
	bool bInstAttribs = (InstAttrMask != 0);

	int nShared = (bInstAttribs) ? 2 : ((bMorph || bInstanced) ? 1 : 0);
	InputLayoutHandle *pDeclCache = &s_InputLayoutPermutations[StreamMask % MASK(VSF_NUM)][nShared][VertexFormat];

#if CRY_PLATFORM_ORBIS && !defined(CUSTOM_FETCH_SHADERS)
	// TODO: Discuss with PS4 implementer!

	// Orbis shader compiler strips out unused declarations so keep creating until we find a shader that's fully bound
	if (pDeclCache->m_pDeclaration && !pDeclCache->m_pDeclaration->IsFullyBound())
	{
		SAFE_RELEASE(pDeclCache->m_pDeclaration);
	}
#endif

	if (*pDeclCache == InputLayoutHandle::Unspecified)
	{
		*pDeclCache = VertexFormat;

		//	iLog->Log("OnDemandVertexDeclaration %d %d %d (DEBUG test - shouldn't log too often)", StreamMask, VertexFormat, bMorph ? 1 : 0);
		const std::pair<SInputLayout, CDeviceInputLayout*>& baseLayout = LookupInputLayout(VertexFormat);

		if (baseLayout.first.m_pVertexShader != pVertexShader)
		{
			SInputLayout out = baseLayout.first;

			if (StreamMask || bInstAttribs)
			{
				// Create instanced vertex declaration
				if (bInstanced)
				{
					for (size_t n = 0; n < out.m_Declaration.size(); n++)
					{
						D3D11_INPUT_ELEMENT_DESC& elem = out.m_Declaration[n];

						elem.InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
						elem.InstanceDataStepRate = 1;
					}
				}

				// Append additional streams
				for (int n = 1; n < VSF_NUM; n++)
				{
					if (!(StreamMask & (1 << n)))
						continue;

					InputLayoutHandle AttachmentFormat = EDefaultInputLayouts::Unspecified;
					switch (n)
					{
					#ifdef TANG_FLOATS
						case VSF_TANGENTS       : AttachmentFormat = EDefaultInputLayouts::T4F_B4F; break;
						case VSF_QTANGENTS      : AttachmentFormat = EDefaultInputLayouts::Q4F; break;
					#else
						case VSF_TANGENTS       : AttachmentFormat = EDefaultInputLayouts::T4S_B4S; break;
						case VSF_QTANGENTS      : AttachmentFormat = EDefaultInputLayouts::Q4S; break;
					#endif
						case VSF_HWSKIN_INFO    : AttachmentFormat = EDefaultInputLayouts::W4B_I4S; break;
						case VSF_VERTEX_VELOCITY: AttachmentFormat = EDefaultInputLayouts::V3F; break;
						case VSF_NORMALS        : AttachmentFormat = EDefaultInputLayouts::N3F; break;
					}

					const std::pair<SInputLayout, CDeviceInputLayout*>& addLayout = LookupInputLayout(AttachmentFormat);
					for (size_t n = 0; n < addLayout.first.m_Declaration.size(); n++)
						out.m_Declaration.push_back(addLayout.first.m_Declaration[n]);
				}

				// Append morph buddies
				if (bMorph)
				{
					uint32 dwNumWithoutMorph = out.m_Declaration.size();

					for (size_t n = 0; n < dwNumWithoutMorph; n++)
					{
						D3D11_INPUT_ELEMENT_DESC El = out.m_Declaration[n];

						El.InputSlot += VSF_MORPHBUDDY;
						El.SemanticIndex += 8;

						out.m_Declaration.push_back(El);
					}

					const std::pair<SInputLayout, CDeviceInputLayout*>& addLayout = LookupInputLayout(EDefaultInputLayouts::W2F);
					for (size_t n = 0; n < addLayout.first.m_Declaration.size(); n++)
						out.m_Declaration.push_back(addLayout.first.m_Declaration[n]);
				}

				// Append instance data
				if (bInstAttribs)
				{
					const int nInstOffs = 1;

					const std::pair<SInputLayout, CDeviceInputLayout*>& addLayout = LookupInputLayout(EDefaultInputLayouts::V4Fi);
					for (int i = 0; i < nUsedAttr; i++)
					{
						for (size_t n = 0; n < addLayout.first.m_Declaration.size(); n++)
						{
							D3D11_INPUT_ELEMENT_DESC El = addLayout.first.m_Declaration[n];

							El.AlignedByteOffset = i * addLayout.first.m_Stride;
							El.SemanticIndex = Attributes[i] + nInstOffs;

							out.m_Declaration.push_back(El);
						}
					}
				}
			}

			// General vertex stream stride
			out.CalculateStride();
			out.CalculateOffsets();
			out.m_pVertexShader = pVertexShader;

			*pDeclCache = GetOrCreateInputLayoutHandle(out);
			assert(*pDeclCache == VertexFormat || *pDeclCache >= EDefaultInputLayouts::PreAllocated);
		}
	}

	// NOTE: s_InputLayoutPermutations is entirely empty in the beginning. Subsequent calls to
	// GetOrCreateInputLayoutHandle() with a valid shader blob will retry creating the input layout objects

	assert(pVertexShader);
	assert(LookupInputLayout(*pDeclCache).first.m_pVertexShader);

	return *pDeclCache;
}

InputLayoutHandle CDeviceObjectFactory::GetOrCreateInputLayoutHandle(const SShaderBlob* pVertexShader, size_t numDescs, const D3D11_INPUT_ELEMENT_DESC* inputLayout)
{
	SInputLayout out;
	for (int n = 0; n < numDescs; ++n)
		out.m_Declaration.push_back(inputLayout[n]);

	// General vertex stream stride
	out.CalculateStride();
	out.CalculateOffsets();
	out.m_pVertexShader = pVertexShader;

	InputLayoutHandle newVF = GetOrCreateInputLayoutHandle(out);

	for (int n = 0; n <= MASK(VSF_NUM); ++n)
	{
		s_InputLayoutPermutations[n][0].resize(uint32(newVF) + 1U);
		s_InputLayoutPermutations[n][1].resize(uint32(newVF) + 1U);
	}

	return newVF;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDeviceGraphicsPSODesc::CDeviceGraphicsPSODesc(CDeviceResourceLayoutPtr pResourceLayout, const SGraphicsPipelineStateDescription& pipelineDesc)
{
	InitWithDefaults();

	m_pResourceLayout = pResourceLayout;
	m_pShader = reinterpret_cast<::CShader*>(pipelineDesc.shaderItem.m_pShader);
	if (auto pTech = m_pShader->GetTechnique(pipelineDesc.shaderItem.m_nTechnique, pipelineDesc.technique, true))
	{
		m_technique = pTech->m_NameCRC;
		m_RenderState = pTech->m_Passes[0].m_RenderState;
	}

	m_ShaderFlags_RT = pipelineDesc.objectRuntimeMask;
	m_ShaderFlags_MDV = pipelineDesc.objectFlags_MDV;
	m_VertexFormat = pipelineDesc.vertexFormat;
	m_ObjectStreamMask = pipelineDesc.streamMask;
	m_PrimitiveType = (ERenderPrimitiveType)pipelineDesc.primitiveType;
}

CDeviceGraphicsPSODesc::CDeviceGraphicsPSODesc()
{
	InitWithDefaults();
}

void CDeviceGraphicsPSODesc::InitWithDefaults()
{
	ZeroStruct(*this);

	m_StencilState = STENC_FUNC(FSS_STENCFUNC_ALWAYS) | STENCOP_FAIL(FSS_STENCOP_KEEP) | STENCOP_ZFAIL(FSS_STENCOP_KEEP) | STENCOP_PASS(FSS_STENCOP_KEEP);
	m_StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	m_StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	m_VertexFormat = EDefaultInputLayouts::P3F_C4B_T2S;
	m_CullMode = eCULL_Back;
	m_PrimitiveType = eptTriangleList;
	m_bDepthClip = true;
	m_bDynamicDepthBias = false;
}

CDeviceGraphicsPSODesc& CDeviceGraphicsPSODesc::operator=(const CDeviceGraphicsPSODesc& other)
{
	// increment ref counts
	m_pRenderPass = other.m_pRenderPass;
	m_pResourceLayout = other.m_pResourceLayout;
	m_pShader = other.m_pShader;

	// NOTE: need to make sure both structs are binary equivalent for hashing and comparison
	memcpy(this, &other, sizeof(CDeviceGraphicsPSODesc));

	return *this;
}

CDeviceGraphicsPSODesc::CDeviceGraphicsPSODesc(const CDeviceGraphicsPSODesc& other)
{
	*this = other;
}

bool CDeviceGraphicsPSODesc::operator==(const CDeviceGraphicsPSODesc& other) const
{
	return memcmp(this, &other, sizeof(CDeviceGraphicsPSODesc)) == 0;
}

uint64 CDeviceGraphicsPSODesc::GetHash() const
{
	uint64 key = XXH64(this, sizeof(CDeviceGraphicsPSODesc), 0);
	return key;
}

void CDeviceGraphicsPSODesc::FillDescs(D3D11_RASTERIZER_DESC& rasterizerDesc, D3D11_BLEND_DESC& blendDesc, D3D11_DEPTH_STENCIL_DESC& depthStencilDesc) const
{
	// 00001110 11111111 11111111 11111111 USED
	// 00001110 11100000 00000000 11111111 BLENDMASK
	// ======== ======== ======== ======== ==================================
	// 00000000 00000000 00000000 00001111 BLSRC
	// 00000000 00000000 00000000 11110000 BLDST
	// 00000000 00000000 00000001 00000000 DEPTHWRITE
	// 00000000 00000000 00000010 00000000 DEPTHTEST
	// 00000000 00000000 00000100 00000000 STENCIL
	// 00000000 00000000 00001000 00000000 ALPHATEST
	// 00000000 00000000 11110000 00000000 NOCOLMASK
	// 00000000 00000011 00000000 00000000 POINT|WIREFRAME
	// 00000000 00011100 00000000 00000000 DEPTHFUNC
	// 00000000 11100000 00000000 00000000 BLENDOP
	// 00001110 00000000 00000000 00000000 BLENDOPALPHA
	const uint32 renderState = m_RenderState;

	ZeroMemory(&rasterizerDesc, sizeof(D3D11_RASTERIZER_DESC));
	ZeroMemory(&blendDesc, sizeof(D3D11_BLEND_DESC));
	ZeroMemory(&depthStencilDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));

	// Fill mode
	rasterizerDesc.DepthClipEnable = m_bDepthClip ? 1 : 0;
	rasterizerDesc.FrontCounterClockwise = 1;
	rasterizerDesc.FillMode = (renderState & GS_WIREFRAME) ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
	rasterizerDesc.CullMode =
	  (m_CullMode == eCULL_Back)
	  ? D3D11_CULL_BACK
	  : ((m_CullMode == eCULL_None) ? D3D11_CULL_NONE : D3D11_CULL_FRONT);

	// scissor is always enabled on DX12.
	rasterizerDesc.ScissorEnable = TRUE;

	// *INDENT-OFF*
	// Blend state
	{
		const bool bBlendEnable = (renderState & GS_BLEND_MASK) != 0;
		blendDesc.RenderTarget[0].BlendEnable = bBlendEnable;

		struct BlendFactors
		{
			D3D11_BLEND BlendColor;
			D3D11_BLEND BlendAlpha;
			bool		bAllowOnAllTargets;
		};

		static BlendFactors SrcBlendFactors[GS_BLSRC_MASK >> GS_BLSRC_SHIFT] =
		{
			{ (D3D11_BLEND)0,             (D3D11_BLEND)0, 					true },        // UNINITIALIZED BLEND FACTOR
			{ D3D11_BLEND_ZERO,           D3D11_BLEND_ZERO,					true },        // GS_BLSRC_ZERO
			{ D3D11_BLEND_ONE,            D3D11_BLEND_ONE,					true },        // GS_BLSRC_ONE
			{ D3D11_BLEND_DEST_COLOR,     D3D11_BLEND_DEST_ALPHA,			true },		   // GS_BLSRC_DSTCOL
			{ D3D11_BLEND_INV_DEST_COLOR, D3D11_BLEND_INV_DEST_ALPHA,		true },        // GS_BLSRC_ONEMINUSDSTCOL
			{ D3D11_BLEND_SRC_ALPHA,      D3D11_BLEND_SRC_ALPHA,			true },        // GS_BLSRC_SRCALPHA
			{ D3D11_BLEND_INV_SRC_ALPHA,  D3D11_BLEND_INV_SRC_ALPHA,		true },        // GS_BLSRC_ONEMINUSSRCALPHA
			{ D3D11_BLEND_DEST_ALPHA,     D3D11_BLEND_DEST_ALPHA,			true },        // GS_BLSRC_DSTALPHA
			{ D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_INV_DEST_ALPHA,		true },        // GS_BLSRC_ONEMINUSDSTALPHA
			{ D3D11_BLEND_SRC_ALPHA_SAT,  D3D11_BLEND_SRC_ALPHA_SAT,		true },        // GS_BLSRC_ALPHASATURATE
			{ D3D11_BLEND_SRC_ALPHA,      D3D11_BLEND_ZERO,					true },        // GS_BLSRC_SRCALPHA_A_ZERO
			{ D3D11_BLEND_SRC1_ALPHA,     D3D11_BLEND_SRC1_ALPHA,			false },       // GS_BLSRC_SRC1ALPHA
		};

		static BlendFactors DstBlendFactors[GS_BLDST_MASK >> GS_BLDST_SHIFT] =
		{
			{ (D3D11_BLEND)0,             (D3D11_BLEND)0,					true },        // UNINITIALIZED BLEND FACTOR
			{ D3D11_BLEND_ZERO,           D3D11_BLEND_ZERO,					true },        // GS_BLDST_ZERO
			{ D3D11_BLEND_ONE,            D3D11_BLEND_ONE,					true },        // GS_BLDST_ONE
			{ D3D11_BLEND_SRC_COLOR,      D3D11_BLEND_SRC_ALPHA,			true },        // GS_BLDST_SRCCOL
			{ D3D11_BLEND_INV_SRC_COLOR,  D3D11_BLEND_INV_SRC_ALPHA,		true },        // GS_BLDST_ONEMINUSSRCCOL
			{ D3D11_BLEND_SRC_ALPHA,      D3D11_BLEND_SRC_ALPHA,			true },        // GS_BLDST_SRCALPHA
			{ D3D11_BLEND_INV_SRC_ALPHA,  D3D11_BLEND_INV_SRC_ALPHA,		true },        // GS_BLDST_ONEMINUSSRCALPHA
			{ D3D11_BLEND_DEST_ALPHA,     D3D11_BLEND_DEST_ALPHA,			true },        // GS_BLDST_DSTALPHA
			{ D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_INV_DEST_ALPHA,		true },        // GS_BLDST_ONEMINUSDSTALPHA
			{ D3D11_BLEND_ONE,            D3D11_BLEND_ZERO,					true },        // GS_BLDST_ONE_A_ZERO
			{ D3D11_BLEND_INV_SRC1_ALPHA, D3D11_BLEND_INV_SRC1_ALPHA,		false },       // GS_BLDST_ONEMINUSSRC1ALPHA
		};

		static D3D11_BLEND_OP BlendOp[GS_BLEND_OP_MASK >> GS_BLEND_OP_SHIFT] =
		{
			D3D11_BLEND_OP_ADD,          // 0 (unspecified): Default
			D3D11_BLEND_OP_MAX,          // GS_BLOP_MAX / GS_BLALPHA_MAX
			D3D11_BLEND_OP_MIN,          // GS_BLOP_MIN / GS_BLALPHA_MIN
			D3D11_BLEND_OP_SUBTRACT,     // GS_BLOP_SUB / GS_BLALPHA_SUB
			D3D11_BLEND_OP_REV_SUBTRACT, // GS_BLOP_SUBREV / GS_BLALPHA_SUBREV
		};

		const bool bSrcBlendAllowAllTargets = SrcBlendFactors[(renderState & GS_BLSRC_MASK) >> GS_BLSRC_SHIFT].bAllowOnAllTargets;
		const bool bDstBlendAllowAllTargets = DstBlendFactors[(renderState & GS_BLDST_MASK) >> GS_BLDST_SHIFT].bAllowOnAllTargets;
		const bool bBlendAllowAllTargets = bSrcBlendAllowAllTargets && bDstBlendAllowAllTargets;

		if (bBlendEnable)
		{			
			blendDesc.RenderTarget[0].SrcBlend       = SrcBlendFactors[(renderState & GS_BLSRC_MASK) >> GS_BLSRC_SHIFT].BlendColor;
			blendDesc.RenderTarget[0].SrcBlendAlpha  = SrcBlendFactors[(renderState & GS_BLSRC_MASK) >> GS_BLSRC_SHIFT].BlendAlpha;
			blendDesc.RenderTarget[0].DestBlend      = DstBlendFactors[(renderState & GS_BLDST_MASK) >> GS_BLDST_SHIFT].BlendColor;
			blendDesc.RenderTarget[0].DestBlendAlpha = DstBlendFactors[(renderState & GS_BLDST_MASK) >> GS_BLDST_SHIFT].BlendAlpha;

			blendDesc.RenderTarget[0].BlendOp      = BlendOp[(renderState & GS_BLEND_OP_MASK) >> GS_BLEND_OP_SHIFT];
			blendDesc.RenderTarget[0].BlendOpAlpha = BlendOp[(renderState & GS_BLALPHA_MASK ) >> GS_BLALPHA_SHIFT ];
			
			if ((renderState & GS_BLALPHA_MASK) == GS_BLALPHA_MIN)
			{
				blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
				blendDesc.RenderTarget[0].SrcBlendAlpha  = D3D11_BLEND_ONE;
			}
		}

		// Copy blend state to support independent blend
		for (size_t i = 0; i < CD3D9Renderer::RT_STACK_WIDTH; ++i)
		{
			blendDesc.RenderTarget[i] = blendDesc.RenderTarget[0];

			// Dual source color blend cannot be enabled for any RT slot but 0
			if (!bBlendAllowAllTargets && i > 0)
			{
				blendDesc.RenderTarget[i].BlendEnable = false;
			}
		}
		blendDesc.IndependentBlendEnable = true;
	}
	
	{ 		
		for (uint32 i = 0; i < 4; ++i)
		{
			uint32 mask = 0xfffffff0 | (ColorMasks[(renderState & GS_COLMASK_MASK) >> GS_COLMASK_SHIFT][i]);
			mask = (~mask) & 0xf;
			blendDesc.RenderTarget[i].RenderTargetWriteMask = mask;
		}	
	}

	// Depth-Stencil
	{
		depthStencilDesc.DepthWriteMask =  (renderState & GS_DEPTHWRITE) ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthEnable    = ((renderState & GS_NODEPTHTEST) && !(renderState & GS_DEPTHWRITE)) ? 0 : 1;

		static D3D11_COMPARISON_FUNC DepthFunc[GS_DEPTHFUNC_MASK >> GS_DEPTHFUNC_SHIFT] =
		{
			D3D11_COMPARISON_LESS_EQUAL,     // GS_DEPTHFUNC_LEQUAL
			D3D11_COMPARISON_EQUAL,          // GS_DEPTHFUNC_EQUAL
			D3D11_COMPARISON_GREATER,        // GS_DEPTHFUNC_GREAT
			D3D11_COMPARISON_LESS,           // GS_DEPTHFUNC_LESS
			D3D11_COMPARISON_GREATER_EQUAL,  // GS_DEPTHFUNC_GEQUAL
			D3D11_COMPARISON_NOT_EQUAL,      // GS_DEPTHFUNC_NOTEQUAL
		};

		depthStencilDesc.DepthFunc =
			(renderState & (GS_NODEPTHTEST|GS_DEPTHWRITE)) == (GS_NODEPTHTEST|GS_DEPTHWRITE)
			? D3D11_COMPARISON_ALWAYS
			: DepthFunc[(m_RenderState & GS_DEPTHFUNC_MASK) >> GS_DEPTHFUNC_SHIFT];

		depthStencilDesc.StencilEnable    = (renderState & GS_STENCIL) ? 1 : 0;
		depthStencilDesc.StencilReadMask  = m_StencilReadMask;
		depthStencilDesc.StencilWriteMask = m_StencilWriteMask;

		depthStencilDesc.FrontFace.StencilFunc        = (D3D11_COMPARISON_FUNC)g_StencilFuncLookup[m_StencilState & FSS_STENCFUNC_MASK];
		depthStencilDesc.FrontFace.StencilFailOp      = (D3D11_STENCIL_OP)g_StencilOpLookup[(m_StencilState & FSS_STENCFAIL_MASK) >> FSS_STENCFAIL_SHIFT];
		depthStencilDesc.FrontFace.StencilDepthFailOp = (D3D11_STENCIL_OP)g_StencilOpLookup[(m_StencilState & FSS_STENCZFAIL_MASK) >> FSS_STENCZFAIL_SHIFT];
		depthStencilDesc.FrontFace.StencilPassOp      = (D3D11_STENCIL_OP)g_StencilOpLookup[(m_StencilState & FSS_STENCPASS_MASK) >> FSS_STENCPASS_SHIFT];
		depthStencilDesc.BackFace                     = depthStencilDesc.FrontFace;

		if (m_StencilState & FSS_STENCIL_TWOSIDED)
		{
			depthStencilDesc.BackFace.StencilFunc        = (D3D11_COMPARISON_FUNC)g_StencilFuncLookup[(m_StencilState & (FSS_STENCFUNC_MASK << FSS_CCW_SHIFT)) >> FSS_CCW_SHIFT];
			depthStencilDesc.BackFace.StencilFailOp      = (D3D11_STENCIL_OP)g_StencilOpLookup[(m_StencilState & (FSS_STENCFAIL_MASK << FSS_CCW_SHIFT)) >> (FSS_STENCFAIL_SHIFT + FSS_CCW_SHIFT)];
			depthStencilDesc.BackFace.StencilDepthFailOp = (D3D11_STENCIL_OP)g_StencilOpLookup[(m_StencilState & (FSS_STENCZFAIL_MASK << FSS_CCW_SHIFT)) >> (FSS_STENCZFAIL_SHIFT + FSS_CCW_SHIFT)];
			depthStencilDesc.BackFace.StencilPassOp      = (D3D11_STENCIL_OP)g_StencilOpLookup[(m_StencilState & (FSS_STENCPASS_MASK << FSS_CCW_SHIFT)) >> (FSS_STENCPASS_SHIFT + FSS_CCW_SHIFT)];
		}
	}
	// *INDENT-ON*
}

uint32 CDeviceGraphicsPSODesc::CombineVertexStreamMasks(uint32 fromShader, uint32 fromObject) const
{
	uint32 result = fromShader;

	if (fromObject & VSM_NORMALS)
		result |= VSM_NORMALS;

	if (fromObject & BIT(VSF_QTANGENTS))
	{
		result &= ~VSM_TANGENTS;
		result |= BIT(VSF_QTANGENTS);
	}

	if (fromObject & VSM_INSTANCED)
		result |= VSM_INSTANCED;

	return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CDeviceComputePSODesc::CDeviceComputePSODesc(CDeviceResourceLayoutPtr pResourceLayout, ::CShader* pShader, const CCryNameTSCRC& technique, uint64 rtFlags, uint32 mdFlags, uint32 mdvFlags)
{
	InitWithDefaults();

	m_pResourceLayout = pResourceLayout;
	m_pShader = pShader;
	m_technique = technique;
	m_ShaderFlags_RT = rtFlags;
	m_ShaderFlags_MD = mdFlags;
	m_ShaderFlags_MDV = mdvFlags;
}

void CDeviceComputePSODesc::InitWithDefaults()
{
	ZeroStruct(*this);
}

CDeviceComputePSODesc& CDeviceComputePSODesc::operator=(const CDeviceComputePSODesc& other)
{
	// increment ref counts
	m_pResourceLayout = other.m_pResourceLayout;
	m_pShader = other.m_pShader;

	// NOTE: need to make sure both structs are binary equivalent for hashing and comparison
	memcpy(this, &other, sizeof(CDeviceComputePSODesc));

	return *this;
}

CDeviceComputePSODesc::CDeviceComputePSODesc(const CDeviceComputePSODesc& other)
{
	*this = other;
}

bool CDeviceComputePSODesc::operator==(const CDeviceComputePSODesc& other) const
{
	return memcmp(this, &other, sizeof(CDeviceComputePSODesc)) == 0;
}

uint64 CDeviceComputePSODesc::GetHash() const
{
	uint64 key = XXH64(this, sizeof(CDeviceComputePSODesc), 0);
	return key;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CDeviceGraphicsPSO::ValidateShadersAndTopologyCombination(const CDeviceGraphicsPSODesc& psoDesc, const std::array<void*, eHWSC_Num>& hwShaderInstances)
{
	const bool bTessellationSupport = hwShaderInstances[eHWSC_Domain] && hwShaderInstances[eHWSC_Hull];

	const bool bControlPointPatchPrimitive =
		(ept1ControlPointPatchList == psoDesc.m_PrimitiveType) ||
		(ept2ControlPointPatchList == psoDesc.m_PrimitiveType) ||
		(ept3ControlPointPatchList == psoDesc.m_PrimitiveType) ||
		(ept4ControlPointPatchList == psoDesc.m_PrimitiveType);
	
	const bool bShadersAndTopologyCombination =
		(!bTessellationSupport && !bControlPointPatchPrimitive) ||
		(bTessellationSupport && bControlPointPatchPrimitive);
	
	return bShadersAndTopologyCombination;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SResourceBinding::IsValid() const
{
	switch (type)
	{
		case EResourceType::ConstantBuffer: return pConstantBuffer && (pConstantBuffer->IsNullBuffer() || pConstantBuffer->GetD3D());
		case EResourceType::Texture:        return pTexture && pTexture->GetDevTexture();
		case EResourceType::Buffer:         return pBuffer && pBuffer->GetDevBuffer();
		case EResourceType::Sampler:        return samplerState != SamplerStateHandle::Unspecified;
		case EResourceType::InvalidType:    return false;
		default: CRY_ASSERT(false);         return false;
	}
}

bool SResourceBinding::IsVolatile() const
{
	switch (type)
	{
		case EResourceType::ConstantBuffer: return true;
		case EResourceType::Texture:        return pTexture && !(pTexture->GetFlags() & FT_DONT_RELEASE);
		case EResourceType::Buffer:         return true;
		case EResourceType::Sampler:        return samplerState == SamplerStateHandle::Unspecified;
		case EResourceType::InvalidType:    return true;
		default: CRY_ASSERT(false);         return true;
	}
}

const std::pair<SResourceView, CDeviceResourceView*>* SResourceBinding::GetDeviceResourceViewInfo() const
{
	CDeviceResource* pDeviceResource = nullptr;

	switch (type)
	{
		case EResourceType::Texture:  pDeviceResource = pTexture->GetDevTexture(); break;
		case EResourceType::Buffer:   pDeviceResource = pBuffer->GetDevBuffer();   break;
		default:                      CRY_ASSERT(false);                           break;
	}

	if (pDeviceResource)
	{
		return &pDeviceResource->LookupResourceView(view);
	}

	return nullptr;
}

template<typename T> T* SResourceBinding::GetDeviceResourceView() const
{
	if (fastCompare)
	{
		if (auto pResourceViewInfo = GetDeviceResourceViewInfo())
		{
			return reinterpret_cast<T*>(pResourceViewInfo->second);
		}
	}

	return nullptr;
}

template D3DSurface*      SResourceBinding::GetDeviceResourceView<D3DSurface>() const;
template D3DDepthSurface* SResourceBinding::GetDeviceResourceView<D3DDepthSurface>() const;

DXGI_FORMAT SResourceBinding::GetResourceFormat() const
{
	if (fastCompare)
	{
		// try to get format from device resource view first
		if (auto pResourceViewInfo = GetDeviceResourceViewInfo())
			return DXGI_FORMAT(pResourceViewInfo->first.m_Desc.nFormat);

		// fall back to resource format
		CRY_ASSERT(view == EDefaultResourceViews::Default || view == EDefaultResourceViews::RasterizerTarget); // only safe with default views
		CRY_ASSERT(type == EResourceType::Texture && pTexture); // TODO implement for buffers

		return DeviceFormats::ConvertFromTexFormat(pTexture->GetDstFormat());
	}

	return DXGI_FORMAT_UNKNOWN;
}

SResourceBindPoint::SResourceBindPoint(const SResourceBinding& resource, uint8 _slotNumber, EShaderStage shaderStages)
{
	slotNumber = _slotNumber;
	stages     = shaderStages;

	switch (resource.type)
	{
		case SResourceBinding::EResourceType::ConstantBuffer:
		{
			slotType = ESlotType::ConstantBuffer;
			flags = EFlags::None;
		}
		break;

		case SResourceBinding::EResourceType::Texture:
		{
			CRY_ASSERT(resource.view == EDefaultResourceViews::Default || resource.pTexture->GetDevTexture());
			const bool bSrv = resource.view == EDefaultResourceViews::Default || SResourceView::IsShaderResourceView(resource.pTexture->GetDevTexture()->LookupResourceView(resource.view).first.m_Desc.Key);

			slotType = bSrv ? ESlotType::TextureAndBuffer : ESlotType::UnorderedAccessView;
			flags = EFlags::IsTexture;
		}
		break;

		case SResourceBinding::EResourceType::Buffer:
		{
			CRY_ASSERT(resource.view == EDefaultResourceViews::Default || resource.pBuffer->GetDevBuffer());
			const bool bSrv = resource.view == EDefaultResourceViews::Default || SResourceView::IsShaderResourceView(resource.pBuffer->GetDevBuffer()->LookupResourceView(resource.view).first.m_Desc.Key);
			const bool bStructured = !!(resource.pBuffer->GetFlags() & CDeviceObjectFactory::USAGE_STRUCTURED);

			slotType  = bSrv        ? ESlotType::TextureAndBuffer    : ESlotType::UnorderedAccessView;
			flags = bStructured ? EFlags::IsStructured           : EFlags::None;
		}
		break;

		case SResourceBinding::EResourceType::Sampler:
		{
			slotType  = ESlotType::Sampler;
			flags = EFlags::None;
		}
		break;
	}
}

SResourceBindPoint::SResourceBindPoint(ESlotType _type, uint8 _slotNumber, EShaderStage _shaderStages, EFlags _flags)
: slotType(_type)
, flags(_flags)
, slotNumber(_slotNumber)
, stages(_shaderStages)
{}

CDeviceResourceSetDesc::EDirtyFlags CDeviceResourceSetDesc::SetResources(const CDeviceResourceSetDesc& other)
{
	CRY_ASSERT(m_resources.size() == 0);
	for (const auto& it : other.m_resources)
	{
		auto insertResult = m_resources.insert(it);

		SResourceBinding&   existingBinding   = insertResult.first->second;
		SResourceBindPoint& existingBindPoint = insertResult.first->first;

		if (m_invalidateCallback)
		{
			it.second.AddInvalidateCallback(m_invalidateCallbackOwner, existingBindPoint, m_invalidateCallback);
		}
	}

	return EDirtyFlags::eDirtyAll;
}

CDeviceResourceSetDesc::CDeviceResourceSetDesc(const CDeviceResourceSetDesc& other)
{
	SetResources(other);

	m_invalidateCallbackOwner = this;
	m_invalidateCallback      = OnResourceInvalidated;
	m_dirtyFlags              = other.m_dirtyFlags.load();
}

CDeviceResourceSetDesc::CDeviceResourceSetDesc(const CDeviceResourceSetDesc& other, void* pInvalidateCallbackOwner, const SResourceBinding::InvalidateCallbackFunction& invalidateCallback)
{
	SetResources(other);

	m_invalidateCallbackOwner = pInvalidateCallbackOwner;
	m_invalidateCallback      = invalidateCallback;
	m_dirtyFlags              = other.m_dirtyFlags.load();
}

CDeviceResourceSetDesc::~CDeviceResourceSetDesc()
{
	ClearResources();
}

template<SResourceBinding::EResourceType resourceType>
bool CompareBindings(const SResourceBinding& resourceA, const SResourceBinding& resourceB)
{
	return resourceA.fastCompare == resourceB.fastCompare && resourceA.view == resourceB.view;
}

template<>
bool CompareBindings<SResourceBinding::EResourceType::ConstantBuffer>(const SResourceBinding& resourceA, const SResourceBinding& resourceB)
{
	return resourceA.fastCompare == resourceB.fastCompare && resourceA.view == resourceB.view;
}

template<>
bool CompareBindings<SResourceBinding::EResourceType::Sampler>(const SResourceBinding& resourceA, const SResourceBinding& resourceB)
{
	return resourceA.samplerState == resourceB.samplerState;
}

template<SResourceBinding::EResourceType resourceType>
CDeviceResourceSetDesc::EDirtyFlags CDeviceResourceSetDesc::UpdateResource(SResourceBindPoint bindPoint, const SResourceBinding& resource)
{
	EDirtyFlags dirtyFlags = EDirtyFlags::eNone;

	auto insertResult = m_resources.insert(std::make_pair(bindPoint, resource));

	SResourceBinding&   existingBinding   = insertResult.first->second;
	SResourceBindPoint& existingBindPoint = insertResult.first->first;

	const bool bChangedBindPoint = (insertResult.second || existingBindPoint.fastCompare != bindPoint.fastCompare);
	if (bChangedBindPoint || existingBinding.type != resource.type || !CompareBindings<resourceType>(existingBinding, resource))
	{
		// remove invalidate callback from existing binding
		if (existingBinding.fastCompare && m_invalidateCallback)
		{
			existingBinding.RemoveInvalidateCallback(m_invalidateCallbackOwner, existingBindPoint);
		}

		existingBindPoint = bindPoint;
		existingBinding   = resource;

		// add invalidate callback to new binding
		if (existingBinding.fastCompare && m_invalidateCallback)
		{
			existingBinding.AddInvalidateCallback(m_invalidateCallbackOwner, existingBindPoint, m_invalidateCallback);
		}

		m_dirtyFlags |= (dirtyFlags = EDirtyFlags::eDirtyBinding | (EDirtyFlags)(EDirtyFlags::eDirtyBindPoint * bChangedBindPoint));
	}
	else
	{
		// UpdateResource call is redundant if the texture stays the same and can't be deleted (read: will never change)
		REDUNDANCY_ASSERT(existingBinding.IsVolatile() || resource.IsVolatile());
	}

	return dirtyFlags;
}

// explicit instantiation
template CDeviceResourceSetDesc::EDirtyFlags CDeviceResourceSetDesc::UpdateResource<SResourceBinding::EResourceType::ConstantBuffer>(SResourceBindPoint bindPoint, const SResourceBinding& binding);
template CDeviceResourceSetDesc::EDirtyFlags CDeviceResourceSetDesc::UpdateResource<SResourceBinding::EResourceType::Texture>(SResourceBindPoint bindPoint, const SResourceBinding& binding);
template CDeviceResourceSetDesc::EDirtyFlags CDeviceResourceSetDesc::UpdateResource<SResourceBinding::EResourceType::Buffer>(SResourceBindPoint bindPoint, const SResourceBinding& binding);
template CDeviceResourceSetDesc::EDirtyFlags CDeviceResourceSetDesc::UpdateResource<SResourceBinding::EResourceType::Sampler>(SResourceBindPoint bindPoint, const SResourceBinding& binding);

CDeviceResourceSetDesc::EDirtyFlags CDeviceResourceSetDesc::ClearResources()
{
	EDirtyFlags dirtyFlags = EDirtyFlags::eNone;

	for (const auto& it : m_resources)
	{
		const SResourceBinding& existingBinding = it.second;
		if (existingBinding.fastCompare && m_invalidateCallback)
		{
			existingBinding.RemoveInvalidateCallback(m_invalidateCallbackOwner);
		}
	}

	if (m_resources.size())
	{
		m_resources.clear();

		m_dirtyFlags |= (dirtyFlags = EDirtyFlags::eDirtyAll);
	}

	return dirtyFlags;
}

bool CDeviceResourceSetDesc::OnResourceInvalidated(void* pThis, SResourceBindPoint bindPoint, UResourceReference pResource, uint32 flags) threadsafe
{
	CDeviceResourceSetDesc* pSet = reinterpret_cast<CDeviceResourceSetDesc*>(pThis);
	EDirtyFlags dirtyFlags = EDirtyFlags::eNone;

	// Device-resource views have changed
	if (flags & eDeviceResourceViewDirty)
		dirtyFlags = EDirtyFlags::eDirtyBinding;
	// Device-resource has changed
	if (flags & eDeviceResourceDirty)
		dirtyFlags = EDirtyFlags::eDirtyBinding;
	// Resource has been destructed
	if (flags & eResourceDestroyed)
	{
		auto it = pSet->m_resources.find(bindPoint);
		if (it != pSet->m_resources.end())
		{
			SResourceBinding& existingBinding = it->second;
			UResourceReference pFoundResource = CryInterlockedCompareExchangePointer((void* volatile*)&existingBinding.fastCompare, nullptr, pResource.pAnonymous);

			if (pFoundResource.pAnonymous == pResource.pAnonymous)
			{
				dirtyFlags |= EDirtyFlags::eDirtyBinding;
			}
		}
	}

	if (dirtyFlags != EDirtyFlags::eNone)
		pSet->m_dirtyFlags |= dirtyFlags;

	// Don't keep the pointer and unregister the callback when the resource goes out of scope
	return !(flags & eResourceDestroyed);
}

CDeviceResourceSet::CDeviceResourceSet(EFlags flags)
	: m_bValid(false)
	, m_Flags(flags)
{}

CDeviceResourceSet::~CDeviceResourceSet()
{}

bool CDeviceResourceSet::Update(CDeviceResourceSetDesc& desc)
{
	// If nothing changed, nothing will become valid automagically
	CDeviceResourceSetDesc::EDirtyFlags dirtyFlags = desc.GetDirtyFlags();
	if (dirtyFlags != CDeviceResourceSetDesc::eNone)
	{
		// If it the given changes lead to a valid object, reset the change-flag
		if ((m_bValid = UpdateImpl(desc, dirtyFlags)))
		{
			desc.AcceptAllChanges();
		}
		else
		{
			desc.AcceptChangedBindPoints();
		}
	}

	return m_bValid;
}

bool CDeviceResourceSet::UpdateWithReevaluation(CDeviceResourceSetPtr& pSet, CDeviceResourceSetDesc& desc)
{
	if (!desc.IsEmpty())
	{
		if (pSet == nullptr)
			pSet = GetDeviceObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);

		return pSet->Update(desc);
	}

	pSet = nullptr;
	desc.AcceptAllChanges();

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SDeviceResourceLayoutDesc::SetResourceSet(uint32 bindSlot, const CDeviceResourceSetDesc& resourceSet)
{
	SLayoutBindPoint bindPoint = { SDeviceResourceLayoutDesc::ELayoutSlotType::ResourceSet, bindSlot };
	m_resourceBindings[bindPoint] = resourceSet.GetResources();
}

void SDeviceResourceLayoutDesc::SetConstantBuffer(uint32 bindSlot, EConstantBufferShaderSlot shaderSlot, ::EShaderStage shaderStages)
{
	SResourceBinding resource((CConstantBuffer*)nullptr, 0);
	SResourceBindPoint resourceBindPoint(resource, shaderSlot, shaderStages);
	SLayoutBindPoint layoutBindPoint = { SDeviceResourceLayoutDesc::ELayoutSlotType::InlineConstantBuffer, bindSlot };

	m_resourceBindings[layoutBindPoint].clear();
	m_resourceBindings[layoutBindPoint].insert(std::make_pair(resourceBindPoint, resource));
}

bool SDeviceResourceLayoutDesc::operator<(const SDeviceResourceLayoutDesc& other) const
{
	if (m_resourceBindings.size() != other.m_resourceBindings.size())
		return m_resourceBindings.size() < other.m_resourceBindings.size();

	for (auto rs0 = m_resourceBindings.begin(), rs1 = other.m_resourceBindings.begin(); rs0 != m_resourceBindings.end(); ++rs0, ++rs1)
	{
		// compare layout bind slots first
		if (!(rs0->first == rs1->first))
			return rs0->first < rs1->first;

		// now compare bind points for current layout slot
		if (rs0->second.size() != rs1->second.size())
			return rs0->second.size() < rs1->second.size();

		for (auto bp0 = rs0->second.begin(), bp1 = rs1->second.begin(); bp0 != rs0->second.end(); ++bp0, ++bp1)
		{
			// compare bind point
			if (bp0->first.fastCompare != bp1->first.fastCompare)
				return bp0->first.fastCompare < bp1->first.fastCompare;

			// Samplers are always static and injected into the layout, so we need to compare the actual sampler state here
			if (bp0->first.slotType == SResourceBindPoint::ESlotType::Sampler)
			{
				if (bp0->second.samplerState != bp1->second.samplerState)
					return bp0->second.samplerState < bp1->second.samplerState;
			}
		}
	}

	return false;
}

UsedBindSlotSet SDeviceResourceLayoutDesc::GetRequiredResourceBindings() const
{
	UsedBindSlotSet result = 0;

	for (auto& it : m_resourceBindings)
		result[it.first.layoutSlot] = true;

	return result;
}

uint64 SDeviceResourceLayoutDesc::GetHash() const
{
	XXH64_state_t hashState;
	XXH64_reset(&hashState, 0);

	for (auto& itLayoutBinding : m_resourceBindings)
	{
		const SLayoutBindPoint& layoutBindPoint = itLayoutBinding.first;
		XXH64_update(&hashState, &layoutBindPoint, sizeof(layoutBindPoint));

		for (auto itResource : itLayoutBinding.second)
		{
			SResourceBindPoint resourceBindPoint = itResource.first;
			XXH64_update(&hashState, &resourceBindPoint.fastCompare, sizeof(resourceBindPoint.fastCompare));

			const SResourceBinding& resource = itResource.second;
			if (resource.type == SResourceBinding::EResourceType::Sampler)
			{
				auto samplerDesc = GetDeviceObjectFactory().LookupSamplerState(resource.samplerState).first;

				uint32 hashedValues[] =
				{
					samplerDesc.m_nMinFilter, samplerDesc.m_nMagFilter,     samplerDesc.m_nMipFilter,
					samplerDesc.m_nAddressU,  samplerDesc.m_nAddressV,      samplerDesc.m_nAddressW,
					samplerDesc.m_nAnisotropy, samplerDesc.m_dwBorderColor, samplerDesc.m_bComparison,
				};

				XXH64_update(&hashState, hashedValues, sizeof(hashedValues));
			}
		}
	}

	return XXH64_digest(&hashState);
}

bool SDeviceResourceLayoutDesc::IsValid() const
{
	// VALIDATION RULES:
	// * Cannot bind multiple resources to same layout (CPU side) slot
	// * Cannot have gaps in layout (CPU side) slots
	// * Cannot bind multiple resources to same shader slot
	//   -> Due to dx12, even things like tex0 => (t0, EShaderStage_Vertex), tex1 => (t0, EShaderStage_Pixel) are invalid
	// * There is a restriction on the GpuBuffer count per resource set. check 'ResourceSetBufferCount'

	auto GetResourceName = [](const SResourceBinding& resource)
	{
		switch (resource.type)
		{
			case SResourceBinding::EResourceType::Texture:        return resource.pTexture->GetName();
			case SResourceBinding::EResourceType::Buffer:         return "GpuBuffer";
			case SResourceBinding::EResourceType::ConstantBuffer: return "ConstantBuffer";
			case SResourceBinding::EResourceType::Sampler:        return "Sampler";
			case SResourceBinding::EResourceType::InvalidType:    return "Void";
		};
		return "Unknown";
	};

	auto GetBindPointName = [](SResourceBindPoint bindPoint)
	{
		static char buffer[64];
		char slotPrefix[] = { 'b', 't', 'u', 's' };

		cry_sprintf(buffer, "%c%d", slotPrefix[min((size_t)bindPoint.slotType, CRY_ARRAY_COUNT(slotPrefix)-1)], bindPoint.slotNumber);

		return buffer;
	};

	std::set<uint32> usedLayoutBindSlots;
	std::map<int16, SResourceBinding> usedShaderBindSlots[(uint8)SResourceBindPoint::ESlotType::Count][eHWSC_Num]; // used slot numbers per slot type and shader stage

	auto validateLayoutSlot = [&](uint32 layoutSlot)
	{
		if (usedLayoutBindSlots.insert(layoutSlot).second == false)
		{
			CRY_ASSERT_TRACE(false, ("Invalid Resource Layout: Multiple resources on layout (CPU side) slot %d", layoutSlot));
			return false;
		}

		return true;
	};

	auto validateResourceBindPoint = [&](SResourceBindPoint bindPoint, const SResourceBinding& resource)
	{
		for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass != eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
		{
			if (bindPoint.stages & SHADERSTAGE_FROM_SHADERCLASS(shaderClass))
			{
				auto insertResult = usedShaderBindSlots[uint8(bindPoint.slotType)][shaderClass].insert(std::make_pair(bindPoint.slotNumber, resource));
				if (insertResult.second == false)
				{
					auto& existingResource = insertResult.first->second;
					auto& currentResource = resource;

					CRY_ASSERT_TRACE(false, ("Invalid Resource Layout : Multiple resources bound to shader slot %s: A: %s - B: %s",
						GetBindPointName(bindPoint), GetResourceName(existingResource), GetResourceName(currentResource)));

					return false;
				}
			}
		}

		return true;
	};

	// validate all resource bindings
	for (auto& itLayoutBinding : m_resourceBindings)
	{
		if (!validateLayoutSlot(itLayoutBinding.first.layoutSlot))
			return false;

		for (auto itResource : itLayoutBinding.second)
		{
			if (!validateResourceBindPoint(itResource.first, itResource.second))
				return false;
		}
	}

	// Make sure there are no 'holes' in the used binding slots
	{
		int previousSlot = -1;
		for (auto slot : usedLayoutBindSlots)
		{
			if (slot != previousSlot + 1)
			{
				CRY_ASSERT_MESSAGE(false, "Invalid Resource Layout: gap in layout (CPU side) slots");
				return false;
			}

			previousSlot = slot;
		}

		if (previousSlot > EResourceLayoutSlot_Max)
			return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeviceCommandList::Close()
{
#if 0
	ERenderListID renderList = EFSLIST_INVALID; // TODO: set to correct renderlist

	#if defined(ENABLE_PROFILING_CODE)
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	CryInterlockedAdd(&rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_nNumPSOSwitches, &m_numPSOSwitches);
	CryInterlockedAdd(&rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_nNumLayoutSwitches, &m_numLayoutSwitches);
	CryInterlockedAdd(&rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_nNumResourceSetSwitches, &m_numResourceSetSwitches);
	CryInterlockedAdd(&rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_nNumInlineSets, &m_numInlineSets);
	CryInterlockedAdd(&rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_nDIPs, &m_numDIPs);
	CryInterlockedAdd(&rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_nPolygons[renderList], &m_numPolygons);
	gcpRendD3D->GetGraphicsPipeline().IncrementNumInvalidDrawcalls(m_numInvalidDIPs);
	#endif
#endif

	CloseImpl();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint64 CDeviceRenderPassDesc::SHash::operator()(const CDeviceRenderPassDesc& renderPassDesc) const
{
	auto addDescToHash = [](XXH64_state_t& hashState, const CDeviceRenderPassDesc& desc)
	{
		uintptr_t resources[MaxRendertargetCount + MaxOutputUAVCount + 1] =
		{
			desc.m_renderTargets[0].fastCompare, desc.m_renderTargets[1].fastCompare, desc.m_renderTargets[2].fastCompare, desc.m_renderTargets[3].fastCompare,
			desc.m_outputUAVs[0].fastCompare,    desc.m_outputUAVs[1].fastCompare,    desc.m_outputUAVs[2].fastCompare,
			desc.m_depthTarget.fastCompare,
		};

		ResourceViewHandle::ValueType views[MaxRendertargetCount + MaxOutputUAVCount + 1] =
		{
			desc.m_renderTargets[0].view, desc.m_renderTargets[1].view, desc.m_renderTargets[2].view, desc.m_renderTargets[3].view,
			desc.m_outputUAVs[0].view,    desc.m_outputUAVs[1].view,    desc.m_outputUAVs[2].view,
			desc.m_depthTarget.view,
		};


		XXH64_update(&hashState, resources, sizeof(resources));
		XXH64_update(&hashState, views, sizeof(views));
	};

	XXH64_state_t hashState;
	XXH64_reset(&hashState, 0x69);

	addDescToHash(hashState, renderPassDesc);

	return XXH64_digest(&hashState);
}

bool CDeviceRenderPassDesc::SEqual::operator()(const CDeviceRenderPassDesc& lhs, const CDeviceRenderPassDesc& rhs) const
{
	return lhs.m_renderTargets == rhs.m_renderTargets &&
	       lhs.m_depthTarget   == rhs.m_depthTarget   &&
	       lhs.m_outputUAVs    == rhs.m_outputUAVs;
}

bool CDeviceRenderPassDesc::SetResources(const CDeviceRenderPassDesc& other)
{
	bool bResourcesInvalidated = false;

	for (size_t slot = 0; slot < m_renderTargets.size(); ++slot)
	{
		const SResourceBindPoint bindPoint(SResourceBindPoint::ESlotType::TextureAndBuffer, uint8(slot), EShaderStage_Pixel);
		bResourcesInvalidated |= UpdateResource(bindPoint, m_renderTargets[slot], other.m_renderTargets[slot]);
	}

	{
		const SResourceBindPoint bindPoint(SResourceBindPoint::ESlotType::TextureAndBuffer, uint8(-1), EShaderStage_Pixel);
		bResourcesInvalidated |= UpdateResource(bindPoint, m_depthTarget, other.m_depthTarget);
	}

	for (size_t slot = 0; slot < m_outputUAVs.size(); ++slot)
	{
		const SResourceBindPoint bindPoint(SResourceBindPoint::ESlotType::UnorderedAccessView, uint8(slot), EShaderStage_Pixel);
		bResourcesInvalidated |= UpdateResource(bindPoint, m_outputUAVs[slot], other.m_outputUAVs[slot]);
	}

	return bResourcesInvalidated;
}

CDeviceRenderPassDesc::CDeviceRenderPassDesc(const CDeviceRenderPassDesc& other)
{
	m_invalidateCallback      = OnResourceInvalidated;
	m_invalidateCallbackOwner = this;

	m_bResourcesInvalidated   = SetResources(other);
}

CDeviceRenderPassDesc::CDeviceRenderPassDesc(const CDeviceRenderPassDesc& other, void* pInvalidateCallbackOwner, const SResourceBinding::InvalidateCallbackFunction& invalidateCallback)
{
	m_invalidateCallback      = invalidateCallback;
	m_invalidateCallbackOwner = pInvalidateCallbackOwner;

	m_bResourcesInvalidated   = SetResources(other);
}

CDeviceRenderPassDesc::~CDeviceRenderPassDesc()
{
	ClearResources();
}

bool CDeviceRenderPassDesc::GetDeviceRendertargetViews(std::array<D3DSurface*, MaxRendertargetCount>& views, int& viewCount) const
{
	viewCount = 0;

	for (auto& rt : m_renderTargets)
	{
		if (!rt.pTexture)
			break;

		ID3D11RenderTargetView* pRenderTargetView = rt.GetDeviceResourceView<ID3D11RenderTargetView>();

		if (!pRenderTargetView)
			return false;

		views[viewCount++] = pRenderTargetView;
	}

	return true;
}

bool CDeviceRenderPassDesc::GetDeviceDepthstencilView(D3DDepthSurface*& pView) const
{
	if (m_depthTarget.pTexture)
	{
		pView = m_depthTarget.GetDeviceResourceView<ID3D11DepthStencilView>();

		if (!pView)
			return false;
	}
	else
	{
		pView = nullptr;
	}
	
	return true;
}

bool CDeviceRenderPassDesc::SetRenderTarget(uint32 slot, CTexture* pTexture, ResourceViewHandle hView)
{
	CRY_ASSERT(slot < MaxRendertargetCount);
	const SResourceBindPoint bindPoint(SResourceBindPoint::ESlotType::TextureAndBuffer, slot, EShaderStage_Pixel);
	bool result = UpdateResource(bindPoint, m_renderTargets[slot], SResourceBinding(pTexture, hView));
	return result;
}

bool CDeviceRenderPassDesc::SetDepthTarget(CTexture* pTexture, ResourceViewHandle hView)
{
	CRY_ASSERT(!pTexture || !m_renderTargets[0].pTexture || (
	           pTexture->GetWidthNonVirtual () == m_renderTargets[0].pTexture->GetWidthNonVirtual () &&
	           pTexture->GetHeightNonVirtual() == m_renderTargets[0].pTexture->GetHeightNonVirtual()));

	const SResourceBindPoint bindPoint(SResourceBindPoint::ESlotType::TextureAndBuffer, -1, EShaderStage_Pixel);
	bool result = UpdateResource(bindPoint, m_depthTarget, SResourceBinding(pTexture, hView));
	return result;
}

bool CDeviceRenderPassDesc::SetOutputUAV(uint32 slot, CGpuBuffer* pBuffer)
{
	CRY_ASSERT(slot < MaxOutputUAVCount);
	const SResourceBindPoint bindPoint(SResourceBindPoint::ESlotType::UnorderedAccessView, slot, EShaderStage_Pixel);
	bool result = UpdateResource(bindPoint, m_outputUAVs[slot], SResourceBinding(pBuffer, EDefaultResourceViews::Default));
	return result;
}

bool CDeviceRenderPassDesc::UpdateResource(SResourceBindPoint bindPoint, SResourceBinding& dstResource, const SResourceBinding& srcResource)
{
	bool bResourcesInvalidated = false;

	if (dstResource.fastCompare != srcResource.fastCompare || dstResource.view != srcResource.view)
	{
		if (dstResource.fastCompare && m_invalidateCallbackOwner)
		{
			dstResource.RemoveInvalidateCallback(m_invalidateCallbackOwner, bindPoint);
		}

		dstResource = srcResource;

		if (srcResource.fastCompare && m_invalidateCallbackOwner)
		{
			srcResource.AddInvalidateCallback(m_invalidateCallbackOwner, bindPoint, m_invalidateCallback);
		}

		m_bResourcesInvalidated = bResourcesInvalidated = true;
	}

	return bResourcesInvalidated;
}

bool CDeviceRenderPassDesc::ClearResources()
{
	bool bResourcesInvalidated = false;

	for (size_t slot = 0; slot < m_renderTargets.size(); ++slot)
	{
		const SResourceBindPoint bindPoint(SResourceBindPoint::ESlotType::TextureAndBuffer, uint8(slot), EShaderStage_Pixel);
		bResourcesInvalidated |= UpdateResource(bindPoint, m_renderTargets[slot], SResourceBinding());
	}

	{
		const SResourceBindPoint bindPoint(SResourceBindPoint::ESlotType::TextureAndBuffer, uint8(-1), EShaderStage_Pixel);
		bResourcesInvalidated |= UpdateResource(bindPoint, m_depthTarget, SResourceBinding());
	}

	for (size_t slot = 0; slot < m_outputUAVs.size(); ++slot)
	{
		const SResourceBindPoint bindPoint(SResourceBindPoint::ESlotType::UnorderedAccessView, uint8(slot), EShaderStage_Pixel);
		bResourcesInvalidated |= UpdateResource(bindPoint, m_outputUAVs[slot], SResourceBinding());
	}

	return bResourcesInvalidated;
}

bool CDeviceRenderPassDesc::OnResourceInvalidated(void* pThis, SResourceBindPoint bindPoint, UResourceReference pResource, uint32 flags) threadsafe
{
	CDeviceRenderPassDesc* pPass = reinterpret_cast<CDeviceRenderPassDesc*>(pThis);
	bool bResourcesInvalidated = false;

	// Device-resource views have changed
	if (flags & eDeviceResourceViewDirty)
		bResourcesInvalidated = true;
	// Device-resource has changed
	if (flags & eDeviceResourceDirty)
		bResourcesInvalidated = true;
	// Resource has been destructed
	if (flags & eResourceDestroyed)
	{
		UResourceReference pFoundResource = (void*)nullptr;

		if (bindPoint.slotType == SResourceBindPoint::ESlotType::UnorderedAccessView)
		{
			pFoundResource = CryInterlockedCompareExchangePointer((void* volatile*)&pPass->m_outputUAVs[bindPoint.slotNumber], nullptr, pResource.pAnonymous);
		}
		else if (bindPoint.slotType == SResourceBindPoint::ESlotType::TextureAndBuffer)
		{
			if ((int8)bindPoint.slotNumber >= 0)
				pFoundResource = CryInterlockedCompareExchangePointer((void* volatile*)&pPass->m_renderTargets[bindPoint.slotNumber], nullptr, pResource.pAnonymous);
			else
				pFoundResource = CryInterlockedCompareExchangePointer((void* volatile*)&pPass->m_depthTarget, nullptr, pResource.pAnonymous);
		}

		if (pFoundResource.pAnonymous == pResource.pAnonymous)
		{
			bResourcesInvalidated = true;
		}
	}

	if (bResourcesInvalidated)
		pPass->m_bResourcesInvalidated = bResourcesInvalidated;

	// Don't keep the pointer and unregister the callback when the resource goes out of scope
	return !(flags & eResourceDestroyed);
}

CDeviceRenderPass_Base::CDeviceRenderPass_Base()
	: m_nHash(0)
	, m_nUpdateCount(0)
	, m_bValid(true)
{}

bool CDeviceRenderPass_Base::Update(const CDeviceRenderPassDesc& passDesc)
{
	// validate resource formats: must not update pass with different layout as this will make PSOs for this pass invalid
#if !defined(RELEASE)
	std::array<DXGI_FORMAT, CDeviceRenderPassDesc::MaxRendertargetCount + 1> currentFormats;

	int formatCount = 0;
	currentFormats[formatCount++] = passDesc.GetDepthTarget().GetResourceFormat();

	for (const auto& target : passDesc.GetRenderTargets())
		currentFormats[formatCount++] = target.GetResourceFormat();

	if (m_nUpdateCount == 0)
	{
		m_targetFormats = currentFormats;
	}

	CRY_ASSERT(m_targetFormats == currentFormats);
#endif

	m_bValid = UpdateImpl(passDesc);
	++m_nUpdateCount;

	const CDeviceRenderPassDesc::SHash hash;
	m_nHash = hash(passDesc);

	return m_bValid;
}

bool CDeviceRenderPass_Base::UpdateWithReevaluation(CDeviceRenderPassPtr& pRenderPass, CDeviceRenderPassDesc& passDesc)
{
	bool bOutputsDirty = passDesc.HasChanged();

	if (bOutputsDirty) // request new render pass in case resource layout has changed
	{
		pRenderPass = GetDeviceObjectFactory().GetOrCreateRenderPass(passDesc);
		if (pRenderPass->Update(passDesc))
		{
			passDesc.AcceptAllChanges();
		}
	}

	else if (!pRenderPass->IsValid()) // make sure render pass is up to date
	{
		if (pRenderPass->Update(passDesc))
		{
			passDesc.AcceptAllChanges();
		}
	}

	return bOutputsDirty;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CDeviceGraphicsPSOPtr CDeviceObjectFactory::CreateGraphicsPSO(const CDeviceGraphicsPSODesc& psoDesc)
{
	CRY_ASSERT(psoDesc.m_pRenderPass != nullptr);

	auto it = m_GraphicsPsoCache.find(psoDesc);
	if (it != m_GraphicsPsoCache.end())
		return it->second;

	auto pPso = CreateGraphicsPSOImpl(psoDesc);
	m_GraphicsPsoCache.emplace(psoDesc, pPso);

	if (!pPso->IsValid())
		m_InvalidGraphicsPsos.emplace(psoDesc, pPso);

	return pPso;
}

CDeviceComputePSOPtr CDeviceObjectFactory::CreateComputePSO(const CDeviceComputePSODesc& psoDesc)
{
	auto it = m_ComputePsoCache.find(psoDesc);
	if (it != m_ComputePsoCache.end())
		return it->second;

	auto pPso = CreateComputePSOImpl(psoDesc);
	m_ComputePsoCache.emplace(psoDesc, pPso);

	if (!pPso->IsValid())
		m_InvalidComputePsos.emplace(psoDesc, pPso);

	return pPso;
}

CDeviceResourceLayoutPtr CDeviceObjectFactory::CreateResourceLayout(const SDeviceResourceLayoutDesc& resourceLayoutDesc)
{
	CDeviceResourceLayoutPtr pResult = nullptr;

	auto it = m_ResourceLayoutCache.find(resourceLayoutDesc);
	if (it != m_ResourceLayoutCache.end())
		return it->second;

	if (resourceLayoutDesc.IsValid())
	{
		if (pResult = CreateResourceLayoutImpl(resourceLayoutDesc))
		{
			m_ResourceLayoutCache[resourceLayoutDesc] = pResult;
		}
	}

	return pResult;
}

void CDeviceObjectFactory::TrimResourceLayouts()
{
	EraseUnusedEntriesFromCache(m_ResourceLayoutCache);
}

const CDeviceInputStream* CDeviceObjectFactory::CreateVertexStreamSet(uint32 numStreams, const SStreamInfo* streams)
{
	TVertexStreams vertexStreams = {};
	bool vertexFilled = false;

	for (int i = 0; i < numStreams; ++i)
		vertexFilled |= !!(vertexStreams[i] = streams[i]);

	return (vertexFilled ? m_uniqueVertexStreams.insert(vertexStreams).first->data() : nullptr);
}

const CDeviceInputStream* CDeviceObjectFactory::CreateIndexStreamSet(const SStreamInfo* stream)
{
	TIndexStreams indexStream = {};
	bool indexFilled = false;

	indexFilled |= !!(indexStream[0] = stream[0]);

	return (indexFilled ? m_uniqueIndexStreams.insert(indexStream).first->data() : nullptr);
}

CDeviceRenderPassPtr CDeviceObjectFactory::GetOrCreateRenderPass(const CDeviceRenderPassDesc& passDesc)
{
	if (auto pRenderPass = GetRenderPass(passDesc))
		return pRenderPass;

	auto pPass = std::make_shared<CDeviceRenderPass>();
	pPass->Update(passDesc);

	m_RenderPassCache.emplace(std::piecewise_construct,
		std::forward_as_tuple(passDesc, pPass.get(), OnRenderPassInvalidated),
		std::forward_as_tuple(pPass));

	return pPass;
}

CDeviceRenderPassPtr CDeviceObjectFactory::GetRenderPass(const CDeviceRenderPassDesc& passDesc)
{
	auto it = m_RenderPassCache.find(passDesc);
	if (it != m_RenderPassCache.end())
		return it->second;

	return nullptr;
}


const CDeviceRenderPassDesc* CDeviceObjectFactory::GetRenderPassDesc(const CDeviceRenderPass_Base* pPass)
{
	for (const auto& it : m_RenderPassCache)
	{
		if (it.second.get() == pPass)
		{
			return &it.first;
		}
	}

	return nullptr;
}

void CDeviceObjectFactory::EraseRenderPass(CDeviceRenderPass* pPass, bool bRemoveInvalidateCallbacks)
{
	for (auto it = m_RenderPassCache.begin(), itEnd = m_RenderPassCache.end(); it != itEnd; ++it)
	{
		if (it->second.get() == pPass)
		{
			if (!bRemoveInvalidateCallbacks)
			{
				auto pPassDesc = const_cast<CDeviceRenderPassDesc*>(&it->first);
				
				pPassDesc->m_invalidateCallbackOwner = nullptr;
				pPassDesc->m_invalidateCallback = nullptr;
			}

			m_RenderPassCache.erase(it);
			break;
		}
	}
}

void CDeviceObjectFactory::TrimRenderPasses()
{
	EraseUnusedEntriesFromCache(m_RenderPassCache);
}

bool CDeviceObjectFactory::OnRenderPassInvalidated(void* pRenderPass, SResourceBindPoint bindPoint, UResourceReference pResource, uint32 flags)
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());

	auto pPass     = reinterpret_cast<CDeviceRenderPass*>(pRenderPass);

	// Don't keep the pointer and unregister the callback when the resource goes out of scope
	if (flags & eResourceDestroyed)
	{
		GetDeviceObjectFactory().EraseRenderPass(pPass, false);
		return false;
	}

	if (flags)
	{
		if (auto pDesc = GetDeviceObjectFactory().GetRenderPassDesc(pPass))
		{
			pPass->Invalidate();
		}
	}

	return true;
}

void CDeviceObjectFactory::TrimResources()
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());

	TrimPipelineStates();
	TrimResourceLayouts();
	TrimRenderPasses();

	for (auto& itLayout : m_ResourceLayoutCache)
	{
		const SDeviceResourceLayoutDesc& desc = itLayout.first;
		for (auto& itLayoutBinding : desc.m_resourceBindings)
		{
			for (auto& itResource : itLayoutBinding.second)
			{
				const SResourceBinding& resource = itResource.second;
				if (resource.type == SResourceBinding::EResourceType::Sampler)
				{
					if (resource.samplerState >= EDefaultSamplerStates::PreAllocated)
						goto trimSamplerStatesDone; // non-preallocated sampler state still in use. cannot trim sampler states at this point
				}
			}
		}
	}

	TrimSamplerStates();

trimSamplerStatesDone:

	bool bCustomInputLayoutsInUse = false;
	for (auto& itPso : m_GraphicsPsoCache)
	{
		const CDeviceGraphicsPSODesc& desc = itPso.first;
		if (desc.m_VertexFormat >= EDefaultInputLayouts::PreAllocated)
		{
			bCustomInputLayoutsInUse = true;
			break;
		}
	}

	if (!bCustomInputLayoutsInUse)
		TrimInputLayouts();
}

void CDeviceObjectFactory::ReleaseResources()
{
	m_GraphicsPsoCache.clear();
	m_InvalidGraphicsPsos.clear();

	m_ComputePsoCache.clear();
	m_InvalidComputePsos.clear();

	m_ResourceLayoutCache.clear();
	m_RenderPassCache.clear();

	ReleaseSamplerStates();
	ReleaseInputLayouts();

	// Unblock/unbind and free live resources
	ReleaseResourcesImpl();
}

void CDeviceObjectFactory::ReloadPipelineStates()
{
	for (auto it = m_GraphicsPsoCache.begin(), itEnd = m_GraphicsPsoCache.end(); it != itEnd; )
	{
		auto itCurrentPSO = it++;
		const CDeviceGraphicsPSO::EInitResult result = itCurrentPSO->second->Init(itCurrentPSO->first);

		if (result != CDeviceGraphicsPSO::EInitResult::Success)
		{
			switch (result)
			{
			case CDeviceGraphicsPSO::EInitResult::Failure:
				m_InvalidGraphicsPsos.emplace(itCurrentPSO->first, itCurrentPSO->second);
				break;
			case CDeviceGraphicsPSO::EInitResult::ErrorShadersAndTopologyCombination:
				m_GraphicsPsoCache.erase(itCurrentPSO);
				break;
			default:
				break;
			}
		}
	}

	for (auto& it : m_ComputePsoCache)
	{
		if (!it.second->Init(it.first))
			m_InvalidComputePsos.emplace(it.first, it.second);
	}
}

void CDeviceObjectFactory::UpdatePipelineStates()
{
	for (auto it = m_InvalidGraphicsPsos.begin(), itEnd = m_InvalidGraphicsPsos.end(); it != itEnd; )
	{
		auto itCurrentPSO = it++;
		auto pPso = itCurrentPSO->second.lock();

		if (!pPso)
		{
			m_InvalidGraphicsPsos.erase(itCurrentPSO);
		}
		else
		{
			const CDeviceGraphicsPSO::EInitResult result = pPso->Init(itCurrentPSO->first);
			if (result != CDeviceGraphicsPSO::EInitResult::Failure)
			{
				if (result == CDeviceGraphicsPSO::EInitResult::ErrorShadersAndTopologyCombination)
				{
					auto find = m_GraphicsPsoCache.find(itCurrentPSO->first);
					if (find != m_GraphicsPsoCache.end())
					{
						m_GraphicsPsoCache.erase(find);
					}
				}

				m_InvalidGraphicsPsos.erase(itCurrentPSO);
			}
		}
	}

	for (auto it = m_InvalidComputePsos.begin(), itEnd = m_InvalidComputePsos.end(); it != itEnd; )
	{
		auto itCurrentPSO = it++;
		auto pPso = itCurrentPSO->second.lock();

		if (!pPso || pPso->Init(itCurrentPSO->first))
			m_InvalidComputePsos.erase(itCurrentPSO);
	}
}

void CDeviceObjectFactory::TrimPipelineStates()
{
	EraseUnusedEntriesFromCache(m_GraphicsPsoCache);
	EraseUnusedEntriesFromCache(m_ComputePsoCache);

	EraseExpiredEntriesFromCache(m_InvalidGraphicsPsos);
	EraseExpiredEntriesFromCache(m_InvalidComputePsos);
}

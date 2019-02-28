// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DeviceObjects.h"
#include "DriverD3D.h"
#include "xxhash.h"
#include "Common/ReverseDepth.h"
#include "Common/Textures/TextureHelpers.h"
#include "../GraphicsPipeline/Common/GraphicsPipelineStateSet.h"
#include "D3D11/DeviceObjects_D3D11.h"


uint8_t SInputLayoutCompositionDescriptor::GenerateShaderMask(const InputLayoutHandle VertexFormat, ID3D11ShaderReflection* pShaderReflection)
{
	uint8_t shaderMask = 0;

	D3D11_SHADER_DESC Desc;
	pShaderReflection->GetDesc(&Desc);

	// layoutDescriptor's names will be ordered in lexicographical ascending order
	const auto* layoutDescriptor = CDeviceObjectFactory::GetInputLayoutDescriptor(VertexFormat);
	if (!layoutDescriptor || !Desc.InputParameters)
		return shaderMask;

	// Read and store lexicographically ordered pointers to reflected names
	std::vector<const char*> reflectedNames;
	reflectedNames.reserve(Desc.InputParameters);
	for (uint32 i=0; i<Desc.InputParameters; i++)
	{
		D3D11_SIGNATURE_PARAMETER_DESC Sig;
		pShaderReflection->GetInputParameterDesc(i, &Sig);
		if (!Sig.SemanticName)
			continue;

		// Insert ordered by element name
		auto it = std::lower_bound(reflectedNames.begin(), reflectedNames.end(), Sig.SemanticName, [](const char* const lhs, const char* const rhs)
		{
			return std::strcmp(lhs, rhs) <= 0;
		});
		reflectedNames.insert(it, Sig.SemanticName);
	}

	// Check which of the names that are expected by the vertex format actually exist as input in the VS
	auto layout_it = layoutDescriptor->m_Declaration.cbegin();
	auto vs_it = reflectedNames.cbegin();
	for (int i = 0; layout_it != layoutDescriptor->m_Declaration.cend() && vs_it != reflectedNames.cend(); ++i, ++layout_it)
	{
		int compResult;
		while (vs_it != reflectedNames.cend() && (compResult = std::strcmp(*vs_it, layout_it->SemanticName)) < 0)
		{
			++vs_it;
		}

		// Match?
		if (compResult == 0)
		{
			shaderMask |= 1 << i;
			++vs_it;
		}
	}

	return shaderMask;
}


CDeviceObjectFactory CDeviceObjectFactory::m_singleton;

CDeviceObjectFactory::~CDeviceObjectFactory()
{
#if (CRY_RENDERER_DIRECT3D >= 120)
	/* No need to do anything */;
#elif (CRY_RENDERER_DIRECT3D >= 110)
	/* Context already deconstructed */;
#elif (CRY_RENDERER_VULKAN >= 10)
	/* No need to do anything */;
#endif
}

bool CDeviceObjectFactory::CanUseCoreCommandList()
{
	return gcpRendD3D->m_pRT->IsRenderThread() && !gcpRendD3D->m_pRT->IsRenderLoadingThread();
}

////////////////////////////////////////////////////////////////////////////
// Fence API (TODO: offload all to CDeviceFenceHandle)

void CDeviceObjectFactory::FlushToGPU(bool bWait, bool bGarbageCollect) const
{
#if (CRY_RENDERER_DIRECT3D >= 120)
	GetDX12Scheduler()->Flush(bWait);
	if (bGarbageCollect)
		GetDX12Scheduler()->GarbageCollect();
#elif (CRY_RENDERER_DIRECT3D >= 110)
	GetDX11Scheduler()->Flush(bWait);
	if (bGarbageCollect)
		GetDX11Scheduler()->GarbageCollect();
#elif (CRY_RENDERER_VULKAN >= 10)
	GetVKScheduler()->Flush(bWait);
	if (bGarbageCollect)
		GetVKScheduler()->GarbageCollect();
#elif (CRY_RENDERER_GNM)
	CGnmGraphicsCommandList* const pCommandList = GnmCommandList(GetCoreCommandList().GetGraphicsInterfaceImpl());
	const SGnmTimeStamp timeStamp = pCommandList->GnmFlush(false);
	if (bWait)
		gGnmDevice->GnmWait(timeStamp);
	/* Garbage collection is automatic (background-thread) */
#else
#pragma message("Missing fall-back FlushToGPU-implementation for the given platform")
#endif
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

std::vector<SInputLayout> CDeviceObjectFactory::m_vertexFormatToInputLayoutCache;
std::unordered_map<SInputLayoutCompositionDescriptor, CDeviceObjectFactory::SInputLayoutPair, SInputLayoutCompositionDescriptor::hasher> CDeviceObjectFactory::s_InputLayoutCompositions;

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
	m_vertexFormatToInputLayoutCache.reserve(EDefaultInputLayouts::PreAllocated);
	for (size_t i = 0; i < EDefaultInputLayouts::PreAllocated; ++i)
		CreateCustomVertexFormat(VertexDecls[i].numDescs, VertexDecls[i].inputDescs);
}

void CDeviceObjectFactory::ReleaseInputLayouts()
{
	m_vertexFormatToInputLayoutCache.clear();
}

void CDeviceObjectFactory::TrimInputLayouts()
{
	m_vertexFormatToInputLayoutCache.erase(m_vertexFormatToInputLayoutCache.begin() + EDefaultInputLayouts::PreAllocated, m_vertexFormatToInputLayoutCache.end());
}

const SInputLayout* CDeviceObjectFactory::GetInputLayoutDescriptor(const InputLayoutHandle VertexFormat)
{
	uint64_t idx = static_cast<uint64_t>(VertexFormat);
	const auto size = m_vertexFormatToInputLayoutCache.size();
	if (idx >= size)
	{
		CryWarning(EValidatorModule::VALIDATOR_MODULE_RENDERER, EValidatorSeverity::VALIDATOR_ERROR, "GetInputLayoutDescriptor(): Supplied handle is neither a predefined format nor was created via CreateCustomVertexFormat()!");
		return nullptr;
	}

	return &m_vertexFormatToInputLayoutCache[static_cast<size_t>(idx)];
}

const CDeviceObjectFactory::SInputLayoutPair* CDeviceObjectFactory::GetOrCreateInputLayout(const SShaderBlob* pVertexShader, int StreamMask, const InputLayoutHandle VertexFormat)
{
	CRY_ASSERT(pVertexShader);
	if (!pVertexShader)
		return nullptr;

	bool bInstanced = (StreamMask & VSM_INSTANCED) != 0;

	// Reflect
	void* pShaderReflBuf;

	{
		CRY_PROFILE_SECTION(PROFILE_RENDERER, "D3DReflect");
		HRESULT hr = D3DReflect(pVertexShader->m_pShaderData, pVertexShader->m_nDataSize, IID_ID3D11ShaderReflection, &pShaderReflBuf);
		CRY_ASSERT(SUCCEEDED(hr) && pShaderReflBuf);
	}

	ID3D11ShaderReflection* pShaderReflection = (ID3D11ShaderReflection*)pShaderReflBuf;

	// Create the composition descriptor
	SInputLayoutCompositionDescriptor compositionDescriptor(VertexFormat, StreamMask, pShaderReflection);

	auto it = s_InputLayoutCompositions.lower_bound(compositionDescriptor);
	if (it == s_InputLayoutCompositions.end() || it->first != compositionDescriptor)
	{
		// Create the input layout for the current permutation
		auto il = CreateInputLayoutForPermutation(pVertexShader, compositionDescriptor, StreamMask, VertexFormat);
		auto deviceInputLayout = CreateInputLayout(il, pVertexShader);

		auto pair = std::make_pair(il, deviceInputLayout);
		// Insert with hint
		return &s_InputLayoutCompositions.insert(it, std::make_pair(compositionDescriptor, pair))->second;
	}

	SAFE_RELEASE(pShaderReflection);

	return &it->second;
}

const CDeviceObjectFactory::SInputLayoutPair* CDeviceObjectFactory::GetOrCreateInputLayout(const SShaderBlob* pVS, const InputLayoutHandle VertexFormat)
{
	return GetOrCreateInputLayout(pVS, 0, VertexFormat);
}

SInputLayout CDeviceObjectFactory::CreateInputLayoutForPermutation(const SShaderBlob* m_pConsumingVertexShader, const SInputLayoutCompositionDescriptor &compositionDescription, int StreamMask, const InputLayoutHandle VertexFormat)
{
	bool bInstanced = (StreamMask & VSM_INSTANCED) != 0;

	const auto* layoutDescriptor = GetInputLayoutDescriptor(VertexFormat);
	CRY_ASSERT(layoutDescriptor);
	if (!layoutDescriptor)
		return SInputLayout({});

	// Copy layout declarations that also exists in shader, using the shaderMask of the composition description
	std::vector<D3D11_INPUT_ELEMENT_DESC> decs;
	decs.reserve(layoutDescriptor->m_Declaration.size());
	for (int i=0; i<layoutDescriptor->m_Declaration.size(); ++i)
			decs.push_back(layoutDescriptor->m_Declaration[i]);

	// Create instanced vertex declaration
	if (bInstanced)
	{
		for (size_t n = 0; n < decs.size(); n++)
		{
			D3D11_INPUT_ELEMENT_DESC& elem = decs[n];

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
		case VSF_TANGENTS: AttachmentFormat = EDefaultInputLayouts::T4F_B4F; break;
		case VSF_QTANGENTS: AttachmentFormat = EDefaultInputLayouts::Q4F; break;
#else
		case VSF_TANGENTS: AttachmentFormat = EDefaultInputLayouts::T4S_B4S; break;
		case VSF_QTANGENTS: AttachmentFormat = EDefaultInputLayouts::Q4S; break;
#endif
		case VSF_HWSKIN_INFO: AttachmentFormat = EDefaultInputLayouts::W4B_I4S; break;
		case VSF_VERTEX_VELOCITY: AttachmentFormat = EDefaultInputLayouts::V3F; break;
		case VSF_NORMALS: AttachmentFormat = EDefaultInputLayouts::N3F; break;
		}

		const auto* addLayout = GetInputLayoutDescriptor(AttachmentFormat);
		CRY_ASSERT(addLayout);
		if (addLayout)
		{
			for (size_t n = 0; n < addLayout->m_Declaration.size(); n++)
				decs.push_back(addLayout->m_Declaration[n]);
		}
	}

	return SInputLayout(std::move(decs));
}

InputLayoutHandle CDeviceObjectFactory::CreateCustomVertexFormat(size_t numDescs, const D3D11_INPUT_ELEMENT_DESC* inputLayout)
{
	std::vector<D3D11_INPUT_ELEMENT_DESC> decs;
	for (int n = 0; n < numDescs; ++n) 
	{
		// Insert ordered by element name
		auto it = std::lower_bound(decs.begin(), decs.end(), inputLayout[n], [](const D3D11_INPUT_ELEMENT_DESC& lhs, const D3D11_INPUT_ELEMENT_DESC& rhs)
		{
			return std::strcmp(lhs.SemanticName, rhs.SemanticName) <= 0;
		});
		decs.insert(it, inputLayout[n]);
	}

	// Store
	auto idx = m_vertexFormatToInputLayoutCache.size();
	m_vertexFormatToInputLayoutCache.emplace_back(std::move(decs));

	return InputLayoutHandle(static_cast<uint8_t>(idx));
}

void CDeviceObjectFactory::EraseRenderPass(CDeviceRenderPass* pPass, bool bRemoveInvalidateCallbacks)
{
	CryAutoCriticalSectionNoRecursive lock(m_RenderPassCacheLock);

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
	CryAutoCriticalSectionNoRecursive lock(m_RenderPassCacheLock);

	EraseUnusedEntriesFromCache(m_RenderPassCache);
}

bool CDeviceObjectFactory::OnRenderPassInvalidated(void* pRenderPass, SResourceBindPoint bindPoint, UResourceReference pResource, uint32 flags)
{
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

	TrimPipelineStates(gRenDev->GetRenderFrameID());
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

	{
		CryAutoCriticalSectionNoRecursive lock(m_RenderPassCacheLock);
		m_RenderPassCache.clear();
	}

	ReleaseSamplerStates();
	ReleaseInputLayouts();

	// Unblock/unbind and free live resources
	ReleaseResourcesImpl();
}

void CDeviceObjectFactory::ReloadPipelineStates(int currentFrameID)
{
	// Throw out expired PSOs before trying to recompile them (saves some time)
	TrimPipelineStates(currentFrameID);

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

	for (auto it = m_ComputePsoCache.begin(), itEnd = m_ComputePsoCache.end(); it != itEnd; )
	{
		auto itCurrentPSO = it++;
		const bool success = itCurrentPSO->second->Init(itCurrentPSO->first);

		if (!success)
			m_InvalidComputePsos.emplace(itCurrentPSO->first, itCurrentPSO->second);
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

void CDeviceObjectFactory::TrimPipelineStates(int currentFrameID, int trimBeforeFrameID)
{
	EraseExpiredEntriesFromCache(m_GraphicsPsoCache, currentFrameID, trimBeforeFrameID);
	EraseExpiredEntriesFromCache(m_ComputePsoCache,  currentFrameID, trimBeforeFrameID);

	EraseExpiredEntriesFromCache(m_InvalidGraphicsPsos);
	EraseExpiredEntriesFromCache(m_InvalidComputePsos);
	// Free Blend, Depth, and Rasterizer State(s)
#if (CRY_RENDERER_DIRECT3D < 120) &&  defined(CRY_RENDERER_DIRECT3D)
	CDeviceStatesManagerDX11::GetInstance()->ReleaseUnusedStates(0 /* kill them all */);
#endif
}

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   RenderPipeline.h : Shaders pipeline declarations.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#ifndef __RENDERPIPELINE_H__
#define __RENDERPIPELINE_H__

#pragma  once

#include <CryThreading/CryThreadSafeRendererContainer.h>
#include <CryThreading/CryThreadSafeWorkerContainer.h>
#include "Shadow_Renderer.h"

#include "LightVolumeBuffer.h"
#include "ParticleBuffer.h"

#include "Common/Shaders/CShader.h"         // CShaderMan
#include "Common/Shaders/ShaderResources.h" // CShaderResources
//====================================================================



class CRenderView;
struct IRenderNode;
struct IRenderMesh;

#define MAX_REND_SHADERS                      4096
#define MAX_REND_SHADER_RES                   16384

struct CRY_ALIGN(32) SRendItem
{
	uint32 SortVal;
	uint32 nBatchFlags;
	union
	{
		uint32 ObjSort;
		float  fDist;
	};
	SRendItemSorter rendItemSorter;
	union
	{
		CRenderObject*         pRenderObject;
		CCompiledRenderObject* pCompiledObject;
	};
	CRenderElement* pElem;
	//uint32 nStencRef  : 8;

	static float EncodeDistanceSortingValue(CRenderObject* pObj)
	{
		return pObj->m_fDistance + pObj->m_fSort;
	}

	static uint32 EncodeObjFlagsSortingValue(CRenderObject* pObj)
	{
		return (pObj->m_ObjFlags & ~0xFFFF) + (pObj->m_nSort & 0xFFFF);
	}

	static bool TestObjFlagsSortingValue(uint32 nFlag, uint32 sortingValue)
	{
		return (nFlag & sortingValue) != 0;
	}

	static inline void ExtractShaderItem(uint32 nVal, SShaderItem& sh)
	{
		CShader* pShader = (CShader*)(CShaderMan::s_pContainer->m_RList[CBaseResource::RListIndexFromId((nVal >> 20) & (MAX_REND_SHADERS - 1))]);
		sh.m_pShader = static_cast<IShader*>(pShader);
		uint32 nTechnique = (nVal & 0x3f);
		if (nTechnique == 0x3f)
			nTechnique = -1;
		sh.m_nTechnique = nTechnique;
		int nRes = (nVal >> 6) & (MAX_REND_SHADER_RES - 1);
		sh.m_pShaderResources = (IRenderShaderResources*)((nRes) ? CShader::s_ShaderResources_known[nRes] : nullptr);
	}

	static inline uint32 PackShaderItem(const SShaderItem& shaderItem)
	{
		uint32 nResID = shaderItem.m_pShaderResources ? ((CShaderResources*)(shaderItem.m_pShaderResources))->m_Id : 0;
		uint32 nShaderId = ((CShader*)shaderItem.m_pShader)->mfGetID();
		assert(nResID < CShader::s_ShaderResources_known.size());
		assert(nShaderId != 0);
		uint32 value = (nResID << 6) | (nShaderId << 20) | (shaderItem.m_nTechnique & 0x3f);
		return value;
	}

	// Sort by SortVal member of RI
	static void mfSortPreprocess(SRendItem* First, int Num);
	// Sort by distance
	static void mfSortByDist(SRendItem* First, int Num, bool bDecals, bool InvertedOrder = false);
	// Sort by light
	static void mfSortByLight(SRendItem* First, int Num, bool bSort, const bool bIgnoreRePtr, bool bSortDecals);
	// Special sorting for ZPass (compromise between depth and batching)
	static void mfSortForZPass(SRendItem* First, int Num);
};

//==================================================================
// StencilStates

//Note: If these are altered, g_StencilFuncLookup and g_StencilOpLookup arrays
//			need to be updated in turn
enum EStencilStateFunction
{
	FSS_STENCFUNC_ALWAYS   = 0x0,
	FSS_STENCFUNC_NEVER    = 0x1,
	FSS_STENCFUNC_LESS     = 0x2,
	FSS_STENCFUNC_LEQUAL   = 0x3,
	FSS_STENCFUNC_GREATER  = 0x4,
	FSS_STENCFUNC_GEQUAL   = 0x5,
	FSS_STENCFUNC_EQUAL    = 0x6,
	FSS_STENCFUNC_NOTEQUAL = 0x7,
	FSS_STENCFUNC_MASK     = 0x7
};

#define FSS_STENCIL_TWOSIDED   0x8

#define FSS_CCW_SHIFT          16

enum EStencilStateOp
{
	FSS_STENCOP_KEEP       = 0x0,
	FSS_STENCOP_REPLACE    = 0x1,
	FSS_STENCOP_INCR       = 0x2,
	FSS_STENCOP_DECR       = 0x3,
	FSS_STENCOP_ZERO       = 0x4,
	FSS_STENCOP_INCR_WRAP  = 0x5,
	FSS_STENCOP_DECR_WRAP  = 0x6,
	FSS_STENCOP_INVERT     = 0x7
};

#define FSS_STENCFAIL_SHIFT    4
#define FSS_STENCFAIL_MASK     (0x7 << FSS_STENCFAIL_SHIFT)

#define FSS_STENCZFAIL_SHIFT   8
#define FSS_STENCZFAIL_MASK    (0x7 << FSS_STENCZFAIL_SHIFT)

#define FSS_STENCPASS_SHIFT    12
#define FSS_STENCPASS_MASK     (0x7 << FSS_STENCPASS_SHIFT)

#define STENC_FUNC(op)        (op)
#define STENC_CCW_FUNC(op)    (op << FSS_CCW_SHIFT)
#define STENCOP_FAIL(op)      (op << FSS_STENCFAIL_SHIFT)
#define STENCOP_ZFAIL(op)     (op << FSS_STENCZFAIL_SHIFT)
#define STENCOP_PASS(op)      (op << FSS_STENCPASS_SHIFT)
#define STENCOP_CCW_FAIL(op)  (op << (FSS_STENCFAIL_SHIFT + FSS_CCW_SHIFT))
#define STENCOP_CCW_ZFAIL(op) (op << (FSS_STENCZFAIL_SHIFT + FSS_CCW_SHIFT))
#define STENCOP_CCW_PASS(op)  (op << (FSS_STENCPASS_SHIFT + FSS_CCW_SHIFT))

//Stencil masks
#define BIT_STENCIL_RESERVED          0x80
#define BIT_STENCIL_INSIDE_CLIPVOLUME 0x40
#define STENCIL_VALUE_OUTDOORS        0x0

#define STENC_VALID_BITS_NUM          7
#define STENC_MAX_REF                 ((1 << STENC_VALID_BITS_NUM) - 1)

//Batch flags.
enum EBatchFlags
{
	FB_GENERAL         = 0x1,
	FB_TRANSPARENT     = 0x2,
	FB_SKIN            = 0x4,
	FB_Z               = 0x8,
	FB_BELOW_WATER     = 0x10,
	FB_ZPREPASS        = 0x20,
	FB_PREPROCESS      = 0x40,
	FB_MOTIONBLUR      = 0x80,
	FB_POST_3D_RENDER  = 0x100,
	FB_MULTILAYERS     = 0x200,
	FB_COMPILED_OBJECT = 0x400,
	FB_CUSTOM_RENDER   = 0x800,
	FB_RESOLVE_FULL    = 0x1000,
	FB_LAYER_EFFECT    = 0x2000,
	FB_WATER_REFL      = 0x4000,
	FB_WATER_CAUSTIC   = 0x8000,
	FB_DEBUG           = 0x10000,
	FB_TILED_FORWARD   = 0x20000,
	FB_REFRACTION      = 0x40000,
	FB_EYE_OVERLAY     = 0x80000,

	FB_MASK            = 0xfffff //! FB flags cannot exceed 0xfffff
};

enum SRenderShaderLightMaskFlags
{
	SLMF_DIRECT = 0,
	SLMF_POINT = 1,
	SLMF_PROJECTED = 2,
	SLMF_TYPE_MASK = (SLMF_POINT | SLMF_PROJECTED)
};

#define SLMF_LTYPE_SHIFT 8
#define SLMF_LTYPE_BITS  4

enum EShapeMeshType
{
	SHAPE_PROJECTOR = 0,
	SHAPE_PROJECTOR1,
	SHAPE_PROJECTOR2,
	SHAPE_CLIP_PROJECTOR,
	SHAPE_CLIP_PROJECTOR1,
	SHAPE_CLIP_PROJECTOR2,
	SHAPE_SIMPLE_PROJECTOR,
	SHAPE_SPHERE,
	SHAPE_BOX,
	SHAPE_MAX,
};


#endif  // __RENDERPIPELINE_H__

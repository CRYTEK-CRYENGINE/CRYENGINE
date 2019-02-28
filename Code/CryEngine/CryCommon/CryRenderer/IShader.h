// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   IShader.h : Shaders common interface.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/
#pragma once

#ifndef _ISHADER_H_
#define _ISHADER_H_

#include <CryCore/smartptr.h>
#include <CryRenderer/IFlares.h> // <> required for Interfuscator
#include <CryRenderer/VertexFormats.h>

#include <CryMath/Cry_XOptimise.h>
#include <CryMemory/CrySizer.h>

#include <CryThreading/CryThreadSafeRendererContainer.h>

#include <CryCore/BitMask.h>

struct IMaterial;
class CRenderElement;
class CRenderObject;
class CREMesh;
struct IRenderMesh;
struct IShader;
struct IVisArea;
class CShader;
class CRenderElement;
class CRenderElement;
struct STexAnim;
struct SShaderPass;
struct SShaderItem;
class ITexture;
struct IMaterial;
struct SParam;
class CMaterial;
struct SShaderSerializeContext;
struct IAnimNode;
struct SSkinningData;
struct SSTexSamplerFX;
struct IClipVolume;

namespace JobManager {
struct SJobState;
}

//! Geometry Culling type.
enum ECull
{
	eCULL_Back = 0, //!< Back culling flag.
	eCULL_Front,    //!< Front culling flag.
	eCULL_None      //!< No culling flag.
};

enum ERenderResource
{
	eRR_Unknown,
	eRR_Mesh,
	eRR_Texture,
	eRR_Shader,
	eRR_ShaderResource,
};

enum EEfResTextures
{
	EFTT_DIFFUSE = 0,
	EFTT_NORMALS,
	EFTT_SPECULAR,
	EFTT_ENV,
	EFTT_DETAIL_OVERLAY,
	EFTT_SMOOTHNESS,
	EFTT_HEIGHT,
	EFTT_DECAL_OVERLAY,
	EFTT_SUBSURFACE,
	EFTT_CUSTOM,
	EFTT_CUSTOM_SECONDARY,
	EFTT_OPACITY,
	EFTT_TRANSLUCENCY,
	EFTT_EMITTANCE,

	EFTT_MAX,
	EFTT_UNKNOWN = EFTT_MAX
};

enum EEfResSamplers
{
	EFSS_ANISO_HIGH = 0,
	EFSS_ANISO_LOW,
	EFSS_TRILINEAR,
	EFSS_BILINEAR,
	EFSS_TRILINEAR_CLAMP,
	EFSS_BILINEAR_CLAMP,
	EFSS_ANISO_HIGH_BORDER,
	EFSS_TRILINEAR_BORDER,

	EFSS_MAX,
};

//=========================================================================

//! Array Pointers for Shaders.
enum ESrcPointer
{
	eSrcPointer_Unknown,
	eSrcPointer_Vert,
	eSrcPointer_Color,
	eSrcPointer_Tex,
	eSrcPointer_TexLM,
	eSrcPointer_Normal,
	eSrcPointer_Tangent,
	eSrcPointer_Max,
};

struct SWaveForm;
struct SWaveForm2;

#define FRF_REFRACTIVE 1
// FREE                2
#define FRF_HEAT       4
#define MAX_HEATSCALE  4

#if !defined(MAX_JOINT_AMOUNT)
	#error MAX_JOINT_AMOUNT is not defined
#endif

#if (MAX_JOINT_AMOUNT <= 256)
typedef uint8  JointIdType;
#else
typedef uint16 JointIdType;
#endif

//=========================================================================

enum EParamType
{
	eType_UNKNOWN,
	eType_BYTE,
	eType_BOOL,
	eType_SHORT,
	eType_INT,
	eType_HALF,
	eType_FLOAT,
	eType_STRING,
	eType_FCOLOR,
	eType_VECTOR,
	eType_TEXTURE_HANDLE,
	eType_CAMERA,
};

enum ESamplerType
{
	eSType_UNKNOWN,
	eSType_Sampler,
	eSType_SamplerComp,
};

union UParamVal
{
	uint8    m_Byte;
	bool     m_Bool;
	short    m_Short;
	int      m_Int;
	float    m_Float;
	char*    m_String;
	float    m_Color[4];
	float    m_Vector[3];
	CCamera* m_pCamera;
};

//! In order to facilitate the memory allocation tracking, we're using this class here.
//! Unfortunately this is necessary because we have particular needs that aren't met by the generic string class.
struct SShaderParam
{
	char        m_Name[32];
	EParamType  m_Type;
	UParamVal   m_Value;
	string      m_Script;
	uint8       m_eSemantic;

	inline void Construct()
	{
		memset(&m_Value, 0, sizeof(m_Value));
		m_Type = eType_UNKNOWN;
		m_eSemantic = 0;
		m_Name[0] = 0;
	}
	inline SShaderParam()
	{
		Construct();
	}
	size_t Size()
	{
		size_t nSize = sizeof(*this);
		if (m_Type == eType_STRING)
			nSize += strlen(m_Value.m_String) + 1;

		return nSize;
	}
	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_Script);
		if (m_Type == eType_STRING)
			pSizer->AddObject(m_Value.m_String, strlen(m_Value.m_String) + 1);
	}

	inline void Destroy()
	{
		if (m_Type == eType_STRING)
			delete[] m_Value.m_String;
	}
	inline ~SShaderParam()
	{
		Destroy();
	}
	inline SShaderParam(const SShaderParam& src)
	{
		memcpy(m_Name, src.m_Name, sizeof(m_Name));
		m_Script = src.m_Script;
		m_Type = src.m_Type;
		m_eSemantic = src.m_eSemantic;
		if (m_Type == eType_STRING)
		{
			m_Value.m_String = new char[strlen(src.m_Value.m_String) + 1];
			strcpy(m_Value.m_String, src.m_Value.m_String);
		}
		else
			m_Value = src.m_Value;
	}
	inline SShaderParam& operator=(const SShaderParam& src)
	{
		this->~SShaderParam();
		new(this)SShaderParam(src);
		return *this;
	}

	static bool SetParam(const char* name, DynArrayRef<SShaderParam>* Params, const UParamVal& pr)
	{
		uint32 i;
		for (i = 0; i < (uint32)Params->size(); i++)
		{
			SShaderParam* sp = &(*Params)[i];
			if (!sp || &sp->m_Value == &pr)
				continue;
			if (!stricmp(sp->m_Name, name))
			{
				if (sp->m_Type == eType_STRING)
				{
					delete[] sp->m_Value.m_String;
				}

				switch (sp->m_Type)
				{
				case eType_FLOAT:
					sp->m_Value.m_Float = pr.m_Float;
					break;
				case eType_SHORT:
					sp->m_Value.m_Short = pr.m_Short;
					break;
				case eType_INT:
				case eType_TEXTURE_HANDLE:
					sp->m_Value.m_Int = pr.m_Int;
					break;

				case eType_VECTOR:
					sp->m_Value.m_Vector[0] = pr.m_Vector[0];
					sp->m_Value.m_Vector[1] = pr.m_Vector[1];
					sp->m_Value.m_Vector[2] = pr.m_Vector[2];
					break;

				case eType_FCOLOR:
					sp->m_Value.m_Color[0] = pr.m_Color[0];
					sp->m_Value.m_Color[1] = pr.m_Color[1];
					sp->m_Value.m_Color[2] = pr.m_Color[2];
					sp->m_Value.m_Color[3] = pr.m_Color[3];
					break;

				case eType_STRING:
					{
						char* str = pr.m_String;
						size_t len = strlen(str) + 1;
						sp->m_Value.m_String = new char[len];
						strcpy(sp->m_Value.m_String, str);
					}
					break;
				}
				break;
			}
		}
		if (i == Params->size())
			return false;
		return true;
	}
	static bool GetValue(const char* szName, DynArrayRef<SShaderParam>* Params, float* v, int nID);

	static bool GetValue(uint8 eSemantic, DynArrayRef<SShaderParam>* Params, float* v, int nID);

	inline void CopyValue(const SShaderParam& src)
	{
		if (m_Type == eType_STRING && this != &src)
		{
			delete[] m_Value.m_String;

			if (src.m_Type == eType_STRING)
			{
				m_Value.m_String = strcpy(new char[strlen(src.m_Value.m_String) + 1], src.m_Value.m_String);
				return;
			}
		}

		m_Value = src.m_Value;
	}

	inline void CopyValueNoString(const SShaderParam& src)
	{
		assert(m_Type != eType_STRING && src.m_Type != eType_STRING);

		m_Value = src.m_Value;
	}

	inline void CopyType(const SShaderParam& src)
	{
		m_Type = src.m_Type;
	}
};

//! \cond INTERNAL
//! IShaderPublicParams can be used to hold a collection of the shader public params.
//! Manipulate this collection, and use them during rendering by submit to the SRendParams.
struct IShaderPublicParams : public _i_multithread_reference_target_t
{
	// <interfuscator:shuffle>
	virtual ~IShaderPublicParams() {}
	
	//! Changes number of parameters in collection.
	virtual void SetParamCount(int nParam) = 0;

	//! Retrieves number of parameters in collection.
	virtual int GetParamCount() const = 0;

	//! Retrieves shader public parameter at specified index of the collection.
	virtual SShaderParam&       GetParam(int nIndex) = 0;
	virtual const SShaderParam& GetParam(int nIndex) const = 0;

	//! Retrieves shader public parameter at specified index of the collection.
	virtual SShaderParam*       GetParamByName(const char* pszName) = 0;
	virtual const SShaderParam* GetParamByName(const char* pszName) const = 0;

	virtual SShaderParam*       GetParamBySemantic(uint8 eParamSemantic) = 0;
	virtual const SShaderParam* GetParamBySemantic(uint8 eParamSemantic) const = 0;

	//! Sets a shader parameter (and if doesn't exists, add it to the parameters list).
	virtual void SetParam(const char* pszName, UParamVal& pParam, EParamType nType = eType_FLOAT, uint8 eSemantic = 0) = 0;

	//! Assigns shader public parameter at specified index of the collection.
	virtual void SetParam(int nIndex, const SShaderParam& param) = 0;

	//! Assigns existing shader parameters list.
	virtual void SetShaderParams(const DynArray<SShaderParam>& pParams) = 0;

	//! Adds a new shader public parameter at the end of the collection.
	virtual void AddParam(const SShaderParam& param) = 0;

	//! Removes a shader public parameter
	virtual void RemoveParamByName(const char* pszName) = 0;
	virtual void RemoveParamBySemantic(uint8 eParamSemantic) = 0;

	//! Assigns collection of shader public parameters to the specified render params structure.
	virtual void  AssignToRenderParams(struct SRendParams& rParams) = 0;

	virtual uint8 GetSemanticByName(const char* pszName) = 0;

	//! Gets shader parameters.
	virtual DynArray<SShaderParam>*       GetShaderParams() = 0;
	virtual const DynArray<SShaderParam>* GetShaderParams() const = 0;
	// </interfuscator:shuffle>
};
//! \endcond

//=================================================================================

class CInputLightMaterial
{
public:
	CInputLightMaterial() :
		m_Diffuse(0, 0, 0, 0),
		m_Specular(0, 0, 0, 0),
		m_Emittance(1, 1, 1, 0),
		m_Opacity(0),
		m_Smoothness(0)
	{
		// memset()
		for (int i = 0; i < EFTT_MAX; i++)
			m_Channels[i][0] = 0.0f,
			m_Channels[i][1] = 1.0f;
	}

	//! Scale and bias.
	ColorF m_Channels[EFTT_MAX][2];

	// TODO: these will go away
	ColorF m_Diffuse;
	ColorF m_Specular;
	ColorF m_Emittance;  //!< RGB: Color, Alpha: Intensity (kcd/m2 or kilonits).
	float  m_Opacity;
	float  m_Smoothness;

	inline friend bool operator==(const CInputLightMaterial& m1, const CInputLightMaterial& m2)
	{
		return !memcmp(&m1, &m2, CInputLightMaterial::Size());
	}

	inline static int Size()
	{
		int nSize = sizeof(CInputLightMaterial);
		return nSize;
	}
};

class CTexture;
#include <CryRenderer/ITexture.h>

//! Vertex modificators definitions (must be 16 bit flag).
#define MDV_BENDING           0x100
#define MDV_DET_BENDING       0x200
#define MDV_DET_BENDING_GRASS 0x400
#define MDV_WIND              0x800
#define MDV_DEPTH_OFFSET      0x2000

//! \cond INTERNAL
//! Deformations/Morphing types.
enum EDeformType
{
	eDT_Unknown              = 0,
	eDT_SinWave              = 1,
	eDT_SinWaveUsingVtxColor = 2,
	eDT_Bulge                = 3,
	eDT_Squeeze              = 4,
	eDT_Perlin2D             = 5,
	eDT_Perlin3D             = 6,
	eDT_FromCenter           = 7,
	eDT_Bending              = 8,
	eDT_ProcFlare            = 9,
	eDT_AutoSprite           = 10,
	eDT_Beam                 = 11,
	eDT_FixedOffset          = 12,
};

//! Wave form evaluator flags.
enum EWaveForm
{
	eWF_None,
	eWF_Sin,
	eWF_HalfSin,
	eWF_InvHalfSin,
	eWF_Square,
	eWF_Triangle,
	eWF_SawTooth,
	eWF_InvSawTooth,
	eWF_Hill,
	eWF_InvHill,
};

#define WFF_CLAMP 1
#define WFF_LERP  2

//! Wave form definition.
struct SWaveForm
{
	EWaveForm m_eWFType;
	uint8     m_Flags;

	float     m_Level;
	float     m_Level1;
	float     m_Amp;
	float     m_Amp1;
	float     m_Phase;
	float     m_Phase1;
	float     m_Freq;
	float     m_Freq1;

	SWaveForm(EWaveForm eWFType, float fLevel, float fAmp, float fPhase, float fFreq)
	{
		m_eWFType = eWFType;
		m_Level = m_Level1 = fLevel;
		m_Amp = m_Amp1 = fAmp;
		m_Phase = m_Phase1 = fPhase;
		m_Freq = m_Freq1 = fFreq;
	}

	int Size()
	{
		int nSize = sizeof(SWaveForm);
		return nSize;
	}
	SWaveForm()
	{
		memset(this, 0, sizeof(SWaveForm));
	}
	bool operator==(const SWaveForm& wf) const
	{
		if (m_eWFType == wf.m_eWFType && m_Level == wf.m_Level && m_Amp == wf.m_Amp && m_Phase == wf.m_Phase && m_Freq == wf.m_Freq && m_Level1 == wf.m_Level1 && m_Amp1 == wf.m_Amp1 && m_Phase1 == wf.m_Phase1 && m_Freq1 == wf.m_Freq1 && m_Flags == wf.m_Flags)
			return true;
		return false;
	}

	SWaveForm& operator+=(const SWaveForm& wf)
	{
		m_Level += wf.m_Level;
		m_Level1 += wf.m_Level1;
		m_Amp += wf.m_Amp;
		m_Amp1 += wf.m_Amp1;
		m_Phase += wf.m_Phase;
		m_Phase1 += wf.m_Phase1;
		m_Freq += wf.m_Freq;
		m_Freq1 += wf.m_Freq1;
		return *this;
	}
};

struct SWaveForm2
{
	EWaveForm m_eWFType;

	float     m_Level;
	float     m_Amp;
	float     m_Phase;
	float     m_Freq;

	SWaveForm2()
	{
		memset(this, 0, sizeof(SWaveForm2));
	}
	bool operator==(const SWaveForm2& wf) const
	{
		if (m_eWFType == wf.m_eWFType && m_Level == wf.m_Level && m_Amp == wf.m_Amp && m_Phase == wf.m_Phase && m_Freq == wf.m_Freq)
			return true;
		return false;
	}

	SWaveForm2& operator+=(const SWaveForm2& wf)
	{
		m_Level += wf.m_Level;
		m_Amp += wf.m_Amp;
		m_Phase += wf.m_Phase;
		m_Freq += wf.m_Freq;
		return *this;
	}
};

struct SDeformInfo
{
	EDeformType m_eType;
	SWaveForm2  m_WaveX;
	SWaveForm2  m_WaveY;
	SWaveForm2  m_WaveZ;
	SWaveForm2  m_WaveW;
	float       m_fDividerX;
	float       m_fDividerY;
	float       m_fDividerZ;
	float       m_fDividerW;
	Vec3        m_vNoiseScale;

	SDeformInfo()
	{
		m_eType = eDT_Unknown;
		m_fDividerX = 0.01f;
		m_fDividerY = 0.01f;
		m_fDividerZ = 0.01f;
		m_fDividerW = 0.01f;
		m_vNoiseScale = Vec3(1, 1, 1);
	}

	inline bool operator==(const SDeformInfo& m)
	{
		if (m_eType == m.m_eType &&
		    m_WaveX == m.m_WaveX && m_WaveY == m.m_WaveY &&
		    m_WaveZ == m.m_WaveZ && m_WaveW == m.m_WaveW &&
		    m_vNoiseScale == m.m_vNoiseScale &&
		    m_fDividerX != m.m_fDividerX && m_fDividerY != m.m_fDividerY &&
		    m_fDividerZ != m.m_fDividerZ && m_fDividerW != m.m_fDividerW)
			return true;

		return false;
	}

	int Size()
	{
		return sizeof(SDeformInfo);
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->Add(*this);
	}
};

struct SDetailDecalInfo
{
	Vec4   vTileOffs[2];  //!< xy = tilling, zw = offsets.
	uint16 nRotation[2];
	uint8  nThreshold[2];
	uint8  nDeformation[2];
	uint8  nBlending;
	uint8  nSSAOAmount;

	SDetailDecalInfo()
	{
		vTileOffs[0] = Vec4(1.0f, 1.0f, 0.0f, 0.0f);
		vTileOffs[1] = Vec4(0.5f, 0.5f, 0.5f, 0.5f);
		nThreshold[0] = nThreshold[1] = 0;
		nDeformation[0] = nDeformation[1] = 0;
		nRotation[0] = nRotation[1] = 0;
		nBlending = 128;
		nSSAOAmount = 0;
	}
	inline bool operator==(const SDetailDecalInfo& m)
	{
		if (vTileOffs[0].x == m.vTileOffs[0].x && vTileOffs[0].y == m.vTileOffs[0].y &&
		    vTileOffs[0].z == m.vTileOffs[0].z && vTileOffs[0].w == m.vTileOffs[0].w &&
		    vTileOffs[1].x == m.vTileOffs[1].x && vTileOffs[1].y == m.vTileOffs[1].y &&
		    vTileOffs[1].z == m.vTileOffs[1].z && vTileOffs[1].w == m.vTileOffs[1].w &&
		    nDeformation[0] == m.nDeformation[0] && nDeformation[1] != m.nDeformation[1] &&
		    nRotation[0] == m.nRotation[0] && nRotation[1] != m.nRotation[1] &&
		    nThreshold[0] != m.nThreshold[0] && nThreshold[1] != m.nThreshold[1] &&
		    nBlending != m.nBlending && nSSAOAmount != m.nSSAOAmount)
			return true;
		return false;
	}

	void Reset()
	{
		vTileOffs[0] = Vec4(1.0f, 1.0f, 0.0f, 0.0f);
		vTileOffs[1] = Vec4(0.5f, 0.5f, 0.5f, 0.5f);
		nThreshold[0] = nThreshold[1] = 0;
		nDeformation[0] = nDeformation[1] = 0;
		nRotation[0] = nRotation[1] = 0;
		nBlending = 128;
		nSSAOAmount = 0;
	}

	int Size()
	{
		return sizeof(SDetailDecalInfo);
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->Add(*this);
	}
};

struct SSkyInfo
{
	ITexture* m_SkyBox[3];
	float     m_fSkyLayerHeight;

	int       Size()
	{
		int nSize = sizeof(SSkyInfo);
		return nSize;
	}
	SSkyInfo()
	{
		memset(this, 0, sizeof(SSkyInfo));
	}
};

enum EResClassName
{
	eRCN_Texture,
	eRCN_Shader,
};
//! \endcond

#include "IRenderer.h"

//==============================================================================

//! Color operations flags.
enum EColorOp
{
	eCO_NOSET                     = 0,
	eCO_DISABLE                   = 1,
	eCO_REPLACE                   = 2,
	eCO_DECAL                     = 3,
	eCO_ARG2                      = 4,
	eCO_MODULATE                  = 5,
	eCO_MODULATE2X                = 6,
	eCO_MODULATE4X                = 7,
	eCO_BLENDDIFFUSEALPHA         = 8,
	eCO_BLENDTEXTUREALPHA         = 9,
	eCO_DETAIL                    = 10,
	eCO_ADD                       = 11,
	eCO_ADDSIGNED                 = 12,
	eCO_ADDSIGNED2X               = 13,
	eCO_MULTIPLYADD               = 14,
	eCO_BUMPENVMAP                = 15,
	eCO_BLEND                     = 16,
	eCO_MODULATEALPHA_ADDCOLOR    = 17,
	eCO_MODULATECOLOR_ADDALPHA    = 18,
	eCO_MODULATEINVALPHA_ADDCOLOR = 19,
	eCO_MODULATEINVCOLOR_ADDALPHA = 20,
	eCO_DOTPRODUCT3               = 21,
	eCO_LERP                      = 22,
	eCO_SUBTRACT                  = 23,
};

enum EColorArg
{
	eCA_Unknown,
	eCA_Specular,
	eCA_Texture,
	eCA_Texture1,
	eCA_Normal,
	eCA_Diffuse,
	eCA_Previous,
	eCA_Constant,
};

#define DEF_TEXARG0 (eCA_Texture | (eCA_Diffuse << 3))
#define DEF_TEXARG1 (eCA_Texture | (eCA_Previous << 3))

enum ETexModRotateType
{
	ETMR_NoChange,
	ETMR_Fixed,
	ETMR_Constant,
	ETMR_Oscillated,
	ETMR_Max
};

enum ETexModMoveType
{
	ETMM_NoChange,
	ETMM_Fixed,
	ETMM_Constant,
	ETMM_Jitter,
	ETMM_Pan,
	ETMM_Stretch,
	ETMM_StretchRepeat,
	ETMM_Max
};

enum ETexGenType
{
	ETG_Stream,
	ETG_World,
	ETG_Camera,
	ETG_Max
};

#define CASE_TEXMOD(var_name)             \
  if (!stricmp( # var_name, szParamName)) \
  {                                       \
    var_name = fValue;                    \
    return true;                          \
  }                                       \

#define CASE_TEXMODANGLE(var_name)        \
  if (!stricmp( # var_name, szParamName)) \
  {                                       \
    var_name = Degr2Word(fValue);         \
    return true;                          \
  }                                       \

#define CASE_TEXMODBYTE(var_name)         \
  if (!stricmp( # var_name, szParamName)) \
  {                                       \
    var_name = (uint8)fValue;             \
    return true;                          \
  }                                       \

#define CASE_TEXMODBOOL(var_name)         \
  if (!stricmp( # var_name, szParamName)) \
  {                                       \
    var_name = (fValue == 1.f);           \
    return true;                          \
  }                                       \

struct SEfTexModificator
{
	bool SetMember(const char* szParamName, float fValue)
	{
		CASE_TEXMODBYTE(m_eTGType);
		CASE_TEXMODBYTE(m_eRotType);
		CASE_TEXMODBYTE(m_eMoveType[0]);
		CASE_TEXMODBYTE(m_eMoveType[1]);
		CASE_TEXMODBOOL(m_bTexGenProjected);

		CASE_TEXMOD(m_Tiling[0]);
		CASE_TEXMOD(m_Tiling[1]);
		CASE_TEXMOD(m_Tiling[2]);
		CASE_TEXMOD(m_Offs[0]);
		CASE_TEXMOD(m_Offs[1]);
		CASE_TEXMOD(m_Offs[2]);

		CASE_TEXMODANGLE(m_Rot[0]);
		CASE_TEXMODANGLE(m_Rot[1]);
		CASE_TEXMODANGLE(m_Rot[2]);
		CASE_TEXMODANGLE(m_RotOscRate[0]);
		CASE_TEXMODANGLE(m_RotOscRate[1]);
		CASE_TEXMODANGLE(m_RotOscRate[2]);
		CASE_TEXMODANGLE(m_RotOscAmplitude[0]);
		CASE_TEXMODANGLE(m_RotOscAmplitude[1]);
		CASE_TEXMODANGLE(m_RotOscAmplitude[2]);
		CASE_TEXMODANGLE(m_RotOscPhase[0]);
		CASE_TEXMODANGLE(m_RotOscPhase[1]);
		CASE_TEXMODANGLE(m_RotOscPhase[2]);
		CASE_TEXMOD(m_RotOscCenter[0]);
		CASE_TEXMOD(m_RotOscCenter[1]);
		CASE_TEXMOD(m_RotOscCenter[2]);

		CASE_TEXMOD(m_OscRate[0]);
		CASE_TEXMOD(m_OscRate[1]);
		CASE_TEXMOD(m_OscAmplitude[0]);
		CASE_TEXMOD(m_OscAmplitude[1]);
		CASE_TEXMOD(m_OscPhase[0]);
		CASE_TEXMOD(m_OscPhase[1]);

		return false;
	}

	Matrix44A m_TexGenMatrix;
	Matrix44A m_TexMatrix;

	float     m_Tiling[3];
	float     m_Offs[3];

	float     m_RotOscCenter[3];

	float     m_OscRate[2];
	float     m_OscAmplitude[2];
	float     m_OscPhase[2];

	//! This members are used only during updating of the matrices.
	float  m_LastTime[2];
	float  m_CurrentJitter[2];

	uint16 m_RotOscPhase[3];
	uint16 m_Rot[3];
	uint16 m_RotOscRate[3];
	uint16 m_RotOscAmplitude[3];

	uint8  m_eTGType;
	uint8  m_eRotType;
	uint8  m_eMoveType[2];
	bool   m_bTexGenProjected;

	void   Reset()
	{
		memset(this, 0, sizeof(*this));
		m_Tiling[0] = m_Tiling[1] = 1.0f;
	}
	inline SEfTexModificator()
	{
		Reset();
	}
	inline SEfTexModificator(const SEfTexModificator& m)
	{
		if (&m != this)
		{
			memcpy(this, &m, sizeof(*this));
		}
	}
	SEfTexModificator& operator=(const SEfTexModificator& src)
	{
		if (&src != this)
		{
			this->~SEfTexModificator();
			new(this)SEfTexModificator(src);
		}
		return *this;
	}
	int Size()
	{
		return sizeof(*this);
	}

	inline bool operator!=(const SEfTexModificator& m)
	{
		return memcmp(this, &m, sizeof(*this)) != 0;
	}
};

//////////////////////////////////////////////////////////////////////
struct IRenderTarget
{
	virtual ~IRenderTarget(){}
	virtual void Release() = 0;
	virtual void AddRef() = 0;
};

//==================================================================================================================

//! Resource texture sampler (runtime).
struct STexSamplerRT
{
	union
	{
		CTexture* m_pTex;
		ITexture* m_pITex;
	};

	union
	{
		struct SHRenderTarget* m_pTarget;
		IRenderTarget*         m_pITarget;
	};

	STexAnim*          m_pAnimInfo;
	IDynTextureSource* m_pDynTexSource;

	uint32             m_nTexFlags;
	SamplerStateHandle m_nTexState;

	uint8              m_eTexType; //!< ETEX_Type e.g. eTT_2D or eTT_Cube.
	int8               m_nSamplerSlot;
	int8               m_nTextureSlot;
	bool               m_bGlobal;

	STexSamplerRT()
	{
		m_nTexState = SamplerStateHandle::Unspecified;
		m_pTex = NULL;
		m_eTexType = eTT_2D;
		m_nTexFlags = 0;
		m_pTarget = NULL;
		m_pAnimInfo = NULL;
		m_pDynTexSource = NULL;
		m_nSamplerSlot = -1;
		m_nTextureSlot = -1;
		m_bGlobal = false;
	}
	~STexSamplerRT()
	{
		Cleanup();
	}

	void Cleanup()
	{
		SAFE_RELEASE(m_pITex);
		// TODO: ref counted deleting of m_pAnimInfo & m_pTarget! - CW
		SAFE_RELEASE(m_pDynTexSource);
		SAFE_RELEASE(m_pITarget);
		SAFE_RELEASE(m_pAnimInfo);
	}
	int Size() const
	{
		int nSize = sizeof(*this);
		return nSize;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
	}

	uint32 GetTexFlags() const { return m_nTexFlags; }
	bool   Update();
	void   PostLoad();
	NO_INLINE STexSamplerRT(const STexSamplerRT& src)
	{
		m_pITex = src.m_pITex;
		if (m_pITex)
			m_pITex->AddRef();
		m_pDynTexSource = src.m_pDynTexSource;
		if (m_pDynTexSource)
			m_pDynTexSource->AddRef();
		m_pITarget = src.m_pITarget;
		if (m_pITarget)
			m_pITarget->AddRef();
		m_pAnimInfo = src.m_pAnimInfo;
		if (m_pAnimInfo)
			m_pAnimInfo->AddRef();
		m_eTexType = src.m_eTexType;
		m_nTexFlags = src.m_nTexFlags;
		m_nTexState = src.m_nTexState;
		m_nSamplerSlot = src.m_nSamplerSlot;
		m_nTextureSlot = src.m_nTextureSlot;
		m_bGlobal = src.m_bGlobal;
	}
	NO_INLINE STexSamplerRT& operator=(const STexSamplerRT& src)
	{
		this->~STexSamplerRT();
		new(this)STexSamplerRT(src);
		return *this;
	}
	inline bool operator!=(const STexSamplerRT& m) const
	{
		if (m_pTex != m.m_pTex || m_eTexType != m.m_eTexType || m_nTexFlags != m.m_nTexFlags || m_nTexState != m.m_nTexState)
			return true;
		return false;
	}
};
//! \endcond

//===============================================================================================================================

struct SEfResTextureExt
{
	int32              m_nFrameUpdated;
	int32              m_nUpdateFlags;
	int32              m_nLastRecursionLevel;
	SEfTexModificator* m_pTexModifier;

	SEfResTextureExt()
	{
		m_nFrameUpdated = -1;
		m_nUpdateFlags = 0;
		m_nLastRecursionLevel = 0;
		m_pTexModifier = NULL;
	}
	~SEfResTextureExt()
	{
		Cleanup();
	}
	void Cleanup()
	{
		SAFE_DELETE(m_pTexModifier);
	}
	inline bool operator!=(const SEfResTextureExt& m) const
	{
		if (m_pTexModifier && m.m_pTexModifier)
			return *m_pTexModifier != *m.m_pTexModifier;
		if (!m_pTexModifier && !m.m_pTexModifier)
			return false;
		return true;
	}
	SEfResTextureExt(const SEfResTextureExt& src)
	{
		if (&src != this)
		{
			Cleanup();
			if (src.m_pTexModifier)
			{
				m_pTexModifier = new SEfTexModificator;
				* m_pTexModifier = *src.m_pTexModifier;
			}
			m_nFrameUpdated = -1;
			m_nUpdateFlags = src.m_nUpdateFlags;
			m_nLastRecursionLevel = -1;
		}
	}
	SEfResTextureExt& operator=(const SEfResTextureExt& src)
	{
		if (&src != this)
		{
			Cleanup();
			new(this)SEfResTextureExt(src);
		}
		return *this;
	}

	void CopyTo(SEfResTextureExt* pTo) const
	{
		if (pTo && pTo != this)
		{
			pTo->Cleanup();
			pTo->m_nFrameUpdated = -1;
			pTo->m_nUpdateFlags = m_nUpdateFlags;
			pTo->m_nLastRecursionLevel = -1;
			pTo->m_pTexModifier = NULL;
		}
	}
	inline int Size() const
	{
		int nSize = sizeof(SEfResTextureExt);
		if (m_pTexModifier)
			nSize += m_pTexModifier->Size();
		return nSize;
	}
};

// In order to facilitate the memory allocation tracking, we're using this class here.
// If you don't like it, please write a substitute for all string within the project and use them everywhere.
struct SEfResTexture
{
	string           m_Name;
	bool             m_bUTile;
	bool             m_bVTile;
	signed char      m_Filter;

	STexSamplerRT    m_Sampler;
	SEfResTextureExt m_Ext;

	// Used to ensure that string memory is managed by the same module that created this struct, even if the name is changed from a different module.
	virtual void SetName(const char* szName)
	{
		m_Name = szName;
	}

	void        UpdateForCreate();
	void        Update(int nTSlot, uint32& nMDMask);
	void        UpdateWithModifier(int nTSlot, uint32& nMDMask);

	inline bool operator!=(const SEfResTexture& m) const
	{
		if (stricmp(m_Name.c_str(), m.m_Name.c_str()) != 0 ||
		    m_bUTile != m.m_bUTile ||
		    m_bVTile != m.m_bVTile ||
		    m_Filter != m.m_Filter ||
		    m_Ext != m.m_Ext ||
		    m_Sampler != m.m_Sampler)
			return true;
		return false;
	}
	inline bool IsHasModificators() const
	{
		return (m_Ext.m_pTexModifier != NULL);
	}

	bool IsNeedTexTransform() const
	{
		if (!m_Ext.m_pTexModifier)
			return false;
		if (m_Ext.m_pTexModifier->m_eRotType != ETMR_NoChange || m_Ext.m_pTexModifier->m_eMoveType[0] != ETMM_NoChange || m_Ext.m_pTexModifier->m_eMoveType[1] != ETMM_NoChange)
			return true;
		return false;
	}
	bool IsNeedTexGen() const
	{
		if (!m_Ext.m_pTexModifier)
			return false;
		if (m_Ext.m_pTexModifier->m_eTGType != ETG_Stream)
			return true;
		return false;
	}
	inline float GetTiling(int n) const
	{
		if (!m_Ext.m_pTexModifier)
			return 1.0f;
		return m_Ext.m_pTexModifier->m_Tiling[n];
	}
	inline float GetOffset(int n) const
	{
		if (!m_Ext.m_pTexModifier)
			return 0;
		return m_Ext.m_pTexModifier->m_Offs[n];
	}
	inline SEfTexModificator* AddModificator()
	{
		if (!m_Ext.m_pTexModifier)
			m_Ext.m_pTexModifier = new SEfTexModificator;
		return m_Ext.m_pTexModifier;
	}
	inline SEfTexModificator* GetModificator() const
	{
		if (!m_Ext.m_pTexModifier)
		{
			static SEfTexModificator dummy;
			dummy.Reset();
			return &dummy;
		}

		return m_Ext.m_pTexModifier;
	}
	int Size() const
	{
		int nSize = sizeof(SEfResTexture) - sizeof(STexSamplerRT) - sizeof(SEfResTextureExt);
		nSize += m_Name.size();
		nSize += m_Sampler.Size();
		nSize += m_Ext.Size();

		return nSize;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->Add(*this);
		pSizer->AddObject(m_Name);
		pSizer->AddObject(m_Sampler);
	}
	void Cleanup()
	{
		m_Sampler.Cleanup();
		m_Ext.Cleanup();
	}

	~SEfResTexture()
	{
		Cleanup();
	}

	void Reset()
	{
		m_bUTile = true;
		m_bVTile = true;
		m_Filter = FILTER_NONE;
		SAFE_DELETE(m_Ext.m_pTexModifier);
		m_Ext.m_nFrameUpdated = -1;
	}
	SEfResTexture(const SEfResTexture& src)
	{
		if (&src != this)
		{
			Cleanup();
			m_Sampler = src.m_Sampler;
			m_Ext = src.m_Ext;
			m_Name = src.m_Name;
			m_bUTile = src.m_bUTile;
			m_bVTile = src.m_bVTile;
			m_Filter = src.m_Filter;
		}
	}
	SEfResTexture& operator=(const SEfResTexture& src)
	{
		if (&src != this)
		{
			Cleanup();
			new(this)SEfResTexture(src);
		}
		return *this;
	}
	void CopyTo(SEfResTexture* pTo) const
	{
		if (pTo && pTo != this)
		{
			pTo->Cleanup();
			pTo->m_Sampler = m_Sampler;
			m_Ext.CopyTo(&pTo->m_Ext);
			pTo->m_Name = m_Name;
			pTo->m_bUTile = m_bUTile;
			pTo->m_bVTile = m_bVTile;
			pTo->m_Filter = m_Filter;
		}
	}

	SEfResTexture()
	{
		Reset();
	}
};

struct SBaseShaderResources : public _i_multithread_reference_target_t
{
	DynArray<SShaderParam> m_ShaderParams;
	string                 m_TexturePath;
	const char*            m_szMaterialName;

	float                  m_AlphaRef;
	uint32                 m_ResFlags;

	uint16                 m_SortPrio;

	uint8                  m_FurAmount;
	uint8                  m_VoxelCoverage;
	uint8                  m_HeatAmount;
	uint8                  m_CloakAmount;

	int Size() const
	{
		int nSize = sizeof(SBaseShaderResources) + m_ShaderParams.size() * sizeof(SShaderParam);
		return nSize;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_ShaderParams);
	}
	SBaseShaderResources& operator=(const SBaseShaderResources& src)
	{
		if (&src != this)
		{
			ReleaseParams();
			m_szMaterialName = src.m_szMaterialName;
			m_ResFlags = src.m_ResFlags;
			m_AlphaRef = src.m_AlphaRef;
			m_HeatAmount = src.m_HeatAmount;
			m_FurAmount = src.m_FurAmount;
			m_VoxelCoverage = src.m_VoxelCoverage;
			m_CloakAmount = src.m_CloakAmount;
			m_SortPrio = src.m_SortPrio;
			m_ShaderParams = src.m_ShaderParams;
		}
		return *this;
	}

	SBaseShaderResources()
	{
		m_ResFlags = 0;
		m_AlphaRef = 0;
		m_HeatAmount = 0;
		m_FurAmount = 0;
		m_VoxelCoverage = 255;
		m_CloakAmount = 255;
		m_SortPrio = 0;
		m_szMaterialName = NULL;
	}

	void ReleaseParams()
	{
		m_ShaderParams.clear();
	}
	virtual ~SBaseShaderResources()
	{
		ReleaseParams();
	}
};

struct IRenderShaderResources
{
	// <interfuscator:shuffle>
	virtual void AddRef() const = 0;
	virtual void UpdateConstants(IShader* pSH) = 0;
	virtual void CloneConstants(const IRenderShaderResources* pSrc) = 0;

	// properties
	virtual void          ToInputLM(CInputLightMaterial& lm) = 0;
	virtual void          SetInputLM(const CInputLightMaterial& lm) = 0;

	virtual const ColorF& GetColorValue(EEfResTextures slot) const = 0;
	virtual void          SetColorValue(EEfResTextures slot, const ColorF& color) = 0;

	virtual float         GetStrengthValue(EEfResTextures slot) const = 0;
	virtual void          SetStrengthValue(EEfResTextures slot, float value) = 0;

	// configs
	virtual const float&               GetAlphaRef() const = 0;
	virtual void                       SetAlphaRef(float v) = 0;

	virtual int                        GetResFlags() = 0;
	virtual void                       SetMtlLayerNoDrawFlags(uint8 nFlags) = 0;
	virtual uint8                      GetMtlLayerNoDrawFlags() const = 0;
	virtual SSkyInfo*                  GetSkyInfo() = 0;
	virtual CCamera*                   GetCamera() = 0;
	virtual void                       SetCamera(CCamera* pCam) = 0;
	virtual void                       SetMaterialName(const char* szName) = 0;
	virtual SEfResTexture*             GetTexture(int nSlot) const = 0;
	virtual DynArrayRef<SShaderParam>& GetParameters() = 0;

	virtual ColorF                     GetFinalEmittance() = 0;
	virtual float                      GetVoxelCoverage() = 0;

	virtual ~IRenderShaderResources() {}
	virtual void                    Release() const = 0;
	virtual void                    ConvertToInputResource(struct SInputShaderResources* pDst) = 0;
	virtual IRenderShaderResources* Clone() const = 0;
	virtual void                    SetShaderParams(struct SInputShaderResources* pDst, IShader* pSH) = 0;

	virtual size_t                  GetResourceMemoryUsage(ICrySizer* pSizer) = 0;
	virtual SDetailDecalInfo*       GetDetailDecalInfo() = 0;

	// Set this shader resources as invalid
	virtual void SetInvalid() = 0;
	// Check if shader resource is valid
	virtual bool IsValid() = 0;

	virtual void GetMemoryUsage(ICrySizer* pSizer) const = 0;
	// </interfuscator:shuffle>

	inline bool IsEmissive() const
	{
		// worst: *reinterpret_cast<int32*>(&) > 0x00000000
		// causes value to pass from FPU to CPU registers
		return GetStrengthValue(EFTT_EMITTANCE) > 0.0f;
	}
	inline bool IsTransparent() const
	{
		// worst: *reinterpret_cast<int32*>(&) < 0x3f800000
		// causes value to pass from FPU to CPU registers
		return GetStrengthValue(EFTT_OPACITY) < 1.0f;
	}
	inline bool IsAlphaTested() const
	{
		return GetAlphaRef() > 0.01f /*0.0f*/;
	}
	inline bool IsInvisible() const
	{
		const float o = GetStrengthValue(EFTT_OPACITY);
		const float a = GetAlphaRef();

		return o == 0.0f || a == 1.0f || o <= a;
	}

};

struct SInputShaderResources : public SBaseShaderResources
{
	CInputLightMaterial m_LMaterial;
	SEfResTexture       m_Textures[EFTT_MAX];
	SDeformInfo         m_DeformInfo;
	SDetailDecalInfo    m_DetailDecalInfo;

	int Size() const
	{
		int nSize = SBaseShaderResources::Size() - sizeof(SEfResTexture) * EFTT_MAX;
		nSize += m_TexturePath.size();
		for (int i = 0; i < EFTT_MAX; i++)
		{
			nSize += m_Textures[i].Size();
		}
		return nSize;
	}
	SInputShaderResources& operator=(const SInputShaderResources& src)
	{
		if (&src != this)
		{
			Cleanup();
			SBaseShaderResources::operator=(src);
			m_TexturePath = src.m_TexturePath;
			m_DeformInfo = src.m_DeformInfo;
			m_DetailDecalInfo = src.m_DetailDecalInfo;
			int i;
			for (i = 0; i < EFTT_MAX; i++)
			{
				m_Textures[i] = src.m_Textures[i];
			}
			m_LMaterial = src.m_LMaterial;
		}
		return *this;
	}

	bool IsEmpty(int nTSlot) const
	{
		return m_Textures[nTSlot].m_Name.empty();
	}

protected:
	friend class CRenderer;
	friend class CShaderMan;

	SInputShaderResources()
	{
		for (int i = 0; i < EFTT_MAX; i++)
		{
			m_Textures[i].Reset();
		}
	}

	SInputShaderResources(struct IRenderShaderResources* pSrc)
	{
		for (int i = 0; i < EFTT_MAX; i++)
		{
			m_Textures[i].Reset();
		}
		pSrc->ConvertToInputResource(this);
		m_ShaderParams = pSrc->GetParameters();
	}

	void Cleanup()
	{
		for (int i = 0; i < EFTT_MAX; i++)
		{
			m_Textures[i].Cleanup();
		}
	}
	virtual ~SInputShaderResources()
	{
		Cleanup();
	}
};

typedef _smart_ptr<SInputShaderResources> SInputShaderResourcesPtr;

//===================================================================================
// Shader gen structure (used for automatic shader script generating).

#define SHGF_HIDDEN                1
#define SHGF_PRECACHE              2
#define SHGF_AUTO_PRECACHE         4
#define SHGF_LOWSPEC_AUTO_PRECACHE 8
#define SHGF_RUNTIME               0x10

// 0x1
#define SHGD_TEX_DETAIL       2
#define SHGD_TEX_NORMALS      4
#define SHGD_TEX_ENVCM        8
#define SHGD_TEX_SPECULAR     0x10
#define SHGD_TEX_TRANSLUCENCY 0x20
#define SHGD_TEX_HEIGHT       0x40
#define SHGD_TEX_SUBSURFACE   0x80
// 0x100
// 0x200
#define SHGD_HW_DURANGO           0x400
#define SHGD_HW_ORBIS             0x800
#define SHGD_TEX_CUSTOM           0x1000
#define SHGD_TEX_CUSTOM_SECONDARY 0x2000
#define SHGD_TEX_DECAL            0x4000
#define SHGD_HW_GLES3             0x20000
#define SHGD_USER_ENABLED         0x40000
// 0x80000
#define SHGD_HW_VULKAN             0x100000
#define SHGD_HW_DX10               0x200000
#define SHGD_HW_DX11               0x400000
#define SHGD_HW_GL4                0x800000
#define SHGD_HW_WATER_TESSELLATION 0x1000000
#define SHGD_HW_SILHOUETTE_POM     0x2000000
#define SHGD_TEX_MASK              (SHGD_TEX_DETAIL | SHGD_TEX_NORMALS | SHGD_TEX_ENVCM | SHGD_TEX_SPECULAR | SHGD_TEX_TRANSLUCENCY | SHGD_TEX_HEIGHT | SHGD_TEX_SUBSURFACE | SHGD_TEX_CUSTOM | SHGD_TEX_CUSTOM_SECONDARY | SHGD_TEX_DECAL)

struct SShaderTextureSlot
{
	SShaderTextureSlot()
	{
		m_TexType = eTT_MaxTexType;
	}

	string m_Name;
	string m_Description;
	uint8  m_TexType;

	void   GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_Name);
		pSizer->AddObject(m_Description);
		pSizer->AddObject(m_TexType);
	}
};

struct SShaderTexSlots
{
	uint32              m_nRefCount;
	SShaderTextureSlot* m_UsedSlots[EFTT_MAX];
	SShaderTexSlots()
	{
		m_nRefCount = 1;
		memset(m_UsedSlots, 0, sizeof(m_UsedSlots));
	}
	~SShaderTexSlots()
	{
		uint32 i;
		for (i = 0; i < EFTT_MAX; i++)
		{
			SShaderTextureSlot* pSlot = m_UsedSlots[i];
			SAFE_DELETE(pSlot);
		}
	}
	void Release()
	{
		m_nRefCount--;
		if (!m_nRefCount)
			delete this;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_UsedSlots);
	}
};

struct SShaderGenBit
{
	SShaderGenBit()
	{
		m_Mask = 0;
		m_Flags = 0;
		m_nDependencySet = 0;
		m_nDependencyReset = 0;
		m_NameLength = 0;
		m_dwToken = 0;
	}
	string              m_ParamName;
	string              m_ParamProp;
	string              m_ParamDesc;
	int                 m_NameLength;
	uint64              m_Mask;
	uint32              m_Flags;
	uint32              m_dwToken;
	std::vector<uint32> m_PrecacheNames;
	std::vector<string> m_DependSets;
	std::vector<string> m_DependResets;
	uint32              m_nDependencySet;
	uint32              m_nDependencyReset;

	void                GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_ParamName);
		pSizer->AddObject(m_ParamProp);
		pSizer->AddObject(m_ParamDesc);
		pSizer->AddObject(m_PrecacheNames);
		pSizer->AddObject(m_DependSets);
		pSizer->AddObject(m_DependResets);
	}
};

struct SShaderGen
{
	uint32                 m_nRefCount;
	TArray<SShaderGenBit*> m_BitMask;
	SShaderGen()
	{
		m_nRefCount = 1;
	}
	~SShaderGen()
	{
		uint32 i;
		for (i = 0; i < m_BitMask.Num(); i++)
		{
			SShaderGenBit* pBit = m_BitMask[i];
			SAFE_DELETE(pBit);
		}
		m_BitMask.Free();
	}
	void Release()
	{
		m_nRefCount--;
		if (!m_nRefCount)
			delete this;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_BitMask);
	}
};

//===================================================================================

enum EShaderType
{
	eST_All     = -1, //!< To set all with one call.

	eST_General = 0,
	eST_Metal,
	eST_Glass,
	eST_Vegetation,
	eST_Ice,
	eST_Terrain,
	eST_Shadow,
	eST_Water,
	eST_FX,
	eST_HUD3D,
	eST_PostProcess,
	eST_HDR,
	eST_Sky,
	eST_Particle,
	eST_Compute,
	eST_Max  //!< To define array size.
};

enum EShaderQuality
{
	eSQ_Low      = 0,
	eSQ_Medium   = 1,
	eSQ_High     = 2,
	eSQ_VeryHigh = 3,
	eSQ_Max      = 4
};

enum ERenderQuality
{
	eRQ_Low      = 0,
	eRQ_Medium   = 1,
	eRQ_High     = 2,
	eRQ_VeryHigh = 3,
	eRQ_Max      = 4
};

//! Shader profile flags .
#define SPF_LOADNORMALALPHA 0x1

struct SShaderProfile
{
	SShaderProfile() : m_iShaderProfileQuality(eSQ_High), m_nShaderProfileFlags(SPF_LOADNORMALALPHA)
	{
	}

	EShaderQuality GetShaderQuality() const
	{
		return (EShaderQuality)CLAMP(m_iShaderProfileQuality, 0, eSQ_VeryHigh);
	}

	void SetShaderQuality(const EShaderQuality& rValue)
	{
		m_iShaderProfileQuality = (int)rValue;
	}

	// ----------------------------------------------------------------

	int    m_iShaderProfileQuality;  //!< EShaderQuality e.g. eSQ_Medium, use Get/Set functions if possible.
	uint32 m_nShaderProfileFlags;    //!< SPF_...
};

//====================================================================================
//! Phys. material flags
#define MATF_NOCLIP 1

//====================================================================================
//! Registered shader techniques ID's
enum EShaderTechniqueID
{
	TTYPE_GENERAL = -1,
	TTYPE_Z       = 0,
	TTYPE_SHADOWGEN,
	TTYPE_GLOWPASS,
	TTYPE_MOTIONBLURPASS,
	TTYPE_CUSTOMRENDERPASS,
	TTYPE_EFFECTLAYER,
	TTYPE_WATERREFLPASS,
	TTYPE_WATERCAUSTICPASS,
	TTYPE_ZPREPASS,

	//! PC specific techniques must go after this point, to support shader serializing TTYPE_CONSOLE_MAX must equal TTYPE_MAX for console.
	TTYPE_CONSOLE_MAX,
	TTYPE_DEBUG = TTYPE_CONSOLE_MAX,

	TTYPE_MAX
};

//====================================================================================

//! EFSLIST_ lists.
//! \note Declaration order/index value has no explicit meaning.
//! If you change this, you must also update SEF_ListDG::GetDescription.
enum ERenderListID
{
	EFSLIST_INVALID = 0,             //!< Don't use, internally used.
	EFSLIST_PREPROCESS,              //!< Pre-process items.
	EFSLIST_GENERAL,                 //!< Opaque ambient_light+shadow passes.
	EFSLIST_ZPREPASS,                //!< Items that are rendered into the z-prepass.
	EFSLIST_TERRAINLAYER,            //!< Unsorted terrain layers.
	EFSLIST_SHADOW_GEN,              //!< Shadow map generation.
	EFSLIST_DECAL,                   //!< Opaque or transparent decals.
	EFSLIST_WATER_VOLUMES,           //!< After decals.
	EFSLIST_TRANSP_BW,               //!< Sorted by distance under-water render items.
	EFSLIST_TRANSP_AW,               //!< Sorted by distance above-water render items.
	EFSLIST_TRANSP_NEAREST,          //!< Nearest transparent items
	EFSLIST_WATER,                   //!< Water-ocean render items.
	EFSLIST_AFTER_HDRPOSTPROCESS,    //!< After hdr post-processing screen effects.
	EFSLIST_AFTER_POSTPROCESS,       //!< After post-processing screen effects.
	EFSLIST_SHADOW_PASS,             //!< Shadow mask generation (usually from from shadow maps).
	EFSLIST_SKIN,                    //!< Skin rendering pre-process.
	EFSLIST_HALFRES_PARTICLES,       //!< Half resolution particles.
	EFSLIST_PARTICLES_THICKNESS,     //!< Particles thickness passes.
	EFSLIST_LENSOPTICS,              //!< Lens-optics processing.
	EFSLIST_VOXELIZE,                //!< Mesh voxelization.
	EFSLIST_EYE_OVERLAY,             //!< Eye overlay layer requires special processing.
	EFSLIST_FOG_VOLUME,              //!< Fog density injection passes.
	EFSLIST_NEAREST_OBJECTS,         //!< Nearest objects.
	EFSLIST_FORWARD_OPAQUE,          //!< Forward opaque pass objects.
	EFSLIST_FORWARD_OPAQUE_NEAREST,  //!< Nearest forward opaque pass objects.
	EFSLIST_CUSTOM,                  //!< Custom scene pass.
	EFSLIST_HIGHLIGHT,               //!< Candidate for selection objects
	EFSLIST_DEBUG_HELPER,            //!< Debug helper render items.
	EFSLIST_SKY,                     //!< Sky elements

	EFSLIST_NUM
};

//================================================================
//! Different preprocess flags for shaders that require preprocessing (like recursive render to texture, screen effects, visibility check, ...) SShader->m_nPreprocess flags in priority order.

#define  SPRID_FIRST         25
#define  SPRID_SCANTEXWATER  26
#define  FSPR_SCANTEXWATER   (1 << SPRID_SCANTEXWATER)
#define  SPRID_SCANTEX       27
#define  FSPR_SCANTEX        (1 << SPRID_SCANTEX)

#define  SPRID_CUSTOMTEXTURE 30
#define  FSPR_CUSTOMTEXTURE  (1 << SPRID_CUSTOMTEXTURE)

#define  FSPR_MASK           0xfff00000
#define  FSPR_MAX            (1 << 31)

#define FEF_DONTSETTEXTURES  1          // Set: explicit setting of samplers (e.g. tex->Apply(1,nTexStatePoint)), not set: set sampler by sematics (e.g. $ZTarget).
#define FEF_DONTSETSTATES    2

// SShader::m_Flags
// Different useful flags
#define EF_RELOAD                        1 // Shader needs tangent vectors array.
#define EF_FORCE_RELOAD                  2
#define EF_RELOADED                      4
#define EF_NODRAW                        8
#define EF_HASCULL                       0x10
#define EF_SUPPORTSDEFERREDSHADING_MIXED 0x20
#define EF_SUPPORTSDEFERREDSHADING_FULL  0x40
#define EF_SUPPORTSDEFERREDSHADING       (EF_SUPPORTSDEFERREDSHADING_MIXED | EF_SUPPORTSDEFERREDSHADING_FULL)
#define EF_DECAL                         0x80
#define EF_LOADED                        0x100
#define EF_LOCALCONSTANTS                0x200
#define EF_BUILD_TREE                    0x400
#define EF_LIGHTSTYLE                    0x800
#define EF_NOCHUNKMERGING                0x1000
#define EF_SUNFLARES                     0x2000
#define EF_NEEDNORMALS                   0x4000 // Need normals operations.
#define EF_OFFSETBUMP                    0x8000
#define EF_NOTFOUND                      0x10000
#define EF_DEFAULT                       0x20000
#define EF_SKY                           0x40000
#define EF_USELIGHTS                     0x80000
#define EF_ALLOW3DC                      0x100000
#define EF_FOGSHADER                     0x200000
#define EF_FAILED_IMPORT                 0x400000 // Currently just for debug, can be removed if necessary
#define EF_PRECACHESHADER                0x800000
#define EF_FORCEREFRACTIONUPDATE         0x1000000
#define EF_SUPPORTSINSTANCING_CONST      0x2000000
#define EF_SUPPORTSINSTANCING_ATTR       0x4000000
#define EF_SUPPORTSINSTANCING            (EF_SUPPORTSINSTANCING_CONST | EF_SUPPORTSINSTANCING_ATTR)
#define EF_WATERPARTICLE                 0x8000000
#define EF_CLIENTEFFECT                  0x10000000
#define EF_SYSTEM                        0x20000000
#define EF_REFRACTIVE                    0x40000000
#define EF_NOPREVIEW                     0x80000000

#define EF_PARSE_MASK                    (EF_SUPPORTSINSTANCING | EF_SKY | EF_HASCULL | EF_USELIGHTS | EF_REFRACTIVE)

// SShader::Flags2
// Additional Different useful flags

#define EF2_PREPR_SCANWATER      0x4
#define EF2_NOCASTSHADOWS        0x8
#define EF2_NODRAW               0x10
#define EF2_WRINKLE_BLENDING     0x20
#define EF2_HASOPAQUE            0x40
#define EF2_AFTERHDRPOSTPROCESS  0x80
#define EF2_DONTSORTBYDIST       0x100
#define EF2_FORCE_WATERPASS      0x200
#define EF2_FORCE_GENERALPASS    0x400
#define EF2_AFTERPOSTPROCESS     0x800
#define EF2_IGNORERESOURCESTATES 0x1000
#define EF2_EYE_OVERLAY          0x2000
#define EF2_FORCE_TRANSPASS      0x4000
#define EF2_FORCE_ZPASS          0x10000
#define EF2_FORCE_DRAWLAST       0x20000
#define EF2_FORCE_DRAWAFTERWATER 0x40000
#define EF2_BILLBOARD            0x80000
#define EF2_DEPTH_FIXUP          0x100000
#define EF2_SINGLELIGHTPASS      0x200000
#define EF2_FORCE_DRAWFIRST      0x400000
#define EF2_HAIR                 0x800000
#define EF2_DETAILBUMPMAPPING    0x1000000
#define EF2_HASALPHATEST         0x2000000
#define EF2_HASALPHABLEND        0x4000000
#define EF2_ZPREPASS             0x8000000
#define EF2_VERTEXCOLORS         0x10000000
#define EF2_SKINPASS             0x20000000
#define EF2_HW_TESSELLATION      0x40000000
#define EF2_ALPHABLENDSHADOWS    0x80000000

struct IShader
{
public:
	// <interfuscator:shuffle>
	virtual ~IShader(){}
	virtual int                        GetID() = 0;
	virtual int                        AddRef() = 0;
	virtual int                        Release() = 0;
	virtual int                        ReleaseForce() = 0;

	virtual const char*                GetName() = 0;
	virtual const char*                GetName() const = 0;
	virtual int                        GetFlags() const = 0;
	virtual int                        GetFlags2() const = 0;
	virtual void                       SetFlags2(int Flags) = 0;
	virtual void                       ClearFlags2(int Flags) = 0;
	virtual bool                       Reload(int nFlags, const char* szShaderName) = 0;
	virtual TArray<CRenderElement*>*     GetREs(int nTech) = 0;
	virtual DynArrayRef<SShaderParam>& GetPublicParams() = 0;
	virtual void                       CopyPublicParamsTo(SInputShaderResources& copyToResource) = 0;
	virtual int                        GetTexId() = 0;
	virtual ITexture*                  GetBaseTexture(int* nPass, int* nTU) = 0;
	virtual unsigned int               GetUsedTextureTypes(void) = 0;
	virtual SShaderTexSlots*           GetUsedTextureSlots(int nTechnique) = 0;
	virtual ECull                      GetCull(void) = 0;
	virtual int                        Size(int Flags) = 0;
	virtual uint64                     GetGenerationMask() = 0;
	virtual SShaderGen*                GetGenerationParams() = 0;
	virtual int                        GetTechniqueID(int nTechnique, int nRegisteredTechnique) = 0;
	virtual InputLayoutHandle          GetVertexFormat(void) = 0;

	virtual EShaderType                GetShaderType() = 0;
	virtual uint32                     GetVertexModificator() = 0;

	virtual void                       GetMemoryUsage(ICrySizer* pSizer) const = 0;
	// </interfuscator:shuffle>

	static uint32 GetTextureSlot(EEfResTextures textureType) { return (uint32)textureType; }
};

struct SShaderItem
{
	IShader*                m_pShader = nullptr;
	IRenderShaderResources* m_pShaderResources = nullptr;
	int32                   m_nTechnique = -1;
	uint32                  m_nPreprocessFlags = 1;

	SShaderItem() {}
	SShaderItem(IShader* pSH) : m_pShader(pSH) {}
	SShaderItem(IShader* pSH, IRenderShaderResources* pRS) : m_pShader(pSH),m_pShaderResources(pRS) {}
	SShaderItem(IShader* pSH, IRenderShaderResources* pRS, int nTechnique)
		 : m_pShader(pSH),m_pShaderResources(pRS),m_nTechnique(nTechnique)
	{}

	SShaderItem Clone() const
	{
		SShaderItem SI;

		SI.m_pShader = m_pShader;
		SI.m_pShader->AddRef();
		SI.m_pShaderResources = m_pShaderResources->Clone();
		SI.m_pShaderResources->CloneConstants(m_pShaderResources);
		SI.m_nTechnique = m_nTechnique;
		SI.m_nPreprocessFlags = m_nPreprocessFlags;

		return SI;
	}

	uint32 PostLoad();
	bool   Update();
	bool   RefreshResourceConstants();

	// If you change this function please check bTransparent variable in CRenderMesh::Render().
	inline bool IsZWrite() const
	{
		IShader* pSH = m_pShader;
		if (pSH->GetFlags() & (EF_NODRAW | EF_DECAL))
			return false;
		if (pSH->GetFlags2() & EF2_FORCE_ZPASS)
			return true;
		if (m_pShaderResources && m_pShaderResources->IsTransparent())
			return false;
		return true;
	}

	inline struct SShaderTechnique* GetTechnique() const;
	bool                            IsMergable(SShaderItem& PrevSI);

	void                            GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_pShader);
		pSizer->AddObject(m_pShaderResources);
	}
};

//////////////////////////////////////////////////////////////////////
//! Define this before including <IRenderMesh.h>
//! \cond INTERNAL
struct CRenderChunk
{
	bool     m_bUsesBones;
	CREMesh* pRE;  //!< Pointer to the mesh.

	float    m_texelAreaDensity;

	uint32   nFirstIndexId;
	uint32   nNumIndices;

	uint32   nFirstVertId;
	uint32   nNumVerts;

	uint16   m_nMatFlags; //!< Material flags from originally assigned material @see EMaterialFlags.
	uint16   m_nMatID;    //!< Material Sub-object id.

	//! Index of sub-object that this chunk originates from, used by sub-object hide mask.
	//! \see IStatObj::GetSubObject
	uint32 nSubObjectIndex;

	CRenderChunk() :
		m_bUsesBones(false),
		pRE(0),
		m_texelAreaDensity(1.0f),
		nFirstIndexId(0),
		nNumIndices(0),
		nFirstVertId(0),
		nNumVerts(0),
		m_nMatFlags(0),
		m_nMatID(0),
		nSubObjectIndex(0)
	{
	}

	int  Size() const;

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
	}
};
//! \endcond

typedef DynArray<CRenderChunk> TRenderChunkArray;

//////////////////////////////////////////////////////////////////////
// DLights
enum eDynamicLightFlags
{
	DLF_AREA_SPEC_TEX           = BIT(0),
	DLF_DIRECTIONAL             = BIT(1),
	DLF_BOX_PROJECTED_CM        = BIT(2),
	// UNUSED										= BIT(3),
	DLF_POST_3D_RENDERER        = BIT(4),
	DLF_CASTSHADOW_MAPS         = BIT(5),
	DLF_POINT                   = BIT(6),
	DLF_PROJECT                 = BIT(7),
	// UNUSED                   = BIT(8),
	//	UNUSED										= BIT(9),
	DLF_IGNORES_VISAREAS        = BIT(10),
	DLF_DEFERRED_CUBEMAPS       = BIT(11),
	DLF_HAS_CLIP_VOLUME         = BIT(12),
	DLF_DISABLED                = BIT(13),
	DLF_AREA_LIGHT              = BIT(14),
	DLF_USE_FOR_SVOGI           = BIT(15),
	// UNUSED										=	BIT(16),
	DLF_FAKE                    = BIT(17),   //!< No lighting, used for Flares, beams and such.
	DLF_SUN                     = BIT(18),
	DLF_LM                      = BIT(19),
	DLF_THIS_AREA_ONLY          = BIT(20),   //!< Affects only current area/sector.
	DLF_AMBIENT                 = BIT(21),   //!< Ambient light (has name indicates, used for replacing ambient).
	DLF_INDOOR_ONLY             = BIT(22),   //!< Do not affect height map.
	DLF_VOLUMETRIC_FOG          = BIT(23),   //!< Affects volumetric fog.
	DLF_LINK_TO_SKY_COLOR       = BIT(24),   //!< Multiply light color with current sky color (use GI sky color if available).
	DLF_ATTACH_TO_SUN           = BIT(25),   //!< Add only to  Light Propagation Volume if it's possible.
	DLF_TRACKVIEW_TIMESCRUBBING = BIT(26),   //!< Add only to  Light Propagation Volume if it's possible.
	DLF_VOLUMETRIC_FOG_ONLY     = BIT(27),   //!< Affects only volumetric fog.

	//! Deprecated. Remove once deferred shading by default.
	DLF_DEFERRED_LIGHT = BIT(29),

	//! Deprecated. Remove all dependencies editor side, etc.
	DLF_SPECULAROCCLUSION = BIT(30),
	DLF_DIFFUSEOCCLUSION  = BIT(31),

	DLF_LIGHTTYPE_MASK    = (DLF_DIRECTIONAL | DLF_POINT | DLF_PROJECT | DLF_AREA_LIGHT)
};

//! Area light types.
#define DLAT_SPHERE            0x1
#define DLAT_RECTANGLE         0x2
#define DLAT_POINT             0x4

#define DL_SHADOW_UPDATE_SHIFT 8

struct IEntity;

#include <CryRenderer/IRenderMesh.h>  // <> required for Interfuscator

struct IAnimNode;

struct ILightAnimWrapper : public _i_reference_target_t
{
public:
	virtual bool Resolve() = 0;
	IAnimNode*   GetNode() const { return m_pNode; }

protected:
	ILightAnimWrapper(const char* name) : m_name(name), m_pNode(0) {}
	virtual ~ILightAnimWrapper() {}

protected:
	string     m_name;
	IAnimNode* m_pNode;
};

#define MAX_RECURSION_LEVELS 2

// Summary:
//	 Same as in the 3dEngine.
#define MAX_LIGHTS_NUM 32
#define ILLUMINANCE_THRESHOLD 0.004f

inline float IlluminanceThreshold()
{
	static ICVar* pCvar = gEnv->pConsole->GetCVar("e_LightIlluminanceThreshold");
	static float fThreshold = pCvar ? pCvar->GetFVal() : ILLUMINANCE_THRESHOLD;
	return fThreshold;
}

ILINE float GetRadiationRadius(float fIntensity, float fThreshold = ILLUMINANCE_THRESHOLD)
{
	// I / R^2 = L;  R = sqrt(I/L)
	fThreshold = max(fThreshold, sqr(ILLUMINANCE_THRESHOLD));
	return crymath::sqrt_fast(fIntensity / fThreshold);
}


struct ShadowMapFrustum;

struct SRenderLight
{
	SRenderLight() {}
	SRenderLight(const SRenderLight& other)
	{
		CopyFrom(other);
	}

	~SRenderLight()
	{
		DropResources();
	}

	SRenderLight& operator=(const SRenderLight& rhs)
	{
		CopyFrom(rhs);
		return *this;
	}

	const Vec3& GetPosition() const
	{
		return m_Origin;
	}
	void SetPosition(const Vec3& vPos)
	{
		m_BaseOrigin = vPos;
		m_Origin = vPos;
	}

	//! Use this instead of m_Color.
	void SetLightColor(const ColorF& cColor)
	{
		m_Color = cColor;
		m_BaseColor = cColor;
		ComputeEffectiveRadius();
	}

	void SetRadius(float fRadius, float fBulb = 0.0f)
	{
		m_fClipRadius = fRadius;
		if (fBulb > 0.0f)
			m_fAttenuationBulbSize = fBulb;
		ComputeEffectiveRadius();
	}

	void ComputeEffectiveRadius()
	{
		if (m_Flags & (DLF_DIRECTIONAL | DLF_DEFERRED_CUBEMAPS))
			m_fRadius = m_fClipRadius;
		else
		{
			float fIntensity = m_Color.Luminance() * GetIntensityScale();
			m_fRadius = min(GetRadiationRadius(fIntensity, IlluminanceThreshold()), m_fClipRadius);
		}
		m_fShadowUpdateMinRadius = m_fRadius;
	}

	float GetIntensityScale() const
	{
		// Adjust light intensity so that the intended brightness is reached 1 meter from the light's surface
		// I / (1 + bulb)^2 = 1; I = (1 + bulb)^2
		if (m_Flags & (DLF_AREA_LIGHT | DLF_AMBIENT))
			return 1.0f;
		return sqr(1.0f + m_fAttenuationBulbSize);
	}

	float GetAttenuation(float fDist) const
	{
		if (m_Flags & DLF_DIRECTIONAL)
			return 1.0f;
		return crymath::rcp(sqr(max(fDist, +m_fAttenuationBulbSize)));
	}
	
	float GetAttenuation(const Vec3& vPos) const
	{
		if (m_Flags & DLF_DIRECTIONAL)
			return 1.0f;
		float fDistSq = (vPos - m_Origin).GetLengthSquared();
		return crymath::rcp(max(fDistSq, sqr(+m_fAttenuationBulbSize)));
	}

	ITexture* GetDiffuseCubemap() const
	{
		assert(m_pDiffuseCubemap != (void*)(uintptr_t)0xfeeefeee);
		return m_pDiffuseCubemap;
	}

	ITexture* GetSpecularCubemap() const
	{
		assert(m_pSpecularCubemap != (void*)(uintptr_t)0xfeeefeee);
		return m_pSpecularCubemap;
	}

	ITexture* GetLightTexture() const
	{
		return m_pLightImage ? m_pLightImage : m_pLightDynTexSource ? m_pLightDynTexSource->GetTexture() : NULL;
	}

	IOpticsElementBase* GetLensOpticsElement() const
	{
		return m_pLensOpticsElement;
	}

	void SetLensOpticsElement(IOpticsElementBase* pOptics)
	{
		if (m_pLensOpticsElement == pOptics)
			return;
		if (pOptics && pOptics->GetType() != eFT_Root)
			return;
		SAFE_RELEASE(m_pLensOpticsElement);
		m_pLensOpticsElement = pOptics;
		if (m_pLensOpticsElement)
			m_pLensOpticsElement->AddRef();
	}

	void GetMemoryUsage(ICrySizer* pSizer) const { /*LATER*/ }

	void AcquireResources()
	{
		SAFE_ACQUIRE(m_Shader.m_pShader);
		SAFE_ACQUIRE(m_pLightImage);
		SAFE_ACQUIRE(m_pLightDynTexSource);
		SAFE_ACQUIRE(m_pDiffuseCubemap);
		SAFE_ACQUIRE(m_pSpecularCubemap);
		SAFE_ACQUIRE(m_pLensOpticsElement);
		SAFE_ACQUIRE(m_pSoftOccQuery);
		SAFE_ACQUIRE(m_pLightAnim);
	}

	void DropResources()
	{
		SAFE_RELEASE(m_Shader.m_pShader);
		SAFE_RELEASE(m_pLightImage);
		SAFE_RELEASE(m_pDiffuseCubemap);
		SAFE_RELEASE(m_pSpecularCubemap);
		SAFE_RELEASE(m_pLensOpticsElement);
		SAFE_RELEASE(m_pSoftOccQuery);
		SAFE_RELEASE(m_pLightAnim);
		SAFE_RELEASE(m_pLightDynTexSource);
	}

	void SetAnimSpeed(float fAnimSpeed)
	{
		m_nAnimSpeed = int_round(min(fAnimSpeed * 255.0f / 4.0f, 255.0f));   // Assuming speed multiplier in range [0, 4]
	}

	float GetAnimSpeed() const
	{
		return ((float) m_nAnimSpeed) * (4.0f / 255.0f);
	}

	void SetFalloffMax(float fMax)
	{
		m_nAttenFalloffMax = int_round(fMax * 255.0f);
	}

	float GetFalloffMax()  const
	{
		return ((float) m_nAttenFalloffMax) / 255.0f;
	}

	//! Good for debugging.
	bool IsOk() const
	{
		for (int i = 0; i < 3; ++i)
		{
			if (m_Color[i] < 0 || m_Color[i] > 1000.0f || _isnan(m_Color[i]))
				return false;
			if (m_BaseColor[i] < 0 || m_BaseColor[i] > 1000.0f || _isnan(m_BaseColor[i]))
				return false;
		}
		return true;
	}

	//! Use this instead of m_Color.
	const ColorF& GetFinalColor(const ColorF& cColor) const
	{
		return m_Color;
	}

	//! Use this instead of m_Color.
	void SetSpecularMult(float fSpecMult)
	{
		m_SpecMult = fSpecMult;
		m_BaseSpecMult = fSpecMult;
	}

	void SetShadowBiasParams(float fShadowBias, float fShadowSlopeBias)
	{
		m_fShadowBias = fShadowBias;
		m_fShadowSlopeBias = fShadowSlopeBias;
	}

	//! Use this instead of m_Color.
	const float& GetSpecularMult() const
	{
		return m_SpecMult;
	}
	void SetMatrix(const Matrix34& Matrix, bool reset = true)
	{
		// Scale the cubemap to adjust the default 45 degree 1/2 angle fustrum to
		// the desired angle (0 to 90 degrees).
		float scaleFactor = tan_tpl((90.0f - m_fLightFrustumAngle) * gf_PI / 180.0f);
		m_ProjMatrix = Matrix33(Matrix) * Matrix33::CreateScale(Vec3(1, scaleFactor, scaleFactor));
		Matrix44 transMat;
		transMat.SetIdentity();
		transMat(3, 0) = -Matrix(0, 3);
		transMat(3, 1) = -Matrix(1, 3);
		transMat(3, 2) = -Matrix(2, 3);
		m_ProjMatrix = transMat * m_ProjMatrix;
		m_ObjMatrix = Matrix;

		// Remove any scale
		m_ObjMatrix.GetColumn0().NormalizeSafe(Vec3Constants<f32>::fVec3_OneX);
		m_ObjMatrix.GetColumn1().NormalizeSafe(Vec3Constants<f32>::fVec3_OneY);
		m_ObjMatrix.GetColumn2().NormalizeSafe(Vec3Constants<f32>::fVec3_OneZ);

		if (reset)
		{
			m_BaseObjMatrix = m_ObjMatrix;
		}
	}

	void SetSpecularCubemap(ITexture* texture)
	{
		m_pSpecularCubemap = texture;
	}

	void SetDiffuseCubemap(ITexture* texture)
	{
		m_pDiffuseCubemap = texture;
	}

	void ReleaseCubemaps()
	{
		SAFE_RELEASE(m_pSpecularCubemap);
		SAFE_RELEASE(m_pDiffuseCubemap);
	}

	//////////////////////////////////////////////////////////////////////////
	void CopyFrom( const SRenderLight &dl )
	{
		if (this == &dl)
			return;

		DropResources();

		m_pOwner = dl.m_pOwner;
		memcpy(m_pObject, dl.m_pObject, sizeof(m_pObject));
		m_Shader = dl.m_Shader;
		m_pShadowMapFrustums = dl.m_pShadowMapFrustums;
		m_pDiffuseCubemap = dl.m_pDiffuseCubemap;
		m_pSpecularCubemap = dl.m_pSpecularCubemap;
		m_pLightImage = dl.m_pLightImage;
		m_pLightDynTexSource = dl.m_pLightDynTexSource;
		m_sName = dl.m_sName;
		m_ProjMatrix = dl.m_ProjMatrix;
		m_ObjMatrix = dl.m_ObjMatrix;
		m_BaseObjMatrix = dl.m_BaseObjMatrix;
		m_Color = dl.m_Color;
		m_BaseColor = dl.m_BaseColor;
		m_Origin = dl.m_Origin;
		m_BaseOrigin = dl.m_BaseOrigin;
		m_fRadius = dl.m_fRadius;
		m_fClipRadius = dl.m_fClipRadius;
		m_ProbeExtents = dl.m_ProbeExtents;
		m_SpecMult = dl.m_SpecMult;
		m_BaseSpecMult = dl.m_BaseSpecMult;
		m_fShadowBias = dl.m_fShadowBias;
		m_fShadowSlopeBias = dl.m_fShadowSlopeBias;
		m_fShadowResolutionScale = dl.m_fShadowResolutionScale;
		m_fHDRDynamic = dl.m_fHDRDynamic;
		m_pLensOpticsElement = dl.m_pLensOpticsElement;
		m_LensOpticsFrustumAngle = dl.m_LensOpticsFrustumAngle;
		m_pSoftOccQuery = dl.m_pSoftOccQuery;
		m_fLightFrustumAngle = dl.m_fLightFrustumAngle;
		m_fProjectorNearPlane = dl.m_fProjectorNearPlane;
		m_Flags = dl.m_Flags;
		m_Id = dl.m_Id;
		m_n3DEngineUpdateFrameID = dl.m_n3DEngineUpdateFrameID;
		m_sX = dl.m_sX;
		m_sY = dl.m_sY;
		m_sWidth = dl.m_sWidth;
		m_sHeight = dl.m_sHeight;
		m_nLightStyle = dl.m_nLightStyle;
		m_nLightPhase = dl.m_nLightPhase;
		m_pLightAnim = dl.m_pLightAnim;
		m_fAreaWidth = dl.m_fAreaWidth;
		m_fAreaHeight = dl.m_fAreaHeight;
		m_fBoxWidth = dl.m_fBoxWidth;
		m_fBoxHeight = dl.m_fBoxHeight;
		m_fBoxLength = dl.m_fBoxLength;
		m_fTimeScrubbed = dl.m_fTimeScrubbed;
		m_nShadowMinResolution = dl.m_nShadowMinResolution;
		m_fShadowUpdateMinRadius = dl.m_fShadowUpdateMinRadius;
		m_nShadowUpdateRatio = dl.m_nShadowUpdateRatio;
		m_nAnimSpeed = dl.m_nAnimSpeed;
		m_nSortPriority = dl.m_nSortPriority;
		m_nAttenFalloffMax = dl.m_nAttenFalloffMax;
		m_fAttenuationBulbSize = dl.m_fAttenuationBulbSize;
		m_fFogRadialLobe = dl.m_fFogRadialLobe;
		m_nEntityId = dl.m_nEntityId;
		memcpy(m_nStencilRef, dl.m_nStencilRef, sizeof(m_nStencilRef));
		memcpy(m_pClipVolumes, dl.m_pClipVolumes, sizeof(m_pClipVolumes));
		
		AcquireResources();
	}

public:
	//////////////////////////////////////////////////////////////////////////
	// Public Member variables
	//////////////////////////////////////////////////////////////////////////

	// Commonly used on most code paths (64 bytes).
	int16  m_Id = 0;
	uint8  m_nStencilRef[2] = {};
	uint32 m_n3DEngineUpdateFrameID = 0;
	uint32 m_nEntityId = (uint32)-1;
	uint32 m_Flags = 0;                   //!< Light flags (DLF_etc).
	Vec3   m_Origin{0,0,0};               //!< World space position.
	float  m_fRadius = 100.f;             //!< xyz= Origin, w=Radius. (Do not change order).
	ColorF m_Color{0,0,0};                //!< w component unused - todo pack spec mul into alpha (post c3 - touches quite some code).
	float  m_SpecMult = 1.f;
	float  m_fHDRDynamic = 0;             //!< <DEPRECATED> 0 to get the same results in HDR, <0 to get darker, >0 to get brighter.
	int16  m_sX = 0;                      //!< Scissor parameters (2d extent).
	int16  m_sY = 0;
	int16  m_sWidth = 0;
	int16  m_sHeight = 0;

	// Env. probes.
	ITexture* m_pDiffuseCubemap = nullptr;   //!< Very small cubemap texture to make a lookup for diffuse.
	ITexture* m_pSpecularCubemap = nullptr;  //!< Cubemap texture to make a lookup for local specular.
	Vec3      m_ProbeExtents{10.f,10.f,10.f};
	float     m_fBoxWidth  = 1.f;
	float     m_fBoxHeight = 1.f;
	float     m_fBoxLength = 1.f;
	uint8     m_nAttenFalloffMax = 255;
	uint8     m_nSortPriority = 0;

	// Shadow map fields.
	struct ILightSource* m_pOwner = nullptr;
	ShadowMapFrustum**   m_pShadowMapFrustums = nullptr;
	float                m_fShadowBias = 1.f;
	float                m_fShadowSlopeBias = 1.f;
	float                m_fShadowResolutionScale = 1.f;
	float                m_fShadowUpdateMinRadius = 4.f;
	uint16               m_nShadowMinResolution = 0;
	uint16               m_nShadowUpdateRatio = (1 << DL_SHADOW_UPDATE_SHIFT);
	uint8                m_ShadowMaskIndex = 0;

	// Projector.
	IDynTextureSource* m_pLightDynTexSource = nullptr; //!< Can be used to project dynamic textures.
	ITexture*          m_pLightImage = nullptr;
	Matrix44           m_ProjMatrix{type_zero::ZERO};
	Matrix34           m_ObjMatrix{type_zero::ZERO};
	float              m_fLightFrustumAngle = 45.f;
	float              m_fProjectorNearPlane = 0;

	// Misc fields. todo: put in cold data struct (post c3 - touches quite some code).
	const char*          m_sName = "";  //!< Optional name of the light source.
	SShaderItem          m_Shader;
	CRenderObject*       m_pObject[MAX_RECURSION_LEVELS] = {};  //!< Object for light coronas and light flares.
	IOpticsElementBase*  m_pLensOpticsElement = nullptr;
	ISoftOcclusionQuery* m_pSoftOccQuery = nullptr;
	ILightAnimWrapper*   m_pLightAnim = nullptr;

	float                m_fTimeScrubbed = 0;

	Matrix34             m_BaseObjMatrix{type_zero::ZERO};
	Vec3                 m_BaseOrigin{0,0,0};  //!< World space position.
	ColorF               m_BaseColor{0,0,0};   //!< w component unused..
	float                m_BaseSpecMult = 1.f;

	float                m_fClipRadius = 100.f;
	float                m_fAttenuationBulbSize = 0.1f;

	float                m_fAreaWidth = 1.f;
	float                m_fAreaHeight = 1.f;

	float                m_fFogRadialLobe = 0;  //!< The blend ratio of two radial lobe for volumetric fog.

	uint8                m_nAnimSpeed = 0;
	uint8                m_nLightStyle = 0;
	uint8                m_nLightPhase = 0;
	uint8                m_LensOpticsFrustumAngle = 255;  //!< from 0 to 255, The range will be adjusted from 0 to 360 when used.

	IClipVolume*         m_pClipVolumes[2] = {};
};

typedef std::list<SRenderLight> RenderLightsList;
typedef int16 RenderLightIndex;

#define DECAL_HAS_NORMAL_MAP   (1 << 0)
#define DECAL_STATIC           (1 << 1)
#define DECAL_HAS_SPECULAR_MAP (1 << 2)

struct SDeferredDecal
{
	SDeferredDecal()
	{
		ZeroStruct(*this);
		rectTexture.w = rectTexture.h = 1.f;
	}

	Matrix34   projMatrix; //!< Defines where projection should be applied in the world.
	IMaterial* pMaterial;  //!< Decal material.
	float      fAlpha;     //!< Transparency of decal, used mostly for distance fading.
	float      fGrowAlphaRef;
	RectF      rectTexture; //!< Subset of texture to render.
	uint32     nFlags;
	uint8      nSortOrder; //!< User defined sort order.
};

struct SDeferredClipVolume
{
	Matrix34                mWorldTM;
	AABB                    mAABB;
	uint8                   nStencilRef;
	uint8                   nFlags;

	_smart_ptr<IRenderMesh> m_pRenderMesh;

	enum { MaxBlendInfoCount = 2 };
	struct BlendInfo
	{
		uint8 blendID;
		Vec4  blendPlane;
	} blendInfo[MaxBlendInfoCount];
};

struct SFogVolumeInfo
{
	Vec3     m_center;
	uint32   m_viewerInsideVolume : 1;
	uint32   m_affectsThisAreaOnly : 1;
	uint32   m_stencilRef : 8;
	uint32   m_volumeType : 1;
	uint32   m_reserved : 21;
	AABB     m_localAABB;
	Matrix34 m_matWSInv;
	float    m_globalDensity;
	float    m_densityOffset;
	Vec2     m_softEdgesLerp;
	ColorF   m_fogColor;
	Vec3     m_heightFallOffDirScaled;
	Vec3     m_heightFallOffBasePoint;
	Vec3     m_eyePosInOS;
	Vec3     m_rampParams;
	Vec3     m_windOffset;
	float    m_noiseScale;
	Vec3     m_noiseFreq;
	float    m_noiseOffset;
	float    m_noiseElapsedTime;
	Vec3     m_emission;
};

struct SCloudBlocker
{
	Vec3  position;
	Vec3  param;
	int32 flags;
};

struct SWaterRippleInfo
{
	static const int32 MaxWaterRipplesInScene = 128;
	static const int32 WaveSimulationGridSize = 50;

	Vec3  position;
	float scale;
	float strength;

	SWaterRippleInfo(const Vec3& vPosition, float fScale, float fStrength)
		: position(vPosition)
		, scale(fScale)
		, strength(fStrength)
	{}

	SWaterRippleInfo(const SWaterRippleInfo& waterRipple)
		: position(waterRipple.position)
		, scale(waterRipple.scale)
		, strength(waterRipple.strength)
	{}

	void GetMemoryUsage(ICrySizer* pSizer) const {}
};

//! Runtime shader flags for HW skinning.
enum EHWSkinningRuntimeFlags
{
	eHWS_MotionBlured             = BIT(2),
	eHWS_SkinnedLinear            = BIT(3),
	eHWS_DC_deformation_Skinning  = BIT(4),
	eHWS_DC_Deformation_PreMorphs = BIT(5),
	eHWS_DC_Deformation_Tangents  = BIT(6)
};

//! Shader graph support.
enum EGrBlockType
{
	eGrBlock_Unknown,
	eGrBlock_VertexInput,
	eGrBlock_VertexOutput,
	eGrBlock_PixelInput,
	eGrBlock_PixelOutput,
	eGrBlock_Texture,
	eGrBlock_Sampler,
	eGrBlock_Function,
	eGrBlock_Constant,
};

enum EGrBlockSamplerType
{
	eGrBlockSampler_Unknown,
	eGrBlockSampler_2D,
	eGrBlockSampler_3D,
	eGrBlockSampler_Cube,
	eGrBlockSampler_Bias2D,
	eGrBlockSampler_BiasCube,
};

enum EGrNodeType
{
	eGrNode_Unknown,
	eGrNode_Input,
	eGrNode_Output,
};

enum EGrNodeFormat
{
	eGrNodeFormat_Unknown,
	eGrNodeFormat_Float,
	eGrNodeFormat_Vector,
	eGrNodeFormat_Matrix,
	eGrNodeFormat_Int,
	eGrNodeFormat_Bool,
	eGrNodeFormat_Texture2D,
	eGrNodeFormat_Texture3D,
	eGrNodeFormat_TextureCUBE,
};

enum EGrNodeIOSemantic
{
	eGrNodeIOSemantic_Unknown,
	eGrNodeIOSemantic_Custom,
	eGrNodeIOSemantic_VPos,
	eGrNodeIOSemantic_Color0,
	eGrNodeIOSemantic_Color1,
	eGrNodeIOSemantic_Color2,
	eGrNodeIOSemantic_Color3,
	eGrNodeIOSemantic_Normal,
	eGrNodeIOSemantic_TexCoord0,
	eGrNodeIOSemantic_TexCoord1,
	eGrNodeIOSemantic_TexCoord2,
	eGrNodeIOSemantic_TexCoord3,
	eGrNodeIOSemantic_TexCoord4,
	eGrNodeIOSemantic_TexCoord5,
	eGrNodeIOSemantic_TexCoord6,
	eGrNodeIOSemantic_TexCoord7,
	eGrNodeIOSemantic_Tangent,
	eGrNodeIOSemantic_Binormal,
};

struct SShaderGraphFunction
{
	string              m_Data;
	string              m_Name;
	std::vector<string> inParams;
	std::vector<string> outParams;
	std::vector<string> szInTypes;
	std::vector<string> szOutTypes;
};

struct SShaderGraphNode
{
	EGrNodeType            m_eType;
	EGrNodeFormat          m_eFormat;
	EGrNodeIOSemantic      m_eSemantic;
	string                 m_CustomSemantics;
	string                 m_Name;
	bool                   m_bEditable;
	bool                   m_bWasAdded;
	SShaderGraphFunction*  m_pFunction;
	DynArray<SShaderParam> m_Properties;

	SShaderGraphNode()
	{
		m_eType = eGrNode_Unknown;
		m_eFormat = eGrNodeFormat_Unknown;
		m_eSemantic = eGrNodeIOSemantic_Unknown;
		m_bEditable = false;
		m_bWasAdded = false;
		m_pFunction = NULL;
	}
	~SShaderGraphNode();
};

typedef std::vector<SShaderGraphNode*> FXShaderGraphNodes;
typedef FXShaderGraphNodes::iterator   FXShaderGraphNodeItor;

struct SShaderGraphBlock
{
	EGrBlockType        m_eType;
	EGrBlockSamplerType m_eSamplerType;
	string              m_ClassName;
	FXShaderGraphNodes  m_Nodes;

	~SShaderGraphBlock();
};

typedef std::vector<SShaderGraphBlock*> FXShaderGraphBlocks;
typedef FXShaderGraphBlocks::iterator   FXShaderGraphBlocksItor;

#include <CryRenderer/RenderElements/RendElement.h>
#include "RenderObject.h"

#endif // _ISHADER

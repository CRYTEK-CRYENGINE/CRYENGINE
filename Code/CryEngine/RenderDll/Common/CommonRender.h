// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _BASERESOURCE_H_
#define _BASERESOURCE_H_

#include <CryMath/Cry_Math.h>

#include "Defs.h"
#include <CryMath/Cry_Color.h>
#include <CryMath/Cry_Camera.h>
#include <CryMemory/MemoryAccess.h>

#include <CryMemory/STLGlobalAllocator.h>

#include "XRenderD3D9/DeviceManager/DeviceFormats.h" // SPixFormat

#define VSCONST_INSTDATA                0
#define VSCONST_SKINMATRIX              0
#define VSCONST_NOISE_TABLE             0
#define NUM_MAX_BONES_PER_GROUP         (250)
#define NUM_MAX_BONES_PER_GROUP_WITH_MB (125)

//////////////////////////////////////////////////////////////////////
class CRenderer;
extern CRenderer* gRenDev;

//////////////////////////////////////////////////////////////////////

#define CR_LITTLE_ENDIAN

struct SWaveForm;

extern bool gbRgb;

inline DWORD COLCONV(DWORD clr)
{
	return ((clr & 0xff00ff00) | ((clr & 0xff0000) >> 16) | ((clr & 0xff) << 16));
}
inline void COLCONV(ColorF& col)
{
	float v = col[0];
	col[0] = col[2];
	col[2] = v;
}

inline void f2d(double* dst, float* src)
{
	for (int i = 0; i < 16; i++)
	{
		dst[i] = src[i];
	}
}

inline void d2f(float* dst, double* src)
{
	for (int i = 0; i < 16; i++)
	{
		dst[i] = (float)src[i];
	}
}

//////////////////////////////////////////////////////////////////////
// Resource conventions

enum EHWShaderClass : uint8
{
	eHWSC_Vertex   = 0,
	eHWSC_Pixel    = 1,
	eHWSC_Geometry = 2,
	eHWSC_Domain   = 3,
	eHWSC_Hull     = 4,
	eHWSC_NumGfx   = 5,

	eHWSC_Compute  = 5,
	eHWSC_Num      = 6
};

enum EShaderStage : uint8
{
	EShaderStage_Vertex            = BIT(eHWSC_Vertex),
	EShaderStage_Pixel             = BIT(eHWSC_Pixel),
	EShaderStage_Geometry          = BIT(eHWSC_Geometry),
	EShaderStage_Domain            = BIT(eHWSC_Domain),
	EShaderStage_Hull              = BIT(eHWSC_Hull),
	EShaderStage_Compute           = BIT(eHWSC_Compute),

	EShaderStage_CountGfx          = eHWSC_NumGfx,
	EShaderStage_Count             = eHWSC_Num,
	EShaderStage_None              = 0,
	EShaderStage_All               = EShaderStage_Vertex | EShaderStage_Pixel | EShaderStage_Geometry | EShaderStage_Domain | EShaderStage_Hull | EShaderStage_Compute,
	EShaderStage_AllWithoutCompute = EShaderStage_Vertex | EShaderStage_Pixel | EShaderStage_Geometry | EShaderStage_Domain | EShaderStage_Hull
};
DEFINE_ENUM_FLAG_OPERATORS(EShaderStage)
#define SHADERSTAGE_FROM_SHADERCLASS(SHADERCLASS) ::EShaderStage(BIT(SHADERCLASS))
#define SHADERSTAGE_FROM_SHADERCLASS_CONDITIONAL(SHADERCLASS, SET) ::EShaderStage((SET) << (SHADERCLASS))

enum EConstantBufferShaderSlot
{
	// Z/G/S-Buffer, Forward
	eConstantBufferShaderSlot_PerDraw                   = 0, // EShaderStage_All
	eConstantBufferShaderSlot_PerMaterial               = 1, // EShaderStage_All
	eConstantBufferShaderSlot_SkinQuat                  = 2, // EShaderStage_Vertex
	eConstantBufferShaderSlot_SkinQuatPrev              = 3, // EShaderStage_Vertex
	eConstantBufferShaderSlot_PerGroup                  = 4, // EShaderStage_Vertex | EShaderStage_Hull
	eConstantBufferShaderSlot_PerPass                   = 5, // EShaderStage_All
	eConstantBufferShaderSlot_PerView                   = 6, // EShaderStage_All
	eConstantBufferShaderSlot_VrProjection              = 7,
	
	// Scaleform
	eConstantBufferShaderSlot_ScaleformMeshAttributes   = 0, // EShaderStage_Vertex
	eConstantBufferShaderSlot_ScaleformRenderParameters = 0, // EShaderStage_Pixel

	// Primitive/Custom/Post
	eConstantBufferShaderSlot_PerPrimitive              = eConstantBufferShaderSlot_PerDraw,

	eConstantBufferShaderSlot_Max                       = 7,
	eConstantBufferShaderSlot_Count                     = 8,
};

enum EResourceLayoutSlot
{
	EResourceLayoutSlot_PerDrawCB                       = 0, // EShaderStage_Vertex | EShaderStage_Pixel | EShaderStage_Domain
	EResourceLayoutSlot_PerDrawExtraRS                  = 1,
	EResourceLayoutSlot_PerMaterialRS                   = 2,
	EResourceLayoutSlot_PerPassRS                       = 3,
	EResourceLayoutSlot_VrProjectionCB                  = 4,

	EResourceLayoutSlot_Max                             = 7,
	EResourceLayoutSlot_Num                             = 8
};

enum EReservedTextureSlot
{
	// Z/G/S-Buffer
	EReservedTextureSlot_SkinExtraWeights               = 14, // EShaderStage_Vertex (mutually exclusive with ComputeSkinVerts)
	EReservedTextureSlot_ComputeSkinVerts               = 14, // EShaderStage_Vertex (mutually exclusive with SkinExtraWeights)
	EReservedTextureSlot_GpuParticleStream              = 14, // EShaderStage_Vertex

	EReservedTextureSlot_DrawInstancingData             = 15, // EShaderStage_Vertex | EShaderStage_Pixel
	EReservedTextureSlot_AdjacencyInfo                  = 16, // EShaderStage_Domain
	EReservedTextureSlot_TerrainBaseMap                 = 29, // EShaderStage_Pixel (set where?)
	
	// Forward
	EReservedTextureSlot_LightvolumeInfos               = 33,
	EReservedTextureSlot_LightVolumeRanges              = 34,

	// Custom/Post
	EReservedTextureSlot_ParticlePositionStream         = 35,
	EReservedTextureSlot_ParticleAxesStream             = 36,
	EReservedTextureSlot_ParticleColorSTStream          = 37,
};

////////////////////////////////////////////////////////////////////////////
// ResourceView API

struct ResourceViewHandle
{
	typedef uint8 ValueType;
	ValueType value;

	constexpr ResourceViewHandle() : value(Unspecified) { }
	constexpr ResourceViewHandle(ValueType v) : value(v) { }

	// Test operators
	template<typename T> bool operator ==(const T other) const { return value == other; }
	template<typename T> bool operator !=(const T other) const { return value != other; }
	// Range operators
	template<typename T> bool operator <=(const T other) const { return value <= other; }
	template<typename T> bool operator >=(const T other) const { return value >= other; }
	// Sorting operators
	template<typename T> bool operator < (const T other) const { return value <  other; }
	template<typename T> bool operator > (const T other) const { return value >  other; }

	// Auto cast for array access operator []
	operator ValueType() const { return value; }

	enum PreDefs : ValueType
	{
		Unspecified = ValueType(~0),
	};
};
static_assert(sizeof(ResourceViewHandle::ValueType) == sizeof(ResourceViewHandle), "ResourceViewHandle is suppose to be as small as the base type");

struct SResourceView
{
	typedef uint64 KeyType;

	enum ResourceViewType
	{
		eShaderResourceView = 0,
		eRenderTargetView,
		eDepthStencilView,
		eUnorderedAccessView,

		eNumResourceViews
	};

	enum ResourceViewFlags
	{
		eSRV_DepthOnly   = 0,
		eSRV_StencilOnly = 1,

		eDSV_ReadWrite   = 0,
		eDSV_ReadOnly    = 1,

		eUAV_WriteOnly   = 0,
		eUAV_ReadWrite   = 1,
	};

	union ResourceViewDesc
	{
		// Texture View configuration
		struct
		{
			// 12 bits CommonHeader
			uint64 eViewType        : 3;
			uint64 nFormat          : 7;
			uint64 nFlags           : 2;

			uint64 bSrgbRead        : 1; // TODO: convert to flags sRGBRead | sRGBWrite for supporting sRGB render-target write
			uint64 bMultisample     : 1;

			uint64 nFirstSlice      : 11;
			uint64 nSliceCount      : 11;
			uint64 nMostDetailedMip : 4;
			uint64 nMipCount        : 4;
			
			uint64 nUnused          : 20;
		};

		// Buffer View configuration
		struct
		{
			uint64 nCommonHeader    : 12;

			uint64 bRaw             : 1;

			uint64 nOffsetBits      : 5;  // up to 2^2^5 -> 2^32 bits for the offset
			uint64 nOffsetAndSize   : 46; // N bits offset, then m bits size, both are given as element-addresses
		};

		KeyType Key;
	};

	SResourceView(uint64 nKey = 0)
	{
		static_assert(sizeof(m_Desc) <= sizeof(KeyType),"SResourceView: sizeof(m_Desc) <= sizeof(KeyType)");

		m_Desc.Key = nKey;
	}

	static SResourceView ShaderResourceView (DXGI_FORMAT nFormat, int nFirstSlice = 0, int nSliceCount = -1, int nMostDetailedMip = 0, int nMipCount = -1, bool bSrgbRead = false, bool bMultisample = false, int nFlags = 0);
	static SResourceView RenderTargetView   (DXGI_FORMAT nFormat, int nFirstSlice = 0, int nSliceCount = -1, int nMipLevel = 0, bool bMultisample = false);
	static SResourceView DepthStencilView   (DXGI_FORMAT nFormat, int nFirstSlice = 0, int nSliceCount = -1, int nMipLevel = 0, bool bMultisample = false, int nFlags = 0);
	static SResourceView UnorderedAccessView(DXGI_FORMAT nFormat, int nFirstSlice = 0, int nSliceCount = -1, int nMipLevel = 0, int nFlags = 0);
	
	static SResourceView ShaderResourceRawView (DXGI_FORMAT nFormat, int nFirstElement = 0, int nElementCount = -1, int nFlags = 0);
	static SResourceView RenderTargetRawView   (DXGI_FORMAT nFormat, int nFirstElement = 0, int nElementCount = -1);
	static SResourceView DepthStencilRawView   (DXGI_FORMAT nFormat, int nFirstElement = 0, int nElementCount = -1, int nFlags = 0);
	static SResourceView UnorderedAccessRawView(DXGI_FORMAT nFormat, int nFirstElement = 0, int nElementCount = -1, int nFlags = 0);

	static bool          IsMultisampled       (KeyType key) { return SResourceView(key).m_Desc.bMultisample; }
	static bool          IsShaderResourceView (KeyType key) { return SResourceView(key).m_Desc.eViewType == eShaderResourceView; }
	static bool          IsRenderTargetView   (KeyType key) { return SResourceView(key).m_Desc.eViewType == eRenderTargetView; }
	static bool          IsDepthStencilView   (KeyType key) { return SResourceView(key).m_Desc.eViewType == eDepthStencilView; }
	static bool          IsUnorderedAccessView(KeyType key) { return SResourceView(key).m_Desc.eViewType == eUnorderedAccessView; }

	bool                 operator ==(const SResourceView& other) const { return m_Desc.Key == other.m_Desc.Key; }
	bool                 operator !=(const SResourceView& other) const { return m_Desc.Key != other.m_Desc.Key; }

	ResourceViewDesc m_Desc;
};
static_assert(sizeof(SResourceView::KeyType) == sizeof(SResourceView), "SResourceView is suppose to be as small as the base type");

// TODO: Move to DeviceObjects.h
struct EDefaultResourceViews : ResourceViewHandle
{
	enum PreDefs : ValueType
	{
		Default            = 0,
		Alternative        = 1,

		RasterizerTarget   = 2,
		UnorderedAccess    = 3,

		PreAllocated       = 4, // from this value and up custom resource views are assigned

		// aliases for RGB data
		Linear             = Default,
		sRGB               = Alternative,

		// aliases for DepthStencil data
		DepthOnly          = Default,
		StencilOnly        = Alternative,

		// aliases for RasterizerTarget data
		RenderTarget       = RasterizerTarget,
		DepthStencil       = RasterizerTarget
	};
};
static_assert(sizeof(EDefaultResourceViews::ValueType) == sizeof(EDefaultResourceViews), "EDefaultResourceViews is suppose to be as small as the base type");

////////////////////////////////////////////////////////////////////////////
// SamplerState API (pre-allocated sampler states)

enum ESamplerReduction
{
	eSamplerReduction_Standard   = 0,
	eSamplerReduction_Comparison = 1,
	eSamplerReduction_Minimum    = 2,
	eSamplerReduction_Maximum    = 3,
};

enum ESamplerFilter
{
	eSamplerFilter_Point  = 0,
	eSamplerFilter_Linear = 1,
};

enum ESamplerAddressMode
{
	eSamplerAddressMode_Wrap   = 0,
	eSamplerAddressMode_Mirror = 1,
	eSamplerAddressMode_Clamp  = 2,
	eSamplerAddressMode_Border = 3
};

/* Not yet used
enum ESamplerMaxAnisotropy
{
	eSamplerMaxAniso_1x  = 0, // equates "off"
	eSamplerMaxAniso_2x  = 1,
	eSamplerMaxAniso_3x  = 2,
	eSamplerMaxAniso_4x  = 3,
	eSamplerMaxAniso_6x  = 4,
	eSamplerMaxAniso_8x  = 5,
	eSamplerMaxAniso_12x = 6,
	eSamplerMaxAniso_16x = 7,
};
*/

struct SSamplerState
{
	// TODO: optimize size and squeeze everything in one 64bit word
	struct
	{
		//	signed char m_nReduction  : 2; // 2 bit, ESamplerReduction

		signed char m_nMinFilter  : 8; // Currently: FILTER_XXX [-1,7] / TODO: Make 1 bit, ESamplerFilter
		signed char m_nMagFilter  : 8; // Currently: FILTER_XXX [-1,7] / TODO: Make 1 bit, ESamplerFilter
		signed char m_nMipFilter  : 8; // Currently: FILTER_XXX [-1,7] / TODO: Make 1 bit, ESamplerFilter

		signed char m_nAddressU   : 8; // Currently: ESamplerAddressMode / TODO: Make 2 bit
		signed char m_nAddressV   : 8; // Currently: ESamplerAddressMode / TODO: Make 2 bit
		signed char m_nAddressW   : 8; // Currently: ESamplerAddressMode / TODO: Make 2 bit

		signed char m_nAnisotropy : 8; // Currently: [0,16] / TODO: Make 3 bit, ESamplerMaxAnisotropy, (drop non-power-two values?)

		signed char m_nPadding    : 8;
	};

	DWORD m_dwBorderColor;                // TODO: 2 bit (index into palette [black, white, transp])
	float m_fMipLodBias;
	bool  m_bActive;                      // TODO: remove
	bool  m_bComparison;                  // TODO: deprecate, use m_nReduction, add comparison function
	bool  m_bSRGBLookup;                  // TODO: deprecate, use format
	byte  m_bPAD;                         // TODO: remove

	constexpr SSamplerState()
		: m_nMinFilter(FILTER_POINT)
		, m_nMagFilter(FILTER_POINT)
		, m_nMipFilter(FILTER_POINT)
		, m_nAddressU(eSamplerAddressMode_Wrap)
		, m_nAddressV(eSamplerAddressMode_Wrap)
		, m_nAddressW(eSamplerAddressMode_Wrap)
		, m_nAnisotropy(0)
		, m_dwBorderColor(0)
		, m_fMipLodBias(0.0f)
		, m_nPadding(0)
		, m_bSRGBLookup(false)
		, m_bActive(false)
		, m_bComparison(false)
		, m_bPAD(0)
	{
	}

	constexpr SSamplerState(int nFilter, bool bClamp)
		: m_nMinFilter(nFilter)
		, m_nMagFilter(nFilter)
		, m_nMipFilter(nFilter)
		, m_nAddressU(bClamp ? eSamplerAddressMode_Clamp : eSamplerAddressMode_Wrap)
		, m_nAddressV(bClamp ? eSamplerAddressMode_Clamp : eSamplerAddressMode_Wrap)
		, m_nAddressW(bClamp ? eSamplerAddressMode_Clamp : eSamplerAddressMode_Wrap)
		, m_nAnisotropy(ExtractAniso(nFilter))
		, m_dwBorderColor(0)
		, m_fMipLodBias(0.0f)
		, m_nPadding(0)
		, m_bSRGBLookup(false)
		, m_bActive(false)
		, m_bComparison(false)
		, m_bPAD(0)
	{
	}

	constexpr SSamplerState(int nFilter, ESamplerAddressMode nAddressU, ESamplerAddressMode nAddressV, ESamplerAddressMode nAddressW, unsigned int borderColor, bool bComparison = false)
		: m_nMinFilter(nFilter)
		, m_nMagFilter(nFilter)
		, m_nMipFilter(nFilter)
		, m_nAddressU(nAddressU)
		, m_nAddressV(nAddressV)
		, m_nAddressW(nAddressW)
		, m_nAnisotropy(ExtractAniso(nFilter))
		, m_dwBorderColor(borderColor)
		, m_fMipLodBias(0.0f)
		, m_nPadding(0)
		, m_bSRGBLookup(false)
		, m_bActive(false)
		, m_bComparison(bComparison)
		, m_bPAD(0)
	{
	}

	constexpr SSamplerState(const SSamplerState& src)
		: m_nMinFilter(src.m_nMinFilter)
		, m_nMagFilter(src.m_nMagFilter)
		, m_nMipFilter(src.m_nMipFilter)
		, m_nAddressU(src.m_nAddressU)
		, m_nAddressV(src.m_nAddressV)
		, m_nAddressW(src.m_nAddressW)
		, m_nAnisotropy(src.m_nAnisotropy)
		, m_dwBorderColor(src.m_dwBorderColor)
		, m_fMipLodBias(0.0f)
		, m_nPadding(src.m_nPadding)
		, m_bSRGBLookup(src.m_bSRGBLookup)
		, m_bActive(src.m_bActive)
		, m_bComparison(src.m_bComparison)
		, m_bPAD(src.m_bPAD)
	{
	}

	SSamplerState& operator=(const SSamplerState& src)
	{
		this->~SSamplerState();
		new(this)SSamplerState(src);
		return *this;
	}
	inline friend bool operator==(const SSamplerState& m1, const SSamplerState& m2)
	{
		return *(uint64*)&m1 == *(uint64*)&m2 && m1.m_dwBorderColor == m2.m_dwBorderColor && m1.m_fMipLodBias == m2.m_fMipLodBias &&
			   m1.m_bActive == m2.m_bActive && m1.m_bComparison == m2.m_bComparison && m1.m_bSRGBLookup == m2.m_bSRGBLookup;
	}
	void Release()
	{
		delete this;
	}

	bool SetFilterMode(int nFilter);
	void SetClampMode(ESamplerAddressMode nAddressU, ESamplerAddressMode nAddressV, ESamplerAddressMode nAddressW);
	void SetBorderColor(DWORD dwColor);
	void SetMipLodBias(float fMipLodBias);
	void SetComparisonFilter(bool bEnable);

	// TODO: deprecate global state based sampler state configuration
	static SSamplerState s_sDefState;
	static bool SetDefaultFilterMode(int nFilter);
	static void SetDefaultClampingMode(ESamplerAddressMode nAddressU, ESamplerAddressMode nAddressV, ESamplerAddressMode nAddressW);

private:
	static constexpr signed char ExtractAniso(int nFilter)
	{
		return nFilter == FILTER_ANISO2X ? 2 :
		       nFilter == FILTER_ANISO4X ? 4 :
		       nFilter == FILTER_ANISO8X ? 8 :
		       nFilter == FILTER_ANISO16X ? 16 :
		       0;
	}
};

// TODO: Move to DeviceObjects.h
struct EDefaultSamplerStates : SamplerStateHandle
{
	enum PreDefs : ValueType
	{
		PointClamp            = 0,
		PointWrap             = 1,
		PointBorder_Black     = 2,
		PointBorder_White     = 3,
		PointCompare          = 4,
		LinearClamp           = 5,
		LinearWrap            = 6,
		LinearBorder_Black    = 7,
		LinearCompare         = 8,
		BilinearClamp         = 9,
		BilinearWrap          = 10,
		BilinearBorder_Black  = 11,
		BilinearCompare       = 12,
		TrilinearClamp        = 13,
		TrilinearWrap         = 14,
		TrilinearBorder_Black = 15,
		TrilinearBorder_White = 16,

		PreAllocated          = 17 // from this value and up custom sampler states are assigned
	};
};
static_assert(sizeof(EDefaultSamplerStates::ValueType) == sizeof(EDefaultSamplerStates), "EDefaultSamplerStates is suppose to be as small as the base type");

////////////////////////////////////////////////////////////////////////////
// InputLayout API

struct SShaderBlob
{
	void* m_pShaderData;
	int   m_nDataSize;
};

struct SInputLayout
{
	std::vector<D3D11_INPUT_ELEMENT_DESC> m_Declaration;			 // Configuration
	uint16                                m_firstSlot;
	std::vector<uint16>                   m_Strides;				 // Stride of each input slot, starting from m_firstSlot
	std::array<int8, 4>                   m_Offsets;				 // The offsets of "POSITION", "COLOR", "TEXCOORD" and "NORMAL"

	enum
	{
		eOffset_Position,
		eOffset_Color,
		eOffset_TexCoord,
		eOffset_Normal,
	};

	SInputLayout(std::vector<D3D11_INPUT_ELEMENT_DESC> &&decs) : m_Declaration(std::move(decs))
	{
		// Calculate first slot index
		m_firstSlot = std::numeric_limits<uint16>::max();
		for (const auto &dec : m_Declaration)
			m_firstSlot = std::min(m_firstSlot, static_cast<uint16>(dec.InputSlot));

		// Calculate strides
		for (const auto &dec : m_Declaration)
		{
			const uint16 slot = dec.InputSlot - m_firstSlot;
			if (m_Strides.size() <= slot)
				m_Strides.resize(slot + 1, 0);

			m_Strides[slot] = std::max(m_Strides[slot], uint16(dec.AlignedByteOffset + DeviceFormats::GetStride(dec.Format)));
		}

		// Calculate offsets
		m_Offsets[eOffset_Position] = m_Offsets[eOffset_Color] = m_Offsets[eOffset_TexCoord] = m_Offsets[eOffset_Normal] = -1;
		for (int n = 0; n < m_Declaration.size(); ++n)
		{
			if (!m_Declaration[n].SemanticName)
				continue;

			if ((m_Offsets[eOffset_Position] == -1) && (!stricmp(m_Declaration[n].SemanticName, "POSITION")))
				m_Offsets[eOffset_Position] = m_Declaration[n].AlignedByteOffset;
			if ((m_Offsets[eOffset_Color] == -1) && (!stricmp(m_Declaration[n].SemanticName, "COLOR")))
				m_Offsets[eOffset_Color] = m_Declaration[n].AlignedByteOffset;
			if ((m_Offsets[eOffset_TexCoord] == -1) && (!stricmp(m_Declaration[n].SemanticName, "TEXCOORD")))
				m_Offsets[eOffset_TexCoord] = m_Declaration[n].AlignedByteOffset;
			if ((m_Offsets[eOffset_Normal] == -1) && (!stricmp(m_Declaration[n].SemanticName, "NORMAL") || !stricmp(m_Declaration[n].SemanticName, "TANGENT")))
				m_Offsets[eOffset_Normal] = m_Declaration[n].AlignedByteOffset;
		}
	}

	SInputLayout(const SInputLayout& src) = default;
	SInputLayout(SInputLayout&& src) = default;
	SInputLayout& operator=(const SInputLayout& src) = default;
	SInputLayout& operator=(SInputLayout&& src) = default;
};

//=================================================================
// C++11 POD version of "typedef std::variant<void*, CBaseResource*, CTexture*, CGpuBuffer*, CConstantBuffer*> UResourceReference;"

class CBaseResource;
class CTexture;
class CGpuBuffer;
class CConstantBuffer;

union UResourceReference
{
	void*              pAnonymous;
	CBaseResource*     pResource;
	CTexture*          pTexture;
	CGpuBuffer*        pBuffer;
	CConstantBuffer*   pConstantBuffer;
	SamplerStateHandle samplerState;

	ILINE UResourceReference(void*              other) : pAnonymous     (other) { }
	ILINE UResourceReference(CBaseResource*     other) : pResource      (other) { }
	ILINE UResourceReference(CTexture*          other) : pTexture       (other) { }
	ILINE UResourceReference(CGpuBuffer*        other) : pBuffer        (other) { }
	ILINE UResourceReference(CConstantBuffer*   other) : pConstantBuffer(other) { }
	ILINE UResourceReference(SamplerStateHandle other) : samplerState   (other) { }

	ILINE UResourceReference& operator=(void*              other) { pAnonymous      = other; return *this; }
	ILINE UResourceReference& operator=(CBaseResource*     other) { pResource       = other; return *this; }
	ILINE UResourceReference& operator=(CTexture*          other) { pTexture        = other; return *this; }
	ILINE UResourceReference& operator=(CGpuBuffer*        other) { pBuffer         = other; return *this; }
	ILINE UResourceReference& operator=(CConstantBuffer*   other) { pConstantBuffer = other; return *this; }
	ILINE UResourceReference& operator=(SamplerStateHandle other) { samplerState    = other; return *this; }
};

//=================================================================
// Resource-Binding and Invalidation API

struct SResourceBindPoint;
struct SResourceBinding;

// Will notify resource's user that some data of the the resource was invalidated.
// dirtyFlags - one or more of the EResourceDirtyFlags enum bits
//! Dirty flags will indicate what kind of device data was invalidated
enum EResourceDirtyFlags
{
	eDeviceResourceDirty     = BIT(0),
	eDeviceResourceViewDirty = BIT(1),

	eResourceDestroyed       = BIT(2),
};

struct SResourceBindPoint
{
	enum class EFlags : uint8
	{
		None         = 0,
		IsTexture    = BIT(0), // need to distinguish between textures and buffers on vulkan
		IsStructured = BIT(1)  // need to distinguish between structured and typed resources on vulkan as they produce different descriptors
	};

	enum class ESlotType : uint8 // NOTE: enum values need to match ResourceGroup enum from hlslcc and request enum from hlsl2spirv
	{
		ConstantBuffer      = 0, // HLSL b slot
		TextureAndBuffer    = 1, // HLSL t slot
		Sampler             = 2, // HLSL s slot
		UnorderedAccessView = 3, // HLSL u slot

		Count,

		InvalidSlotType
	};

	SResourceBindPoint() : fastCompare(0) {}
	SResourceBindPoint(const SResourceBinding& resource, uint8 slotNumber, EShaderStage shaderStages);
	SResourceBindPoint(ESlotType type, uint8 slotNumber, EShaderStage shaderStages, EFlags flags = EFlags::None);

	// ignore flags in all comparators (NOTE/TODO: shouldn't we ignore the stages as well?)
	bool operator<(const SResourceBindPoint& other) const noexcept
	{
		constexpr uint32 flagsMask = ~(0xFF << (offsetof(SResourceBindPoint, flags) * 8));
		return (fastCompare & flagsMask) < (other.fastCompare & flagsMask);
	}

	bool operator==(const SResourceBindPoint& other) const noexcept
	{
		constexpr uint32 flagsMask = ~(0xFF << (offsetof(SResourceBindPoint, flags) * 8));
		return (fastCompare & flagsMask) == (other.fastCompare & flagsMask);
	}
	bool operator!=(const SResourceBindPoint& other) const noexcept { return !(*this == other); }

	union
	{
		struct
		{
			EShaderStage    stages;
			EFlags          flags;
			uint8           slotNumber;
			ESlotType       slotType;
		};

		uint32 fastCompare;
	};
};
static_assert(sizeof(SResourceBindPoint::fastCompare) == sizeof(SResourceBindPoint), "Size mismatch between fastCompare and bind point struct");

DEFINE_ENUM_FLAG_OPERATORS(SResourceBindPoint::EFlags)

struct SResourceBinding
{
	typedef bool InvalidateCallbackSignature(void*, SResourceBindPoint, UResourceReference, uint32);
	typedef std::function<InvalidateCallbackSignature> InvalidateCallbackFunction;

	enum class EResourceType : uint8
	{
		InvalidType = 0,

		ConstantBuffer,
		Texture,
		Buffer,
		Sampler,
		Resource,
	};

	inline SResourceBinding()
		: fastCompare(0)
		, type(SResourceBinding::EResourceType::InvalidType)
	{}

	inline SResourceBinding(CTexture* _pTexture, ResourceViewHandle _view)
		: pTexture(_pTexture)
		, view(_view)
		, type(EResourceType::Texture)
	{}

	inline SResourceBinding(CGpuBuffer* _pBuffer, ResourceViewHandle _view)
		: pBuffer(_pBuffer)
		, view(_view)
		, type(EResourceType::Buffer)
	{}

	inline SResourceBinding(CConstantBuffer* _pConstantBuffer, ResourceViewHandle _view)
		: pConstantBuffer(_pConstantBuffer)
		, view(_view)
		, type(EResourceType::ConstantBuffer)
	{}

	inline SResourceBinding(SamplerStateHandle _samplerState)
		: fastCompare(0)
		, type(EResourceType::Sampler)
	{
		samplerState = _samplerState;
	}

	inline SResourceBinding(CBaseResource* _pResource)
		: pResource(_pResource)
		, type(EResourceType::Resource)
	{}

	bool IsValid() const;
	bool IsVolatile() const;
	void AddInvalidateCallback(void* pCallbackOwner, SResourceBindPoint bindPoint, const InvalidateCallbackFunction& callback)  threadsafe const;
	void RemoveInvalidateCallback(void* pCallbackOwner, SResourceBindPoint bindPoint = SResourceBindPoint()) threadsafe const;

	const std::pair<SResourceView, CDeviceResourceView*>* GetDeviceResourceViewInfo() const;
	template<typename T> T*                               GetDeviceResourceView() const;
	DXGI_FORMAT                                           GetResourceFormat() const;

	bool operator==(const SResourceBinding& other) const { return fastCompare == other.fastCompare && view == other.view && type == other.type; }

	union
	{
		CTexture*          pTexture;
		CGpuBuffer*        pBuffer;
		CConstantBuffer*   pConstantBuffer;
		SamplerStateHandle samplerState;
		CBaseResource*     pResource;

		uintptr_t          fastCompare;
	};

	ResourceViewHandle view;
	EResourceType      type;
};

class CResourceBindingInvalidator
{

private:
	struct SInvalidateCallback
	{
		int refCount;
		SResourceBindPoint bindpoint;
		SResourceBinding::InvalidateCallbackFunction callback;

		SInvalidateCallback(SResourceBindPoint bindpoint)
			: refCount(0)
			, bindpoint(bindpoint)
		{}
		SInvalidateCallback(const SResourceBinding::InvalidateCallbackFunction& cb, SResourceBindPoint bindpoint)
			: refCount(0)
			, bindpoint(bindpoint)
			, callback(cb)
		{}
		bool operator<(const SInvalidateCallback &rhs) const noexcept { return bindpoint < rhs.bindpoint; }
		bool operator<(const SResourceBindPoint &rhs) const noexcept { return bindpoint < rhs; }
	};

	std::unordered_map<void*, std::vector<SInvalidateCallback>> m_invalidateCallbacks;
	CryRWLock                                                   m_invalidationLock;

public:
	CResourceBindingInvalidator() { }
	virtual ~CResourceBindingInvalidator() { CRY_ASSERT_MESSAGE(m_invalidateCallbacks.empty(), "Make sure any clients (e.g. Renderpasses, resource sets, etc..) are released before destroying this resource"); }

	size_t CountInvalidateCallbacks() threadsafe;
	void AddInvalidateCallback(void* listener, const SResourceBindPoint bindPoint, const SResourceBinding::InvalidateCallbackFunction& callback) threadsafe;
	void RemoveInvalidateCallbacks(void* listener, const SResourceBindPoint bindPoint = SResourceBindPoint()) threadsafe;
	void InvalidateDeviceResource(CTexture* pTexture, uint32 dirtyFlags) threadsafe;
	void InvalidateDeviceResource(CGpuBuffer* pBuffer, uint32 dirtyFlags) threadsafe;
	void InvalidateDeviceResource(UResourceReference pResource, uint32 dirtyFlags) threadsafe;
};

////////////////////////////////////////////////////////////////////////////
// Resource and Resource-Directory API

typedef std::map<CCryNameTSCRC, CBaseResource*>                              ResourcesMap;
typedef ResourcesMap::iterator                                               ResourcesMapItor;

typedef std::vector<CBaseResource*, stl::STLGlobalAllocator<CBaseResource*>> ResourcesList;
typedef std::vector<int, stl::STLGlobalAllocator<int>>                       ResourceIds;

struct SResourceContainer
{
	ResourcesList m_RList;             // List of objects for acces by Id's
	ResourcesMap  m_RMap;              // Map of objects for fast searching
	ResourceIds   m_AvailableIDs;      // Available object Id's for efficient ID's assigning after deleting

	SResourceContainer()
	{
		m_RList.reserve(512);
	}

	~SResourceContainer();

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddObject(m_RList);
		pSizer->AddObject(m_RMap);
		pSizer->AddObject(m_AvailableIDs);
	}
};

typedef std::map<CCryNameTSCRC, SResourceContainer*> ResourceClassMap;
typedef ResourceClassMap::iterator                   ResourceClassMapItor;

class CBaseResource : NoCopy, public CResourceBindingInvalidator
{
private:
	// Per resource variables
	volatile int32          m_nRefCount;
	int                     m_nID;
	CCryNameTSCRC           m_ClassName;
	CCryNameTSCRC           m_NameCRC;

	static ResourceClassMap m_sResources;

	bool                    m_bDeleted = false;

public:
	static CryRWLock        s_cResLock;

private:
	void UnregisterAndDelete();

public:
	// CCryUnknown interface
	inline void   SetRefCounter(int nRefCounter) { m_nRefCount = nRefCounter; }
	virtual int   GetRefCounter() const { return m_nRefCount; }
	virtual int32 AddRef()
	{
		int32 nRef = CryInterlockedIncrement(&m_nRefCount);
		return nRef;
	}
	virtual int32 Release()
	{
		// TODO: simplify, it's making ref-counting on CTexture much more expensive than it needs to be
		IF(m_nRefCount > 0, 1)
		{
			int32 nRef = CryInterlockedDecrement(&m_nRefCount);
			if (nRef < 0)
			{
				CryFatalError("CBaseResource::Release() called more than once!");
			}

			if (nRef == 0)
			{
				UnregisterAndDelete();
				return 0;
			}
			return nRef;
		}
		return 0;
	}

	// Increment ref count, if not already scheduled for destruction.
	int32 TryAddRef()
	{
		volatile int nOldRef, nNewRef;
		do
		{
			nOldRef = m_nRefCount;
			if (nOldRef == 0)
				return 0;
			nNewRef = nOldRef + 1;
		}
		while (CryInterlockedCompareExchange(alias_cast<volatile LONG*>(&m_nRefCount), nNewRef, nOldRef) != nOldRef);
		return nNewRef;
	}

	// Constructors.
	CBaseResource() : m_nRefCount(1), m_nID(0) {}

	// Destructor.
	virtual ~CBaseResource() { };

	CCryNameTSCRC GetNameCRC() const { return m_NameCRC; }
	//inline const char *GetName() const { return m_Name.c_str(); }
	//inline const char *GetClassName() const { return m_ClassName.c_str(); }
	inline int                 GetID() const  { return m_nID; }
	inline void                SetID(int nID) { m_nID = nID; }

	virtual bool               IsValid() const;

	static ILINE int           RListIndexFromId(int id)  { return id - 1; }
	static ILINE int           IdFromRListIndex(int idx) { return idx + 1; }

	static ResourceClassMap&   GetMaps()                 { return m_sResources; }
	static CBaseResource*      GetResource(const CCryNameTSCRC& className, int nID, bool bAddRef);
	static CBaseResource*      GetResource(const CCryNameTSCRC& className, const CCryNameTSCRC& Name, bool bAddRef);
	static SResourceContainer* GetResourcesForClass(const CCryNameTSCRC& className);
	static void                ShutDown();

	bool                       Register(const CCryNameTSCRC& resName, const CCryNameTSCRC& Name);
	bool                       UnRegister();

	virtual void               GetMemoryUsage(ICrySizer* pSizer) const = 0;
};

#endif

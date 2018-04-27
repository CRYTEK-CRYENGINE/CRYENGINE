// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GLResource.hpp
//  Version:     v1.00
//  Created:     30/04/2013 by Valerio Guagliumi.
//  Description: Declares the resource types and related functions
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __GLRESOURCE__
#define __GLRESOURCE__

#include "GLCommon.hpp"
#include "GLFormat.hpp"

#define DXGL_USE_PBO_FOR_STAGING_TEXTURES  !DXGL_FULL_EMULATION
#define DXGL_SHARED_OBJECT_SYNCHRONIZATION !OGL_SINGLE_CONTEXT
#define DXGL_STREAMING_CONSTANT_BUFFERS    1

namespace NCryOpenGL
{

class CDevice;
class CContext;
struct STextureUnitContext;
struct SSamplerState;
struct SShaderTextureViewConfiguration;
struct SShaderTextureBufferViewConfiguration;
struct SShaderImageViewConfiguration;
struct SOutputMergerTextureViewConfiguration;

DXGL_DECLARE_PTR(struct, SFrameBuffer);
DXGL_DECLARE_PTR(struct, SOutputMergerView);
DXGL_DECLARE_PTR(struct, SOutputMergerTextureView);
DXGL_DECLARE_PTR(struct, STexture);
DXGL_DECLARE_PTR(struct, SShaderView);
DXGL_DECLARE_PTR(struct, SShaderTextureBasedView);
DXGL_DECLARE_PTR(struct, SShaderTextureView);
DXGL_DECLARE_PTR(struct, SShaderTextureBufferView);
DXGL_DECLARE_PTR(struct, SShaderImageView);
DXGL_DECLARE_PTR(struct, SShaderBufferView);
DXGL_DECLARE_PTR(struct, SDefaultFrameBufferTexture);
DXGL_DECLARE_PTR(struct, SBuffer);
DXGL_DECLARE_PTR(struct, SQuery);
#if DXGL_STREAMING_CONSTANT_BUFFERS
DXGL_DECLARE_PTR(struct, SContextFrame)
#endif //DXGL_STREAMING_CONSTANT_BUFFERS
#if OGL_SINGLE_CONTEXT
DXGL_DECLARE_PTR(struct, SInitialDataCopy);
#endif

#if OGL_SINGLE_CONTEXT
enum { MAX_NUM_CONTEXT_PER_DEVICE = 1 };
#else
enum { MAX_NUM_CONTEXT_PER_DEVICE = 32 };
#endif

struct SFrameBufferConfiguration
{
	enum
	{
		DEPTH_ATTACHMENT_INDEX       = 0,
		STENCIL_ATTACHMENT_INDEX     = 1,
		FIRST_COLOR_ATTACHMENT_INDEX = STENCIL_ATTACHMENT_INDEX + 1,
		MAX_COLOR_ATTACHMENTS        = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
		MAX_ATTACHMENTS              = FIRST_COLOR_ATTACHMENT_INDEX + MAX_COLOR_ATTACHMENTS
	};

	SOutputMergerViewPtr m_akAttachments[MAX_ATTACHMENTS];

	static GLenum AttachmentIndexToID(uint32 uIndex);
	static uint32 AttachmentIDToIndex(GLenum eID);
};

struct SSharingFence
{
#if DXGL_SHARED_OBJECT_SYNCHRONIZATION
	GLsync m_pFence;
	SBitMask<MAX_NUM_CONTEXT_PER_DEVICE, SSpinlockBitMaskWord> m_akWaitIssued;
#endif //DXGL_SHARED_OBJECT_SYNCHRONIZATION

	SSharingFence();
	~SSharingFence();

	void IssueFence(CDevice* pDevice, bool bAvoidFlushing = false);
	void IssueWait(CContext* pContext);
};

struct SResourceNamePool;

#if OGL_SINGLE_CONTEXT
typedef uint32 TResourceNameRefCount;
ILINE uint32 IncrementResourceNameRefCount(TResourceNameRefCount& uRefCount) { return ++uRefCount; }
ILINE uint32 DecrementResourceNameRefCount(TResourceNameRefCount& uRefCount) { return --uRefCount; }
#else
typedef volatile LONG TResourceNameRefCount;
ILINE LONG IncrementResourceNameRefCount(volatile TResourceNameRefCount& iRefCount) { return AtomicIncrement(&iRefCount); }
ILINE LONG DecrementResourceNameRefCount(volatile TResourceNameRefCount& iRefCount) { return AtomicDecrement(&iRefCount); }
#endif

struct UResourceNameSlot
{
	SList::TListEntry m_kNextFree;
	struct SUsed
	{
		GLuint                m_uName;
		UResourceNameSlot*    m_pBlock;
		TResourceNameRefCount m_uRefCount;
	} m_kUsed;
};

struct SResourceNameBlockHeader
{
	UResourceNameSlot*    m_pNext;
	SResourceNamePool*    m_pPool;
	TResourceNameRefCount m_uRefCount;
	SList                 m_kFreeList;
	uint32                m_uSize;
};

class CResourceName
{
public:
	CResourceName();
	CResourceName(UResourceNameSlot* pSlot);
	CResourceName(const CResourceName& kOther);
	~CResourceName();

	CResourceName& operator=(const CResourceName& kOther);
	bool           operator==(const CResourceName& kOther) const;
	bool           operator!=(const CResourceName& kOther) const;

	GLuint         GetName() const;
	bool           IsValid() const;
protected:
	void           Acquire();
	void           Release();

	UResourceNameSlot* m_pSlot;
};

struct SResourceNamePool
{
	typedef UResourceNameSlot        TSlot;
	typedef SResourceNameBlockHeader TBlockHeader;

	enum { MIN_BLOCK_SIZE = 256, GROWTH_FACTOR = 2, NUM_HEADER_SLOTS = (sizeof(TBlockHeader) + sizeof(TSlot) - 1) / sizeof(TSlot) };

	TSlot*           m_pBlocksHead;
	TCriticalSection m_kBlockSection;

	SResourceNamePool();
	~SResourceNamePool();

	CResourceName Create(GLuint uName);
};

inline CResourceName::CResourceName()
	: m_pSlot(NULL)
{
}

inline CResourceName::CResourceName(UResourceNameSlot* pSlot)
	: m_pSlot(pSlot)
{
	if (IncrementResourceNameRefCount(m_pSlot->m_kUsed.m_uRefCount) == 1)
		IncrementResourceNameRefCount(alias_cast<SResourceNameBlockHeader*>(m_pSlot->m_kUsed.m_pBlock)->m_uRefCount);
}

inline CResourceName::CResourceName(const CResourceName& kOther)
	: m_pSlot(kOther.m_pSlot)
{
	Acquire();
}

inline CResourceName::~CResourceName()
{
	Release();
}

inline CResourceName& CResourceName::operator=(const CResourceName& kOther)
{
	Release();
	m_pSlot = kOther.m_pSlot;
	Acquire();
	return *this;
}

inline bool CResourceName::operator==(const CResourceName& kOther) const
{
	return m_pSlot == kOther.m_pSlot;
}

inline bool CResourceName::operator!=(const CResourceName& kOther) const
{
	return !operator==(kOther);
}

inline void CResourceName::Acquire()
{
	if (m_pSlot != NULL && IncrementResourceNameRefCount(m_pSlot->m_kUsed.m_uRefCount) == 1)
		IncrementResourceNameRefCount(alias_cast<SResourceNameBlockHeader*>(m_pSlot->m_kUsed.m_pBlock)->m_uRefCount);
}

inline void CResourceName::Release()
{
	if (m_pSlot != NULL && DecrementResourceNameRefCount(m_pSlot->m_kUsed.m_uRefCount) == 0)
	{
		SResourceNameBlockHeader* pBlockHeader(alias_cast<SResourceNameBlockHeader*>(m_pSlot->m_kUsed.m_pBlock));
		if (DecrementResourceNameRefCount(pBlockHeader->m_uRefCount) == 0)
			delete[] m_pSlot->m_kUsed.m_pBlock;
		else
			pBlockHeader->m_kFreeList.Push(m_pSlot->m_kNextFree);
	}
}

inline GLuint CResourceName::GetName() const
{
	return m_pSlot == NULL ? 0 : m_pSlot->m_kUsed.m_uName;
}

inline bool CResourceName::IsValid() const
{
	return m_pSlot != NULL;
}

DXGL_DECLARE_REF_COUNTED(struct, SResource)
{
	typedef void (* UpdateSubresourceFunc) (SResource* pResource, uint32 uSubresource, const D3D11_BOX* pDstBox, const void* pSrcData, uint32 uSrcRowPitch, uint32 uSrcDepthPitch, CContext* pContext);
	typedef bool (* MapSubresourceFunc) (SResource* pResource, uint32 uSubresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource, CContext* pContext);
	typedef void (* UnmapSubresourceFunc) (SResource* pResource, uint32 uSubresource, CContext* pContext);

	CResourceName m_kName;
	UpdateSubresourceFunc m_pfUpdateSubresource;
	MapSubresourceFunc m_pfMapSubresource;
	UnmapSubresourceFunc m_pfUnmapSubresource;
	SSharingFence m_kCreationFence;

	SResource();
	SResource(const SResource &kOther);

	virtual ~SResource();
};

struct SFrameBufferObject
{
	typedef SBitMask<SFrameBufferConfiguration::MAX_COLOR_ATTACHMENTS, SUnsafeBitMaskWord> TColorAttachmentMask;

	CResourceName        m_kName;
	bool                 m_bUsesSRGB;
	TColorAttachmentMask m_kDrawMaskCache;

	SFrameBufferObject();
	~SFrameBufferObject();
};

DXGL_DECLARE_REF_COUNTED(struct, SFrameBuffer)
{
	SFrameBufferConfiguration m_kConfiguration;
	SFrameBufferObject m_kObject;
	CContext* m_pContext;

	// Specification of the list of color attachments that are enabled for writing
	GLenum m_aeDrawBuffers[SFrameBufferConfiguration::MAX_COLOR_ATTACHMENTS];
	GLuint m_uNumDrawBuffers;
	SFrameBufferObject::TColorAttachmentMask m_kDrawMask;

	SFrameBuffer(const SFrameBufferConfiguration &kConfiguration);
	~SFrameBuffer();

	bool Validate();
};

struct SMappedSubTexture
{
	uint8* m_pBuffer;
	uint32 m_uDataOffset;
	bool   m_bUpload;
	uint32 m_uRowPitch;
	uint32 m_uImagePitch;

	SMappedSubTexture()
		: m_pBuffer(NULL)
		, m_uDataOffset(0)
		, m_bUpload(false)
		, m_uRowPitch(0)
		, m_uImagePitch(0)
	{
	}
};

struct STextureState
{
	GLint        m_iBaseMipLevel;
	GLint        m_iMaxMipLevel;
#if DXGL_SUPPORT_STENCIL_TEXTURES
	GLint        m_iDepthStencilTextureMode; // 0 if not used
#endif                                     //DXGL_SUPPORT_STENCIL_TEXTURES
	TSwizzleMask m_uSwizzleMask;

	void ApplyFormatMode(GLuint uTexture, GLenum eTarget);
	void Apply(GLuint uTexture, GLenum eTarget);

	bool operator==(const STextureState& kOther) const
	{
		return
		  m_iBaseMipLevel == kOther.m_iBaseMipLevel &&
		  m_iMaxMipLevel == kOther.m_iMaxMipLevel &&
#if DXGL_SUPPORT_STENCIL_TEXTURES
		  m_iDepthStencilTextureMode == kOther.m_iDepthStencilTextureMode &&
#endif //DXGL_SUPPORT_STENCIL_TEXTURES
		  m_uSwizzleMask == kOther.m_uSwizzleMask;
	}

	bool operator!=(const STextureState& kOther) const
	{
		return !operator==(kOther);
	}
};

template<typename T>
struct STexVec
{
	T x, y, z;

	STexVec() {}
	STexVec(T kX, T kY, T kZ) : x(kX), y(kY), z(kZ) {}
};

typedef STexVec<GLsizei> STexSize;
typedef STexVec<GLint>   STexPos;

struct STexSubresourceID
{
	GLint  m_iMipLevel;
	uint32 m_uElement;
};

enum EBufferBinding
{
	eBB_Array,
	eBB_CopyRead,
	eBB_CopyWrite,
#if DXGL_SUPPORT_DRAW_INDIRECT
	eBB_DrawIndirect,
#endif //DXGL_SUPPORT_DRAW_INDIRECT
	eBB_ElementArray,
	eBB_PixelPack,
	eBB_PixelUnpack,
	eBB_TransformFeedback,
	eBB_UniformBuffer,
#if DXGL_SUPPORT_ATOMIC_COUNTERS
	eBB_AtomicCounter,
#endif //DXGL_SUPPORT_ATOMIC_COUNTERS
#if DXGL_SUPPORT_DISPATCH_INDIRECT
	eBB_DispachIndirect,
#endif //DXGL_SUPPORT_DISPATCH_INDIRECT
#if DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
	eBB_ShaderStorage,
#endif //DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
	eBB_NUM
};

struct SBufferRange
{
	uint32 m_uOffset;
	uint32 m_uSize;

	SBufferRange(uint32 uOffset = 0, uint32 uSize = 0)
		: m_uOffset(uOffset)
		, m_uSize(uSize)
	{
	}

	bool operator==(const SBufferRange& kOther) const
	{
		return
		  m_uOffset == kOther.m_uOffset &&
		  m_uSize == kOther.m_uSize;
	}

	bool operator!=(const SBufferRange& kOther) const
	{
		return !operator==(kOther);
	}
};

struct STexture : SResource
{
	STexture(GLsizei iWidth, GLsizei iHeight, GLsizei iDepth, GLenum eTarget, EGIFormat eFormat, uint32 uNumMipLevels, uint32 uNumElements);
	virtual ~STexture();

	virtual SShaderTextureViewPtr       CreateShaderResourceView(const SShaderTextureViewConfiguration& kConfiguration, CContext* pContext);
	virtual SShaderImageViewPtr         CreateUnorderedAccessView(const SShaderImageViewConfiguration& kConfiguration, CContext* pContext);
	virtual SOutputMergerTextureViewPtr CreateOutputMergerView(const SOutputMergerTextureViewConfiguration& kConfiguration, CContext* pContext);
	SOutputMergerTextureViewPtr         GetCompatibleOutputMergerView(const SOutputMergerTextureViewConfiguration& kConfiguration, CContext* pContext);

	virtual void                        OnCopyRead(CContext* pContext)  {};
	virtual void                        OnCopyWrite(CContext* pContext) {};

	typedef std::vector<SMappedSubTexture>           TMappedSubTextures;
#if !DXGL_SUPPORT_COPY_IMAGE
	typedef std::vector<SOutputMergerTextureViewPtr> TCopySubTextureViews;
#endif //!DXGL_SUPPORT_COPY_IMAGE
#if DXGL_USE_PBO_FOR_STAGING_TEXTURES || !DXGL_SUPPORT_COPY_IMAGE
	typedef void (*   TransferDataFunc) (STexture* pTexture, STexSubresourceID kSubID, STexPos kOffset, STexSize kSize, const SMappedSubTexture& kDataLocation, CContext* pContext);
	typedef uint32 (* LocatePackedDataFunc) (STexture* pTexture, STexSubresourceID kSubID, STexPos kOffset, SMappedSubTexture& kDataLocation);
#endif //DXGL_USE_PBO_FOR_STAGING_TEXTURES || !DXGL_SUPPORT_COPY_IMAGE

	GLsizei   m_iWidth, m_iHeight, m_iDepth;
	GLenum    m_eTarget;
	EGIFormat m_eFormat;
	uint32    m_uNumMipLevels;
	uint32    m_uNumElements; // array_size * number_of_faces

#if DXGL_SUPPORT_COPY_IMAGE
	// Texture view used for glCopyImageSubData if the driver requires a custom view for that, texture name otherwise
	CResourceName m_kCopyImageView;
	GLenum        m_eCopyImageTarget;
#endif //DXGL_SUPPORT_COPY_IMAGE

#if DXGL_USE_PBO_FOR_STAGING_TEXTURES || !DXGL_SUPPORT_COPY_IMAGE
	TransferDataFunc     m_pfUnpackData;
	TransferDataFunc     m_pfPackData;
	LocatePackedDataFunc m_pfLocatePackedDataFunc;
#endif //DXGL_USE_PBO_FOR_STAGING_TEXTURES || !DXGL_SUPPORT_COPY_IMAGE

#if DXGL_USE_PBO_FOR_STAGING_TEXTURES
	CResourceName* m_akPixelBuffers;  // Only used for staging textures
#else
	uint8*         m_pSystemMemoryCopy; // Only used for staging textures
#endif

	TMappedSubTextures   m_kMappedSubTextures;
#if !DXGL_SUPPORT_COPY_IMAGE || !DXGL_SUPPORT_GETTEXIMAGE
	TCopySubTextureViews m_kCopySubTextureViews;
#endif //!DXGL_SUPPORT_COPY_IMAGE

	STextureState             m_kCache;
	SShaderTextureView*       m_pShaderViewsHead;
	SOutputMergerTextureView* m_pOutputMergerViewsHead;
	SShaderTextureView*       m_pBoundModifier; // NULL if no SRV for this texture has bound custom texture parameters
};

#if DXGL_SUPPORT_APITRACE

struct SDynamicBufferCopy
{
	uint8* m_pData;
	uint32 m_uSize;

	#if CRY_PLATFORM_WINDOWS
	void**        m_apDirtyPages;
	uint32        m_uNumPages;
	static uint32 ms_uPageSize;
	#endif // CRY_PLATFORM_WINDOWS

	SDynamicBufferCopy();
	~SDynamicBufferCopy();

	uint8* Allocate(uint32 uSize);
	void   Free();
	void   ResetDirtyRegion();
	bool   GetDirtyRegion(uint32& uOffset, uint32& uSize);
};

#endif //DXGL_SUPPORT_APITRACE

struct SBuffer : SResource
{
	SBuffer();
	virtual ~SBuffer();

	SShaderViewPtr              CreateShaderResourceView(EGIFormat eFormat, uint32 uFirstElement, uint32 uNumElements, uint32 uFlags, CContext* pContext);
	SShaderViewPtr              CreateUnorderedAccessView(EGIFormat eFormat, uint32 uFirstElement, uint32 uNumElements, uint32 uFlags, CContext* pContext);

	SShaderTextureBufferViewPtr GetTextureBuffer(SShaderTextureBufferViewConfiguration& kConfiguration, CContext* pContext);
	SShaderBufferViewPtr        CreateBufferView(uint32 uFirstElement, uint32 uNumElements, CContext* pContext);

	typedef bool (* MapBufferRangeFunc) (SBuffer* pBuffer, size_t uOffset, size_t uSize, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource, CContext* pContext);
	MapBufferRangeFunc                    m_pfMapBufferRange;

	SBitMask<eBB_NUM, SUnsafeBitMaskWord> m_kBindings;
	GLenum                                m_eUsage;
	uint32                                m_uSize;
	uint32                                m_uElementSize; // 0 if not structured
	uint8*                                m_pSystemMemoryCopy;
	bool m_bMapped;
	size_t                                m_uMapOffset;
	size_t                                m_uMapSize;

	SShaderTextureBufferView*             m_pTextureBuffersHead;

#if DXGL_SUPPORT_APITRACE
	SDynamicBufferCopy m_kDynamicCopy;
	bool               m_bSystemMemoryCopyOwner;
#endif //DXGL_SUPPORT_APITRACE
#if DXGL_STREAMING_CONSTANT_BUFFERS
	struct SContextCache
	{
		SContextFramePtr m_spFrame;
		SBufferRange     m_kRange;
		uint32           m_uUnit;
		uint32           m_uStreamOffset;
	};
	DXGL_TODO("Evaluate if it is worth to have multiple possible bindings cached - eventually add another dimension and keep items sorted by slot for binary search");
	#if OGL_SINGLE_CONTEXT
	bool          m_bContextCacheValid;
	SContextCache m_kContextCache;
	#else
	SContextCache m_akContextCaches[MAX_NUM_CONTEXT_PER_DEVICE];
	SBitMask<MAX_NUM_CONTEXT_PER_DEVICE, SSpinlockBitMaskWord> m_kContextCachesValid;
	#endif
	bool m_bStreaming;
#endif //DXGL_STREAMING_CONSTANT_BUFFERS
};

DXGL_DECLARE_REF_COUNTED(struct, SQuery)
{
	virtual ~SQuery();

	virtual void Begin();
	virtual void End();
	virtual bool GetData(void* pData, uint32 uDataSize, bool bFlush) = 0;
	virtual uint32 GetDataSize();
};

struct SPlainQuery : SQuery
{
	SPlainQuery(GLenum eTarget);
	virtual ~SPlainQuery();

	virtual void Begin();
	virtual void End();

	GLuint m_uName;
	GLenum m_eTarget;
	bool   m_bBegun;
};

struct SOcclusionQuery : SPlainQuery
{
	SOcclusionQuery();

	virtual bool   GetData(void* pData, uint32 uDataSize, bool bFlush);
	virtual uint32 GetDataSize();
};

struct SFenceSync : SQuery
{
	SFenceSync();
	virtual ~SFenceSync();

	virtual void   End();
	virtual bool   GetData(void* pData, uint32 uDataSize, bool bFlush);
	virtual uint32 GetDataSize();

	GLsync m_pFence;
};

#if DXGL_SUPPORT_TIMER_QUERIES

struct STimestampDisjointQuery : SQuery
{
	virtual bool   GetData(void* pData, uint32 uDataSize, bool bFlush);
	virtual uint32 GetDataSize();
};

struct STimestampQuery : SQuery
{
	STimestampQuery();
	virtual ~STimestampQuery();

	virtual void   End();
	virtual bool   GetData(void* pData, uint32 uDataSize, bool bFlush);
	virtual uint32 GetDataSize();

	GLuint m_uName;
};

#endif //DXGL_SUPPORT_TIMER_QUERIES

struct SDefaultFrameBufferTexture : STexture
{
	SDefaultFrameBufferTexture(GLsizei iWidth, GLsizei iHeight, EGIFormat eFormat, GLbitfield eBufferMask);
	virtual ~SDefaultFrameBufferTexture();

	virtual SShaderTextureViewPtr       CreateShaderResourceView(const SShaderTextureViewConfiguration& kConfiguration, CContext* pContext);
	virtual SShaderImageViewPtr         CreateUnorderedAccessView(const SShaderImageViewConfiguration& kConfiguration, CContext* pContext);
	virtual SOutputMergerTextureViewPtr CreateOutputMergerView(const SOutputMergerTextureViewConfiguration& kConfiguration, CContext* pContext);

	virtual void                        OnCopyRead(CContext* pContext);
	virtual void                        OnCopyWrite(CContext* pContext);

	void                                IncTextureRefCount(CContext* pContext);
	void                                DecTextureRefCount();
	void                                EnsureTextureExists(CContext* pContext);
	void                                ReleaseTexture();
	void                                OnWrite(CContext* pContext, bool bDefaultFrameBuffer);
	void                                OnRead(CContext* pContext);
	void                                Flush(CContext* pContext);
#if DXGL_FULL_EMULATION
	void                                SetCustomWindowContext(const TWindowContext& kCustomWindowContext);
#endif //DXGL_FULL_EMULATION

	static void   UpdateSubresource(SResource* pResource, uint32 uSubresource, const D3D11_BOX* pDstBox, const void* pSrcData, uint32 uSrcRowPitch, uint32 uSrcDepthPitch, CContext* pContext);
	static bool   MapSubresource(SResource* pResource, uint32 uSubresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource, CContext* pContext);
	static void   UnmapSubresource(SResource* pResource, uint32 uSubresource, CContext* pContext);
#if DXGL_USE_PBO_FOR_STAGING_TEXTURES || !DXGL_SUPPORT_COPY_IMAGE
	static void   UnpackData(STexture* pTexture, STexSubresourceID kSubID, STexPos kOffset, STexSize kSize, const SMappedSubTexture& kDataLocation, CContext* pContext);
	static void   PackData(STexture* pTexture, STexSubresourceID kSubID, STexPos kOffset, STexSize kSize, const SMappedSubTexture& kDataLocation, CContext* pContext);
	static uint32 LocatePackedData(STexture* pTexture, STexSubresourceID kSubID, STexPos kOffset, SMappedSubTexture& kDataLocation);
#endif //DXGL_USE_PBO_FOR_STAGING_TEXTURES || !DXGL_SUPPORT_COPY_IMAGE

	uint32             m_uTextureRefCount;
	SFrameBufferObject m_kInputFBO;
	SFrameBufferObject m_kDefaultFBO;
	GLbitfield         m_uBufferMask;
	GLenum             m_eInputColorBuffer, m_eOutputColorBuffer;
	GLsizei            m_iDefaultFBOWidth, m_iDefaultFBOHeight;
	bool               m_bInputDirty, m_bOutputDirty;
#if DXGL_FULL_EMULATION
	TWindowContext     m_kCustomWindowContext;
#endif //DXGL_FULL_EMULATION
};

#if OGL_SINGLE_CONTEXT

DXGL_DECLARE_REF_COUNTED(struct, SInitialDataCopy)
{
	D3D11_SUBRESOURCE_DATA* m_akSubresourceData;
	uint32 m_uNumSubresources;

	SInitialDataCopy(const D3D11_SUBRESOURCE_DATA * akSubresourceData, uint32 uNumSubresources);
	~SInitialDataCopy();
};

SInitialDataCopyPtr CreateTexture1DInitialDataCopy(const D3D11_TEXTURE1D_DESC& kDesc, const D3D11_SUBRESOURCE_DATA* pInitialData);
SInitialDataCopyPtr CreateTexture2DInitialDataCopy(const D3D11_TEXTURE2D_DESC& kDesc, const D3D11_SUBRESOURCE_DATA* pInitialData);
SInitialDataCopyPtr CreateTexture3DInitialDataCopy(const D3D11_TEXTURE3D_DESC& kDesc, const D3D11_SUBRESOURCE_DATA* pInitialData);
SInitialDataCopyPtr CreateBufferInitialDataCopy(const D3D11_BUFFER_DESC& kDesc, const D3D11_SUBRESOURCE_DATA* pInitialData);

#endif

STexturePtr                   CreateTexture1D(const D3D11_TEXTURE1D_DESC& kDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, CContext* pContext);
STexturePtr                   CreateTexture2D(const D3D11_TEXTURE2D_DESC& kDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, CContext* pContext);
STexturePtr                   CreateTexture3D(const D3D11_TEXTURE3D_DESC& kDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, CContext* pContext);
SBufferPtr                    CreateBuffer(const D3D11_BUFFER_DESC& kDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, CContext* pContext);
SQueryPtr                     CreateQuery(const D3D11_QUERY_DESC& kDesc, CContext* pContext);
SDefaultFrameBufferTexturePtr CreateBackBufferTexture(const D3D11_TEXTURE2D_DESC& kDesc);

CResourceName                 CreateTextureView(STexture* pTexture, const SGIFormatInfo* pFormatInfo, GLenum eTarget, GLuint uMinMipLevel, GLuint uNumMipLevels, GLuint uMinLayer, GLuint uNumLayers, CContext* pContext);

GLenum                        GetBufferBindingTarget(EBufferBinding eBinding);
GLenum                        GetBufferBindingPoint(EBufferBinding eBinding);

void                          CopyTexture(STexture* pDstTexture, STexture* pSrcTexture, CContext* pContext);
void                          CopyBuffer(SBuffer* pDstBuffer, SBuffer* pSrcBuffer, CContext* pContext);
void                          CopySubTexture(STexture* pDstTexture, uint32 uDstSubresource, uint32 uDstX, uint32 uDstY, uint32 uDstZ, STexture* pSrcTexture, uint32 uSrcSubresource, const D3D11_BOX* pSrcBox, CContext* pContext);
void                          CopySubBuffer(SBuffer* pDstBuffer, uint32 uDstSubresource, uint32 uDstX, uint32 uDstY, uint32 uDstZ, SBuffer* pSrcBuffer, uint32 uSrcSubresource, const D3D11_BOX* pSrcBox, CContext* pContext);

}

#endif //__GLRESOURCE__

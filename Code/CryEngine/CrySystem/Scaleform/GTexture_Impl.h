// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _GTEXTURE_XRENDER_H_
#define _GTEXTURE_XRENDER_H_

#pragma once

#ifdef INCLUDE_SCALEFORM_SDK

#include <CryCore/Platform/platform.h>
#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IShader.h>

#pragma warning(push)
#pragma warning(disable : 6326)   // Potential comparison of a constant with another constant
#include <CryCore/Platform/CryWindows.h>
#include <GRenderer.h>            // includes <windows.h>
#pragma warning(pop)

#include <CryCore/Project/ProjectDefines.h>
#include "GImageInfo.h"

class GRendererXRender;
class GImageBase;
class ITexture;

class GTextureXRenderBase:public GTexture
{
	// GTexture interface
public:
	virtual GRenderer* GetRenderer() const;

	virtual Handle GetUserData() const       { return m_userData; }
	virtual void   SetUserData(Handle hData) { m_userData = hData; }

	virtual void AddChangeHandler(ChangeHandler* pHandler)    {}
	virtual void RemoveChangeHandler(ChangeHandler* pHandler) {}

	// GTextureXRenderBase interface
public:
	virtual bool InitTextureFromFile(const char* pFilename) = 0;
	virtual bool InitTextureFromTexId(int texid)            = 0;

public:
	GTextureXRenderBase(GRenderer* pRenderer, int32 texID = -1, ETEX_Format fmt = eTF_Unknown, bool bIsTemp = false);
	virtual ~GTextureXRenderBase();

	int32 GetID() const { return m_texID; } // GetID() non-virtual / m_texID member of base for performance reasons to allow inlining of GetID() calls
	int32* GetIDPtr() { return &m_texID; }

	ETEX_Format GetFmt() const { return m_fmt; }
	bool        IsYUV() const  { return m_fmt == eTF_YUV || m_fmt == eTF_YUVA; }

public:
#if defined(ENABLE_FLASH_INFO)
	static size_t GetTextureMemoryUsed()  { return (size_t) ms_textureMemoryUsed; }
	static uint32 GetNumTextures()        { return (uint32) ms_numTexturesCreated; }
	static uint32 GetNumTexturesTotal()   { return (uint32) ms_numTexturesCreatedTotal; }
	static uint32 GetNumTexturesTempRT()  { return (uint32) ms_numTexturesCreatedTemp; }
	static int32  GetFontCacheTextureID() { return ms_fontCacheTextureID; }
#endif

protected:
#if defined(ENABLE_FLASH_INFO)
	static volatile int32 ms_textureMemoryUsed;
	static volatile int32 ms_numTexturesCreated;
	static volatile int32 ms_numTexturesCreatedTotal;
	static volatile int32 ms_numTexturesCreatedTemp;
	static volatile int32 ms_fontCacheTextureID;
#endif

protected:
	int32 m_texID;
	ETEX_Format m_fmt;
	Handle m_userData;
	GRenderer* m_pRenderer;
#if defined(ENABLE_FLASH_INFO)
	bool m_isTemp;
#endif
};

class GTextureXRender:public GTextureXRenderBase
{
	// GTexture interface
public:
	virtual bool IsDataValid() const { return m_texID > 0; }

	virtual bool InitTexture(GImageBase* pIm, UInt usage = Usage_Wrap);
	virtual bool InitDynamicTexture(int width, int height, GImage::ImageFormat format, int mipmaps, UInt usage);

	virtual void Update(int level, int n, const UpdateRect* pRects, const GImageBase* pIm);

	virtual int  Map(int level, int n, MapRect* maps, int flags = 0);
	virtual bool Unmap(int level, int n, MapRect* maps, int flags = 0);

	// GTextureXRenderBase interface
public:
	virtual bool InitTextureFromFile(const char* pFilename);
	virtual bool InitTextureFromTexId(int texid);

public:
	GTextureXRender(GRenderer* pRenderer, int texID = -1, ETEX_Format fmt = eTF_Unknown);
	virtual ~GTextureXRender();

private:
	bool InitTextureInternal(const GImageBase* pIm);
	bool InitTextureInternal(ETEX_Format texFmt, int32 width, int32 height, int32 pitch, uint8* pData);
	void SwapRB(uint8* pImageData, uint32 width, uint32 height, uint32 pitch) const;
#if defined(NEED_ENDIAN_SWAP)
	void SwapEndian(uint8* pImageData, uint32 width, uint32 height, uint32 pitch) const;
#endif
};

class GTextureXRenderTempRT:public GTextureXRender
{
	// GTexture interface overrides
public:
	virtual Handle GetUserData() const       { return (Handle)this; }
	virtual void   SetUserData(Handle hData) { assert(false); }

public:
	GTextureXRenderTempRT(GRenderer* pRenderer, int texID = -1, ETEX_Format fmt = eTF_Unknown);
	virtual ~GTextureXRenderTempRT();
};

class GTextureXRenderTempRTLockless:public GTextureXRenderBase
{
	// GTexture interface
public:
	virtual Handle GetUserData() const       { return (Handle)m_pRT; }
	virtual void   SetUserData(Handle hData) { assert(false); }

	virtual bool IsDataValid() const { return true; }

	virtual bool InitTexture(GImageBase* pIm, UInt usage = Usage_Wrap)                                          { assert(false); return false; }
	virtual bool InitDynamicTexture(int width, int height, GImage::ImageFormat format, int mipmaps, UInt usage) { assert(false); return false; }

	virtual void Update(int level, int n, const UpdateRect* pRects, const GImageBase* pIm) { assert(false); }

	virtual int  Map(int level, int n, MapRect* maps, int flags = 0)   { assert(false); return -1; }
	virtual bool Unmap(int level, int n, MapRect* maps, int flags = 0) { assert(false); return false; }

	// GTextureXRenderBase interface
public:
	virtual bool InitTextureFromFile(const char* pFilename) { assert(false); return false; }
	virtual bool InitTextureFromTexId(int texid)            { assert(false); return false; }

public:
	GTextureXRenderTempRTLockless(GRenderer* pRenderer);
	virtual ~GTextureXRenderTempRTLockless();

	void SetRT(GTexture* pRT) {	m_pRT = pRT; }
	GTexture* GetRT() { return m_pRT; }

private:
	GPtr<GTexture> m_pRT;
};

class GTextureXRenderYUV:public GTextureXRenderBase
{
	// GTexture interface
public:
	virtual bool IsDataValid() const { return m_numIDs > 0; }

	virtual bool InitTexture(GImageBase* pIm, UInt usage = Usage_Wrap);
	virtual bool InitDynamicTexture(int width, int height, GImage::ImageFormat format, int mipmaps, UInt usage);

	virtual void Update(int level, int n, const UpdateRect* pRects, const GImageBase* pIm);

	virtual int  Map(int level, int n, MapRect* maps, int flags = 0);
	virtual bool Unmap(int level, int n, MapRect* maps, int flags = 0);

	// GTextureXRenderBase interface
public:
	virtual bool InitTextureFromFile(const char* pFilename);
	virtual bool InitTextureFromTexId(int texid);

public:
	GTextureXRenderYUV(GRenderer* pRenderer);
	virtual ~GTextureXRenderYUV();

	const int32* GetIDs() const    { return m_texIDs; }
	int32        GetNumIDs() const { return m_numIDs; }

	uint32 Res(uint32 i, uint32 val) const { return i == 0 || i == 3 ? val : val >> 1; }

	bool IsStereoContent() const       { return m_isStereoContent; }
	void SetStereoContent(bool enable) { m_isStereoContent = enable; }

private:
	void Clear();

private:
	int32  m_texIDs[4];
	int32  m_numIDs;
	uint32 m_width;
	uint32 m_height;
	bool m_isStereoContent;
};

#endif // #ifdef INCLUDE_SCALEFORM_SDK

#endif // #ifndef _GTEXTURE_XRENDER_H_
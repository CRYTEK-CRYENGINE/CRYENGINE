// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef CRYFONT_ICRYFONT_H
#define CRYFONT_ICRYFONT_H

#pragma once

#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Color.h>
#include <CryString/CryString.h>
#include <CrySystem/IEngineModule.h>

struct ISystem;
class ICrySizer;

struct ICryFont;
struct IFFont;

struct IRenderer;
struct IRenderAuxGeom;

extern "C"
#ifdef CRYFONT_EXPORTS
DLL_EXPORT
#else
DLL_IMPORT
#endif
ICryFont * CreateCryFontInterface(ISystem * pSystem);

typedef ICryFont*(* PFNCREATECRYFONTINTERFACE)(ISystem* pSystem);

struct IFontEngineModule : public Cry::IDefaultModule
{
	CRYINTERFACE_DECLARE_GUID(IFontEngineModule, "cddd1b3c-a105-4612-ada4-ea7e6f919df1"_cry_guid);
};

//////////////////////////////////////////////////////////////////////////////////////////////
//! Main interface to the engine's font rendering implementation, allowing retrieval of fonts for run-time rendering
struct ICryFont
{
	// <interfuscator:shuffle>
	virtual ~ICryFont(){}
	virtual void Release() = 0;

	//! Creates a named font (case sensitive)
	virtual IFFont* NewFont(const char* pFontName) = 0;

	//! Gets a named font (case sensitive)
	virtual IFFont* GetFont(const char* pFontName) const = 0;

	//! Globally sets common font render properties based on the initialized renderer
	virtual void SetRendererProperties(IRenderer* pRenderer) = 0;

	//! Puts the objects used in this module into the sizer interface
	virtual void GetMemoryUsage(ICrySizer* pSizer) const = 0;

	//! All font names separated by ,
	//! Example: "console,default,hud"
	virtual string GetLoadedFontNames() const = 0;
	// </interfuscator:shuffle>
};

//////////////////////////////////////////////////////////////////////////////////////////////
#define TTFFLAG_SMOOTH_NONE         0x00000000    // No smooth.
#define TTFFLAG_SMOOTH_BLUR         0x00000001    // Smooth by blurring it.
#define TTFFLAG_SMOOTH_SUPERSAMPLE  0x00000002    // Smooth by rendering the characters into a bigger texture, and then resize it to the normal size using bilinear filtering.

#define TTFFLAG_SMOOTH_MASK         0x0000000f    // Mask for retrieving.
#define TTFFLAG_SMOOTH_SHIFT        0             // Shift amount for retrieving.

#define TTFLAG_SMOOTH_AMOUNT_2X     0x00010000    // Blur / supersample [2x]
#define TTFLAG_SMOOTH_AMOUNT_4X     0x00020000    // Blur / supersample [4x]

#define TTFFLAG_SMOOTH_AMOUNT_MASK  0x000f0000    // Mask for retrieving.
#define TTFFLAG_SMOOTH_AMOUNT_SHIFT 16            // Shift amount for retrieving.

#define TTFFLAG_CREATE(smooth, amount)  ((((smooth) << TTFFLAG_SMOOTH_SHIFT) & TTFFLAG_SMOOTH_MASK) | (((amount) << TTFFLAG_SMOOTH_AMOUNT_SHIFT) & TTFFLAG_SMOOTH_AMOUNT_MASK))
#define TTFFLAG_GET_SMOOTH(flag)        (((flag) & TTFLAG_SMOOTH_MASK) >> TTFFLAG_SMOOTH_SHIFT)
#define TTFFLAG_GET_SMOOTH_AMOUNT(flag) (((flag) & TTFLAG_SMOOTH_SMOUNT_MASK) >> TTFFLAG_SMOOTH_AMOUNT_SHIFT)

#define FONTRF_HCENTERED 0x80000000               // The font will be centered horizontally around the x coo
#define FONTRF_VCENTERED 0x40000000               // The font will be centered vertically around the y coo
#define FONTRF_FILTERED  0x20000000               // The font will be drawn with bilinear filtering

//////////////////////////////////////////////////////////////////////////
struct STextDrawContext
{
	unsigned int m_fxIdx;

	Vec2         m_size;
	float        m_widthScale;

	float        m_clipX;
	float        m_clipY;
	float        m_clipWidth;
	float        m_clipHeight;

	int          m_drawTextFlags;

	bool         m_proportional;
	bool         m_sizeIn800x600;
	bool         m_clippingEnabled;
	bool         m_framed;

	ColorB       m_colorOverride;

	STextDrawContext()
		: m_fxIdx(0)
		, m_size(16.0f, 16.0f)
		, m_widthScale(1.0f)
		, m_clipX(0)
		, m_clipY(0)
		, m_clipWidth(0)
		, m_clipHeight(0)
		, m_proportional(true)
		, m_sizeIn800x600(true)
		, m_clippingEnabled(false)
		, m_framed(false)
		, m_colorOverride(0, 0, 0, 0)
		, m_drawTextFlags(0)
	{
	}

	void  Reset()                                                      { *this = STextDrawContext(); }
	void  SetEffect(unsigned int fxIdx)                                { m_fxIdx = fxIdx; }
	void  SetSize(const Vec2& size)                                    { m_size = size; }
	void  SetCharWidthScale(float widthScale)                          { m_widthScale = widthScale; }
	void  SetClippingRect(float x, float y, float width, float height) { m_clipX = x; m_clipY = y; m_clipWidth = width; m_clipHeight = height; }
	void  SetProportional(bool proportional)                           { m_proportional = proportional; }
	void  SetSizeIn800x600(bool sizeIn800x600)                         { m_sizeIn800x600 = sizeIn800x600; }
	void  EnableClipping(bool enable)                                  { m_clippingEnabled = enable; }
	void  EnableFrame(bool enable)                                     { m_framed = enable; }
	void  SetColor(const ColorF& col)                                  { m_colorOverride = col; }
	void  SetFlags(int flags)                                          { m_drawTextFlags = flags; }

	float GetCharWidth() const                                         { return m_size.x; }
	float GetCharHeight() const                                        { return m_size.y; }
	float GetCharWidthScale() const                                    { return m_widthScale; }
	int   GetFlags() const                                             { return m_drawTextFlags; }

	bool  IsColorOverridden() const                                    { return m_colorOverride.a != 0; }
};

//////////////////////////////////////////////////////////////////////////////////////////////
//! Main interface for a type of font in the engine, allowing drawing onto the 2D viewport and textures
struct IFFont
{
	// <interfuscator:shuffle>
	virtual ~IFFont(){}

	//! Releases the font object.
	virtual void Release() = 0;

	//! Loads a font from a TTF file.
	virtual bool Load(const char* pFontFilePath, unsigned int width, unsigned int height, unsigned int flags) = 0;

	//! Loads a font from a XML file.
	virtual bool Load(const char* pXMLFile) = 0;

	//! Frees internally memory internally allocated by Load().
	virtual void Free() = 0;

	//! Draws a formatted string (UTF-8).
	virtual void DrawString(float x, float y, const char* pStr, const bool asciiMultiLine, const STextDrawContext& ctx) = 0;

	//! Draws a formatted string (UTF-8), taking z into account.
	virtual void DrawString(float x, float y, float z, const char* pStr, const bool asciiMultiLine, const STextDrawContext& ctx) = 0;

	//! Computes the text size (UTF-8).
	virtual Vec2 GetTextSize(const char* pStr, const bool asciiMultiLine, const STextDrawContext& ctx) = 0;

	//! Computes virtual text-length (UTF-8) (because of special chars...).
	virtual size_t GetTextLength(const char* pStr, const bool asciiMultiLine) const = 0;

	//! Wraps text based on specified maximum line width (UTF-8)
	virtual void WrapText(string& result, float maxWidth, const char* pStr, const STextDrawContext& ctx) = 0;

	//! Puts the memory used by this font into the given sizer.
	virtual void GetMemoryUsage(ICrySizer* pSizer) const = 0;

	//! useful for special feature rendering interleaved with fonts (e.g. box behind the text)
	virtual void         GetGradientTextureCoord(float& minU, float& minV, float& maxU, float& maxV) const = 0;

	virtual unsigned int GetEffectId(const char* pEffectName) const = 0;

	//! Only to be used by renderer, from the render thread.
	virtual void RenderCallback(float x, float y, float z, const char* pStr, const bool asciiMultiLine, const STextDrawContext& ctx,IRenderAuxGeom* pAux) = 0;
	// </interfuscator:shuffle>
};

#endif // CRYFONT_ICRYFONT_H

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef CRYFONT_CRYFONT_H
#define CRYFONT_CRYFONT_H

#if !defined(USE_NULLFONT_ALWAYS)

	#include <map>

class CFFont;

class CCryFont : public ICryFont
{
	friend class CFFont;

public:
	CCryFont(ISystem* pSystem);
	virtual ~CCryFont();

	virtual void    Release();
	virtual IFFont* NewFont(const char* pFontName);
	virtual IFFont* GetFont(const char* pFontName) const;
	virtual void    SetRendererProperties(IRenderer* pRenderer);
	virtual void    GetMemoryUsage(ICrySizer* pSizer) const;
	virtual string  GetLoadedFontNames() const;

public:
	void  UnregisterFont(const char* pFontName);
	bool  RndPropIsRGBA() const          { return m_rndPropIsRGBA; }
	float RndPropHalfTexelOffset() const { return m_rndPropHalfTexelOffset; }

private:
	typedef std::map<string, CFFont*> FontMap;
	typedef FontMap::iterator         FontMapItor;
	typedef FontMap::const_iterator   FontMapConstItor;

private:
	FontMap  m_fonts;
	ISystem* m_pSystem;
	bool     m_rndPropIsRGBA;
	float    m_rndPropHalfTexelOffset;
};

#endif

#endif // CRYFONT_CRYFONT_H

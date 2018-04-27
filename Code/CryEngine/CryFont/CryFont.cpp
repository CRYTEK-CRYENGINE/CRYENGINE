// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if !defined(USE_NULLFONT_ALWAYS)

	#include "CryFont.h"
	#include "FFont.h"
	#include "FontTexture.h"
	#include "FontRenderer.h"

	#if !defined(_RELEASE)
static void DumpFontTexture(IConsoleCmdArgs* pArgs)
{
	if (pArgs->GetArgCount() != 2)
		return;

	const char* pFontName = pArgs->GetArg(1);

	if (pFontName && *pFontName && *pFontName != '0')
	{
		string fontFile(pFontName);
		fontFile += ".bmp";

		CFFont* pFont = (CFFont*) gEnv->pCryFont->GetFont(pFontName);
		if (pFont)
		{
			pFont->GetFontTexture()->WriteToFile(fontFile.c_str());
			gEnv->pLog->LogWithType(IMiniLog::eInputResponse, "Dumped \"%s\" texture to \"%s\"!", pFontName, fontFile.c_str());
		}
	}
}

static void DumpFontNames(IConsoleCmdArgs* pArgs)
{
	string names = gEnv->pCryFont->GetLoadedFontNames();
	gEnv->pLog->LogWithType(IMiniLog::eInputResponse, "Currently loaded fonts: %s", names.c_str());
}
	#endif

CCryFont::CCryFont(ISystem* pSystem)
	: m_pSystem(pSystem)
	, m_fonts()
	, m_rndPropIsRGBA(false)
	, m_rndPropHalfTexelOffset(0.5f)
{
	assert(m_pSystem);

	CryLogAlways("Using FreeType %d.%d.%d", FREETYPE_MAJOR, FREETYPE_MINOR, FREETYPE_PATCH);

	#if !defined(_RELEASE)
	REGISTER_COMMAND("r_DumpFontTexture", DumpFontTexture, 0,
	                 "Dumps the specified font's texture to a bitmap file\n"
	                 "Use r_DumpFontTexture to get the loaded font names\n"
	                 "Usage: r_DumpFontTexture <fontname>");
	REGISTER_COMMAND("r_DumpFontNames", DumpFontNames, 0,
	                 "Logs a list of fonts currently loaded");
	#endif
}

CCryFont::~CCryFont()
{
	for (FontMapItor it = m_fonts.begin(), itEnd = m_fonts.end(); it != itEnd; )
	{
		CFFont* pFont = it->second;
		++it; // iterate as Release() below will remove font from the map
		SAFE_RELEASE(pFont);
	}
}

void CCryFont::Release()
{
	delete this;
}

IFFont* CCryFont::NewFont(const char* pFontName)
{
	string name = pFontName;
	name.MakeLower();

	FontMapItor it = m_fonts.find(CONST_TEMP_STRING(name.c_str()));
	if (it != m_fonts.end())
		return it->second;

	CFFont* pFont = new CFFont(m_pSystem, this, name.c_str());
	m_fonts.insert(FontMapItor::value_type(name, pFont));
	return pFont;
}

IFFont* CCryFont::GetFont(const char* pFontName) const
{
	FontMapConstItor it = m_fonts.find(CONST_TEMP_STRING(pFontName));
	return it != m_fonts.end() ? it->second : 0;
}

void CCryFont::SetRendererProperties(IRenderer* pRenderer)
{
	if (pRenderer)
	{
		m_rndPropIsRGBA = (pRenderer->GetFeatures() & RFT_RGBA) != 0;
		m_rndPropHalfTexelOffset = 0.0f;
	}
}

void CCryFont::GetMemoryUsage(ICrySizer* pSizer) const
{
	if (!pSizer->Add(*this))
		return;

	pSizer->AddObject(m_fonts);

	#ifndef _LIB
	{
		SIZER_COMPONENT_NAME(pSizer, "STL Allocator Waste");
		CryModuleMemoryInfo meminfo;
		ZeroStruct(meminfo);
		CryGetMemoryInfoForModule(&meminfo);
		pSizer->AddObject((this + 2), (size_t)meminfo.STL_wasted);
	}
	#endif
}

string CCryFont::GetLoadedFontNames() const
{
	string ret;
	for (FontMapConstItor it = m_fonts.begin(), itEnd = m_fonts.end(); it != itEnd; ++it)
	{
		CFFont* pFont = it->second;
		if (pFont)
		{
			if (!ret.empty())
				ret += ",";
			ret += pFont->GetName();
		}
	}
	return ret;
}

void CCryFont::UnregisterFont(const char* pFontName)
{
	FontMapItor it = m_fonts.find(CONST_TEMP_STRING(pFontName));
	if (it != m_fonts.end())
		m_fonts.erase(it);
}

#endif

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if !defined(USE_NULLFONT_ALWAYS)

	#include "FFont.h"
	#include "FontTexture.h"
	#include <CryMath/Cry_Math.h>

	#if CRY_PLATFORM_WINDOWS
		#include <CryCore/Platform/CryWindows.h>
		#include <shlobj.h> // requires <windows.h>
		#include <CryString/StringUtils.h>
	#endif

//////////////////////////////////////////////////////////////////////////
// Xml parser implementation

enum
{
	ELEMENT_UNKNOWN        = 0,
	ELEMENT_FONT           = 1,
	ELEMENT_EFFECT         = 2,
	ELEMENT_PASS           = 4,
	ELEMENT_PASS_COLOR     = 5,
	ELEMENT_PASS_POSOFFSET = 12,
	ELEMENT_PASS_BLEND     = 14
};

static inline int GetBlendModeFromString(const string& str, bool dst)
{
	int blend = GS_BLSRC_ONE;

	if (str == "zero")
	{
		blend = dst ? GS_BLDST_ZERO : GS_BLSRC_ZERO;
	}
	else if (str == "one")
	{
		blend = dst ? GS_BLDST_ONE : GS_BLSRC_ONE;
	}
	else if (str == "srcalpha" ||
	         str == "src_alpha")
	{
		blend = dst ? GS_BLDST_SRCALPHA : GS_BLSRC_SRCALPHA;
	}
	else if (str == "invsrcalpha" ||
	         str == "inv_src_alpha")
	{
		blend = dst ? GS_BLDST_ONEMINUSSRCALPHA : GS_BLSRC_ONEMINUSSRCALPHA;
	}
	else if (str == "dstalpha" ||
	         str == "dst_alpha")
	{
		blend = dst ? GS_BLDST_DSTALPHA : GS_BLSRC_DSTALPHA;
	}
	else if (str == "invdstalpha" ||
	         str == "inv_dst_alpha")
	{
		blend = dst ? GS_BLDST_ONEMINUSDSTALPHA : GS_BLSRC_ONEMINUSDSTALPHA;
	}
	else if (str == "dstcolor" ||
	         str == "dst_color")
	{
		blend = GS_BLSRC_DSTCOL;
	}
	else if (str == "srccolor" ||
	         str == "src_color")
	{
		blend = GS_BLDST_SRCCOL;
	}
	else if (str == "invdstcolor" ||
	         str == "inv_dst_color")
	{
		blend = GS_BLSRC_ONEMINUSDSTCOL;
	}
	else if (str == "invsrccolor" ||
	         str == "inv_src_color")
	{
		blend = GS_BLDST_ONEMINUSSRCCOL;
	}

	return blend;
}

class CXmlFontShader
{
public:
	CXmlFontShader(CFFont* pFont)
	{
		m_pFont = pFont;
		m_nElement = ELEMENT_UNKNOWN;
		m_pEffect = NULL;
		m_pPass = NULL;
		m_FontTexSize.set(0, 0);
		m_bNoRescale = false;
		m_FontSmoothAmount = 0;
		m_FontSmoothMethod = FONT_SMOOTH_NONE;
	}

	~CXmlFontShader()
	{
	}

	void ScanXmlNodesRecursively(XmlNodeRef node)
	{
		if (!node)
		{
			return;
		}

		FoundElement(node->getTag());

		for (int i = 0, count = node->getNumAttributes(); i < count; ++i)
		{
			const char* key = "";
			const char* value = "";
			if (node->getAttributeByIndex(i, &key, &value))
			{
				FoundAttribute(key, value);
			}
		}

		for (int i = 0, count = node->getChildCount(); i < count; ++i)
		{
			XmlNodeRef child = node->getChild(i);
			ScanXmlNodesRecursively(child);
		}
	}

private:
	// notify methods
	void FoundElement(const string& name)
	{
		//MessageBox(NULL, string("[" + name + "]").c_str(), "FoundElement", MB_OK);
		// process the previous element
		switch (m_nElement)
		{
		case ELEMENT_FONT:
			{
				if (!m_FontTexSize.x || !m_FontTexSize.y)
				{
					m_FontTexSize.set(512, 512);
				}

				bool fontLoaded = m_pFont->Load(m_strFontPath.c_str(), m_FontTexSize.x, m_FontTexSize.y, TTFFLAG_CREATE(m_FontSmoothMethod, m_FontSmoothAmount));
	#if CRY_PLATFORM_WINDOWS
				if (!fontLoaded)
				{
					TCHAR sysFontPath[MAX_PATH];
					if (SUCCEEDED(SHGetFolderPath(0, CSIDL_FONTS, 0, SHGFP_TYPE_DEFAULT, sysFontPath)))
					{
						const char* pFontPath = m_strFontPath.c_str();
						const char* pFontName = PathUtil::GetFile(pFontPath);

						string newFontPath(sysFontPath);
						newFontPath += "/";
						newFontPath += pFontName;
						m_pFont->Load(newFontPath, m_FontTexSize.x, m_FontTexSize.y, TTFFLAG_CREATE(m_FontSmoothMethod, m_FontSmoothAmount));
					}
				}
	#endif
			}
			break;

		default:
			break;
		}

		// Translate the m_nElement name to a define
		if (name == "font")
		{
			m_nElement = ELEMENT_FONT;
		}
		else if (name == "effect")
		{
			m_nElement = ELEMENT_EFFECT;
		}
		else if (name == "pass")
		{
			m_pPass = NULL;
			m_nElement = ELEMENT_PASS;
			if (m_pEffect)
				m_pPass = m_pEffect->AddPass();
		}
		else if (name == "color")
		{
			m_nElement = ELEMENT_PASS_COLOR;
		}
		else if (name == "pos" ||
		         name == "offset")
		{
			m_nElement = ELEMENT_PASS_POSOFFSET;
		}
		else if (name == "blend" ||
		         name == "blending")
		{
			m_nElement = ELEMENT_PASS_BLEND;
		}
		else
		{
			m_nElement = ELEMENT_UNKNOWN;
		}
	}

	void FoundAttribute(const string& name, const string& value)
	{
		//MessageBox(NULL, string(name + "\n" + value).c_str(), "FoundAttribute", MB_OK);
		switch (m_nElement)
		{
		case ELEMENT_FONT:
			if (name == "path")
			{
				m_strFontPath = value;
			}
			else if (name == "w")
			{
				m_FontTexSize.x = (int)atof(value.c_str());
			}
			else if (name == "h")
			{
				m_FontTexSize.y = (int)atof(value.c_str());
			}
			else if (name == "norescale")
			{
				if (value.empty() || (atoi(value.c_str()) != 0))
				{
					m_bNoRescale = true;
				}
			}
			else if (name == "smooth")
			{
				if (value == "blur")
				{
					m_FontSmoothMethod = FONT_SMOOTH_BLUR;
				}
				else if (value == "supersample")
				{
					m_FontSmoothMethod = FONT_SMOOTH_SUPERSAMPLE;
				}
				else if (value == "none")
				{
					m_FontSmoothMethod = FONT_SMOOTH_NONE;
				}
			}
			else if (name == "smooth_amount")
			{
				m_FontSmoothAmount = (long)atof(value.c_str());
			}
			break;

		case ELEMENT_EFFECT:
			if (name == "name")
			{
				if (value == "default")
				{
					m_pEffect = m_pFont->GetDefaultEffect();
					m_pEffect->ClearPasses();
				}
				else
				{
					m_pEffect = m_pFont->AddEffect(value.c_str());
				}
			}
			break;

		case ELEMENT_PASS_COLOR:
			if (!m_pPass) break;
			if (name == "r")       { m_pPass->m_color.r = (uint8)((float)atof(value.c_str()) * 255.0f); }
			else if (name == "g")  { m_pPass->m_color.g = (uint8)((float)atof(value.c_str()) * 255.0f); }
			else if (name == "b")  { m_pPass->m_color.b = (uint8)((float)atof(value.c_str()) * 255.0f); }
			else if (name == "a")  { m_pPass->m_color.a = (uint8)((float)atof(value.c_str()) * 255.0f); }
			break;

		case ELEMENT_PASS_POSOFFSET:
			if (!m_pPass) break;
			if (name == "x")
			{
				m_pPass->m_posOffset.x = (float)atoi(value.c_str());
			}
			else if (name == "y")
			{
				m_pPass->m_posOffset.y = (float)atoi(value.c_str());
			}
			break;

		case ELEMENT_PASS_BLEND:
			if (!m_pPass) break;
			if (name == "src")
			{
				m_pPass->m_blendSrc = GetBlendModeFromString(value, false);
			}
			else if (name == "dst")
			{
				m_pPass->m_blendDest = GetBlendModeFromString(value, true);
			}
			else if (name == "type")
			{
				if (value == "modulate")
				{
					m_pPass->m_blendSrc = GS_BLSRC_SRCALPHA;
					m_pPass->m_blendDest = GS_BLDST_ONEMINUSSRCALPHA;
				}
				else if (value == "additive")
				{
					m_pPass->m_blendSrc = GS_BLSRC_SRCALPHA;
					m_pPass->m_blendDest = GS_BLDST_ONE;
				}
			}
			break;

		default:
		case ELEMENT_UNKNOWN:
			break;
		}
	}

public:
	CFFont*                 m_pFont;

	unsigned long           m_nElement;

	CFFont::SEffect*        m_pEffect;
	CFFont::SRenderingPass* m_pPass;

	string                  m_strFontPath;
	Vec2i                   m_FontTexSize;
	bool                    m_bNoRescale;

	int                     m_FontSmoothMethod;
	int                     m_FontSmoothAmount;
};

//////////////////////////////////////////////////////////////////////////
// Main loading function
bool CFFont::Load(const char* pXMLFile)
{
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, 0, "Font %s", pXMLFile);

	m_curPath = "";
	if (pXMLFile)
	{
		const char* ch = strrchr(pXMLFile, '/');
		if (ch)
			m_curPath = string(pXMLFile, 0, ch - pXMLFile + 1);
	}

	XmlNodeRef root = GetISystem()->LoadXmlFromFile(pXMLFile);
	if (!root)
		return false;

	CXmlFontShader xmlfs(this);
	xmlfs.ScanXmlNodesRecursively(root);

	return true;
}

#endif

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if !defined(USE_NULLFONT_ALWAYS)

	#include "FFont.h"
	#include "CryFont.h"
	#include "FontTexture.h"
	#include <CryString/UnicodeIterator.h>

static ColorB ColorTable[10] =
{
	ColorB(0x00, 0x00, 0x00), // black
	ColorB(0xff, 0xff, 0xff), // white
	ColorB(0x00, 0x00, 0xff), // blue
	ColorB(0x00, 0xff, 0x00), // green
	ColorB(0xff, 0x00, 0x00), // red
	ColorB(0x00, 0xff, 0xff), // cyan
	ColorB(0xff, 0xff, 0x00), // yellow
	ColorB(0xff, 0x00, 0xff), // purple
	ColorB(0xff, 0x80, 0x00), // orange
	ColorB(0x8f, 0x8f, 0x8f), // grey
};

static const int TabCharCount = 4;
static const size_t MsgBufferSize = 1024;
static const size_t MaxDrawVBQuads = 1024;

CFFont::CFFont(ISystem* pSystem, CCryFont* pCryFont, const char* pFontName)
	: m_name(pFontName)
	, m_curPath("")
	, m_pFontTexture(0)
	, m_fontBufferSize(0)
	, m_pFontBuffer(0)
	, m_texID(-1)
	, m_pSystem(pSystem)
	, m_pCryFont(pCryFont)
	, m_fontTexDirty(false)
	, m_effects()
{
	assert(m_name.c_str());
	assert(m_pSystem);
	assert(m_pCryFont);

	// create default effect
	SEffect* pEffect = AddEffect("default");
	pEffect->AddPass();
}

CFFont::~CFFont()
{
	if (m_pCryFont)
	{
		m_pCryFont->UnregisterFont(m_name);
		m_pCryFont = 0;
	}

	Free();
}

void CFFont::Release()
{
	delete this;
}

// Load a font from a TTF file
bool CFFont::Load(const char* pFontFilePath, unsigned int width, unsigned int height, unsigned int flags)
{
	if (!pFontFilePath)
		return false;

	Free();

	ICryPak* pPak = gEnv->pCryPak;

	string fullFile;
	if (pPak->IsAbsPath(pFontFilePath))
		fullFile = pFontFilePath;
	else
		fullFile = m_curPath + pFontFilePath;

	int iSmoothMethod = (flags & TTFFLAG_SMOOTH_MASK) >> TTFFLAG_SMOOTH_SHIFT;
	int iSmoothAmount = (flags & TTFFLAG_SMOOTH_AMOUNT_MASK) >> TTFFLAG_SMOOTH_AMOUNT_SHIFT;

	FILE* f = pPak->FOpen(fullFile.c_str(), "rb");
	if (!f)
		return false;

	size_t fileSize = pPak->FGetSize(f);
	if (!fileSize)
	{
		pPak->FClose(f);
		return false;
	}

	unsigned char* pBuffer = new unsigned char[fileSize];
	if (!pPak->FReadRaw(pBuffer, fileSize, 1, f))
	{
		pPak->FClose(f);
		delete[] pBuffer;
		return false;
	}

	pPak->FClose(f);

	if (!m_pFontTexture)
		m_pFontTexture = new CFontTexture();

	if (!m_pFontTexture || !m_pFontTexture->CreateFromMemory(pBuffer, (int) fileSize, width, height, iSmoothMethod, iSmoothAmount))
	{
		delete[] pBuffer;
		return false;
	}

	m_pFontBuffer = pBuffer;
	m_fontBufferSize = fileSize;

	m_fontTexDirty = false;

	InitCache();

	return true;
}

void CFFont::Free()
{
	IRenderer* pRenderer = gEnv->pRenderer;
	if (m_texID >= 0 && pRenderer)
	{
		pRenderer->RemoveTexture(m_texID);
		m_texID = -1;
	}

	delete m_pFontTexture;
	m_pFontTexture = 0;

	delete[] m_pFontBuffer;
	m_pFontBuffer = 0;
}

void CFFont::DrawString(float x, float y, const char* pStr, const bool asciiMultiLine, const STextDrawContext& ctx)
{
	CFFont::DrawString(x, y, 1.0f, pStr, asciiMultiLine, ctx);
}

#include <CryRenderer/IRenderAuxGeom.h>

void CFFont::DrawString(float x, float y, float z, const char* pStr, const bool asciiMultiLine, const STextDrawContext& ctx)
{
	IF(!pStr, 0) return;
	RenderCallback(x, y, 1.0f, pStr, asciiMultiLine, ctx, IRenderAuxGeom::GetAux());
}

ILINE DWORD COLCONV(DWORD clr)
{
	return ((clr & 0xff00ff00) | ((clr & 0xff0000) >> 16) | ((clr & 0xff) << 16));
}

void CFFont::RenderCallback(float x, float y, float z, const char* pStr, const bool asciiMultiLine, const STextDrawContext& ctx,IRenderAuxGeom* pAux)
{
	const size_t fxSize = m_effects.size();

	IF (fxSize && m_texID == -1 && !InitTexture(), 0)
		return;

	Prepare(pStr, true);

	pAux->SetTexture(m_texID);

	const size_t fxIdx = ctx.m_fxIdx < fxSize ? ctx.m_fxIdx : 0;
	const SEffect& fx = m_effects[fxIdx];

	bool isRGB = m_pCryFont->RndPropIsRGBA();

	float halfTexelShift = m_pCryFont->RndPropHalfTexelOffset();

	bool passZeroColorOverridden = ctx.IsColorOverridden();

	uint32 alphaBlend = passZeroColorOverridden ? ctx.m_colorOverride.a : fx.m_passes[0].m_color.a;
	if (alphaBlend > 128)
		++alphaBlend; // 0..256 for proper blending

	IRenderer* pRenderer = gEnv->pRenderer;
	assert(pRenderer);


	Vec2 size = ctx.m_size; // in pixel
	if (ctx.m_sizeIn800x600)
		pRenderer->ScaleCoord(size.x, size.y);

	float rcpCellWidth;
	Vec2 scale;

	if (ctx.m_proportional)
	{
		// to have backward compatible behavior (can be refactored later)
		rcpCellWidth = (1.0f / (512.0f / 16.0f)) * size.x;
		scale = Vec2(rcpCellWidth * ctx.m_widthScale, size.y / 32.0f);
	}
	else
	{
		rcpCellWidth = size.x / 16.0f;
		scale = Vec2(rcpCellWidth * ctx.m_widthScale, size.y * ctx.m_widthScale / 16.0f);
	}

	Vec2 baseXY = Vec2(x, y); // in pixels
	if (ctx.m_sizeIn800x600)
		pRenderer->ScaleCoord(baseXY.x, baseXY.y);

	// Apply overscan border
	const int flags = ctx.GetFlags();
	if ((flags & eDrawText_2D) && !(flags & eDrawText_IgnoreOverscan))
	{
		Vec2 overscanBorder = Vec2(0.0f, 0.0f);
		pRenderer->EF_Query(EFQ_OverscanBorders, overscanBorder);

		const float screenWidth = (float)pRenderer->GetOverlayWidth();
		const float screenHeight = (float)pRenderer->GetOverlayHeight();
		Vec2 overscanBorderOffset = ZERO;
		if (!(flags & eDrawText_Center))
		{
			overscanBorderOffset.x = (overscanBorder.x * screenWidth);
		}
		if (!(flags & eDrawText_CenterV))
		{
			overscanBorderOffset.y = (overscanBorder.y * screenHeight);
		}
		if (flags & eDrawText_Right)
		{
			overscanBorderOffset.x = -overscanBorderOffset.x;
		}
		if (flags & eDrawText_Bottom)
		{
			overscanBorderOffset.y = -overscanBorderOffset.y;
		}
		baseXY += overscanBorderOffset;
	}

	// snap for pixel perfect rendering (better quality for text)
	{
		baseXY.x = floor(baseXY.x);
		baseXY.y = floor(baseXY.y);

		// for smaller fonts (half res or less) it's better to average multiple pixels (we don't miss lines)
		if (scale.x < 0.9f)
			baseXY.x += 0.5f; // try to average two columns (for exact half res)
		if (scale.y < 0.9f)
			baseXY.y += 0.25f; // hand tweaked value to get a good result with tiny font (640x480 underscore in console)
	}

	for (size_t j = 0, numPasses = fx.m_passes.size(); j < numPasses; ++j)
	{
		size_t i = numPasses - j - 1;

		const SRenderingPass* pPass = &fx.m_passes[i];

		if (!i)
			alphaBlend = 256;

		ColorB passColor = !i && passZeroColorOverridden ? ctx.m_colorOverride : fx.m_passes[i].m_color;

		// gather pass data
		Vec2 offset = pPass->m_posOffset; // in pixels

		float charX = baseXY.x + offset.x; // in pixels
		float charY = baseXY.y + offset.y; // in pixels

		ColorB color = passColor;

		bool drawFrame = ctx.m_framed && i == numPasses - 1;

		SVF_P3F_C4B_T2F* pVertex = pAux->BeginDrawBuffer(MaxDrawVBQuads * 6, m_texID != 0);
	
		//int vbLen = 0;
		size_t vbOffset = 0;

		if (drawFrame)
		{
			ColorB tempColor(255, 255, 255, 255);
			DWORD frameColor = tempColor.pack_abgr8888();
			if (!isRGB)
				frameColor = COLCONV(frameColor);

			Vec2 textSize = GetTextSizeUInternal(pStr, asciiMultiLine, ctx);

			float x0 = baseXY.x - 12;
			float y0 = baseXY.y - 6;
			float x1 = baseXY.x + textSize.x + 12;
			float y1 = baseXY.y + textSize.y + 6;

			bool culled = false;
			IF (ctx.m_clippingEnabled, 0)
			{
				float clipX = ctx.m_clipX;
				float clipY = ctx.m_clipY;
				float clipR = ctx.m_clipX + ctx.m_clipWidth;
				float clipB = ctx.m_clipY + ctx.m_clipHeight;

				if ((x0 >= clipR) || (y0 >= clipB) || (x1 < clipX) || (y1 < clipY))
					culled = true;

				x0 = max(clipX, x0);
				y0 = max(clipY, y0);
				x1 = min(clipR, x1);
				y1 = min(clipB, y1);

				//if (!culled && ((x1 - x0 <= 0.0f) || (y1 - y0 <= 0.0f)))
				//	culled = true;
			}

			IF (!culled, 1)
			{
				Vec3 v0(x0, y0, z);
				Vec3 v2(x1, y1, z);
				Vec3 v1(v2.x, v0.y, v0.z); // to avoid float->half conversion
				Vec3 v3(v0.x, v2.y, v0.z); // to avoid float->half conversion

				Vec2 gradientUvMin, gradientUvMax;
				GetGradientTextureCoord(gradientUvMin.x, gradientUvMin.y, gradientUvMax.x, gradientUvMax.y);

				// define the frame quad
				pVertex[0].xyz = v0;
				pVertex[0].color.dcolor = frameColor;
				pVertex[0].st = Vec2(gradientUvMin.x, gradientUvMax.y);

				pVertex[1].xyz = v1;
				pVertex[1].color.dcolor = frameColor;
				pVertex[1].st = pVertex[0].st;

				pVertex[2].xyz = v2;
				pVertex[2].color.dcolor = frameColor;
				pVertex[2].st = pVertex[0].st;

				pVertex[3].xyz = v2;
				pVertex[3].color.dcolor = frameColor;
				pVertex[3].st = pVertex[0].st;

				pVertex[4].xyz = v3;
				pVertex[4].color.dcolor = frameColor;
				pVertex[4].st = pVertex[0].st;

				pVertex[5].xyz = v0;
				pVertex[5].color.dcolor = frameColor;
				pVertex[5].st = pVertex[0].st;

				vbOffset = 6;
			}
		}

		// parse the string, ignoring control characters
		Unicode::CIterator<const char*, false> pChar(pStr);
		while (uint32_t ch = *pChar)
		{
			++pChar;
			switch (ch)
			{
			case '\\':
				{
					if (*pChar != 'n' || !asciiMultiLine)
						break;
					++pChar;
				}
			case '\n':
				{
					charX = baseXY.x + offset.x;
					charY += size.y;
					continue;
				}
				break;
			case '\r':
				{
					charX = baseXY.x + offset.x;
					continue;
				}
				break;
			case '\t':
				{
					if (ctx.m_proportional)
						charX += TabCharCount * size.x * FONT_SPACE_SIZE;
					else
						charX += TabCharCount * size.x * ctx.m_widthScale;
					continue;
				}
				break;
			case ' ':
				{
					if (ctx.m_proportional)
						charX += size.x * FONT_SPACE_SIZE;
					else
						charX += size.x * ctx.m_widthScale;
					continue;
				}
				break;
			case '$':
				{
					if (*pChar == '$')
						++pChar;
					else if (isdigit(*pChar))
					{
						if (!i)
						{
							int colorIndex = (*pChar) - '0';
							ColorB newColor = ColorTable[colorIndex];
							color.r = newColor.r;
							color.g = newColor.g;
							color.b = newColor.b;
							// Leave alpha at original value!
						}
						++pChar;
						continue;
					}
					else if (*pChar == 'O' || *pChar == 'o')
					{
						if (!i)
							color = passColor;
						++pChar;
						continue;
					}
					else if (*pChar)
					{
						//++pChar;
						//continue;
					}
				}
				break;
			default:
				break;
			}

			int charWidth = m_pFontTexture->GetCharacterWidth(ch);
			float width = charWidth * rcpCellWidth;

			// get texture coordinates
			float texCoord[4];

			int charOffsetX, charOffsetY; // in font texels
			int charSizeX, charSizeY;     // in font texels
			m_pFontTexture->GetTextureCoord(m_pFontTexture->GetCharSlot(ch), texCoord, charSizeX, charSizeY, charOffsetX, charOffsetY);

			float advance;

			if (ctx.m_proportional)
			{
				advance = (charWidth + FONT_GLYPH_PROP_SPACING) * scale.x; // FONT_GLYPH_PROP_SPACING pixel space between characters for proportional fonts
				charOffsetX = 0;
			}
			else
				advance = size.x * ctx.m_widthScale;

			float px = charX + charOffsetX * scale.x; // in pixels
			float py = charY + charOffsetY * scale.y; // in pixels
			float pr = px + charSizeX * scale.x;
			float pb = py + charSizeY * scale.y;

			// compute clipping
			float newX = px; // in pixels
			float newY = py; // in pixels
			float newR = pr; // in pixels
			float newB = pb; // in pixels

			if (ctx.m_clippingEnabled)
			{
				float clipX = ctx.m_clipX;
				float clipY = ctx.m_clipY;
				float clipR = ctx.m_clipX + ctx.m_clipWidth;
				float clipB = ctx.m_clipY + ctx.m_clipHeight;

				// clip non visible
				if ((px >= clipR) || (py >= clipB) || (pr < clipX) || (pb < clipY))
				{
					charX += advance;
					continue;
				}
				// clip partially visible
				else
				{
					if ((width <= 0.0f) || (size.y <= 0.0f))
					{
						charX += advance;
						continue;
					}

					// clip the image to the scissor rect
					newX = max(clipX, px);
					newY = max(clipY, py);
					newR = min(clipR, pr);
					newB = min(clipB, pb);

					float rcpWidth = 1.0f / width;
					float rcpHeight = 1.0f / size.y;

					float texW = texCoord[2] - texCoord[0];
					float texH = texCoord[3] - texCoord[1];

					// clip horizontal
					texCoord[0] = texCoord[0] + texW * (newX - px) * rcpWidth;
					texCoord[2] = texCoord[2] + texW * (newR - pr) * rcpWidth;

					// clip vertical
					texCoord[1] = texCoord[1] + texH * (newY - py) * rcpHeight;
					texCoord[3] = texCoord[3] + texH * (newB - pb) * rcpHeight;
				}
			}

			newX += halfTexelShift;
			newY += halfTexelShift;
			newR += halfTexelShift;
			newB += halfTexelShift;

			//int offset = vbLen * 6;

			Vec3 v0(newX, newY, z);
			Vec3 v2(newR, newB, z);
			Vec3 v1(v2.x, v0.y, v0.z); // to avoid float->half conversion
			Vec3 v4(v0.x, v2.y, v0.z); // to avoid float->half conversion

			Vec2 tc0(texCoord[0], texCoord[1]);
			Vec2 tc2(texCoord[2], texCoord[3]);
			Vec2 tc1(tc2.x, tc0.y); // to avoid float->half conversion
			Vec2 tc3(tc0.x, tc2.y); // to avoid float->half conversion

			DWORD packedColor = 0;
			{
				ColorB tempColor = color;
				tempColor.a = ((uint32) tempColor.a * alphaBlend) >> 8;
				packedColor = tempColor.pack_abgr8888();

				if (!isRGB)
					packedColor = COLCONV(packedColor);
			}

			// define char quad
			pVertex[vbOffset].xyz = v0;
			pVertex[vbOffset].color.dcolor = packedColor;
			pVertex[vbOffset].st = tc0;

			pVertex[vbOffset + 1].xyz = v1;
			pVertex[vbOffset + 1].color.dcolor = packedColor;
			pVertex[vbOffset + 1].st = tc1;

			pVertex[vbOffset + 2].xyz = v2;
			pVertex[vbOffset + 2].color.dcolor = packedColor;
			pVertex[vbOffset + 2].st = tc2;

			pVertex[vbOffset + 3].xyz = v2;
			pVertex[vbOffset + 3].color.dcolor = packedColor;
			pVertex[vbOffset + 3].st = tc2;

			pVertex[vbOffset + 4].xyz = v4;
			pVertex[vbOffset + 4].color.dcolor = packedColor;
			pVertex[vbOffset + 4].st = tc3;

			pVertex[vbOffset + 5].xyz = v0;
			pVertex[vbOffset + 5].color.dcolor = packedColor;
			pVertex[vbOffset + 5].st = tc0;
			vbOffset += 6;

			IF (vbOffset == MaxDrawVBQuads * 6, 0)
			{
                pAux->EndDrawBuffer(vbOffset);
                pVertex = pAux->BeginDrawBuffer(MaxDrawVBQuads * 6, m_texID != 0);
				vbOffset = 0;
			}

			charX += advance;
		}


        pAux->EndDrawBuffer(vbOffset);
	}
	pAux->SetTexture(-1);
}

Vec2 CFFont::GetTextSize(const char* pStr, const bool asciiMultiLine, const STextDrawContext& ctx)
{
	IF (!pStr, 0)
		return Vec2(0.0f, 0.0f);

	return GetTextSizeUInternal(pStr, asciiMultiLine, ctx);
}

Vec2 CFFont::GetTextSizeUInternal(const char* pStr, const bool asciiMultiLine, const STextDrawContext& ctx)
{
	const size_t fxSize = m_effects.size();

	IF (!pStr || !m_pFontTexture || !fxSize, 0)
		return Vec2(0, 0);

	Prepare(pStr, false);

	IRenderer* pRenderer = gEnv->pRenderer;
	assert(pRenderer);

	Vec2 size = ctx.m_size; // in pixels
	if (ctx.m_sizeIn800x600)
		pRenderer->ScaleCoord(size.x, size.y);

	float rcpCellWidth;
	Vec2 scale;
	if (ctx.m_proportional)
	{
		// to have backward compatible behavior (can be refactored later)
		rcpCellWidth = (1.0f / (512.0f / 16.0f)) * size.x;
		scale = Vec2(rcpCellWidth, size.y / 32.0f);
	}
	else
	{
		rcpCellWidth = size.x / 16.0f;
		scale = Vec2(rcpCellWidth * ctx.m_widthScale, size.y * ctx.m_widthScale / 16.0f);
	}

	float maxW = 0;
	float maxH = 0;

	const size_t fxIdx = ctx.m_fxIdx < fxSize ? ctx.m_fxIdx : 0;
	const SEffect& fx = m_effects[fxIdx];

	for (size_t i = 0, numPasses = fx.m_passes.size(); i < numPasses; ++i)
	{
		const SRenderingPass* pPass = &fx.m_passes[numPasses - i - 1];

		// gather pass data
		Vec2 offset = pPass->m_posOffset;

		float charX = offset.x;
		float charY = offset.y + size.y;

		if (charY > maxH)
			maxH = charY;

		// parse the string, ignoring control characters
		Unicode::CIterator<const char*, false> pChar(pStr);
		while (uint32_t ch = *pChar)
		{
			++pChar;
			switch (ch)
			{
			case '\\':
				{
					if (*pChar != 'n' || !asciiMultiLine)
						break;

					++pChar;
				}
			case '\n':
				{
					if (charX > maxW)
						maxW = charX;

					charX = offset.x;
					charY += size.y;

					if (charY > maxH)
						maxH = charY;

					continue;
				}
				break;
			case '\r':
				{
					if (charX > maxW)
						maxW = charX;

					charX = offset.x;
					continue;
				}
				break;
			case '\t':
				{
					if (ctx.m_proportional)
						charX += TabCharCount * size.x * FONT_SPACE_SIZE;
					else
						charX += TabCharCount * size.x * ctx.m_widthScale;
					continue;
				}
				break;
			case ' ':
				{
					charX += FONT_SPACE_SIZE * size.x;
					continue;
				}
				break;
			case '$':
				{
					if (*pChar == '$')
						++pChar;
					else if (*pChar)
					{
						++pChar;
						continue;
					}
				}
				break;
			default:
				break;
			}

			float advance;
			if (ctx.m_proportional)
			{
				int iCharWidth = m_pFontTexture->GetCharacterWidth(ch);
				advance = (iCharWidth + FONT_GLYPH_PROP_SPACING) * scale.x; // FONT_GLYPH_PROP_SPACING pixel space between characters for proportional fonts
			}
			else
				advance = size.x * ctx.m_widthScale;

			charX += advance;
		}

		if (charX > maxW)
			maxW = charX;
	}

	return Vec2(maxW, maxH);
}

size_t CFFont::GetTextLength(const char* pStr, const bool asciiMultiLine) const
{
	size_t len = 0;

	// parse the string, ignoring control characters
	const char* pChar = pStr;
	while (char ch = *pChar++)
	{
		if ((ch & 0xC0) == 0x80) continue; // Skip UTF-8 continuation bytes, we count only the first byte of a code-point

		switch (ch)
		{
		case '\\':
			{
				if (*pChar != 'n' || !asciiMultiLine)
				{
					break;
				}
				++pChar;
			}
		case '\n':
		case '\r':
		case '\t':
			{
				continue;
			}
			break;
		case '$':
			{
				if (*pChar == '$')
				{
					++pChar;
				}
				else if (*pChar)
				{
					++pChar;
					continue;
				}
			}
			break;
		default:
			break;
		}
		++len;
	}

	return len;
}

void CFFont::WrapText(string& result, float maxWidth, const char* pStr, const STextDrawContext& ctx)
{
	result = pStr;

	if (ctx.m_sizeIn800x600)
		maxWidth = gEnv->pRenderer->ScaleCoordX(maxWidth);

	Vec2 strSize = GetTextSizeUInternal(result.c_str(), true, ctx);

	if (strSize.x <= maxWidth)
		return;

	int lastSpace = -1;
	const char* pLastSpace = NULL;
	float lastSpaceWidth = 0.0f;

	float curCharWidth = 0.0f;
	float curLineWidth = 0.0f;
	float biggestLineWidth = 0.0f;
	float widthSum = 0.0f;

	int curChar = 0;
	Unicode::CIterator<const char*, false> pChar(result.c_str());
	while (uint32_t ch = *pChar)
	{
		++pChar;
		// ignore color codes
		if (ch == '$')
		{
			if (*pChar)
			{
				++pChar;
				++curChar;

				if ((*pChar) != '$')
				{
					++curChar;
					continue;
				}
				ch = *pChar;
			}
		}

		// get char width and sum it to the line width
		// Note: This is not unicode compatible, since char-width depends on surrounding context (ie, combining diacritics etc)
		char codepoint[5];
		Unicode::Convert(codepoint, ch);
		curCharWidth = GetTextSizeUInternal(codepoint, true, ctx).x;

		// keep track of spaces
		// they are good for splitting the string
		if (ch == ' ')
		{
			lastSpace = curChar;
			lastSpaceWidth = curLineWidth + curCharWidth;
			pLastSpace = pChar.GetPosition();
			assert(*pLastSpace == ' ');
		}

		// if line exceed allowed width, split it
		if ((curLineWidth + curCharWidth >= maxWidth) && (*pChar))
		{
			if ((lastSpace > 0) && ((curChar - lastSpace) < 16) && (curChar - lastSpace > 0)) // 16 is the default threshold
			{
				*(char*)pLastSpace = '\n';  // This is safe inside UTF-8 because space is single-byte codepoint

				if (lastSpaceWidth > biggestLineWidth)
					biggestLineWidth = lastSpaceWidth;

				curLineWidth = curLineWidth - lastSpaceWidth + curCharWidth;
				widthSum += curLineWidth;
			}
			else
			{
				const char* pBuf = pChar.GetPosition();
				size_t bytesProcessed = pBuf - result.c_str();
				result.insert(bytesProcessed, '\n');    // Insert the newline, this invalidates the iterator
				pBuf = result.c_str() + bytesProcessed; // In case reallocation occurs, we ensure we are inside the new buffer
				assert(*pBuf == '\n');
				pChar.SetPosition(pBuf); // pChar once again points inside the target string, at the current character
				assert(*pChar == ch);
				++pChar;
				++curChar;

				if (curLineWidth > biggestLineWidth)
					biggestLineWidth = curLineWidth;

				widthSum += curLineWidth;
				curLineWidth = curCharWidth;
			}

			// if we don't need any more line breaks, then just stop
			if (strSize.x - widthSum <= maxWidth)
				break;

			lastSpaceWidth = 0;
			lastSpace = 0;
		}
		else
			curLineWidth += curCharWidth;

		++curChar;
	}
}

void CFFont::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(m_name);
	pSizer->AddObject(m_curPath);
	pSizer->AddObject(m_pFontTexture);
	pSizer->AddObject(m_pFontBuffer, m_fontBufferSize);
	pSizer->AddObject(m_effects);
	//pSizer->AddObject(m_pDrawVB, sizeof(SVF_P3F_C4B_T2F) * MaxDrawVBQuads * 6);
}

void CFFont::GetGradientTextureCoord(float& minU, float& minV, float& maxU, float& maxV) const
{
	const CTextureSlot* pSlot = m_pFontTexture->GetGradientSlot();
	assert(pSlot);

	float invWidth = 1.0f / (float) m_pFontTexture->GetWidth();
	float invHeight = 1.0f / (float) m_pFontTexture->GetHeight();

	// deflate by one pixel to avoid bilinear filtering on the borders
	minU = pSlot->vTexCoord[0] + invWidth;
	minV = pSlot->vTexCoord[1] + invHeight;
	maxU = pSlot->vTexCoord[0] + (pSlot->iCharWidth - 1) * invWidth;
	maxV = pSlot->vTexCoord[1] + (pSlot->iCharHeight - 1) * invHeight;
}

unsigned int CFFont::GetEffectId(const char* pEffectName) const
{
	if (pEffectName)
	{
		for (size_t i = 0, numEffects = m_effects.size(); i < numEffects; ++i)
		{
			if (!strcmp(m_effects[i].m_name.c_str(), pEffectName))
				return i;
		}
	}

	return 0;
}

bool CFFont::InitTexture()
{
	m_texID = gEnv->pRenderer->FontCreateTexture(m_pFontTexture->GetWidth(), m_pFontTexture->GetHeight(), (uint8*)m_pFontTexture->GetBuffer(), eTF_A8);
	return m_texID >= 0;
}

bool CFFont::InitCache()
{
	m_pFontTexture->CreateGradientSlot();

	// precache (not required but for faster printout later)
	const char first = ' ';
	const char last = '~';
	char buf[last - first + 2];
	char* p = buf;

	// precache all [normal] printable characters to the string (missing ones are updated on demand)
	for (int i = first; i <= last; ++i)
		*p++ = i;
	*p = 0;

	Prepare(buf, false);

	return true;
}

CFFont::SEffect* CFFont::AddEffect(const char* pEffectName)
{
	m_effects.push_back(SEffect(pEffectName));
	return &m_effects[m_effects.size() - 1];
}

CFFont::SEffect* CFFont::GetDefaultEffect()
{
	return &m_effects[0];
}

void CFFont::Prepare(const char* pStr, bool updateTexture)
{
	bool texUpdateNeeded = m_fontTexDirty || m_pFontTexture->PreCacheString(pStr) == 1;
	if (updateTexture && texUpdateNeeded && m_texID >= 0)
	{
		gEnv->pRenderer->FontUpdateTexture(m_texID, 0, 0, m_pFontTexture->GetWidth(), m_pFontTexture->GetHeight(), (unsigned char*) m_pFontTexture->GetBuffer());
		m_fontTexDirty = false;
	}
	else
		m_fontTexDirty = texUpdateNeeded;
}

#endif

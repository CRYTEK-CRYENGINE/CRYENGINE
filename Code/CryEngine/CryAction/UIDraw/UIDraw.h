// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: UI draw functions

   -------------------------------------------------------------------------
   History:
   - 07:11:2005: Created by Julien Darre

*************************************************************************/
#ifndef __UIDRAW_H__
#define __UIDRAW_H__

//-----------------------------------------------------------------------------------------------------

#include "IUIDraw.h"

//-----------------------------------------------------------------------------------------------------

class CUIDraw : public IUIDraw
{
public:
	void   Release();

	void   PreRender();
	void   PostRender();

	uint32 GetColorARGB(uint8 ucAlpha, uint8 ucRed, uint8 ucGreen, uint8 ucBlue);

	int    CreateTexture(const char* strName, bool dontRelease = true);

	bool   DeleteTexture(int iTextureID);

	void   GetTextureSize(int iTextureID, float& rfSizeX, float& rfSizeY);

	void   GetMemoryStatistics(ICrySizer* s);

	void   DrawTriangle(float fX0, float fY0, float fX1, float fY1, float fX2, float fY2, uint32 uiColor);

	void   DrawLine(float fX1, float fY1, float fX2, float fY2, uint32 uiDiffuse);

	void   DrawQuadSimple(float fX, float fY, float fSizeX, float fSizeY, uint32 uiDiffuse, int iTextureID);

	void   DrawQuad(float fX,
	                float fY,
	                float fSizeX,
	                float fSizeY,
	                uint32 uiDiffuse = 0,
	                uint32 uiDiffuseTL = 0, uint32 uiDiffuseTR = 0, uint32 uiDiffuseDL = 0, uint32 uiDiffuseDR = 0,
	                int iTextureID = -1,
	                float fUTexCoordsTL = 0.0f, float fVTexCoordsTL = 0.0f,
	                float fUTexCoordsTR = 1.0f, float fVTexCoordsTR = 0.0f,
	                float fUTexCoordsDL = 0.0f, float fVTexCoordsDL = 1.0f,
	                float fUTexCoordsDR = 1.0f, float fVTexCoordsDR = 1.0f,
	                bool bUse169 = true);

	void DrawImage(int iTextureID, float fX,
	               float fY,
	               float fSizeX,
	               float fSizeY,
	               float fAngleInDegrees,
	               float fRed,
	               float fGreen,
	               float fBlue,
	               float fAlpha,
	               float fS0 = 0.0f,
	               float fT0 = 0.0f,
	               float fS1 = 1.0f,
	               float fT1 = 1.0f);

	void DrawImageCentered(int iTextureID, float fX,
	                       float fY,
	                       float fSizeX,
	                       float fSizeY,
	                       float fAngleInDegrees,
	                       float fRed,
	                       float fGreen,
	                       float fBlue,
	                       float fAlpha);

	void DrawTextSimple(IFFont* pFont,
	                    float fX, float fY,
	                    float fSizeX, float fSizeY,
	                    const char* strText, ColorF color,
	                    EUIDRAWHORIZONTAL eUIDrawHorizontal, EUIDRAWVERTICAL eUIDrawVertical);

	void DrawText(IFFont* pFont,
	              float fX,
	              float fY,
	              float fSizeX,
	              float fSizeY,
	              const char* strText,
	              float fAlpha,
	              float fRed,
	              float fGreen,
	              float fBlue,
	              EUIDRAWHORIZONTAL eUIDrawHorizontalDocking,
	              EUIDRAWVERTICAL eUIDrawVerticalDocking,
	              EUIDRAWHORIZONTAL eUIDrawHorizontal,
	              EUIDRAWVERTICAL eUIDrawVertical);

	void DrawWrappedText(IFFont* pFont,
	                     float fX,
	                     float fY,
	                     float fMaxWidth,
	                     float fSizeX,
	                     float fSizeY,
	                     const char* strText,
	                     float fAlpha,
	                     float fRed,
	                     float fGreen,
	                     float fBlue,
	                     EUIDRAWHORIZONTAL eUIDrawHorizontalDocking,
	                     EUIDRAWVERTICAL eUIDrawVerticalDocking,
	                     EUIDRAWHORIZONTAL eUIDrawHorizontal,
	                     EUIDRAWVERTICAL eUIDrawVertical);

	void GetTextDim(IFFont* pFont,
	                float* fWidth,
	                float* fHeight,
	                float fSizeX,
	                float fSizeY,
	                const char* strText);

	void GetWrappedTextDim(IFFont* pFont,
	                       float* fWidth,
	                       float* fHeight,
	                       float fMaxWidth,
	                       float fSizeX,
	                       float fSizeY,
	                       const char* strText);

	// ~IUIDraw

	CUIDraw();
	~CUIDraw();

protected:
	void InternalDrawText(IFFont* pFont,
	                      float fX,
	                      float fY,
	                      float fMaxWidth,
	                      float fSizeX,
	                      float fSizeY,
	                      const char* strText,
	                      float fAlpha,
	                      float fRed,
	                      float fGreen,
	                      float fBlue,
	                      EUIDRAWHORIZONTAL eUIDrawHorizontalDocking,
	                      EUIDRAWVERTICAL eUIDrawVerticalDocking,
	                      EUIDRAWHORIZONTAL eUIDrawHorizontal,
	                      EUIDRAWVERTICAL eUIDrawVertical);

	void InternalGetTextDim(IFFont* pFont,
	                        float* fWidth,
	                        float* fHeight,
	                        float fMaxWidth,
	                        float fSizeX,
	                        float fSizeY,
	                        const char* strText);

	typedef std::map<int, ITexture*> TTexturesMap;
	TTexturesMap m_texturesMap;

	IRenderer*   m_pRenderer;
};

//-----------------------------------------------------------------------------------------------------

#endif

//-----------------------------------------------------------------------------------------------------

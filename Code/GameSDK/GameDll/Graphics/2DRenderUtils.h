// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: UI draw functions

-------------------------------------------------------------------------
History:
- 07:11:2005: Created by Julien Darre
- 18:08:2009: Refactored for consistency by Frank Harrison
- 01:09:2009: Major refactor by Frank Harrison
- 22:09:2009: Moved from UIDraw to game side.

*************************************************************************/
#ifndef __2DRenderUtils_H__
#define __2DRenderUtils_H__

//-----------------------------------------------------------------------------------------------------

#include <CryMath/Cry_Color.h>
#include "IUIDraw.h" // For alignment flags.
#include <CryRenderer/IRenderAuxGeom.h> // RenderStates

//-----------------------------------------------------------------------------------------------------

typedef uint8 UIDRAWFLAGS;

//-----------------------------------------------------------------------------------------------------

class ScreenLayoutManager;

//-----------------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------------

class C2DRenderUtils //: public IUIDraw
{

public:

	C2DRenderUtils(ScreenLayoutManager* pLayoutManager);
	~C2DRenderUtils();

	void Release();

	void PreRender();
	void PostRender();

	void GetMemoryStatistics(ICrySizer * s);

	void RenderTest( float fTime );

	//-------------------------------------------------------------------
	// Shapes

	void DrawTriangle(float fX0,float fY0,float fX1,float fY1,float fX2,float fY2,const ColorF& uiColor);

	void DrawLine( float fX1, float fY1, float fX2, float fY2, const ColorF& uiDiffuse);

	void DrawQuad(float fX, float fY,
		float fSizeX, float fSizeY,
		const ColorF& uiDiffuse=ColorF(0,0,0,0),
#if C2DRU_USE_DVN_VB
		const ColorF& uiDiffuseTL=ColorF(0,0,0,0),
		const ColorF& uiDiffuseTR=ColorF(0,0,0,0),
		const ColorF& uiDiffuseDL=ColorF(0,0,0,0),
		const ColorF& uiDiffuseDR=ColorF(0,0,0,0),
#endif
		int iTextureID=-1
#if C2DRU_USE_DVN_VB
		,
		float fUTexCoordsTL=0.0f,float fVTexCoordsTL=0.0f,
		float fUTexCoordsTR=1.0f,float fVTexCoordsTR=0.0f,
		float fUTexCoordsDL=0.0f,float fVTexCoordsDL=1.0f,
		float fUTexCoordsDR=1.0f,float fVTexCoordsDR=1.0f
#endif 
		);

	void DrawRect( float x, float y, float fSizeX, float fSizeY, const ColorF& color );


	//--------------------------------------------------------------------------
	// Images

	void DrawImage( int iTextureID,
	                float fX, float fY,
	                float fSizeX, float fSizeY,
	                float fAngleInDegrees,
	                const ColorF& cfColor,
	                float fS0=0.0f, float fT0=0.0f,
	                float fS1=1.0f, float fT1=1.0f,
	                const EUIDRAWHORIZONTAL eUIDrawHorizontal        = UIDRAWHORIZONTAL_LEFT,
	                const EUIDRAWVERTICAL   eUIDrawVertical          = UIDRAWVERTICAL_TOP,
	                const EUIDRAWHORIZONTAL eUIDrawHorizontalDocking = UIDRAWHORIZONTAL_LEFT,
	                const EUIDRAWVERTICAL   eUIDrawVerticalDocking   = UIDRAWVERTICAL_TOP,
									const bool pushToList = false );

	void DrawImageStereo( int iTextureID,
	                float fX, float fY,
	                float fSizeX, float fSizeY,
	                float fAngleInDegrees,
	                const ColorF& cfColor,
	                float fS0=0.0f, float fT0=0.0f,
	                float fS1=1.0f, float fT1=1.0f,
									float fStereoDepth=0.0f,
	                const EUIDRAWHORIZONTAL eUIDrawHorizontal        = UIDRAWHORIZONTAL_LEFT,
	                const EUIDRAWVERTICAL   eUIDrawVertical          = UIDRAWVERTICAL_TOP,
	                const EUIDRAWHORIZONTAL eUIDrawHorizontalDocking = UIDRAWHORIZONTAL_LEFT,
	                const EUIDRAWVERTICAL   eUIDrawVerticalDocking   = UIDRAWVERTICAL_TOP);

	void DrawImageCentered(int iTextureID,float fX,
		float fY,
		float fSizeX,
		float fSizeY,
		float fAngleInDegrees,
		const ColorF& cfColor,
		const bool pushToList = false );

	void Draw2dImageList();

	//------------------------------------------------
	// Text

	void SetFont( IFFont *pFont );

	void DrawText( 
		const float fX, const float fY,
		const float fSizeX, const float fSizeY,
		const char *strText,
		const ColorF& color,
		EUIDRAWHORIZONTAL	eUIDrawHorizontal        = UIDRAWHORIZONTAL_LEFT, // Checked
		EUIDRAWVERTICAL		eUIDrawVertical          = UIDRAWVERTICAL_TOP,
		EUIDRAWHORIZONTAL	eUIDrawHorizontalDocking = UIDRAWHORIZONTAL_LEFT,
		EUIDRAWVERTICAL		eUIDrawVerticalDocking   = UIDRAWVERTICAL_TOP );

	void GetTextDim(IFFont *pFont,
		float *fWidth,
		float *fHeight,
		const float fSizeX,
		const float fSizeY,
		const char *strText);

	void DrawWrappedText(
		const float fX,
		const float fY,
		const float fMaxWidth,
		const float fSizeX,
		const float fSizeY,
		const char *strText,
		const ColorF& cfColor,
		const EUIDRAWHORIZONTAL	eUIDrawHorizontal        = UIDRAWHORIZONTAL_LEFT, // Checked
		const EUIDRAWVERTICAL		eUIDrawVertical          = UIDRAWVERTICAL_TOP,
		const EUIDRAWHORIZONTAL	eUIDrawHorizontalDocking = UIDRAWHORIZONTAL_LEFT,
		const EUIDRAWVERTICAL		eUIDrawVerticalDocking   = UIDRAWVERTICAL_TOP);

	void GetWrappedTextDim(	IFFont *pFont,
		float *fWidth,
		float *fHeight,
		const float fMaxWidth,
		const float fSizeX,
		const float fSizeY,
		const char *strText);

protected:
	// Internal funcs draw without scaling/aligning to SafeArea

	//-------------------------------------------------------------------
	// Shapes

	void InternalDrawTriangle(float fX0,float fY0,float fX1,float fY1,float fX2,float fY2,const ColorF& uiColor);

	void InternalDrawLine(float fX1, float fY1, float fX2, float fY2, const ColorF& uiDiffuse);

	void InternalDrawQuad( float fX, float fY,
	                       float fSizeX, float fSizeY,
	                       const ColorF& cfDiffuse=ColorF(0,0,0,0),
#                if C2DRU_USE_DVN_VB
	                       const ColorF& cfDiffuseTL=ColorF(0,0,0,0),
	                       const ColorF& cfDiffuseTR=ColorF(0,0,0,0),
	                       const ColorF& cfDiffuseDL=ColorF(0,0,0,0),
	                       const ColorF& cfDiffuseDR=ColorF(0,0,0,0),
#                endif // C2DRU_USE_DVN_VB
												 int iTextureID=-1
#                if C2DRU_USE_DVN_VB
												 ,
	                       float fUTexCoordsTL=0.0f, float fVTexCoordsTL=0.0f,
	                       float fUTexCoordsTR=1.0f, float fVTexCoordsTR=0.0f,
	                       float fUTexCoordsDL=0.0f, float fVTexCoordsDL=1.0f,
	                       float fUTexCoordsDR=1.0f, float fVTexCoordsDR=1.0f
#                endif
	                       );

	void InternalDrawRect( 
		float x1, float y1, 
		float x2, float y2, 
		const ColorF& color );

	//--------------------------------------------------------------------------
	// Images
	void InternalDrawImage( int iTextureID,
	                        float fX, float fY,
	                        float fSizeX, float fSizeY,
	                        float fAngleInDegrees,
	                        const ColorF& cfColor,
	                        float fS0, float fT0,
	                        float fS1, float fT1,
													const bool pushToList = false );

	void InternalDrawImageStereo( int iTextureID,
	                        float fX, float fY,
	                        float fSizeX, float fSizeY,
	                        float fAngleInDegrees,
	                        const ColorF& cfColor,
	                        float fS0, float fT0,
	                        float fS1, float fT1,
													float fStereoDepth);

	//------------------------------------------------
	// Text

	void InternalDrawText(
		const float fX, const float fY,
		const char *strText );

	void InternalGetTextDim( IFFont *pFont,
		float *fWidth,
		float *fHeight,
		float fMaxWidth,
		const char *strText );

	//------------------------------------------------------
	// Utility

	ILINE void InitFont( IFFont *pFont, const float sx, const float sy, const ColorF& col=ColorF(0.0f,0.0f,0.0f,0.0f))
	{
		// Copied from CRenderer::Draw2dText()
		// TODO : Fix proportional vs monospaced fonts.
		m_ctx.Reset();
		m_ctx.SetEffect(0);// use default effect (i.e. none) - not setting this before other calls which depend upon the effects [like SetColor()] causes a crash!
		m_ctx.SetSizeIn800x600(false); // We DO NOT want 800x600 here - 800x600 should go. 
		m_ctx.SetProportional(true);
		m_ctx.SetCharWidthScale(1);
		m_ctx.SetSize(Vec2(sx,sy));
		m_ctx.SetColor(col); // set the color the first pass only.
		m_ctx.SetFlags(0); // Handle this in 2DRenderUtils
	}

	//------------------------------------------------------
	// Test Render Funcs

#if ENABLE_HUD_EXTRA_DEBUG
	void RenderTest_CTRL( float fTime, const ColorF& color );
	void RenderTest_Grid( float fTime, const ColorF& color );
	void RenderTest_Tris( float fTime, const ColorF& color );
	void RenderTest_Text( float fTime, const ColorF& color );
	void RenderTest_Quads( float fTime, const ColorF& color );
	void RenderTest_Textures( float fTime, const ColorF& color );
#endif // ENABLE_HUD_EXTRA_DEBUG

	//------------------------------------------------------

	ScreenLayoutManager *m_pLayoutManager;
	IRenderer           *m_pRenderer;
	IRenderAuxGeom			*m_pAuxGeom;
	SAuxGeomRenderFlags m_prevAuxRenderFlags;

	ITexture            *m_white_texture;

	IFFont              *m_pFont;
	STextDrawContext    m_ctx;

#if ENABLE_HUD_EXTRA_DEBUG
	static int          s_debugTestLevel;
#endif
};

//-----------------------------------------------------------------------------------------------------

#endif // __2DRenderUtils_H__

//-----------------------------------------------------------------------------------------------------

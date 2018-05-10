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
#include "StdAfx.h"
#include "2DRenderUtils.h"
#include "UI/UIManager.h"
#include "UI/Utils/ScreenLayoutManager.h"

//-----------------------------------------------------------------------------------------------------

#if ENABLE_HUD_EXTRA_DEBUG
int  C2DRenderUtils::s_debugTestLevel = 0;
#endif

//-----------------------------------------------------------------------------------------------------

C2DRenderUtils::C2DRenderUtils(ScreenLayoutManager* pLayoutManager)
{
	m_pLayoutManager = pLayoutManager;
	m_pRenderer = gEnv->pRenderer;

	if (m_pRenderer)
	{
		// TODO : Init/compile for Render-Testing only or get from Textures.h...
		m_white_texture = m_pRenderer->EF_LoadTexture("%ENGINE%/EngineAssets/Textures/White.dds", FT_DONT_RELEASE | FT_DONT_STREAM);
		m_pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	}

	SetFont(gEnv->pCryFont->GetFont("default"));

#if ENABLE_HUD_EXTRA_DEBUG
	REGISTER_CVAR2("hud_2DtestRenderType", &s_debugTestLevel, 0, 0, "Draw 2D calibration graphics.");
#endif
}

//-----------------------------------------------------------------------------------------------------

C2DRenderUtils::~C2DRenderUtils()
{
#if ENABLE_HUD_EXTRA_DEBUG
	gEnv->pConsole->UnregisterVariable("hud_2DtestRenderType");
#endif
}

//-----------------------------------------------------------------------------------------------------

void C2DRenderUtils::Release()
{
	delete this;
}

//-----------------------------------------------------------------------------------------------------

void C2DRenderUtils::PreRender()
{
	if (!m_pRenderer)
	{
		return;
	}

	m_prevAuxRenderFlags	= m_pAuxGeom->GetRenderFlags().m_renderFlags;
	m_pAuxGeom->SetRenderFlags( e_Mode2D|e_AlphaBlended|e_FillModeSolid|e_CullModeNone|e_DepthWriteOff|e_DepthTestOff );
}

//-----------------------------------------------------------------------------------------------------

void C2DRenderUtils::PostRender()
{
	if (!m_pRenderer)
	{
		return;
	}

	//reset render settings (aux geometry)
	m_pAuxGeom->SetRenderFlags(m_prevAuxRenderFlags);
}

//-----------------------------------------------------------------------------------------------------
void C2DRenderUtils::DrawRect( float x, float y, float fSizeX, float fSizeY, const ColorF& screen_edge_color )
{
	m_pLayoutManager->AdjustToSafeArea( &x, &y, &fSizeX, &fSizeY );

	InternalDrawRect( x, y, fSizeX, fSizeY, screen_edge_color );
}

//-----------------------------------------------------------------------------------------------------
void C2DRenderUtils::InternalDrawRect( const float x, const float y, const float fSizeX, const float fSizeY, const ColorF& screen_edge_color )
{	
	if (!m_pRenderer)
	{
		return;
	}

	const float width = 1.0f;// TODO : fixme : g_pGameCVars->hud_safearea_linewidth;

	float x1 = x;
	float y1 = y;
	float x2 = x + fSizeX;
	float y2 = y + fSizeY;

	//m_pRenderer->SetState(GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA | GS_NODEPTHTEST);
	InternalDrawQuad( x1, y1, fSizeX+width, width,        screen_edge_color );//top
	InternalDrawQuad( x2, y1, width,        fSizeY+width, screen_edge_color );//right
	InternalDrawQuad( x1, y2, fSizeX+width, width,        screen_edge_color );//bottom
	InternalDrawQuad( x1, y1, width,        fSizeY+width, screen_edge_color );//left
}

//-----------------------------------------------------------------------------------------------------
void C2DRenderUtils::DrawLine( float fX1, float fY1, float fX2, float fY2, const ColorF& cfDiffuse)
{
	m_pLayoutManager->AdjustToSafeArea( &fX1, &fY1 );
	m_pLayoutManager->AdjustToSafeArea( &fX2, &fY2 );

	InternalDrawLine( fX1, fY1, fX2, fY2, cfDiffuse);
}

//-----------------------------------------------------------------------------------------------------
// FIXME :
//		Check this works on consoles.
void C2DRenderUtils::InternalDrawLine(float fX1, float fY1, float fX2, float fY2, const ColorF& cfDiffuse)
{
	if (!m_pRenderer)
	{
		return;
	}

	const float fOff = 0.0f;

	m_pLayoutManager->ConvertFromVirtualToNormalisedScreenSpace( &fX1, &fY1 );
	m_pLayoutManager->ConvertFromVirtualToNormalisedScreenSpace( &fX2, &fY2 );

	const Vec3 p1(fX1+fOff, fY1+fOff, 0.0f);
	const Vec3 p2(fX2+fOff, fY2+fOff, 0.0f);

	const ColorB lineCol = cfDiffuse;

	m_pAuxGeom->DrawLine( p1, lineCol, p2, lineCol, 1.0f );

}

//-----------------------------------------------------------------------------------------------------
void C2DRenderUtils::DrawTriangle(float fX0,float fY0,float fX1,float fY1,float fX2,float fY2,const ColorF& cfColor)
{
	m_pLayoutManager->AdjustToSafeArea( &fX0, &fY0 );
	m_pLayoutManager->AdjustToSafeArea( &fX1, &fY1 );
	m_pLayoutManager->AdjustToSafeArea( &fX2, &fY2 );

	InternalDrawTriangle( fX0, fY0, fX1, fY1, fX2, fY2, cfColor);
}

//-----------------------------------------------------------------------------------------------------
void C2DRenderUtils::InternalDrawTriangle(float fX0,float fY0,float fX1,float fY1,float fX2,float fY2,const ColorF& cfColor)
{
	if (!m_pRenderer)
	{
		return;
	}

	const float fOff = 0.0;

	m_pLayoutManager->ConvertFromVirtualToNormalisedScreenSpace( &fX0, &fY0 );
	m_pLayoutManager->ConvertFromVirtualToNormalisedScreenSpace( &fX1, &fY1 );
	m_pLayoutManager->ConvertFromVirtualToNormalisedScreenSpace( &fX2, &fY2 );

	Vec3 p0(fX0+fOff, fY0+fOff, 0.0f);
	Vec3 p1(fX1+fOff, fY1+fOff, 0.0f);
	Vec3 p2(fX2+fOff, fY2+fOff, 0.0f);

	m_pAuxGeom->DrawTriangle( p0, cfColor, p1, cfColor, p2, cfColor );
}

//-----------------------------------------------------------------------------------------------------
void C2DRenderUtils::DrawQuad( float fX, float fY,
                               float fSizeX, float fSizeY,
                               const ColorF& cfDiffuse,
                               int iTextureID
                               )
{
	m_pLayoutManager->AdjustToSafeArea( &fX, &fY, &fSizeX, &fSizeY );

	InternalDrawQuad(fX, fY, fSizeX, fSizeY, cfDiffuse);
}

//-----------------------------------------------------------------------------------------------------
void C2DRenderUtils::InternalDrawQuad( float fX, float fY,
                                       float fSizeX, float fSizeY,
                                       const ColorF& cfDiffuse,
                                       int iTextureID
)
{

	if (!m_pRenderer)
	{
		return;
	}

	const float fOff = 0.0f;//-0.5f;

	Vec2 p1( fX+fOff, fY+fOff );
	Vec2 p2( fX+fSizeX+fOff, fY+fSizeY+fOff );
	m_pLayoutManager->ConvertFromVirtualToRenderScreenSpace( &(p1.x), &(p1.y) );
	m_pLayoutManager->ConvertFromVirtualToRenderScreenSpace( &(p2.x), &(p2.y) );
	const float z  = 0.0f;

	uint32 uiDiffuse = cfDiffuse.pack_argb8888();

	InternalDrawImage(iTextureID,fX,fY,fSizeX,fSizeY,0.0f,cfDiffuse,0,0,1,1);

}

//-----------------------------------------------------------------------------------------------------

void C2DRenderUtils::Draw2dImageList()
{
	if (!m_pRenderer)
	{
		return;
	}
}

//-----------------------------------------------------------------------------------------------------

void C2DRenderUtils::DrawImage(int iTextureID,	
															 float fX, float fY,
															 float fSizeX, float fSizeY,
															 float fAngleInDegrees,
															 const ColorF& cfColor,
															 float fS0 /* = 0.0f*/, float fT0 /* = 0.0f*/,
															 float fS1 /* = 1.0f*/, float fT1 /* = 1.0f*/,
															 const EUIDRAWHORIZONTAL	eUIDrawHorizontal        /* = UIDRAWHORIZONTAL_LEFT*/,  // Checked
															 const EUIDRAWVERTICAL		eUIDrawVertical          /* = UIDRAWVERTICAL_TOP*/,
															 const EUIDRAWHORIZONTAL	eUIDrawHorizontalDocking /* = UIDRAWHORIZONTAL_LEFT*/,
															 const EUIDRAWVERTICAL		eUIDrawVerticalDocking   /* = UIDRAWVERTICAL_TOP */,
															 bool pushToList /* = false*/)
{
	m_pLayoutManager->AdjustToSafeArea( &fX, &fY, &fSizeX, &fSizeY, eUIDrawHorizontal, eUIDrawVertical, eUIDrawHorizontalDocking, eUIDrawVerticalDocking );

	InternalDrawImage( 
		iTextureID, 
		fX, fY, fSizeX, fSizeY, fAngleInDegrees,
		cfColor,
		fS0, fT0, fS1, fT1,
		pushToList);
}

void C2DRenderUtils::DrawImageStereo(int iTextureID,	
															 float fX, float fY,
															 float fSizeX, float fSizeY,
															 float fAngleInDegrees,
															 const ColorF& cfColor,
															 float fS0 /* = 0.0f*/, float fT0 /* = 0.0f*/,
															 float fS1 /* = 1.0f*/, float fT1 /* = 1.0f*/,
															 float fStereoDepth /* = 0.0f*/,
															 const EUIDRAWHORIZONTAL	eUIDrawHorizontal        /* = UIDRAWHORIZONTAL_LEFT*/,  // Checked
															 const EUIDRAWVERTICAL		eUIDrawVertical          /* = UIDRAWVERTICAL_TOP*/,
															 const EUIDRAWHORIZONTAL	eUIDrawHorizontalDocking /* = UIDRAWHORIZONTAL_LEFT*/,
															 const EUIDRAWVERTICAL		eUIDrawVerticalDocking   /* = UIDRAWVERTICAL_TOP */)
{
	m_pLayoutManager->AdjustToSafeArea( &fX, &fY, &fSizeX, &fSizeY, eUIDrawHorizontal, eUIDrawVertical, eUIDrawHorizontalDocking, eUIDrawVerticalDocking );

	InternalDrawImageStereo( 
		iTextureID, 
		fX, fY, fSizeX, fSizeY, fAngleInDegrees,
		cfColor,
		fS0, fT0, fS1, fT1,
		fStereoDepth);
}

//-----------------------------------------------------------------------------------------------------
void C2DRenderUtils::InternalDrawImage(int iTextureID,	
																			 float fX, float fY,
																			 float fSizeX, float fSizeY,
																			 float fAngleInDegrees,
																			 const ColorF& cfColor,
																			 float fS0 /* = 0.0f*/, float fT0 /* = 0.0f*/,
																			 float fS1 /* = 1.0f*/, float fT1 /* = 1.0f*/,
																			 bool pushToList /* = false*/)
{
	if (!m_pRenderer)
	{
		return;
	}

		IRenderAuxImage::Draw2dImage(	fX, fY,
			fSizeX, fSizeY,
			iTextureID,
			fS0,fT0,fS1,fT1,
			fAngleInDegrees,
			cfColor.r, cfColor.g, cfColor.b, cfColor.a);
}

void C2DRenderUtils::InternalDrawImageStereo(int iTextureID,	
																			 float fX, float fY,
																			 float fSizeX, float fSizeY,
																			 float fAngleInDegrees,
																			 const ColorF& cfColor,
																			 float fS0 /* = 0.0f*/, float fT0 /* = 0.0f*/,
																			 float fS1 /* = 1.0f*/, float fT1 /* = 1.0f*/,
																			 float fStereoDepth /* = 0.f*/)
{
	if (!m_pRenderer)
	{
		return;
	}

	IRenderAuxImage::Draw2dImage(	fX, fY,
		fSizeX, fSizeY,
		iTextureID,
		fS0,fT0,fS1,fT1,
		fAngleInDegrees,
		cfColor.r, cfColor.g, cfColor.b, cfColor.a,
		1.0f,
		fStereoDepth);
}

//-----------------------------------------------------------------------------------------------------

void C2DRenderUtils::DrawImageCentered(int iTextureID,	float fX,
																			 float fY,
																			 float fSizeX,
																			 float fSizeY,
																			 float fAngleInDegrees,
																			 const ColorF& cfColor,
																			 bool pushToList /* = false*/)
{
	float fImageX = fX - 0.5f * fSizeX;
	float fImageY = fY - 0.5f * fSizeY;

	DrawImage(iTextureID,fImageX,fImageY,fSizeX,fSizeY,fAngleInDegrees,cfColor, 0.0f, 0.0f, 1.0f, 1.0f, UIDRAWHORIZONTAL_LEFT, UIDRAWVERTICAL_TOP, UIDRAWHORIZONTAL_LEFT, UIDRAWVERTICAL_TOP, pushToList);
}

//-----------------------------------------------------------------------------------------------------

void C2DRenderUtils::DrawText( 
															const float fX, const float fY,
															const float fSizeX, const float fSizeY,
															const char *strText,
															const ColorF& color,
															const EUIDRAWHORIZONTAL	eUIDrawHorizontal        /* = UIDRAWHORIZONTAL_LEFT*/,  // Checked
															const EUIDRAWVERTICAL		eUIDrawVertical          /* = UIDRAWVERTICAL_TOP*/,
															const EUIDRAWHORIZONTAL	eUIDrawHorizontalDocking /* = UIDRAWHORIZONTAL_LEFT*/,
															const EUIDRAWVERTICAL		eUIDrawVerticalDocking   /* = UIDRAWVERTICAL_TOP */)
{
	float drawX = fX;
	float drawY = fY;
	float sizeX = (fSizeX<=0.0f) ? 15.0f : fSizeX;
	float sizeY = (fSizeY<=0.0f) ? 15.0f : fSizeY;

	InitFont( m_pFont, sizeX, sizeY, color );

	float w=0.0f, h=0.0f;
	InternalGetTextDim( m_pFont, &w, &h, 0.0f, strText );
	const float wcpy=w, hcpy=h;
	m_pLayoutManager->AdjustToSafeArea( &drawX, &drawY, &w, &h, eUIDrawHorizontal, eUIDrawVertical, eUIDrawHorizontalDocking, eUIDrawVerticalDocking );
	// Scale the font
	sizeX *= w/wcpy; 
	sizeY *= h/hcpy;
	InitFont( m_pFont, sizeX, sizeY, color );
	//InternalDrawRect( drawX, drawY, w, h, ColorF(0.0f,0,1.0f,0.1f));
	InternalDrawText( drawX, drawY, strText );
}

//-----------------------------------------------------------------------------------------------------

void C2DRenderUtils::InternalDrawText(const float fX, const float fY,
																			const char *strText )
{
	if (!m_pRenderer)
	{
		return;
	}

	float drawX = fX;
	float drawY = fY;
	m_pLayoutManager->ConvertFromVirtualToRenderScreenSpace( &drawX, &drawY );
	m_pFont->DrawString(drawX, drawY, strText, true, m_ctx);
	
	// Font drawing reset the no-depth test flag.
	//m_pRenderer->SetState(GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA | GS_NODEPTHTEST);
}

//-----------------------------------------------------------------------------------------------------
void C2DRenderUtils::GetTextDim(	IFFont *pFont,
																float *fWidth,
																float *fHeight,
																const float fSizeX,
																const float fSizeY,
																const char *strText)
{
	if(NULL == pFont)
	{
		return;
	}

	float sizeX = (fSizeX<=0.0f) ? 15.0f : fSizeX;
	float sizeY = (fSizeY<=0.0f) ? 15.0f : fSizeY;

	InitFont( pFont, sizeX, sizeY );

	InternalGetTextDim( pFont, fWidth, fHeight, 0.0f, strText );
}

//-----------------------------------------------------------------------------------------------------
void C2DRenderUtils::InternalGetTextDim(	IFFont *pFont,
																				float *fWidth,
																				float *fHeight,
																				float fMaxWidth,
																				const char *strText)
{
	string wrappedStr;
	const bool bWrapText = fMaxWidth > 0.0f;
	if (bWrapText)
	{
		m_pFont->WrapText(wrappedStr, fMaxWidth, strText, m_ctx);
		strText = wrappedStr.c_str();
	}

	Vec2 dim=pFont->GetTextSize(strText, true, m_ctx);
	m_pLayoutManager->ConvertFromRenderToVirtualScreenSpace( &dim.x, &dim.y );

	if (fWidth)
		*fWidth = dim.x;
	if (fHeight)
		*fHeight = dim.y;
}

//-----------------------------------------------------------------------------------------------------
void C2DRenderUtils::DrawWrappedText(
																			const float fX,
																			const float fY,
																			const float fMaxWidth,
																			const float fSizeX,
																			const float fSizeY,
																			const char *strText,
																			const ColorF& cfColor,
																			const EUIDRAWHORIZONTAL	eUIDrawHorizontal /*= UIDRAWHORIZONTAL_LEFT*/, // Checked
																			const EUIDRAWVERTICAL		eUIDrawVertical /*= UIDRAWVERTICAL_TOP*/,
																			const EUIDRAWHORIZONTAL	eUIDrawHorizontalDocking /*= UIDRAWHORIZONTAL_LEFT*/,
																			const EUIDRAWVERTICAL		eUIDrawVerticalDocking /*= UIDRAWVERTICAL_TOP*/
																			)
{
	float drawX = fX;
	float drawY = fY;
	float sizeX = (fSizeX<=0.0f) ? 15.0f : fSizeX;
	float sizeY = (fSizeY<=0.0f) ? 15.0f : fSizeY;

	InitFont( m_pFont, fSizeX, fSizeY, cfColor );

	string wrappedStr;
	const bool bWrapText = fMaxWidth > 0.0f;
	if (bWrapText)
	{
		m_pFont->WrapText(wrappedStr, fMaxWidth, strText, m_ctx);
		strText = wrappedStr.c_str();
	}
	Vec2 vDim = m_pFont->GetTextSize(strText, true, m_ctx);

	const float wcpy=vDim.x, hcpy=vDim.y;
	m_pLayoutManager->AdjustToSafeArea( &drawX, &drawY, &vDim.x, &vDim.y, eUIDrawHorizontal, eUIDrawVertical, eUIDrawHorizontalDocking, eUIDrawVerticalDocking );

	// scale font size appropriately
	sizeX *= vDim.x/wcpy; 
	sizeY *= vDim.y/hcpy;
	InitFont( m_pFont, sizeX, sizeY, cfColor );
	//InternalDrawRect( drawX, drawY, vDim.x, vDim.y, ColorF(0.0f,1.0f,0.0f,0.1f));
	InternalDrawText( drawX, drawY, strText );
}

//-----------------------------------------------------------------------------------------------------
// TODO :
//	* Fix/Check rendering location/scaling.
void C2DRenderUtils::GetWrappedTextDim(IFFont *pFont,
																				float *fWidth,
																				float *fHeight,
																				const float fMaxWidth,
																				const float fSizeX,
																				const float fSizeY,
																				const char *strText)
{
	float sizeX = (fSizeX<=0.0f) ? 15.0f : fSizeX;
	float sizeY = (fSizeY<=0.0f) ? 15.0f : fSizeY;

	InitFont( pFont, sizeX, sizeY );
	InternalGetTextDim(pFont, fWidth, fHeight, fMaxWidth, strText);
}


//-----------------------------------------------------------------------------------------------------

void C2DRenderUtils::GetMemoryStatistics(ICrySizer * s)
{
	SIZER_SUBCOMPONENT_NAME(s, "UIDraw");
	s->Add(*this);
}

//-----------------------------------------------------------------------------------------------------

void C2DRenderUtils::SetFont( IFFont *pFont )
{
	assert(pFont);
	m_pFont = pFont;
}

//-----------------------------------------------------------------------------------------------------

#if ENABLE_HUD_EXTRA_DEBUG

void C2DRenderUtils::RenderTest_Grid( float fTime, const ColorF& color )
{
	const float rowheight = 10.0f;
	const float colwidth  = 10.0f;

	const int ylines = (int)(m_pLayoutManager->GetVirtualHeight()/rowheight);
	float fY = 0;
	for( int y=0; y<ylines; y++ )
	{
		DrawLine( 0, fY, m_pLayoutManager->GetVirtualWidth(), fY, color );
		fY += rowheight;
	}

	const int xlines = (int)(m_pLayoutManager->GetVirtualWidth()/colwidth);
	float fX = 0;
	for( int x=0; x<xlines; x++ )
	{
		DrawLine( fX, 0, fX, m_pLayoutManager->GetVirtualHeight(), color );
		fX += colwidth;
	}
}

void C2DRenderUtils::RenderTest_Tris( float fTime, const ColorF& color )
{
	float no_tris = 10.f;
	float tri_width  = m_pLayoutManager->GetVirtualWidth()/no_tris;
	float tri_height = m_pLayoutManager->GetVirtualHeight()/no_tris;

	const int xtris = (int)(m_pLayoutManager->GetVirtualWidth()/tri_width);
	const int ytris = (int)(m_pLayoutManager->GetVirtualHeight()/tri_height);

	float fX = 0;
	float fY = 0;
	for( int x=0; x<xtris; x++ ) //float fX=0; fX<=m_pLayoutManager->GetVirtualWidth()-tri_width; fX+=tri_width )
	{
		for( int y=0; y<ytris; y++ ) //float fY=0; fY<=m_pLayoutManager->GetVirtualHeight()-tri_height; fY+=tri_height )
		{
			float t1x = fX;
			float t1y = fY;
			float t2x = fX+tri_width;
			float t2y = fY;
			float t3x = fX+(tri_width/2.f);
			float t3y = fY+tri_height;

			DrawTriangle( t1x, t1y, t2x, t2y, t3x, t3y, color );

			ColorF black( 0.0f, 0.0f, 0.0f, 1.0f );
			DrawLine( t1x, t1y, t2x, t2y, black );
			DrawLine( t2x, t2y, t3x, t3y, black );
			DrawLine( t3x, t3y, t1x, t1y, black );

			fY += tri_height;
		}

		fY = 0;
		fX += tri_width;
	}
}

void C2DRenderUtils::RenderTest_Text( float fTime, const ColorF& color )
{
	IFFont *deffont = gEnv->pCryFont->GetFont("default");
	SetFont(deffont);

	ColorF colAligned = color;
	colAligned.a = 1.0f;
	// wchar wrapped
	const float maxWidth = 25.0f;
	ColorF trans = color;
	trans.a *= 0.5;

	DrawWrappedText(   0.f,   0.f, maxWidth, 20.f, 20.f, "TLWRAPPED", trans, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_TOP );
	DrawWrappedText( 800.f,   0.f, maxWidth, 20.f, 20.f, "TRWRAPPED", trans, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_TOP );
	DrawWrappedText( 400.f,   0.f, maxWidth, 20.f, 20.f, "TCWRAPPED", trans, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_TOP );
	DrawWrappedText( 800.f, 600.f, maxWidth, 20.f, 20.f, "BRWRAPPED", trans, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_BOTTOM );
	DrawWrappedText(   0.f, 600.f, maxWidth, 20.f, 20.f, "BLWRAPPED", trans, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_BOTTOM );
	DrawWrappedText( 400.f, 600.f, maxWidth, 20.f, 20.f, "BCWRAPPED", trans, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_BOTTOM );
	DrawWrappedText( 800.f, 300.f, maxWidth, 20.f, 20.f, "MRWRAPPED", trans, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_BOTTOM );
	DrawWrappedText(   0.f, 300.f, maxWidth, 20.f, 20.f, "MLWRAPPED", trans, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_BOTTOM );
	DrawWrappedText( 400.f, 300.f, maxWidth, 20.f, 20.f, "MCWRAPPED", trans, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_BOTTOM );
	DrawWrappedText(   0.f,   0.f, maxWidth, 20.f, 20.f, "TLWRAPPEDA", colAligned, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_TOP,    UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_TOP );
	DrawWrappedText(   0.f,   0.f, maxWidth, 20.f, 20.f, "TRWRAPPEDA", colAligned, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_TOP,    UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_TOP );
	DrawWrappedText(   0.f,   0.f, maxWidth, 20.f, 20.f, "TCWRAPPEDA", colAligned, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_TOP,    UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_TOP );
	DrawWrappedText(   0.f,   0.f, maxWidth, 20.f, 20.f, "BRWRAPPEDA", colAligned, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_BOTTOM, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_BOTTOM );
	DrawWrappedText(   0.f,   0.f, maxWidth, 20.f, 20.f, "BLWRAPPEDA", colAligned, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_BOTTOM, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_BOTTOM );
	DrawWrappedText(   0.f,   0.f, maxWidth, 20.f, 20.f, "BCWRAPPEDA", colAligned, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_BOTTOM, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_BOTTOM );
	DrawWrappedText(   0.f,   0.f, maxWidth, 20.f, 20.f, "MRWRAPPEDA", colAligned, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_CENTER, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_CENTER );
	DrawWrappedText(   0.f,   0.f, maxWidth, 20.f, 20.f, "MLWRAPPEDA", colAligned, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_CENTER, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_CENTER );
	DrawWrappedText(   0.f,   0.f, maxWidth, 20.f, 20.f, "MCWRAPPEDA", colAligned, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_CENTER, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_CENTER );

	// char
	/*
	DrawText(   0.f,   0.f, 20.f, 20.f, "TLDT", color, UIDRAWHORIZONTAL_LEFT, UIDRAWVERTICAL_TOP );
	DrawText( 800.f,   0.f, 20.f, 20.f, "TRDT", color, UIDRAWHORIZONTAL_RIGHT, UIDRAWVERTICAL_TOP );
	DrawText( 400.f,   0.f, 20.f, 20.f, "TopCenterDT", color, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_TOP );
	DrawText( 800.f, 600.f, 20.f, 20.f, "BRDT", color, UIDRAWHORIZONTAL_RIGHT, UIDRAWVERTICAL_BOTTOM );
	DrawText(   0.f, 600.f, 20.f, 20.f, "BLDT", color, UIDRAWHORIZONTAL_LEFT, UIDRAWVERTICAL_BOTTOM );
	DrawText(   0.f, 300.f, 20.f, 20.f, "BottomCenterDT", color, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_BOTTOM );
	DrawText( 800.f, 300.f, 20.f, 20.f, "MRDT", color, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_BOTTOM );
	DrawText(   0.f, 300.f, 20.f, 20.f, "MLDT", color, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_BOTTOM );
	DrawText( 400.f, 300.f, 20.f, 20.f, "MiddleCenterDT", color, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_BOTTOM );
	*/
	float posx = 0.0f, posy = 0.f;
	DrawText(   posx,   posy, 20.f, 20.f, "TLDTA", colAligned, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_TOP,    UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_TOP );
	DrawText(   posx,   posy, 20.f, 20.f, "TRDTA", colAligned, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_TOP,    UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_TOP );
	DrawText(   posx,   posy+20.0f, 20.f, 20.f, "TopCenterDTA", colAligned, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_TOP,    UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_TOP );
	DrawText(   posx,   posy, 20.f, 20.f, "BRDTA", colAligned, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_BOTTOM, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_BOTTOM );
	DrawText(   posx,   posy, 20.f, 20.f, "BLDTA", colAligned, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_BOTTOM, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_BOTTOM );
	DrawText(   posx,   posy-20.0f, 20.f, 20.f, "BottomCenterDTA", colAligned, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_BOTTOM, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_BOTTOM );
	DrawText(   posx,   posy, 20.f, 20.f, "MRDTA", colAligned, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_CENTER, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_CENTER );
	DrawText(   posx,   posy, 20.f, 20.f, "MLDTA", colAligned, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_CENTER, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_CENTER );
	DrawText(   posx,   posy+20.0f, 20.f, 20.f, "MiddleCenterDTA", colAligned, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_CENTER, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_CENTER );

	posx = 800.0f/3.0f;
	posy = 600.0f/3.0f;

	{
		ColorF dbgColour(1.f, 1.f, 1.f, 0.2f);
		CUIManager* pHud = g_pGame->GetUI();
		ScreenLayoutManager* pLayoutManager = pHud->GetLayoutManager();
		ScreenLayoutStates prevStates = pLayoutManager->GetState();
		//gEnv->pRenderer->SetState(GS_NODEPTHTEST);
		DrawQuad(  posx,    0.f,   1.f,  600.f, dbgColour); // T2B
		DrawQuad(     0.f, posy, 800.f,    1.f, dbgColour); // L2R
		pLayoutManager->SetState(prevStates);
	}

	// pivot align
	DrawText(   posx,   posy, 20.f, 20.f, "TLDTPA", colAligned, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_TOP,    UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_TOP );
	DrawText(   posx,   posy, 20.f, 20.f, "TRDTAPA", colAligned, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_TOP,    UIDRAWHORIZONTAL_LEFT,  UIDRAWVERTICAL_TOP );
	posy+=20.0f;
	DrawText(   posx,   posy, 20.f, 20.f, "MRDTPA", colAligned, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_CENTER, UIDRAWHORIZONTAL_LEFT,  UIDRAWVERTICAL_TOP );
	DrawText(   posx,   posy, 20.f, 20.f, "MLDTPA", colAligned, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_CENTER, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_TOP  );
	posy+=20.0f;
	DrawText(   posx,   posy, 20.f, 20.f, "BRDTPA", colAligned, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_BOTTOM, UIDRAWHORIZONTAL_LEFT,  UIDRAWVERTICAL_TOP  );
	DrawText(   posx,   posy, 20.f, 20.f, "BLDTPA", colAligned, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_BOTTOM, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_TOP  );
	posy+=20.0f;
	DrawText(   posx,   posy,       20.f, 20.f, "TopCenterDTAPA", colAligned, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_TOP,    UIDRAWHORIZONTAL_LEFT, UIDRAWVERTICAL_TOP );
	DrawText(   posx,   posy+20.f,  20.f, 20.f, "MiddleCenterDTPA", colAligned, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_CENTER, UIDRAWHORIZONTAL_LEFT, UIDRAWVERTICAL_TOP  );
	DrawText(   posx,   posy+40.0f, 20.f, 20.f, "BottomCenterDTPA", colAligned, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_BOTTOM, UIDRAWHORIZONTAL_LEFT, UIDRAWVERTICAL_TOP  );

	// screen align
	DrawText(   0.f,   0.f, 20.f, 20.f, "TLDTSA", colAligned, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_TOP,    UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_TOP );
	DrawText(   0.f,   0.f, 20.f, 20.f, "TRDTSA", colAligned, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_TOP,    UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_TOP );
	DrawText(   0.f,   0.f, 20.f, 20.f, "TopCenterDTSA", colAligned, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_TOP,    UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_TOP );
	DrawText(   0.f,   0.f, 20.f, 20.f, "BRDTSA", colAligned, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_TOP,    UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_BOTTOM );
	DrawText(   0.f,   0.f, 20.f, 20.f, "BLDTSA", colAligned, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_TOP,    UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_BOTTOM );
	DrawText(   0.f,   0.f, 20.f, 20.f, "BottomCenterDTSA", colAligned, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_TOP,    UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_BOTTOM );
	DrawText(   0.f,   0.f, 20.f, 20.f, "MRDTSA", colAligned, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_TOP,    UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_CENTER );
	DrawText(   0.f,   0.f, 20.f, 20.f, "MLDTSA", colAligned, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_TOP,    UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_CENTER );
	DrawText(   0.f,   0.f, 20.f, 20.f, "MiddleCenterDTSA", colAligned, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_TOP,    UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_CENTER );

	// wchar
	DrawText(   0.f,   0.f, 20.f, 20.f, "TLDW", color, UIDRAWHORIZONTAL_LEFT,  UIDRAWVERTICAL_TOP );
	DrawText( 800.f,   0.f, 20.f, 20.f, "TRDW", color, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_TOP );
	DrawText( 400.f,   0.f, 20.f, 20.f, "TCDW", color, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_TOP );
	DrawText( 800.f, 600.f, 20.f, 20.f, "BRDW", color, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_BOTTOM );
	DrawText(   0.f, 600.f, 20.f, 20.f, "BLDW", color, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_BOTTOM );
	DrawText( 400.f, 600.f, 20.f, 20.f, "BCDW", color, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_BOTTOM );
	DrawText( 800.f, 300.f, 20.f, 20.f, "MRDW", trans, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_BOTTOM );
	DrawText(   0.f, 300.f, 20.f, 20.f, "MLDW", trans, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_BOTTOM );
	DrawText( 400.f, 300.f, 20.f, 20.f, "MCDW", trans, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_BOTTOM );
	DrawText(   0.f,   0.f, 20.f, 20.f, "TLDWA", colAligned, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_TOP,    UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_TOP );
	DrawText(   0.f,   0.f, 20.f, 20.f, "TRDWA", colAligned, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_TOP,    UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_TOP );
	DrawText(   0.f,   0.f, 20.f, 20.f, "TCDWA", colAligned, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_TOP,    UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_TOP );
	DrawText(   0.f,   0.f, 20.f, 20.f, "BRDWA", colAligned, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_BOTTOM, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_BOTTOM );
	DrawText(   0.f,   0.f, 20.f, 20.f, "BLDWA", colAligned, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_BOTTOM, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_BOTTOM );
	DrawText(   0.f,   0.f, 20.f, 20.f, "BCDWA", colAligned, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_BOTTOM, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_BOTTOM );
	DrawText(   0.f,   0.f, 20.f, 20.f, "MRDWA", colAligned, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_CENTER, UIDRAWHORIZONTAL_RIGHT,  UIDRAWVERTICAL_CENTER );
	DrawText(   0.f,   0.f, 20.f, 20.f, "MLDWA", colAligned, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_CENTER, UIDRAWHORIZONTAL_LEFT,   UIDRAWVERTICAL_CENTER );
	DrawText(   0.f,   0.f, 20.f, 20.f, "MCDWA", colAligned, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_CENTER, UIDRAWHORIZONTAL_CENTER, UIDRAWVERTICAL_CENTER );

	for( int i=0; i<gEnv->pRenderer->GetWidth(); i+=10)
	{
		for( int j=0; j<gEnv->pRenderer->GetHeight(); j+=10)
		{
			ColorF mcColor( (float)i/(float)gEnv->pRenderer->GetWidth(), (float)j/(float)gEnv->pRenderer->GetHeight(),1.0f,0.2f );
			DrawText( (float)i, (float)j, 20.f, 20.f, "ixjabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", mcColor );
		}
	}
}

void C2DRenderUtils::RenderTest_Quads( float fTime, const ColorF& color )
{
	float no_quads = 20.f;
	float quad_width  = m_pLayoutManager->GetVirtualWidth()/no_quads;
	float quad_height = m_pLayoutManager->GetVirtualHeight()/no_quads;

	const int xquads = (int)(m_pLayoutManager->GetVirtualWidth()/quad_width);
	const int yquads = (int)(m_pLayoutManager->GetVirtualHeight()/quad_height);

	float fX = 0;
	float fY = 0;
	for( int ix=0; ix<xquads; ix++ ) //float fX=0; fX<=m_pLayoutManager->GetVirtualWidth()-quad_width; fX+=quad_width )
	{
		for( int iy=0; iy<yquads; iy++ ) //float fY=0; fY<=m_pLayoutManager->GetVirtualHeight()-quad_height; fY+=quad_height )
		{
			float x = fX;
			float y = fY;
			float sx = quad_width;
			float sy = quad_height;

			DrawQuad( x, y, sx, sy, color );

			ColorF black( 0.0f, 0.0f, 0.0f, 1.0f );
			DrawLine(    x,    y, x+sx,    y, black );
			DrawLine( x+sx,    y, x+sx, y+sy, black );
			DrawLine( x+sx, y+sy,    x, y+sy, black );
			DrawLine(    x, y+sy,    x,    y, black );

			fY += quad_height;
		}

		fX += quad_width;
		fY = 0;
	}
}

void C2DRenderUtils::RenderTest_Textures( float fTime, const ColorF& color )
{
	int textureID = m_white_texture->GetTextureID();

	float sx = 50.0f;
	float sy = 50.0f;

	ColorF renderCol = color;
	renderCol.r = CLAMP(renderCol.r, renderCol.r, renderCol.r+0.5f);
	renderCol.a *= 0.5f;
	DrawQuad(                                 0.f,   0.f, sx, sy, renderCol );
	DrawImage( textureID,                     0.f,   0.f, sx, sy, 0.0f, color, 0.0f, 1.0f, 1.0f, 0.0f );
	DrawQuad(             m_pLayoutManager->GetVirtualWidth()-sx,   0.f, sx, sy, renderCol );
	DrawImage( textureID, m_pLayoutManager->GetVirtualWidth()-sx,   0.f, sx, sy, 0.0f, color );
	DrawQuad(             m_pLayoutManager->GetVirtualWidth()-sx, m_pLayoutManager->GetVirtualHeight()-sy, sx, sy, renderCol );
	DrawImage( textureID, m_pLayoutManager->GetVirtualWidth()-sx, m_pLayoutManager->GetVirtualHeight()-sy, sx, sy, 0.0f, color );
	DrawQuad(                                 0.f, m_pLayoutManager->GetVirtualHeight()-sy, sx, sy, renderCol );
	DrawImage( textureID,                     0.f, m_pLayoutManager->GetVirtualHeight()-sy, sx, sy, 0.0f, color );
}

void C2DRenderUtils::RenderTest_CTRL( float fTime, const ColorF& activeColor )
{
	switch( s_debugTestLevel )
	{
	case 1 :
		RenderTest_Grid( fTime, activeColor );
		break;
	case 2 :
		RenderTest_Tris( fTime, activeColor );
		break;
	case 3 :
		RenderTest_Text( fTime, activeColor );
		break;
	case 4 :
		RenderTest_Textures( fTime, activeColor );
		break;
	case 5 :
		RenderTest_Quads( fTime, activeColor );
		break;
	}
}

void C2DRenderUtils::RenderTest( float fTime )
{
	if (!m_pRenderer)
	{
		return;
	}

	if(!s_debugTestLevel)
	{
		return;
	}

	const static float ktimeout = 5.0f;
	static float time = ktimeout;
	static int which = 0;
	const static int maxwhich = 2;
	time -= fTime;
	if( time < 0 )
	{
		time=ktimeout;
		which++;
		if( which>maxwhich )
		{
			which=0;
		}

		switch(which)
		{
		case 0:
			CryLogAlways("Full screen drawing");
			break;

		case 1:
			CryLogAlways("Safe area drawing only (no scaling)");
			break;

		case 2:
			CryLogAlways("Safe area drawing only & adapt to Y");
			break;
		}
	}

	ScreenLayoutStates prev_state = m_pLayoutManager->GetState();
	ColorF activeColor( 1.0f, 0.0f, 0.0f, 0.3f );
	//m_pRenderer->SetState(GS_BLSRC_SRCALPHA|GS_BLDST_ONEMINUSSRCALPHA|GS_NODEPTHTEST);

	switch(which)
	{
	case 0:
		//------------------
		// Full screen drawing
		m_pLayoutManager->SetState(eSLO_DoNotAdaptToSafeArea|eSLO_ScaleMethod_None);
		RenderTest_CTRL(fTime, activeColor);
		break;

	case 1:
		//------------------
		// Safe area drawing only (no scaling)
		m_pLayoutManager->SetState(eSLO_AdaptToSafeArea);
		activeColor = ColorF( 0.0f, 1.0f, 0.0f, 0.3f );
		RenderTest_CTRL(fTime, activeColor);
		break;

	case 2 :
		//------------------
		// Scale with Y
		m_pLayoutManager->SetState(eSLO_AdaptToSafeArea|eSLO_ScaleMethod_WithY);
		activeColor = ColorF( 0.0f, 0.0f, 1.0f, 0.3f );
		RenderTest_CTRL(fTime, activeColor);
		break;
	}

	m_pLayoutManager->SetState(prev_state);
}
#endif // ENABLE_HUD_EXTRA_DEBUG
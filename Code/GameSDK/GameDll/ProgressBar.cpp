// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProgressBar.h"
#include <CryRenderer/IRenderAuxGeom.h>

CProgressBar::CProgressBar():m_params(ProgressBarParams())
{

}

CProgressBar::~CProgressBar()
{

}

void CProgressBar::Init(const ProgressBarParams& params)
{
	m_params = params; 
}

void CProgressBar::SetProgressValue( float zeroToOneProgress )
{
	m_params.m_zeroToOneProgressValue = zeroToOneProgress;
}

void CProgressBar::SetFilledBarColour( const ColorB& col )
{
	m_params.m_filledBarColor = col; 
}

void CProgressBar::SetEmptyBarColour( const ColorB& col )
{
	m_params.m_emptyBarColor = col; 
}

void CProgressBar::SetBackerColour( ColorB& col )
{
	m_params.m_backerColor = col; 
}

void CProgressBar::SetPosition( const Vec2& pos )
{
	m_params.m_normalisedScreenPosition = pos; 
}

void CProgressBar::SetText(const char *inText, const ColorF &col)
{
	m_params.m_text = inText;
	m_params.m_textColor = col;
}

// Private helper functionality
void RenderBar(const Vec2& normalizedCentrePosOffset, const float barwidth,  const float barHeight, const ColorB& col, const float scale = 1.0f  )
{
	// Could cache this and inject in constructor if necessary
	//assert(pRenderAuxGeom); 
	struct IRenderAuxGeom* pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

	// TEST - draw a big X or text 
	float nBarWidth    =  barwidth;
	float nBarHeight   =  barHeight;

	float x = normalizedCentrePosOffset.x; 
	float y = normalizedCentrePosOffset.y; 

	// Setup Tri indices
	vtx_idx indTri[ 6 ] = 
	{
		0, 1, 2, 
		0, 2, 3	
	};	

	// Factor in scaling here + position offset
	float scaledBarWidth  = nBarWidth * scale;
	float scaledBarHeight = nBarHeight * scale;

	// Pos offset is to centre the bar
	float xPosOffset = normalizedCentrePosOffset.x + (-scaledBarWidth * 0.5f); 
	float yPosOffset = normalizedCentrePosOffset.y + (-scaledBarHeight * 0.5f); 

	Vec3 bar[ 4 ] =
	{
		Vec3( xPosOffset, yPosOffset, 0.0f ),
		Vec3( xPosOffset + scaledBarWidth, yPosOffset, 0.0f ),
		Vec3( xPosOffset + scaledBarWidth, yPosOffset + scaledBarHeight , 0.0f ),
		Vec3( xPosOffset, yPosOffset + scaledBarHeight, 0.0f )
	};

	pRenderAuxGeom->DrawTriangles( bar, 4, indTri, 6, col );
	
	
}

void CProgressBar::Render()
{
	// Could cache this and inject in constructor if necessary
	//assert(pRenderAuxGeom); 
	struct IRenderAuxGeom* pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	const SAuxGeomRenderFlags oldFlags = pRenderAuxGeom->GetRenderFlags();

	SAuxGeomRenderFlags flags( e_Def2DPublicRenderflags );
	flags.SetDepthTestFlag( e_DepthTestOff );
	flags.SetDepthWriteFlag( e_DepthWriteOff );
	flags.SetCullMode( e_CullModeNone );
	//flags.SetAlphaBlendMode( e_AlphaBlended );
	pRenderAuxGeom->SetRenderFlags( flags );

	// Draw 3 bars (backer, empty progress, full progress)

	// Draw Backer bar
	RenderBar(m_params.m_normalisedScreenPosition, m_params.m_width, m_params.m_height, m_params.m_backerColor);

	// Scale inner bars
	float innerBarScale = 0.95f; 

	// Draw remaining empty bar
	RenderBar(m_params.m_normalisedScreenPosition, m_params.m_width * innerBarScale, m_params.m_height * innerBarScale, m_params.m_emptyBarColor, innerBarScale);

	// Draw progress bar (scaled slightly smaller)
	if(m_params.m_zeroToOneProgressValue > 0.0f)
	{
		Vec2 filledBarPos = m_params.m_normalisedScreenPosition; // Need to counteract centering. 
		float filledBarWidth = m_params.m_width * m_params.m_zeroToOneProgressValue * innerBarScale; 
		filledBarPos.x += (0.5f * filledBarWidth) - (0.5f * m_params.m_width * innerBarScale);
		RenderBar(filledBarPos, filledBarWidth, m_params.m_height * innerBarScale, m_params.m_filledBarColor, innerBarScale);
	}

	// Draw string if there is one
	if (m_params.m_text.length() > 0)
	{
		float colour[4];
		colour[0] = m_params.m_textColor[0];
		colour[1] = m_params.m_textColor[1];
		colour[2] = m_params.m_textColor[2];
		colour[3] = m_params.m_textColor[3];
		
		float pixelPosX, pixelPosY;
		pixelPosX = m_params.m_normalisedScreenPosition.x * gEnv->pRenderer->GetOverlayWidth();
		pixelPosY = (m_params.m_normalisedScreenPosition.y-m_params.m_height*0.5f) * gEnv->pRenderer->GetOverlayHeight();
		IRenderAuxText::Draw2dLabel(pixelPosX, pixelPosY, 3.0f, colour, true, "%s", m_params.m_text.c_str());
	}

	// Restore old render flags
	pRenderAuxGeom->SetRenderFlags(oldFlags);
}

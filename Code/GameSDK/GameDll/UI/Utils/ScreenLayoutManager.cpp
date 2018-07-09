// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: UI draw functions

-------------------------------------------------------------------------
History:
- 22:09:2009: Created by Frank Harrison for console TRC compliance.

*************************************************************************/
#include "StdAfx.h"
#include "ScreenLayoutManager.h"
#include "UI/HUD/HUDEventDispatcher.h"

static const float EPSILON = FLT_EPSILON;

float ScreenLayoutManager::INV_VIRTUAL_SCREEN_WIDTH  = 1.0f/VIRTUAL_SCREEN_WIDTH;
float ScreenLayoutManager::INV_VIRTUAL_SCREEN_HEIGHT = 1.0f/VIRTUAL_SCREEN_HEIGHT;

float ScreenLayoutManager::RENDER_SCREEN_WIDTH = VIRTUAL_SCREEN_WIDTH;
float ScreenLayoutManager::RENDER_SCREEN_HEIGHT = VIRTUAL_SCREEN_HEIGHT;

float ScreenLayoutManager::INV_RENDER_SCREEN_WIDTH = ScreenLayoutManager::INV_VIRTUAL_SCREEN_HEIGHT;
float ScreenLayoutManager::INV_RENDER_SCREEN_HEIGHT = ScreenLayoutManager::INV_VIRTUAL_SCREEN_HEIGHT;

const float ScreenLayoutManager::TITLE_SAFE_AREA = 0.90f; //90%
const float ScreenLayoutManager::SONY_PS4_TRC_SAFE_AREA = 0.85f; //85%
const float ScreenLayoutManager::MICROSOFT_XBOXONE_TCR_SAFE_AREA = 0.85f; //85%
const float ScreenLayoutManager::ACTION_SAFE_AREA = 0.80f; //80%

ScreenLayoutManager* ScreenLayoutManager::s_inst = NULL;

//-----------------------------------------------------------------------------------------------------
ScreenLayoutManager::ScreenLayoutManager( )
: m_flags(eSLO_Default)
, m_modelViewMatrix(IDENTITY)
, m_projectionMatrix(IDENTITY)
{
	assert(!s_inst);
	s_inst = this;

	REGISTER_COMMAND("HUD_setCustomSafeArea",SetSafeArea,VF_CHEAT,"Set a custom safe area 0->1 is proportion or, >1 is an ID.");

	m_curSafeAreaID = eHSAID_default;

	UpdateHUDCanvasSize();
}

//-----------------------------------------------------------------------------------------------------
ScreenLayoutManager::~ScreenLayoutManager( )
{
	GetISystem()->GetIConsole()->RemoveCommand("HUD_setCustomSafeArea");

	s_inst = NULL;
}

//-----------------------------------------------------------------------------------------------------
void ScreenLayoutManager::SetState( ScreenLayoutStates flags )
{
	CRY_ASSERT_MESSAGE( !((flags&eSLO_ScaleMethod_None) && (flags&eSLO_ScaleMethod_WithY)), "HUD: Conflicting scale methods" );
	CRY_ASSERT_MESSAGE( !((flags&eSLO_ScaleMethod_None) && (flags&eSLO_ScaleMethod_WithX)), "HUD: Conflicting scale methods" );
	CRY_ASSERT_MESSAGE( !((flags&eSLO_ScaleMethod_WithY) && (flags&eSLO_ScaleMethod_WithX)), "HUD: Conflicting scale methods" );
	m_flags = flags; // |=  /?
}

//-----------------------------------------------------------------------------------------------------
ScreenLayoutStates ScreenLayoutManager::GetState( void ) const
{
	return (ScreenLayoutStates)m_flags;
}

void ScreenLayoutManager::UpdateHUDCanvasSize( void )
{
	if (gEnv->pRenderer)
	{
		const int renderWidth = gEnv->pRenderer->GetOverlayWidth();
		const int renderHeight = gEnv->pRenderer->GetOverlayHeight();
		RENDER_SCREEN_WIDTH = (float)renderWidth;
		RENDER_SCREEN_HEIGHT = (float)renderHeight;

		assert(RENDER_SCREEN_WIDTH > 0.0f);
		assert(RENDER_SCREEN_HEIGHT > 0.0f);
		INV_RENDER_SCREEN_WIDTH = 1.0f*__fres(RENDER_SCREEN_WIDTH);
		INV_RENDER_SCREEN_HEIGHT = 1.0f*__fres(RENDER_SCREEN_HEIGHT);

		// Force update of HUDAssets and other objects.
		const float pa = gEnv->pRenderer->GetPixelAspectRatio();
		Vec2 curCanvasSize(RENDER_SCREEN_WIDTH*__fres(pa), RENDER_SCREEN_HEIGHT);
#if 0
		Vec2 bp = GetSafeAreaBorderScreenFraction(eHSAID_curent);
		bp.x = 1.0f - (2 * bp.x);
		bp.y = 1.0f - (2 * bp.y);
		curCanvasSize.x *= bp.x;
		curCanvasSize.y *= bp.y;
#endif

		SHUDEvent resizeEvent(eHUDEvent_OnResolutionChange);
		resizeEvent.AddData(SHUDEventData(curCanvasSize.x));
		resizeEvent.AddData(SHUDEventData(curCanvasSize.y));
		resizeEvent.AddData(SHUDEventData(RENDER_SCREEN_WIDTH));
		resizeEvent.AddData(SHUDEventData(RENDER_SCREEN_HEIGHT));
		resizeEvent.AddData(SHUDEventData(int_round(RENDER_SCREEN_WIDTH)));
		resizeEvent.AddData(SHUDEventData(int_round(RENDER_SCREEN_HEIGHT)));
		//resizeEvent.AddData(SHUDEventData(pa));
		CHUDEventDispatcher::CallEvent(resizeEvent);
	}
}

//---------------------------------------------------
void ScreenLayoutManager::SetSafeArea( EHUDSafeAreaID eSafeAreaID ) 
{
#if !defined(_RELEASE)
	m_curSafeAreaID = eSafeAreaID;
#else
	m_curSafeAreaID = eHSAID_default; // Only use overscan borders for release
#endif
	UpdateHUDCanvasSize();
}

//---------------------------------------------------
void ScreenLayoutManager::SetSafeArea( const Vec2& safe_area_percentage ) 
{
#if !defined(_RELEASE)
	m_curSafeAreaID = eHSAID_custom;
#else
	m_curSafeAreaID = eHSAID_default; // Only use overscan borders for release
#endif
	m_customSafeArea = safe_area_percentage;
	UpdateHUDCanvasSize();
}

//---------------------------------------------------
/*static*/ void ScreenLayoutManager::SetSafeArea( IConsoleCmdArgs* pArgs )
{
	if( !s_inst )
	{
		CryLogAlways( "No ScreenLayoutInstance available!" );
		return;
	}

	if( pArgs->GetArgCount() != 2 && pArgs->GetArgCount() != 3 )
	{
		CryLogAlways( "Incorrect Number of params" );
		return;
	}

	if( pArgs->GetArgCount() == 2 )
	{
		int id = (int)atoi( pArgs->GetArg(1) );

		// amount > 1.0f / probably an ID
		if( id <= 0 || id > eHSAID_END )
		{
			CryLog( "unknown safe area id." );
			return;
		}

		s_inst->SetSafeArea( (EHUDSafeAreaID)id );

		return;
	}

	float xammount = (float)atof( pArgs->GetArg(1) );
	float yammount = (float)atof( pArgs->GetArg(2) );
	if( xammount > 0.0f && yammount > 0.0f )
	{
		s_inst->SetSafeArea( Vec2(xammount, yammount) );
		return;
	}
}

const Vec2 ScreenLayoutManager::GetSafeAreaScreenProportion( EHUDSafeAreaID which /*= eHSAID_default*/ ) const
{
	const Vec2 ret = GetSafeAreaBorderScreenProportion( which );
	return Vec2(1.0f-(2*ret.x),1.0f-(2*ret.y));
}

//---------------------------------------------------
const Vec2 ScreenLayoutManager::GetSafeAreaBorderScreenProportion( EHUDSafeAreaID which /*= eHSAID_default*/ ) const
{
	Vec2 ret(1.0f,1.0f); // break badly on unhandled ;).
	switch( which )
	{
	case eHSAID_overscan :
		if(gEnv->pRenderer)
		{
			gEnv->pRenderer->EF_Query(EFQ_OverscanBorders, ret);
		}
		break;
	case eHSAID_fullscreen :
		ret.x -= 0.99f; // very thin
		ret.y -= 0.99f; // very thin
		break;
	case eHSAID_title :
		ret.x -= TITLE_SAFE_AREA;
		ret.y -= TITLE_SAFE_AREA;
		break;
	case eHSAID_action :
		ret.x -= ACTION_SAFE_AREA;
		ret.y -= ACTION_SAFE_AREA;
		break;
	case eHSAID_PS4_trc :
		ret.x -= SONY_PS4_TRC_SAFE_AREA;
		ret.y -= SONY_PS4_TRC_SAFE_AREA;
		break;
	case eHSAID_XBoxOne_tcr :
		ret.x -= MICROSOFT_XBOXONE_TCR_SAFE_AREA;
		ret.y -= MICROSOFT_XBOXONE_TCR_SAFE_AREA;
		break;
	case eHSAID_custom :
		ret -= m_customSafeArea;
		break;
	case eHSAID_curent :
		return GetSafeAreaBorderScreenProportion( m_curSafeAreaID );
	default :
		CRY_ASSERT_MESSAGE(0, "HUD: Unhandled SafeArea ID/type");
	}

	return ret*0.5f;
}

//-----------------------------------------------------------------------------------------------------
void ScreenLayoutManager::ConvertFromVirtualToRenderScreenSpace( float* inout_x, float* inout_y ) const
{
	*inout_x *= RENDER_SCREEN_WIDTH  * INV_VIRTUAL_SCREEN_WIDTH;
	*inout_y *= RENDER_SCREEN_HEIGHT * INV_VIRTUAL_SCREEN_HEIGHT;
}

//-----------------------------------------------------------------------------------------------------
// TODO : Remove me as everything HUD should be rendered in virtual space.
void ScreenLayoutManager::ConvertFromRenderToVirtualScreenSpace( float* inout_x, float* inout_y ) const
{
	*inout_x *= VIRTUAL_SCREEN_WIDTH * INV_RENDER_SCREEN_WIDTH;
	*inout_y *= VIRTUAL_SCREEN_HEIGHT * INV_RENDER_SCREEN_HEIGHT;
}

//-----------------------------------------------------------------------------------------------------
void ScreenLayoutManager::ConvertFromVirtualToNormalisedScreenSpace( float* inout_x, float* inout_y ) const
{
	*inout_x /= VIRTUAL_SCREEN_WIDTH;
	*inout_y /= VIRTUAL_SCREEN_HEIGHT;
}

//-----------------------------------------------------------------------------------------------------
//  Adjusts a 2D Bounding area to scale with screen and safe areas based on the current 
//  layout state.
//
//  in/out :
//  			fX,fY - in:  the position, in screen space of the asset's pivot i.e. the (center,left), (top,left) 
//  			             or (bottom,right) point of the asset (for example) (in Virtual Space)
//  			       out:  the top left corner of the BB after aligning and scaling with delta screen 
//  			             resolution and safe areas (out Virtual Space).
//  
//  			fSizeX,fSizeY - in:  the size of the BB before any alignment or adjustments are done (in Virtual Space)
//  			              - out: the size of the BB after all appropriate alignments and adjustments are done (out Virtual Space)
//
//  As we want to preserve proportional distance of elements from the screen 
//  edge then the p(x,y) needs to be adjusted based on some proportional
//  relationship to the delta-screen resolution :
//                     p (is-proportional-to) k*(delta)Screen
//  
//  The size of element s(w,h), when not preserving aspect or keeping square
//  should follow the same rule.
//                     s (is-propotional-to) k*(delta)Screen
//  
//  When scaling elements with (delta)ScreenY only, then p is the same as above
//  (to preserve proportional distance from the screen edge), but s(w.h) varies
//  as follows :
//                     s.w (is-propotional-to) k*(delta)ScreenY
//                     s.h (is-propotional-to) k*(delta)ScreenY
//
void ScreenLayoutManager::AdjustToSafeArea( float *fX, float* fY, float* fSizeX /* = NULL*/, float* fSizeY /* = NULL*/,
                                            const EUIDRAWHORIZONTAL eUIDrawHorizontal,        
                                            const EUIDRAWVERTICAL   eUIDrawVertical,
                                            const EUIDRAWHORIZONTAL eUIDrawHorizontalDocking, 
                                            const EUIDRAWVERTICAL   eUIDrawVerticalDocking  ) const
{
	//-----------------------------------------------
	// Get intermediate p(x,y) in proportional space
	// at this point the p is at still the (center,left),
	// (top,left) or (bottom,right) of the asset.
	Vec2 p( *fX/GetVirtualWidth(), *fY/GetVirtualHeight() );
	Vec2 s( 0.0f, 0.0f );
	float assetAspect = 1.0f;

	CRY_ASSERT_MESSAGE( fSizeX && fSizeY || (fSizeX == NULL && fSizeY == NULL), "HUD: Invalid combination of size pointers, probably a mistake in calling code!" );
	const bool bHaveSize = fSizeX && fSizeY && (*fSizeY)!=0.0f;
	if( bHaveSize)
	{
		s = Vec2( (*fSizeX)/GetVirtualWidth(), (*fSizeY)/GetVirtualHeight() );
		assetAspect = (*fSizeX)/(*fSizeY);
	}

	// Do the work and get every thing in proportional space.
	AdjustToSafeAreaProportional( p,s,assetAspect, eUIDrawHorizontal,eUIDrawVertical,eUIDrawHorizontalDocking,eUIDrawVerticalDocking );

	// offset p by the safe-area borders if we are adjusting by safe-areas.
	if( !(m_flags & eSLO_DoNotAdaptToSafeArea) )
	{
		const Vec2 safeAreaBorderP = GetSafeAreaBorderScreenProportion(m_curSafeAreaID);
		p.x += safeAreaBorderP.x; // x border width
		p.y += safeAreaBorderP.y; // y border height
	}

	// Publish the results :
	*fX = p.x * GetVirtualWidth();
	*fY = p.y * GetVirtualHeight();

	if(bHaveSize)
	{
		*fSizeX = s.x * GetVirtualWidth();
		*fSizeY = s.y * GetVirtualHeight();

#if 0 // not accurate.
		if( !(m_flags&eSLO_ScaleMethod_None) )
		{
			// check we've preserved aspect ratio of the asset when expecting to.
			assert( fabs( (*fSizeX)/(*fSizeY) - assetAspect ) <= (EPSILON) );
		}
#endif 
	}
}

void ScreenLayoutManager::AdjustToSafeAreaProportional( Vec2& p, Vec2& s, const float assetAspect,
                                                        const EUIDRAWHORIZONTAL eUIDrawHorizontal,        
                                                        const EUIDRAWVERTICAL   eUIDrawVertical,
                                                        const EUIDRAWHORIZONTAL eUIDrawHorizontalDocking, 
                                                        const EUIDRAWVERTICAL   eUIDrawVerticalDocking  ) const
{
	// Get the canvas aspect ratio
	const float canvasAspect = GetTagetSpaceAspect();

	//-----------------------------------------------
	// Get final asset size(w,h) in proportional space.
	s = GetAssetSizeAsProportionOfScreen( assetAspect, canvasAspect, s );

	//-----------------------------------------------
	// Get
	// at this point the p is at still the (center,left),
	// (top,left) or (bottom,right) of the asset.
	// Adjust canvas for borders
	if( !(m_flags & eSLO_DoNotAdaptToSafeArea) )
	{
		p = ScaleProportionalPositionInToSafeArea( p );
	}

	// Get the position of the pivot p in screen space 
	// when aligned/docked to the screen.
	Vec2 sap(1.0f,1.0f);
	if( !(m_flags & eSLO_DoNotAdaptToSafeArea) )
	{
		sap = GetSafeAreaScreenProportion(m_curSafeAreaID);
	}
	AlignToScreen( &p.x, &p.y, p.x, p.y, sap.x, sap.y, eUIDrawHorizontalDocking, eUIDrawVerticalDocking );

	// If we have a size get the TL point of the asset
	// after applying the align flags to the asset.
	// i.e. in the asset is centered about it's pivot
	// then the TL === p-s/2
	AlignAroundPivot( &p.x, &p.y, p.x, p.y, s.x, s.y, eUIDrawHorizontal, eUIDrawVertical );

}

const float ScreenLayoutManager::GetTagetSpaceAspect( void ) const
{
	// Get the the proportional area of the canvas/screen to be used for
	// rendering
	float safeAreaAspect = 1.0f;
	if( !(m_flags & eSLO_DoNotAdaptToSafeArea) )
	{
		const Vec2 sap = GetSafeAreaScreenProportion(m_curSafeAreaID);
		safeAreaAspect = sap.x/sap.y;
	}

	return RENDER_SCREEN_WIDTH * INV_RENDER_SCREEN_HEIGHT * safeAreaAspect;
}

const Vec2 ScreenLayoutManager::ScaleProportionalPositionInToSafeArea( const Vec2 proportionalP ) const
{
	const Vec2 safeAreaP = GetSafeAreaScreenProportion( m_curSafeAreaID );
	return Vec2( proportionalP.x*safeAreaP.x, proportionalP.y*safeAreaP.y );
}

const Vec2 ScreenLayoutManager::GetAssetSizeAsProportionOfScreen( const float assetAspect, const float canvasAspect, const Vec2& proportionalSize ) const
{
	if(m_flags & eSLO_ScaleMethod_None)
		return proportionalSize;

	Vec2 sa = Vec2(1.0f,1.0f);
	// Adjust canvas for borders
	if( !(m_flags & eSLO_DoNotAdaptToSafeArea) )
	{
		sa = GetSafeAreaScreenProportion( m_curSafeAreaID );
	}

	//-----------------------------------------------
	// Get s(w,h)
	float w = proportionalSize.x;
	float h = proportionalSize.y;

	if(m_flags & eSLO_ScaleMethod_WithY )
	{
		// height proportion is invariant wrt canvas (means y scales with ΔscreenY)
		h *= sa.y;
		const float inv_canvasAspect = canvasAspect==0 ? 0 : (1.0f/canvasAspect);
		w = proportionalSize.y * sa.x * assetAspect * inv_canvasAspect;
	}
	else if(m_flags & eSLO_ScaleMethod_WithX )
	{
		// width proportion is invariant (means x scales with ΔscreenX)
		//h *= (1.0f/assetAspect)*canvasAspect*safeAreaAspect;
		w *= sa.x;
		const float inv_assetAspect = assetAspect!=0.0f ? (1.0f/assetAspect) : 0.0f;
		h = proportionalSize.x * sa.y * inv_assetAspect*canvasAspect;
	}

	return Vec2(w,h);
}

//----------------------------------------------------------------------------------
void ScreenLayoutManager::AlignAroundPivot( float* out_x, float* out_y,
													 const float posX, const float posY,
													 const float dimX, const float dimY,
													 const EUIDRAWHORIZONTAL	eUIDrawHorizontal,        
													 const EUIDRAWVERTICAL    eUIDrawVertical ) const
{
	if(UIDRAWHORIZONTAL_CENTER == eUIDrawHorizontal)
	{
		*out_x = posX - (dimX * 0.5f);
	}
	else if(UIDRAWHORIZONTAL_RIGHT == eUIDrawHorizontal)
	{
		*out_x = posX - dimX;
	}

	if(UIDRAWVERTICAL_CENTER == eUIDrawVertical)
	{
		*out_y = posY - (dimY * 0.5f);
	}
	else if(UIDRAWVERTICAL_BOTTOM == eUIDrawVertical)
	{
		*out_y = posY - dimY;
	}
}

//----------------------------------------------------------------------------------
void ScreenLayoutManager::AlignToScreen( float* out_x, float* out_y,
                                         const float posX, const float posY,
                                         const float screenWidth, const float screenHeight,
                                         const EUIDRAWHORIZONTAL eUIDrawHorizontalDocking, 
                                         const EUIDRAWVERTICAL   eUIDrawVerticalDocking ) const
{
	if(UIDRAWHORIZONTAL_CENTER == eUIDrawHorizontalDocking)
	{
		*out_x = posX + (screenWidth * 0.5f);
	}
	else if(UIDRAWHORIZONTAL_RIGHT == eUIDrawHorizontalDocking)
	{
		*out_x = posX + (screenWidth);
	}

	if(UIDRAWVERTICAL_CENTER == eUIDrawVerticalDocking)
	{
		*out_y += screenHeight * 0.5f;
	}
	else if(UIDRAWVERTICAL_BOTTOM == eUIDrawVerticalDocking)
	{
		*out_y += screenHeight;
	}
}

//----------------------------------------------------------------------------------
void ScreenLayoutManager::GetWorldPositionInScreenSpace( const Vec3& worldSpace, Vec3* pOut_ScreenSpace ) const
{
	int32 v[4] = {0, 0, (int)VIRTUAL_SCREEN_WIDTH, (int)VIRTUAL_SCREEN_HEIGHT};

	Matrix44A mIdent;
	mIdent.SetIdentity();
	mathVec3Project(pOut_ScreenSpace, &worldSpace, v, &m_projectionMatrix, &m_modelViewMatrix, &mIdent);
}

void ScreenLayoutManager::GetWorldPositionInNormalizedScreenSpace( const Vec3& worldSpace, Vec3* pOut_ScreenSpace ) const
{
	int32 v[4] = {0, 0, 1, 1};

	Matrix44A mIdent;
	mIdent.SetIdentity();
	mathVec3Project(pOut_ScreenSpace, &worldSpace, v, &m_projectionMatrix, &m_modelViewMatrix, &mIdent);
}

void ScreenLayoutManager::CacheViewProjectionMatrix()
{
	const CCamera &cam = gEnv->pSystem->GetViewCamera();
	
	// Code borrowed from CD3D9Renderer::SetCamera
	static const Matrix33 matRotX = Matrix33::CreateRotationX(-gf_PI/2);
	m_modelViewMatrix = Matrix44A( matRotX*cam.GetViewMatrix() ).GetTransposed();

	mathMatrixPerspectiveFov(&m_projectionMatrix, cam.GetFov(), cam.GetProjRatio(), cam.GetNearPlane(), cam.GetFarPlane());
}


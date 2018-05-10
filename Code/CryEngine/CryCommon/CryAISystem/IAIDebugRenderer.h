// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   IAIDebugRenderer.h
   $Id$
   Description: Helper functions to draw some interesting debug shapes.

   -------------------------------------------------------------------------
   History:
   - 2006-9-19   : Created (AIDebugDrawHelpers.h) by Mikko Mononen
   - 2009-2-11   : Moved to CryCommon by Evgeny Adamenkov

 *********************************************************************/

#ifndef _I_AI_DEBUG_RENDERER_H_
#define _I_AI_DEBUG_RENDERER_H_

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryRenderer/IRenderAuxGeom.h>

struct IAIDebugRenderer
{
	// <interfuscator:shuffle>
	virtual ~IAIDebugRenderer() {}

	//! Get view camera's VERTICAL field of view in RADIANS from range [0, PI).
	virtual float GetCameraFOV() = 0;

	//! Get view camera position.
	virtual Vec3 GetCameraPos() = 0;

	//! Takes terrain and water elevation (maximum of) or just vPoint.z (if bUseTerrainOrWater == false)
	//! and returns it elevated a little bit (so that the point is visible for debug purposes).
	virtual float GetDebugDrawZ(const Vec3& vPoint, bool bUseTerrainOrWater) = 0;

	//! Get screen dimensions.
	virtual int  GetWidth() = 0;
	virtual int  GetHeight() = 0;

	virtual void DrawAABB(const AABB& aabb, bool bSolid, const ColorB& color, const EBoundingBoxDrawStyle& bbDrawStyle) = 0;
	virtual void DrawAABB(const AABB& aabb, const Matrix34& matWorld, bool bSolid, const ColorB& color, const EBoundingBoxDrawStyle& bbDrawStyle) = 0;

	//! Draws an arrow constrained on XY-plane along the length-vector.
	virtual void DrawArrow(const Vec3& vPos, const Vec3& vLength, float fWidth, const ColorB& color) = 0;

	//! Draw capsule constrained on XY-plane.
	virtual void DrawCapsuleOutline(const Vec3& vPos0, const Vec3& vPos1, float fRadius, const ColorB& color) = 0;

	//! Draw circle constrained on XY-plane.
	virtual void DrawCircleOutline(const Vec3& vPos, float fRadius, const ColorB& color) = 0;

	//! Draw agent circles.
	virtual void DrawCircles(const Vec3& vPos, float fMinRadius, float fMaxRadius, int numRings, const ColorF& insideColor, const ColorF& outsideColor) = 0;

	//! Draw cone.
	virtual void DrawCone(const Vec3& vPos, const Vec3& vDir, float fRadius, float fHeight, const ColorB& color, bool fDrawShaded = true) = 0;

	//! Draw cylinder.
	virtual void DrawCylinder(const Vec3& vPos, const Vec3& vDir, float fRadius, float fHeight, const ColorB& color, bool bDrawShaded = true) = 0;

	//! Debug oriented 2d ellipse.
	virtual void DrawEllipseOutline(const Vec3& vPos, float fRadiusX, float fRadiusY, float fOrientation, const ColorB& color) = 0;

	//! Draw label on the screen (rather than in the game world).
	//! On a 1280x1024 display, in the Editor, with a RollupBar and Console,
	//! NCol is from range 0..90, nRos is from range 0..55 (approximately).
	virtual void Draw2dLabel(int nCol, int nRow, const char* szText, const ColorB& color) = 0;

	virtual void Draw2dLabel(float fX, float fY, float fFontSize, const ColorB& color, bool bCenter, const char* text, ...) PRINTF_PARAMS(7, 8) = 0;

	//! Draw label in the game world (rather than on the screen).
	virtual void Draw3dLabel(Vec3 vPos, float fFontSize, const char* text, ...) PRINTF_PARAMS(4, 5) = 0;
	virtual void Draw3dLabelEx(Vec3 vPos, float fFontSize, const ColorB& color, bool bFixedSize, bool bCenter, bool bDepthTest, bool bFramed, const char* text, ...) PRINTF_PARAMS(9, 10) = 0;

	//! Draw label
	virtual void DrawLabel(Vec3 pos, SDrawTextInfo& ti, const char* text) = 0;

	//! Draw a 2D image on the screen (HUD etc.).
	virtual void Draw2dImage(float fX, float fY, float fWidth, float fHeight, int nTextureID, float fS0 = 0, float fT0 = 0, float fS1 = 1, float fT1 = 1, float fAngle = 0, float fR = 1, float fG = 1, float fB = 1, float fA = 1, float fZ = 1) = 0;

	virtual void DrawLine(const Vec3& v0, const ColorB& colorV0, const Vec3& v1, const ColorB& colorV1, float fThickness = 1.0f) = 0;

	virtual void DrawOBB(const OBB& obb, const Vec3& vPos, bool bSolid, const ColorB& color, const EBoundingBoxDrawStyle bbDrawStyle) = 0;
	virtual void DrawOBB(const OBB& obb, const Matrix34& matWorld, bool bSolid, const ColorB& color, const EBoundingBoxDrawStyle bbDrawStyle) = 0;

	virtual void DrawPolyline(const Vec3* va, uint32 nPoints, bool bClosed, const ColorB& color, float fThickness = 1.0f) = 0;
	virtual void DrawPolyline(const Vec3* va, uint32 nPoints, bool bClosed, const ColorB* colorArray, float fThickness = 1.0f) = 0;

	//! Draw arc constrained on XY-plane with hem.
	//! \param vDir should be normalized.
	virtual void DrawRangeArc(const Vec3& vPos, const Vec3& vDir, float fAngle, float fRadius, float fWidth, const ColorB& colorFill, const ColorB& colorOutline, bool bDrawOutline) = 0;

	//! Draw box constrained on XY-plane with hem.
	//! \param vDir should be normalized.
	virtual void DrawRangeBox(const Vec3& vPos, const Vec3& vDir, float fSizeX, float fSizeY, float fWidth, const ColorB& colorFill, const ColorB& colorOutline, bool bDrawOutline) = 0;

	//! Draw circle constrained on XY-plane with hem.
	virtual void DrawRangeCircle(const Vec3& vPos, float fRadius, float fWidth, const ColorB& colorFill, const ColorB& colorOutline, bool bDrawOutline) = 0;

	//! Draw polygon with a hem constrained on XY-plane.
	virtual void DrawRangePolygon(const Vec3 polygon[], int nVertices, float fWidth, const ColorB& colorFill, const ColorB& colorOutline, bool bDrawOutline) = 0;

	virtual void DrawSphere(const Vec3& vPos, float fRadius, const ColorB& color, bool bDrawShaded = true) = 0;

	virtual void DrawTriangle(const Vec3& v0, const ColorB& colorV0, const Vec3& v1, const ColorB& colorV1, const Vec3& v2, const ColorB& colorV2) = 0;

	virtual void DrawTriangles(const Vec3* va, unsigned int numPoints, const ColorB& color) = 0;

	//! Draws wire frame cone.
	//! \param fFOV is the the angle between the cone side and its axis.
	//! \param vDir should be normalized.
	virtual void DrawWireFOVCone(const Vec3& vPos, const Vec3& vDir, float fRadius, float fFOV, const ColorB& color) = 0;

	//! Draw wire frame sphere (circle per plane).
	virtual void DrawWireSphere(const Vec3& vPos, float fRadius, const ColorB& color) = 0;

	//! Load texture. Can possibly return NULL!.
	virtual ITexture* LoadTexture(const char* sNameOfTexture, uint32 nFlags) = 0;

	//! Project to screen.
	//! \return true if successful.
	//! \note Not guaranteed to work if used outside Renderer.
	virtual bool ProjectToScreen(float fInX, float fInY, float fInZ, float* fOutX, float* fOutY, float* fOutZ) = 0;

	//! Draw text on screen.
	virtual void TextToScreen(float fX, float fY, const char* format, ...) PRINTF_PARAMS(4, 5) = 0;

	//! Draw text on screen in color.
	virtual void TextToScreenColor(int nX, int nY, float fRed, float fGreen, float fBlue, float fAlpha, const char* format, ...) PRINTF_PARAMS(8, 9) = 0;

	//! The following methods affect render flags.

	//! Prepare to draw on the screen, rather than in the game world.
	virtual void Init2DMode() = 0;

	//! Prepare to draw in the game world, rather than on the screen.
	virtual void Init3DMode() = 0;

	//! Turn alpha blending on or off.
	virtual void SetAlphaBlended(bool bOn) = 0;

	//! Turn back face culling on or off.
	virtual void SetBackFaceCulling(bool bOn) = 0;

	//! Turn Z-buffer test on or off.
	//! If the test is off, the primitives (that are in the camera's field of view) are always rendered.
	virtual void SetDepthTest(bool bOn) = 0;

	//! Set whether the shapes should participate in Z-buffering.
	virtual void SetDepthWrite(bool bOn) = 0;

	//! Set whether the shapes (or text) should be rendered on top of everything rendered so far.
	//! (see ai_StatsTarget for an example).
	virtual void SetDrawInFront(bool bOn) = 0;

	//! Methods which can be used to save/restore drawing context
	//! (e.g. flags like alpha blending, back face culling, etc).
	virtual unsigned int PopState() = 0;
	virtual unsigned int PushState() = 0;
	// </interfuscator:shuffle>
};

#endif  // #ifndef _I_AI_DEBUG_RENDERER_H_

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Cry_Color.h>
#include <CryMath/Cry_Geo.h>
#include <CryRenderer/IRenderer.h>

// forward declarations.
struct IDisplayViewport;
struct IRenderer;
struct IRenderAuxGeom;
struct IIconManager;
struct I3DEngine;
class CDisplaySettings;
class CCamera;

enum DisplayFlags
{
	DISPLAY_2D                = BIT(0),
	DISPLAY_HIDENAMES         = BIT(1),
	DISPLAY_BBOX              = BIT(2),
	DISPLAY_TRACKS            = BIT(3),
	DISPLAY_TRACKTICKS        = BIT(4),
	DISPLAY_WORLDSPACEAXIS    = BIT(5), //!< Set if axis must be displayed in world space.
	DISPLAY_LINKS             = BIT(6),
	DISPLAY_DEGRADATED        = BIT(7), //!< Display Objects in degradated quality (When moving/modifying).
	DISPLAY_SELECTION_HELPERS = BIT(8), //!< Display advanced selection helpers.
};

/*!
 *  DisplayContex is a structure passed to BaseObject Display method.
 *	It contains everything the object should know to display itself in a view.
 *	All fields must be filled before passing that structure to Display call.
 */
struct EDITOR_COMMON_API DisplayContext
{
	enum ETextureIconFlags
	{
		TEXICON_ADDITIVE     = BIT(0),
		TEXICON_ALIGN_BOTTOM = BIT(1),
		TEXICON_ALIGN_TOP    = BIT(2),
		TEXICON_ON_TOP       = BIT(3),
	};

	IDisplayViewport* view;
	IRenderer*        renderer;
	IRenderAuxGeom*   pRenderAuxGeom;
	IIconManager*     pIconManager;
	I3DEngine*        engine;
	CCamera*          camera;
	AABB              box; // Bounding box of volume that need to be repainted.
	int               flags;

	//! Ctor.
	DisplayContext();
	// Helper methods.
	void              SetView(IDisplayViewport* pView);
	IDisplayViewport* GetView() const { return view; }
	void              Flush2D();

	void              SetCamera(CCamera* pCamera);
	const CCamera&    GetCamera() const { return (camera) ? *camera : GetISystem()->GetViewCamera(); }

	int               GetWidth() const { return static_cast<int>(m_width); }
	int               GetHeight() const { return static_cast<int>(m_height); }

	void              SetDisplayContext(const SDisplayContextKey &displayContextKey, IRenderer::EViewportType eType = IRenderer::eViewportType_Default);

	//////////////////////////////////////////////////////////////////////////
	// Draw functions
	//////////////////////////////////////////////////////////////////////////
	//! Set current materialc color.
	void   SetColor(float r, float g, float b, float a = 1) { m_color4b = ColorB(int(r * 255.0f), int(g * 255.0f), int(b * 255.0f), int(a * 255.0f)); };
	void   SetColor(const Vec3& color, float a = 1) { m_color4b = ColorB(int(color.x * 255.0f), int(color.y * 255.0f), int(color.z * 255.0f), int(a * 255.0f)); };
	void   SetColor(COLORREF rgb, float a = 1) { m_color4b = ColorB(GetRValue(rgb), GetGValue(rgb), GetBValue(rgb), int(a * 255.0f)); };
	void   SetColor(const ColorB& color) { m_color4b = color; };
	void   SetColor(const ColorF& color) {
		m_color4b = ColorB(int(color.r * 255.0f), int(color.g * 255.0f), int(color.b * 255.0f), int(color.a * 255.0f));
	};
	void   SetAlpha(float a = 1) { m_color4b.a = int(a * 255.0f); };
	ColorB GetColor() const { return m_color4b; }

	void   SetSelectedColor(float fAlpha = 1);
	void   SetFreezeColor();

	//! Get color to draw selectin of object.
	COLORREF GetSelectedColor();
	COLORREF GetFreezeColor();

	// Draw 3D quad.
	void DrawQuad(const Vec3& p1, const Vec3& p2, const Vec3& p3, const Vec3& p4);
	void DrawQuadGradient(const Vec3& p1, const Vec3& p2, const Vec3& p3, const Vec3& p4, ColorB firstColor, ColorB secondColor);
	// Draw 3D Triangle.
	void DrawTri(const Vec3& p1, const Vec3& p2, const Vec3& p3);
	// Draw wireframe box.
	void DrawWireBox(const Vec3& min, const Vec3& max);
	// Draw filled box
	void DrawSolidBox(const Vec3& min, const Vec3& max);
	void DrawPoint(const Vec3& p, int nSize = 1);
	void DrawLine(const Vec3& p1, const Vec3& p2);
	void DrawDottedLine(const Vec3& p1, const Vec3& p2, float interval = 0.1f);
	void DrawLine(const Vec3& p1, const Vec3& p2, const ColorF& col1, const ColorF& col2);
	void DrawLine(const Vec3& p1, const Vec3& p2, COLORREF rgb1, COLORREF rgb2);
	void DrawPolyLine(const Vec3* pnts, int numPoints, bool cycled = true);

	void DrawWireQuad2d(const CPoint& p1, const CPoint& p2, float z, bool drawInFront = false, bool depthTest = true);
	void DrawLine2d(const CPoint& p1, const CPoint& p2, float z);
	void DrawLine2dGradient(const CPoint& p1, const CPoint& p2, float z, ColorB firstColor, ColorB secondColor);
	void DrawWireCircle2d(const CPoint& center, float radius, float z);

	// Draw circle from lines on terrain, position is in world space.
	void DrawTerrainCircle(const Vec3& worldPos, float radius, float height);
	void DrawTerrainCircle(const Vec3& center, float radius, float angle1, float angle2, float height);

	// Draw circle.
	void DrawCircle(const Vec3& pos, float radius, int nUnchangedAxis = 2 /*z axis*/);

	// Draw part of circle from 0 - angle radians in 3D taking current modelview matrix into account.
	void DrawCircle3D(float radius, float angle = gf_PI2, float thickness = 1.0f);

	// Draw part of filled circle from 0 - angle radians in 3D taking current modelview matrix into account.
	void DrawDisc(float radius, float angle = gf_PI2);

	// Draw ring segment from 0 to angle degrees between inner and outer radius
	void DrawRing(float innerRadius, float outerRadius, float angle = gf_PI2);

	// Draw cylinder.
	void DrawCylinder(const Vec3& p1, const Vec3& p2, float radius, float height);

	//! Draw rectangle on top of terrain.
	//! Coordinates are in world space.
	void            DrawTerrainRect(float x1, float y1, float x2, float y2, float height);

	void            DrawTerrainLine(Vec3 worldPos1, Vec3 worldPos2);

	void            DrawWireSphere(const Vec3& pos, float radius);
	void            DrawWireSphere(const Vec3& pos, const Vec3 radius);

	void            PushMatrix(const Matrix34& tm);
	void            PopMatrix();
	const Matrix34& GetMatrix();

	// Draw special 3D objects.
	void DrawBall(const Vec3& pos, float radius);

	//! Draws 3d arrow.
	void DrawArrow(const Vec3& src, const Vec3& trg, float fHeadScale = 1);

	// Draw texture label in 2d view coordinates.
	// w,h in pixels.
	void DrawTextureLabel(const Vec3& pos, int nWidth, int nHeight, int nTexId, int nTexIconFlags = 0, int srcOffsetX = 0, int scrOffsetY = 0,
		float fDistanceScale = 1.0f, float distanceSquared = 0);

	void RenderObject(int objectType, const Vec3& pos, float scale, const SRenderingPassInfo& passInfo);
	void RenderObject(int objectType, const Matrix34& tm, const SRenderingPassInfo& passInfo);

	void DrawTextLabel(const Vec3& pos, float size, const char* text, const bool bCenter = false, int srcOffsetX = 0, int scrOffsetY = 0);
	void Draw2dTextLabel(float x, float y, float size, const char* text, bool bCenter = false);
	void DrawTextOn2DBox(const Vec3& pos, const char* text, float textScale, const ColorF& TextColor, const ColorF& TextBackColor);
	void SetLineWidth(float width);

	//! Is given bbox visible in this display context.
	bool IsVisible(const AABB& bounds);

	//! Gets current render state.
	uint32 GetState() const;
	//! Set a new render state.
	//! @param returns previous render state.
	uint32 SetState(uint32 state);
	//! Set a new render state flags.
	//! @param returns previous render state.
	uint32 SetStateFlag(uint32 state);
	//! Clear specified flags in render state.
	//! @param returns previous render state.
	uint32 ClearStateFlag(uint32 state);

	void   DepthTestOff();
	void   DepthTestOn();

	void   DepthWriteOff();
	void   DepthWriteOn();

	void   CullOff();
	void   CullOn();

	// Enables drawing helper lines in front of usual geometry, adds a small z offset to all drawn lines.
	bool SetDrawInFrontMode(bool bOn);

	// Description:
	//   Changes fill mode.
	// Arguments:
	//    nFillMode is one of the values from EAuxGeomPublicRenderflags_FillMode
	int   SetFillMode(int nFillMode);

	Vec3  ToWorldPos(const Vec3& v) { return ToWS(v); }
	float GetLineWidth() const { return m_thickness; }

	SDisplayContextKey GetDisplayContextKey() const { return m_displayContextKey; }

private:
	// Convert vector to world space.
	Vec3  ToWS(const Vec3& v) { return m_matrixStack[m_currentMatrix].TransformPoint(v); }

	void  InternalDrawLine(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1);

	ColorB   m_color4b;
	uint32   m_renderState;
	float    m_thickness;
	float    m_width;
	float    m_height;

	int      m_currentMatrix;
	//! Matrix stack.
	Matrix34 m_matrixStack[32];
	int      m_previousMatrixIndex[32];

	// Display Helper Sizes
	const int displayHelperSizeLarge = 32;
	const int displayHelperSizeSmall = 4;
	SDisplayContextKey m_displayContextKey;
	IRenderer::EViewportType m_eType = IRenderer::eViewportType_Default;

	struct STextureLabel
	{
		float x, y, z; // 2D position (z in world space).
		float w, h;    // Width height.
		int   nTexId;  // Texture id.
		int   flags;   // ETextureIconFlags
		float color[4];
	};
	std::vector<STextureLabel> m_textureLabels;
};


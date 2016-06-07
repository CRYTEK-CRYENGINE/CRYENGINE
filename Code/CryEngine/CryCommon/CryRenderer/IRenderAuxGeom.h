// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _IRENDERAUXGEOM_H_
#define _IRENDERAUXGEOM_H_

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

struct SAuxGeomRenderFlags;

#include "IRenderer.h"

enum EBoundingBoxDrawStyle
{
	eBBD_Faceted,
	eBBD_Extremes_Color_Encoded
};

//! Auxiliary geometry render interface.
//! Used mostly for debugging, editor purposes, the Auxiliary geometry render
//! interface provide functions to render 2d geometry and also text.
struct IRenderAuxGeom
{
	// <interfuscator:shuffle>
	virtual ~IRenderAuxGeom(){}

	//! Sets render flags.
	virtual void SetRenderFlags(const SAuxGeomRenderFlags& renderFlags) = 0;

	//! Gets render flags.
	virtual SAuxGeomRenderFlags GetRenderFlags() = 0;

	// 2D/3D rendering functions.

	//! Draw a point.
	//! \param v    Vector storing the position of the point.
	//! \param col  Color used to draw the point.
	//! \param size Size of the point drawn.
	virtual void DrawPoint(const Vec3& v, const ColorB& col, uint8 size = 1) = 0;

	//! Draw n points.
	//! \param v         Pointer to a list of vector storing the position of the points.
	//! \param numPoints Number of point we will find starting from the area memory defined by v.
	//! \param col       Color used to draw the points.
	//! \param size	     Size of the points drawn.
	//! ##@{
	virtual void DrawPoints(const Vec3* v, uint32 numPoints, const ColorB& col, uint8 size = 1) = 0;
	virtual void DrawPoints(const Vec3* v, uint32 numPoints, const ColorB* col, uint8 size = 1) = 0;
	//! ##@}

	//! Draw a line.
	//! \param v0         Starting vertex of the line.
	//! \param colV0      Color of the first vertex.
	//! \param v1         Ending vertex of the line.
	//! \param colV1      Color of the second vertex.
	//! \param thickness  Thickness of the line.
	virtual void DrawLine(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, float thickness = 1.0f) = 0;

	//! Draw n lines.
	//! \param v          List of vertexes belonging to the lines we want to draw.
	//! \param numPoints  Number of the points we will find starting from the area memory defined by v.
	//! \param col        Color of the vertexes.
	//! \param thickness  Thickness of the line.
	//! ##@{
	virtual void DrawLines(const Vec3* v, uint32 numPoints, const ColorB& col, float thickness = 1.0f) = 0;
	virtual void DrawLines(const Vec3* v, uint32 numPoints, const ColorB* col, float thickness = 1.0f) = 0;
	//! ##@}

	//! Draw n lines.
	//! \param v          List of vertexes belonging to the lines we want to draw.
	//! \param numPoints  Number of the points we will find starting from the area memory defined by v.
	//! \param ind
	//! \param numIndices
	//! \param col        Color of the vertexes.
	//! \param thickness  Thickness of the line.
	//! ##@{
	virtual void DrawLines(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB& col, float thickness = 1.0f) = 0;
	virtual void DrawLines(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB* col, float thickness = 1.0f) = 0;
	//! ##@}

	//! Draw a polyline.
	//! \param v          List of vertexes belonging to the polyline we want to draw.
	//! \param numPoints  Number of the points we will find starting from the area memory defined by v.
	//! \param closed     If true a line between the last vertex and the first one is drawn.
	//! \param col        Color of the vertexes.
	//! \param thickness  Thickness of the line.
	//! ##@{
	virtual void DrawPolyline(const Vec3* v, uint32 numPoints, bool closed, const ColorB& col, float thickness = 1.0f) = 0;
	virtual void DrawPolyline(const Vec3* v, uint32 numPoints, bool closed, const ColorB* col, float thickness = 1.0f) = 0;
	//! ##@}

	//! Draw a triangle.
	//! \param v0			First vertex of the triangle.
	//! \param colV0		Color of the first vertex of the triangle.
	//! \param v1			Second vertex of the triangle.
	//! \param colV1		Color of the second vertex of the triangle.
	//! \param v2			Third vertex of the triangle.
	//! \param colV2		Color of the third vertex of the triangle.
	virtual void DrawTriangle(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, const Vec3& v2, const ColorB& colV2) = 0;

	//! Draw n triangles.
	//! \param v			List of vertexes belonging to the sequence of triangles we have to draw.
	//! \param numPoints	Number of the points we will find starting from the area memory defined by v.
	//! \param col			Color of the vertexes.
	//! ##@{
	virtual void DrawTriangles(const Vec3* v, uint32 numPoints, const ColorB& col) = 0;
	virtual void DrawTriangles(const Vec3* v, uint32 numPoints, const ColorB* col) = 0;
	//! ##@}

	//! Draw n triangles.
	//! \param v			List of vertexes belonging to the sequence of triangles we have to draw.
	//! \param numPoints	Number of the points we will find starting from the area memory defined by v.
	//! \param ind
	//! \param numIndices
	//! \param col			Color of the vertexes.
	//! ##@{
	virtual void DrawTriangles(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB& col) = 0;
	virtual void DrawTriangles(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB* col) = 0;
	//! ##@}

	//! Draw a Axis-aligned Bounding Boxes (AABB).
	//! ##@{
	virtual void DrawAABB(const AABB& aabb, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) = 0;
	virtual void DrawAABBs(const AABB* aabbs, uint32 aabbCount, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) = 0;
	virtual void DrawAABB(const AABB& aabb, const Matrix34& matWorld, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) = 0;
	//! ##@}

	//! Draw a Oriented Bounding Boxes (AABB).
	//! ##@{
	virtual void DrawOBB(const OBB& obb, const Vec3& pos, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) = 0;
	virtual void DrawOBB(const OBB& obb, const Matrix34& matWorld, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) = 0;
	//! ##@}

	//! Draw a sphere.
	//! \param pos			Center of the sphere.
	//! \param radius		Radius of the sphere.
	//! \param col			Color of the sphere.
	//! \param drawShaded	True if you want to draw the sphere shaded, false otherwise.
	virtual void DrawSphere(const Vec3& pos, float radius, const ColorB& col, bool drawShaded = true) = 0;

	//! Draw a cone.
	//! \param pos		Center of the base of the cone.
	//! \param dir		Direction of the cone.
	//! \param radius		Radius of the base of the cone.
	//! \param height		Height of the cone.
	//! \param col		Color of the cone.
	//! \param drawShaded True if you want to draw the cone shaded, false otherwise.
	virtual void DrawCone(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorB& col, bool drawShaded = true) = 0;

	//! Draw a cylinder.
	//! \param pos		Center of the base of the cylinder.
	//! \param dir		Direction of the cylinder.
	//! \param radius		Radius of the base of the cylinder.
	//! \param height		Height of the cylinder.
	//! \param col		Color of the cylinder.
	//! \param drawShaded True if you want to draw the cylinder shaded, false otherwise.
	virtual void DrawCylinder(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorB& col, bool drawShaded = true) = 0;

	//! Draw a bone.
	virtual void DrawBone(const Vec3& rParent, const Vec3& rBone, ColorB col) = 0;

	//! Draw Text.
	//! ##@{
	virtual void RenderText(Vec3 pos, SDrawTextInfo& ti, const char* format, va_list args) = 0;

	void         Draw2dLabel(float x, float y, float font_size, const ColorF& fColor, bool bCenter, const char* format, va_list args)
	{
		SDrawTextInfo ti;
		ti.xscale = ti.yscale = font_size;
		ti.flags = eDrawText_2D | eDrawText_800x600 | eDrawText_FixedSize | ((bCenter) ? eDrawText_Center : 0);
		ti.color[0] = fColor[0];
		ti.color[1] = fColor[1];
		ti.color[2] = fColor[2];
		ti.color[3] = fColor[3];

		RenderText(Vec3(x, y, 0.5f), ti, format, args);
	}

	void Draw2dLabel(float x, float y, float font_size, const ColorF& fColor, bool bCenter, const char* label_text, ...) PRINTF_PARAMS(7, 8)
	{
		va_list args;
		va_start(args, label_text);
		Draw2dLabel(x, y, font_size, fColor, bCenter, label_text, args);
		va_end(args);
	}

	void Draw2dLabel(float x, float y, float font_size, const float pfColor[4], bool bCenter, const char* label_text, ...) PRINTF_PARAMS(7, 8)
	{
		va_list args;
		va_start(args, label_text);
		Draw2dLabel(x, y, font_size, *(ColorF*)pfColor, bCenter, label_text, args);
		va_end(args);
	}
	//! ##@}

	//! Push a world matrix for the next primitives
	//! \param mat  Contains the matrix that will be used for trasforms of further primitives
	//! \return     Index of previous used matrix index in the matrix buffer, as set internally or by SetMatrixIndex!
	virtual int PushMatrix(const Matrix34& mat) = 0;

	//! Get the world matrix for the next primitives
	//! \return active matrix in the matrix buffer.
	virtual Matrix34* GetMatrix() = 0;

	//! Set world matrix for the next primitives based on ID. This function allows us to do push/pop semantics
	//! \param matID matrix ID. Should be a matrix id returned by PushMatrix
	virtual void SetMatrixIndex(int matID) = 0;

	//! If possible flushes all elements stored on the buffer to rendering system.
	//! Note 1: rendering system may start processing flushed commands immediately or postpone it till Commit() call.
	//! Note 2: worker threads's commands are always postponed till Commit() call.
	virtual void Flush() = 0;
	// </interfuscator:shuffle>

	//! Flushes yet unprocessed elements and notifies rendering system that issuing rendering commands for current frame is done and frame is ready to be drawn.
	//! Thus Commit() guarantees that all previously issued commands will appear on the screen.
	//! Each thread rendering AUX geometry MUST call Commit() at the end of drawing cycle/frame
	//! "frames" indicate how many frames current commands butch must be presented on screen unless there till next butch is ready.
	//! for render and main thread this parameter has no effect
	virtual void Commit(uint frames = 0) = 0;
	// </interfuscator:shuffle>
};

//! Don't change the xxxShift values blindly as they affect the rendering output.
//! For example, 2D primitives have to be rendered after 3D primitives, alpha blended geometry must be rendered after opaque ones, etc.
//! This also applies to the individual flags in EAuxGeomPublicRenderflags_*!.
//! \note Bits 0 - 22 are currently reserved for prim type and per draw call render parameters (point size, etc.).
//! Check RenderAuxGeom.h in ../RenderDll/Common.
enum EAuxGeomPublicRenderflagBitMasks
{

	e_Mode2D3DShift      = 31,
	e_Mode2D3DMask       = 0x1 << e_Mode2D3DShift,

	e_AlphaBlendingShift = 29,
	e_AlphaBlendingMask  = 0x3 << e_AlphaBlendingShift,

	e_DrawInFrontShift   = 28,
	e_DrawInFrontMask    = 0x1 << e_DrawInFrontShift,

	e_FillModeShift      = 26,
	e_FillModeMask       = 0x3 << e_FillModeShift,

	e_CullModeShift      = 24,
	e_CullModeMask       = 0x3 << e_CullModeShift,

	e_DepthWriteShift    = 23,
	e_DepthWriteMask     = 0x1 << e_DepthWriteShift,

	e_DepthTestShift     = 22,
	e_DepthTestMask      = 0x1 << e_DepthTestShift,

	e_PublicParamsMask   = e_Mode2D3DMask | e_AlphaBlendingMask | e_DrawInFrontMask | e_FillModeMask |
	                       e_CullModeMask | e_DepthWriteMask | e_DepthTestMask

};

//! Don't change the xxxShift values blindly as they affect the rendering output.
//! For example, 2D primitives must be rendered after 3D primitives, alpha blended geometry must be rendered after opaque ones, etc.
//! This also applies to the individual flags in EAuxGeomPublicRenderflagBitMasks!.
//! \note Bits 0 - 22 are currently reserved for prim type and per draw call render parameters (point size, etc.). Check RenderAuxGeom.h in ../RenderDll/Common.
//! \see EAuxGeomPublicRenderflagBitMasks
enum EAuxGeomPublicRenderflags_Mode2D3D
{
	e_Mode3D = 0x0 << e_Mode2D3DShift,
	e_Mode2D = 0x1 << e_Mode2D3DShift,
};

//! Don't change the xxxShift values blindly as they affect the rendering output.
//! For example, 2D primitives must be rendered after 3D primitives, alpha blended geometry must be rendered after opaque ones, etc.
//! This also applies to the individual flags in EAuxGeomPublicRenderflagBitMasks!.
//! \note Bits 0 - 22 are currently reserved for prim type and per draw call render parameters (point size, etc.). Check RenderAuxGeom.h in ../RenderDll/Common.
//! \see EAuxGeomPublicRenderflagBitMasks
enum EAuxGeomPublicRenderflags_AlphaBlendMode
{
	e_AlphaNone     = 0x0 << e_AlphaBlendingShift,
	e_AlphaAdditive = 0x1 << e_AlphaBlendingShift,
	e_AlphaBlended  = 0x2 << e_AlphaBlendingShift,
};

//! Don't change the xxxShift values blindly as they affect the rendering output.
//! For example, 2D primitives must be rendered after 3D primitives, alpha blended geometry must be rendered after opaque ones, etc.
//! This also applies to the individual flags in EAuxGeomPublicRenderflagBitMasks!.
//! \note Bits 0 - 22 are currently reserved for prim type and per draw call render parameters (point size, etc.). Check RenderAuxGeom.h in ../RenderDll/Common.
//! \see EAuxGeomPublicRenderflagBitMasks
enum EAuxGeomPublicRenderflags_DrawInFrontMode
{
	e_DrawInFrontOff = 0x0 << e_DrawInFrontShift,
	e_DrawInFrontOn  = 0x1 << e_DrawInFrontShift,
};

//! Don't change the xxxShift values blindly as they affect the rendering output.
//! For example, 2D primitives must be rendered after 3D primitives, alpha blended geometry must be rendered after opaque ones, etc.
//! This also applies to the individual flags in EAuxGeomPublicRenderflagBitMasks!.
//! \note Bits 0 - 22 are currently reserved for prim type and per draw call render parameters (point size, etc.). Check RenderAuxGeom.h in ../RenderDll/Common.
//! \see EAuxGeomPublicRenderflagBitMasks
enum EAuxGeomPublicRenderflags_FillMode
{
	e_FillModeSolid     = 0x0 << e_FillModeShift,
	e_FillModeWireframe = 0x1 << e_FillModeShift,
	e_FillModePoint     = 0x2 << e_FillModeShift,
};

//! Don't change the xxxShift values blindly as they affect the rendering output.
//! For example, 2D primitives must be rendered after 3D primitives, alpha blended geometry must be rendered after opaque ones, etc.
//! This also applies to the individual flags in EAuxGeomPublicRenderflagBitMasks!.
//! \note Bits 0 - 22 are currently reserved for prim type and per draw call render parameters (point size, etc.). Check RenderAuxGeom.h in ../RenderDll/Common.
//! \see EAuxGeomPublicRenderflagBitMasks
enum EAuxGeomPublicRenderflags_CullMode
{
	e_CullModeNone  = 0x0 << e_CullModeShift,
	e_CullModeFront = 0x1 << e_CullModeShift,
	e_CullModeBack  = 0x2 << e_CullModeShift,
};

//! Don't change the xxxShift values blindly as they affect the rendering output.
//! For example, 2D primitives must be rendered after 3D primitives, alpha blended geometry must be rendered after opaque ones, etc.
//! This also applies to the individual flags in EAuxGeomPublicRenderflagBitMasks!.
//! \note Bits 0 - 22 are currently reserved for prim type and per draw call render parameters (point size, etc.). Check RenderAuxGeom.h in ../RenderDll/Common.
//! \see EAuxGeomPublicRenderflagBitMasks
enum EAuxGeomPublicRenderflags_DepthWrite
{
	e_DepthWriteOn  = 0x0 << e_DepthWriteShift,
	e_DepthWriteOff = 0x1 << e_DepthWriteShift,
};

//! Don't change the xxxShift values blindly as they affect the rendering output.
//! For example, 2D primitives must be rendered after 3D primitives, alpha blended geometry must be rendered after opaque ones, etc.
//! This also applies to the individual flags in EAuxGeomPublicRenderflagBitMasks!.
//! \note Bits 0 - 22 are currently reserved for prim type and per draw call render parameters (point size, etc.). Check RenderAuxGeom.h in ../RenderDll/Common.
//! \see EAuxGeomPublicRenderflagBitMasks
enum EAuxGeomPublicRenderflags_DepthTest
{
	e_DepthTestOn  = 0x0 << e_DepthTestShift,
	e_DepthTestOff = 0x1 << e_DepthTestShift,
};

enum EAuxGeomPublicRenderflags_Defaults
{
	//! Default render flags for 3D primitives.
	e_Def3DPublicRenderflags = e_Mode3D | e_AlphaNone | e_DrawInFrontOff | e_FillModeSolid |
	                           e_CullModeBack | e_DepthWriteOn | e_DepthTestOn,

	//! Default render flags for 2D primitives.
	e_Def2DPublicRenderflags = e_Mode2D | e_AlphaNone | e_DrawInFrontOff | e_FillModeSolid |
	                           e_CullModeBack | e_DepthWriteOn | e_DepthTestOn
};

struct SAuxGeomRenderFlags
{
	uint32 m_renderFlags;

	SAuxGeomRenderFlags();
	SAuxGeomRenderFlags(const SAuxGeomRenderFlags& rhs);
	SAuxGeomRenderFlags(uint32 renderFlags);
	SAuxGeomRenderFlags& operator=(const SAuxGeomRenderFlags& rhs);
	SAuxGeomRenderFlags& operator=(uint32 rhs);

	bool                 operator==(const SAuxGeomRenderFlags& rhs) const;
	bool                 operator==(uint32 rhs) const;
	bool                 operator!=(const SAuxGeomRenderFlags& rhs) const;
	bool                 operator!=(uint32 rhs) const;

	//! Gets the flags for 2D/3D rendering mode.
	EAuxGeomPublicRenderflags_Mode2D3D GetMode2D3DFlag() const;

	//! Sets the flags for 2D/3D rendering mode.
	void SetMode2D3DFlag(const EAuxGeomPublicRenderflags_Mode2D3D& state);

	//! Gets the flags for alpha blend mode.
	EAuxGeomPublicRenderflags_AlphaBlendMode GetAlphaBlendMode() const;

	//! Sets the flags for the alpha blend mode.
	void                                      SetAlphaBlendMode(const EAuxGeomPublicRenderflags_AlphaBlendMode& state);

	EAuxGeomPublicRenderflags_DrawInFrontMode GetDrawInFrontMode() const;
	void                                      SetDrawInFrontMode(const EAuxGeomPublicRenderflags_DrawInFrontMode& state);

	//! Gets the flags for the filling mode.
	EAuxGeomPublicRenderflags_FillMode GetFillMode() const;

	//! Sets the flags for the filling mode.
	void SetFillMode(const EAuxGeomPublicRenderflags_FillMode& state);

	//! Gets the flags for the culling mode.
	EAuxGeomPublicRenderflags_CullMode GetCullMode() const;

	//! Sets the flags for the culling mode.
	void                                 SetCullMode(const EAuxGeomPublicRenderflags_CullMode& state);

	EAuxGeomPublicRenderflags_DepthWrite GetDepthWriteFlag() const;
	void                                 SetDepthWriteFlag(const EAuxGeomPublicRenderflags_DepthWrite& state);

	//! Gets the flags for the depth test.
	EAuxGeomPublicRenderflags_DepthTest GetDepthTestFlag() const;

	//! Sets the flags for the depth test.
	void SetDepthTestFlag(const EAuxGeomPublicRenderflags_DepthTest& state);
};

inline
SAuxGeomRenderFlags::SAuxGeomRenderFlags()
	: m_renderFlags(e_Def3DPublicRenderflags)
{
}

inline
SAuxGeomRenderFlags::SAuxGeomRenderFlags(const SAuxGeomRenderFlags& rhs)
	: m_renderFlags(rhs.m_renderFlags)
{
}

inline
SAuxGeomRenderFlags::SAuxGeomRenderFlags(uint32 renderFlags)
	: m_renderFlags(renderFlags)
{
}

inline SAuxGeomRenderFlags&
SAuxGeomRenderFlags::operator=(const SAuxGeomRenderFlags& rhs)
{
	m_renderFlags = rhs.m_renderFlags;
	return(*this);
}

inline SAuxGeomRenderFlags&
SAuxGeomRenderFlags::operator=(uint32 rhs)
{
	m_renderFlags = rhs;
	return(*this);
}

inline bool
SAuxGeomRenderFlags::operator==(const SAuxGeomRenderFlags& rhs) const
{
	return(m_renderFlags == rhs.m_renderFlags);
}

inline bool
SAuxGeomRenderFlags::operator==(uint32 rhs) const
{
	return(m_renderFlags == rhs);
}

inline bool
SAuxGeomRenderFlags::operator!=(const SAuxGeomRenderFlags& rhs) const
{
	return(m_renderFlags != rhs.m_renderFlags);
}

inline bool
SAuxGeomRenderFlags::operator!=(uint32 rhs) const
{
	return(m_renderFlags != rhs);
}

inline EAuxGeomPublicRenderflags_Mode2D3D
SAuxGeomRenderFlags::GetMode2D3DFlag() const
{
	int mode2D3D((int)(m_renderFlags & (uint32)e_Mode2D3DMask));
	switch (mode2D3D)
	{
	case e_Mode2D:
		{
			return(e_Mode2D);
		}
	case e_Mode3D:
	default:
		{
			assert(e_Mode3D == mode2D3D);
			return(e_Mode3D);
		}
	}
}

inline void
SAuxGeomRenderFlags::SetMode2D3DFlag(const EAuxGeomPublicRenderflags_Mode2D3D& state)
{
	m_renderFlags &= ~e_Mode2D3DMask;
	m_renderFlags |= state;
}

inline EAuxGeomPublicRenderflags_AlphaBlendMode
SAuxGeomRenderFlags::GetAlphaBlendMode() const
{
	uint32 alphaBlendMode(m_renderFlags & e_AlphaBlendingMask);
	switch (alphaBlendMode)
	{
	case e_AlphaAdditive:
		{
			return(e_AlphaAdditive);
		}
	case e_AlphaBlended:
		{
			return(e_AlphaBlended);
		}
	case e_AlphaNone:
	default:
		{
			assert(e_AlphaNone == alphaBlendMode);
			return(e_AlphaNone);
		}
	}
}

inline void
SAuxGeomRenderFlags::SetAlphaBlendMode(const EAuxGeomPublicRenderflags_AlphaBlendMode& state)
{
	m_renderFlags &= ~e_AlphaBlendingMask;
	m_renderFlags |= state;
}

inline EAuxGeomPublicRenderflags_DrawInFrontMode
SAuxGeomRenderFlags::GetDrawInFrontMode() const
{
	uint32 drawInFrontMode(m_renderFlags & e_DrawInFrontMask);
	switch (drawInFrontMode)
	{
	case e_DrawInFrontOff:
		{
			return(e_DrawInFrontOff);
		}
	case e_DrawInFrontOn:
	default:
		{
			assert(e_DrawInFrontOn == drawInFrontMode);
			return(e_DrawInFrontOn);
		}
	}
}

inline void
SAuxGeomRenderFlags::SetDrawInFrontMode(const EAuxGeomPublicRenderflags_DrawInFrontMode& state)
{
	m_renderFlags &= ~e_DrawInFrontMask;
	m_renderFlags |= state;
}

inline EAuxGeomPublicRenderflags_FillMode
SAuxGeomRenderFlags::GetFillMode() const
{
	uint32 fillMode(m_renderFlags & e_FillModeMask);
	switch (fillMode)
	{
	case e_FillModePoint:
		{
			return(e_FillModePoint);
		}
	case e_FillModeWireframe:
		{
			return(e_FillModeWireframe);
		}
	case e_FillModeSolid:
	default:
		{
			assert(e_FillModeSolid == fillMode);
			return(e_FillModeSolid);
		}
	}
}

inline void
SAuxGeomRenderFlags::SetFillMode(const EAuxGeomPublicRenderflags_FillMode& state)
{
	m_renderFlags &= ~e_FillModeMask;
	m_renderFlags |= state;
}

inline EAuxGeomPublicRenderflags_CullMode
SAuxGeomRenderFlags::GetCullMode() const
{
	uint32 cullMode(m_renderFlags & e_CullModeMask);
	switch (cullMode)
	{
	case e_CullModeNone:
		{
			return(e_CullModeNone);
		}
	case e_CullModeFront:
		{
			return(e_CullModeFront);
		}
	case e_CullModeBack:
	default:
		{
			assert(e_CullModeBack == cullMode);
			return(e_CullModeBack);
		}
	}
}

inline void
SAuxGeomRenderFlags::SetCullMode(const EAuxGeomPublicRenderflags_CullMode& state)
{
	m_renderFlags &= ~e_CullModeMask;
	m_renderFlags |= state;
}

inline EAuxGeomPublicRenderflags_DepthWrite
SAuxGeomRenderFlags::GetDepthWriteFlag() const
{
	uint32 depthWriteFlag(m_renderFlags & e_DepthWriteMask);
	switch (depthWriteFlag)
	{
	case e_DepthWriteOff:
		{
			return(e_DepthWriteOff);
		}
	case e_DepthWriteOn:
	default:
		{
			assert(e_DepthWriteOn == depthWriteFlag);
			return(e_DepthWriteOn);
		}
	}
}

inline void
SAuxGeomRenderFlags::SetDepthWriteFlag(const EAuxGeomPublicRenderflags_DepthWrite& state)
{
	m_renderFlags &= ~e_DepthWriteMask;
	m_renderFlags |= state;
}

inline EAuxGeomPublicRenderflags_DepthTest
SAuxGeomRenderFlags::GetDepthTestFlag() const
{
	uint32 depthTestFlag(m_renderFlags & e_DepthTestMask);
	switch (depthTestFlag)
	{
	case e_DepthTestOff:
		{
			return(e_DepthTestOff);
		}
	case e_DepthTestOn:
	default:
		{
			assert(e_DepthTestOn == depthTestFlag);
			return(e_DepthTestOn);
		}
	}
}

inline void
SAuxGeomRenderFlags::SetDepthTestFlag(const EAuxGeomPublicRenderflags_DepthTest& state)
{
	m_renderFlags &= ~e_DepthTestMask;
	m_renderFlags |= state;
}

class CRenderAuxGeomRenderFlagsRestore
{
public:
	explicit CRenderAuxGeomRenderFlagsRestore(IRenderAuxGeom* pRender);
	~CRenderAuxGeomRenderFlagsRestore();

private:
	IRenderAuxGeom*     m_pRender;
	SAuxGeomRenderFlags m_backuppedRenderFlags;

	//! Copy-construction is not supported.
	CRenderAuxGeomRenderFlagsRestore(const CRenderAuxGeomRenderFlagsRestore&);
	CRenderAuxGeomRenderFlagsRestore& operator=(const CRenderAuxGeomRenderFlagsRestore&);
};

inline CRenderAuxGeomRenderFlagsRestore::CRenderAuxGeomRenderFlagsRestore(IRenderAuxGeom* pRender)
{
	m_pRender = pRender;
	m_backuppedRenderFlags = m_pRender->GetRenderFlags();
}

inline CRenderAuxGeomRenderFlagsRestore::~CRenderAuxGeomRenderFlagsRestore()
{
	m_pRender->SetRenderFlags(m_backuppedRenderFlags);
}

#endif // #ifndef _IRENDERAUXGEOM_H_

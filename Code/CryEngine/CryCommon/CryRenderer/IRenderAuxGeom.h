// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _IRENDERAUXGEOM_H_
#define _IRENDERAUXGEOM_H_

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

struct SAuxGeomRenderFlags;
struct SRender2DImageDescription;

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
	//! \return Previously set render flags
	virtual SAuxGeomRenderFlags SetRenderFlags(const SAuxGeomRenderFlags& renderFlags) = 0;

	//! Gets render flags.
	virtual SAuxGeomRenderFlags GetRenderFlags() = 0;

	//! Assign current rendering camera to the Aux renderer, used for correctly projecting 3D elements
	virtual const CCamera&      GetCamera() const = 0;

	//! Set current display context for the following auxiliary rendering.
	virtual void SetCurrentDisplayContext(const SDisplayContextKey& displayContextKey) = 0;

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
	//! \param v          List of vertices belonging to the lines we want to draw.
	//! \param numPoints  Number of the points we will find starting from the area memory defined by v.
	//! \param col        Color of the vertices.
	//! \param thickness  Thickness of the line.
	//! ##@{
	virtual void DrawLines(const Vec3* v, uint32 numPoints, const ColorB& col, float thickness = 1.0f) = 0;
	virtual void DrawLines(const Vec3* v, uint32 numPoints, const ColorB* col, float thickness = 1.0f) = 0;
	//! ##@}

	//! Draw n lines.
	//! \param v            List of vertices belonging to the lines we want to draw.
	//! \param packedColor  List of colors belonging to each vertex we want to draw. Packed into one uint32 via ARGB8888. Is of same length as v (i.e., numPoints).
	//! \param numPoints    Number of the points we will find starting from the area memory defined by v.
	//! \param thickness    Thickness of the line.
	//! \param alphaFlag    Enable/disable transparency.
	//! ##@{
	virtual void DrawLines(const Vec3* v, const uint32* packedColorARGB8888, uint32 numPoints, float thickness = 1.0f, bool alphaFlag = true) = 0;
	//! ##@}

	//! Draw n lines.
	//! \param v          List of vertices belonging to the lines we want to draw.
	//! \param numPoints  Number of the points we will find starting from the area memory defined by v.
	//! \param ind
	//! \param numIndices
	//! \param col        Color of the vertices.
	//! \param thickness  Thickness of the line.
	//! ##@{
	virtual void DrawLines(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB& col, float thickness = 1.0f) = 0;
	virtual void DrawLines(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB* col, float thickness = 1.0f) = 0;
	//! ##@}

	//! Draw n line strips.
	//! \param v          List of vertices belonging to the line strip we want to draw.
	//! \param numPoints  Number of the points we will find starting from the area memory defined by v.
	//! \param col        Color of the vertices.
	//! \param thickness  Thickness of the line.
	//! ##@{
	virtual void DrawLineStrip(const Vec3* v, uint32 numPoints, const ColorB* col, float thickness = 1.0f) = 0;
	//! ##@}

	//! Draw a polyline.
	//! \param v          List of vertices belonging to the polyline we want to draw.
	//! \param numPoints  Number of the points we will find starting from the area memory defined by v.
	//! \param closed     If true a line between the last vertex and the first one is drawn.
	//! \param col        Color of the vertices.
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
	//! \param v			List of vertices belonging to the sequence of triangles we have to draw.
	//! \param numPoints	Number of the points we will find starting from the area memory defined by v.
	//! \param col			Color of the vertices.
	//! ##@{
	virtual void DrawTriangles(const Vec3* v, uint32 numPoints, const ColorB& col) = 0;
	virtual void DrawTriangles(const Vec3* v, uint32 numPoints, const ColorB* col) = 0;
	//! ##@}

	//! Draw n triangles.
	//! \param v			List of vertices belonging to the sequence of triangles we have to draw.
	//! \param numPoints	Number of the points we will find starting from the area memory defined by v.
	//! \param ind
	//! \param numIndices
	//! \param col			Color of the vertices.
	//! ##@{
	virtual void DrawTriangles(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB& col) = 0;
	virtual void DrawTriangles(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB* col) = 0;
	//! ##@}

	//! Draw n triangles.
	//! \param inVertices	List of vertices belonging to the sequence of triangles we have to draw.
	//! \param numVertices	Number of the points we will find starting from the area memory defined by v.
	//! \param textured		Whether the triangles are textured or not.
	//! ##@{
	virtual void DrawBuffer(const SAuxVertex* inVertices, uint32 numVertices, bool textured) = 0;
	//! ##@}

	//! passes a pointer to buffer of vertices for drawing n triangles.
	//! \param numVertices	Maximum number of points we want to fill.
	//! \param textured		Whether the triangles are textured or not.
	//! ##@{
	virtual SAuxVertex*         BeginDrawBuffer(uint32 maxVertices, bool textured) = 0;
	//! ##@}

	//! Signals filling the vertex buffer is done. There MUST be ONE corresponding BeginDrawBuffer call before this function call.
	//! \param numVertices Number of vertices that are actually drawn.
	//! ##@{
	virtual void EndDrawBuffer(uint32 numVertices) = 0;
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
	virtual void RenderTextQueued(Vec3 pos, const SDrawTextInfo& ti, const char* text) = 0;

	void DrawQuad(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, const Vec3& v2, const ColorB& colV2,const Vec3& v3, const ColorB& colV3)
	{
		DrawTriangle(v0,colV0,v1,colV1,v2,colV2);
		DrawTriangle(v0,colV0,v2,colV2,v3,colV3);

	}

	void RenderText(Vec3 pos, const SDrawTextInfo& ti, const char* format, va_list args)
	{
		if( format )
		{
			char str[512];

			cry_vsprintf(str, format, args);

			RenderTextQueued(pos, ti, str);
		}
	}

	void Draw2dLabel(float x, float y, float font_size, const ColorF& fColor, bool bCenter, const char* format, va_list args)
	{
		SDrawTextInfo ti;
		ti.scale = Vec2(font_size);
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


	//! Adds a 2D image that should be drawn on the screen to an internal render list.
	//! The function supports placing images in stereo 3D space.
	//! The stereo params are the same that are used for the scene. A value of 0 is handled as a special case and always places the image on the screen plane.
	//! image.stereoDepth - Places image in stereo 3d space. The depth is specified in camera space.
	virtual void PushImage(const SRender2DImageDescription &image) = 0;

	virtual int SetTexture(int idTexture) { return -1; }

	//! Get the world matrix for the next primitives
	//! \return active matrix in the matrix buffer.
	virtual Matrix34* GetMatrix() = 0;

	//! Set world matrix for the next primitives based on ID. This function allows us to do push/pop semantics
	//! \param matID matrix ID. Should be a matrix id returned by PushMatrix
	virtual void SetMatrixIndex(int matID) = 0;

	//! Flushes yet unprocessed elements and notifies rendering system that issuing rendering commands for current frame is done and frame is ready to be drawn.
	//! Thus Submit() guarantees that all previously issued commands will appear on the screen.
	//! Each thread rendering AUX geometry MUST call Submit() at the end of drawing cycle/frame
	//! "frames" indicate how many frames current commands butch must be presented on screen unless there till next butch is ready.
	//! for render and main thread this parameter has no effect
	virtual void Submit(uint frames = 0) = 0;
	// </interfuscator:shuffle>

	//! Create and set the projection matrix to orthographic projection. The orthographics projection will be used until it is set to off.
	virtual void SetOrthographicProjection(bool enable, float l = 0, float r = 1, float b = 0, float t = 1, float n = -1e10, float f = 1e10) = 0;

	static IRenderAuxGeom* GetAux()
	{
		return gEnv->pRenderer ? gEnv->pRenderer->GetIRenderAuxGeom() : gEnv->pAuxGeomRenderer;
	}
};



class IRenderAuxText
{
public:
	struct AColor
	{
		float rgba[4];

		static const float* white()
		{
			static float col[] = { 1, 1, 1, 1 };
			return col;
		}

		AColor(const float* ptr)
		{
			const float* col = ptr ? ptr : white();

			rgba[0] = col[0];
			rgba[1] = col[1];
			rgba[2] = col[2];
			rgba[3] = col[3];
		}

		AColor(const ColorF& cf)
		{
			rgba[0] = cf.r;
			rgba[1] = cf.g;
			rgba[2] = cf.b;
			rgba[3] = cf.a;
		}

		AColor(const Vec3& v)
		{
			rgba[0] = v.x;
			rgba[1] = v.y;
			rgba[2] = v.z;
			rgba[3] = 1;
		}

		AColor(const ColorB& cb)
		{
			rgba[0] = cb.r / 255.0f;
			rgba[1] = cb.g / 255.0f;
			rgba[2] = cb.b / 255.0f;
			rgba[3] = cb.a / 255.0f;
		}

		AColor(float r, float g, float b, float a)
		{
			rgba[0] = r;
			rgba[1] = g;
			rgba[2] = b;
			rgba[3] = a;
		}
	};

	struct ASize
	{
		Vec2 val;

		ASize(float f)          : val(f)    {}
		ASize(float x, float y) : val(x, y) {}
	};

	static void DrawText(Vec3 pos, const SDrawTextInfo& ti, const char* text)
	{
		IRenderAuxGeom::GetAux()->RenderTextQueued(pos, ti, text);
	}

	static void DrawText(Vec3 pos, const ASize& size, const AColor& color, int flags, const char* text)
	{
		if( text && !gEnv->IsDedicated() )
		{
			SDrawTextInfo ti;
			ti.scale = size.val;
			ti.flags = flags;
			ti.color[0] = color.rgba[0];
			ti.color[1] = color.rgba[1];
			ti.color[2] = color.rgba[2];
			ti.color[3] = color.rgba[3];

			DrawText(pos, ti, text);
		}
	}

	static void DrawText(Vec3 pos, const ASize& size, const AColor& color, int flags, const char* format, va_list args)
	{
		if( format )
		{
			char str[512];

			cry_vsprintf(str, format, args);

			DrawText(pos, size, color, flags, str);
		}
	}

	static void DrawTextF(Vec3 pos, const ASize& size, const AColor& color, int flags, const char* label_text, ...) PRINTF_PARAMS(5, 6)
	{
		va_list args;
		va_start(args, label_text);
		DrawText(pos, size, color, flags, label_text, args);
		va_end(args);
	}

	static void DrawLabel(Vec3 pos, float font_size, const char* text)
	{
		DrawText(pos, font_size, AColor::white(), eDrawText_FixedSize, text);
	}

	static void DrawLabelF(Vec3 pos, float font_size, const char* label_text, ...) PRINTF_PARAMS(3, 4)
	{
		va_list args;
		va_start(args, label_text);
		DrawText(pos, font_size, AColor::white(), eDrawText_FixedSize, label_text, args);
		va_end(args);
	}

	static void DrawLabelEx(Vec3 pos, float font_size, const AColor& color, bool bFixedSize, bool bCenter, const char* text)
	{
		DrawText(pos, font_size, color, ((bFixedSize) ? eDrawText_FixedSize : 0) | ((bCenter) ? eDrawText_Center : 0), text);
	}

	static void DrawLabelExF(Vec3 pos, float font_size, const AColor& color, bool bFixedSize, bool bCenter, const char* label_text, ...) PRINTF_PARAMS(6, 7)
	{
		va_list args;
		va_start(args, label_text);
		DrawText(pos, font_size, color, ((bFixedSize) ? eDrawText_FixedSize : 0) | ((bCenter) ? eDrawText_Center : 0), label_text, args);
		va_end(args);
	}

	static void Draw2dLabelEx(float x, float y, float font_size, const AColor& color, EDrawTextFlags flags, const char* label_text, ...) PRINTF_PARAMS(6, 7)
	{
		va_list args;
		va_start(args, label_text);
		DrawText(Vec3(x, y, 0.5f), font_size, color, flags, label_text, args);
		va_end(args);
	}

	static void Draw2dLabel(float x, float y, float font_size, const AColor& color, bool bCenter, const char* label_text, ...) PRINTF_PARAMS(6, 7)
	{
		va_list args;
		va_start(args, label_text);
		DrawText(Vec3(x, y, 0.5f), font_size, color, eDrawText_2D | eDrawText_800x600 | eDrawText_FixedSize | ((bCenter) ? eDrawText_Center : 0), label_text, args);
		va_end(args);
	}


	static void Draw2dText(float posX, float posY, const AColor& color, const char* pStr)
	{
		DrawText(Vec3(posX, posY, 1.f), 1, color, eDrawText_LegacyBehavior, pStr);
	}

	static void PrintToScreen(float x, float y, float size, const char* buf)
	{
		DrawText(Vec3(x, y, 1.f), ASize(size * 0.5f / 8, size * 1.f / 8), AColor::white(), eDrawText_800x600 | eDrawText_2D | eDrawText_LegacyBehavior, buf);
	}

	static void WriteXY(int x, int y, float xscale, float yscale, float r, float g, float b, float a, const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		DrawText(Vec3(float(x), float(y), 1.f), ASize(xscale, yscale), AColor(r, g, b, a), eDrawText_800x600 | eDrawText_2D | eDrawText_LegacyBehavior, format, args);
		va_end(args);
	}

	static void TextToScreenColor(int x, int y, float r, float g, float b, float a, const char* text)
	{
		WriteXY((int)(8 * x), (int)(6 * y), 1, 1, r, g, b, a, "%s", text);
	}

	static void TextToScreen(float x, float y, const char* text)
	{
		TextToScreenColor((int)(8 * x), (int)(6 * y), 1, 1, 1, 1, text);
	}

	static void TextToScreenF(float x, float y, const char* format, ...)
	{
		char buffer[512];
		va_list args;
		va_start(args, format);
		cry_vsprintf(buffer, format, args);
		va_end(args);

		TextToScreenColor((int)(8 * x), (int)(6 * y), 1, 1, 1, 1, buffer);
	}
};

//! Don't change the xxxShift values blindly as they affect the rendering output.
//! For example, 2D primitives have to be rendered after 3D primitives, alpha blended geometry must be rendered after opaque ones, etc.
//! This also applies to the individual flags in EAuxGeomPublicRenderflags_*!.
//! \note Bits 0 - 22 are currently reserved for prim type and per draw call render parameters (point size, etc.).
//! Check RenderAuxGeom.h in ../RenderDll/Common.
enum EAuxGeomPublicRenderflagBitMasks
{

	e_Mode2D3DShift      = 30,
	e_Mode2D3DMask       = 0x3 << e_Mode2D3DShift,

	e_AlphaBlendingShift = 28,
	e_AlphaBlendingMask  = 0x3 << e_AlphaBlendingShift,

	e_DrawInFrontShift   = 27,
	e_DrawInFrontMask    = 0x1 << e_DrawInFrontShift,

	e_FillModeShift      = 25,
	e_FillModeMask       = 0x3 << e_FillModeShift,

	e_CullModeShift      = 23,
	e_CullModeMask       = 0x3 << e_CullModeShift,

	e_DepthWriteShift    = 22,
	e_DepthWriteMask     = 0x1 << e_DepthWriteShift,

	e_DepthTestShift     = 21,
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
	e_Mode3D   = 0x0 << e_Mode2D3DShift, // the coordinates are expected to be in world coordinates
	e_Mode2D   = 0x1 << e_Mode2D3DShift, // the coordinates are expected to be in screen space coordinate (in ([0,width-1], [0,height-1]) ranges)
	e_ModeText = 0x2 << e_Mode2D3DShift, // 
	e_ModeUnit = 0x3 << e_Mode2D3DShift, // the coordinates are expected to be in normalized device coordinates (in ([0,1], [0,1]) ranges)
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
	                           e_CullModeBack | e_DepthWriteOn | e_DepthTestOn,

	//! Default render flags for images.
	e_Def2DImageRenderflags = e_Mode2D | e_DepthTestOff | e_AlphaBlended | e_CullModeNone
};

struct SAuxGeomRenderFlags
{
	uint32 m_renderFlags = e_Def3DPublicRenderflags;

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
{}

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
		return(e_Mode2D);
	case e_Mode3D:
		return(e_Mode3D);
	case e_ModeText:
		return(e_ModeText);
	case e_ModeUnit:
		return(e_ModeUnit);
	default:
		{
			assert(0);
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

// 2D Images added to the rendering frame
struct SRender2DImageDescription
{
	float x = 0;
	float y = 0;
	float z = 0;
	float w = 0;
	float h = 0;
	Vec2  uv[2];           //!< Texture UV coordinates
	float angle = 0;
	float stereoDepth = 0; //!< Places image in stereo 3d space. The depth is specified in camera space.
	ColorB color;

	uint32 textureId = 0;
	uint32 targetId = 0;

	SDisplayContextKey displayContextKey;
	SAuxGeomRenderFlags renderFlags = e_Def2DImageRenderflags;
};

// Helper class to abstract pushing 2d images for rendering
class IRenderAuxImage
{
	struct S2DImage
	{
		float x = 0;
		float y = 0;
		float z = 0;
		float w = 0;
		float h = 0;
		Vec2  uv[2]; // Texture UV coordinates
		float angle = 0;
		float stereoDepth = 0;
		ColorB color;

		uint32 textureId = 0;
		uint32 targetId = 0;
	};

public:

	static void Draw2dImage(float xpos, float ypos, float w, float h, int texture_id, float s0 = 0, float t0 = 0, float s1 = 1, float t1 = 1, float angle = 0,
		float r = 1, float g = 1, float b = 1, float a = 1, float z = 1, float stereoDepth = 0)
	{
		if (gEnv->pRenderer)
		{
			SRender2DImageDescription img;
			img.x = xpos;
			img.y = ypos;
			img.z = z;
			img.w = w;
			img.h = h;
			img.textureId = texture_id;
			//img.targetId = 0;
			img.uv[0].x = s0;
			img.uv[0].y = t0;
			img.uv[1].x = s1;
			img.uv[1].y = t1;
			img.angle = angle;
			img.color = ColorB(ColorF(r, g, b, a));
			img.stereoDepth = stereoDepth;

			IRenderAuxGeom::GetAux()->PushImage(img);
		}
	}
	static void Draw2dImage(float xpos, float ypos, float w, float h, int texture_id, float s0, float t0, float s1,float t1, float angle,const ColorF &col)
	{
		if (gEnv->pRenderer)
		{
			SRender2DImageDescription img;
			img.x = xpos;
			img.y = ypos;
			img.z = 1.f;
			img.w = w;
			img.h = h;
			img.textureId = texture_id;
			//img.targetId = 0;
			img.uv[0].x = s0;
			img.uv[0].y = t0;
			img.uv[1].x = s1;
			img.uv[1].y = t1;
			img.angle = angle;
			img.color = ColorB(col);
			img.stereoDepth = 0.f;

			IRenderAuxGeom::GetAux()->PushImage(img);
		}
	}
	//! Draws a image using the current matrix.
	static void DrawImage(float xpos, float ypos, float w, float h, int texture_id, float s0, float t0, float s1, float t1, float r, float g, float b, float a, bool filtered = true)
	{
		if (gEnv->pRenderer)
		{
			SRender2DImageDescription img;
			img.x = xpos;
			img.y = ypos;
			img.z = 1.f;
			img.w = w;
			img.h = h;
			img.textureId = texture_id;
			//img.targetId = 0;
			img.uv[0].x = s0;
			img.uv[0].y = t0;
			img.uv[1].x = s1;
			img.uv[1].y = t1;
			img.angle = 0.f;
			img.color = ColorB(ColorF(r, g, b, a));
			img.stereoDepth = 0.f;

			IRenderAuxGeom::GetAux()->PushImage(img);
		}
	}
};

#endif // #ifndef _IRENDERAUXGEOM_H_

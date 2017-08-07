// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef NULL_RENDER_AUX_GEOM_H
#define NULL_RENDER_AUX_GEOM_H

#include <CryRenderer/IRenderAuxGeom.h>

class CNULLRenderer;
class ICrySizer;

#if CRY_PLATFORM_WINDOWS && !defined(_RELEASE)
	#define ENABLE_WGL_DEBUG_RENDERER
#endif

#ifdef ENABLE_WGL_DEBUG_RENDERER
	#include <CryCore/Platform/CryWindows.h>
	#include <gl/GL.h>
	#include <gl/GLU.h>
#endif

class CNULLRenderAuxGeom : public IRenderAuxGeom
{
public:
	// interface
	virtual void                SetRenderFlags(const SAuxGeomRenderFlags& renderFlags) {}
	virtual SAuxGeomRenderFlags GetRenderFlags()                                       { return SAuxGeomRenderFlags(); }

	virtual void                Flush()                                                {}
	virtual void                Commit(uint frames = 0)                                {}

	virtual void                DrawPoint(const Vec3& v, const ColorB& col, uint8 size = 1);
	virtual void                DrawPoints(const Vec3* v, uint32 numPoints, const ColorB& col, uint8 size = 1);
	virtual void                DrawPoints(const Vec3* v, uint32 numPoints, const ColorB* col, uint8 size = 1);

	virtual void                DrawLine(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, float thickness = 1.0f);
	virtual void                DrawLines(const Vec3* v, uint32 numPoints, const ColorB& col, float thickness = 1.0f);
	virtual void                DrawLines(const Vec3* v, uint32 numPoints, const ColorB* col, float thickness = 1.0f);
	virtual void                DrawLines(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB& col, float thickness = 1.0f);
	virtual void                DrawLines(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB* col, float thickness = 1.0f);
	virtual void                DrawPolyline(const Vec3* v, uint32 numPoints, bool closed, const ColorB& col, float thickness = 1.0f);
	virtual void                DrawPolyline(const Vec3* v, uint32 numPoints, bool closed, const ColorB* col, float thickness = 1.0f);

	virtual void                DrawTriangle(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, const Vec3& v2, const ColorB& colV2);
	virtual void                DrawTriangles(const Vec3* v, uint32 numPoints, const ColorB& col);
	virtual void                DrawTriangles(const Vec3* v, uint32 numPoints, const ColorB* col);
	virtual void                DrawTriangles(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB& col);
	virtual void                DrawTriangles(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB* col);

	virtual void                DrawBuffer(const SAuxVertex* inVertices, uint32 numVertices, bool textured) {};

	virtual void                DrawAABB(const AABB& aabb, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle)                           {}
	virtual void                DrawAABBs(const AABB* aabb, uint32 aabbCount, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle)        {}
	virtual void                DrawAABB(const AABB& aabb, const Matrix34& matWorld, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle) {}

	virtual void                DrawOBB(const OBB& obb, const Vec3& pos, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle)             {}
	virtual void                DrawOBB(const OBB& obb, const Matrix34& matWorld, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle)    {}

	virtual void                DrawSphere(const Vec3& pos, float radius, const ColorB& col, bool drawShaded = true);
	virtual void                DrawCone(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorB& col, bool drawShaded = true)     {}
	virtual void                DrawCylinder(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorB& col, bool drawShaded = true) {}

	virtual void                DrawBone(const Vec3& rParent, const Vec3& rBone, ColorB col)                                                          {}

	virtual void                RenderTextQueued(Vec3 pos, const SDrawTextInfo& ti, const char* text)                                              {}
	virtual void                DrawBufferRT(const SAuxVertex* data, int numVertices, int blendMode, const Matrix44* matViewProj, int texID)                  {}

	virtual int32               PushMatrix(const Matrix34& mat)                                                                                             { return -1; }
	virtual Matrix34*           GetMatrix()                                                                                                           { return nullptr; }
	virtual void                SetMatrixIndex(int32 matID)                                                                                           {}

public:
	static CNULLRenderAuxGeom* Create()
	{
		if (s_pThis == NULL)
			s_pThis = new CNULLRenderAuxGeom();
		return s_pThis;
	}

public:
	~CNULLRenderAuxGeom();

	void BeginFrame();
	void EndFrame();

private:
	CNULLRenderAuxGeom();

	static CNULLRenderAuxGeom* s_pThis;

#ifdef ENABLE_WGL_DEBUG_RENDERER
	static void DebugRendererShowWindow(IConsoleCmdArgs* args)
	{
		if (!s_pThis || !s_pThis->IsGLLoaded())
			return;

		ShowWindow(s_pThis->m_hwnd, s_hidden ? SW_SHOWNA : SW_HIDE);
		s_hidden = !s_hidden;
	}

	static void DebugRendererSetEyePos(IConsoleCmdArgs* args)
	{
		if (!s_pThis || !s_pThis->IsGLLoaded())
			return;

		if (args->GetArgCount() != 4)
			return;

		Vec3 pos;
		pos.x = (float)atof(args->GetArg(1));
		pos.y = (float)atof(args->GetArg(2));
		pos.z = (float)atof(args->GetArg(3));

		s_pThis->m_eye = pos;
	}

	static void DebugRendererUpdateSystemView(IConsoleCmdArgs* args)
	{
		if (!s_pThis || !s_pThis->IsGLLoaded())
			return;

		if (args->GetArgCount() != 2)
			return;

		s_pThis->m_updateSystemView = ((int)atoi(args->GetArg(1)) != 0);
	}

	HWND  m_hwnd;
	HDC   m_hdc;
	HGLRC m_glrc;

	bool  m_glLoaded;
	bool IsGLLoaded() const
	{
		if (!m_glLoaded)
		{
			CryLogAlways("Couldn't initialize OpenGL library, the debug renderer is unavailable");
		}
		return m_glLoaded;
	}

	GLUquadricObj* m_qobj;

	// Window Procedure
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_CREATE:
			return 0;

		case WM_CLOSE:
			PostQuitMessage(0);
			return 0;

		case WM_DESTROY:
			return 0;

		case WM_ACTIVATE:
			if (LOWORD(wParam) != WA_INACTIVE)
				s_active = true;
			else
				s_active = false;
			return 0;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}

	bool EnableOpenGL();
	void DisableOpenGL();

	struct SPoint // GL_C3F_V3F
	{
		GLfloat color[3];  // rgb
		GLfloat vertex[3]; // xyz

		SPoint() {}

		SPoint(const Vec3& v, const ColorB& c)
		{
			color[0] = c.r / 255.f;
			color[1] = c.g / 255.f;
			color[2] = c.b / 255.f;
			vertex[0] = v.x;
			vertex[1] = v.y;
			vertex[2] = v.z;
		}

		const SPoint& operator=(const SPoint& rhs)
		{
			memcpy(color, rhs.color, sizeof(color));
			memcpy(vertex, rhs.vertex, sizeof(vertex));
			return *this;
		}
	};

	std::vector<SPoint> m_points;

	struct SLine
	{
		SPoint points[2];

		SLine() {}
		SLine(const SPoint& p0, const SPoint& p1)
		{
			points[0] = p0;
			points[1] = p1;
		}
	};

	std::vector<SLine> m_lines;

	struct SPolyLine
	{
		std::vector<SPoint> points;
	};

	std::vector<SPolyLine> m_polyLines;

	struct STriangle
	{
		SPoint points[3];

		STriangle() {}
		STriangle(const SPoint& p0, const SPoint& p1, const SPoint& p2)
		{
			points[0] = p0;
			points[1] = p1;
			points[2] = p2;
		}
	};

	std::vector<STriangle> m_triangles;

	struct SSphere
	{
		SPoint p;
		float  r;

		SSphere() {}
		SSphere(const SPoint& p_, float r_) : p(p_), r(r_) {}
	};

	std::vector<SSphere> m_spheres;

	Vec3                 m_eye, m_dir, m_up;
	bool                 m_updateSystemView;

	static bool          s_active;
	static bool          s_hidden;
#endif
};

#endif // NULL_RENDER_AUX_GEOM_H

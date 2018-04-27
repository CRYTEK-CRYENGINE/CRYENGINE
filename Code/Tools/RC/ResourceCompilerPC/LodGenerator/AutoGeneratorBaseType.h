// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace LODGenerator
{
	struct SAutoUVEdge;
	struct SAutoUVVertex
	{
		Vec3 pos;
		Vec2 projected;
		Vec3 smoothedNormal;
	};

	struct SAutoUVVertexNormal
	{
		SAutoUVVertexNormal() : normal(ZERO), weighting(0.0f) {}
		Vec3 normal;
		float weighting;
	};

	struct SAutoUVFace
	{
		SAutoUVVertex *v[3];
		SAutoUVEdge* edges[3];
		float a[3];
		bool bDone;
	};

	struct SAutoUVEdge
	{
		SAutoUVVertex *v[2];
		SAutoUVFace *faces[2];
	};

	struct SAutoUVStackEntry
	{
		SAutoUVEdge *edge;
		Vec2 projected[2];
	};

	struct SAutoUVTri
	{
	public:
		SAutoUVTri(Vec2 &a, Vec2 &b, Vec2 &c, Vec3 &ap, Vec3 &bp, Vec3 &cp, SAutoUVVertex *va, SAutoUVVertex *vb, SAutoUVVertex *vc);

	public:
		Vec2 v[3];
		Vec3 pos[3];
		Vec3 tangent;
		Vec3 bitangent;
		Vec3 normal;
		SAutoUVVertex *orig[3];
	};

	struct SAutoUVFinalVertex
	{
		Vec3 pos;
		Vec2 uv;
		Vec3 tangent;
		Vec3 bitangent;
		Vec3 normal;
		SAutoUVVertex *orig;
	};

	struct SAutoUVSquare
	{
	public:
		SAutoUVSquare(int _w, int _h, float _mx, float _my, int polyStart, int polyEnd);
		bool operator <(const SAutoUVSquare &rhs) const;
		void Rotate();
		void RotateTris(std::vector<SAutoUVTri> &tris);

	public:
		int w,h;
		int x,y;
		int s,e;
		float mx,my;
	};

	class SAutoUVExpandingRasteriser
	{
	public:
		SAutoUVExpandingRasteriser(float resolution);
		~SAutoUVExpandingRasteriser();

		void ExpandToFitVertices(int bbMin[2], int bbMax[2]);		
		bool RayTest(Vec2 &a, Vec2 &b, Vec2 c, int x, int y);
		bool RasteriseTriangle(Vec2 &a, Vec2 &b, Vec2 &c, bool bWrite);		

	public:
		int m_min[2];
		int m_max[2];
		char *m_pixels;
		float m_resolution;
		bool m_bInitialised;
	};
}
// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RenderAuxGeom.h"

#if defined(ENABLE_RENDER_AUX_GEOM)

static inline uint32 PackColor(const ColorB& col)
{
	return(((uint8) (col.a) << 24) +
	       ((uint8) (col.r) << 16) +
	       ((uint8) (col.g) << 8) +
	       ((uint8) (col.b)));
}

static inline ColorB
ScaleColor(ColorB color, float scale)
{
	Vec3 col((float) color.r, (float) color.g, (float) color.b);
	col *= scale;
	return(ColorB((uint8) col.x, (uint8)col.y, (uint8) col.z, color.a));
}

static inline uint32 AlphaFlags(const ColorB& colV)
{
	if (colV.a > 0 && colV.a < 0xFF)
		return e_AlphaBlended;
	return 0;
}

CAuxGeomCB::CAuxGeomCB(IRenderAuxGeomImpl* pRenderAuxGeom)
	: m_pRenderAuxGeom(pRenderAuxGeom)
	, m_lastFlushPos(0)
{
	CRY_ASSERT(m_pRenderAuxGeom != nullptr);
	m_cbCurrent = AddCBuffer();
}

CAuxGeomCB::~CAuxGeomCB()
{
	for (auto const pData : m_cbData)
	{
		delete pData;
	}
}

void CAuxGeomCB::SetRenderFlags(const SAuxGeomRenderFlags& renderFlags)
{
	// make sure caller only tries to set public bits
	CRY_ASSERT(0 == (renderFlags.m_renderFlags & ~e_PublicParamsMask));
	m_cbCurrent->m_curRenderFlags = renderFlags;
}

SAuxGeomRenderFlags CAuxGeomCB::GetRenderFlags()
{
	return(m_cbCurrent->m_curRenderFlags);
}

void CAuxGeomCB::DrawPoint(const Vec3& v, const ColorB& col, uint8 size)
{
	CRY_ASSERT(size > 0);
	SAuxVertex* pVertex(nullptr);
	AddPrimitive(pVertex, 1, CreatePointRenderFlags(size));

	pVertex->xyz = v;
	pVertex->color.dcolor = PackColor(col);
}

void CAuxGeomCB::DrawPoints(const Vec3* v, uint32 numPoints, const ColorB& col, uint8 size)
{
	CRY_ASSERT(size > 0);
	SAuxVertex* pVertices(nullptr);
	AddPrimitive(pVertices, numPoints, CreatePointRenderFlags(size));

	uint32 color(PackColor(col));
	for (uint32 i(0); i < numPoints; ++i)
	{
		pVertices[i].xyz = v[i];
		pVertices[i].color.dcolor = color;
	}
}

void CAuxGeomCB::DrawPoints(const Vec3* v, uint32 numPoints, const ColorB* col, uint8 size)
{
	CRY_ASSERT(size > 0);
	SAuxVertex* pVertices(nullptr);
	AddPrimitive(pVertices, numPoints, CreatePointRenderFlags(size));

	for (uint32 i(0); i < numPoints; ++i)
	{
		pVertices[i].xyz = v[i];
		pVertices[i].color.dcolor = PackColor(col[i]);
	}
}

void CAuxGeomCB::DrawLine(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, float thickness)
{
	if (thickness <= 1.0f)
	{
		SAuxVertex* pVertices(nullptr);
		AddPrimitive(pVertices, 2, CreateLineRenderFlags(false) | AlphaFlags(colV0) | AlphaFlags(colV1));

		pVertices[0].xyz = v0;
		pVertices[0].color.dcolor = PackColor(colV0);
		pVertices[1].xyz = v1;
		pVertices[1].color.dcolor = PackColor(colV1);
	}
	else
	{
		DrawThickLine(v0, colV0, v1, colV1, thickness);
	}
}

void CAuxGeomCB::DrawLines(const Vec3* v, uint32 numPoints, const ColorB& col, float thickness)
{
	CRY_ASSERT((numPoints >= 2) && (0 == (numPoints & 1)));

	if (thickness <= 1.0f)
	{
		SAuxVertex* pVertices(nullptr);
		AddPrimitive(pVertices, numPoints, CreateLineRenderFlags(false) | AlphaFlags(col));

		uint32 color(PackColor(col));
		for (uint32 i(0); i < numPoints; ++i)
		{
			pVertices[i].xyz = v[i];
			pVertices[i].color.dcolor = color;
		}
	}
	else
	{
		for (uint32 i(0); i < numPoints; i += 2)
		{
			DrawThickLine(v[i], col, v[i + 1], col, thickness);
		}
	}
}

void CAuxGeomCB::DrawLines(const Vec3* v, uint32 numPoints, const ColorB* col, float thickness)
{
	CRY_ASSERT((numPoints >= 2) && (0 == (numPoints & 1)));

	if (thickness <= 1.0f)
	{
		SAuxVertex* pVertices(nullptr);
		AddPrimitive(pVertices, numPoints, CreateLineRenderFlags(false));

		for (uint32 i(0); i < numPoints; ++i)
		{
			pVertices[i].xyz = v[i];
			pVertices[i].color.dcolor = PackColor(col[i]);
		}
	}
	else
	{
		for (uint32 i(0); i < numPoints; i += 2)
		{
			DrawThickLine(v[i], col[i], v[i + 1], col[i + 1], thickness);
		}
	}
}

void CAuxGeomCB::DrawLines(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB& col, float thickness)
{
	CRY_ASSERT(numPoints >= 2);
	CRY_ASSERT((numIndices >= 2) && (0 == (numIndices & 1)));

	if (thickness <= 1.0f)
	{
		SAuxVertex* pVertices(nullptr);
		vtx_idx* pIndices(nullptr);
		AddIndexedPrimitive(pVertices, numPoints, pIndices, numIndices, CreateLineRenderFlags(true) | AlphaFlags(col));

		uint32 color(PackColor(col));
		for (uint32 i(0); i < numPoints; ++i)
		{
			pVertices[i].xyz = v[i];
			pVertices[i].color.dcolor = color;
		}

		memcpy(pIndices, ind, sizeof(pIndices[0]) * numIndices);
	}
	else
	{
		for (uint32 i(0); i < numIndices; i += 2)
		{
			DrawThickLine(v[ind[i]], col, v[ind[i + 1]], col, thickness);
		}
	}
}

void CAuxGeomCB::DrawLines(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB* col, float thickness)
{
	CRY_ASSERT(numPoints >= 2);
	CRY_ASSERT((numIndices >= 2) && (0 == (numIndices & 1)));

	if (thickness <= 1.0f)
	{
		SAuxVertex* pVertices(nullptr);
		vtx_idx* pIndices(nullptr);
		AddIndexedPrimitive(pVertices, numPoints, pIndices, numIndices, CreateLineRenderFlags(true));

		for (uint32 i(0); i < numPoints; ++i)
		{
			pVertices[i].xyz = v[i];
			pVertices[i].color.dcolor = PackColor(col[i]);
		}

		memcpy(pIndices, ind, sizeof(pIndices[0]) * numIndices);
	}
	else
	{
		for (uint32 i(0); i < numIndices; i += 2)
		{
			DrawThickLine(v[ind[i]], col[ind[i]], v[ind[i + 1]], col[ind[i + 1]], thickness);
		}
	}
}

void CAuxGeomCB::DrawPolyline(const Vec3* v, uint32 numPoints, bool closed, const ColorB& col, float thickness)
{
	CRY_ASSERT(numPoints >= 2);
	CRY_ASSERT(!closed || numPoints >= 3);   // if "closed" then we need at least three vertices

	if (thickness <= 1.0f)
	{
		SAuxVertex* pVertices(nullptr);
		vtx_idx* pIndices(nullptr);
		AddIndexedPrimitive(pVertices, numPoints, pIndices, (false != closed) ? 2 * numPoints : 2 * (numPoints - 1), CreateLineRenderFlags(true) | AlphaFlags(col));

		uint32 color(PackColor(col));
		for (uint32 i(0); i < numPoints; ++i)
		{
			pVertices[i].xyz = v[i];
			pVertices[i].color.dcolor = color;
		}

		for (uint32 i(0); i < numPoints - 1; ++i)
		{
			pIndices[0] = i;
			pIndices[1] = i + 1;
			pIndices += 2;
		}
		if (false != closed)
		{
			pIndices[0] = numPoints - 1;
			pIndices[1] = 0;
		}
	}
	else
	{
		for (uint32 i(0); i < numPoints - 1; ++i)
		{
			DrawThickLine(v[i], col, v[i + 1], col, thickness);
		}
		if (false != closed)
		{
			DrawThickLine(v[numPoints - 1], col, v[0], col, thickness);
		}
	}
}

void CAuxGeomCB::DrawPolyline(const Vec3* v, uint32 numPoints, bool closed, const ColorB* col, float thickness)
{
	CRY_ASSERT(numPoints >= 2);
	CRY_ASSERT(!closed || numPoints >= 3);   // if "closed" then we need at least three vertices

	if (thickness <= 1.0f)
	{
		SAuxVertex* pVertices(nullptr);
		vtx_idx* pIndices(nullptr);
		AddIndexedPrimitive(pVertices, numPoints, pIndices, (false != closed) ? 2 * numPoints : 2 * (numPoints - 1), CreateLineRenderFlags(true));

		for (uint32 i(0); i < numPoints; ++i)
		{
			pVertices[i].xyz = v[i];
			pVertices[i].color.dcolor = PackColor(col[i]);
		}

		for (uint32 i(0); i < numPoints - 1; ++i)
		{
			pIndices[0] = i;
			pIndices[1] = i + 1;
			pIndices += 2;
		}
		if (false != closed)
		{
			pIndices[0] = numPoints - 1;
			pIndices[1] = 0;
		}
	}
	else
	{
		for (uint32 i(0); i < numPoints - 1; ++i)
		{
			DrawThickLine(v[i], col[i], v[i + 1], col[i + 1], thickness);
		}
		if (false != closed)
		{
			DrawThickLine(v[numPoints - 1], col[numPoints - 1], v[0], col[0], thickness);
		}
	}
}

void CAuxGeomCB::DrawThickLine(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, float thickness)
{
	CRY_ASSERT(thickness > 0.0f);
	CRY_ASSERT(gRenDev != nullptr);

	// allocate space for two triangles
	SAuxVertex* pVertices(nullptr);
	AddPrimitive(pVertices, 4, CreateLineRenderFlags(false) | e_LineListParam_ProcessThickLines);

	// Encode paramaters for thick line in vertex memory.
	// The actual "aux render" implementation has to process these before feeding it to the GPU.
	// This was done as implementation needs access to several render specific informations
	// (see D3DRenderAuxGeom.cpp for an implementation).
	pVertices[0].xyz = v0;
	pVertices[0].color.dcolor = PackColor(colV0);
	pVertices[0].st = Vec2(thickness, 0.0f);
	pVertices[1].xyz = v1;
	pVertices[1].color.dcolor = PackColor(colV1);
	pVertices[1].st = Vec2(thickness, 0.0f);
}

void CAuxGeomCB::DrawTriangle(const Vec3& v0, const ColorB& colV0, const Vec3& v1, const ColorB& colV1, const Vec3& v2, const ColorB& colV2)
{
	SAuxVertex* pVertices(nullptr);
	AddPrimitive(pVertices, 3, CreateTriangleRenderFlags(false));

	pVertices[0].xyz = v0;
	pVertices[0].color.dcolor = PackColor(colV0);

	pVertices[1].xyz = v1;
	pVertices[1].color.dcolor = PackColor(colV1);

	pVertices[2].xyz = v2;
	pVertices[2].color.dcolor = PackColor(colV2);
}

void CAuxGeomCB::DrawTriangles(const Vec3* v, uint32 numPoints, const ColorB& col)
{
	CRY_ASSERT((numPoints >= 3) && (0 == (numPoints % 3)));

	SAuxVertex* pVertices(nullptr);
	AddPrimitive(pVertices, numPoints, CreateTriangleRenderFlags(false));

	uint32 color(PackColor(col));
	for (uint32 i(0); i < numPoints; ++i)
	{
		pVertices[i].xyz = v[i];
		pVertices[i].color.dcolor = color;
	}
}

void CAuxGeomCB::DrawTriangles(const Vec3* v, uint32 numPoints, const ColorB* col)
{
	CRY_ASSERT((numPoints >= 3) && (0 == (numPoints % 3)));

	SAuxVertex* pVertices(nullptr);
	AddPrimitive(pVertices, numPoints, CreateTriangleRenderFlags(false));

	for (uint32 i(0); i < numPoints; ++i)
	{
		pVertices[i].xyz = v[i];
		pVertices[i].color.dcolor = PackColor(col[i]);
	}
}

void CAuxGeomCB::DrawTriangles(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB& col)
{
	CRY_ASSERT(numPoints >= 3);
	CRY_ASSERT((numIndices >= 3) && (0 == (numIndices % 3)));

	SAuxVertex* pVertices(nullptr);
	vtx_idx* pIndices(nullptr);
	AddIndexedPrimitive(pVertices, numPoints, pIndices, numIndices, CreateTriangleRenderFlags(true));

	uint32 color(PackColor(col));
	for (uint32 i(0); i < numPoints; ++i)
	{
		pVertices[i].xyz = v[i];
		pVertices[i].color.dcolor = color;
	}

	memcpy(pIndices, ind, sizeof(pIndices[0]) * numIndices);
}

void CAuxGeomCB::DrawTriangles(const Vec3* v, uint32 numPoints, const vtx_idx* ind, uint32 numIndices, const ColorB* col)
{
	CRY_ASSERT(numPoints >= 3);
	CRY_ASSERT((numIndices >= 3) && (0 == (numIndices % 3)));

	SAuxVertex* pVertices(nullptr);
	vtx_idx* pIndices(nullptr);
	AddIndexedPrimitive(pVertices, numPoints, pIndices, numIndices, CreateTriangleRenderFlags(true));

	for (uint32 i(0); i < numPoints; ++i)
	{
		pVertices[i].xyz = v[i];
		pVertices[i].color.dcolor = PackColor(col[i]);
	}

	memcpy(pIndices, ind, sizeof(pIndices[0]) * numIndices);
}

void CAuxGeomCB::DrawBuffer(const SAuxVertex* inVertices, uint32 numVertices, bool textured)
{
	CRY_ASSERT((numVertices >= 3) && (0 == (numVertices % 3)));

	SAuxVertex* bufVertices(nullptr);
	AddPrimitive(bufVertices, numVertices, CreateTriangleRenderFlags(false, textured));

	memcpy(bufVertices, inVertices, numVertices * sizeof(SAuxVertex));
}

void CAuxGeomCB::DrawAABB(const AABB& aabb, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle)
{
	SAuxVertex* pVertices(nullptr);
	vtx_idx* pIndices(nullptr);

	if (eBBD_Extremes_Color_Encoded == bbDrawStyle)
	{
		if (false == bSolid)
		{
			AddIndexedPrimitive(pVertices, 8, pIndices, 24, CreateLineRenderFlags(true) | AlphaFlags(col));

			pIndices[0] = 0;
			pIndices[1] = 1;
			pIndices[2] = 1;
			pIndices[3] = 2;
			pIndices[4] = 2;
			pIndices[5] = 3;
			pIndices[6] = 3;
			pIndices[7] = 0;

			pIndices[8] = 4;
			pIndices[9] = 5;
			pIndices[10] = 5;
			pIndices[11] = 6;
			pIndices[12] = 6;
			pIndices[13] = 7;
			pIndices[14] = 7;
			pIndices[15] = 4;

			pIndices[16] = 0;
			pIndices[17] = 4;
			pIndices[18] = 1;
			pIndices[19] = 5;
			pIndices[20] = 2;
			pIndices[21] = 6;
			pIndices[22] = 3;
			pIndices[23] = 7;
		}
		else
		{
			AddIndexedPrimitive(pVertices, 8, pIndices, 36, CreateTriangleRenderFlags(true));

			pIndices[0] = 0;
			pIndices[1] = 1;
			pIndices[2] = 2;
			pIndices[3] = 0;
			pIndices[4] = 2;
			pIndices[5] = 3;

			pIndices[6] = 7;
			pIndices[7] = 6;
			pIndices[8] = 5;
			pIndices[9] = 7;
			pIndices[10] = 5;
			pIndices[11] = 4;

			pIndices[12] = 3;
			pIndices[13] = 2;
			pIndices[14] = 6;
			pIndices[15] = 3;
			pIndices[16] = 6;
			pIndices[17] = 7;

			pIndices[18] = 4;
			pIndices[19] = 5;
			pIndices[20] = 1;
			pIndices[21] = 4;
			pIndices[22] = 1;
			pIndices[23] = 0;

			pIndices[24] = 1;
			pIndices[25] = 5;
			pIndices[26] = 6;
			pIndices[27] = 1;
			pIndices[28] = 6;
			pIndices[29] = 2;

			pIndices[30] = 4;
			pIndices[31] = 0;
			pIndices[32] = 3;
			pIndices[33] = 4;
			pIndices[34] = 3;
			pIndices[35] = 7;
		}

		uint32 color(PackColor(col));
		uint32 colorMin(PackColor(ColorB(15, 15, 15, col.a)));
		uint32 colorMax(PackColor(ColorB(255, 255, 255, col.a)));

		pVertices[0].xyz = Vec3(aabb.min.x, aabb.min.y, aabb.min.z);
		pVertices[0].color.dcolor = colorMin;
		pVertices[1].xyz = Vec3(aabb.min.x, aabb.max.y, aabb.min.z);
		pVertices[1].color.dcolor = color;
		pVertices[2].xyz = Vec3(aabb.max.x, aabb.max.y, aabb.min.z);
		pVertices[2].color.dcolor = color;
		pVertices[3].xyz = Vec3(aabb.max.x, aabb.min.y, aabb.min.z);
		pVertices[3].color.dcolor = color;
		pVertices[4].xyz = Vec3(aabb.min.x, aabb.min.y, aabb.max.z);
		pVertices[4].color.dcolor = color;
		pVertices[5].xyz = Vec3(aabb.min.x, aabb.max.y, aabb.max.z);
		pVertices[5].color.dcolor = color;
		pVertices[6].xyz = Vec3(aabb.max.x, aabb.max.y, aabb.max.z);
		pVertices[6].color.dcolor = colorMax;
		pVertices[7].xyz = Vec3(aabb.max.x, aabb.min.y, aabb.max.z);
		pVertices[7].color.dcolor = color;
	}
	else
	{
		if (false == bSolid)
		{
			AddIndexedPrimitive(pVertices, 8, pIndices, 24, CreateLineRenderFlags(true) | AlphaFlags(col));

			uint32 color(PackColor(col));

			pVertices[0].xyz = Vec3(aabb.min.x, aabb.min.y, aabb.min.z);
			pVertices[0].color.dcolor = color;
			pVertices[1].xyz = Vec3(aabb.min.x, aabb.max.y, aabb.min.z);
			pVertices[1].color.dcolor = color;
			pVertices[2].xyz = Vec3(aabb.max.x, aabb.max.y, aabb.min.z);
			pVertices[2].color.dcolor = color;
			pVertices[3].xyz = Vec3(aabb.max.x, aabb.min.y, aabb.min.z);
			pVertices[3].color.dcolor = color;
			pVertices[4].xyz = Vec3(aabb.min.x, aabb.min.y, aabb.max.z);
			pVertices[4].color.dcolor = color;
			pVertices[5].xyz = Vec3(aabb.min.x, aabb.max.y, aabb.max.z);
			pVertices[5].color.dcolor = color;
			pVertices[6].xyz = Vec3(aabb.max.x, aabb.max.y, aabb.max.z);
			pVertices[6].color.dcolor = color;
			pVertices[7].xyz = Vec3(aabb.max.x, aabb.min.y, aabb.max.z);
			pVertices[7].color.dcolor = color;

			pIndices[0] = 0;
			pIndices[1] = 1;
			pIndices[2] = 1;
			pIndices[3] = 2;
			pIndices[4] = 2;
			pIndices[5] = 3;
			pIndices[6] = 3;
			pIndices[7] = 0;

			pIndices[8] = 4;
			pIndices[9] = 5;
			pIndices[10] = 5;
			pIndices[11] = 6;
			pIndices[12] = 6;
			pIndices[13] = 7;
			pIndices[14] = 7;
			pIndices[15] = 4;

			pIndices[16] = 0;
			pIndices[17] = 4;
			pIndices[18] = 1;
			pIndices[19] = 5;
			pIndices[20] = 2;
			pIndices[21] = 6;
			pIndices[22] = 3;
			pIndices[23] = 7;
		}
		else
		{
			AddIndexedPrimitive(pVertices, 24, pIndices, 36, CreateTriangleRenderFlags(true));

			Vec3 xyz(aabb.min.x, aabb.min.y, aabb.min.z);
			Vec3 xyZ(aabb.min.x, aabb.min.y, aabb.max.z);
			Vec3 xYz(aabb.min.x, aabb.max.y, aabb.min.z);
			Vec3 xYZ(aabb.min.x, aabb.max.y, aabb.max.z);
			Vec3 Xyz(aabb.max.x, aabb.min.y, aabb.min.z);
			Vec3 XyZ(aabb.max.x, aabb.min.y, aabb.max.z);
			Vec3 XYz(aabb.max.x, aabb.max.y, aabb.min.z);
			Vec3 XYZ(aabb.max.x, aabb.max.y, aabb.max.z);

			uint32 colDown(PackColor(ScaleColor(col, 0.5f)));
			pVertices[0].xyz = xyz;
			pVertices[0].color.dcolor = colDown;
			pVertices[1].xyz = xYz;
			pVertices[1].color.dcolor = colDown;
			pVertices[2].xyz = XYz;
			pVertices[2].color.dcolor = colDown;
			pVertices[3].xyz = Xyz;
			pVertices[3].color.dcolor = colDown;

			pIndices[0] = 0;
			pIndices[1] = 1;
			pIndices[2] = 2;
			pIndices[3] = 0;
			pIndices[4] = 2;
			pIndices[5] = 3;

			uint32 colTop(PackColor(col));
			pVertices[4].xyz = xyZ;
			pVertices[4].color.dcolor = colTop;
			pVertices[5].xyz = XyZ;
			pVertices[5].color.dcolor = colTop;
			pVertices[6].xyz = XYZ;
			pVertices[6].color.dcolor = colTop;
			pVertices[7].xyz = xYZ;
			pVertices[7].color.dcolor = colTop;

			pIndices[6] = 4;
			pIndices[7] = 5;
			pIndices[8] = 6;
			pIndices[9] = 4;
			pIndices[10] = 6;
			pIndices[11] = 7;

			uint32 colBack(PackColor(ScaleColor(col, 0.6f)));
			pVertices[8].xyz = xyz;
			pVertices[8].color.dcolor = colBack;
			pVertices[9].xyz = Xyz;
			pVertices[9].color.dcolor = colBack;
			pVertices[10].xyz = XyZ;
			pVertices[10].color.dcolor = colBack;
			pVertices[11].xyz = xyZ;
			pVertices[11].color.dcolor = colBack;

			pIndices[12] = 8;
			pIndices[13] = 9;
			pIndices[14] = 10;
			pIndices[15] = 8;
			pIndices[16] = 10;
			pIndices[17] = 11;

			uint32 colFront(PackColor(ScaleColor(col, 0.9f)));
			pVertices[12].xyz = xYz;
			pVertices[12].color.dcolor = colFront;
			pVertices[13].xyz = xYZ;
			pVertices[13].color.dcolor = colFront;
			pVertices[14].xyz = XYZ;
			pVertices[14].color.dcolor = colFront;
			pVertices[15].xyz = XYz;
			pVertices[15].color.dcolor = colFront;

			pIndices[18] = 12;
			pIndices[19] = 13;
			pIndices[20] = 14;
			pIndices[21] = 12;
			pIndices[22] = 14;
			pIndices[23] = 15;

			uint32 colLeft(PackColor(ScaleColor(col, 0.7f)));
			pVertices[16].xyz = xyz;
			pVertices[16].color.dcolor = colLeft;
			pVertices[17].xyz = xyZ;
			pVertices[17].color.dcolor = colLeft;
			pVertices[18].xyz = xYZ;
			pVertices[18].color.dcolor = colLeft;
			pVertices[19].xyz = xYz;
			pVertices[19].color.dcolor = colLeft;

			pIndices[24] = 16;
			pIndices[25] = 17;
			pIndices[26] = 18;
			pIndices[27] = 16;
			pIndices[28] = 18;
			pIndices[29] = 19;

			uint32 colRight(PackColor(ScaleColor(col, 0.8f)));
			pVertices[20].xyz = Xyz;
			pVertices[20].color.dcolor = colRight;
			pVertices[21].xyz = XYz;
			pVertices[21].color.dcolor = colRight;
			pVertices[22].xyz = XYZ;
			pVertices[22].color.dcolor = colRight;
			pVertices[23].xyz = XyZ;
			pVertices[23].color.dcolor = colRight;

			pIndices[30] = 20;
			pIndices[31] = 21;
			pIndices[32] = 22;
			pIndices[33] = 20;
			pIndices[34] = 22;
			pIndices[35] = 23;
		}
	}
}

void CAuxGeomCB::DrawAABBs(const AABB* aabbs, uint32 aabbCount, bool bSolid, const ColorB& col,
                           const EBoundingBoxDrawStyle& bbDrawStyle)
{
	SAuxVertex* pVertices(nullptr);
	vtx_idx* pIndices(nullptr);

	if (eBBD_Extremes_Color_Encoded == bbDrawStyle)
	{
		if (false == bSolid)
		{
			AddIndexedPrimitive(pVertices, 8 * aabbCount, pIndices, 24 * aabbCount, CreateLineRenderFlags(true));

			for (size_t i = 0; i < aabbCount; ++i)
			{
				size_t n = i * 24;
				size_t nv = i * 8;
				pIndices[n + 0] = 0;
				pIndices[n + 1] = nv + 1;
				pIndices[n + 2] = 1;
				pIndices[n + 3] = nv + 2;
				pIndices[n + 4] = 2;
				pIndices[n + 5] = nv + 3;
				pIndices[n + 6] = 3;
				pIndices[n + 7] = nv + 0;

				pIndices[n + 8] = 4;
				pIndices[n + 9] = nv + 5;
				pIndices[n + 10] = 5;
				pIndices[n + 11] = nv + 6;
				pIndices[n + 12] = 6;
				pIndices[n + 13] = nv + 7;
				pIndices[n + 14] = 7;
				pIndices[n + 15] = nv + 4;

				pIndices[n + 16] = 0;
				pIndices[n + 17] = nv + 4;
				pIndices[n + 18] = 1;
				pIndices[n + 19] = nv + 5;
				pIndices[n + 20] = 2;
				pIndices[n + 21] = nv + 6;
				pIndices[n + 22] = 3;
				pIndices[n + 23] = nv + 7;
			}
		}
		else
		{
			AddIndexedPrimitive(pVertices, 8 * aabbCount, pIndices, 36 * aabbCount, CreateTriangleRenderFlags(true));

			for (size_t i = 0; i < aabbCount; ++i)
			{
				size_t n = i * 36;
				size_t nv = i * 8;

				pIndices[n + 0] = nv + 0;
				pIndices[n + 1] = nv + 1;
				pIndices[n + 2] = nv + 2;
				pIndices[n + 3] = nv + 0;
				pIndices[n + 4] = nv + 2;
				pIndices[n + 5] = nv + 3;

				pIndices[n + 6] = nv + 7;
				pIndices[n + 7] = nv + 6;
				pIndices[n + 8] = nv + 5;
				pIndices[n + 9] = nv + 7;
				pIndices[n + 10] = nv + 5;
				pIndices[n + 11] = nv + 4;

				pIndices[n + 12] = nv + 3;
				pIndices[n + 13] = nv + 2;
				pIndices[n + 14] = nv + 6;
				pIndices[n + 15] = nv + 3;
				pIndices[n + 16] = nv + 6;
				pIndices[n + 17] = nv + 7;

				pIndices[n + 18] = nv + 4;
				pIndices[n + 19] = nv + 5;
				pIndices[n + 20] = nv + 1;
				pIndices[n + 21] = nv + 4;
				pIndices[n + 22] = nv + 1;
				pIndices[n + 23] = nv + 0;

				pIndices[n + 24] = nv + 1;
				pIndices[n + 25] = nv + 5;
				pIndices[n + 26] = nv + 6;
				pIndices[n + 27] = nv + 1;
				pIndices[n + 28] = nv + 6;
				pIndices[n + 29] = nv + 2;

				pIndices[n + 30] = nv + 4;
				pIndices[n + 31] = nv + 0;
				pIndices[n + 32] = nv + 3;
				pIndices[n + 33] = nv + 4;
				pIndices[n + 34] = nv + 3;
				pIndices[n + 35] = nv + 7;
			}
		}

		uint32 color(PackColor(col));
		uint32 colorMin(PackColor(ColorB(15, 15, 15, col.a)));
		uint32 colorMax(PackColor(ColorB(255, 255, 255, col.a)));

		for (size_t i = 0; i < aabbCount; ++i)
		{
			const AABB& aabb = aabbs[i];

			size_t n = i * 8;
			pVertices[n + 0].xyz = Vec3(aabb.min.x, aabb.min.y, aabb.min.z);
			pVertices[n + 0].color.dcolor = colorMin;
			pVertices[n + 1].xyz = Vec3(aabb.min.x, aabb.max.y, aabb.min.z);
			pVertices[n + 1].color.dcolor = color;
			pVertices[n + 2].xyz = Vec3(aabb.max.x, aabb.max.y, aabb.min.z);
			pVertices[n + 2].color.dcolor = color;
			pVertices[n + 3].xyz = Vec3(aabb.max.x, aabb.min.y, aabb.min.z);
			pVertices[n + 3].color.dcolor = color;
			pVertices[n + 4].xyz = Vec3(aabb.min.x, aabb.min.y, aabb.max.z);
			pVertices[n + 4].color.dcolor = color;
			pVertices[n + 5].xyz = Vec3(aabb.min.x, aabb.max.y, aabb.max.z);
			pVertices[n + 5].color.dcolor = color;
			pVertices[n + 6].xyz = Vec3(aabb.max.x, aabb.max.y, aabb.max.z);
			pVertices[n + 6].color.dcolor = colorMax;
			pVertices[n + 7].xyz = Vec3(aabb.max.x, aabb.min.y, aabb.max.z);
			pVertices[n + 7].color.dcolor = color;
		}
	}
	else
	{
		if (false == bSolid)
		{
			AddIndexedPrimitive(pVertices, 8 * aabbCount, pIndices, 24 * aabbCount, CreateLineRenderFlags(true));

			uint32 color(PackColor(col));

			for (size_t i = 0; i < aabbCount; ++i)
			{
				const AABB& aabb = aabbs[i];
				size_t nv = i * 24;
				size_t n = i * 8;

				pVertices[nv + 0].xyz = Vec3(aabb.min.x, aabb.min.y, aabb.min.z);
				pVertices[nv + 0].color.dcolor = color;
				pVertices[nv + 1].xyz = Vec3(aabb.min.x, aabb.max.y, aabb.min.z);
				pVertices[nv + 1].color.dcolor = color;
				pVertices[nv + 2].xyz = Vec3(aabb.max.x, aabb.max.y, aabb.min.z);
				pVertices[nv + 2].color.dcolor = color;
				pVertices[nv + 3].xyz = Vec3(aabb.max.x, aabb.min.y, aabb.min.z);
				pVertices[nv + 3].color.dcolor = color;
				pVertices[nv + 4].xyz = Vec3(aabb.min.x, aabb.min.y, aabb.max.z);
				pVertices[nv + 4].color.dcolor = color;
				pVertices[nv + 5].xyz = Vec3(aabb.min.x, aabb.max.y, aabb.max.z);
				pVertices[nv + 5].color.dcolor = color;
				pVertices[nv + 6].xyz = Vec3(aabb.max.x, aabb.max.y, aabb.max.z);
				pVertices[nv + 6].color.dcolor = color;
				pVertices[nv + 7].xyz = Vec3(aabb.max.x, aabb.min.y, aabb.max.z);
				pVertices[nv + 7].color.dcolor = color;

				pIndices[n + 0] = nv + 0;
				pIndices[n + 1] = nv + 1;
				pIndices[n + 2] = nv + 1;
				pIndices[n + 3] = nv + 2;
				pIndices[n + 4] = nv + 2;
				pIndices[n + 5] = nv + 3;
				pIndices[n + 6] = nv + 3;
				pIndices[n + 7] = nv + 0;

				pIndices[n + 8] = nv + 4;
				pIndices[n + 9] = nv + 5;
				pIndices[n + 10] = nv + 5;
				pIndices[n + 11] = nv + 6;
				pIndices[n + 12] = nv + 6;
				pIndices[n + 13] = nv + 7;
				pIndices[n + 14] = nv + 7;
				pIndices[n + 15] = nv + 4;

				pIndices[n + 16] = nv + 0;
				pIndices[n + 17] = nv + 4;
				pIndices[n + 18] = nv + 1;
				pIndices[n + 19] = nv + 5;
				pIndices[n + 20] = nv + 2;
				pIndices[n + 21] = nv + 6;
				pIndices[n + 22] = nv + 3;
				pIndices[n + 23] = nv + 7;
			}
		}
		else
		{
			AddIndexedPrimitive(pVertices, 24 * aabbCount, pIndices, 36 * aabbCount, CreateTriangleRenderFlags(true));

			for (size_t i = 0; i < aabbCount; ++i)
			{
				const AABB& aabb = aabbs[i];

				size_t n = i * 36;
				size_t nv = i * 24;

				Vec3 xyz(aabb.min.x, aabb.min.y, aabb.min.z);
				Vec3 xyZ(aabb.min.x, aabb.min.y, aabb.max.z);
				Vec3 xYz(aabb.min.x, aabb.max.y, aabb.min.z);
				Vec3 xYZ(aabb.min.x, aabb.max.y, aabb.max.z);
				Vec3 Xyz(aabb.max.x, aabb.min.y, aabb.min.z);
				Vec3 XyZ(aabb.max.x, aabb.min.y, aabb.max.z);
				Vec3 XYz(aabb.max.x, aabb.max.y, aabb.min.z);
				Vec3 XYZ(aabb.max.x, aabb.max.y, aabb.max.z);

				uint32 colDown(PackColor(ScaleColor(col, 0.5f)));
				pVertices[nv + 0].xyz = xyz;
				pVertices[nv + 0].color.dcolor = colDown;
				pVertices[nv + 1].xyz = xYz;
				pVertices[nv + 1].color.dcolor = colDown;
				pVertices[nv + 2].xyz = XYz;
				pVertices[nv + 2].color.dcolor = colDown;
				pVertices[nv + 3].xyz = Xyz;
				pVertices[nv + 3].color.dcolor = colDown;

				pIndices[n + 0] = nv + 0;
				pIndices[n + 1] = nv + 1;
				pIndices[n + 2] = nv + 2;
				pIndices[n + 3] = nv + 0;
				pIndices[n + 4] = nv + 2;
				pIndices[n + 5] = nv + 3;

				uint32 colTop(PackColor(col));
				pVertices[nv + 4].xyz = xyZ;
				pVertices[nv + 4].color.dcolor = colTop;
				pVertices[nv + 5].xyz = XyZ;
				pVertices[nv + 5].color.dcolor = colTop;
				pVertices[nv + 6].xyz = XYZ;
				pVertices[nv + 6].color.dcolor = colTop;
				pVertices[nv + 7].xyz = xYZ;
				pVertices[nv + 7].color.dcolor = colTop;

				pIndices[n + 6] = nv + 4;
				pIndices[n + 7] = nv + 5;
				pIndices[n + 8] = nv + 6;
				pIndices[n + 9] = nv + 4;
				pIndices[n + 10] = nv + 6;
				pIndices[n + 11] = nv + 7;

				uint32 colBack(PackColor(ScaleColor(col, 0.6f)));
				pVertices[nv + 8].xyz = xyz;
				pVertices[nv + 8].color.dcolor = colBack;
				pVertices[nv + 9].xyz = Xyz;
				pVertices[nv + 9].color.dcolor = colBack;
				pVertices[nv + 10].xyz = XyZ;
				pVertices[nv + 10].color.dcolor = colBack;
				pVertices[nv + 11].xyz = xyZ;
				pVertices[nv + 11].color.dcolor = colBack;

				pIndices[n + 12] = nv + 8;
				pIndices[n + 13] = nv + 9;
				pIndices[n + 14] = nv + 10;
				pIndices[n + 15] = nv + 8;
				pIndices[n + 16] = nv + 10;
				pIndices[n + 17] = nv + 11;

				uint32 colFront(PackColor(ScaleColor(col, 0.9f)));
				pVertices[nv + 12].xyz = xYz;
				pVertices[nv + 12].color.dcolor = colFront;
				pVertices[nv + 13].xyz = xYZ;
				pVertices[nv + 13].color.dcolor = colFront;
				pVertices[nv + 14].xyz = XYZ;
				pVertices[nv + 14].color.dcolor = colFront;
				pVertices[nv + 15].xyz = XYz;
				pVertices[nv + 15].color.dcolor = colFront;

				pIndices[n + 18] = nv + 12;
				pIndices[n + 19] = nv + 13;
				pIndices[n + 20] = nv + 14;
				pIndices[n + 21] = nv + 12;
				pIndices[n + 22] = nv + 14;
				pIndices[n + 23] = nv + 15;

				uint32 colLeft(PackColor(ScaleColor(col, 0.7f)));
				pVertices[nv + 16].xyz = xyz;
				pVertices[nv + 16].color.dcolor = colLeft;
				pVertices[nv + 17].xyz = xyZ;
				pVertices[nv + 17].color.dcolor = colLeft;
				pVertices[nv + 18].xyz = xYZ;
				pVertices[nv + 18].color.dcolor = colLeft;
				pVertices[nv + 19].xyz = xYz;
				pVertices[nv + 19].color.dcolor = colLeft;

				pIndices[n + 24] = nv + 16;
				pIndices[n + 25] = nv + 17;
				pIndices[n + 26] = nv + 18;
				pIndices[n + 27] = nv + 16;
				pIndices[n + 28] = nv + 18;
				pIndices[n + 29] = nv + 19;

				uint32 colRight(PackColor(ScaleColor(col, 0.8f)));
				pVertices[nv + 20].xyz = Xyz;
				pVertices[nv + 20].color.dcolor = colRight;
				pVertices[nv + 21].xyz = XYz;
				pVertices[nv + 21].color.dcolor = colRight;
				pVertices[nv + 22].xyz = XYZ;
				pVertices[nv + 22].color.dcolor = colRight;
				pVertices[nv + 23].xyz = XyZ;
				pVertices[nv + 23].color.dcolor = colRight;

				pIndices[n + 30] = nv + 20;
				pIndices[n + 31] = nv + 21;
				pIndices[n + 32] = nv + 22;
				pIndices[n + 33] = nv + 20;
				pIndices[n + 34] = nv + 22;
				pIndices[n + 35] = nv + 23;
			}
		}
	}
}

// TODO: remove this function in favor of just using PushMatrix around the previous function users
void CAuxGeomCB::DrawAABB(const AABB& aabb, const Matrix34& mat, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle)
{
	SAuxVertex* pVertices(nullptr);
	vtx_idx* pIndices(nullptr);
	int oldMatrixIndex = PushMatrix(mat);

	if (eBBD_Extremes_Color_Encoded == bbDrawStyle)
	{
		if (false == bSolid)
		{
			AddIndexedPrimitive(pVertices, 8, pIndices, 24, CreateLineRenderFlags(true) | AlphaFlags(col));

			pIndices[0] = 0;
			pIndices[1] = 1;
			pIndices[2] = 1;
			pIndices[3] = 2;
			pIndices[4] = 2;
			pIndices[5] = 3;
			pIndices[6] = 3;
			pIndices[7] = 0;

			pIndices[8] = 4;
			pIndices[9] = 5;
			pIndices[10] = 5;
			pIndices[11] = 6;
			pIndices[12] = 6;
			pIndices[13] = 7;
			pIndices[14] = 7;
			pIndices[15] = 4;

			pIndices[16] = 0;
			pIndices[17] = 4;
			pIndices[18] = 1;
			pIndices[19] = 5;
			pIndices[20] = 2;
			pIndices[21] = 6;
			pIndices[22] = 3;
			pIndices[23] = 7;
		}
		else
		{
			AddIndexedPrimitive(pVertices, 8, pIndices, 36, CreateTriangleRenderFlags(true));

			pIndices[0] = 0;
			pIndices[1] = 1;
			pIndices[2] = 2;
			pIndices[3] = 0;
			pIndices[4] = 2;
			pIndices[5] = 3;

			pIndices[6] = 7;
			pIndices[7] = 6;
			pIndices[8] = 5;
			pIndices[9] = 7;
			pIndices[10] = 5;
			pIndices[11] = 4;

			pIndices[12] = 3;
			pIndices[13] = 2;
			pIndices[14] = 6;
			pIndices[15] = 3;
			pIndices[16] = 6;
			pIndices[17] = 7;

			pIndices[18] = 4;
			pIndices[19] = 5;
			pIndices[20] = 1;
			pIndices[21] = 4;
			pIndices[22] = 1;
			pIndices[23] = 0;

			pIndices[24] = 1;
			pIndices[25] = 5;
			pIndices[26] = 6;
			pIndices[27] = 1;
			pIndices[28] = 6;
			pIndices[29] = 2;

			pIndices[30] = 4;
			pIndices[31] = 0;
			pIndices[32] = 3;
			pIndices[33] = 4;
			pIndices[34] = 3;
			pIndices[35] = 7;
		}

		uint32 color(PackColor(col));
		uint32 colorMin(PackColor(ColorB(15, 15, 15, col.a)));
		uint32 colorMax(PackColor(ColorB(255, 255, 255, col.a)));

		pVertices[0].xyz = Vec3(aabb.min.x, aabb.min.y, aabb.min.z);
		pVertices[0].color.dcolor = colorMin;
		pVertices[1].xyz = Vec3(aabb.min.x, aabb.max.y, aabb.min.z);
		pVertices[1].color.dcolor = color;
		pVertices[2].xyz = Vec3(aabb.max.x, aabb.max.y, aabb.min.z);
		pVertices[2].color.dcolor = color;
		pVertices[3].xyz = Vec3(aabb.max.x, aabb.min.y, aabb.min.z);
		pVertices[3].color.dcolor = color;
		pVertices[4].xyz = Vec3(aabb.min.x, aabb.min.y, aabb.max.z);
		pVertices[4].color.dcolor = color;
		pVertices[5].xyz = Vec3(aabb.min.x, aabb.max.y, aabb.max.z);
		pVertices[5].color.dcolor = color;
		pVertices[6].xyz = Vec3(aabb.max.x, aabb.max.y, aabb.max.z);
		pVertices[6].color.dcolor = colorMax;
		pVertices[7].xyz = Vec3(aabb.max.x, aabb.min.y, aabb.max.z);
		pVertices[7].color.dcolor = color;
	}
	else
	{
		if (false == bSolid)
		{
			AddIndexedPrimitive(pVertices, 8, pIndices, 24, CreateLineRenderFlags(true) | AlphaFlags(col));

			uint32 color(PackColor(col));

			pVertices[0].xyz = Vec3(aabb.min.x, aabb.min.y, aabb.min.z);
			pVertices[0].color.dcolor = color;
			pVertices[1].xyz = Vec3(aabb.min.x, aabb.max.y, aabb.min.z);
			pVertices[1].color.dcolor = color;
			pVertices[2].xyz = Vec3(aabb.max.x, aabb.max.y, aabb.min.z);
			pVertices[2].color.dcolor = color;
			pVertices[3].xyz = Vec3(aabb.max.x, aabb.min.y, aabb.min.z);
			pVertices[3].color.dcolor = color;
			pVertices[4].xyz = Vec3(aabb.min.x, aabb.min.y, aabb.max.z);
			pVertices[4].color.dcolor = color;
			pVertices[5].xyz = Vec3(aabb.min.x, aabb.max.y, aabb.max.z);
			pVertices[5].color.dcolor = color;
			pVertices[6].xyz = Vec3(aabb.max.x, aabb.max.y, aabb.max.z);
			pVertices[6].color.dcolor = color;
			pVertices[7].xyz = Vec3(aabb.max.x, aabb.min.y, aabb.max.z);
			pVertices[7].color.dcolor = color;

			pIndices[0] = 0;
			pIndices[1] = 1;
			pIndices[2] = 1;
			pIndices[3] = 2;
			pIndices[4] = 2;
			pIndices[5] = 3;
			pIndices[6] = 3;
			pIndices[7] = 0;

			pIndices[8] = 4;
			pIndices[9] = 5;
			pIndices[10] = 5;
			pIndices[11] = 6;
			pIndices[12] = 6;
			pIndices[13] = 7;
			pIndices[14] = 7;
			pIndices[15] = 4;

			pIndices[16] = 0;
			pIndices[17] = 4;
			pIndices[18] = 1;
			pIndices[19] = 5;
			pIndices[20] = 2;
			pIndices[21] = 6;
			pIndices[22] = 3;
			pIndices[23] = 7;
		}
		else
		{
			AddIndexedPrimitive(pVertices, 24, pIndices, 36, CreateTriangleRenderFlags(true));

			Vec3 xyz(Vec3(aabb.min.x, aabb.min.y, aabb.min.z));
			Vec3 xyZ(Vec3(aabb.min.x, aabb.min.y, aabb.max.z));
			Vec3 xYz(Vec3(aabb.min.x, aabb.max.y, aabb.min.z));
			Vec3 xYZ(Vec3(aabb.min.x, aabb.max.y, aabb.max.z));
			Vec3 Xyz(Vec3(aabb.max.x, aabb.min.y, aabb.min.z));
			Vec3 XyZ(Vec3(aabb.max.x, aabb.min.y, aabb.max.z));
			Vec3 XYz(Vec3(aabb.max.x, aabb.max.y, aabb.min.z));
			Vec3 XYZ(Vec3(aabb.max.x, aabb.max.y, aabb.max.z));

			uint32 colDown(PackColor(ScaleColor(col, 0.5f)));
			pVertices[0].xyz = xyz;
			pVertices[0].color.dcolor = colDown;
			pVertices[1].xyz = xYz;
			pVertices[1].color.dcolor = colDown;
			pVertices[2].xyz = XYz;
			pVertices[2].color.dcolor = colDown;
			pVertices[3].xyz = Xyz;
			pVertices[3].color.dcolor = colDown;

			pIndices[0] = 0;
			pIndices[1] = 1;
			pIndices[2] = 2;
			pIndices[3] = 0;
			pIndices[4] = 2;
			pIndices[5] = 3;

			uint32 colTop(PackColor(col));
			pVertices[4].xyz = xyZ;
			pVertices[4].color.dcolor = colTop;
			pVertices[5].xyz = XyZ;
			pVertices[5].color.dcolor = colTop;
			pVertices[6].xyz = XYZ;
			pVertices[6].color.dcolor = colTop;
			pVertices[7].xyz = xYZ;
			pVertices[7].color.dcolor = colTop;

			pIndices[6] = 4;
			pIndices[7] = 5;
			pIndices[8] = 6;
			pIndices[9] = 4;
			pIndices[10] = 6;
			pIndices[11] = 7;

			uint32 colBack(PackColor(ScaleColor(col, 0.6f)));
			pVertices[8].xyz = xyz;
			pVertices[8].color.dcolor = colBack;
			pVertices[9].xyz = Xyz;
			pVertices[9].color.dcolor = colBack;
			pVertices[10].xyz = XyZ;
			pVertices[10].color.dcolor = colBack;
			pVertices[11].xyz = xyZ;
			pVertices[11].color.dcolor = colBack;

			pIndices[12] = 8;
			pIndices[13] = 9;
			pIndices[14] = 10;
			pIndices[15] = 8;
			pIndices[16] = 10;
			pIndices[17] = 11;

			uint32 colFront(PackColor(ScaleColor(col, 0.9f)));
			pVertices[12].xyz = xYz;
			pVertices[12].color.dcolor = colFront;
			pVertices[13].xyz = xYZ;
			pVertices[13].color.dcolor = colFront;
			pVertices[14].xyz = XYZ;
			pVertices[14].color.dcolor = colFront;
			pVertices[15].xyz = XYz;
			pVertices[15].color.dcolor = colFront;

			pIndices[18] = 12;
			pIndices[19] = 13;
			pIndices[20] = 14;
			pIndices[21] = 12;
			pIndices[22] = 14;
			pIndices[23] = 15;

			uint32 colLeft(PackColor(ScaleColor(col, 0.7f)));
			pVertices[16].xyz = xyz;
			pVertices[16].color.dcolor = colLeft;
			pVertices[17].xyz = xyZ;
			pVertices[17].color.dcolor = colLeft;
			pVertices[18].xyz = xYZ;
			pVertices[18].color.dcolor = colLeft;
			pVertices[19].xyz = xYz;
			pVertices[19].color.dcolor = colLeft;

			pIndices[24] = 16;
			pIndices[25] = 17;
			pIndices[26] = 18;
			pIndices[27] = 16;
			pIndices[28] = 18;
			pIndices[29] = 19;

			uint32 colRight(PackColor(ScaleColor(col, 0.8f)));
			pVertices[20].xyz = Xyz;
			pVertices[20].color.dcolor = colRight;
			pVertices[21].xyz = XYz;
			pVertices[21].color.dcolor = colRight;
			pVertices[22].xyz = XYZ;
			pVertices[22].color.dcolor = colRight;
			pVertices[23].xyz = XyZ;
			pVertices[23].color.dcolor = colRight;

			pIndices[30] = 20;
			pIndices[31] = 21;
			pIndices[32] = 22;
			pIndices[33] = 20;
			pIndices[34] = 22;
			pIndices[35] = 23;
		}
	}

	m_cbCurrent->m_curWorldMatIdx = oldMatrixIndex;
}

void CAuxGeomCB::DrawOBB(const OBB& obb, const Vec3& pos, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle)
{
	SAuxVertex* pVertices(nullptr);
	vtx_idx* pIndices(nullptr);

	if (eBBD_Extremes_Color_Encoded == bbDrawStyle)
	{
		if (false == bSolid)
		{
			AddIndexedPrimitive(pVertices, 8, pIndices, 24, CreateLineRenderFlags(true) | AlphaFlags(col));

			pIndices[0] = 0;
			pIndices[1] = 1;
			pIndices[2] = 1;
			pIndices[3] = 2;
			pIndices[4] = 2;
			pIndices[5] = 3;
			pIndices[6] = 3;
			pIndices[7] = 0;

			pIndices[8] = 4;
			pIndices[9] = 5;
			pIndices[10] = 5;
			pIndices[11] = 6;
			pIndices[12] = 6;
			pIndices[13] = 7;
			pIndices[14] = 7;
			pIndices[15] = 4;

			pIndices[16] = 0;
			pIndices[17] = 4;
			pIndices[18] = 1;
			pIndices[19] = 5;
			pIndices[20] = 2;
			pIndices[21] = 6;
			pIndices[22] = 3;
			pIndices[23] = 7;
		}
		else
		{
			AddIndexedPrimitive(pVertices, 8, pIndices, 36, CreateTriangleRenderFlags(true));

			pIndices[0] = 0;
			pIndices[1] = 1;
			pIndices[2] = 2;
			pIndices[3] = 0;
			pIndices[4] = 2;
			pIndices[5] = 3;

			pIndices[6] = 7;
			pIndices[7] = 6;
			pIndices[8] = 5;
			pIndices[9] = 7;
			pIndices[10] = 5;
			pIndices[11] = 4;

			pIndices[12] = 3;
			pIndices[13] = 2;
			pIndices[14] = 6;
			pIndices[15] = 3;
			pIndices[16] = 6;
			pIndices[17] = 7;

			pIndices[18] = 4;
			pIndices[19] = 5;
			pIndices[20] = 1;
			pIndices[21] = 4;
			pIndices[22] = 1;
			pIndices[23] = 0;

			pIndices[24] = 1;
			pIndices[25] = 5;
			pIndices[26] = 6;
			pIndices[27] = 1;
			pIndices[28] = 6;
			pIndices[29] = 2;

			pIndices[30] = 4;
			pIndices[31] = 0;
			pIndices[32] = 3;
			pIndices[33] = 4;
			pIndices[34] = 3;
			pIndices[35] = 7;
		}

		uint32 color(PackColor(col));
		uint32 colorMin(PackColor(ColorB(15, 15, 15, col.a)));
		uint32 colorMax(PackColor(ColorB(255, 255, 255, col.a)));

		AABB aabb(obb.c - obb.h, obb.c + obb.h);
		pVertices[0].xyz = obb.m33 * Vec3(aabb.min.x, aabb.min.y, aabb.min.z) + pos;
		pVertices[0].color.dcolor = colorMin;
		pVertices[1].xyz = obb.m33 * Vec3(aabb.min.x, aabb.max.y, aabb.min.z) + pos;
		pVertices[1].color.dcolor = color;
		pVertices[2].xyz = obb.m33 * Vec3(aabb.max.x, aabb.max.y, aabb.min.z) + pos;
		pVertices[2].color.dcolor = color;
		pVertices[3].xyz = obb.m33 * Vec3(aabb.max.x, aabb.min.y, aabb.min.z) + pos;
		pVertices[3].color.dcolor = color;
		pVertices[4].xyz = obb.m33 * Vec3(aabb.min.x, aabb.min.y, aabb.max.z) + pos;
		pVertices[4].color.dcolor = color;
		pVertices[5].xyz = obb.m33 * Vec3(aabb.min.x, aabb.max.y, aabb.max.z) + pos;
		pVertices[5].color.dcolor = color;
		pVertices[6].xyz = obb.m33 * Vec3(aabb.max.x, aabb.max.y, aabb.max.z) + pos;
		pVertices[6].color.dcolor = colorMax;
		pVertices[7].xyz = obb.m33 * Vec3(aabb.max.x, aabb.min.y, aabb.max.z) + pos;
		pVertices[7].color.dcolor = color;
	}
	else
	{
		if (false == bSolid)
		{
			AddIndexedPrimitive(pVertices, 8, pIndices, 24, CreateLineRenderFlags(true) | AlphaFlags(col));

			uint32 color(PackColor(col));

			AABB aabb(obb.c - obb.h, obb.c + obb.h);
			pVertices[0].xyz = obb.m33 * Vec3(aabb.min.x, aabb.min.y, aabb.min.z) + pos;
			pVertices[0].color.dcolor = color;
			pVertices[1].xyz = obb.m33 * Vec3(aabb.min.x, aabb.max.y, aabb.min.z) + pos;
			pVertices[1].color.dcolor = color;
			pVertices[2].xyz = obb.m33 * Vec3(aabb.max.x, aabb.max.y, aabb.min.z) + pos;
			pVertices[2].color.dcolor = color;
			pVertices[3].xyz = obb.m33 * Vec3(aabb.max.x, aabb.min.y, aabb.min.z) + pos;
			pVertices[3].color.dcolor = color;
			pVertices[4].xyz = obb.m33 * Vec3(aabb.min.x, aabb.min.y, aabb.max.z) + pos;
			pVertices[4].color.dcolor = color;
			pVertices[5].xyz = obb.m33 * Vec3(aabb.min.x, aabb.max.y, aabb.max.z) + pos;
			pVertices[5].color.dcolor = color;
			pVertices[6].xyz = obb.m33 * Vec3(aabb.max.x, aabb.max.y, aabb.max.z) + pos;
			pVertices[6].color.dcolor = color;
			pVertices[7].xyz = obb.m33 * Vec3(aabb.max.x, aabb.min.y, aabb.max.z) + pos;
			pVertices[7].color.dcolor = color;

			pIndices[0] = 0;
			pIndices[1] = 1;
			pIndices[2] = 1;
			pIndices[3] = 2;
			pIndices[4] = 2;
			pIndices[5] = 3;
			pIndices[6] = 3;
			pIndices[7] = 0;

			pIndices[8] = 4;
			pIndices[9] = 5;
			pIndices[10] = 5;
			pIndices[11] = 6;
			pIndices[12] = 6;
			pIndices[13] = 7;
			pIndices[14] = 7;
			pIndices[15] = 4;

			pIndices[16] = 0;
			pIndices[17] = 4;
			pIndices[18] = 1;
			pIndices[19] = 5;
			pIndices[20] = 2;
			pIndices[21] = 6;
			pIndices[22] = 3;
			pIndices[23] = 7;
		}
		else
		{
			AddIndexedPrimitive(pVertices, 24, pIndices, 36, CreateTriangleRenderFlags(true));

			AABB aabb(obb.c - obb.h, obb.c + obb.h);
			Vec3 xyz(obb.m33 * Vec3(aabb.min.x, aabb.min.y, aabb.min.z) + pos);
			Vec3 xyZ(obb.m33 * Vec3(aabb.min.x, aabb.min.y, aabb.max.z) + pos);
			Vec3 xYz(obb.m33 * Vec3(aabb.min.x, aabb.max.y, aabb.min.z) + pos);
			Vec3 xYZ(obb.m33 * Vec3(aabb.min.x, aabb.max.y, aabb.max.z) + pos);
			Vec3 Xyz(obb.m33 * Vec3(aabb.max.x, aabb.min.y, aabb.min.z) + pos);
			Vec3 XyZ(obb.m33 * Vec3(aabb.max.x, aabb.min.y, aabb.max.z) + pos);
			Vec3 XYz(obb.m33 * Vec3(aabb.max.x, aabb.max.y, aabb.min.z) + pos);
			Vec3 XYZ(obb.m33 * Vec3(aabb.max.x, aabb.max.y, aabb.max.z) + pos);

			uint32 colDown(PackColor(ScaleColor(col, 0.5f)));
			pVertices[0].xyz = xyz;
			pVertices[0].color.dcolor = colDown;
			pVertices[1].xyz = xYz;
			pVertices[1].color.dcolor = colDown;
			pVertices[2].xyz = XYz;
			pVertices[2].color.dcolor = colDown;
			pVertices[3].xyz = Xyz;
			pVertices[3].color.dcolor = colDown;

			pIndices[0] = 0;
			pIndices[1] = 1;
			pIndices[2] = 2;
			pIndices[3] = 0;
			pIndices[4] = 2;
			pIndices[5] = 3;

			uint32 colTop(PackColor(col));
			pVertices[4].xyz = xyZ;
			pVertices[4].color.dcolor = colTop;
			pVertices[5].xyz = XyZ;
			pVertices[5].color.dcolor = colTop;
			pVertices[6].xyz = XYZ;
			pVertices[6].color.dcolor = colTop;
			pVertices[7].xyz = xYZ;
			pVertices[7].color.dcolor = colTop;

			pIndices[6] = 4;
			pIndices[7] = 5;
			pIndices[8] = 6;
			pIndices[9] = 4;
			pIndices[10] = 6;
			pIndices[11] = 7;

			uint32 colBack(PackColor(ScaleColor(col, 0.6f)));
			pVertices[8].xyz = xyz;
			pVertices[8].color.dcolor = colBack;
			pVertices[9].xyz = Xyz;
			pVertices[9].color.dcolor = colBack;
			pVertices[10].xyz = XyZ;
			pVertices[10].color.dcolor = colBack;
			pVertices[11].xyz = xyZ;
			pVertices[11].color.dcolor = colBack;

			pIndices[12] = 8;
			pIndices[13] = 9;
			pIndices[14] = 10;
			pIndices[15] = 8;
			pIndices[16] = 10;
			pIndices[17] = 11;

			uint32 colFront(PackColor(ScaleColor(col, 0.9f)));
			pVertices[12].xyz = xYz;
			pVertices[12].color.dcolor = colFront;
			pVertices[13].xyz = xYZ;
			pVertices[13].color.dcolor = colFront;
			pVertices[14].xyz = XYZ;
			pVertices[14].color.dcolor = colFront;
			pVertices[15].xyz = XYz;
			pVertices[15].color.dcolor = colFront;

			pIndices[18] = 12;
			pIndices[19] = 13;
			pIndices[20] = 14;
			pIndices[21] = 12;
			pIndices[22] = 14;
			pIndices[23] = 15;

			uint32 colLeft(PackColor(ScaleColor(col, 0.7f)));
			pVertices[16].xyz = xyz;
			pVertices[16].color.dcolor = colLeft;
			pVertices[17].xyz = xyZ;
			pVertices[17].color.dcolor = colLeft;
			pVertices[18].xyz = xYZ;
			pVertices[18].color.dcolor = colLeft;
			pVertices[19].xyz = xYz;
			pVertices[19].color.dcolor = colLeft;

			pIndices[24] = 16;
			pIndices[25] = 17;
			pIndices[26] = 18;
			pIndices[27] = 16;
			pIndices[28] = 18;
			pIndices[29] = 19;

			uint32 colRight(PackColor(ScaleColor(col, 0.8f)));
			pVertices[20].xyz = Xyz;
			pVertices[20].color.dcolor = colRight;
			pVertices[21].xyz = XYz;
			pVertices[21].color.dcolor = colRight;
			pVertices[22].xyz = XYZ;
			pVertices[22].color.dcolor = colRight;
			pVertices[23].xyz = XyZ;
			pVertices[23].color.dcolor = colRight;

			pIndices[30] = 20;
			pIndices[31] = 21;
			pIndices[32] = 22;
			pIndices[33] = 20;
			pIndices[34] = 22;
			pIndices[35] = 23;
		}
	}
}

void CAuxGeomCB::DrawOBB(const OBB& obb, const Matrix34& mat, bool bSolid, const ColorB& col, const EBoundingBoxDrawStyle& bbDrawStyle)
{
	SAuxVertex* pVertices(nullptr);
	vtx_idx* pIndices(nullptr);
	int oldMatrixIndex = PushMatrix(mat);

	if (eBBD_Extremes_Color_Encoded == bbDrawStyle)
	{
		if (false == bSolid)
		{
			AddIndexedPrimitive(pVertices, 8, pIndices, 24, CreateLineRenderFlags(true) | AlphaFlags(col));

			pIndices[0] = 0;
			pIndices[1] = 1;
			pIndices[2] = 1;
			pIndices[3] = 2;
			pIndices[4] = 2;
			pIndices[5] = 3;
			pIndices[6] = 3;
			pIndices[7] = 0;

			pIndices[8] = 4;
			pIndices[9] = 5;
			pIndices[10] = 5;
			pIndices[11] = 6;
			pIndices[12] = 6;
			pIndices[13] = 7;
			pIndices[14] = 7;
			pIndices[15] = 4;

			pIndices[16] = 0;
			pIndices[17] = 4;
			pIndices[18] = 1;
			pIndices[19] = 5;
			pIndices[20] = 2;
			pIndices[21] = 6;
			pIndices[22] = 3;
			pIndices[23] = 7;
		}
		else
		{
			AddIndexedPrimitive(pVertices, 8, pIndices, 36, CreateTriangleRenderFlags(true));

			pIndices[0] = 0;
			pIndices[1] = 1;
			pIndices[2] = 2;
			pIndices[3] = 0;
			pIndices[4] = 2;
			pIndices[5] = 3;

			pIndices[6] = 7;
			pIndices[7] = 6;
			pIndices[8] = 5;
			pIndices[9] = 7;
			pIndices[10] = 5;
			pIndices[11] = 4;

			pIndices[12] = 3;
			pIndices[13] = 2;
			pIndices[14] = 6;
			pIndices[15] = 3;
			pIndices[16] = 6;
			pIndices[17] = 7;

			pIndices[18] = 4;
			pIndices[19] = 5;
			pIndices[20] = 1;
			pIndices[21] = 4;
			pIndices[22] = 1;
			pIndices[23] = 0;

			pIndices[24] = 1;
			pIndices[25] = 5;
			pIndices[26] = 6;
			pIndices[27] = 1;
			pIndices[28] = 6;
			pIndices[29] = 2;

			pIndices[30] = 4;
			pIndices[31] = 0;
			pIndices[32] = 3;
			pIndices[33] = 4;
			pIndices[34] = 3;
			pIndices[35] = 7;
		}

		uint32 color(PackColor(col));
		uint32 colorMin(PackColor(ColorB(15, 15, 15, col.a)));
		uint32 colorMax(PackColor(ColorB(255, 255, 255, col.a)));

		AABB aabb(obb.c - obb.h, obb.c + obb.h);
		pVertices[0].xyz = obb.m33 * Vec3(aabb.min.x, aabb.min.y, aabb.min.z);
		pVertices[0].color.dcolor = colorMin;
		pVertices[1].xyz = obb.m33 * Vec3(aabb.min.x, aabb.max.y, aabb.min.z);
		pVertices[1].color.dcolor = color;
		pVertices[2].xyz = obb.m33 * Vec3(aabb.max.x, aabb.max.y, aabb.min.z);
		pVertices[2].color.dcolor = color;
		pVertices[3].xyz = obb.m33 * Vec3(aabb.max.x, aabb.min.y, aabb.min.z);
		pVertices[3].color.dcolor = color;
		pVertices[4].xyz = obb.m33 * Vec3(aabb.min.x, aabb.min.y, aabb.max.z);
		pVertices[4].color.dcolor = color;
		pVertices[5].xyz = obb.m33 * Vec3(aabb.min.x, aabb.max.y, aabb.max.z);
		pVertices[5].color.dcolor = color;
		pVertices[6].xyz = obb.m33 * Vec3(aabb.max.x, aabb.max.y, aabb.max.z);
		pVertices[6].color.dcolor = colorMax;
		pVertices[7].xyz = obb.m33 * Vec3(aabb.max.x, aabb.min.y, aabb.max.z);
		pVertices[7].color.dcolor = color;
	}
	else
	{
		if (false == bSolid)
		{
			AddIndexedPrimitive(pVertices, 8, pIndices, 24, CreateLineRenderFlags(true) | AlphaFlags(col));

			uint32 color(PackColor(col));

			AABB aabb(obb.c - obb.h, obb.c + obb.h);
			pVertices[0].xyz = obb.m33 * Vec3(aabb.min.x, aabb.min.y, aabb.min.z);
			pVertices[0].color.dcolor = color;
			pVertices[1].xyz = obb.m33 * Vec3(aabb.min.x, aabb.max.y, aabb.min.z);
			pVertices[1].color.dcolor = color;
			pVertices[2].xyz = obb.m33 * Vec3(aabb.max.x, aabb.max.y, aabb.min.z);
			pVertices[2].color.dcolor = color;
			pVertices[3].xyz = obb.m33 * Vec3(aabb.max.x, aabb.min.y, aabb.min.z);
			pVertices[3].color.dcolor = color;
			pVertices[4].xyz = obb.m33 * Vec3(aabb.min.x, aabb.min.y, aabb.max.z);
			pVertices[4].color.dcolor = color;
			pVertices[5].xyz = obb.m33 * Vec3(aabb.min.x, aabb.max.y, aabb.max.z);
			pVertices[5].color.dcolor = color;
			pVertices[6].xyz = obb.m33 * Vec3(aabb.max.x, aabb.max.y, aabb.max.z);
			pVertices[6].color.dcolor = color;
			pVertices[7].xyz = obb.m33 * Vec3(aabb.max.x, aabb.min.y, aabb.max.z);
			pVertices[7].color.dcolor = color;

			pIndices[0] = 0;
			pIndices[1] = 1;
			pIndices[2] = 1;
			pIndices[3] = 2;
			pIndices[4] = 2;
			pIndices[5] = 3;
			pIndices[6] = 3;
			pIndices[7] = 0;

			pIndices[8] = 4;
			pIndices[9] = 5;
			pIndices[10] = 5;
			pIndices[11] = 6;
			pIndices[12] = 6;
			pIndices[13] = 7;
			pIndices[14] = 7;
			pIndices[15] = 4;

			pIndices[16] = 0;
			pIndices[17] = 4;
			pIndices[18] = 1;
			pIndices[19] = 5;
			pIndices[20] = 2;
			pIndices[21] = 6;
			pIndices[22] = 3;
			pIndices[23] = 7;
		}
		else
		{
			AddIndexedPrimitive(pVertices, 24, pIndices, 36, CreateTriangleRenderFlags(true));

			AABB aabb(obb.c - obb.h, obb.c + obb.h);
			Vec3 xyz(obb.m33 * Vec3(aabb.min.x, aabb.min.y, aabb.min.z));
			Vec3 xyZ(obb.m33 * Vec3(aabb.min.x, aabb.min.y, aabb.max.z));
			Vec3 xYz(obb.m33 * Vec3(aabb.min.x, aabb.max.y, aabb.min.z));
			Vec3 xYZ(obb.m33 * Vec3(aabb.min.x, aabb.max.y, aabb.max.z));
			Vec3 Xyz(obb.m33 * Vec3(aabb.max.x, aabb.min.y, aabb.min.z));
			Vec3 XyZ(obb.m33 * Vec3(aabb.max.x, aabb.min.y, aabb.max.z));
			Vec3 XYz(obb.m33 * Vec3(aabb.max.x, aabb.max.y, aabb.min.z));
			Vec3 XYZ(obb.m33 * Vec3(aabb.max.x, aabb.max.y, aabb.max.z));

			uint32 colDown(PackColor(ScaleColor(col, 0.5f)));
			pVertices[0].xyz = xyz;
			pVertices[0].color.dcolor = colDown;
			pVertices[1].xyz = xYz;
			pVertices[1].color.dcolor = colDown;
			pVertices[2].xyz = XYz;
			pVertices[2].color.dcolor = colDown;
			pVertices[3].xyz = Xyz;
			pVertices[3].color.dcolor = colDown;

			pIndices[0] = 0;
			pIndices[1] = 1;
			pIndices[2] = 2;
			pIndices[3] = 0;
			pIndices[4] = 2;
			pIndices[5] = 3;

			uint32 colTop(PackColor(col));
			pVertices[4].xyz = xyZ;
			pVertices[4].color.dcolor = colTop;
			pVertices[5].xyz = XyZ;
			pVertices[5].color.dcolor = colTop;
			pVertices[6].xyz = XYZ;
			pVertices[6].color.dcolor = colTop;
			pVertices[7].xyz = xYZ;
			pVertices[7].color.dcolor = colTop;

			pIndices[6] = 4;
			pIndices[7] = 5;
			pIndices[8] = 6;
			pIndices[9] = 4;
			pIndices[10] = 6;
			pIndices[11] = 7;

			uint32 colBack(PackColor(ScaleColor(col, 0.6f)));
			pVertices[8].xyz = xyz;
			pVertices[8].color.dcolor = colBack;
			pVertices[9].xyz = Xyz;
			pVertices[9].color.dcolor = colBack;
			pVertices[10].xyz = XyZ;
			pVertices[10].color.dcolor = colBack;
			pVertices[11].xyz = xyZ;
			pVertices[11].color.dcolor = colBack;

			pIndices[12] = 8;
			pIndices[13] = 9;
			pIndices[14] = 10;
			pIndices[15] = 8;
			pIndices[16] = 10;
			pIndices[17] = 11;

			uint32 colFront(PackColor(ScaleColor(col, 0.9f)));
			pVertices[12].xyz = xYz;
			pVertices[12].color.dcolor = colFront;
			pVertices[13].xyz = xYZ;
			pVertices[13].color.dcolor = colFront;
			pVertices[14].xyz = XYZ;
			pVertices[14].color.dcolor = colFront;
			pVertices[15].xyz = XYz;
			pVertices[15].color.dcolor = colFront;

			pIndices[18] = 12;
			pIndices[19] = 13;
			pIndices[20] = 14;
			pIndices[21] = 12;
			pIndices[22] = 14;
			pIndices[23] = 15;

			uint32 colLeft(PackColor(ScaleColor(col, 0.7f)));
			pVertices[16].xyz = xyz;
			pVertices[16].color.dcolor = colLeft;
			pVertices[17].xyz = xyZ;
			pVertices[17].color.dcolor = colLeft;
			pVertices[18].xyz = xYZ;
			pVertices[18].color.dcolor = colLeft;
			pVertices[19].xyz = xYz;
			pVertices[19].color.dcolor = colLeft;

			pIndices[24] = 16;
			pIndices[25] = 17;
			pIndices[26] = 18;
			pIndices[27] = 16;
			pIndices[28] = 18;
			pIndices[29] = 19;

			uint32 colRight(PackColor(ScaleColor(col, 0.8f)));
			pVertices[20].xyz = Xyz;
			pVertices[20].color.dcolor = colRight;
			pVertices[21].xyz = XYz;
			pVertices[21].color.dcolor = colRight;
			pVertices[22].xyz = XYZ;
			pVertices[22].color.dcolor = colRight;
			pVertices[23].xyz = XyZ;
			pVertices[23].color.dcolor = colRight;

			pIndices[30] = 20;
			pIndices[31] = 21;
			pIndices[32] = 22;
			pIndices[33] = 20;
			pIndices[34] = 22;
			pIndices[35] = 23;
		}
	}
	m_cbCurrent->m_curWorldMatIdx = oldMatrixIndex;
}

void CAuxGeomCB::DrawSphere(const Vec3& pos, float radius, const ColorB& col, bool drawShaded)
{
	if (radius > 0.0f)
	{
		SAuxDrawObjParams* pDrawParams(nullptr);
		AddObject(pDrawParams, CreateObjectRenderFlags(eDOT_Sphere));

		pDrawParams->m_matWorld = Matrix34::CreateTranslationMat(pos) * Matrix33::CreateScale(Vec3(radius, radius, radius));
		pDrawParams->m_color = PackColor(col);
		pDrawParams->m_size = radius;
		pDrawParams->m_shaded = drawShaded;
	}
}

void CAuxGeomCB::DrawCone(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorB& col, bool drawShaded)
{
	if (radius > 0.0f && height > 0.0f && dir.GetLengthSquared() > 0.0f)
	{
		SAuxDrawObjParams* pDrawParams(nullptr);
		AddObject(pDrawParams, CreateObjectRenderFlags(eDOT_Cone));

		Vec3 direction(dir.normalized());
		Vec3 orthogonal(direction.GetOrthogonal().normalized());

		Matrix33 matRot;
		matRot.SetIdentity();
		matRot.SetColumn(0, orthogonal);
		matRot.SetColumn(1, direction);
		matRot.SetColumn(2, orthogonal.Cross(direction));

		pDrawParams->m_matWorld = Matrix34::CreateTranslationMat(pos) * matRot * Matrix33::CreateScale(Vec3(radius, height, radius));
		pDrawParams->m_color = PackColor(col);
		pDrawParams->m_size = max(radius, height * 0.5f);
		pDrawParams->m_shaded = drawShaded;
	}
}

void CAuxGeomCB::DrawCylinder(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorB& col, bool drawShaded)
{
	if (radius > 0.0f && height > 0.0f && dir.GetLengthSquared() > 0.0f)
	{
		SAuxDrawObjParams* pDrawParams(nullptr);
		AddObject(pDrawParams, CreateObjectRenderFlags(eDOT_Cylinder));

		Vec3 direction(dir.normalized());
		Vec3 orthogonal(direction.GetOrthogonal().normalized());

		Matrix33 matRot;
		matRot.SetIdentity();
		matRot.SetColumn(0, orthogonal);
		matRot.SetColumn(1, direction);
		matRot.SetColumn(2, orthogonal.Cross(direction));

		pDrawParams->m_matWorld = Matrix34::CreateTranslationMat(pos) * matRot * Matrix33::CreateScale(Vec3(radius, height, radius));
		pDrawParams->m_color = PackColor(col);
		pDrawParams->m_size = max(radius, height * 0.5f);
		pDrawParams->m_shaded = drawShaded;
	}
}

void CAuxGeomCB::DrawBone(const Vec3& p, const Vec3& c, ColorB col)
{
	Vec3 vBoneVec = c - p;
	float fBoneLength = vBoneVec.GetLength();

	if (fBoneLength < 1e-4)
		return;

	Matrix33 m33 = Matrix33::CreateRotationV0V1(Vec3(1, 0, 0), vBoneVec / fBoneLength);
	Matrix34 m34 = Matrix34(m33, p);

	f32 t = min(0.01f, fBoneLength * 0.05f);

	//bone points in x-direction
	Vec3 s = Vec3(ZERO);
	Vec3 m0 = Vec3(t, +t, +t);
	Vec3 m1 = Vec3(t, -t, +t);
	Vec3 m2 = Vec3(t, -t, -t);
	Vec3 m3 = Vec3(t, +t, -t);
	Vec3 e = Vec3(fBoneLength, 0, 0);

	Vec3 VBuffer[6];
	ColorB CBuffer[6];

	VBuffer[0] = m34 * s;
	CBuffer[0] = RGBA8(0xff, 0x1f, 0x1f, 0x00);  //start of bone (joint)

	VBuffer[1] = m34 * m0;
	CBuffer[1] = col;
	VBuffer[2] = m34 * m1;
	CBuffer[2] = col;
	VBuffer[3] = m34 * m2;
	CBuffer[3] = col;
	VBuffer[4] = m34 * m3;
	CBuffer[4] = col;

	VBuffer[5] = m34 * e;
	CBuffer[5] = RGBA8(0x07, 0x0f, 0x1f, 0x00); //end of bone

	DrawLine(VBuffer[0], CBuffer[0], VBuffer[1], CBuffer[1]);
	DrawLine(VBuffer[0], CBuffer[0], VBuffer[2], CBuffer[2]);
	DrawLine(VBuffer[0], CBuffer[0], VBuffer[3], CBuffer[3]);
	DrawLine(VBuffer[0], CBuffer[0], VBuffer[4], CBuffer[4]);

	DrawLine(VBuffer[1], CBuffer[1], VBuffer[2], CBuffer[2]);
	DrawLine(VBuffer[2], CBuffer[2], VBuffer[3], CBuffer[3]);
	DrawLine(VBuffer[3], CBuffer[3], VBuffer[4], CBuffer[4]);
	DrawLine(VBuffer[4], CBuffer[4], VBuffer[1], CBuffer[1]);

	DrawLine(VBuffer[5], CBuffer[5], VBuffer[1], CBuffer[1]);
	DrawLine(VBuffer[5], CBuffer[5], VBuffer[2], CBuffer[2]);
	DrawLine(VBuffer[5], CBuffer[5], VBuffer[3], CBuffer[3]);
	DrawLine(VBuffer[5], CBuffer[5], VBuffer[4], CBuffer[4]);
}

	#include "CommonRender.h"

void CAuxGeomCB::RenderTextQueued(Vec3 pos, const SDrawTextInfo& ti, const char* text)
{
	if (!gEnv->IsDedicated())
	{
		ColorB col(ColorF(ti.color[0], ti.color[1], ti.color[2], ti.color[3]));

		m_cbCurrent->m_TextMessages.PushEntry_Text(pos, col, ti.scale, ti.flags, text);
	}
}

	#include "D3DStereo.h"

void CAuxGeomCB::DrawStringImmediate(IFFont_RenderProxy* pFont, float x, float y, float z, const char* pStr, const bool asciiMultiLine, const STextDrawContext& ctx)
{
	m_pRenderAuxGeom->DrawStringImmediate(pFont, x, y, z, pStr, asciiMultiLine, ctx);
}

void CAuxGeomCB::DrawBufferRT(const SAuxVertex* data, int numVertices, int blendMode, const Matrix44* matViewProj, int texID)
{
	m_pRenderAuxGeom->DrawBufferRT(data, numVertices, blendMode, matViewProj, texID);
}

int32 CAuxGeomCB::PushMatrix(const Matrix34& mat)
{
	int curIndex = m_cbCurrent->m_curWorldMatIdx;
	m_cbCurrent->m_curWorldMatIdx = m_cbCurrent->m_auxWorldMatrices.size();
	m_cbCurrent->m_auxWorldMatrices.push_back(mat);
	return curIndex;
}

int CAuxGeomCB::SetTexture(int texID)
{
	int prev = m_cbCurrent->m_textureID;
	m_cbCurrent->m_textureID = texID;
	return prev;
}

Matrix34* CAuxGeomCB::GetMatrix()
{
	return &m_cbCurrent->m_auxWorldMatrices[m_cbCurrent->m_curWorldMatIdx];
}

void CAuxGeomCB::SetMatrixIndex(int matID)
{
	m_cbCurrent->m_curWorldMatIdx = matID;
}

void CAuxGeomCB::Flush()
{
	Flush(true);
}

void CAuxGeomCB::Flush(bool reset)
{
	FUNCTION_PROFILER_RENDERER

	size_t lastFlushPos = GetLastFlushPos();
	size_t curFlushPos = GetCurFlushPos();
	if ((lastFlushPos < curFlushPos) || reset)
	{
		UpdateLastFlushPos();
		SAuxGeomCBRawDataPackaged data = SAuxGeomCBRawDataPackaged(AccessData());
		m_pRenderAuxGeom->Flush(data, lastFlushPos, curFlushPos, reset);
	}

	m_pRenderAuxGeom->FlushTextMessages(m_cbCurrent->m_TextMessages, reset);
}

void CAuxGeomCB::Commit(uint frames)
{
	Flush();
}

void CAuxGeomCB::Process()
{
	m_pRenderAuxGeom->FlushTextMessages(m_cbCurrent->m_TextMessages, true);
	m_lastFlushPos = 0;
	m_cbCurrent->Reset();
}

void CAuxGeomCBWorkerThread::Flush()
{
}

void CAuxGeomCBWorkerThread::Commit(uint frames)
{
	m_cbCurrent->SetUsed(true);
	m_cbCurrent->SetCount(frames);

	Flush();

	CRY_ASSERT(m_cbCurrent->m_curTransMatIdx == -1);
	m_cbCurrent->m_curTransMatIdx = -1;

	m_cbCurrent = (SAuxGeomCBRawData*)CryInterlockedExchangePointer((void* volatile*)&m_CBReady, m_cbCurrent);

	if (m_cbCurrent == nullptr)
	{
		for (auto const pData : m_cbData)
		{
			if (!pData->IsUsed())
			{
				m_cbCurrent = pData;
			}
		}

		if (!m_cbCurrent)
		{
			m_cbCurrent = AddCBuffer();
		}
	}

	m_lastFlushPos = 0;

	m_cbCurrent->Reset();
}

void CAuxGeomCBWorkerThread::Process()
{
	if (SAuxGeomCBRawData* current = (SAuxGeomCBRawData*)CryInterlockedExchangePointer((void* volatile*)&m_CBReady, nullptr))
	{
		if (m_cbProcessed) m_cbProcessed->SetUsed(false);

		m_cbProcessed = current;
	}

	if (SAuxGeomCBRawData* processed = m_cbProcessed)
	{
		if (size_t curFlushPos = processed->m_auxPushBuffer.size())
		{
			SAuxGeomCBRawDataPackaged data = SAuxGeomCBRawDataPackaged(processed);
			m_pRenderAuxGeom->Flush(data, 0, curFlushPos);
		}

		m_pRenderAuxGeom->FlushTextMessages(processed->m_TextMessages, false);

		if (processed->Count() == 1)
		{
			processed->SetUsed(false);
			m_cbProcessed = nullptr;
		}
	}
}

void CAuxGeomCB::SAuxGeomCBRawData::GetSortedPushBuffer(size_t begin, size_t end, AuxSortedPushBuffer& auxSortedPushBuffer) const
{
	CRY_ASSERT(begin < end);
	CRY_ASSERT(end <= m_auxPushBuffer.size());

	auxSortedPushBuffer.reserve(end - begin);
	auxSortedPushBuffer.resize(0);

	for (AuxPushBuffer::const_iterator it(m_auxPushBuffer.begin() + begin), itEnd(m_auxPushBuffer.begin() + end); it != itEnd; ++it)
	{
		auxSortedPushBuffer.push_back(&(*it));
	}

	std::sort(auxSortedPushBuffer.begin(), auxSortedPushBuffer.end(), PushBufferSortFunc());
}

void CAuxGeomCB::AddPushBufferEntry(uint32 numVertices, uint32 numIndices, const SAuxGeomRenderFlags& renderFlags)
{
	CRY_ASSERT(numVertices > 0);

	AuxPushBuffer& auxPushBuffer(AccessData()->m_auxPushBuffer);

	if (GetWorldMatrixIndex() == -1)
	{
		perror("");
	}

	EPrimType primType = GetPrimType(renderFlags);
	int textureID = IsTextured(renderFlags) ? AccessData()->m_textureID : -1;

	if (false == auxPushBuffer.empty() &&
	    auxPushBuffer[auxPushBuffer.size() - 1].m_textureID == textureID &&
	    auxPushBuffer[auxPushBuffer.size() - 1].m_renderFlags == renderFlags &&
	    auxPushBuffer[auxPushBuffer.size() - 1].m_transMatrixIdx == GetTransMatrixIndex() &&
	    auxPushBuffer[auxPushBuffer.size() - 1].m_worldMatrixIdx == GetWorldMatrixIndex() &&
	    (e_PtList == primType || e_LineList == primType || e_TriList == primType))
	{
		// Perform a runtime optimization (pre-merging) which effectively reduces the number of PB entries created.
		// We can merge this entry with the previous one as its render flags match with the ones of the previous entry
		// (e.g. merges consecutive DrawLine(...) calls with same render flags into one PB entry).

		// Only done for non-indexed primitives as otherwise there would be the additional overhead of patching
		// the indices for each push buffer entry. Indices already and still have to be patched during rendering
		// anyway (merging) so in case of indexed primitives there'd be no real benefit. Also, merging up too many
		// indexed primitives could potentially produce a push buffer entry which cannot be rendered as it exceeds
		// the vb/ib buffer size for auxiliary geometries in the renderer.
		SAuxPushBufferEntry& lastPBEntry(auxPushBuffer[auxPushBuffer.size() - 1]);
		lastPBEntry.m_numVertices += numVertices;
		lastPBEntry.m_numIndices += numIndices;
	}
	else
	{
		// create new push buffer entry
		auxPushBuffer.emplace_back(numVertices, numIndices, AccessData()->m_auxVertexBuffer.size(), AccessData()->m_auxIndexBuffer.size(), GetTransMatrixIndex(), GetWorldMatrixIndex(), renderFlags, textureID);
	}
}

void CAuxGeomCB::AddPrimitive(SAuxVertex*& pVertices, uint32 numVertices, const SAuxGeomRenderFlags& renderFlags)
{
	CRY_ASSERT(numVertices > 0);

	// add push buffer entry to allow later merging of batches committed via DP
	AddPushBufferEntry(numVertices, 0, renderFlags);

	// get vertex ptr
	AuxVertexBuffer& auxVertexBuffer(AccessData()->m_auxVertexBuffer);
	AuxVertexBuffer::size_type oldVBSize(auxVertexBuffer.size());
	auxVertexBuffer.resize(oldVBSize + numVertices);
	pVertices = &auxVertexBuffer[oldVBSize];
}

void CAuxGeomCB::AddIndexedPrimitive(SAuxVertex*& pVertices, uint32 numVertices, vtx_idx*& pIndices, uint32 numIndices, const SAuxGeomRenderFlags& renderFlags)
{
	CRY_ASSERT(numVertices > 0);
	CRY_ASSERT(numIndices > 0);

	// add push buffer entry to allow later merging of batches committed via DIP
	AddPushBufferEntry(numVertices, numIndices, renderFlags);

	// get vertex ptr
	AuxVertexBuffer& auxVertexBuffer(AccessData()->m_auxVertexBuffer);
	AuxVertexBuffer::size_type oldVBSize(auxVertexBuffer.size());
	auxVertexBuffer.resize(oldVBSize + numVertices);
	pVertices = &auxVertexBuffer[oldVBSize];

	// get index ptr
	AuxIndexBuffer& auxIndexBuffer(AccessData()->m_auxIndexBuffer);
	AuxIndexBuffer::size_type oldIBSize(auxIndexBuffer.size());
	auxIndexBuffer.resize(oldIBSize + numIndices);
	pIndices = &auxIndexBuffer[oldIBSize];
}

void CAuxGeomCB::AddObject(SAuxDrawObjParams*& pDrawParams, const SAuxGeomRenderFlags& renderFlags)
{
	// create new push buffer entry
	AuxPushBuffer& auxPushBuffer(AccessData()->m_auxPushBuffer);
	AuxDrawObjParamBuffer& auxDrawObjParamBuffer(AccessData()->m_auxDrawObjParamBuffer);
	auxPushBuffer.emplace_back(auxDrawObjParamBuffer.size(), GetTransMatrixIndex(), GetWorldMatrixIndex(), renderFlags);

	// get draw param buffer ptr
	AuxDrawObjParamBuffer::size_type oldSize(auxDrawObjParamBuffer.size());
	auxDrawObjParamBuffer.resize(oldSize + 1);
	pDrawParams = &auxDrawObjParamBuffer[oldSize];
}

void CAuxGeomCB::SAuxGeomCBRawData::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(&m_auxPushBuffer, m_auxPushBuffer.capacity() * sizeof(AuxPushBuffer::value_type));
	pSizer->AddObject(&m_auxVertexBuffer, m_auxVertexBuffer.capacity() * sizeof(AuxVertexBuffer::value_type));
	pSizer->AddObject(&m_auxIndexBuffer, m_auxIndexBuffer.capacity() * sizeof(AuxIndexBuffer::value_type));
	pSizer->AddObject(&m_auxDrawObjParamBuffer, m_auxDrawObjParamBuffer.capacity() * sizeof(AuxDrawObjParamBuffer::value_type));
	pSizer->AddObject(m_TextMessages);
}

#endif // #if defined(ENABLE_RENDER_AUX_GEOM)

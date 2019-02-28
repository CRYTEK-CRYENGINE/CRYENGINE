// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TileGenerator.h"
#include <CryAISystem/IAIDebugRenderer.h>

namespace MNM
{
static bool IsOnScreen(const Vec3& worldPos)
{
	float x, y, z;
	if (gEnv->pRenderer->ProjectToScreen(worldPos.x, worldPos.y, worldPos.z, &x, &y, &z))
	{
		if ((z >= 0.0f) && (z <= 1.0f))
		{
			return true;
		}
	}
	return false;
}

static bool IsOnScreen(const Vec3& worldPos, Vec2& outScreenPos)
{
	float x, y, z;
	if (gEnv->pRenderer->ProjectToScreen(worldPos.x, worldPos.y, worldPos.z, &x, &y, &z))
	{
		if ((z >= 0.0f) && (z <= 1.0f))
		{
			const CCamera& camera = GetISystem()->GetViewCamera();

			outScreenPos.x = x * 0.01f * camera.GetViewSurfaceX();
			outScreenPos.y = y * 0.01f * camera.GetViewSurfaceZ();
			return true;
		}
	}
	return false;
}

// TODO pavloi 2016.03.10: IRenderAuxGeom doesn't give an easy way of printing text
static void RenderText(IRenderAuxGeom* const pRenderAuxGeom, const Vec3& pos, SDrawTextInfo& ti, const char* szFormat, ...)
{
	va_list args;
	va_start(args, szFormat);
	pRenderAuxGeom->RenderText(pos, ti, szFormat, args);
	va_end(args);
}

struct SEmptyInfoPrinter
{
	void operator()(const CompactSpanGrid::Span& /*span*/, const SSpanCoord& /*spanCoord*/, const Vec3& /*topCenter*/, IRenderAuxGeom* const /*pRenderAuxGeom*/) const
	{
		// print nothing
	}
};

template<typename TyFilter, typename TyInfoPrinter>
void DrawSpanGrid(const Vec3 volumeMin, const Vec3 voxelSize, const CompactSpanGrid& grid, const TyFilter& filter, const ColorB& color, const TyInfoPrinter& printer) PREFAST_SUPPRESS_WARNING(6262)
{
	const size_t width  = grid.GetWidth();
	const size_t height = grid.GetHeight();
	const size_t gridSize = width * height;

	IRenderAuxGeom* renderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	size_t aabbCount = 0;

	const size_t MaxAABB = 1024;
	AABB aabbs[MaxAABB];

	SAuxGeomRenderFlags oldFlags = renderAuxGeom->GetRenderFlags();
	SAuxGeomRenderFlags renderFlags(oldFlags);

	renderFlags.SetAlphaBlendMode(e_AlphaBlended);
	renderFlags.SetDepthWriteFlag(e_DepthWriteOff);

	for (size_t y = 0; y < height; ++y)
	{
		const float minY = volumeMin.y + y * voxelSize.y;

		for (size_t x = 0; x < width; ++x)
		{
			const float minX = volumeMin.x + x * voxelSize.x;
			const float minZ = volumeMin.z;

			if (const CompactSpanGrid::Cell cell = grid.GetCell(x, y))
			{
				size_t count = cell.count;

				for (size_t s = 0; s < count; ++s)
				{
					const CompactSpanGrid::Span& span = grid.GetSpan(cell.index + s);

					if (filter(grid, cell.index + s))
					{
						const AABB aabb(Vec3(minX, minY, minZ + span.bottom * voxelSize.z),
						                Vec3(minX + voxelSize.x, minY + voxelSize.y, minZ + (span.bottom + span.height) * voxelSize.z));

						aabbs[aabbCount++] = aabb;

						Vec3 topCenter(aabb.GetCenter());
						topCenter.z = aabb.max.z;
						printer(span, SSpanCoord(x, y, s, cell.index + s), topCenter, renderAuxGeom);

						if (aabbCount == MaxAABB)
						{
							renderAuxGeom->DrawAABBs(aabbs, aabbCount, true, color, eBBD_Faceted);
							aabbCount = 0;
						}
					}
				}
			}
		}
	}

	if (aabbCount)
	{
		renderAuxGeom->DrawAABBs(aabbs, aabbCount, true, color, eBBD_Faceted);
		aabbCount = 0;
	}

	renderAuxGeom->SetRenderFlags(oldFlags);
}

template<typename TyFilter>
void DrawSpanGrid(const Vec3 volumeMin, const Vec3 voxelSize, const CompactSpanGrid& grid, const TyFilter& filter, const ColorB& color) PREFAST_SUPPRESS_WARNING(6262)
{
	DrawSpanGrid(volumeMin, voxelSize, grid, filter, color, SEmptyInfoPrinter());
}

template<typename TyFilter, typename TyColorPicker, typename TyInfoPrinter>
void DrawSpanGrid(const Vec3 volumeMin, const Vec3 voxelSize, const CompactSpanGrid& grid, const TyFilter& filter,
                  const TyColorPicker& color, const TyInfoPrinter& printer)
{
	const size_t width = grid.GetWidth();
	const size_t height = grid.GetHeight();
	const size_t gridSize = width * height;

	IRenderAuxGeom* renderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

	SAuxGeomRenderFlags oldFlags = renderAuxGeom->GetRenderFlags();
	SAuxGeomRenderFlags renderFlags(oldFlags);

	renderFlags.SetAlphaBlendMode(e_AlphaBlended);
	renderFlags.SetDepthWriteFlag(e_DepthWriteOff);

	for (size_t y = 0; y < height; ++y)
	{
		const float minY = volumeMin.y + y * voxelSize.y;

		for (size_t x = 0; x < width; ++x)
		{
			const float minX = volumeMin.x + x * voxelSize.x;
			const float minZ = volumeMin.z;

			if (const CompactSpanGrid::Cell cell = grid.GetCell(x, y))
			{
				size_t count = cell.count;

				for (size_t s = 0; s < count; ++s)
				{
					const CompactSpanGrid::Span& span = grid.GetSpan(cell.index + s);

					if (filter(grid, cell.index + s))
					{
						AABB aabb(Vec3(minX, minY, minZ + span.bottom * voxelSize.z),
						          Vec3(minX + voxelSize.x, minY + voxelSize.y, minZ + (span.bottom + span.height) * voxelSize.z));

						Vec3 topCenter(aabb.GetCenter());
						topCenter.z = aabb.max.z;
						printer(span, SSpanCoord(x, y, s, cell.index + s), topCenter, renderAuxGeom);

						renderAuxGeom->DrawAABB(aabb, true, color(grid, cell.index + s), eBBD_Faceted);
					}
				}
			}
		}
	}

	renderAuxGeom->SetRenderFlags(oldFlags);
}

template<typename TyFilter, typename TyColorPicker>
void DrawSpanGrid(const Vec3 volumeMin, const Vec3 voxelSize, const CompactSpanGrid& grid, const TyFilter& filter,
                  const TyColorPicker& color)
{
	DrawSpanGrid(volumeMin, voxelSize, grid, filter, color, SEmptyInfoPrinter());
}

template<typename TyFilter, typename TyColorPicker, typename TyInfoPrinter>
void DrawSpanGridTop(const Vec3 volumeMin, const Vec3 voxelSize, const CompactSpanGrid& grid, const TyFilter& filter,
                     const TyColorPicker& color, const TyInfoPrinter& printer)
{
	const size_t width = grid.GetWidth();
	const size_t height = grid.GetHeight();
	const size_t gridSize = width * height;

	IRenderAuxGeom* renderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

	SAuxGeomRenderFlags oldFlags = renderAuxGeom->GetRenderFlags();
	SAuxGeomRenderFlags renderFlags(oldFlags);

	renderFlags.SetAlphaBlendMode(e_AlphaBlended);
	renderFlags.SetDepthWriteFlag(e_DepthWriteOff);

	for (size_t y = 0; y < height; ++y)
	{
		const float minY = volumeMin.y + y * voxelSize.y;

		for (size_t x = 0; x < width; ++x)
		{
			const float minX = volumeMin.x + x * voxelSize.x;
			const float minZ = volumeMin.z;

			if (const CompactSpanGrid::Cell cell = grid.GetCell(x, y))
			{
				size_t count = cell.count;

				for (size_t s = 0; s < count; ++s)
				{
					const CompactSpanGrid::Span& span = grid.GetSpan(cell.index + s);

					if (filter(grid, cell.index + s))
					{
						ColorB vcolor = color(grid, cell.index + s);

						Vec3 v0 = Vec3(minX, minY, minZ + (span.bottom + span.height) * voxelSize.z);
						Vec3 v1 = Vec3(v0.x, v0.y + voxelSize.y, v0.z);
						Vec3 v2 = Vec3(v0.x + voxelSize.x, v0.y + voxelSize.y, v0.z);
						Vec3 v3 = Vec3(v0.x + voxelSize.x, v0.y, v0.z);

						renderAuxGeom->DrawTriangle(v0, vcolor, v2, vcolor, v1, vcolor);
						renderAuxGeom->DrawTriangle(v0, vcolor, v3, vcolor, v2, vcolor);

						printer(span, SSpanCoord(x, y, s, cell.index + s), (v0 + v2) * 0.5f, renderAuxGeom);
					}
				}
			}
		}
	}

	renderAuxGeom->SetRenderFlags(oldFlags);
}

template<typename TyFilter, typename TyColorPicker>
void DrawSpanGridTop(const Vec3 volumeMin, const Vec3 voxelSize, const CompactSpanGrid& grid, const TyFilter& filter,
                     const TyColorPicker& color)
{
	DrawSpanGridTop(volumeMin, voxelSize, grid, filter, color, SEmptyInfoPrinter());
}

ColorB ColorFromNumber(size_t n)
{
	return ColorB(64 + (n * 64) % 192, 64 + ((n * 32) + 64) % 192, 64 + (n * 24) % 192, 255);
}

struct AllPassFilter
{
	bool operator()(const CompactSpanGrid& grid, size_t i) const
	{
		return true;
	}
};

struct NotWalkableFilter
{
	bool operator()(const CompactSpanGrid& grid, size_t i) const
	{
		const CompactSpanGrid::Span& span = grid.GetSpan(i);
		return (span.flags & CTileGenerator::NotWalkable) != 0;
	}
};

struct WalkableFilter
{
	bool operator()(const CompactSpanGrid& grid, size_t i) const
	{
		const CompactSpanGrid::Span& span = grid.GetSpan(i);
		return (span.flags & CTileGenerator::NotWalkable) == 0;
	}
};

struct BoundaryFilter
{
	bool operator()(const CompactSpanGrid& grid, size_t i) const
	{
		const CompactSpanGrid::Span& span = grid.GetSpan(i);
		return (span.flags & CTileGenerator::TileBoundary) != 0;
	}
};

struct RawColorPicker
{
	RawColorPicker(const ColorB& _normal, const ColorB& _backface)
		: normal(_normal)
		, backface(_backface)
	{}

	ColorB operator()(const CompactSpanGrid& grid, size_t i) const
	{
		const CompactSpanGrid::Span& span = grid.GetSpan(i);
		if (!span.backface)
			return normal;
		return backface;
	}
private:
	ColorB normal;
	ColorB backface;
};

struct DistanceColorPicker
{
	DistanceColorPicker(const uint16* _distances)
		: distances(_distances)
	{}

	ColorB operator()(const CompactSpanGrid& grid, size_t i) const
	{
		uint8 distance = (uint8)min<uint16>(distances[i] * 4, 255);
		return ColorB(distance, distance, distance, 255);
	}
private:
	const uint16* distances;
};

struct LabelColor
{
	ColorB operator()(size_t r) const
	{
		if (r < CTileGenerator::NoLabel)
			return ColorFromNumber(r);
		else if (r == CTileGenerator::NoLabel)
			return Col_DarkGray;
		else if ((r& CTileGenerator::BorderLabelV) == CTileGenerator::BorderLabelV)
			return Col_DarkSlateBlue;
		else if ((r& CTileGenerator::BorderLabelH) == CTileGenerator::BorderLabelH)
			return Col_DarkOliveGreen;
		else if (r & CTileGenerator::ExternalContour)
			return Col_DarkSlateBlue * 0.5f + Col_DarkOliveGreen * 0.5f;
		else if (r & CTileGenerator::InternalContour)
			return Col_DarkSlateBlue * 0.25f + Col_DarkOliveGreen * 0.75f;
		else
			return Col_DarkSlateBlue * 0.5f + Col_DarkOliveGreen * 0.5f;

		return Col_VioletRed;
	}
};

struct LabelColorPicker
{
	LabelColorPicker(const uint16* _labels)
		: labels(_labels)
	{}

	ColorB operator()(const CompactSpanGrid& grid, size_t i) const
	{
		return LabelColor()(labels[i]);
	}
private:
	const uint16* labels;
};

struct SPaintColor
{
	ColorB operator()(size_t r) const
	{
		switch (r)
		{
		case CTileGenerator::Paint::NoPaint:
			return Col_DarkGray;
		case CTileGenerator::Paint::BadPaint:
			return Col_DarkSlateBlue;
		}
		static_assert(CTileGenerator::Paint::OkPaintStart == 2, "There are unknown values before TileGenerator::Paint::OkPaintStart");
		return ColorFromNumber(r - CTileGenerator::Paint::OkPaintStart);
	}
};

struct SPaintColorPicker
{
	SPaintColorPicker(const uint16* _paints)
		: paints(_paints)
	{
	}

	ColorB operator()(const CompactSpanGrid& grid, size_t i) const
	{
		return SPaintColor()(paints[i]);
	}
private:
	const uint16* paints;
};

struct SLegendRenderer
{
	void DrawColorLegendLine(const Vec2& pos, const Vec2& size, const float fontSize, const ColorB& color, const char* szLabel)
	{
		const ColorF fontColor(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);

		// TODO pavloi 2016.03.09: why 2d quad is not working? It's hard to tell a color from text.

		const vtx_idx auxIndices[6] = { 2, 1, 0, 2, 3, 1 };
		const Vec3 quadVertices[4] = {
			Vec3(pos.x,          pos.y,          1.0f),
			Vec3(pos.x + size.x, pos.y,          1.0f),
			Vec3(pos.x,          pos.y + size.y, 1.0f),
			Vec3(pos.x + size.x, pos.y + size.y, 1.0f)
		};
		pRenderAuxGeom->DrawTriangles(quadVertices, 4, auxIndices, 6, color);
		pRenderAuxGeom->Draw2dLabel(quadVertices[1].x, quadVertices[1].y, fontSize, fontColor, false, szLabel);
	}

	SLegendRenderer()
		: pRenderAuxGeom(gEnv->pRenderer->GetIRenderAuxGeom())
		, oldRenderFlags(pRenderAuxGeom ? pRenderAuxGeom->GetRenderFlags() : SAuxGeomRenderFlags())
	{}

	~SLegendRenderer()
	{
		if (pRenderAuxGeom)
		{
			pRenderAuxGeom->SetRenderFlags(oldRenderFlags);
		}
	}

	void SetDefaultRenderFlags()
	{
		SAuxGeomRenderFlags flags = e_Def3DPublicRenderflags;
		flags.SetMode2D3DFlag(e_Mode2D);
		flags.SetAlphaBlendMode(e_AlphaBlended);
		flags.SetDrawInFrontMode(e_DrawInFrontOn);
		flags.SetDepthTestFlag(e_DepthTestOff);

		pRenderAuxGeom->SetRenderFlags(flags);
	}

	operator bool() const { return pRenderAuxGeom != nullptr; }

	IRenderAuxGeom*           pRenderAuxGeom;
	const SAuxGeomRenderFlags oldRenderFlags;
};

struct SLabelColorLegendRenderer
{
	static void DrawLegend()
	{
		const float fontSize = 1.8f;
		const float dy = 15.0f;
		const float x = 10.0f;
		float y = 120.0f;

		LabelColor colorer;
		SLegendRenderer legend;
		if (legend)
		{
			legend.DrawColorLegendLine(Vec2(x, (y += dy)), Vec2(dy, dy), fontSize, colorer(CTileGenerator::NoLabel), "NoLabel");
			legend.DrawColorLegendLine(Vec2(x, (y += dy)), Vec2(dy, dy), fontSize, colorer(CTileGenerator::BorderLabelV), "BorderLabelV");
			legend.DrawColorLegendLine(Vec2(x, (y += dy)), Vec2(dy, dy), fontSize, colorer(CTileGenerator::BorderLabelH), "BorderLabelH");
			legend.DrawColorLegendLine(Vec2(x, (y += dy)), Vec2(dy, dy), fontSize, colorer(CTileGenerator::ExternalContour), "ExternalContour");
			legend.DrawColorLegendLine(Vec2(x, (y += dy)), Vec2(dy, dy), fontSize, colorer(CTileGenerator::InternalContour), "InternalContour");
			legend.DrawColorLegendLine(Vec2(x, (y += dy)), Vec2(dy, dy), fontSize, colorer(CTileGenerator::NoLabel | CTileGenerator::InternalContour), "Something else");
		}
	}
};

struct SPaintColorLegendRenderer
{
	static void DrawLegend(const std::vector<uint16>& paints)
	{
		const float fontSize = 1.8f;
		const float dy = 15.0f;
		const float x = 10.0f;
		float y = 120.0f;

		uint16 maxPaintValue = CTileGenerator::Paint::NoPaint;
		if (!paints.empty())
		{
			auto iter = std::max_element(paints.begin(), paints.end());
			maxPaintValue = *iter;
		}

		SPaintColor colorer;
		SLegendRenderer legend;
		if (legend)
		{
			legend.DrawColorLegendLine(Vec2(x, (y += dy)), Vec2(dy, dy), fontSize, colorer(CTileGenerator::Paint::NoPaint), "NoPaint");
			legend.DrawColorLegendLine(Vec2(x, (y += dy)), Vec2(dy, dy), fontSize, colorer(CTileGenerator::Paint::BadPaint), "BadPaint");
			legend.DrawColorLegendLine(Vec2(x, (y += dy)), Vec2(dy, dy), fontSize, colorer(CTileGenerator::Paint::OkPaintStart), "OkPaintStart");
			for (uint16 paintVal = CTileGenerator::Paint::OkPaintStart, idx = 1; paintVal < maxPaintValue; ++paintVal, ++idx)
			{
				legend.DrawColorLegendLine(Vec2(x, (y += dy)), Vec2(dy, dy), fontSize, colorer(paintVal), stack_string().Format("paint #%u", (uint32)idx).c_str());
			}
		}
	}
};

#if DEBUG_MNM_GATHER_NONWALKABLE_REASONS

class CNonWalkableInfoPrinter
{
public:
	CNonWalkableInfoPrinter(const CTileGenerator::NonWalkableSpanReasonMap& nonWalkabaleSpanInfos)
		: m_nonWalkabaleSpanInfos(nonWalkabaleSpanInfos)
	{}

	void operator()(const CompactSpanGrid::Span& span, const SSpanCoord& spanCoord, const Vec3& topCenter, IRenderAuxGeom* const pRenderAuxGeom) const
	{
		const auto& iterCoordInfoPair = m_nonWalkabaleSpanInfos.find(spanCoord);
		if (iterCoordInfoPair != m_nonWalkabaleSpanInfos.end())
		{
			if (!IsOnScreen(topCenter))
			{
				return;
			}

			const CTileGenerator::SNonWalkableReason& info = iterCoordInfoPair->second;

			SDrawTextInfo ti;
			ti.scale = Vec2(0.08f);
			ti.flags = eDrawText_800x600 | eDrawText_IgnoreOverscan | eDrawText_Center | eDrawText_CenterV;

			if (info.bIsNeighbourReason)
			{
				RenderText(pRenderAuxGeom, topCenter, ti, "%" PRISIZE_T ", %" PRISIZE_T ", %" PRISIZE_T " %s\n%s",
				           spanCoord.cellX, spanCoord.cellY, spanCoord.spanIdx, info.szReason,
				           info.neighbourReason.szReason);
			}
			else
			{
				RenderText(pRenderAuxGeom, topCenter, ti, "%" PRISIZE_T ", %" PRISIZE_T ", %" PRISIZE_T " %s",
				           spanCoord.cellX, spanCoord.cellY, spanCoord.spanIdx, info.szReason);
			}
		}
	}
private:
	const CTileGenerator::NonWalkableSpanReasonMap& m_nonWalkabaleSpanInfos;
};

#endif // DEBUG_MNM_GATHER_NONWALKABLE_REASONS

class CDistanceInfoPrinter
{
public:
	CDistanceInfoPrinter(const uint16* pDistances_)
		: pDistances(pDistances_)
	{
	}

	void operator()(const CompactSpanGrid::Span& span, const SSpanCoord& spanCoord, const Vec3& topCenter, IRenderAuxGeom* const pRenderAuxGeom) const
	{
		if (IsOnScreen(topCenter))
		{
			SDrawTextInfo ti;
			ti.scale = Vec2(0.1f);
			ti.flags = eDrawText_800x600 | eDrawText_IgnoreOverscan | eDrawText_Center | eDrawText_CenterV;

			RenderText(pRenderAuxGeom, topCenter, ti, "%u", uint32(pDistances[spanCoord.spanAbsIdx]));
		}
	}
private:
	const uint16* pDistances;
};

static void DrawRawTriangulation(const std::vector<Triangle>& rawGeometryTriangles)
{
	IRenderAuxGeom* pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
	const size_t triCount = rawGeometryTriangles.size();
	if (triCount > 0)
	{
		const ColorB vcolor(Col_SlateBlue, 0.65f);
		const ColorB lcolor(Col_White);

		const Vec3 offset(0.0f, 0.0f, 0.0f);
		const Vec3 lineOffset(0.0f, 0.0f, 0.0005f);

		SAuxGeomRenderFlags oldFlags = pRenderAuxGeom->GetRenderFlags();
		SAuxGeomRenderFlags renderFlags(oldFlags);

		renderFlags.SetAlphaBlendMode(e_AlphaBlended);
		renderFlags.SetDepthWriteFlag(e_DepthWriteOff);
		pRenderAuxGeom->SetRenderFlags(renderFlags);

		for (size_t i = 0; i < triCount; ++i)
		{
			const Triangle& triangle = rawGeometryTriangles[i];
			const Vec3 v0 = triangle.v0 + offset;
			const Vec3 v1 = triangle.v1 + offset;
			const Vec3 v2 = triangle.v2 + offset;

			pRenderAuxGeom->DrawTriangle(v0, vcolor, v1, vcolor, v2, vcolor);
		}

		renderFlags.SetAlphaBlendMode(e_AlphaNone);
		renderFlags.SetDepthWriteFlag(e_DepthWriteOn);
		pRenderAuxGeom->SetRenderFlags(renderFlags);

		for (size_t i = 0; i < triCount; ++i)
		{
			const Triangle& triangle = rawGeometryTriangles[i];

			const Vec3 v0 = triangle.v0 + offset + lineOffset;
			const Vec3 v1 = triangle.v1 + offset + lineOffset;
			const Vec3 v2 = triangle.v2 + offset + lineOffset;

			pRenderAuxGeom->DrawLine(v0, lcolor, v1, lcolor);
			pRenderAuxGeom->DrawLine(v1, lcolor, v2, lcolor);
			pRenderAuxGeom->DrawLine(v2, lcolor, v0, lcolor);
		}
		pRenderAuxGeom->SetRenderFlags(oldFlags);
	}
}

class CContourRenderer
{
public:
	struct SDummyContourVertexPrinter
	{
		void operator()(IRenderAuxGeom& /*renderAuxGeom*/, const CTileGenerator::ContourVertex& /*vtx*/, const size_t /*vtxIdx*/, const Vec3& /*vtxPosWorld*/) const
		{}
	};

	struct SContourVertexNumberPrinter
	{
		void operator()(IRenderAuxGeom& renderAuxGeom, const CTileGenerator::ContourVertex& vtx, const size_t vtxIdx, const Vec3& vtxPosWorld) const
		{
			Vec2 screenPos;
			if (IsOnScreen(vtxPosWorld, *&screenPos))
			{
				ColorF textColor(Col_White);

				IRenderAuxText::Draw2dLabel(screenPos.x, screenPos.y, 1.5f, (float*)&textColor, true, "%" PRISIZE_T, vtxIdx);
			}
		}
	};

#if DEBUG_MNM_GATHER_EXTRA_CONTOUR_VERTEX_INFO
	struct SContourVertexDebugInfoPrinter
	{
		SContourVertexDebugInfoPrinter(const CTileGenerator::ContourVertexDebugInfos& infos)
			: infos(infos)
		{}

		void operator()(IRenderAuxGeom& renderAuxGeom, const CTileGenerator::ContourVertex& vtx, const size_t vtxIdx, const Vec3& vtxPosWorld) const
		{
			if (vtx.debugInfoIndex == (~0))
			{
				return;
			}

			const char* szText = nullptr;
			if (vtx.debugInfoIndex < infos.size())
			{
				const CTileGenerator::SContourVertexDebugInfo& info = infos[vtx.debugInfoIndex];

				if (IsOnScreen(vtxPosWorld))
				{
					SDrawTextInfo ti;
					ti.scale = Vec2(0.1f);
					ti.flags = eDrawText_800x600 | eDrawText_IgnoreOverscan | eDrawText_Center | eDrawText_CenterV;

					const char dirNames[4] = { 'N', 'E', 'S', 'W' };
					const char* const classNames[3] = { "UW", "NB", "WB" };

					RenderText(&renderAuxGeom, vtxPosWorld, ti,
					           "(%d;%d;%d):%d:%c\n"
					           "%s %s %s, %u %u %u %u\n"
					           "%s",
					           info.tracer.pos.x, info.tracer.pos.y, info.tracer.pos.z, info.tracerIndex, dirNames[info.tracer.dir],
					           classNames[info.lclass], classNames[info.flclass], classNames[info.fclass], info.walkableBit, info.borderBitH, info.borderBitV, info.internalBorderV,
					           info.unremovableReason.c_str());
				}
			}

		}

		const CTileGenerator::ContourVertexDebugInfos& infos;
	};
#endif // DEBUG_MNM_GATHER_EXTRA_CONTOUR_VERTEX_INFO
public:
	CContourRenderer(const Vec3& origin, const CTileGenerator::Params& params)
		: m_origin(origin)
		, m_voxelSize(params.voxelSize)
		, m_contourBoundaryVertexRadius(ContourBoundaryVertexRadius(m_voxelSize.x))
		, m_pRenderAuxGeom(gEnv->pRenderer->GetIRenderAuxGeom())
		, m_oldRenderFlags(m_pRenderAuxGeom->GetRenderFlags())
	{
		SAuxGeomRenderFlags flags = m_oldRenderFlags;
		// Note: uncomment and recode for contour without depth write
		//flags.SetAlphaBlendMode(e_AlphaBlended);
		//flags.SetDepthWriteFlag(e_DepthWriteOff);
		m_pRenderAuxGeom->SetRenderFlags(flags);
	}

	~CContourRenderer()
	{
		m_pRenderAuxGeom->SetRenderFlags(m_oldRenderFlags);
	}

	template<typename TVertexPrinter>
	void DrawRegions(const CTileGenerator::Regions& regions, const ColorB unremovableColor, const TVertexPrinter& vertexPrinter)
	{
		if (!regions.empty())
		{
			for (size_t i = 0; i < regions.size(); ++i)
			{
				const CTileGenerator::Region& region = regions[i];

				if (region.contour.empty())
					continue;

				DrawRegion(region, i, unremovableColor, vertexPrinter);
			}
		}
	}

	template<typename TVertexPrinter>
	void DrawRegion(const CTileGenerator::Region& region, const size_t regionIndex, const ColorB unremovableColor, const TVertexPrinter& vertexPrinter)
	{
		const ColorB vcolor = LabelColor()(regionIndex);

		DrawContour(region.contour, vcolor, unremovableColor, vertexPrinter);

		for (size_t h = 0; h < region.holes.size(); ++h)
		{
			const CTileGenerator::Contour& hole = region.holes[h];

			DrawContour(hole, vcolor, unremovableColor, vertexPrinter);
		}

		size_t xx = 0;
		size_t yy = 0;
		size_t zz = 0;

		for (size_t c = 0; c < region.contour.size(); ++c)
		{
			const CTileGenerator::ContourVertex& cv = region.contour[c];
			xx += cv.x;
			yy += cv.y;
			zz = std::max<size_t>(zz, cv.z);
		}

		Vec3 idLocation(xx / (float)region.contour.size(), yy / (float)region.contour.size(), (float)zz);
		idLocation = m_origin + idLocation.CompMul(m_voxelSize);

		Vec2 screenPos;
		if (IsOnScreen(idLocation, *&screenPos))
		{
			ColorF textColor(vcolor.pack_abgr8888());

			IRenderAuxText::Draw2dLabel(screenPos.x, screenPos.y, 1.8f, (float*)&textColor, true, "%" PRISIZE_T, regionIndex);
			IRenderAuxText::Draw2dLabel(screenPos.x, screenPos.y + 1.6f * 10.0f, 1.1f, (float*)&textColor, true,
			                             "spans: %" PRISIZE_T " holes: %" PRISIZE_T, region.spanCount, region.holes.size());
		}
	}

	template<typename TVertexPrinter>
	void DrawContour(const CTileGenerator::Contour& contour, const ColorB vcolor, const ColorB unremovableColor, const TVertexPrinter& printer) const
	{
		const CTileGenerator::ContourVertex& v = contour.front();

		Vec3 v0 = ContourVertexWorldPos(v);

		for (size_t s = 1; s <= contour.size(); ++s)
		{
			const size_t vtxIndex = s % contour.size();
			const CTileGenerator::ContourVertex& nv = contour[vtxIndex];
			const ColorB pcolor = (nv.flags & CTileGenerator::ContourVertex::Unremovable) ? unremovableColor : vcolor;

			const Vec3 v1 = ContourVertexWorldPos(nv);

			m_pRenderAuxGeom->DrawLine(v0, vcolor, v1, vcolor, 7.0f);

			if (nv.flags & CTileGenerator::ContourVertex::TileBoundary)
			{
				m_pRenderAuxGeom->DrawCone(v1, Vec3(0.0f, 0.0f, 1.0f), m_contourBoundaryVertexRadius, 0.085f, pcolor);

				if (nv.flags & CTileGenerator::ContourVertex::TileBoundaryV)
					m_pRenderAuxGeom->DrawAABB(AABB(v1, m_contourBoundaryVertexRadius), IDENTITY, true, pcolor, eBBD_Faceted);
			}
			else if (nv.flags & CTileGenerator::ContourVertex::TileBoundaryV)
				m_pRenderAuxGeom->DrawAABB(AABB(v1, m_contourBoundaryVertexRadius), IDENTITY, true, pcolor, eBBD_Faceted);
			else
				m_pRenderAuxGeom->DrawSphere(v1, m_contourBoundaryVertexRadius, pcolor);

			printer(*m_pRenderAuxGeom, nv, vtxIndex, v1);

			v0 = v1;
		}
	}

	Vec3 ContourVertexWorldPos(const CTileGenerator::ContourVertex& vtx) const
	{
		return m_origin + Vec3(
		  vtx.x * m_voxelSize.x,
		  vtx.y * m_voxelSize.y,
		  vtx.z * m_voxelSize.z);
	}

	static float ContourBoundaryVertexRadius(float voxelSizeX)
	{
		return std::min(voxelSizeX * 0.3f, 0.03f);
	}

private:
	const Vec3          m_origin;
	const Vec3          m_voxelSize;
	const float         m_contourBoundaryVertexRadius;
	IRenderAuxGeom*     m_pRenderAuxGeom;
	SAuxGeomRenderFlags m_oldRenderFlags;
};

void CTileGenerator::Draw(const EDrawMode mode, const bool bDrawAdditionalInfo) const
{
#if DEBUG_MNM_ENABLED

	const size_t border = BorderSizeH(m_params);
	const size_t borderV = BorderSizeV(m_params);
	const Vec3 origin = Vec3(
	  m_params.origin.x - m_params.voxelSize.x * border,
	  m_params.origin.y - m_params.voxelSize.y * border,
	  m_params.origin.z - m_params.voxelSize.z * borderV);

	const float blinking = 0.25f + 0.75f * fabs_tpl(sin_tpl(gEnv->pTimer->GetCurrTime() * gf_PI));
	const ColorB red = (Col_Red * blinking) + (Col_VioletRed * (1.0f - blinking));

	const AABB tileVolume(m_params.origin, m_params.origin + Vec3(m_params.sizeX, m_params.sizeY, m_params.sizeZ));
	gAIEnv.GetDebugRenderer()->DrawAABB(tileVolume, false, Col_Red, eBBD_Faceted);
	gAIEnv.GetDebugRenderer()->DrawAABB(m_debugVoxelizedVolume, false, Col_DarkOliveGreen, eBBD_Faceted);

	switch (mode)
	{
	case EDrawMode::DrawNone:
	default:
		break;
	case EDrawMode::DrawRawInputGeometry:
		{
			DrawRawTriangulation(m_debugRawGeometry);
		}
		break;

	case EDrawMode::DrawRawVoxels:
		if (m_spanGridRaw.GetSpanCount())
		{
			MNM::DrawSpanGrid(origin, m_params.voxelSize, m_spanGridRaw
			                  , AllPassFilter(), RawColorPicker(Col_LightGray, Col_DarkGray));
		}
		break;

	case EDrawMode::DrawFilteredVoxels:
		if (m_spanGrid.GetSpanCount())
		{
			MNM::DrawSpanGrid(origin, m_params.voxelSize, m_spanGrid
			                  , AllPassFilter(), RawColorPicker(Col_LightGray, Col_DarkGray));
		}
		break;

	case EDrawMode::DrawFlaggedVoxels:
		if (m_spanGridFlagged.GetSpanCount())
		{
			if (bDrawAdditionalInfo)
			{
	#if DEBUG_MNM_GATHER_NONWALKABLE_REASONS
				MNM::DrawSpanGrid(origin, m_params.voxelSize, m_spanGridFlagged, NotWalkableFilter(), ColorB(Col_VioletRed), CNonWalkableInfoPrinter(m_debugNonWalkableReasonMap));
	#endif
			}
			else
			{
				MNM::DrawSpanGrid(origin, m_params.voxelSize, m_spanGridFlagged, NotWalkableFilter(), ColorB(Col_VioletRed));
			}
			MNM::DrawSpanGrid(origin, m_params.voxelSize, m_spanGridFlagged, WalkableFilter(), ColorB(Col_SlateBlue));
			MNM::DrawSpanGrid(origin, m_params.voxelSize, m_spanGridFlagged, BoundaryFilter(), ColorB(Col_NavyBlue, 0.5f));
		}
		break;
	case EDrawMode::DrawDistanceTransform:
		if (!m_distances.empty())
		{
			if (bDrawAdditionalInfo)
			{
				DrawSpanGridTop(origin, m_params.voxelSize, m_spanGrid, AllPassFilter(), DistanceColorPicker(&m_distances.front()), CDistanceInfoPrinter(&m_distances.front()));
			}
			else
			{
				DrawSpanGridTop(origin, m_params.voxelSize, m_spanGrid, AllPassFilter(), DistanceColorPicker(&m_distances.front()));
			}
		}
		break;

	case EDrawMode::DrawPainting:
		if (!m_paint.empty())
		{
			DrawSpanGridTop(origin, m_params.voxelSize, m_spanGrid, AllPassFilter(), SPaintColorPicker(&m_paint.front()));
			if (bDrawAdditionalInfo)
			{
				SPaintColorLegendRenderer::DrawLegend(m_paint);
			}
		}
		break;

	case EDrawMode::DrawSegmentation:
		DrawSegmentation(origin, bDrawAdditionalInfo);
		break;

	case EDrawMode::DrawNumberedContourVertices:
		{
			CContourRenderer contourRenderer(origin, m_params);
			contourRenderer.DrawRegions(m_regions, red, CContourRenderer::SContourVertexNumberPrinter());
		}
		break;

	case EDrawMode::DrawContourVertices:
		{
			CContourRenderer contourRenderer(origin, m_params);
	#if DEBUG_MNM_GATHER_EXTRA_CONTOUR_VERTEX_INFO
			if (bDrawAdditionalInfo)
			{
				DrawSegmentation(origin + Vec3(0, 0, -0.01f), false);
				contourRenderer.DrawRegions(m_regions, red, CContourRenderer::SContourVertexDebugInfoPrinter(m_debugContourVertexDebugInfos));
				DrawTracers(origin + Vec3(0, 0, 0.003f));
				// Note: uncomment and recode to show even vertices, which were discarded during contour creation
				//contourRenderer.DrawContour(m_debugDiscardedVertices, Col_Blue, red, CContourRenderer::SContourVertexDebugInfoPrinter(m_debugContourVertexDebugInfos));
			}
			else
	#endif // DEBUG_MNM_GATHER_EXTRA_CONTOUR_VERTEX_INFO
			{
				contourRenderer.DrawRegions(m_regions, red, CContourRenderer::SDummyContourVertexPrinter());
			}
		}
		break;

	case EDrawMode::DrawTracers:
		{
			DrawTracers(origin);
		}
		break;

	case EDrawMode::DrawSimplifiedContours:
		{
			DrawSimplifiedContours(origin);

		}
		break;
	case EDrawMode::DrawTriangulation:
		{
			DrawNavTriangulation();
		}
		break;
	case EDrawMode::DrawBVTree:
		{
			DrawNavTriangulation();

			IRenderAuxGeom* renderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

			for (size_t i = 0; i < m_bvtree.size(); ++i)
			{
				const Tile::SBVNode& node = m_bvtree[i];

				AABB nodeAABB;

				nodeAABB.min = m_params.origin + node.aabb.min.GetVec3();
				nodeAABB.max = m_params.origin + node.aabb.max.GetVec3();

				renderAuxGeom->DrawAABB(nodeAABB, IDENTITY, false, Col_White, eBBD_Faceted);
			}
		}
		break;
	}

#endif // DEBUG_MNM_ENABLED
}

void CTileGenerator::DrawSegmentation(const Vec3& origin, const bool bDrawAdditionalInfo) const
{
	if (!m_labels.empty())
	{
		DrawSpanGridTop(origin, m_params.voxelSize, m_spanGrid, WalkableFilter(), LabelColorPicker(&m_labels.front()));
		if (bDrawAdditionalInfo)
		{
			SLabelColorLegendRenderer::DrawLegend();
		}
	}

	for (size_t i = 0; i < m_regions.size(); ++i)
	{
		const Region& region = m_regions[i];

		if (region.contour.empty())
			continue;

		size_t xx = 0;
		size_t yy = 0;
		size_t zz = 0;

		for (size_t c = 0; c < region.contour.size(); ++c)
		{
			const ContourVertex& v = region.contour[c];
			xx += v.x;
			yy += v.y;
			zz = std::max<size_t>(zz, v.z);
		}

		Vec3 idLocation(xx / (float)region.contour.size(), yy / (float)region.contour.size(), (float)zz);
		idLocation = origin + idLocation.CompMul(m_params.voxelSize);

		Vec2 screenPos;
		if (IsOnScreen(idLocation, *&screenPos))
		{
			const ColorB vcolor = LabelColor()(i);
			ColorF textColor(vcolor.pack_abgr8888());

			IRenderAuxText::Draw2dLabel(screenPos.x, screenPos.y, 1.8f, (float*)&textColor, true, "%" PRISIZE_T, i);
			IRenderAuxText::Draw2dLabel(screenPos.x, screenPos.y + 1.6f * 10.0f, 1.1f, (float*)&textColor, true,
			                             "spans: %" PRISIZE_T " holes: %" PRISIZE_T, region.spanCount, region.holes.size());
		}
	}
}

void CTileGenerator::DrawTracers(const Vec3& origin) const
{
	if (IRenderAuxGeom* pRender = gEnv->pRenderer->GetIRenderAuxGeom())
	{
		const SAuxGeomRenderFlags old = pRender->GetRenderFlags();
		SAuxGeomRenderFlags flags;
		flags.SetAlphaBlendMode(e_AlphaBlended);
		flags.SetDepthWriteFlag(e_DepthWriteOff);

		pRender->SetRenderFlags(flags);
		{
			const Vec3 corners[4] =
			{
				Vec3(-m_params.voxelSize.x, +m_params.voxelSize.y, 0.f) * 0.5f,
				Vec3(+m_params.voxelSize.x, +m_params.voxelSize.y, 0.f) * 0.5f,
				Vec3(+m_params.voxelSize.x, -m_params.voxelSize.y, 0.f) * 0.5f,
				Vec3(-m_params.voxelSize.x, -m_params.voxelSize.y, 0.f) * 0.5f
			};
			const ColorF cols[4] =
			{
				Col_Red, Col_Yellow, Col_Green, Col_Blue
			};
			const int numPaths = m_tracerPaths.size();
			for (int i = 0; i < numPaths; i++)
			{
				const TracerPath& path = m_tracerPaths[i];
				const int numTracers = path.steps.size();
				for (int j = 0; j < numTracers; j++)
				{
					const Tracer& tracer = path.steps[j];
					const Vec3 mid(origin + (m_params.voxelSize.CompMul(tracer.pos)));
					const size_t corner1 = (tracer.dir) % 4;
					const size_t corner2 = (tracer.dir + 3) % 4;
					pRender->DrawTriangle(mid, Col_White, mid + corners[corner1], cols[tracer.dir], mid + corners[corner2], cols[tracer.dir]);
				}
			}
		}
		pRender->SetRenderFlags(old);
	}
}

void CTileGenerator::DrawSimplifiedContours(const Vec3& origin) const
{
	IRenderAuxGeom* renderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

	for (size_t i = 0; i < m_polygons.size(); ++i)
	{
		const PolygonContour& contour = m_polygons[i].contour;

		if (contour.empty())
			continue;

		const ColorB vcolor = LabelColor()(i);

		const PolygonVertex& pv = contour[contour.size() - 1];

		Vec3 v0(origin.x + pv.x * m_params.voxelSize.x,
		        origin.y + pv.y * m_params.voxelSize.y,
		        origin.z + pv.z * m_params.voxelSize.z);

		for (size_t s = 0; s < contour.size(); ++s)
		{
			const PolygonVertex& nv = contour[s];
			const Vec3 v1(origin.x + nv.x * m_params.voxelSize.x, origin.y + nv.y * m_params.voxelSize.y, origin.z + nv.z * m_params.voxelSize.z);

			renderAuxGeom->DrawLine(v0, vcolor, v1, vcolor, 8.0f);
			renderAuxGeom->DrawSphere(v1, 0.025f, vcolor);
			IRenderAuxText::DrawLabelF(v1, 1.8f, "%d", (int)s);

			v0 = v1;
		}

		{
			for (size_t h = 0; h < m_polygons[i].holes.size(); ++h)
			{
				const PolygonContour& hole = m_polygons[i].holes[h].verts;
				const PolygonVertex& hv = hole.front();

				float holeHeightRaise = 0.05f;

				v0(origin.x + hv.x * m_params.voxelSize.x,
				   origin.y + hv.y * m_params.voxelSize.y,
				   (origin.z + hv.z * m_params.voxelSize.z) + holeHeightRaise);

				for (size_t s = 1; s <= hole.size(); ++s)
				{
					const PolygonVertex& nv = hole[s % hole.size()];
					const ColorB pcolor = vcolor;

					Vec3 v1(origin.x + nv.x * m_params.voxelSize.x,
					        origin.y + nv.y * m_params.voxelSize.y,
					        (origin.z + nv.z * m_params.voxelSize.z) + holeHeightRaise);

					renderAuxGeom->DrawLine(v0, vcolor, v1, vcolor, 7.0f);
					renderAuxGeom->DrawCylinder(v1, Vec3(0.0f, 0.0f, 1.0f), 0.025f, 0.04f, pcolor);

					v0 = v1;
				}
			}
		}

	}
}

void CTileGenerator::DrawNavTriangulation() const
{
	IRenderAuxGeom* renderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

	const CGeneratedMesh::Triangles& triangles = m_mesh.GetTriangles();
	const CGeneratedMesh::Vertices& vertices = m_mesh.GetVertices();

	const size_t triCount = triangles.size();

	const ColorB vcolor(Col_SlateBlue, 0.65f);
	const ColorB lcolor(Col_White);
	const ColorB bcolor(Col_Black);

	const Vec3 offset(0.0f, 0.0f, 0.05f);
	const Vec3 loffset(0.0f, 0.0f, 0.0005f);

	SAuxGeomRenderFlags oldFlags = renderAuxGeom->GetRenderFlags();
	SAuxGeomRenderFlags renderFlags(oldFlags);

	renderFlags.SetAlphaBlendMode(e_AlphaBlended);
	renderFlags.SetDepthWriteFlag(e_DepthWriteOff);

	renderAuxGeom->SetRenderFlags(renderFlags);

	for (size_t i = 0; i < triCount; ++i)
	{
		const Tile::STriangle& triangle = triangles[i];
		const Vec3 v0 = vertices[triangle.vertex[0]].GetVec3() + m_params.origin + offset;
		const Vec3 v1 = vertices[triangle.vertex[1]].GetVec3() + m_params.origin + offset;
		const Vec3 v2 = vertices[triangle.vertex[2]].GetVec3() + m_params.origin + offset;

		renderAuxGeom->DrawTriangle(v0, vcolor, v1, vcolor, v2, vcolor);
	}

	renderAuxGeom->SetRenderFlags(oldFlags);

	{
		for (size_t i = 0; i < triCount; ++i)
		{
			const Tile::STriangle& triangle = triangles[i];

			const Vec3 v0 = vertices[triangle.vertex[0]].GetVec3() + m_params.origin + offset;
			const Vec3 v1 = vertices[triangle.vertex[1]].GetVec3() + m_params.origin + offset;
			const Vec3 v2 = vertices[triangle.vertex[2]].GetVec3() + m_params.origin + offset;

			renderAuxGeom->DrawLine(v0 + loffset, lcolor, v1 + loffset, lcolor);
			renderAuxGeom->DrawLine(v1 + loffset, lcolor, v2 + loffset, lcolor);
			renderAuxGeom->DrawLine(v2 + loffset, lcolor, v0 + loffset, lcolor);
		}
	}
}

} // namespace MNM

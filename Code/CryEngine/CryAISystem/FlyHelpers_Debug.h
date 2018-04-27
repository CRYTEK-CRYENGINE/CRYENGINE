// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FLY_HELPERS__DEBUG__H__
#define __FLY_HELPERS__DEBUG__H__

namespace FlyHelpers
{
void DrawDebugLocation(const Vec3& position, const ColorB& color)
{
	gEnv->pRenderer->GetIRenderAuxGeom()->DrawCone(position + Vec3(0, 0, 2), Vec3(0, 0, -1), 1, 2, color);
}

void DrawDebugPath(const Path& path, const ColorB& color)
{
	char buffer[16];
	const size_t segmentCount = path.GetSegmentCount();
	for (size_t i = 0; i < segmentCount; ++i)
	{
		const Lineseg segment = path.GetSegment(i);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(segment.start, color, segment.end, color);

		cry_sprintf(buffer, "%" PRISIZE_T, i);
		IRenderAuxText::DrawLabel(segment.start, 1.0f, buffer);
	}
}
}

#endif

// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Schematyc2
{
	namespace GdiplusUtils
	{
		void CreateBeveledRectPath(const Gdiplus::RectF& rect, const float (&bevels)[4], Gdiplus::GraphicsPath& path);
		void PaintBeveledRect(Gdiplus::Graphics& graphics, const Gdiplus::RectF& rect, const float (&bevels)[4], const Gdiplus::Pen& pen);
		void FillBeveledRect(Gdiplus::Graphics& graphics, const Gdiplus::RectF& rect, const float (&bevels)[4], const Gdiplus::Brush& brush);
		Gdiplus::Color CreateColor(const ColorB& color);
		Gdiplus::Color LerpColor(Gdiplus::Color from, Gdiplus::Color to, float lerp);
	}
}
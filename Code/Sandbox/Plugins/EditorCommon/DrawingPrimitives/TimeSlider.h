// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QRect>

class QPainter;
class QPalette;

namespace DrawingPrimitives
{
struct STimeSliderOptions
{
	QRect m_rect;
	int   m_precision;
	int   m_position;
	float m_time;
	bool  m_bHasFocus;
};

void DrawTimeSlider(QPainter& painter, const STimeSliderOptions& options);
}


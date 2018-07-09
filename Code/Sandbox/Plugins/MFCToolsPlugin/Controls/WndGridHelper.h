// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __WndGridHelper_h__
#define __WndGridHelper_h__
#pragma once

//////////////////////////////////////////////////////////////////////////
class CWndGridHelper
{
public:
	Vec2   zoom;
	Vec2   origin;
	Vec2   step;
	Vec2   pixelsPerGrid;
	int    nMajorLines;
	CRect  rect;
	CPoint nMinPixelsPerGrid;
	CPoint nMaxPixelsPerGrid;
	//////////////////////////////////////////////////////////////////////////
	CPoint firstGridLine;
	CPoint numGridLines;

	//////////////////////////////////////////////////////////////////////////
	CWndGridHelper()
	{
		zoom = Vec2(1, 1);
		step = Vec2(10, 10);
		pixelsPerGrid = Vec2(10, 10);
		origin = Vec2(0, 0);
		nMajorLines = 10;
		nMinPixelsPerGrid = CPoint(50, 10);
		nMaxPixelsPerGrid = CPoint(100, 20);
		firstGridLine = CPoint(0, 0);
		numGridLines = CPoint(0, 0);
	}

	//////////////////////////////////////////////////////////////////////////
	Vec2 ClientToWorld(CPoint point)
	{
		Vec2 v;
		v.x = (point.x - rect.left) / zoom.x + origin.x;
		v.y = (point.y - rect.top) / zoom.y + origin.y;
		return v;
	}
	//////////////////////////////////////////////////////////////////////////
	CPoint WorldToClient(Vec2 v)
	{
		CPoint p;
		p.x = floor((v.x - origin.x) * zoom.x + 0.5f) + rect.left;
		p.y = floor((v.y - origin.y) * zoom.y + 0.5f) + rect.top;
		return p;
	}

	void SetOrigin(Vec2 neworigin)
	{
		origin = neworigin;
	}
	void SetZoom(Vec2 newzoom)
	{
		zoom = newzoom;
	}
	//////////////////////////////////////////////////////////////////////////
	void SetZoom(Vec2 newzoom, CPoint center)
	{
		if (newzoom.x < 0.01f)
			newzoom.x = 0.01f;
		if (newzoom.y < 0.01f)
			newzoom.y = 0.01f;

		Vec2 prevz = zoom;

		// Zoom to mouse position.
		float ofsx = origin.x;
		float ofsy = origin.y;

		Vec2 z1 = zoom;
		Vec2 z2 = newzoom;

		zoom = newzoom;

		// Calculate new offset to center zoom on mouse.
		float x2 = center.x - rect.left;
		float y2 = center.y - rect.top;
		ofsx = -(x2 / z2.x - x2 / z1.x - ofsx);
		ofsy = -(y2 / z2.y - y2 / z1.y - ofsy);
		origin.x = ofsx;
		origin.y = ofsy;
	}
	void CalculateGridLines()
	{
		pixelsPerGrid.x = zoom.x;
		pixelsPerGrid.y = zoom.y;

		step = Vec2(1.00f, 1.00f);
		nMajorLines = 2;

		int griditers;
		if (pixelsPerGrid.x <= nMinPixelsPerGrid.x)
		{
			griditers = 0;
			while (pixelsPerGrid.x <= nMinPixelsPerGrid.x && griditers++ < 1000)
			{
				step.x = step.x * nMajorLines;
				pixelsPerGrid.x = step.x * zoom.x;
			}
		}
		else
		{
			griditers = 0;
			while (pixelsPerGrid.x >= nMaxPixelsPerGrid.x && griditers++ < 1000)
			{
				step.x = step.x / nMajorLines;
				pixelsPerGrid.x = step.x * zoom.x;
			}
		}

		if (pixelsPerGrid.y <= nMinPixelsPerGrid.y)
		{
			griditers = 0;
			while (pixelsPerGrid.y <= nMinPixelsPerGrid.y && griditers++ < 1000)
			{
				step.y = step.y * nMajorLines;
				pixelsPerGrid.y = step.y * zoom.y;
			}
		}
		else
		{
			griditers = 0;
			while (pixelsPerGrid.y >= nMaxPixelsPerGrid.y && griditers++ < 1000)
			{
				step.y = step.y / nMajorLines;
				pixelsPerGrid.y = step.y * zoom.y;
			}

		}

		firstGridLine.x = origin.x / step.x;
		firstGridLine.y = origin.y / step.y;

		numGridLines.x = ((rect.right - rect.left) / zoom.x) / step.x + 1;
		numGridLines.y = ((rect.bottom - rect.top) / zoom.y) / step.y + 1;
	}
	int GetGridLineX(int nGridLineX) const
	{
		return floor((nGridLineX * step.x - origin.x) * zoom.x + 0.5f);
	}
	int GetGridLineY(int nGridLineY) const
	{
		return floor((nGridLineY * step.y - origin.y) * zoom.y + 0.5f);
	}
	float GetGridLineXValue(int nGridLineX) const
	{
		return (nGridLineX * step.x);
	}
	float GetGridLineYValue(int nGridLineY) const
	{
		return (nGridLineY * step.y);
	}
};

#endif // __WndGridHelper_h__


// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __COLORCHARTBASE_H__
#define __COLORCHARTBASE_H__


#include "ColorChart.h"


class ImageObject;


const int CHART_IMAGE_WIDTH = 78;
const int CHART_IMAGE_HEIGHT = 66;


class CColorChartBase : public IColorChart
{
public:
	// IColorChart interface
	virtual void Release() = 0;
	virtual void GenerateDefault() = 0;
	virtual bool GenerateFromInput(ImageObject* pImg);
	virtual ImageObject* GenerateChartImage() = 0;

protected:
	CColorChartBase();
	virtual ~CColorChartBase();

	bool ExtractFromImage(ImageObject* pImg);
	virtual bool ExtractFromImageAt(ImageObject* pImg, uint32 x, uint32 y) = 0;
};


#endif //__COLORCHARTBASE_H__

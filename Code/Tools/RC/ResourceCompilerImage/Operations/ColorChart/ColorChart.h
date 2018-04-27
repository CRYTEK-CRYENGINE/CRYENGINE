// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __COLORCHART_H__
#define __COLORCHART_H__


class ImageObject;


struct IColorChart
{
public:
	virtual void Release() = 0;
	virtual void GenerateDefault() = 0;
	virtual bool GenerateFromInput(ImageObject* pImg) = 0;
	virtual ImageObject* GenerateChartImage() = 0;

protected:
	virtual ~IColorChart() {};
};


IColorChart* Create3dLutColorChart();
IColorChart* CreateLeastSquaresLinColorChart();

inline uint32 GetAt(uint32 x, uint32 y, void* pData, uint32 pitch)
{
	uint32* p = (uint32*) ((size_t) pData + pitch * y + x * sizeof(uint32));
	return *p;
}

#endif //__COLORCHART_H__

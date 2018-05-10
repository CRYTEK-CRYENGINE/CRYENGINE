// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "../../ImageObject.h"

#include "ColorChartBase.h"

//////////////////////////////////////////////////////////////////////////
// helper

class C3dLutData
{
public:
	enum EPrimaryShades
	{
		ePS_Red   = 16,
		ePS_Green = 16,
		ePS_Blue  = 16,

		ePS_NumColors = ePS_Red * ePS_Green * ePS_Blue
	};

	struct SColor
	{
		unsigned char r, g, b, _padding;
	};

	typedef std::vector<SColor> ColorMapping;

public:
	C3dLutData();
	explicit C3dLutData(const ColorMapping& mapping);
	~C3dLutData();

	bool SetColorChartData(const ColorMapping& mapping);
	const ColorMapping& GetMapping() const;

private:
	ColorMapping m_mapping;
};


C3dLutData::C3dLutData()
: m_mapping()
{
}


C3dLutData::C3dLutData(const ColorMapping& mapping)
: m_mapping(mapping)
{
}


C3dLutData::~C3dLutData()
{
}


bool C3dLutData::SetColorChartData(const ColorMapping& mapping)
{
	if (mapping.size() != ePS_NumColors)
		return false;

	m_mapping = mapping;
	return true;
}


const C3dLutData::ColorMapping& C3dLutData::GetMapping() const
{
	return m_mapping;
}

//////////////////////////////////////////////////////////////////////////
// C3dLutColorChart impl

class C3dLutColorChart : public CColorChartBase
{
public:
	C3dLutColorChart();
	virtual ~C3dLutColorChart();

	virtual void Release();
	virtual void GenerateDefault();
	virtual ImageObject* GenerateChartImage();

protected:
	virtual bool ExtractFromImageAt(ImageObject* pImg, uint32 x, uint32 y);

private:
	C3dLutData m_data;
};


C3dLutColorChart::C3dLutColorChart()
	: CColorChartBase()
	, m_data()
{
}


C3dLutColorChart::~C3dLutColorChart()
{
}


void C3dLutColorChart::Release()
{
	delete this;
}


void C3dLutColorChart::GenerateDefault()
{
	C3dLutData::ColorMapping mapping;
	mapping.reserve(C3dLutData::ePS_NumColors);

	C3dLutData::SColor col;
	col._padding = 0;

	for (int b = 0; b < C3dLutData::ePS_Blue; ++b)
	{
		for (int g = 0; g < C3dLutData::ePS_Green; ++g)
		{
			for (int r = 0; r < C3dLutData::ePS_Red; ++r)
			{
				col.r = 255 * r / (C3dLutData::ePS_Red);
				col.g = 255 * g / (C3dLutData::ePS_Green);
				col.b = 255 * b / (C3dLutData::ePS_Blue);
				int l = 255 - (col.r * 3 + col.g * 6 + col.b) / 10;
				col.r = col.g = col.b = (unsigned char) l;
				mapping.push_back(col);
			}
		}
	}

	bool res = m_data.SetColorChartData(mapping);
	assert(res);
}


ImageObject* C3dLutColorChart::GenerateChartImage()
{
	const uint32 mipCount = 1;
	std::auto_ptr<ImageObject> pImg(new ImageObject(C3dLutData::ePS_Red * C3dLutData::ePS_Blue, C3dLutData::ePS_Green, mipCount, ePixelFormat_A8R8G8B8, ImageObject::eCubemap_No));

	{
		char* pData;
		uint32 pitch;
		pImg->GetImagePointer( 0, pData, pitch);

		assert(pitch % C3dLutData::ePS_Blue == 0);
		size_t nSlicePitch = (pitch / C3dLutData::ePS_Blue);
		const C3dLutData::ColorMapping& mapping = m_data.GetMapping();
		uint32 src = 0;
		uint32 dst = 0;
		for (int b = 0; b < C3dLutData::ePS_Blue; ++b)
		{
			for (int g = 0; g < C3dLutData::ePS_Green; ++g)
			{
				uint32* p = (uint32*) ((size_t) pData + g * pitch + b * nSlicePitch );
				for (int r = 0; r < C3dLutData::ePS_Red; ++r)
				{
					const C3dLutData::SColor& c = mapping[src];
					*p = (c.r << 16) | (c.g << 8) | c.b;
					++src;
					++p;
				}
			}
		}
	}

	return pImg.release();
}

bool C3dLutColorChart::ExtractFromImageAt(ImageObject* pImg, uint32 x, uint32 y)
{
	assert(pImg);

	int ox = x + 1;
	int oy = y + 1;

	char* pData;
	uint32 pitch;
	pImg->GetImagePointer( 0, pData, pitch);

	C3dLutData::ColorMapping mapping;
	mapping.reserve(C3dLutData::ePS_NumColors);

	C3dLutData::SColor col;
	col._padding = 0;

	for (int b = 0; b < C3dLutData::ePS_Blue; ++b)
	{
		int px = ox + C3dLutData::ePS_Red * (b % 4);
		int py = oy + C3dLutData::ePS_Green * (b / 4);

		for (int g = 0; g < C3dLutData::ePS_Green; ++g)
		{
			for (int r = 0; r < C3dLutData::ePS_Red; ++r)
			{
				uint32 c = GetAt(px+r, py+g, pData, pitch);
				
				col.r = (c >> 16) & 0xFF;
				col.g = (c >>  8) & 0xFF;
				col.b = (c      ) & 0xFF;
				mapping.push_back(col);
			}
		}
	}

	return m_data.SetColorChartData(mapping);
}

//////////////////////////////////////////////////////////////////////////
// C3dLutColorChart factory

IColorChart* Create3dLutColorChart()
{
	return new C3dLutColorChart();
}

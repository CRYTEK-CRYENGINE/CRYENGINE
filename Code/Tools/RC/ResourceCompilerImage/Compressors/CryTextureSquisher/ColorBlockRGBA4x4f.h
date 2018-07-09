// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef ColorBlockRGBA4x4f_H
#define ColorBlockRGBA4x4f_H

#include "ColorTypes.h"

// Uncompressed 4x4 color block of single precision floating points.
struct ColorBlockRGBA4x4f
{
	ColorBlockRGBA4x4f()
	{
	}

	void setRGBAf(const void* imgARGBf, unsigned int width, unsigned int heigth, unsigned int pitch, unsigned int x, unsigned int y);
	void getRGBAf(      void* imgARGBf, unsigned int width, unsigned int heigth, unsigned int pitch, unsigned int x, unsigned int y);
	
	void setAf(const void* imgAf, unsigned int width, unsigned int heigth, unsigned int pitch, unsigned int x, unsigned int y);
	void getAf(      void* imgAf, unsigned int width, unsigned int heigth, unsigned int pitch, unsigned int x, unsigned int y);

	const ColorRGBAf* colors() const
	{
		return m_color;
	}

	ColorRGBAf* colors()
	{
		return m_color;
	}

	ColorRGBAf color(unsigned int i) const
	{
		return m_color[i];
	}

	ColorRGBAf& color(unsigned int i)
	{
		return m_color[i];
	}

private:
	static const unsigned int COLOR_COUNT = 4*4;

	ColorRGBAf m_color[COLOR_COUNT];
};

#endif

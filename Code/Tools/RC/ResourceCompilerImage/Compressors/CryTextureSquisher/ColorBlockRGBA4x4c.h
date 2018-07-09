// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef ColorBlockRGBA4x4_H
#define ColorBlockRGBA4x4_H

#include "ColorTypes.h"

// Uncompressed 4x4 color block of 8bit integers.
struct ColorBlockRGBA4x4c
{
	ColorBlockRGBA4x4c()
	{
	}

	void setBGRA8(const void* imgBGRA8, unsigned int width, unsigned int heigth, unsigned int pitch, unsigned int x, unsigned int y);
	void getBGRA8(      void* imgBGRA8, unsigned int width, unsigned int heigth, unsigned int pitch, unsigned int x, unsigned int y);
	
	void setA8(const void* imgA8, unsigned int width, unsigned int heigth, unsigned int pitch, unsigned int x, unsigned int y);
	void getA8(      void* imgA8, unsigned int width, unsigned int heigth, unsigned int pitch, unsigned int x, unsigned int y);

	bool isSingleColorIgnoringAlpha() const;

	const ColorRGBA8* colors() const
	{
		return m_color;
	}

	ColorRGBA8* colors()
	{
		return m_color;
	}

	ColorRGBA8 color(unsigned int i) const
	{
		return m_color[i];
	}

	ColorRGBA8& color(unsigned int i)
	{
		return m_color[i];
	}

private:
	static const unsigned int COLOR_COUNT = 4*4;

	ColorRGBA8 m_color[COLOR_COUNT];
};

#endif

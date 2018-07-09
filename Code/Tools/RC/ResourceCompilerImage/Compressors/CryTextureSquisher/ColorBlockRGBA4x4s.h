// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef ColorBlockRGBA4x4s_H
#define ColorBlockRGBA4x4s_H

#include "ColorTypes.h"

// Uncompressed 4x4 color block of 16bit integers.
struct ColorBlockRGBA4x4s
{
	ColorBlockRGBA4x4s()
	{
	}

	void setBGRA16(const void* imgBGRA16, unsigned int width, unsigned int heigth, unsigned int pitch, unsigned int x, unsigned int y);
	void getBGRA16(      void* imgBGRA16, unsigned int width, unsigned int heigth, unsigned int pitch, unsigned int x, unsigned int y);
	
	void setA16(const void* imgA16, unsigned int width, unsigned int heigth, unsigned int pitch, unsigned int x, unsigned int y);
	void getA16(      void* imgA16, unsigned int width, unsigned int heigth, unsigned int pitch, unsigned int x, unsigned int y);

	bool isSingleColorIgnoringAlpha() const;

	const ColorRGBA16* colors() const
	{
		return m_color;
	}

	ColorRGBA16* colors()
	{
		return m_color;
	}

	ColorRGBA16 color(unsigned int i) const
	{
		return m_color[i];
	}

	ColorRGBA16& color(unsigned int i)
	{
		return m_color[i];
	}

private:
	static const unsigned int COLOR_COUNT = 4*4;

	ColorRGBA16 m_color[COLOR_COUNT];
};

#endif

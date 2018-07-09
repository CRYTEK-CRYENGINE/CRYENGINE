// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ColorBlockRGBA4x4c.h"
#include "Util.h"

static_assert(sizeof(int) == 4, "Invalid type size!");

void ColorBlockRGBA4x4c::setBGRA8(const void* imgBGRA8, unsigned int const width, unsigned int const height, unsigned int const pitch, unsigned int x, unsigned int y)
{
	assert(imgBGRA8);
	assert(pitch >= width * sizeof(ColorBGRA8));
	assert(x < width);
	assert(y < height);

	const unsigned int bw = Util::getMin(width  - x, 4U);
	const unsigned int bh = Util::getMin(height - y, 4U);

	// note: it's allowed for source data to be not aligned to 4 byte boundary
	// (so, we cannot cast source data pointer to ColorBGRA8* in code below)

	if ((bw == 4) && (bh == 4))
	{
		for (unsigned int row = 0; row < 4; ++row)
		{
			const uint8* const pSrc = ((const uint8*)imgBGRA8) + (pitch * (y + row)) + (x * sizeof(ColorBGRA8));

			ColorRGBA8* const pDst = &m_color[row << 2];

			pDst[0].setBGRA(&pSrc[0 * sizeof(ColorBGRA8) / sizeof(*pSrc)]);
			pDst[1].setBGRA(&pSrc[1 * sizeof(ColorBGRA8) / sizeof(*pSrc)]);
			pDst[2].setBGRA(&pSrc[2 * sizeof(ColorBGRA8) / sizeof(*pSrc)]);
			pDst[3].setBGRA(&pSrc[3 * sizeof(ColorBGRA8) / sizeof(*pSrc)]);
		}
	}
	else
	{
		// Rare case: block is smaller than 4x4.
		// Let's repeat pixels in this case.
		// It will keep frequency of colors, except the case 
		// when width and/or height equals 3. But, this case 
		// is very rare because images usually are "power of 2" sized, and even
		// if they are not, nobody will notice that the resulting encoding 
		// for such block is not ideal.

		static unsigned int remainder[] =
		{
			0, 0, 0, 0,
			0, 1, 0, 1,
			0, 1, 2, 0,
			0, 1, 2, 3,
		};

		for (unsigned int row = 0; row < 4; ++row)
		{
			const unsigned int by = remainder[(bh - 1) * 4 + row];
			const uint8* const pSrc = ((const uint8*)imgBGRA8) + pitch * (y + by);

			ColorRGBA8* pDst = &m_color[row * 4];

			for (unsigned int col = 0; col < 4; ++col)
			{
				const unsigned int bx = remainder[(bw - 1) * 4 + col];

				pDst[col].setBGRA(&pSrc[(x + bx) * sizeof(ColorBGRA8) / sizeof(*pSrc)]);
			}
		}
	}
}

void ColorBlockRGBA4x4c::getBGRA8(void* imgBGRA8, unsigned int const width, unsigned int const height, unsigned int const pitch, unsigned int x, unsigned int y)
{
	assert(imgBGRA8);
	assert(pitch >= width * sizeof(ColorBGRA8));
	assert(x < width);
	assert(y < height);

	const unsigned int bw = Util::getMin(width  - x, 4U);
	const unsigned int bh = Util::getMin(height - y, 4U);

	// note: it's allowed for source data to be not aligned to 4 byte boundary
	// (so, we cannot cast source data pointer to ColorBGRA8* in code below)

	if ((bw == 4) && (bh == 4))
	{
		for (unsigned int row = 0; row < 4; ++row)
		{
			uint8* const pDst = ((uint8*)imgBGRA8) + (pitch * (y + row)) + (x * sizeof(ColorBGRA8));

			const ColorRGBA8* const pSrc = &m_color[row << 2];

			pSrc[0].getBGRA(&pDst[0 * sizeof(ColorBGRA8) / sizeof(*pDst)]);
			pSrc[1].getBGRA(&pDst[1 * sizeof(ColorBGRA8) / sizeof(*pDst)]);
			pSrc[2].getBGRA(&pDst[2 * sizeof(ColorBGRA8) / sizeof(*pDst)]);
			pSrc[3].getBGRA(&pDst[3 * sizeof(ColorBGRA8) / sizeof(*pDst)]);
		}
	}
	else
	{
		// Rare case: block is smaller than 4x4.
		// Let's repeat pixels in this case.
		// It will keep frequency of colors, except the case 
		// when width and/or height equals 3. But, this case 
		// is very rare because images usually are "power of 2" sized, and even
		// if they are not, nobody will notice that the resulting encoding 
		// for such block is not ideal.

		static unsigned int remainder[] =
		{
			0, 0, 0, 0,
			0, 1, 0, 1,
			0, 1, 2, 0,
			0, 1, 2, 3,
		};

		for (unsigned int row = 0; row < 4; ++row)
		{
			const unsigned int by = remainder[(bh - 1) * 4 + row];
			uint8* const pDst = ((uint8*)imgBGRA8) + pitch * (y + by);

			const ColorRGBA8* const pSrc = &m_color[row * 4];

			for (unsigned int col = 0; col < 4; ++col)
			{
				const unsigned int bx = remainder[(bw - 1) * 4 + col];

				pSrc[col].getBGRA(&pDst[(x + bx) * sizeof(ColorBGRA8) / sizeof(*pSrc)]);
			}
		}
	}
}

void ColorBlockRGBA4x4c::setA8(const void* imgA8, unsigned int const width, unsigned int const height, unsigned int const pitch, unsigned int x, unsigned int y)
{
	assert(imgA8);
	assert(pitch >= width * sizeof(uint8));
	assert(x < width);
	assert(y < height);

	const unsigned int bw = Util::getMin(width  - x, 4U);
	const unsigned int bh = Util::getMin(height - y, 4U);

	// note: it's allowed for source data to be not aligned to 4 byte boundary
	// (so, we cannot cast source data pointer to ColorBGRA8* in code below)

	if ((bw == 4) && (bh == 4))
	{
		for (unsigned int row = 0; row < 4; ++row)
		{
			const uint8* const pSrc = ((const uint8*)imgA8) + (pitch * (y + row)) + (x * sizeof(uint8));

			ColorRGBA8* const pDst = &m_color[row << 2];

			pDst[0].setRGBA(0, 0, 0, pSrc[0]);
			pDst[1].setRGBA(0, 0, 0, pSrc[1]);
			pDst[2].setRGBA(0, 0, 0, pSrc[2]);
			pDst[3].setRGBA(0, 0, 0, pSrc[3]);
		}
	}
	else
	{
		// Rare case: block is smaller than 4x4.
		// Let's repeat pixels in this case.
		// It will keep frequency of colors, except the case 
		// when width and/or height equals 3. But, this case 
		// is very rare because images usually are "power of 2" sized, and even
		// if they are not, nobody will notice that the resulting encoding 
		// for such block is not ideal.

		static unsigned int remainder[] =
		{
			0, 0, 0, 0,
			0, 1, 0, 1,
			0, 1, 2, 0,
			0, 1, 2, 3,
		};

		for (unsigned int row = 0; row < 4; ++row)
		{
			const unsigned int by = remainder[(bh - 1) * 4 + row];
			const uint8* const pSrc = ((const uint8*)imgA8) + pitch * (y + by);

			ColorRGBA8* pDst = &m_color[row * 4];

			for (unsigned int col = 0; col < 4; ++col)
			{
				const unsigned int bx = remainder[(bw - 1) * 4 + col];

				pDst[col].setRGBA(0, 0, 0, pSrc[x + bx]);
			}
		}
	}
}

void ColorBlockRGBA4x4c::getA8(void* imgA8, unsigned int const width, unsigned int const height, unsigned int const pitch, unsigned int x, unsigned int y)
{
	assert(imgA8);
	assert(pitch >= width * sizeof(uint8));
	assert(x < width);
	assert(y < height);

	const unsigned int bw = Util::getMin(width  - x, 4U);
	const unsigned int bh = Util::getMin(height - y, 4U);

	// note: it's allowed for source data to be not aligned to 4 byte boundary
	// (so, we cannot cast source data pointer to ColorBGRA8* in code below)

	if ((bw == 4) && (bh == 4))
	{
		for (unsigned int row = 0; row < 4; ++row)
		{
			uint8* const pDst = ((uint8*)imgA8) + (pitch * (y + row)) + (x * sizeof(uint8));
			uint8 r, g, b;

			const ColorRGBA8* const pSrc = &m_color[row << 2];

			pSrc[0].getRGBA(r, g, b, pDst[0]);
			pSrc[1].getRGBA(r, g, b, pDst[1]);
			pSrc[2].getRGBA(r, g, b, pDst[2]);
			pSrc[3].getRGBA(r, g, b, pDst[3]);
		}
	}
	else
	{
		// Rare case: block is smaller than 4x4.
		// Let's repeat pixels in this case.
		// It will keep frequency of colors, except the case 
		// when width and/or height equals 3. But, this case 
		// is very rare because images usually are "power of 2" sized, and even
		// if they are not, nobody will notice that the resulting encoding 
		// for such block is not ideal.

		static unsigned int remainder[] =
		{
			0, 0, 0, 0,
			0, 1, 0, 1,
			0, 1, 2, 0,
			0, 1, 2, 3,
		};

		for (unsigned int row = 0; row < 4; ++row)
		{
			const unsigned int by = remainder[(bh - 1) * 4 + row];
			uint8* const pDst = ((uint8*)imgA8) + pitch * (y + by);
			uint8 r, g, b;

			const ColorRGBA8* const pSrc = &m_color[row * 4];

			for (unsigned int col = 0; col < 4; ++col)
			{
				const unsigned int bx = remainder[(bw - 1) * 4 + col];

				pSrc[col].getRGBA(r, g, b, pDst[x + bx]);
			}
		}
	}
}

bool ColorBlockRGBA4x4c::isSingleColorIgnoringAlpha() const
{
	for (unsigned int i = 1; i < COLOR_COUNT; ++i)
	{
		if ((m_color[0].b != m_color[i].b) ||
		    (m_color[0].g != m_color[i].g) ||
		    (m_color[0].r != m_color[i].r))
		{
			return false;
		}
	}

	return true;
}

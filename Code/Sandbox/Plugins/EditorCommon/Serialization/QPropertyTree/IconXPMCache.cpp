/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#include "StdAfx.h"
#include <memory>
#include "IconXPMCache.h"
#include "QPropertyTree.h"
#include <CrySerialization/yasli/decorators/IconXPM.h>
#include "Serialization/PropertyTree/Unicode.h"
#include <QApplication>
#include <QStyleOption>
#include <QPainter>
#include <QBitmap>

#ifndef _MSC_VER
# define _stricmp strcasecmp
#endif

// ---------------------------------------------------------------------------

namespace property_tree {

IconXPMCache::~IconXPMCache()
{
	flush();
}


void IconXPMCache::flush()
{
	for (XPMToBitmap::iterator it = xpmToImageMap_.begin(); it != xpmToImageMap_.end(); ++it)
		delete it->second.bitmap;
	xpmToImageMap_.clear();

	for (FilenameToBitmap::iterator it = filenameToImageMap_.begin(); it != filenameToImageMap_.end(); ++it)
		delete it->second.bitmap;
	filenameToImageMap_.clear();
}

struct RGBAImage
{
	int width_;
	int height_;
	std::vector<Color> pixels_;

	RGBAImage() : width_(0), height_(0) {}
};

bool IconXPMCache::parseXPM(RGBAImage* out, const yasli::IconXPM& icon) 
{
	if (icon.lineCount < 3) {
		return false;
	}

	// parse values
	std::vector<Color> pixels;
	int width = 0;
	int height = 0;
	int charsPerPixel = 0;
	int colorCount = 0;
	int hotSpotX = -1;
	int hotSpotY = -1;

	int scanResult = sscanf(icon.source[0], "%d %d %d %d %d %d", &width, &height, &colorCount, &charsPerPixel, &hotSpotX, &hotSpotY);
	if (scanResult != 4 && scanResult != 6)
		return false;

	if (charsPerPixel > 4)
		return false;

	if(icon.lineCount != 1 + colorCount + height) {
		YASLI_ASSERT(0 && "Wrong line count");
		return false;
	}

	// parse colors
	std::vector<std::pair<int, Color> > colors;
	colors.resize(colorCount);

	for (int colorIndex = 0; colorIndex < colorCount; ++colorIndex) {
		const char* p = icon.source[colorIndex + 1];
		int code = 0;
		for (int charIndex = 0; charIndex < charsPerPixel; ++charIndex) {
			if (*p == '\0')
				return false;
			code = (code << 8) | *p;
			++p;
		}
		colors[colorIndex].first = code;

		while (*p == '\t' || *p == ' ')
			++p;

		if (*p == '\0')
			return false;

		if (*p != 'c' && *p != 'g')
			return false;
		++p;

		while (*p == '\t' || *p == ' ')
			++p;

		if (*p == '\0')
			return false;

		if (*p == '#') {
			++p;
			if (strlen(p) == 6) {
				int colorCode;
				if(sscanf(p, "%x", &colorCode) != 1)
					return false;
				Color color((colorCode & 0xff0000) >> 16,
							(colorCode & 0xff00) >> 8,
							(colorCode & 0xff),
							255);
				colors[colorIndex].second = color;
			}
		}
		else {
			if(_stricmp(p, "None") == 0)
				colors[colorIndex].second = Color(0, 0, 0, 0);
			else if (_stricmp(p, "Black") == 0)
				colors[colorIndex].second = Color(0, 0, 0, 255)/*GetSysColor(COLOR_BTNTEXT)*/;
			else {
				// unknown color
				colors[colorIndex].second = Color(255, 0, 0, 255);
			}
		}
	}

	// parse pixels
	pixels.resize(width * height);
	int pi = 0;
	for (int y = 0; y < height; ++y) {
		const char* p = icon.source[1 + colorCount + y];
		if (strlen(p) != width * charsPerPixel)
			return false;

		for (int x = 0; x < width; ++x) {
			int code = 0;
			for (int i = 0; i < charsPerPixel; ++i) {
				code = (code << 8) | *p;
				++p;
			}

			for (size_t i = 0; i < size_t(colorCount); ++i)
				if (colors[i].first == code)
					pixels[pi] = colors[i].second;
			++pi;
		}
	}

	out->pixels_.swap(pixels);
	out->width_ = width;
	out->height_ = height;
	return true;
}


QImage* IconXPMCache::getImageForIcon(const Icon& icon)
{
	if (icon.type == icon.TYPE_XPM)
	{
		XPMToBitmap::iterator it = xpmToImageMap_.find(icon.xpm.source);
		if (it != xpmToImageMap_.end())
			return it->second.bitmap;
		RGBAImage image;
		if (!parseXPM(&image, icon.xpm))
			return 0;

		BitmapCache& cache = xpmToImageMap_[icon.xpm.source];
		cache.pixels.swap(image.pixels_);
		cache.bitmap = new QImage((unsigned char*)&cache.pixels[0], image.width_, image.height_, QImage::Format_ARGB32);
		return cache.bitmap;
	}
	else if (icon.type == icon.TYPE_FILE)
	{
		FilenameToBitmap::iterator it = filenameToImageMap_.find(icon.filename);
		if (it != filenameToImageMap_.end())
			return it->second.bitmap;
		;

		BitmapCache& cache = filenameToImageMap_[icon.filename];
		cache.bitmap = new QImage(property_tree::PropertyIcon(QString::fromUtf8(icon.filename.c_str())).pixmap(16, 16).toImage());
		return cache.bitmap;
	}

	static QImage emptyImage;
	return &emptyImage;
}

// ---------------------------------------------------------------------------


}


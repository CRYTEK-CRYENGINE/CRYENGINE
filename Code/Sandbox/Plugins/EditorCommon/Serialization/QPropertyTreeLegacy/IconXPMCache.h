/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once

#include "Serialization/PropertyTreeLegacy/Color.h"
#include "Serialization/PropertyTreeLegacy/IDrawContext.h"
#include "Serialization/PropertyTreeLegacy/Rect.h"

#include <map>
#include <vector>

class QImage;
namespace yasli { struct IconXPM; }

namespace property_tree {
struct RGBAImage;
struct Color;

struct IconXPMCache
{
	void flush();

	~IconXPMCache();

	QImage* getImageForIcon(const Icon& icon);
private:
	struct BitmapCache {
		std::vector<Color> pixels;
		QImage* bitmap;
	};

	static bool parseXPM(RGBAImage* out, const yasli::IconXPM& xpm);
	typedef std::map<const char* const*, BitmapCache> XPMToBitmap;
	XPMToBitmap xpmToImageMap_;
	typedef std::map<yasli::string, BitmapCache> FilenameToBitmap;
	FilenameToBitmap filenameToImageMap_;
};

}

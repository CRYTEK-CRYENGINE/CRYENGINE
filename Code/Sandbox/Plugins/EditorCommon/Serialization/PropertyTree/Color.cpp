/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#include "StdAfx.h"
#include "Color.h"
#include "MathUtils.h"
#include <CrySerialization/yasli/Archive.h>
#include <math.h>


namespace property_tree {

// HSV
// h=0..360, s=0..1, v=0..1
inline void HSVtoRGB(float h, float s, float v,
	float& r, float& g, float& b)
{
	const float min = 1e-5f;
	int i;
	float f, m, n, k;

	if (s < min){
		r = g = b = v;
	}
	else {
		if (h >= 360.0f)
			h = 0;
		else
			h = h / 60.0f;

		i = xround(floor(h));
		f = h - i;
		m = v*(1 - s);
		n = v*(1 - s*f);
		k = v*(1 - s*(1 - f));

		switch (i){
		case 0:
			r = v; g = k; b = m;
			break;
		case 1:
			r = n; g = v; b = m;
			break;
		case 2:
			r = m; g = v; b = k;
			break;
		case 3:
			r = m; g = n; b = v;
			break;
		case 4:
			r = k; g = m; b = v;
			break;
		case 5:
			r = v; g = m; b = n;
			break;
		default:
			YASLI_ASSERT(0);
		}
	}

	YASLI_ASSERT(r >= 0 && r <= 1);
	YASLI_ASSERT(g >= 0 && g <= 1);
	YASLI_ASSERT(b >= 0 && b <= 1);
}

void Color::setHSV(float h, float s, float v, unsigned char alpha)
{
	float rf, gf, bf;
	HSVtoRGB(h, s, v, rf, gf, bf);
	r = xround(rf * 255);
	g = xround(gf * 255);
	b = xround(bf * 255);
	a = alpha;
}


void Color::toHSV(float& h, float& s, float& v)
{
	float rf = r / 255.f;
	float gf = g / 255.f;
	float bf = b / 255.f;
	v = max(max(rf, gf), bf);
	float temp = min(min(rf, gf), bf);
	if (v == 0)
		s = 0;
	else
		s = (v - temp) / v;

	if (s == 0)
		h = 0;
	else {
		float Cr = (v - rf) / (v - temp);
		float Cg = (v - gf) / (v - temp);
		float Cb = (v - bf) / (v - temp);

		if (rf == v) {
			h = Cb - Cg;
		}
		else if (gf == v) {
			h = 2 + Cr - Cb;
		}
		else if (bf == v) {
			h = 4 + Cg - Cr;
		}

		h = 60 * h;
		if (h < 0)h += 360;
	}
}

void Color::YASLI_SERIALIZE_METHOD(yasli::Archive& ar)
{
	ar(r, "", "^");
	ar(g, "", "^");
	ar(b, "", "^");
	ar(a, "", "^");
}

}


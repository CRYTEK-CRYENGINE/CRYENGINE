/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once

#include <CrySerialization/yasli/Config.h>

namespace yasli { class Archive; }

namespace property_tree {

struct Color
{
    unsigned char b, g, r, a;
	
	Color() : r(255), g(255), b(255), a(255) { }
	Color(unsigned char _r, unsigned char _g, unsigned char _b, unsigned char _a = 255) { r=_r; g=_g; b=_b; a=_a; }
	explicit Color(unsigned long _argb) { argb() = _argb; }
	void set(int rc,int gc,int bc,int ac = 255) { r=rc; g=gc; b=bc; a=ac; }
	
	Color& setGDI(unsigned long color) { 
        b = (unsigned char)(color >> 16);
        g = (unsigned char)(color >> 8);
        r = (unsigned char)(color);
		a = 255;
		return *this;
	}

	void setHSV(float h,float s,float v, unsigned char alpha = 255);
	void toHSV(float& h,float& s, float& v);

	Color& operator*= (float f) { r=int(r*f); g=int(g*f); b=int(b*f); a=int(a*f); return *this; }
	Color& operator+= (Color &p) { r+=p.r; g+=p.g; b+=p.b; a+=p.a; return *this; }
	Color& operator-= (Color &p) { r-=p.r; g-=p.g; b-=p.b; a-=p.a; return *this; }
	Color operator+ (Color &p) { return Color(r+p.r,g+p.g,b+p.b,a+p.a); }
	Color operator- (Color &p) { return Color(r-p.r,g-p.g,b-p.b,a-p.a); }
	Color operator* (float f) const { return Color(int(r*f), int(g*f), int(b*f), int(a*f)); }
	Color operator* (int f) const { return Color(r*f,g*f,b*f,a*f); }
	Color operator/ (int f) const { if(f!=0) f=(1<<16)/f; else f=1<<16; return Color((r*f)>>16,(g*f)>>16,(b*f)>>16,(a*f)>>16); }
	
	bool operator==(const Color& rhs) const { return argb() == rhs.argb(); }
	bool operator!=(const Color& rhs) const { return argb() != rhs.argb(); }
	
	unsigned long argb() const { return *reinterpret_cast<const unsigned long*>(this); }
	unsigned long& argb() { return *reinterpret_cast<unsigned long*>(this); }
	unsigned long rgb() const { return r | g << 8 | b << 16; }
	unsigned long rgba() const { return r | g << 8 | b << 16 | a << 24; }
	unsigned char& operator[](int i) { return ((unsigned char*)this)[i];}
	Color interpolate(const Color &v, float f) const
	{
		return Color(int(r+int(v.r-r)*f),
					 int(g+int(v.g-g)*f),
					 int(b+int(v.b-b)*f),
					 int(a+(v.a-a)*f));
	}
	void YASLI_SERIALIZE_METHOD(yasli::Archive& ar);
};

}


// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace yasli {

class Archive;

// Icon, stored in XPM format
struct IconXPM
{
	const char* const* source;
	int lineCount;

	IconXPM()
	: source(0)
	, lineCount(0)
	{
	}
  template<size_t Size>
	explicit IconXPM(const char* (&xpm)[Size])
	{
		source = xpm;
		lineCount = Size;
	}
  template<size_t Size>
	explicit IconXPM(char* (&xpm)[Size])
	{
		source = xpm;
		lineCount = Size;
	}

	IconXPM(const char* const* source, int lineCount)
	: source(source)
	, lineCount(lineCount)
	{
	}

	void YASLI_SERIALIZE_METHOD(Archive& ar) 	{}
	bool operator<(const IconXPM& rhs) const { return source < rhs.source; }

};

struct IconXPMToggle
{
	bool* variable_;
	bool value_;
	IconXPM iconTrue_;
	IconXPM iconFalse_;

	template<size_t Size1, size_t Size2>
	IconXPMToggle(bool& variable, char* (&xpmTrue)[Size1], char* (&xpmFalse)[Size2])
	: iconTrue_(xpmTrue)
	, iconFalse_(xpmFalse)
	, variable_(&variable)
	, value_(variable)
	{
	}

	template<size_t Size1, size_t Size2>
	IconXPMToggle(bool& variable, const char* (&xpmTrue)[Size1], const char* (&xpmFalse)[Size2])
	: iconTrue_(xpmTrue)
	, iconFalse_(xpmFalse)
	, variable_(&variable)
	, value_(variable)
	{
	}

	IconXPMToggle(bool& variable, const IconXPM& iconTrue, const IconXPM& iconFalse)
	: iconTrue_(iconTrue)
	, iconFalse_(iconFalse)
	, variable_(&variable)
	, value_(variable)
	{
	}

	IconXPMToggle(const IconXPMToggle& orig)
	: variable_(0)
	, value_(orig.value_)
	, iconTrue_(orig.iconTrue_)
	, iconFalse_(orig.iconFalse_)
	{
	}

	IconXPMToggle()
	: variable_(0)
	{
	}
	IconXPMToggle& operator=(const IconXPMToggle& rhs){
		value_ = rhs.value_;
		return *this;
	}
	~IconXPMToggle()
	{
		if (variable_)
			*variable_ = value_;
	}

	template<class TArchive>
	void YASLI_SERIALIZE_METHOD(TArchive& ar)
	{
		ar(value_, "value", "Value");
	}
};

}

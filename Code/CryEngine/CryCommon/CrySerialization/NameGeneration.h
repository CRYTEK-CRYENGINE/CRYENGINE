// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include <CryCore/Platform/platform.h>

namespace Serialization
{

//! Skip underscores and prefixes in identifier.
inline string ToDisplayName(cstr name)
{
	if (name[0] == 'm' && name[1] == '_')
		name += 2;
	while (*name == '_')
		name++;
	string disp;
	if (*name)
	{
		disp += toupper(*name);
		disp += name + 1;
	}
	return disp;
}

//! Convert label to name: remove spaces.
inline string LabelToName(cstr name)
{
	string label;

	for (; *name; name++)
	{
		if (isalnum(*name))
			label.push_back(*name);
	}
	return label;
}

//! Convert name to label: add spaces at case or underscore breaks.
inline string NameToLabel(cstr name)
{
	string label;

	while (*name == '_')
		name++;

	if (*name)
	{
		label.push_back(toupper(*name));
		name++;
	}

	for (; *name; name++)
	{
		// if mixed-case or underscore found
		if ((isupper(*name) && islower(name[-1])) || *name == '_')
		{
			// add space
			label.push_back(' ');
			if (*name == '_')
				continue;
		}
		label.push_back(*name);
	}
	return label;
}

}

#define SERIALIZE_VAR(ar, var)                                            \
  static string var ## _Name = Serialization::ToDisplayName( # var);      \
  static string var ## _Label = Serialization::NameToLabel(var ## _Name); \
  ar(var, var ## _Name, var ## _Label)

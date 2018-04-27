// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Platform/platform.h>

namespace Serialization
{

//! Skip underscores and prefixes in identifier.
inline cstr VarToName(char* name)
{
	if (name[0] == 'm' && name[1] == '_')
		name += 2;
	while (*name == '_')
		name++;
	if (*name)
		*name = toupper(*name);
	return name;
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

#define SERIALIZE_VAR(ar, var)                                  \
	do {                                                        \
	  static char _var[] = # var;                               \
	  static cstr _name = Serialization::VarToName(_var);       \
	  static string _label = Serialization::NameToLabel(_name); \
	  ar(var, _name, _label);                                   \
	} while (false)                                             \


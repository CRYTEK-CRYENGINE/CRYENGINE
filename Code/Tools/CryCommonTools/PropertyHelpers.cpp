// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PropertyHelpers.h"
#include "StringHelpers.h"


bool PropertyHelpers::GetPropertyValue(const string& a_propertiesString, const char* a_propertyName, string& a_value)
{
	if ((a_propertyName==0) || (a_propertyName[0]==0))
	{
		return false;
	}

	const char* lineStart = a_propertiesString.c_str();

	while (*lineStart)
	{
		string key;
		string value;

		const size_t lineEndPosition = strcspn(lineStart, "\n");
		const size_t equalPosition = strcspn(lineStart, "=");

		if (equalPosition < lineEndPosition)
		{
			key = string(lineStart, equalPosition);
			value = string(lineStart + equalPosition + 1, lineEndPosition - equalPosition - 1);
		}
		else
		{
			key = string(lineStart, lineEndPosition);
			value = "";
		}

		key = StringHelpers::Trim( key );

		if (stricmp(key.c_str(), a_propertyName) == 0)
		{
			a_value = StringHelpers::Trim( value );
			return true;
		}

		lineStart += lineEndPosition;
		if (*lineStart)
		{
			++lineStart;
		}
	}

	return false;
}

void PropertyHelpers::SetPropertyValue(string& a_propertiesString, const char* a_propertyName, const char* a_value)
{
	if ((a_propertyName==0) || (a_propertyName[0]==0))
	{
		return;
	}

	const string newValue = StringHelpers::Trim(string(a_value));

	const char* lineStart = a_propertiesString.c_str();

	while (*lineStart)
	{
		const size_t lineEndPosition = strcspn(lineStart, "\n");
		const size_t equalPosition = strcspn(lineStart, "=");

		const string key = StringHelpers::Trim( string(lineStart, ((equalPosition < lineEndPosition) ? equalPosition : lineEndPosition)) );

		if (stricmp(key.c_str(), a_propertyName) == 0)
		{
			const size_t prefixSz = lineStart - a_propertiesString.c_str();
			const size_t expressionSz = lineEndPosition;

			if (newValue.empty())
			{
				a_propertiesString = a_propertiesString.substr(0,prefixSz) + string(a_propertyName) + a_propertiesString.substr(prefixSz + expressionSz,string::npos);
			}
			else
			{
				a_propertiesString = a_propertiesString.substr(0,prefixSz) + string(a_propertyName) + string("=") + newValue + a_propertiesString.substr(prefixSz + expressionSz,string::npos);
			}
			return;
		}

		lineStart += lineEndPosition;
		if (*lineStart)
		{
			++lineStart;
		}
	}

	if ( a_propertiesString.empty() || (a_propertiesString[a_propertiesString.size()-1] != '\n') )
	{
		a_propertiesString += string("\r\n");
	}

	if (newValue.empty())
	{
		a_propertiesString += string(a_propertyName);
	}
	else
	{
		a_propertiesString += string(a_propertyName) + string("=") + newValue;
	}
}

bool PropertyHelpers::HasProperty(const string& a_propertiesString, const char* a_propertyName)
{
	string value;
	return PropertyHelpers::GetPropertyValue(a_propertiesString, a_propertyName, value);
}

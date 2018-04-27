// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PROPERTYHELPERS_H__
#define __PROPERTYHELPERS_H__

namespace PropertyHelpers
{
	bool GetPropertyValue(const string& propertiesString, const char* propertyName, string& value);
	void SetPropertyValue(string& a_propertiesString, const char* propertyName, const char* value);
	bool HasProperty(const string& propertiesString, const char* propertyName);
}

#endif



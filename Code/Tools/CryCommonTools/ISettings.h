// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ISETTINGS_H__
#define __ISETTINGS_H__

class ISettings
{
public:
	virtual bool GetSettingString(char* buffer, int bufferSizeInBytes, const char* key) = 0;
	virtual bool GetSettingInt(int& value, const char* key) = 0;
};

inline bool GetSettingByRef(ISettings* settings, const string& key, string& value)
{
	char buffer[1024];
	bool success = false;
	if (settings)
		success = settings->GetSettingString(buffer, sizeof(buffer), key.c_str());
	if (success)
		value = buffer;
	return success;
}

inline bool GetSettingByRef(ISettings* settings, const string& key, int& value)
{
	return settings->GetSettingInt(value, key.c_str());
}

template <typename T> inline T GetSetting(ISettings* settings, const string& key, const T& dflt)
{
	T value;
	if (!GetSettingByRef(settings, key, value))
		value = dflt;
	return value;
}

#endif //__ISETTINGS_H__

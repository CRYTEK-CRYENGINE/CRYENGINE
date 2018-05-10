// Copyright 2001-2015 Crytek GmbH. All rights reserved.
#pragma once

class IObjectEnumerator
{
public:
	virtual void AddEntry(const char* path, const char* id, const char* sortStr = "") = 0;
	virtual void RemoveEntry(const char* path, const char* id) = 0;
	virtual void ChangeEntry(const char* path, const char* id, const char* sortStr = "") = 0;
};


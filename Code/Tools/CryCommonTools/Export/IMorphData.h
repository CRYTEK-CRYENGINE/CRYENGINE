// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IMORPHDATA_H__
#define __IMORPHDATA_H__

class IMorphData
{
public:
	virtual void SetHandle(void* handle) = 0;
	virtual void AddMorph(void* handle, const char* name, const char* fullName = NULL) = 0;
	virtual void* GetHandle() const = 0;
	virtual int GetMorphCount() const = 0;
	virtual void* GetMorphHandle(int morphIndex) const = 0;
};

#endif //__IMORPHDATA_H__

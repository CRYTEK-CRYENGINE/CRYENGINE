// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MORPHDATA_H__
#define __MORPHDATA_H__

#include "IMorphData.h"

class MorphData : public IMorphData
{
public:
	MorphData();

	virtual void SetHandle(void* handle);
	virtual void AddMorph(void* handle, const char* name, const char* fullname);
	virtual void* GetHandle() const;
	virtual int GetMorphCount() const;
	virtual void* GetMorphHandle(int morphIndex) const;

	std::string GetMorphName(int morphIndex) const;
	std::string GetMorphFullName(int morphIndex) const;

private:
	struct Entry
	{
		Entry(void* handle, const std::string& name, const std::string& fullname): handle(handle), name(name), fullname(fullname) {}
		void* handle;
		std::string name;
		std::string fullname;
	};

	void* m_handle;
	std::vector<Entry> m_morphs;
};

#endif //__MORPHDATA_H__

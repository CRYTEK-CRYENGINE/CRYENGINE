// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PACKED_CHR_LOADER__H__
#define __PACKED_CHR_LOADER__H__

#include "SkeletonInfo.h"

struct IPakSystem;
class ICryXML;

class SkeletonLoader
{
public:
	SkeletonLoader();

	CSkeletonInfo* Load(const char* filename, IPakSystem* pakSystem, ICryXML* xml, const string& tempPath);

	const char* GetTempName() const { return m_tempFileName.c_str(); }
	const CSkeletonInfo& Skeleton() const{ return m_skeletonInfo; }
	bool IsLoaded() const{ return m_bLoaded; }
private:
	CSkeletonInfo m_skeletonInfo;
	string m_tempFileName;
	bool m_bLoaded;
};


#endif

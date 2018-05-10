// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PROCEDURAL_CLIP_FACTORY__H__
#define __PROCEDURAL_CLIP_FACTORY__H__

#include "ICryMannequinProceduralClipFactory.h"

class CProceduralClipFactory
	: public IProceduralClipFactory
{
public:
	CProceduralClipFactory();
	virtual ~CProceduralClipFactory();

	// IProceduralClipFactory
	virtual const char*          FindTypeName(const THash& typeNameHash) const;

	virtual size_t               GetRegisteredCount() const;
	virtual THash                GetTypeNameHashByIndex(const size_t index) const;
	virtual const char*          GetTypeNameByIndex(const size_t index) const;

	virtual bool                 Register(const char* const typeName, const SProceduralClipFactoryRegistrationInfo& registrationInfo);

	virtual IProceduralParamsPtr CreateProceduralClipParams(const char* const typeName) const;
	virtual IProceduralParamsPtr CreateProceduralClipParams(const THash& typeNameHash) const;

	virtual IProceduralClipPtr   CreateProceduralClip(const char* const typeName) const;
	virtual IProceduralClipPtr   CreateProceduralClip(const THash& typeNameHash) const;
	// ~IProceduralClipFactory

private:
	const SProceduralClipFactoryRegistrationInfo* FindRegistrationInfo(const THash& typeNameHash) const;

private:
	typedef VectorMap<THash, string> THashToNameMap;
	THashToNameMap m_typeHashToName;

	typedef VectorMap<THash, SProceduralClipFactoryRegistrationInfo> TTypeHashToRegistrationInfoMap;
	TTypeHashToRegistrationInfoMap m_typeHashToRegistrationInfo;
};

#endif

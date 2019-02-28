// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProceduralClipFactory.h"
#include "ICryMannequin.h"
#include "Mannequin/Serialization.h"

CProceduralClipFactory::CProceduralClipFactory()
{
}

CProceduralClipFactory::~CProceduralClipFactory()
{
}

const char* CProceduralClipFactory::FindTypeName(const THash& typeNameHash) const
{
	const THashToNameMap::const_iterator cit = m_typeHashToName.find(typeNameHash);
	const bool typeNameFound = (cit != m_typeHashToName.end());
	if (typeNameFound)
	{
		const char* const typeName = cit->second.c_str();
		return typeName;
	}
	return NULL;
}

size_t CProceduralClipFactory::GetRegisteredCount() const
{
	const size_t registeredCount = m_typeHashToName.size();
	return registeredCount;
}

IProceduralClipFactory::THash CProceduralClipFactory::GetTypeNameHashByIndex(const size_t index) const
{
	CRY_ASSERT(index < m_typeHashToName.size());

	THashToNameMap::const_iterator cit = m_typeHashToName.begin();
	std::advance(cit, index);

	const THash typeNameHash = cit->first;
	return typeNameHash;
}

const char* CProceduralClipFactory::GetTypeNameByIndex(const size_t index) const
{
	CRY_ASSERT(index < m_typeHashToName.size());

	THashToNameMap::const_iterator cit = m_typeHashToName.begin();
	std::advance(cit, index);

	const char* const typeName = cit->second.c_str();
	return typeName;
}

bool CProceduralClipFactory::Register(const char* const typeName, const SProceduralClipFactoryRegistrationInfo& registrationInfo)
{
	CRY_ASSERT(typeName);
	CRY_ASSERT(registrationInfo.pProceduralClipCreator != NULL);
	CRY_ASSERT(registrationInfo.pProceduralParamsCreator != NULL);

	const THash typeNameHash(typeName);

	const char* const registeredTypeNameForHash = FindTypeName(typeNameHash);
	const bool alreadyRegistered = (registeredTypeNameForHash != NULL);
	if (alreadyRegistered)
	{
		const bool namesMatch = (strcmp(typeName, registeredTypeNameForHash) == 0);
		if (namesMatch)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "CProceduralClipFactory::Register: Register called more than once for type with name '%s'.", typeName);
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "CProceduralClipFactory::Register: Trying to register type with name '%s' and hash '%u', but hash collides with already registered type with name '%s'. Choose a different name for the type.", typeName, typeNameHash.ToUInt32(), registeredTypeNameForHash);
			// TODO: FatalError?
		}

		CRY_ASSERT(false);
		return false;
	}

#if defined(_DEBUG)
	CryLogAlways("CProceduralClipFactory: Registering procedural clip with name '%s'.", typeName);
#endif
	m_typeHashToName[typeNameHash] = string(typeName);
	m_typeHashToRegistrationInfo[typeNameHash] = registrationInfo;

	return true;
}

IProceduralParamsPtr CProceduralClipFactory::CreateProceduralClipParams(const char* const typeName) const
{
	CRY_ASSERT(typeName);

	const THash typeNameHash(typeName);
	return CreateProceduralClipParams(typeNameHash);
}

IProceduralParamsPtr CProceduralClipFactory::CreateProceduralClipParams(const THash& typeNameHash) const
{
	const SProceduralClipFactoryRegistrationInfo* const pRegistrationInfo = FindRegistrationInfo(typeNameHash);
	if (!pRegistrationInfo)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "CProceduralClipFactory::CreateProceduralClipParams: Failed to create procedural params for type with hash '%u'.", typeNameHash.ToUInt32());
		return IProceduralParamsPtr();
	}
	IProceduralParamsPtr pParams = (*pRegistrationInfo->pProceduralParamsCreator)();
	return pParams;
}

IProceduralClipPtr CProceduralClipFactory::CreateProceduralClip(const char* const typeName) const
{
	CRY_ASSERT(typeName);

	const THash typeNameHash(typeName);
	return CreateProceduralClip(typeNameHash);
}

IProceduralClipPtr CProceduralClipFactory::CreateProceduralClip(const THash& typeNameHash) const
{
	const SProceduralClipFactoryRegistrationInfo* const pRegistrationInfo = FindRegistrationInfo(typeNameHash);
	if (!pRegistrationInfo)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "CProceduralClipFactory::CreateProceduralClip: Failed to create procedural clip for type with hash '%u'.", typeNameHash.ToUInt32());
		return IProceduralClipPtr();
	}
	IProceduralClipPtr pClip = (*pRegistrationInfo->pProceduralClipCreator)();
	return pClip;
}

const SProceduralClipFactoryRegistrationInfo* CProceduralClipFactory::FindRegistrationInfo(const THash& typeNameHash) const
{
	const TTypeHashToRegistrationInfoMap::const_iterator cit = m_typeHashToRegistrationInfo.find(typeNameHash);
	const bool registrationInfoFound = (cit != m_typeHashToRegistrationInfo.end());
	if (registrationInfoFound)
	{
		const SProceduralClipFactoryRegistrationInfo& registrationInfo = cit->second;
		return &registrationInfo;
	}

	return NULL;
}

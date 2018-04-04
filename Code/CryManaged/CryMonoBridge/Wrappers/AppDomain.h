#pragma once

#include "MonoDomain.h"

// Wrapped manager of a mono app domain
class CAppDomain final : public CMonoDomain
{
	friend class CMonoLibrary;
	friend class CMonoClass;
	friend class CMonoRuntime;

public:
	CAppDomain(char *name, bool bActivate = false);
	CAppDomain(MonoInternals::MonoDomain* pMonoDomain);
	virtual ~CAppDomain() {}

	// CMonoDomain
	virtual bool IsRoot() override { return false; }

	virtual bool Reload() override;
	// ~CMonoDomain

	void SerializeObject(CMonoObject* pSerializer, MonoInternals::MonoObject* pObject, bool bIsAssembly);
	std::shared_ptr<CMonoObject> DeserializeObject(CMonoObject* pSerializer, bool bIsAssembly);

protected:
	void CreateDomain(char *name, bool bActivate);

	void SerializeDomainData(std::vector<char>& bufferOut);
	void DeserializeDomainData(const std::vector<char>& serializedData);

protected:
	string m_name;

	uint64 m_serializationTicks;
};
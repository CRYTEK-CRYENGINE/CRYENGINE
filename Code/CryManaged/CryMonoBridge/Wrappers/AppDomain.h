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
	virtual ~CAppDomain();

	// CMonoDomain
	virtual bool IsRoot() override { return false; }

	virtual bool Reload() override;
	virtual bool IsReloading() override { return m_isReloading; }
	// ~CMonoDomain

	CMonoLibrary* GetCryCommonLibrary() const { return m_pLibCommon; }
	CMonoLibrary* GetCryCoreLibrary() const { return m_pLibCore; }

	CMonoLibrary* CompileFromSource(const char* szDirectory);

	void SerializeObject(CMonoObject* pSerializer, MonoInternals::MonoObject* pObject, bool bIsAssembly);
	std::shared_ptr<CMonoObject> DeserializeObject(CMonoObject* pSerializer, const CMonoClass* const pObjectClass);

protected:
	void CreateDomain(char *name, bool bActivate);

	void SerializeDomainData(std::vector<char>& bufferOut);
	std::shared_ptr<CMonoObject> CreateDeserializer(const std::vector<char>& serializedData);

protected:
	string m_name;
	bool  m_isReloading = false;

	uint64 m_serializationTicks;

	CMonoLibrary* m_pLibCore;
	CMonoLibrary* m_pLibCommon;
};
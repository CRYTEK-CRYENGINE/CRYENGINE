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

	void Initialize();

	CMonoLibrary* GetCryCommonLibrary() const { return m_pLibCommon; }
	CMonoLibrary* GetCryCoreLibrary() const { return m_pLibCore; }

	CMonoLibrary* CompileFromSource(const char* szDirectory);
	CMonoLibrary* GetCompiledLibrary();

	void SerializeObject(CMonoObject* pSerializer, MonoInternals::MonoObject* pObject, bool bIsAssembly);
	std::shared_ptr<CMonoObject> DeserializeObject(CMonoObject* pSerializer, const CMonoClass* const pObjectClass);
	void DeserializeDeletedObject(CMonoObject* pSerializer);

	CMonoClass* GetVector2Class() const { return m_pVector2Class; }
	CMonoClass* GetVector3Class() const { return m_pVector3Class; }
	CMonoClass* GetVector4Class() const { return m_pVector4Class; }
	CMonoClass* GetQuaternionClass() const { return m_pQuaternionClass; }
	CMonoClass* GetAngles3Class() const { return m_pAngles3Class; }
	CMonoClass* GetColorClass() const { return m_pColorClass; }

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

	CMonoClass* m_pVector2Class;
	CMonoClass* m_pVector3Class;
	CMonoClass* m_pVector4Class;
	CMonoClass* m_pQuaternionClass;
	CMonoClass* m_pAngles3Class;
	CMonoClass* m_pColorClass;
};
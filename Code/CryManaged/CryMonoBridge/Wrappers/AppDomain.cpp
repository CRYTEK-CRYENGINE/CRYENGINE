#include "StdAfx.h"
#include "AppDomain.h"

#include "MonoRuntime.h"
#include "MonoLibrary.h"
#include "MonoClass.h"
#include "MonoMethod.h"

#include "RootMonoDomain.h"

#include <CrySystem/IProjectManager.h>

CAppDomain::CAppDomain(char *name, bool bActivate)
	: m_name(name)
{
	CreateDomain(name, bActivate);

	m_bNativeDomain = true;
}

CAppDomain::CAppDomain(MonoInternals::MonoDomain* pMonoDomain)
{
	m_pDomain = pMonoDomain;

#ifndef _RELEASE
	MonoInternals::mono_debug_domain_create(m_pDomain);
#endif

	m_bNativeDomain = false;
}

void CAppDomain::CreateDomain(char *name, bool bActivate)
{
	m_pDomain = MonoInternals::mono_domain_create_appdomain(name, nullptr);

	char baseDirectory[_MAX_PATH];
	CryGetExecutableFolder(CRY_ARRAY_COUNT(baseDirectory), baseDirectory);

	string sConfigFile;
	sConfigFile.Format("%smono\\etc\\mono\\4.5\\machine.config", baseDirectory);

	char szEngineBinaryDir[_MAX_PATH];
	CryGetExecutableFolder(CRY_ARRAY_COUNT(szEngineBinaryDir), szEngineBinaryDir);

	MonoInternals::mono_domain_set_config(m_pDomain, szEngineBinaryDir, sConfigFile);

	if (bActivate)
	{
		Activate();
	}

#ifndef _RELEASE
	MonoInternals::mono_debug_domain_create(m_pDomain);
#endif
}

bool CAppDomain::Reload()
{
	// Make note of whether we were active or not, so we can reapply state to the new domain
	bool bWasActive = IsActive();

	std::vector<char> serializedData;

	// Serialize statics and objects in the assemblies
	SerializeDomainData(serializedData);

	// Unload assemblies
	for (auto it = m_loadedLibraries.rbegin(); it != m_loadedLibraries.rend(); ++it)
	{
		it->get()->Unload();
	}

	// Kill the domain
	Unload();

	// Now start recreating the domain
	// Copy the name, Mono doesn't actually touch the char array in memory but let's be safe.
	string name = m_name;
	CreateDomain(const_cast<char*>(name.c_str()), bWasActive);

	// Reload assemblies
	for (const std::unique_ptr<CMonoLibrary>& pLibrary : m_loadedLibraries)
	{
		pLibrary->Reload();
	}

	DeserializeDomainData(serializedData);

	return true;
}

void CAppDomain::SerializeDomainData(std::vector<char>& serializedData)
{
	// Create the shared serialization utilities that may be called by libraries and objects
	// Now serialize the statics contained inside this library
	CMonoLibrary* pNetCoreLibrary = GetMonoRuntime()->GetRootDomain()->GetNetCoreLibrary();

	std::shared_ptr<CMonoClass> pMemoryStreamClass = pNetCoreLibrary->GetTemporaryClass("System.IO", "MemoryStream");
	std::shared_ptr<CMonoObject> pMemoryStream = std::static_pointer_cast<CMonoObject>(pMemoryStreamClass->CreateInstance());

	m_serializationTicks = 0;

	std::shared_ptr<CMonoClass> pCryObjectWriterClass = GetMonoRuntime()->GetCryCoreLibrary()->GetTemporaryClass("CryEngine.Serialization", "ObjectWriter");

	void* writerConstructorArgs[1];
	writerConstructorArgs[0] = pMemoryStream->GetManagedObject();

	std::shared_ptr<CMonoObject> pSerializer = std::static_pointer_cast<CMonoObject>(pCryObjectWriterClass->CreateInstance(writerConstructorArgs, 1));

	for (const std::unique_ptr<CMonoLibrary>& pLibrary : m_loadedLibraries)
	{
		pLibrary->Serialize(pSerializer.get());
	}

	// Grab the serialized buffer
	std::shared_ptr<CMonoObject> pManagedBuffer = pMemoryStream->GetClass()->FindMethod("GetBuffer")->Invoke(pMemoryStream.get());

	serializedData.resize(pManagedBuffer->GetArraySize());
	char* arrayBuffer = pManagedBuffer->GetArrayAddress(sizeof(char), 0);

	memcpy(serializedData.data(), arrayBuffer, serializedData.size());

	CryLogAlways("Total serialization time: %f ms, %iMB used", (1000.f * gEnv->pTimer->TicksToSeconds(m_serializationTicks)), serializedData.size() / (1024 * 1024));
}

void CAppDomain::DeserializeDomainData(const std::vector<char>& serializedData)
{
	// Now serialize the statics contained inside this library
	CMonoLibrary* pNetCoreLibrary = GetMonoRuntime()->GetRootDomain()->GetNetCoreLibrary();

	m_serializationTicks = 0;

	std::shared_ptr<CMonoClass> pCryObjectReaderClass = GetMonoRuntime()->GetCryCoreLibrary()->GetTemporaryClass("CryEngine.Serialization", "ObjectReader");

	CMonoDomain* pDomain = pCryObjectReaderClass->GetAssembly()->GetDomain();

	MonoInternals::MonoArray* pMonoManagedBuffer = MonoInternals::mono_array_new(pDomain->GetMonoDomain(), MonoInternals::mono_get_byte_class(), serializedData.size());
	std::shared_ptr<CMonoClass> pBufferClass = pNetCoreLibrary->GetClassFromMonoClass(MonoInternals::mono_object_get_class((MonoInternals::MonoObject*)pMonoManagedBuffer));

	std::shared_ptr<CMonoObject> pManagedBuffer = std::make_shared<CMonoObject>((MonoInternals::MonoObject*)pMonoManagedBuffer, pBufferClass);
	char* arrayBuffer = pManagedBuffer->GetArrayAddress(sizeof(char), 0);

	memcpy(arrayBuffer, serializedData.data(), serializedData.size());

	void* memoryStreamCtorArgs[2];
	memoryStreamCtorArgs[0] = pMonoManagedBuffer;

	bool bWritable = false;
	MonoInternals::MonoObject* pBoolObject = MonoInternals::mono_value_box(pDomain->GetMonoDomain(), MonoInternals::mono_get_boolean_class(), &bWritable);
	memoryStreamCtorArgs[1] = pBoolObject;

	std::shared_ptr<CMonoClass> pMemoryStreamClass = pNetCoreLibrary->GetTemporaryClass("System.IO", "MemoryStream");
	std::shared_ptr<CMonoObject> pMemoryStream = std::static_pointer_cast<CMonoObject>(pMemoryStreamClass->CreateInstance(memoryStreamCtorArgs, 2));

	void* readerConstructorParams[1];
	readerConstructorParams[0] = pMemoryStream->GetManagedObject();

	std::shared_ptr<CMonoObject> pSerializer = std::static_pointer_cast<CMonoObject>(pCryObjectReaderClass->CreateInstance(readerConstructorParams, 1));

	for (const std::unique_ptr<CMonoLibrary>& pLibrary : m_loadedLibraries)
	{
		pLibrary->Deserialize(pSerializer.get());
	}

	CryLogAlways("Total de-serialization time: %f ms", (1000.f * gEnv->pTimer->TicksToSeconds(m_serializationTicks)));
}

void CAppDomain::SerializeObject(CMonoObject* pSerializer, MonoInternals::MonoObject* pObject, bool bIsAssembly)
{
	int64 startTime = CryGetTicks();

	void* writeParams[1];
	writeParams[0] = pObject;

	pSerializer->GetClass()->FindMethod(bIsAssembly ? "WriteStatics" : "Write", 1)->Invoke(pSerializer, writeParams);

	m_serializationTicks += CryGetTicks() - startTime;
}

std::shared_ptr<CMonoObject> CAppDomain::DeserializeObject(CMonoObject* pSerializer, bool bIsAssembly)
{
	int64 startTime = CryGetTicks();

	// Now serialize the statics contained inside this library
	std::shared_ptr<CMonoObject> pReadResult = pSerializer->GetClass()->FindMethod(bIsAssembly ? "ReadStatics" : "Read")->Invoke(pSerializer);

	m_serializationTicks += CryGetTicks() - startTime;

	return pReadResult;
}
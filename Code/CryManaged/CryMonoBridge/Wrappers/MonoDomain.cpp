// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MonoDomain.h"

#include "MonoRuntime.h"
#include "MonoLibrary.h"
#include "MonoString.h"
#include "RootMonoDomain.h"

#include <CrySystem/IProjectManager.h>

CMonoDomain::~CMonoDomain()
{
	UnloadAssemblies();

	// Destroy libraries in reverse order, to resolve dependency issues
	for (auto it = m_loadedLibraries.rbegin(); it != m_loadedLibraries.rend(); ++it)
	{
		// Delete the CMonoLibrary object
		it->reset();
	}

	// Destroy the domain
	Unload();

	CleanTempDirectory();
}

bool CMonoDomain::Activate(bool bForce)
{
	return MonoInternals::mono_domain_set(m_pDomain, bForce) != 0;
}

bool CMonoDomain::IsActive() const
{
	return m_pDomain == MonoInternals::mono_domain_get(); 
}

std::shared_ptr<CMonoString> CMonoDomain::CreateString(const char* szString)
{
	return std::make_shared<CMonoString>(m_pDomain, szString);
}

std::shared_ptr<CMonoString> CMonoDomain::CreateString(MonoInternals::MonoString* pManagedString)
{
	return std::make_shared<CMonoString>(pManagedString);
}

string CMonoDomain::TempDirectoryPath()
{
#ifdef CRY_PLATFORM_WINDOWS 
	TCHAR tempPath[MAX_PATH];
	GetTempPathA(MAX_PATH, tempPath);

	string folderName = PathUtil::Make(tempPath, "CE_ManagedBinaries");
	int instance = gEnv->pSystem->GetApplicationInstance();

	if (instance != 0)
	{
		return folderName.AppendFormat("(%d)\\", instance);
	}
	else
	{
		return folderName.Append("\\");
	}
#else
#pragma error("TODO: Implement for other platforms");
#endif
}

void CMonoDomain::Unload()
{
	// Make sure we don't try to unload a domain that came from the managed side
	if (!m_bNativeDomain)
		return;

	if (m_pDomain == MonoInternals::mono_get_root_domain())
	{
		MonoInternals::mono_jit_cleanup(m_pDomain);
	}
	else
	{
		if (IsActive())
		{
			// Fall back to the root domain
			MonoInternals::mono_domain_set(MonoInternals::mono_get_root_domain(), true);
		}

		MonoInternals::mono_domain_finalize(m_pDomain, 2000);

		MonoInternals::MonoObject *pException;
		try
		{
			MonoInternals::mono_domain_try_unload(m_pDomain, &pException);
		}
		catch (char *ex)
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "An exception was raised during AppDomain unload: %s", ex);
		}

		if (pException != nullptr)
		{
			GetMonoRuntime()->HandleException((MonoInternals::MonoException*)pException);
		}
	}
}

void CMonoDomain::CacheObjectMethods()
{
	std::shared_ptr<CMonoClass> pObjectClass = GetMonoRuntime()->GetRootDomain()->GetNetCoreLibrary().GetTemporaryClass("System", "Object");
	m_pReferenceEqualsMethod = pObjectClass->FindMethod("ReferenceEquals", 2);

	if (std::shared_ptr<CMonoMethod> pReferenceEqualsMethod = m_pReferenceEqualsMethod.lock())
	{
		m_referenceEqualsThunk = static_cast<ReferenceEqualsFunction>(pReferenceEqualsMethod->GetUnmanagedThunk());
	}
	else
	{
		m_referenceEqualsThunk = nullptr;
	}
}

CMonoLibrary* CMonoDomain::LoadLibrary(const char* path, int loadIndex)
{
	string sPath = path;
	if (strcmp("dll", PathUtil::GetExt(sPath.c_str())))
	{
		sPath.append(".dll");
	}

	PathUtil::UnifyFilePath(sPath);

	for(const std::unique_ptr<CMonoLibrary>& pLibrary : m_loadedLibraries)
	{
		if (!stricmp(pLibrary->GetPath(), sPath))
		{
			return pLibrary.get();
		}
	}

	m_loadedLibraries.emplace_back(stl::make_unique<CMonoLibrary>(sPath, this));
	
	// Pass image data to the library, it has to be deleted when the assembly is done
	if (m_loadedLibraries.back()->Load(loadIndex))
	{
		return m_loadedLibraries.back().get();
	}
	else
	{
		m_loadedLibraries.pop_back();
	}

	return nullptr;
}

CMonoLibrary& CMonoDomain::GetLibraryFromMonoAssembly(MonoInternals::MonoAssembly* pAssembly, MonoInternals::MonoImage* pImage)
{
	for (const std::unique_ptr<CMonoLibrary>& pLibrary : m_loadedLibraries)
	{
		if (pLibrary.get()->GetAssembly() == pAssembly)
		{
			return *pLibrary.get();
		}
		else if (pLibrary.get()->GetImage() == pImage)
		{
			if (pAssembly != nullptr && pLibrary.get()->GetAssembly() == nullptr)
			{
				pLibrary.get()->SetAssembly(pAssembly);
			}

			return *pLibrary.get();
		}
	}

	MonoInternals::MonoAssemblyName* pAssemblyName = MonoInternals::mono_assembly_get_name(pAssembly);
	string assemblyPath = MonoInternals::mono_assembly_name_get_name(pAssemblyName);
	
	m_loadedLibraries.emplace_back(stl::make_unique<CMonoLibrary>(pAssembly, pImage, assemblyPath, this));
	return *m_loadedLibraries.back().get();
}

void CMonoDomain::UnloadAssemblies()
{
	for (auto it = m_loadedLibraries.rbegin(); it != m_loadedLibraries.rend(); ++it)
	{
		it->get()->Unload();
	}
}

void CMonoDomain::CleanTempDirectory()
{
	string tempDirectory = TempDirectoryPath();
	gEnv->pCryPak->RemoveDir(tempDirectory.c_str(), true);
}
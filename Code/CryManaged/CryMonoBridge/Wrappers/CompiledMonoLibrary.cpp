// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CompiledMonoLibrary.h"
#include "MonoRuntime.h"
#include "MonoLibrary.h"
#include "MonoDomain.h"

CCompiledMonoLibrary::CCompiledMonoLibrary(const char* szDirectory, CMonoDomain* pDomain)
	: CMonoLibrary(nullptr, "", pDomain)
	, m_directory(szDirectory)
{
	Load();
}

bool CCompiledMonoLibrary::Load()
{
	// Zero assembly and image, in case we are reloading
	m_pAssembly = nullptr;
	m_pImage = nullptr;

	std::vector<string> sourceFiles;
	FindSourceFilesInDirectoryRecursive(m_directory, sourceFiles);
	if (sourceFiles.size() == 0)
	{
		// Don't treat no assets as a failure, this is OK!
		return true;
	}

	CMonoLibrary* pCoreLibrary = GetMonoRuntime()->GetCryCoreLibrary();
	
	std::shared_ptr<CMonoClass> pCompilerClass = pCoreLibrary->GetTemporaryClass("CryEngine.Compilation", "Compiler");
	std::shared_ptr<CMonoMethod> pCompilationMethod = pCompilerClass->FindMethod("CompileCSharpSourceFiles", 2).lock();

	MonoInternals::MonoArray* pStringArray = MonoInternals::mono_array_new(m_pDomain->GetMonoDomain(), MonoInternals::mono_get_string_class(), sourceFiles.size());
	for (int i = 0; i < sourceFiles.size(); ++i)
	{
		mono_array_set(pStringArray, MonoInternals::MonoString*, i, mono_string_new(m_pDomain->GetMonoDomain(), sourceFiles[i]));
	}

	MonoInternals::MonoString* pOutString = nullptr;
	void* pParams[2] = { pStringArray, &pOutString };
	std::shared_ptr<CMonoObject> pResult = pCompilationMethod->InvokeStatic(pParams);
	bool hasLoadedAssembly = false;
	if (pResult)
	{
		if (MonoInternals::MonoReflectionAssembly* pReflectionAssembly = (MonoInternals::MonoReflectionAssembly*)pResult->GetManagedObject())
		{
			if (m_pAssembly = MonoInternals::mono_reflection_assembly_get_assembly(pReflectionAssembly))
			{
				m_pImage = MonoInternals::mono_assembly_get_image(m_pAssembly);
				m_assemblyPath = MonoInternals::mono_image_get_filename(m_pImage);
				hasLoadedAssembly = true;
			}
		}
	}
	else if(!m_assemblyPath.empty())
	{
		// Reload previously working assembly
		if (m_pAssembly = MonoInternals::mono_domain_assembly_open(m_pDomain->GetHandle(), m_assemblyPath))
		{
			m_pImage = MonoInternals::mono_assembly_get_image(m_pAssembly);
			hasLoadedAssembly = true;
		}
	}
	
	// If no assembly has been loaded, attempt to load the dll from the default location.
	// This prevents that no assemblies are loaded when the sandbox is started with compile errors in the C# assets.
	if(!hasLoadedAssembly)
	{
		const char* szAssemblyPath = "user/scripts/CRYENGINE.CSharp.dll";
		if (m_pAssembly = MonoInternals::mono_domain_assembly_open(m_pDomain->GetHandle(), szAssemblyPath))
		{
			m_pImage = MonoInternals::mono_assembly_get_image(m_pAssembly);
		}
	}

	const char* szCompileMessage = MonoInternals::mono_string_to_utf8(pOutString);
	GetMonoRuntime()->NotifyCompileFinished(szCompileMessage);

	return true;
}

void CCompiledMonoLibrary::FindSourceFilesInDirectoryRecursive(const char* szDirectory, std::vector<string>& sourceFiles)
{
	string searchPath = PathUtil::Make(szDirectory, "*.cs");

	_finddata_t fd;
	intptr_t handle = gEnv->pCryPak->FindFirst(searchPath, &fd);
	if (handle != -1)
	{
		do
		{
			sourceFiles.emplace_back(PathUtil::Make(szDirectory, fd.name));
		} while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

		gEnv->pCryPak->FindClose(handle);
	}

	// Find additional directories
	searchPath = PathUtil::Make(szDirectory, "*.*");

	handle = gEnv->pCryPak->FindFirst(searchPath, &fd);
	if (handle != -1)
	{
		do
		{
			if (fd.attrib & _A_SUBDIR)
			{
				if (strcmp(fd.name, ".") != 0 && strcmp(fd.name, "..") != 0)
				{
					string sDirectory = PathUtil::Make(szDirectory, fd.name);

					FindSourceFilesInDirectoryRecursive(sDirectory, sourceFiles);
				}
			}
		} while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

		gEnv->pCryPak->FindClose(handle);
	}
}
// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CompiledMonoLibrary.h"
#include "MonoRuntime.h"
#include "MonoLibrary.h"
#include "MonoDomain.h"
#include <CrySystem/IProjectManager.h>
#include <CrySystem/ICryPluginManager.h>
#include "AppDomain.h"

CCompiledMonoLibrary::CCompiledMonoLibrary(const char* szDirectory, CMonoDomain* pDomain)
	: CMonoLibrary(nullptr, nullptr, "", pDomain)
	, m_directory(szDirectory)
{
	string assemblyName = GetMonoRuntime()->GetGeneratedAssemblyName();
	m_assemblyPath = PathUtil::Make(PathUtil::Make(gEnv->pSystem->GetIProjectManager()->GetCurrentProjectDirectoryAbsolute(), "bin"), assemblyName + ".dll");
	Load(-1);
}

CCompiledMonoLibrary::CCompiledMonoLibrary(CMonoDomain* pDomain)
	: CMonoLibrary(nullptr, nullptr, "", pDomain)
{
	string assemblyName = GetMonoRuntime()->GetGeneratedAssemblyName();
	m_assemblyPath = PathUtil::Make(PathUtil::Make(gEnv->pSystem->GetIProjectManager()->GetCurrentProjectDirectoryAbsolute(), "bin"), assemblyName + ".dll");
	Load(-1);
}

bool CCompiledMonoLibrary::Load(int loadIndex)
{
	// Zero assembly and image, in case we are reloading
	m_pAssembly = nullptr;
	m_pImage = nullptr;

	int64 outErrorCount;
	std::vector<string> monoCompileErrrors;
	std::vector<SCSharpCompilerError> compileErrors;

	if (gEnv->IsEditor())
	{
		std::vector<string> sourceFiles;
		FindSourceFilesInDirectoryRecursive(m_directory, sourceFiles);
		if (sourceFiles.size() == 0)
		{
			// Don't treat no assets as a failure, this is OK!
			// The old dll file needs to be removed though.
			if (gEnv->pCryPak->IsFileExist(m_assemblyPath))
			{
				gEnv->pCryPak->RemoveFile(m_assemblyPath);

				// Remove debug files as well.
				string mdbFilePath = m_assemblyPath + ".mdb";
				string pdbFilePath = m_assemblyPath;
				pdbFilePath = PathUtil::ReplaceExtension(pdbFilePath, "pdb");
				if (gEnv->pCryPak->IsFileExist(mdbFilePath))
				{
					gEnv->pCryPak->RemoveFile(mdbFilePath);
				}
				if (gEnv->pCryPak->IsFileExist(pdbFilePath))
				{
					gEnv->pCryPak->RemoveFile(pdbFilePath);
				}
			}
			return true;
		}

		std::vector<string> pluginPaths;
		Cry::IProjectManager* projectManager = gEnv->pSystem->GetIProjectManager();
		uint16 pluginCount = projectManager->GetPluginCount();
		for (uint16 i = 0; i < pluginCount; ++i)
		{
			string path;
			Cry::IPluginManager::EType type;
			DynArray<EPlatform> platforms;
			projectManager->GetPluginInfo(i, type, path, platforms);
			if (type == Cry::IPluginManager::EType::Managed)
			{
				pluginPaths.emplace_back(path);
			}
		}

		CMonoLibrary* pCoreLibrary = GetMonoRuntime()->GetCryCoreLibrary();

		std::shared_ptr<CMonoClass> pCompilerClass = pCoreLibrary->GetTemporaryClass("CryEngine.Compilation", "Compiler");
		std::shared_ptr<CMonoMethod> pCompilationMethod = pCompilerClass->FindMethod("CompileCSharpSourceFiles", 5).lock();

		// Path where the assembly will be saved to.
		std::shared_ptr<CMonoString> pMonoAssemblyPath = pCoreLibrary->GetDomain()->CreateString(m_assemblyPath.c_str());

		// String array of all C# source files.
		MonoInternals::MonoArray* pSourceFilesStringArray = MonoInternals::mono_array_new(m_pDomain->GetMonoDomain(), MonoInternals::mono_get_string_class(), sourceFiles.size());
		for (int i = 0; i < sourceFiles.size(); ++i)
		{
			mono_array_set(pSourceFilesStringArray, MonoInternals::MonoString*, i, mono_string_new(m_pDomain->GetMonoDomain(), sourceFiles[i]));
		}

		// String array with paths to all managed plugins in this project.
		MonoInternals::MonoArray* pPluginsStringArray = MonoInternals::mono_array_new(m_pDomain->GetMonoDomain(), MonoInternals::mono_get_string_class(), pluginPaths.size());
		for (int i = 0; i < pluginPaths.size(); ++i)
		{
			mono_array_set(pPluginsStringArray, MonoInternals::MonoString*, i, mono_string_new(m_pDomain->GetMonoDomain(), pluginPaths[i]));
		}

		MonoInternals::MonoArray* pArray = MonoInternals::mono_array_new(GetMonoRuntime()->GetActiveDomain()->GetHandle(), MonoInternals::mono_get_string_class(), 0);
		void* pParams[5] = { pMonoAssemblyPath->GetManagedObject(), pSourceFilesStringArray, pPluginsStringArray, &pArray, &outErrorCount };
		pCompilationMethod->InvokeStatic(pParams);
		// TODO: Refactor asap, when mono arrays are usable through C++
		// NOTE: We do multiply by five at the moment because we want the length of the array returned by mono. Every compiler error will have inserted five values into the array.
		const size_t errorCount = static_cast<size_t>(outErrorCount * 5);
		// Loop through mono array to fill mono compile errors.
		for (size_t i = 0; i < errorCount; ++i)
		{
			MonoInternals::MonoObject* pObjectString = mono_array_get(pArray, MonoInternals::MonoObject*, i);
			MonoInternals::MonoString* pMonoString = reinterpret_cast<MonoInternals::MonoString*>(pObjectString);
			CMonoString managedValue(pMonoString);
			const char* szErrorMessage = managedValue.GetString();
			CRY_ASSERT_MESSAGE(szErrorMessage, "Error message must be non-null.");
			if (szErrorMessage)
			{
				monoCompileErrrors.emplace_back(szErrorMessage);
			}
		}
		compileErrors.reserve(static_cast<size_t>(outErrorCount));
		// Loop through mono compile errors to fill compile errors vector.
		for (auto it = monoCompileErrrors.begin(); it != monoCompileErrrors.end(); ++it)
		{
			const bool isWarning = *it == "False" ? false : true;
			string errorNumber(*(++it));
			string errorText(*(++it));
			string fileName(*(++it));
			const int32 line = std::stoi((++it)->c_str());
			compileErrors.emplace_back(isWarning, errorNumber, errorText, fileName, line);
		}
		// ~TODO
	}

	// Load the assembly by default, even if compiling might have failed because the Sandbox can use the previous assembly.
	if (!m_assemblyPath.IsEmpty())
	{
		LoadLibraryFile(m_assemblyPath);
	}

	if (gEnv->IsEditor())
	{
		GetMonoRuntime()->NotifyCompileFinished(compileErrors);
	}

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
		}
		while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

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
		}
		while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

		gEnv->pCryPak->FindClose(handle);
	}
}
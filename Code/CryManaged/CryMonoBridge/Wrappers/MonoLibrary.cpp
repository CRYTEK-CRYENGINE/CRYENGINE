// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MonoLibrary.h"

#include "MonoRuntime.h"
#include "MonoDomain.h"
#include "MonoClass.h"
#include "MonoObject.h"
#include "MonoException.h"

#include "AppDomain.h"
#include "RootMonoDomain.h"

CMonoLibrary::CMonoLibrary(const char* filePath, CMonoDomain* pDomain)
	: CMonoLibrary(nullptr, nullptr, filePath, pDomain)
{
}

CMonoLibrary::CMonoLibrary(MonoInternals::MonoAssembly* pAssembly, MonoInternals::MonoImage* pImage, const char* filePath, CMonoDomain* pDomain)
	: m_pAssembly(pAssembly)
	, m_pDomain(pDomain)
	, m_assemblyPath(filePath)
	, m_pImage(pImage)
{
}

CMonoLibrary::~CMonoLibrary()
{
	// Destroy classes before closing the assembly
	m_classes.clear();

	Unload();
}

bool CMonoLibrary::Load(int loadIndex)
{
	// Loading with CryPak disabled for now, need to make sure that chain-reload works correctly and that images are closed.
	//#define LOAD_MONO_LIBRARIES_WITH_CRYPAK
#ifdef LOAD_MONO_LIBRARIES_WITH_CRYPAK
	// Now load from disk
	CCryFile file;
	file.Open(m_assemblyPath, "rb", ICryPak::FLAGS_PATH_REAL);

	if (file.GetHandle() == nullptr)
	{
		// Search in additional binary directories
		for (const string& binaryDirectory : GetMonoRuntime()->GetBinaryDirectories())
		{
			m_assemblyPath = PathUtil::Make(binaryDirectory, m_assemblyPath);

			file.Open(m_assemblyPath, "rb", ICryPak::FLAGS_PATH_REAL);
			if (file.GetHandle() != nullptr)
			{
				break;
			}
		}

		if (file.GetHandle() == nullptr)
		{
			return nullptr;
		}
	}

	m_assemblyData.resize(file.GetLength());
	file.SeekToBegin();
	file.ReadType(m_assemblyData.data(), m_assemblyData.size());

	MonoImageOpenStatus status = MONO_IMAGE_ERROR_ERRNO;

	// Load the assembly image
	m_pImage = MonoInternals::mono_image_open_from_data_with_name(m_assemblyData.data(), m_assemblyData.size(), false, &status, false, nullptr);

	// Make sure to load references this image requires before loading the assembly itself
	for (int i = 0; i < MonoInternals::mono_image_get_table_rows(m_pImage, MONO_TABLE_ASSEMBLYREF); ++i)
	{
		MonoInternals::mono_assembly_load_reference(m_pImage, i);
	}

	gEnv->pLog->LogWithType(IMiniLog::eMessage, "[Mono] Load Library: %s", m_assemblyPath.c_str());

	#ifndef _RELEASE
	// load debug information
	string sDebugDatabasePath = m_assemblyPath;
	PathUtil::ReplaceExtension(sDebugDatabasePath, ".pdb");

	CCryFile debugFile;
	debugFile.Open(sDebugDatabasePath, "rb", ICryPak::FLAGS_PATH_REAL);
	if (debugFile.GetHandle() != nullptr)
	{
		m_assemblyDebugData.resize(debugFile.GetLength());
		debugFile.SeekToBegin();
		debugFile.ReadType(m_assemblyDebugData.data(), m_assemblyDebugData.size());

		MonoInternals::mono_debug_open_image_from_memory(m_pImage, m_assemblyDebugData.data(), m_assemblyPath.size());
	}
	#endif

	m_pAssembly = MonoInternals::mono_assembly_load_from_full(m_pImage, m_assemblyPath.c_str(), &status, false);

	return m_pAssembly != nullptr;
#else
	return LoadLibraryFile(m_assemblyPath, loadIndex);
#endif
}

bool CMonoLibrary::LoadLibraryFile(string& assemblyPath, int loadIndex)
{
	string tempAssemblyPath = assemblyPath;

	// Do a copy of the binary on windows to allow reload during development
	// Otherwise Mono will lock the file.
#if defined(CRY_PLATFORM_WINDOWS) && !defined(RELEASE)
	if (gEnv->pCryPak->IsFileExist(tempAssemblyPath))
	{
		string tempBinaryDirectory = m_pDomain->TempDirectoryPath();

		// Non-engine assemblies need to be put in a separate folder so they won't conflict with eachother.
		if (loadIndex != -1)
		{
			string assemblyFolderName = "ManagedAssembly";
			tempBinaryDirectory = PathUtil::Make(tempBinaryDirectory, assemblyFolderName.AppendFormat("_%d\\", loadIndex));
		}

		DWORD attribs = GetFileAttributesA(tempBinaryDirectory.c_str());
		if (attribs == INVALID_FILE_ATTRIBUTES)
		{
			CryCreateDirectory(tempBinaryDirectory.c_str());
		}

		string fileName = PathUtil::GetFile(assemblyPath);
		tempAssemblyPath = PathUtil::Make(tempBinaryDirectory, fileName);

		// If mdb
		string mdbPathSource = assemblyPath + ".mdb";
		string mdbPathTarget = tempAssemblyPath + ".mdb";

		// Also copy debug databases, if present
		string pdbPathSource = PathUtil::ReplaceExtension(assemblyPath, ".pdb");
		string pdbPathTarget = PathUtil::ReplaceExtension(tempAssemblyPath, ".pdb");

		// We are only interested in the latest debug symbols, so only copy the latest one.
		if (IsFileNewer(pdbPathSource, mdbPathSource))
		{
			mdbPathTarget = "";
		}
		else
		{
			pdbPathTarget = "";
		}

		// It could be that old .dll, mdb and .pdb files are still in the temporary directory.
		// This can cause unexpected behavior or out of sync debug-symbols, so delete the old files first.
		RemoveAndCopyFile(mdbPathSource, mdbPathTarget);
		RemoveAndCopyFile(pdbPathSource, pdbPathTarget);
		RemoveAndCopyFile(assemblyPath, tempAssemblyPath);
	}
#endif

	m_pAssembly = MonoInternals::mono_domain_assembly_open(m_pDomain->GetMonoDomain(), tempAssemblyPath);

	if (m_pAssembly != nullptr)
	{
		m_pImage = MonoInternals::mono_assembly_get_image(m_pAssembly);

		return true;
	}

	return false;
}

void CMonoLibrary::Unload()
{
	for (auto it = m_classes.begin(); it != m_classes.end(); ++it)
	{
		it->get()->OnAssemblyUnload();
	}
}

void CMonoLibrary::Reload()
{
	if (Load())
	{
		for (auto it = m_classes.begin(); it != m_classes.end(); ++it)
		{
			it->get()->ReloadClass();
		}
	}
}

void CMonoLibrary::Serialize(CMonoObject* pSerializer)
{
	if (!CanSerialize())
	{
		return;
	}

	// Make sure we know the file path to this binary, we'll need to reload it later.
	// This is automatically cached in GetFilePath.
	GetFilePath();

	static_cast<CAppDomain*>(m_pDomain)->SerializeObject(pSerializer, m_pAssembly != nullptr ? GetManagedObject() : nullptr, true);

	for (auto it = m_classes.begin(); it != m_classes.end(); ++it)
	{
		it->get()->Serialize(pSerializer);
	}
}

void CMonoLibrary::Deserialize(CMonoObject* pSerializer)
{
	if (!CanSerialize())
	{
		return;
	}

	static_cast<CAppDomain*>(m_pDomain)->DeserializeObject(pSerializer, nullptr);

	for (auto it = m_classes.begin(); it != m_classes.end(); ++it)
	{
		it->get()->Deserialize(pSerializer);
	}
}

bool CMonoLibrary::CanSerialize() const
{
	return m_pImage != MonoInternals::mono_get_corlib();
}

CMonoClass* CMonoLibrary::GetClass(const char* szNamespace, const char* szClassName)
{
	for (auto it = m_classes.begin(); it != m_classes.end(); ++it)
	{
		if (!strcmp(it->get()->GetNamespace(), szNamespace) && !strcmp(it->get()->GetName(), szClassName))
		{
			return it->get();;
		}
	}

	m_classes.emplace_back(std::make_shared<CMonoClass>(this, szNamespace, szClassName));

	std::shared_ptr<CMonoClass> pClass = m_classes.back();
	if (pClass->GetMonoClass() == nullptr)
	{
		m_classes.pop_back();
		return nullptr;
	}

	pClass->SetWeakPointer(pClass);

	return pClass.get();
}

std::shared_ptr<CMonoClass> CMonoLibrary::GetTemporaryClass(const char* szNamespace, const char* szClassName)
{
	std::shared_ptr<CMonoClass> pClass = std::make_shared<CMonoClass>(this, szNamespace, szClassName);
	if (pClass->GetMonoClass() == nullptr)
	{
		return nullptr;
	}

	// Since this class may expire at any time the class has to contain a weak pointer of itself
	// This is converted to a shared_ptr when the class creates objects, to ensure that the class always outlives its instances
	pClass->SetWeakPointer(pClass);

	return pClass;
}

std::shared_ptr<CMonoException> CMonoLibrary::GetExceptionImplementation(const char* szNamespace, const char* szExceptionClassName, const char* szMessage)
{
	MonoInternals::MonoException* pException;
	if (szMessage != nullptr)
	{
		pException = MonoInternals::mono_exception_from_name_msg(m_pImage, szNamespace, szExceptionClassName, szMessage);
	}
	else
	{
		pException = MonoInternals::mono_exception_from_name(m_pImage, szNamespace, szExceptionClassName);
	}

	return std::make_shared<CMonoException>(pException);
}

std::shared_ptr<CMonoClass> CMonoLibrary::GetClassFromMonoClass(MonoInternals::MonoClass* pMonoClass)
{
	for (auto it = m_classes.begin(); it != m_classes.end(); ++it)
	{
		if (it->get()->GetMonoClass() == pMonoClass)
		{
			return *it;
		}
	}

	std::shared_ptr<CMonoClass> pClass = std::make_shared<CMonoClass>(this, pMonoClass);
	pClass->SetWeakPointer(pClass);
	m_classes.emplace_back(pClass);

	return pClass;
}

MonoInternals::MonoObject* CMonoLibrary::GetManagedObject()
{
	return (MonoInternals::MonoObject*)MonoInternals::mono_assembly_get_object(m_pDomain->GetMonoDomain(), m_pAssembly);
}

const char* CMonoLibrary::GetImageName() const
{
	return MonoInternals::mono_image_get_name(m_pImage);
}

bool CMonoLibrary::RemoveAndCopyFile(string& sourceFile, string& targetFile) const
{
	// The path can be relative, so the default IsFileExist is not a sure way to check if the file exists.
	// Instead we try opening the file and if it works the file exists.
	if (FILE* handle = gEnv->pCryPak->FOpen(targetFile, "rb", ICryPak::FOPEN_HINT_QUIET | ICryPak::FLAGS_PATH_REAL))
	{
		gEnv->pCryPak->FClose(handle);
		gEnv->pCryPak->RemoveFile(targetFile);
	}
	
	if (targetFile.empty())
	{
		return true;
	}

	if (FILE* handle = gEnv->pCryPak->FOpen(sourceFile, "rb", ICryPak::FOPEN_HINT_QUIET | ICryPak::FLAGS_PATH_REAL))
	{
		gEnv->pCryPak->FClose(handle);
		return gEnv->pCryPak->CopyFileOnDisk(sourceFile, targetFile, false);
	}
	return false;
}

bool CMonoLibrary::IsFileNewer(string& fileA, string& fileB) const
{
	uint64 timeA = 0;
	uint64 timeB = 0;
	if (FILE* handle = gEnv->pCryPak->FOpen(fileA, "rb", ICryPak::FOPEN_HINT_QUIET | ICryPak::FLAGS_PATH_REAL))
	{
		timeA = gEnv->pCryPak->GetModificationTime(handle);
		gEnv->pCryPak->FClose(handle);
	}
	
	if (FILE* handle = gEnv->pCryPak->FOpen(fileB, "rb", ICryPak::FOPEN_HINT_QUIET | ICryPak::FLAGS_PATH_REAL))
	{
		timeB = gEnv->pCryPak->GetModificationTime(handle);
		gEnv->pCryPak->FClose(handle);
	}

	return timeA > timeB;
}

const char* CMonoLibrary::GetFilePath()
{
	if (m_assemblyPath.size() == 0 && m_pAssembly != nullptr)
	{
		MonoInternals::MonoAssemblyName* pAssemblyName = MonoInternals::mono_assembly_get_name(m_pAssembly);
		m_assemblyPath = MonoInternals::mono_assembly_name_get_name(pAssemblyName);
	}

	return m_assemblyPath;
}

CMonoDomain* CMonoLibrary::GetDomain() const
{
	return m_pDomain;
}

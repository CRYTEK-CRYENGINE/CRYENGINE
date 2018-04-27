// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "MonoException.h"

class CMonoDomain;
class CMonoClass;
class CMonoObject;

struct MonoAssembly;
struct MonoImage;
struct MonoClass;
struct MonoObject;

class CMonoLibrary
{
	friend class CAppDomain;
	friend class CMonoDomain;

	// Start public API below
public:
	CMonoLibrary(const char* filePath, CMonoDomain* pDomain);
	CMonoLibrary(MonoInternals::MonoAssembly* pAssembly, MonoInternals::MonoImage* pImage, const char* filePath, CMonoDomain* pDomain);
	~CMonoLibrary();
	
	bool IsLoaded() const { return m_pAssembly != nullptr; }
	virtual bool WasCompiledAtRuntime() { return false; }

	const char* GetFilePath();

	CMonoDomain* GetDomain() const;

	// Finds a class inside this assembly, returns null if the lookup failed
	// The class will be stored inside the assembly on success and automatically updated on domain reload
	CMonoClass* GetClass(const char *nameSpace, const char *className);

	// Finds a class inside this assembly, returns null if the lookup failed
	// The pointer returned is entirely temporary, and will be destroyed when the shared pointer goes out of scope
	std::shared_ptr<CMonoClass> GetTemporaryClass(const char *nameSpace, const char *className);
	std::shared_ptr<CMonoException> GetExceptionImplementation(const char* nameSpace, const char* exceptionClass, const char* message = "");
	
	std::shared_ptr<CMonoException> GetException(const char *nameSpace, const char *exceptionClass, const char *message, ...)
	{
		va_list	args;
		char szBuffer[4096];
		va_start(args, message);
		int count = cry_vsprintf(szBuffer, sizeof(szBuffer), message, args);
		if (count == -1 || count >= sizeof(szBuffer))
			szBuffer[sizeof(szBuffer) - 1] = '\0';
		va_end(args);

		return GetExceptionImplementation(nameSpace, exceptionClass, szBuffer);
	}

	std::shared_ptr<CMonoClass> GetClassFromMonoClass(MonoInternals::MonoClass* pClass);

	void SetAssembly(MonoInternals::MonoAssembly* pAssembly) { m_pAssembly = pAssembly; }
	MonoInternals::MonoAssembly* GetAssembly() const { return m_pAssembly; }
	MonoInternals::MonoImage* GetImage() const { return m_pImage; }

	MonoInternals::MonoObject* GetManagedObject();

protected:
	virtual bool Load(int loadIndex = -1);
	bool LoadLibraryFile(string& assemblyPath, int loadIndex = -1);
	void Unload();
	void Reload();

	void Serialize(CMonoObject* pSerializer);
	void Deserialize(CMonoObject* pSerializer);

	bool CanSerialize() const;

	const char* GetPath() const { return m_assemblyPath; }
	const char* GetImageName() const;

	// Removes the file at targetFile, and copies the sourceFile to targetFile.
	// Returns false if copying failed.
	bool RemoveAndCopyFile(string& sourceFile, string& targetFile) const;
	
	// Returns true if fileA is newer that fileB, false otherwise.
	bool IsFileNewer(string& fileA, string& fileB) const;

protected:
	MonoInternals::MonoAssembly* m_pAssembly;
	MonoInternals::MonoImage* m_pImage;

	string        m_assemblyPath;

	CMonoDomain*   m_pDomain;

	std::list<std::shared_ptr<CMonoClass>> m_classes;

	std::vector<char> m_assemblyData;
	std::vector<unsigned char> m_assemblyDebugData;
};
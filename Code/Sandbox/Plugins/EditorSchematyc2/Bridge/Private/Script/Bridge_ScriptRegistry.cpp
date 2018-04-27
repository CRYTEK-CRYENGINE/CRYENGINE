// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Bridge_ScriptRegistry.h"

#include "Bridge_ScriptFile.h"
#include <CrySchematyc2/Script/IScriptRegistry.h>


namespace Bridge {

//////////////////////////////////////////////////////////////////////////
IScriptFile* CScriptRegistry::LoadFile(const char* szFileName)
{
	if (Schematyc2::IScriptFile* pFile = Delegate()->LoadFile(szFileName))
	{
		return Wrapfile(pFile);
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
IScriptFile* CScriptRegistry::CreateFile(const char* szFileName, Schematyc2::EScriptFileFlags flags)
{
	if (Schematyc2::IScriptFile* pFile = Delegate()->CreateFile(szFileName, flags))
	{
		return Wrapfile(pFile);
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
IScriptFile* CScriptRegistry::GetFile(const Schematyc2::SGUID& guid)
{
	if (Schematyc2::IScriptFile* pFile = Delegate()->GetFile(guid))
	{
		return Wrapfile(pFile);
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
IScriptFile* CScriptRegistry::GetFile(const char* szFileName)
{
	if (Schematyc2::IScriptFile* pFile = Delegate()->GetFile(szFileName))
	{
		return Wrapfile(pFile);
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CScriptRegistry::VisitFiles(const ScriptFileVisitor& visitor, const char* szFilePath)
{
	SCHEMATYC2_SYSTEM_ASSERT(visitor);
	if (visitor)
	{
		for (FilesBySchematycFiles::iterator itFile = m_filesBySchematycFiles.begin(), itEndFile = m_filesBySchematycFiles.end(); itFile != itEndFile; ++itFile)
		{
			IScriptFile& file = *itFile->second;
			if (!szFilePath || CryStringUtils::stristr(file.GetFileName(), szFilePath))
			{
				if (visitor(file) != Schematyc2::EVisitStatus::Continue)
				{
					return;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CScriptRegistry::VisitFiles(const ScriptFileConstVisitor& visitor, const char* szFilePath) const
{
	SCHEMATYC2_SYSTEM_ASSERT(visitor);
	if (visitor)
	{
		for (FilesBySchematycFiles::const_iterator itFile = m_filesBySchematycFiles.begin(), itEndFile = m_filesBySchematycFiles.end(); itFile != itEndFile; ++itFile)
		{
			const IScriptFile& file = *itFile->second;
			if (!szFilePath || CryStringUtils::stristr(file.GetFileName(), szFilePath))
			{
				if (visitor(file) != Schematyc2::EVisitStatus::Continue)
				{
					return;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CScriptRegistry::RefreshFiles(const Schematyc2::SScriptRefreshParams& params)
{
	Delegate()->RefreshFiles(params);
}

//////////////////////////////////////////////////////////////////////////
bool CScriptRegistry::Load()
{
	return Delegate()->Load();
}

//////////////////////////////////////////////////////////////////////////
void CScriptRegistry::Save(bool bAlwaysSave)
{
	Delegate()->Save(bAlwaysSave);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptModule* CScriptRegistry::AddModule(const char* szName, Schematyc2::IScriptElement* pScope)
{
	return Delegate()->AddModule(szName, pScope);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptEnumeration* CScriptRegistry::AddEnumeration(const char* szName, Schematyc2::IScriptElement* pScope)
{
	return Delegate()->AddEnumeration(szName, pScope);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptFunction* CScriptRegistry::AddFunction(const char* szName, Schematyc2::IScriptElement* pScope)
{
	return Delegate()->AddFunction(szName, pScope);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptClass* CScriptRegistry::AddClass(const char* szName, const Schematyc2::SGUID& foundationGUID, Schematyc2::IScriptElement* pScope)
{
	return Delegate()->AddClass(szName, foundationGUID, pScope);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptElement* CScriptRegistry::GetElement(const Schematyc2::SGUID& guid)
{
	return Delegate()->GetElement(guid);
}

//////////////////////////////////////////////////////////////////////////
void CScriptRegistry::RemoveElement(const Schematyc2::SGUID& guid)
{
	Delegate()->RemoveElement(guid);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptRegistry::VisitElements(const Schematyc2::ScriptElementVisitor& visitor, Schematyc2::IScriptElement* pScope, Schematyc2::EVisitFlags flags)
{
	return Delegate()->VisitElements(visitor, pScope, flags);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::EVisitStatus CScriptRegistry::VisitElements(const Schematyc2::ScriptElementConstVisitor& visitor, const Schematyc2::IScriptElement* pScope, Schematyc2::EVisitFlags flags) const
{
	return Delegate()->VisitElements(visitor, pScope, flags);
}

//////////////////////////////////////////////////////////////////////////
bool CScriptRegistry::IsElementNameUnique(const char* szName, Schematyc2::IScriptElement* pScope) const
{
	return Delegate()->IsElementNameUnique(szName, pScope);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::SScriptRegistrySignals& CScriptRegistry::Signals()
{
	return Delegate()->Signals();
}

//////////////////////////////////////////////////////////////////////////
Bridge::IScriptFile* CScriptRegistry::Wrapfile(Schematyc2::IScriptFile* file)
{
	CRY_ASSERT(file);

	auto itFile = m_filesBySchematycFiles.find(file);
	if (itFile == m_filesBySchematycFiles.end())
	{
		CScriptFilePtr pFile(new CScriptFile(file));
		m_filesBySchematycFiles.insert(FilesBySchematycFiles::value_type(file, pFile));

		return pFile.get();
	}

	return itFile->second.get();
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IScriptRegistry* CScriptRegistry::Delegate() const
{
	CRY_ASSERT(gEnv->pSchematyc2);
	return &gEnv->pSchematyc2->GetScriptRegistry();
}

}

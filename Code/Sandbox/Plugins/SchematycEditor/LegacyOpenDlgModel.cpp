// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LegacyOpenDlgModel.h"

#include <CrySchematyc2/IFramework.h>
#include <CrySchematyc2/Script/IScriptRegistry.h>

#include "Util.h"

namespace Cry {
namespace SchematycEd {

uint32 CLegacyOpenDlgEntry::GetType() const
{
	return CAbstractDictionaryEntry::Type_Entry;
}

QVariant CLegacyOpenDlgEntry::GetColumnValue(int32 columnIndex) const
{
	switch (columnIndex)
	{
		case CLegacyOpenDlgModel::Column_Name:
			return QVariant::fromValue(m_name);	
		default:
			break;	
	}

	return QVariant();
}

QString  CLegacyOpenDlgEntry::GetToolTip() const
{
	return m_description;
}

QString CLegacyOpenDlgEntry::GetName() const
{
	return m_name;
}

QString CLegacyOpenDlgEntry::GetFullName() const
{
	return m_fullName;
}

SGUID CLegacyOpenDlgEntry::GetTypeGUID()
{
	return m_guid;
}


CLegacyOpenDlgModel::CLegacyOpenDlgModel()
{
	Load();
}

CLegacyOpenDlgModel::~CLegacyOpenDlgModel()
{
}

int32 CLegacyOpenDlgModel::GetNumEntries() const
{
	return m_entries.size();
}

const CAbstractDictionaryEntry* CLegacyOpenDlgModel::GetEntry(int32 index) const
{
	if (index < m_entries.size())
	{
		return &m_entries[index];
	}

	return nullptr;
}

int32 CLegacyOpenDlgModel::GetNumColumns() const
{
	return Column_Count;
};

QString CLegacyOpenDlgModel::GetColumnName(int32 index) const
{
	switch (index)
	{
		case Column_Name:
			return QString("Name");
		default:
			break;
	}

	return QString();
}

int32 CLegacyOpenDlgModel::GetDefaultFilterColumn() const
{
	return Column_Name;
}

int32 CLegacyOpenDlgModel::GetDefaultSortColumn() const
{
	return Column_Name;
}

void CLegacyOpenDlgModel::Load()
{
	m_entries.reserve(100);

	using VisitClassFunc = std::function<EVisitStatus(const IScriptClass&, const stack_string&)>;
	VisitClassFunc visitClassFunc = [this](const IScriptClass& scriptClass, const stack_string& filePath)
	{
		CLegacyOpenDlgEntry entry;

		entry.m_name        = scriptClass.GetName();
		entry.m_guid        = scriptClass.GetGUID();
		entry.m_fullName    = filePath;
		entry.m_description = scriptClass.GetDescription();

		m_entries.emplace_back(entry);

		return EVisitStatus::Continue;
	};

	using VisitScriptsFunc = std::function<void(const VisitClassFunc& visitClassFunc, const stack_string&)>;
	static VisitScriptsFunc visitScriptsFunc = [](const VisitClassFunc& visitClassFunc, const stack_string& path)
	{
		TStringVector subFolderNames;
		TStringVector fileNames;

		GetSubFoldersAndFileNames(path.c_str(), "*.*", true, subFolderNames, fileNames);

		for (auto& fileName : fileNames)
		{
			stack_string scriptFileName = path;
			scriptFileName.append("/");
			scriptFileName.append(fileName.c_str());

			const IScriptFile* pScriptFile = gEnv->pSchematyc2->GetScriptRegistry().GetFile(scriptFileName.c_str());
			if (pScriptFile)
			{
				auto lambda = std::bind(visitClassFunc, std::placeholders::_1, scriptFileName);

				SGUID scopeGUID;
				pScriptFile->VisitClasses(ScriptClassConstVisitor::FromLambdaFunction(lambda), scopeGUID, false);
			}
		}

		for (auto& subFolderName : subFolderNames)
		{
			stack_string subFolderPath = path;
			subFolderPath.append("/");
			subFolderPath.append(subFolderName.c_str());

			visitScriptsFunc(visitClassFunc, subFolderPath);
		}
	};

	visitScriptsFunc(visitClassFunc, gEnv->pSchematyc2->GetOldScriptsFolder());
}


} //namespace SchematycEd
} //namespace Cry

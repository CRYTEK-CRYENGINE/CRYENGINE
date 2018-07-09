// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleNodeTreeModel.h"

namespace CryParticleEditor {

QVariant CNodesDictionaryNode::GetColumnValue(int32 columnIndex) const
{
	switch (columnIndex)
	{
	case CNodesDictionary::eColumn_Name:
		{
			return QVariant::fromValue(m_name);
		}
	case CNodesDictionary::eColumn_Identifier:
		{
			return GetIdentifier();
		}
	default:
		break;
	}

	return QVariant();
}

const CAbstractDictionaryEntry* CNodesDictionaryNode::GetParentEntry() const
{
	return static_cast<const CAbstractDictionaryEntry*>(m_pParent);
}

QVariant CNodesDictionaryNode::GetIdentifier() const
{
	return QVariant::fromValue(m_filePath);
}

QVariant CNodesDictionaryCategory::GetColumnValue(int32 columnIndex) const
{
	switch (columnIndex)
	{
	case CNodesDictionary::eColumn_Name:
		{
			return QVariant::fromValue(m_name);
		}
	default:
		break;
	}

	return QVariant();
}

const CAbstractDictionaryEntry* CNodesDictionaryCategory::GetChildEntry(int32 index) const
{
	if (index < m_categories.size())
	{
		return static_cast<const CAbstractDictionaryEntry*>(m_categories[index]);
	}
	const int32 nodeIndex = index - m_categories.size();
	if (nodeIndex < m_nodes.size())
	{
		return static_cast<const CAbstractDictionaryEntry*>(m_nodes[nodeIndex]);
	}
	return nullptr;
}

const CAbstractDictionaryEntry* CNodesDictionaryCategory::GetParentEntry() const
{
	return static_cast<const CAbstractDictionaryEntry*>(m_pParent);
}

CNodesDictionaryCategory& CNodesDictionaryCategory::CreateCategory(QString name)
{
	m_categories.emplace_back(new CNodesDictionaryCategory(name, this));
	return *m_categories.back();
}

CNodesDictionaryNode& CNodesDictionaryCategory::CreateNode(QString name, QString filePath)
{
	m_nodes.emplace_back(new CNodesDictionaryNode(name, filePath, this));
	return *m_nodes.back();
}

CNodesDictionary::CNodesDictionary()
	: m_root("root")
{
	LoadTemplates();
}

CNodesDictionary::~CNodesDictionary()
{

}

const CAbstractDictionaryEntry* CNodesDictionary::GetEntry(int32 index) const
{
	return m_root.GetChildEntry(index);
}

void CNodesDictionary::LoadTemplates()
{
	const string assetDir = "%ENGINE%/EngineAssets/Particles/";
	const size_t extensionLength = sizeof(".pfxp") - 1;

	std::stack<string> dirStack;
	std::stack<CNodesDictionaryCategory*> catStack;
	dirStack.push(assetDir);
	catStack.push(&m_root);

	ICryPak* pPak = gEnv->pCryPak;

	while (dirStack.size())
	{
		const string mask = PathUtil::Make(dirStack.top(), "*");
		const string path = dirStack.top();
		CNodesDictionaryCategory* pCategory = catStack.top();
		dirStack.pop();
		catStack.pop();

		_finddata_t data;
		intptr_t handle = pPak->FindFirst(mask.c_str(), &data);
		if (handle != -1)
		{
			do
			{
				if (!strcmp(data.name, ".") || !strcmp(data.name, ".."))
					continue;				

				if ((data.attrib & _A_SUBDIR) != 0)
				{
					dirStack.push(PathUtil::Make(path.c_str(), data.name));
					catStack.push(&pCategory->CreateCategory(data.name));
				}
				else if (stricmp(PathUtil::GetExt(data.name), "pfxp") == 0)
				{
					const string filename(data.name);
					const QString name = filename.substr(0, filename.length() - extensionLength);
					const QString filePath = path + "/" + filename;
					pCategory->CreateNode(name, filePath);
				}				
			}
			while (pPak->FindNext(handle, &data) >= 0);
			pPak->FindClose(handle);
		}
	}
}

}


// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "LegacyAssetTypes.h"
#include "DataBaseDialog.h"
#include "EntityPrototypeManager.h"
#include "GameTokens/GameTokenManager.h"
#include "Particles/ParticleDialog.h"
#include "Particles/ParticleManager.h"
#include "Prefabs/PrefabManager.h"
#include "QtUtil.h"

REGISTER_ASSET_TYPE(CScriptType)
REGISTER_ASSET_TYPE(CXmlType)
REGISTER_ASSET_TYPE(CAnimatedMeshType)
REGISTER_ASSET_TYPE(CMeshAnimationType)

namespace Private_LegacyAssetTypes
{

static const char* const s_szRootTag = "rootTag";

struct SXmlType
{
	const char* const m_szXmlTag;
	const char* const m_szXmlType;
	const EDataBaseItemType m_dataBaseItemType;
	CBaseLibraryManager* const m_pLibraryManager;
};

// Unfortunately, root node tags might alias. For example, an XML describing an entity named "GameTokensLibrary"
// will also be recognized as a library.
static const std::vector<SXmlType>& GetXmlTypes()
{
	static const std::vector<SXmlType> xmlTypes
	{
		{
			{ "EntityPrototypeLibrary", "Entity Archetype Library", EDB_TYPE_ENTITY_ARCHETYPE, GetIEditorImpl()->GetEntityProtManager() },
			{ "GameTokensLibrary", "Game Token Library", EDB_TYPE_GAMETOKEN, GetIEditorImpl()->GetGameTokenManager() },
			{ "ParticleLibrary", "Particle Library", EDB_TYPE_PARTICLE, GetIEditorImpl()->GetParticleManager() },
			{ "PrefabsLibrary", "Prefab Library", EDB_TYPE_PREFAB, GetIEditorImpl()->GetPrefabManager() }
		}
	};
	return xmlTypes;
}

static const SXmlType* GetXmlTypeFromXmlTag(const char* szTag)
{
	const std::vector<SXmlType>& xmlTypes = GetXmlTypes();
	for (const SXmlType& type : xmlTypes)
	{
		if (!stricmp(type.m_szXmlTag, szTag))
		{
			return &type;
		}
	}
	return nullptr;
}

static QStringList PopulateXmlTypeAttribute()
{
	const std::vector<SXmlType>& xmlTypes = GetXmlTypes();
	QStringList out;
	out.reserve(xmlTypes.size());
	for (const SXmlType& type : xmlTypes)
	{
		out.push_back(QString(type.m_szXmlType));
	}
	return out;
}

} // namespace Private_LegacyAssetTypes

CItemModelAttributeEnumFunc CXmlType::s_xmlTypeAttribute("XML Type", &Private_LegacyAssetTypes::PopulateXmlTypeAttribute, CItemModelAttribute::StartHidden);

std::vector<CItemModelAttribute*> CXmlType::GetDetails() const
{
	return { &s_xmlTypeAttribute };
}

QVariant CXmlType::GetDetailValue(const CAsset* pAsset, const CItemModelAttribute* pDetail) const
{
	using namespace Private_LegacyAssetTypes;
	const SXmlType* const pXmlType = GetXmlTypeFromXmlTag(pAsset->GetDetailValue(s_szRootTag).c_str());
	if (pXmlType)
	{
		return QtUtil::ToQString(pXmlType->m_szXmlType);
	}
	else
	{
		return QVariant();
	}
}

CAssetEditor* CXmlType::Edit(CAsset* pAsset) const
{
	using namespace Private_LegacyAssetTypes;

	if (!pAsset->GetFilesCount())
	{
		return nullptr;
	}

	const SXmlType* const pXmlType = GetXmlTypeFromXmlTag(pAsset->GetDetailValue(s_szRootTag).c_str());
	if (!pXmlType)
	{
		return nullptr;
	}

	const string libraryPath = pAsset->GetFile(0);
	const string libraryName = PathUtil::GetFileName(libraryPath);

	CBaseLibraryManager* const pLibraryManager = pXmlType->m_pLibraryManager;
	CRY_ASSERT(pLibraryManager);

	IDataBaseLibrary* pLibrary = pLibraryManager->FindLibrary(libraryName);
	if (!pLibrary)
	{
		GetIEditor()->GetIUndoManager()->Suspend();
		pLibrary = pLibraryManager->LoadLibrary(libraryPath.c_str());
		GetIEditor()->GetIUndoManager()->Resume();
		if (!pLibrary)
		{
			return nullptr;
		}
	}

	GetIEditor()->OpenView(DATABASE_VIEW_NAME);

	CDataBaseDialog* const pDataBaseDlg = (CDataBaseDialog*)GetIEditor()->FindView(DATABASE_VIEW_NAME);
	if (pDataBaseDlg == nullptr)
	{
		return nullptr;
	}

	pDataBaseDlg->SelectDialog(pXmlType->m_dataBaseItemType);

	CBaseLibraryDialog* const pBaseLibraryDialog = (CBaseLibraryDialog*)pDataBaseDlg->GetCurrent();
	if (pBaseLibraryDialog == nullptr)
	{
		return nullptr;
	}

	pBaseLibraryDialog->Reload();

	pBaseLibraryDialog->SelectLibrary(libraryName);

	return nullptr;
}


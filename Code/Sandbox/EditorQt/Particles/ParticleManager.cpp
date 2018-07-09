// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleManager.h"

#include "ParticleItem.h"
#include "ParticleLibrary.h"

#include "GameEngine.h"

#include "Controls/PropertyItem.h"
#include "DataBaseDialog.h"
#include "ParticleDialog.h"

#define PARTICLES_LIBS_PATH "Libs/Particles/"

//////////////////////////////////////////////////////////////////////////
// CParticleManager implementation.
//////////////////////////////////////////////////////////////////////////
CParticleManager::CParticleManager()
{
	m_bUniqNameMap = true;
	m_bUniqGuidMap = false;
	m_pLevelLibrary = (CBaseLibrary*)AddLibrary("Level");
	m_pLevelLibrary->SetLevelLibrary(true);

	//register deprecated property editors
	GetIEditorImpl()->RegisterDeprecatedPropertyEditor(ePropertyParticleName, WrapMemberFunction(this, &CParticleManager::OnPickParticle));
}

//////////////////////////////////////////////////////////////////////////
CParticleManager::~CParticleManager()
{
}

//////////////////////////////////////////////////////////////////////////
void CParticleManager::ClearAll()
{
	CBaseLibraryManager::ClearAll();

	m_pLevelLibrary = (CBaseLibrary*)AddLibrary("Level");
	m_pLevelLibrary->SetLevelLibrary(true);
}

//////////////////////////////////////////////////////////////////////////
CBaseLibraryItem* CParticleManager::MakeNewItem()
{
	return new CParticleItem;
}
//////////////////////////////////////////////////////////////////////////
CBaseLibrary* CParticleManager::MakeNewLibrary()
{
	return new CParticleLibrary(this);
}

bool CParticleManager::OnPickParticle(const string& oldValue, string& newValue)
{
	CString libraryName = "";
	CBaseLibraryItem* pItem = NULL;
	CString particleName(oldValue);
	newValue = oldValue;

	if (strcmp(particleName, "") != 0)
	{
		int pos = 0;
		libraryName = particleName.Tokenize(".", pos);
		CParticleManager* particleMgr = GetIEditorImpl()->GetParticleManager();
		IDataBaseLibrary* pLibrary = particleMgr->FindLibrary(libraryName);
		if (pLibrary == NULL)
		{
			CString fullLibraryName = libraryName + ".xml";
			fullLibraryName.MakeLower();
			fullLibraryName = CString("Libs/Particles/") + fullLibraryName;
			GetIEditorImpl()->GetIUndoManager()->Suspend();
			pLibrary = particleMgr->LoadLibrary(fullLibraryName);
			GetIEditorImpl()->GetIUndoManager()->Resume();
			if (pLibrary == NULL)
				return false;
		}

		particleName.Delete(0, pos);
		pItem = (CBaseLibraryItem*)pLibrary->FindItem(particleName.GetString());
		if (pItem == NULL)
		{
			CString leafParticleName(particleName);

			int lastDotPos = particleName.ReverseFind('.');
			while (!pItem && lastDotPos > -1)
			{
				particleName.Delete(lastDotPos, particleName.GetLength() - lastDotPos);
				lastDotPos = particleName.ReverseFind('.');
				pItem = (CBaseLibraryItem*)pLibrary->FindItem(particleName.GetString());
			}

			leafParticleName.Replace(particleName, "");
			if (leafParticleName.IsEmpty() || (leafParticleName.GetLength() == 1 && leafParticleName[0] == '.'))
				return false;
			if (leafParticleName[0] == '.')
				leafParticleName.Delete(0);
		}
	}

	GetIEditorImpl()->OpenView(DATABASE_VIEW_NAME);

	CDataBaseDialog* pDataBaseDlg = (CDataBaseDialog*)GetIEditorImpl()->FindView("DataBase View");
	if (pDataBaseDlg == NULL)
		return false;

	pDataBaseDlg->SelectDialog(EDB_TYPE_PARTICLE);

	CParticleDialog* particleDlg = (CParticleDialog*)pDataBaseDlg->GetCurrent();
	if (particleDlg == NULL)
		return false;

	particleDlg->Reload();

	if (pItem && strcmp(libraryName, "") != 0)
		particleDlg->SelectLibrary(libraryName.GetString());

	particleDlg->SelectItem(pItem);

	return true;
}

//////////////////////////////////////////////////////////////////////////
string CParticleManager::GetRootNodeName()
{
	return "ParticleLibs";
}
//////////////////////////////////////////////////////////////////////////
string CParticleManager::GetLibsPath()
{
	if (m_libsPath.IsEmpty())
		m_libsPath = PARTICLES_LIBS_PATH;
	return m_libsPath;
}

//////////////////////////////////////////////////////////////////////////
void CParticleManager::Export(XmlNodeRef& node)
{
	XmlNodeRef libs = node->newChild("ParticlesLibrary");
	for (int i = 0; i < GetLibraryCount(); i++)
	{
		IDataBaseLibrary* pLib = GetLibrary(i);
		if (pLib->IsLevelLibrary())
			continue;
		// Level libraries are saved in level.
		XmlNodeRef libNode = libs->newChild("Library");
		libNode->setAttr("Name", pLib->GetName());
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleManager::PasteToParticleItem(CParticleItem* pItem, XmlNodeRef& node, bool bWithChilds)
{
	assert(pItem);
	assert(node != NULL);

	CBaseLibraryItem::SerializeContext serCtx(node, true);
	serCtx.bCopyPaste = true;
	serCtx.bIgnoreChilds = !bWithChilds;
	pItem->Serialize(serCtx);
	pItem->GenerateIdRecursively();
}

//////////////////////////////////////////////////////////////////////////
void CParticleManager::DeleteItem(IDataBaseItem* pItem)
{
	CParticleItem* pPartItem = ((CParticleItem*)pItem);
	if (pPartItem->GetParent())
		pPartItem->SetParent(NULL);

	CBaseLibraryManager::DeleteItem(pItem);
}

#ifndef _LIB
#include <CryCore/Common_TypeInfo.h>
// Manually instantiate templates as needed here.
STRUCT_INFO_T_INSTANTIATE(Color_tpl, <float>)
#endif


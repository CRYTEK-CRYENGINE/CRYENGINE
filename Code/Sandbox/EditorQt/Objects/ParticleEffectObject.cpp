// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleEffectObject.h"
#include "Particles\ParticleDialog.h"
#include "DataBaseDialog.h"
#include "Particles\ParticleManager.h"
#include "Particles\ParticleItem.h"
#include "Objects/ObjectLoader.h"
#include "Objects/DisplayContext.h"
#include "Objects/InspectorWidgetCreator.h"
#include "GameEngine.h"
#include "Gizmos/AxisHelper.h"
#include "LevelEditor/LevelAssetType.h"

#include <Serialization/Decorators/EditorActionButton.h>

REGISTER_CLASS_DESC(CParticleEffectDesc);

IMPLEMENT_DYNCREATE(CParticleEffectObject, CEntityObject)

void CParticleEffectObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CEntityObject::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CParticleEffectObject>("Particle Effect", [](CParticleEffectObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		if (ar.openBlock("effect", "<Effect"))
		{
			ar(Serialization::ActionButton(std::bind(&CParticleEffectObject::OnMenuGoToDatabase, pObject)), "go_to_database", "^Edit");
			ar.closeBlock();
		}
	});
}

void CParticleEffectObject::Serialize(CObjectArchive& ar)
{
	CEntityObject::Serialize(ar);
	XmlNodeRef xmlNode = ar.node;
	if (ar.bLoading)
	{
		// Load
		string particleEntityName = m_ParticleEntityName;

		xmlNode->getAttr("ParticleEntityName", particleEntityName);

		// keep like this and don't set directly, might do some notification in future
		if (particleEntityName != m_ParticleEntityName)
		{
			m_ParticleEntityName = particleEntityName;
		}
	}
	else
	{
		xmlNode->setAttr("ParticleEntityName", m_ParticleEntityName);
	}
}

bool CParticleEffectObject::Init(CBaseObject* prev, const string& file)
{
	const bool init = CEntityObject::Init(prev, file);
	if (init)
	{
		CParticleEffectObject* prev_part = static_cast<CParticleEffectObject*>(prev);
		m_entityClass = "ParticleEffect";
		if (prev)
			m_ParticleEntityName = prev_part->m_ParticleEntityName;
		else
			m_ParticleEntityName = file;
	}
	return init;
}

void CParticleEffectObject::PostInit(const string& file)
{
	AssignEffect(m_ParticleEntityName);
}

// This could be moved to System or Common
bool DoDebuggerAdjustments()
{
	if (ICVar* pVar = gEnv->pConsole->GetCVar("sys_debugger_adjustments"))
	{
		if (pVar->GetIVal() && IsDebuggerPresent())
			return true;
	}
	return false;
}

void CParticleEffectObject::AssignEffect(const string& effectName)
{
	m_ParticleEntityName = effectName;

	CVarBlock* pProperties = GetProperties();
	if (pProperties)
	{
		if (DoDebuggerAdjustments())
		{
			// Workaround for hung debugger when a breakpoint is hit during mouse-captured drag:
			// set emitter inactive on creation.
			if (IVariable* pVar = pProperties->FindVariable("bActive"))
				pVar->Set(false);
		}
		IVariable* pVar = pProperties->FindVariable("ParticleEffect");
		if (pVar && pVar->GetType() == IVariable::STRING)
		{
			string lastAssetName;
			pVar->Get(lastAssetName);
			pVar->Set(effectName);

			if (lastAssetName != "" && lastAssetName != effectName)
			{
				CEntityScript* pScript = GetScript();
				if (pScript)
					pScript->SendEvent(GetIEntity(), "Restart");
			}
		}
	}

	IParticleEntityComponent* pParticleEntity = m_pEntity ? m_pEntity->GetComponent<IParticleEntityComponent>() : nullptr;
	if (pParticleEntity)
	{
		pParticleEntity->SetParticleEffectName(effectName);
	}
}

void CParticleEffectObject::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	const Matrix34& wtm = GetWorldTM();

	float fHelperScale = 1 * m_helperScale * gGizmoPreferences.helperScale;
	Vec3 dir = wtm.TransformVector(Vec3(0, fHelperScale, 0));
	Vec3 wp = wtm.GetTranslation();
	if (IsFrozen())
		dc.SetFreezeColor();
	else
		dc.SetColor(1, 1, 0);
	dc.DrawArrow(wp, wp + dir * 2, fHelperScale);

	DrawDefault(dc);
}

string CParticleEffectObject::GetParticleName()  const
{
	string effectName;
	if (GetProperties())
	{
		IVariable* pVar = GetProperties()->FindVariable("ParticleEffect");
		if (pVar && pVar->GetType() == IVariable::STRING)
			pVar->Get(effectName);
	}

	if (effectName.IsEmpty())
		return m_ParticleEntityName;

	return effectName;
}

string CParticleEffectObject::GetComment() const
{
	string comment;
	if (GetProperties())
	{
		IVariable* pVar = GetProperties()->FindVariable("Comment");
		if (pVar && pVar->GetType() == IVariable::STRING)
			pVar->Get(comment);
	}
	return comment;
}

bool CParticleEffectObject::IsGroup(const string& effectName)
{
	//! In order to filter names about particle group in upper level to avoid unnecessary loading.
	int dotCounter = 0;
	for (int i = 0; i < effectName.GetLength(); ++i)
	{
		if (effectName[i] == '.')
			++dotCounter;
	}
	return dotCounter <= 1;
}

void CParticleEffectObject::OnMenuGoToDatabase() const
{
	string particleName(GetParticleName());

	if (particleName.Right(4).MakeLower() == ".pfx")
	{
		// pfx2
		GetIEditorImpl()->ExecuteCommand("particle.show_effect %s", particleName.GetString());
	}
	else
	{
		// pfx
		int pos = 0;
		string libraryName = particleName.Tokenize(".", pos);
		CParticleManager* particleMgr = GetIEditorImpl()->GetParticleManager();
		IDataBaseLibrary* pLibrary = particleMgr->FindLibrary(libraryName);
		if (pLibrary == NULL)
		{
			string fullLibraryName = libraryName + ".xml";
			fullLibraryName.MakeLower();
			fullLibraryName = string("Libs/Particles/") + fullLibraryName;
			GetIEditorImpl()->GetIUndoManager()->Suspend();
			pLibrary = particleMgr->LoadLibrary(fullLibraryName);
			GetIEditorImpl()->GetIUndoManager()->Resume();
			if (pLibrary == NULL)
				return;
		}

		particleName.Delete(0, pos);
		CBaseLibraryItem* pItem = (CBaseLibraryItem*)pLibrary->FindItem(particleName);
		if (pItem == NULL)
		{
			string leafParticleName(particleName);

			int lastDotPos = particleName.ReverseFind('.');
			while (!pItem && lastDotPos > -1)
			{
				particleName.Delete(lastDotPos, particleName.GetLength() - lastDotPos);
				lastDotPos = particleName.ReverseFind('.');
				pItem = (CBaseLibraryItem*)pLibrary->FindItem(particleName);
			}

			leafParticleName.Replace(particleName, "");
			if (leafParticleName.IsEmpty() || (leafParticleName.GetLength() == 1 && leafParticleName[0] == '.'))
				return;
			if (leafParticleName[0] == '.')
				leafParticleName.Delete(0);

			pItem = GetChildParticleItem((CParticleItem*)pItem, "", leafParticleName);
			if (pItem == NULL)
				return;
		}

		GetIEditorImpl()->OpenView(DATABASE_VIEW_NAME);

		CDataBaseDialog* pDataBaseDlg = (CDataBaseDialog*)GetIEditorImpl()->FindView("DataBase View");
		if (pDataBaseDlg == NULL)
			return;
		pDataBaseDlg->SelectDialog(EDB_TYPE_PARTICLE);

		CParticleDialog* particleDlg = (CParticleDialog*)pDataBaseDlg->GetCurrent();
		if (particleDlg == NULL)
			return;

		particleDlg->Reload();
		particleDlg->SelectLibrary(libraryName);
		particleDlg->SelectItem(pItem);
	}
}

CParticleItem* CParticleEffectObject::GetChildParticleItem(CParticleItem* pParentItem, string composedName, const string& wishName) const
{
	if (!pParentItem || (pParentItem && pParentItem->GetChildCount() <= 0))
		return NULL;

	for (int i = 0; i < pParentItem->GetChildCount(); ++i)
	{
		CParticleItem* pChildParticle = ((CParticleItem*)pParentItem)->GetChild(i);
		if (pChildParticle == NULL)
			continue;
		if (wishName == (composedName + pChildParticle->GetName()))
			return pChildParticle;
		CParticleItem* pWishItem = GetChildParticleItem(pChildParticle, composedName + pChildParticle->GetName() + ".", wishName);
		if (pWishItem)
			return pWishItem;
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CParticleEffectObject::CFastParticleParser::ExtractParticlePathes(const string& particlePath)
{
	m_ParticleList.clear();

	XmlNodeRef rootNode = XmlHelpers::LoadXmlFromFile(particlePath);

	if (rootNode == NULL)
		return;

	string rootName;
	rootNode->getAttr("Name", rootName);

	for (int i = 0; i < rootNode->getChildCount(); ++i)
	{
		XmlNodeRef itemNode = rootNode->getChild(i);
		if (itemNode)
		{
			if (itemNode->getTag() && !stricmp(itemNode->getTag(), "Particles"))
				ParseParticle(itemNode, rootName);
		}
	}
}

void CParticleEffectObject::CFastParticleParser::ExtractLevelParticles()
{
	m_ParticleList.clear();

	if (CParticleManager* pParticleManager = GetIEditorImpl()->GetParticleManager())
	{
		if (IDataBaseLibrary* pLevelLibrary = pParticleManager->FindLibrary("Level"))
		{
			if (CGameEngine* pGameEngine = GetIEditorImpl()->GetGameEngine())
			{
				const string& levelPath = pGameEngine->GetLevelPath();
				const string& levelName = pGameEngine->GetLevelName();
				string fullPath = PathUtil::Make(levelPath, levelName, CLevelType::GetFileExtensionStatic());
				ICryPak* pIPak = GetIEditorImpl()->GetSystem()->GetIPak();

				if (pIPak && pIPak->IsFileExist(fullPath))
				{
					if (XmlNodeRef rootNode = XmlHelpers::LoadXmlFromFile(fullPath))
					{
						if (XmlNodeRef particleLibrary = rootNode->findChild(pParticleManager->GetRootNodeName()))
						{
							if (XmlNodeRef levelLibrary = particleLibrary->findChild("LevelLibrary"))
							{
								for (int i = 0; i < levelLibrary->getChildCount(); ++i)
								{
									if (XmlNodeRef itemNode = levelLibrary->getChild(i))
									{
										if (itemNode->getTag() && !stricmp(itemNode->getTag(), "Particles"))
											ParseParticle(itemNode, "Level");
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleEffectObject::CFastParticleParser::ParseParticle(XmlNodeRef& node, const string& parentPath)
{
	string particleName;
	node->getAttr("Name", particleName);
	string particlePath = parentPath + string(".") + particleName;

	SParticleInfo si;
	si.pathName = particlePath;
	for (int i = 0; i < node->getChildCount(); ++i)
	{
		XmlNodeRef childItemNode = node->getChild(i);

		if (childItemNode == NULL)
			continue;

		if (childItemNode->getTag() && !stricmp(childItemNode->getTag(), "Childs"))
		{
			si.bHaveChildren = true;
			break;
		}
	}
	m_ParticleList.push_back(si);

	for (int i = 0; i < node->getChildCount(); ++i)
	{
		XmlNodeRef childItemNode = node->getChild(i);

		if (childItemNode == NULL)
			continue;

		if (childItemNode->getTag() && !stricmp(childItemNode->getTag(), "Childs"))
		{
			for (int k = 0; k < childItemNode->getChildCount(); ++k)
			{
				XmlNodeRef child2ItemNode = childItemNode->getChild(k);

				if (child2ItemNode == NULL)
					continue;

				if (child2ItemNode->getTag() && !stricmp(child2ItemNode->getTag(), "Particles"))
					ParseParticle(child2ItemNode, particlePath);
			}
		}
	}
}


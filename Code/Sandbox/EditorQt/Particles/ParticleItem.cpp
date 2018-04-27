// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleItem.h"

#include "ParticleLibrary.h"
#include "BaseLibraryManager.h"

#include <CryParticleSystem/ParticleParams.h>

//////////////////////////////////////////////////////////////////////////
CParticleItem::CParticleItem()
{
	m_pParentParticles = 0;
	m_pEffect = gEnv->pParticleManager->CreateEffect();
	m_pEffect->AddRef();
	m_bDebugEnabled = true;
	m_bSaveEnabled = false;
}

//////////////////////////////////////////////////////////////////////////
CParticleItem::CParticleItem(IParticleEffect* pEffect)
{
	assert(pEffect);
	m_pParentParticles = 0;
	m_pEffect = pEffect;

	{
		// Set library item name to full effect name, without library prefix.
		string sEffectName = pEffect->GetFullName();
		const char* sItemName = sEffectName.c_str();
		if (const char* sFind = strchr(sItemName, '.'))
			sItemName = sFind + 1;
		m_name = sItemName;
	}
	m_bDebugEnabled = true;
	m_bSaveEnabled = false;
}

//////////////////////////////////////////////////////////////////////////
CParticleItem::~CParticleItem()
{
	gEnv->pParticleManager->DeleteEffect(m_pEffect);
}

//////////////////////////////////////////////////////////////////////////
void CParticleItem::SetName(const string& name)
{
	if (m_pEffect)
	{
		if (!m_pEffect->GetParent())
		{
			string fullname = GetLibrary()->GetName() + "." + name;
			m_pEffect->SetName(fullname);
		}
		else
			m_pEffect->SetName(name);
	}
	CBaseLibraryItem::SetName(name);
}

void CParticleItem::AddAllChildren()
{
	// Add all children recursively.
	for (int i = 0; i < m_pEffect->GetChildCount(); i++)
	{
		CParticleItem* pItem = new CParticleItem(m_pEffect->GetChild(i));
		if (GetLibrary())
			GetLibrary()->AddItem(pItem, true);
		pItem->m_pParentParticles = this;
		m_childs.push_back(pItem);
		pItem->AddAllChildren();
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleItem::Serialize(SerializeContext& ctx)
{
	XmlNodeRef node = ctx.node;
	if (ctx.bLoading)
	{
		if (!ctx.bIgnoreChilds)
			ClearChilds();

		m_pEffect->Serialize(node, true, !ctx.bIgnoreChilds);

		AddAllChildren();
	}
	else
	{
		m_pEffect->Serialize(node, false, true);
	}

	m_bModified = false;
}

//////////////////////////////////////////////////////////////////////////
int CParticleItem::GetChildCount() const
{
	return m_childs.size();
}

//////////////////////////////////////////////////////////////////////////
CParticleItem* CParticleItem::GetChild(int index) const
{
	assert(index >= 0 && index < m_childs.size());
	return m_childs[index];
}

//////////////////////////////////////////////////////////////////////////
void CParticleItem::SetParent(CParticleItem* pParent)
{
	if (pParent == m_pParentParticles)
		return;

	TSmartPtr<CParticleItem> refholder = this;
	if (m_pParentParticles)
		stl::find_and_erase(m_pParentParticles->m_childs, this);

	string sNewName = GetShortName();
	m_pParentParticles = pParent;
	if (pParent)
	{
		pParent->m_childs.push_back(this);
		m_library = pParent->m_library;
		if (m_pEffect)
			m_pEffect->SetParent(pParent->m_pEffect);
		sNewName = pParent->GetName() + "." + sNewName;
	}
	else
		m_pEffect->SetParent(NULL);

	// Change name to be within group.
	sNewName = m_library->GetManager()->MakeUniqItemName(sNewName);
	SetName(sNewName);
}

//////////////////////////////////////////////////////////////////////////
void CParticleItem::ClearChilds()
{
	// Also delete them from the library.
	for (int i = 0; i < m_childs.size(); i++)
	{
		m_childs[i]->m_pParentParticles = NULL;
		if (GetLibrary())
			GetLibrary()->RemoveItem(m_childs[i]);
	}
	m_childs.clear();

	if (m_pEffect)
		m_pEffect->ClearChilds();
}

/*
   //////////////////////////////////////////////////////////////////////////
   void CParticleItem::InsertChild( int slot,CParticleItem *pItem )
   {
   if (slot < 0)
    slot = 0;
   if (slot > m_childs.size())
    slot = m_childs.size();

   assert( pItem );
   pItem->m_pParentParticles = this;
   pItem->m_library = m_library;

   m_childs.insert( m_childs.begin() + slot,pItem );
   m_pMatInfo->RemoveAllSubMtls();
   for (int i = 0; i < m_childs.size(); i++)
   {
    m_pMatInfo->AddSubMtl( m_childs[i]->m_pMatInfo );
   }
   }
 */

//////////////////////////////////////////////////////////////////////////
int CParticleItem::FindChild(CParticleItem* pItem)
{
	for (int i = 0; i < m_childs.size(); i++)
	{
		if (m_childs[i] == pItem)
		{
			return i;
		}
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////
CParticleItem* CParticleItem::GetParent() const
{
	return m_pParentParticles;
}

//////////////////////////////////////////////////////////////////////////
IParticleEffect* CParticleItem::GetEffect() const
{
	return m_pEffect;
}

/*
    E/S		0			1
    0/0		0/0		0/0
    0/1		0/1		1/1
    1/0		0/1		1/1
    1/1		0/1		1/1
 */
void CParticleItem::DebugEnable(int iEnable)
{
	if (m_pEffect)
	{
		if (iEnable < 0)
			iEnable = !m_bDebugEnabled;
		m_bDebugEnabled = iEnable != 0;
		m_bSaveEnabled = m_bSaveEnabled || m_pEffect->IsEnabled();
		m_pEffect->SetEnabled(m_bSaveEnabled && m_bDebugEnabled);
		for (int i = 0; i < m_childs.size(); i++)
			m_childs[i]->DebugEnable(iEnable);
	}
}

int CParticleItem::GetEnabledState() const
{
	int nEnabled = m_pEffect->IsEnabled();
	for (int i = 0; i < m_childs.size(); i++)
		if (m_childs[i]->GetEnabledState())
			nEnabled |= 2;
	return nEnabled;
}

//////////////////////////////////////////////////////////////////////////
void CParticleItem::GenerateIdRecursively()
{
	GenerateId();
	for (int i = 0; i < m_childs.size(); i++)
	{
		m_childs[i]->GenerateIdRecursively();
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleItem::Update()
{
	// Mark library as modified.
	if (NULL != GetLibrary())
	{
		GetLibrary()->SetModified();
	}
}

//////////////////////////////////////////////////////////////////////////
void CParticleItem::GatherUsedResources(CUsedResources& resources)
{
	if (m_pEffect->GetParticleParams().sTexture.length())
		resources.Add(m_pEffect->GetParticleParams().sTexture.c_str());
	if (m_pEffect->GetParticleParams().sMaterial.length())
		resources.Add(m_pEffect->GetParticleParams().sMaterial.c_str());
	if (m_pEffect->GetParticleParams().sGeometry.length())
		resources.Add(m_pEffect->GetParticleParams().sGeometry.c_str());
	if (m_pEffect->GetParticleParams().sStartTrigger.length())
		resources.Add(m_pEffect->GetParticleParams().sStartTrigger.c_str());
	if (m_pEffect->GetParticleParams().sStopTrigger.length())
		resources.Add(m_pEffect->GetParticleParams().sStopTrigger.c_str());
}



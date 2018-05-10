// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SceneElementCommon.h"
#include "Scene.h"

CSceneElementCommon::CSceneElementCommon(CScene* pScene, int id)
	: m_name()
	, m_pParent(nullptr)
	, m_siblingIndex(0)
	, m_pScene(pScene)
	, m_id(id)
{
	CRY_ASSERT(pScene);
}

int CSceneElementCommon::GetId() const
{
	return m_id;
}

CSceneElementCommon* CSceneElementCommon::GetParent() const
{
	return m_pParent;
}

CSceneElementCommon* CSceneElementCommon::GetChild(int index) const
{
	return index >= 0 && index < m_children.size() ? m_children[index] : nullptr;
}

int CSceneElementCommon::GetNumChildren() const
{
	return (int)m_children.size();
}

bool CSceneElementCommon::IsLeaf() const
{
	return m_children.empty();
}

string CSceneElementCommon::GetName() const
{
	return m_name;
}

int CSceneElementCommon::GetSiblingIndex() const
{
	return m_siblingIndex;
}

CScene* CSceneElementCommon::GetScene()
{
	return m_pScene;
}

void CSceneElementCommon::SetSiblingIndex(int index)
{
	m_siblingIndex = index;
}

void CSceneElementCommon::SetName(const char* szName)
{
	assert(szName && *szName);
	m_name = szName;
}

void CSceneElementCommon::SetName(const string& name)
{
	m_name = name;
}

void CSceneElementCommon::AddChild(CSceneElementCommon* pChild)
{
	assert(pChild);
	assert(!pChild->m_pParent && pChild->m_siblingIndex == 0);

	pChild->m_pParent = this;
	pChild->m_siblingIndex = (int)m_children.size();

	m_children.push_back(pChild);

	m_pScene->signalHierarchyChanged(nullptr, pChild);
}

CSceneElementCommon* CSceneElementCommon::RemoveChild(int index)
{
	if (index < 0 || index >= m_children.size())
	{
		return nullptr;
	}
	CSceneElementCommon* const pOrphan = m_children[index];
	pOrphan->m_pParent = nullptr;
	pOrphan->m_siblingIndex = -1;
	m_children.erase(m_children.begin() + index);

	for (int i = 0; i < m_children.size(); ++i)
	{
		m_children[i]->SetSiblingIndex(i);
	}

	m_pScene->signalHierarchyChanged(this, pOrphan);

	return pOrphan;
}

void CSceneElementCommon::MakeRoot(CSceneElementCommon* pSceneElement)
{
	if (!pSceneElement->GetParent())
	{
		return;
	}
	pSceneElement->GetParent()->RemoveChild(pSceneElement->GetSiblingIndex());
}


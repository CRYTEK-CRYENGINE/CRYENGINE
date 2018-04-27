// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "OpticsReference.h"

#if defined(FLARES_SUPPORT_EDITING)
DynArray<FuncVariableGroup> COpticsReference::GetEditorParamGroups()
{
	return DynArray<FuncVariableGroup>();
}
#endif

COpticsReference::COpticsReference(const char* name) : m_name(name)
{
}

void COpticsReference::Load(IXmlNode* pNode)
{
}

void COpticsReference::AddElement(IOpticsElementBase* pElement)
{
	m_OpticsList.push_back(pElement);
}

void COpticsReference::InsertElement(int nPos, IOpticsElementBase* pElement)
{
	if (nPos < 0 || nPos >= (int)m_OpticsList.size())
		return;
	m_OpticsList.insert(m_OpticsList.begin() + nPos, pElement);
}

void COpticsReference::Remove(int i)
{
	if (i < 0 || i >= (int)m_OpticsList.size())
		return;
	m_OpticsList.erase(m_OpticsList.begin() + i);
}

void COpticsReference::RemoveAll()
{
	m_OpticsList.clear();
}

int COpticsReference::GetElementCount() const
{
	return m_OpticsList.size();
}

IOpticsElementBase* COpticsReference::GetElementAt(int i) const
{
	if (i < 0 || i >= (int)m_OpticsList.size())
		return NULL;
	return m_OpticsList[i];
}

void COpticsReference::GetMemoryUsage(ICrySizer* pSizer) const
{
	for (int i = 0, iSize(m_OpticsList.size()); i < iSize; ++i)
		m_OpticsList[i]->GetMemoryUsage(pSizer);
}

void COpticsReference::Invalidate()
{
	for (int i = 0, iSize(m_OpticsList.size()); i < iSize; ++i)
		m_OpticsList[i]->Invalidate();
}

void COpticsReference::RenderPreview(const SLensFlareRenderParam* pParam, const Vec3& vPos)
{
	for (int i = 0, iSize(m_OpticsList.size()); i < iSize; ++i)
		m_OpticsList[i]->RenderPreview(pParam, vPos);
}

void COpticsReference::DeleteThis()
{
	delete this;
}

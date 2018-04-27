// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "OpticsProxy.h"

#if defined(FLARES_SUPPORT_EDITING)
DynArray<FuncVariableGroup> COpticsProxy::GetEditorParamGroups()
{
	return DynArray<FuncVariableGroup>();
}
#endif

void COpticsProxy::DeleteThis()
{
	delete this;
}

COpticsProxy::COpticsProxy(const char* name) :
	m_bEnable(false),
	m_name(name),
	m_pOpticsReference(NULL)
{
}

void COpticsProxy::Load(IXmlNode* pNode)
{
	XmlNodeRef pProxy = pNode->findChild("Proxy");
	if (pProxy)
	{
		const char* referenceName(NULL);
		if (pProxy->getAttr("Reference", &referenceName))
		{
			if (referenceName && referenceName[0])
			{
				int nReferenceIndex(-1);
				if (gEnv->pOpticsManager->Load(referenceName, nReferenceIndex))
				{
					IOpticsElementBase* pOptics = gEnv->pOpticsManager->GetOptics(nReferenceIndex);
					if (pOptics->GetType() == eFT_Reference)
						m_pOpticsReference = (COpticsReference*)gEnv->pOpticsManager->GetOptics(nReferenceIndex);
				}
			}
		}
	}
}

void COpticsProxy::GetMemoryUsage(ICrySizer* pSizer) const
{
}

void COpticsProxy::Invalidate()
{
}

void COpticsProxy::RenderPreview(const SLensFlareRenderParam* pParam, const Vec3& vPos)
{
	if (m_pOpticsReference)
		m_pOpticsReference->RenderPreview(pParam, vPos);
}

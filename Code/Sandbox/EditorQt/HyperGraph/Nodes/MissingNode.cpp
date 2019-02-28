// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MissingNode.h"

#include "Objects/EntityObject.h"
#include "Objects/ObjectLoader.h"

CMissingNode::CMissingNode(const char* sMissingClassName)
	: m_sMissingClassName(sMissingClassName)
{
	ZeroStruct(m_entityGuid);
	if (!gEnv->IsEditor()) assert(false);
}

CHyperNode* CMissingNode::Clone()
{
	CMissingNode* pNode = new CMissingNode(m_sMissingClassName);
	pNode->CopyFrom(*this);

	pNode->m_sMissingName = m_sMissingName;
	pNode->m_sGraphEntity = m_sGraphEntity;
	pNode->m_entityGuid = m_entityGuid;
	pNode->m_iOrgFlags = m_iOrgFlags;

	return pNode;
}

CHyperNodePort* CMissingNode::FindPort(const char* portname, bool bInput)
{
	CHyperNodePort* res = CHyperNode::FindPort(portname, bInput);

	if (!res)
		res = CreateMissingPort(portname, bInput);

	return res;
}

void CMissingNode::Serialize(XmlNodeRef& node, bool bLoading, CObjectArchive* ar)
{
	if (bLoading)
	{
		node->getAttr("Name", m_sMissingName);
		node->getAttr("GraphEntity", m_sGraphEntity);
		node->getAttr("flags", m_iOrgFlags);

		if (node->getAttr("EntityGUID", m_entityGuid) && ar)
			m_entityGuid = ar->ResolveID(m_entityGuid);

		XmlNodeRef portsNode = node->findChild("Inputs");
		if (portsNode)
		{
			for (int i = 0; i < portsNode->getNumAttributes(); ++i)
			{
				cstr sKey, sVal;
				portsNode->getAttributeByIndex(i, &sKey, &sVal);
				CreateMissingPort(sKey, true, true);
			}
		}
	}
	else
	{
		SetName(m_sMissingName);
	}

	CHyperNode::Serialize(node, bLoading, ar);

	if (!m_sGraphEntity.IsEmpty())
		node->setAttr("GraphEntity", m_sGraphEntity);

	if (m_entityGuid != CryGUID::Null())
	{
		node->setAttr("EntityGUID", m_entityGuid);
	}
	node->setAttr("flags", m_iOrgFlags);

	SetName("MISSING");
}

const char* CMissingNode::GetInfoAsString() const
{
	string sNodeInfo;
	sNodeInfo.Format("!!Missing Node!!\nClass:  %s", m_sMissingClassName);
	return sNodeInfo.c_str();
}


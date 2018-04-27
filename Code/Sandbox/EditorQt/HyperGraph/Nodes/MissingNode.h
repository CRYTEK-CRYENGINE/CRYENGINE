// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "HyperGraph/HyperGraphNode.h"

#define MISSING_NODE_CLASS ("MissingNode")

class CMissingNode : public CHyperNode
{
public:
	CMissingNode(const char* sMissingClassName);

	// CHyperNode
	virtual void            Init() override {}
	virtual CHyperNode*     Clone() override;
	virtual void            Serialize(XmlNodeRef& node, bool bLoading, CObjectArchive* ar = 0) override;

	virtual bool            IsEditorSpecialNode() const override { return true; }
	virtual bool            IsFlowNode() const override          { return false; }
	virtual const char*     GetClassName() const override        { return MISSING_NODE_CLASS; };
	virtual const char*     GetDescription() const override      { return "This node class no longer exists in the code base"; }
	virtual const char*     GetInfoAsString() const override;

	virtual CHyperNodePort* FindPort(const char* portname, bool bInput);
	// ~CHyperNode

private:
	CString m_sMissingClassName;
	CString m_sMissingName;
	CString m_sGraphEntity;
	CryGUID m_entityGuid;
	int     m_iOrgFlags;
};


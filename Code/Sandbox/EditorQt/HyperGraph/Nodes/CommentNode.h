// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "HyperGraph/HyperGraphNode.h"

class CCommentNode : public CHyperNode
{
public:
	static const char* GetClassType()
	{
		return "_comment";
	}
	CCommentNode();

	// CHyperNode
	void                Init() override                      {}
	CHyperNode*         Clone() override;

	virtual bool        IsEditorSpecialNode() const override { return true; }
	virtual bool        IsFlowNode() const override          { return false; }
	virtual const char* GetDescription() const override      { return "Simple Comment."; }
	virtual const char* GetInfoAsString() const override     { return "Class: Comment"; }
	// ~CHyperNode
};


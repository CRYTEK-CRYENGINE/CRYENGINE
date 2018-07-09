// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "HyperGraph/HyperGraphNode.h"

class CQuickSearchNode : public CHyperNode
{
public:
	static const char* GetClassType()
	{
		return "QuickSearch";
	}
	CQuickSearchNode();

	// CHyperNode
	void         Init() override                      {}
	CHyperNode*  Clone() override;

	virtual bool IsEditorSpecialNode() const override { return true; }
	virtual bool IsFlowNode() const override          { return false; }
	// ~CHyperNode

	void         SetSearchResultCount(int count)      { m_iSearchResultCount = count; };
	int          GetSearchResultCount()               { return m_iSearchResultCount; };

	void         SetIndex(int index)                  { m_iIndex = index; };
	int          GetIndex()                           { return m_iIndex; };

private:
	int m_iSearchResultCount;
	int m_iIndex;
};


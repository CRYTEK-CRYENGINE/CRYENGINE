// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlashUIArrayNodes.h
//  Version:     v1.00
//  Created:     21/4/2011 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include <CryFlowGraph/IFlowBaseNode.h>

class CFlashUIToArrayNode : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlashUIToArrayNode(SActivationInfo* pActInfo) {}
	virtual ~CFlashUIToArrayNode() {}

	virtual void GetConfiguration(SFlowNodeConfig& config);
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual void GetMemoryUsage(ICrySizer* s) const { s->Add(*this); }

private:
	enum EInputs
	{
		eI_Set = 0,
		eI_Count,
		eI_Val1,
		eI_Val2,
		eI_Val3,
		eI_Val4,
	};

	enum EOutputs
	{
		eO_OnSet = 0,
		eO_ArgList,
	};
};

class CFlashUIFromArrayNode : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlashUIFromArrayNode(SActivationInfo* pActInfo) {}
	virtual ~CFlashUIFromArrayNode() {}

	virtual void GetConfiguration(SFlowNodeConfig& config);
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual void GetMemoryUsage(ICrySizer* s) const { s->Add(*this); }

private:
	enum EInputs
	{
		eI_Array = 0,
	};

	enum EOutputs
	{
		eO_Count = 0,
		eO_Val1,
		eO_Val2,
		eO_Val3,
		eO_Val4,
		eO_LeftOver,
	};
};

class CFlashUIFromArrayExNode : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlashUIFromArrayExNode(SActivationInfo* pActInfo) {}
	virtual ~CFlashUIFromArrayExNode() {}

	virtual void GetConfiguration(SFlowNodeConfig& config);
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual void GetMemoryUsage(ICrySizer* s) const { s->Add(*this); }

private:
	enum EInputs
	{
		eI_Get,
		eI_Array,
		eI_Index,
	};

	enum EOutputs
	{
		eO_Val = 0,
	};
};

class CFlashUIArrayConcatNode : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlashUIArrayConcatNode(SActivationInfo* pActInfo) {}
	virtual ~CFlashUIArrayConcatNode() {}

	virtual void GetConfiguration(SFlowNodeConfig& config);
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual void GetMemoryUsage(ICrySizer* s) const { s->Add(*this); }

private:
	enum EInputs
	{
		eI_Set = 0,
		eI_Arr1,
		eI_Arr2,
	};

	enum EOutputs
	{
		eO_OnSet = 0,
		eO_ArgList,
	};
};

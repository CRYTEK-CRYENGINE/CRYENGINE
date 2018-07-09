// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FLOWGRAPH_MIGRATION_HELPER_H__
#define __FLOWGRAPH_MIGRATION_HELPER_H__

#pragma once

#include "IHyperGraph.h"

#include <map>

class CFlowGraphMigrationHelper
{
public:
	struct NodeEntry;
	// Condition interface
	struct ICondition : public _i_reference_target_t
	{
		// is the Condition fulfilled?
		virtual bool Fulfilled(XmlNodeRef node, NodeEntry& entry) = 0;
	};
	typedef _smart_ptr<ICondition>     IConditionPtr;
	typedef std::vector<IConditionPtr> ConditionVector;

	struct PortEntry
	{
		PortEntry() : transformFunc(0), bRemapValue(false), bIsOutput(false) {}
		CString         newName;
		HSCRIPTFUNCTION transformFunc;
		bool            bRemapValue;
		bool            bIsOutput;
	};
	typedef std::map<CString, PortEntry, stl::less_stricmp<CString>> PortMappings;
	typedef std::map<CString, CString, stl::less_stricmp<CString>>   InputValues;

	struct NodeEntry
	{
		CString         newName;
		ConditionVector conditions;
		PortMappings    inputPortMappings;
		PortMappings    outputPortMappings;
		InputValues     inputValues;
	};
	typedef std::map<CString, NodeEntry, stl::less_stricmp<CString>> NodeMappings;

	struct ReportEntry
	{
		HyperNodeID nodeId;
		CString     description;
	};

public:
	CFlowGraphMigrationHelper();
	~CFlowGraphMigrationHelper();

	// Substitute (and alter) the node
	// node must be a Graph XML node
	// returns true if something changed, false otherwise
	bool Substitute(XmlNodeRef node);

	// Get the substitution report
	const std::vector<ReportEntry>& GetReport() const;

protected:
	bool LoadSubstitutions();
	void AddEntry(XmlNodeRef node);
	bool EvalConditions(XmlNodeRef node, NodeEntry& entry);
	void ReleaseLUAFuncs();

protected:
	bool                         m_bLoaded;
	bool                         m_bInitialized;
	NodeMappings                 m_nodeMappings;
	std::vector<ReportEntry>     m_report;
	std::vector<HSCRIPTFUNCTION> m_transformFuncs;
};

#endif // __FLOWGRAPH_MIGRATION_HELPER_H__


// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GoalPipeXMLReader_H__
#define __GoalPipeXMLReader_H__

#include <CryAISystem/IGoalPipe.h>

class CPipeManager;

class CGoalPipeXMLReader
{
public:
	CGoalPipeXMLReader();

	bool LoadGoalPipesFromXmlFile(const char* filename);
	bool LoadGoalPipesFromXmlNode(const XmlNodeRef& root);

	void ParseGoalPipe(const char* szGoalPipeName, const XmlNodeRef goalPipeNode, CPipeManager::ActionToTakeWhenDuplicateFound actionToTakeWhenDuplicateFound = CPipeManager::ReplaceDuplicateAndReportError);

private:
	void   ParseGoalOps(IGoalPipe* pGoalPipe, const XmlNodeRef& root,
	                    string sIfLabel = string(), bool b_IfElseEnd_Halves_ReverseOrder = true);
	void   ParseGoalOp(IGoalPipe* pGoalPipe, const XmlNodeRef& goalOpNode);

	string GenerateUniqueLabelName();

private:
	class CXMLAttrReader
	{
		typedef std::pair<const char*, int> TRecord;
		std::vector<TRecord> m_dictionary;

	public:
		void Add(const char* szKey, int nValue) { m_dictionary.push_back(TRecord(szKey, nValue)); }
		bool Get(const char* szKey, int& nValue);
	};

	CXMLAttrReader        m_dictBranchType;

	IGoalPipe::EGroupType m_currentGrouping;

	uint32                m_nextUniqueLabelID;
};

#endif  // #ifndef __GoalPipeXMLReader_H__

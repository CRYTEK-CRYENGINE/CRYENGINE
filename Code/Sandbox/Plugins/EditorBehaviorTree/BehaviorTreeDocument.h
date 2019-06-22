// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/BehaviorTree/IBehaviorTree.h>

class BehaviorTreeDocument
{
public:
	BehaviorTreeDocument();

	void Reset();
	bool Loaded();

	bool Changed();
	void SetChanged();

	void Serialize(Serialization::IArchive& archive);

	void NewFile(const char* behaviorTreeName, const char* absoluteFilePath);
	bool OpenFile(const char* behaviorTreeName, const char* absoluteFilePath, const BehaviorTree::NodeSerializationHints::EventsFlags& eventsFlags);
	bool Save();
	bool SaveToFile(const char* behaviorTreeName, const char* absoluteFilePath);

private:
	XmlNodeRef GenerateBehaviorTreeXml();

	bool                                  m_loaded;
	bool                                  m_changed;
	string                                m_behaviorTreeName;
	string                                m_absoluteFilePath;
	BehaviorTree::BehaviorTreeTemplatePtr m_behaviorTreeTemplate;
};


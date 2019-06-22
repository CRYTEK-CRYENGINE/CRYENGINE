// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BehaviorTreeDocument.h"

#include <CryAISystem/BehaviorTree/XmlLoader.h>
#include <CryAISystem/BehaviorTree/SerializationSupport.h>

BehaviorTreeDocument::BehaviorTreeDocument()
	: m_changed(false)
	, m_loaded(false)
{
	m_behaviorTreeTemplate = std::make_shared<BehaviorTree::BehaviorTreeTemplate>();
}

void BehaviorTreeDocument::Reset()
{
	m_loaded = false;
	m_changed = false;
	m_behaviorTreeName.clear();
	m_absoluteFilePath.clear();
	*m_behaviorTreeTemplate = BehaviorTree::BehaviorTreeTemplate();
}

bool BehaviorTreeDocument::Loaded()
{
	return m_loaded;
}

bool BehaviorTreeDocument::Changed()
{
	return m_changed;
}

void BehaviorTreeDocument::SetChanged()
{
	m_changed = true;
}

void BehaviorTreeDocument::Serialize(Serialization::IArchive& archive)
{
	if (!m_behaviorTreeTemplate)
		return;

	BehaviorTree::BehaviorTreeTemplate* pBehaviorTreeTemplate = m_behaviorTreeTemplate.get();
	Serialization::SContext behaviorTreeTemplateContext(archive, pBehaviorTreeTemplate);

	archive(m_behaviorTreeName, "name", "!Tree Name");

	m_behaviorTreeTemplate->metaExtensionTable.Serialize(archive);

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION

#ifdef USING_VARIABLE_COLLECTION_SERIALIZATION
	archive(m_behaviorTreeTemplate->variableDeclarations, "variableDeclarations", "Variables");
	Serialization::SContext variableDeclarations(archive, &m_behaviorTreeTemplate->variableDeclarations);

	archive(m_behaviorTreeTemplate->eventsDeclaration, "events", "Events");
	Serialization::SContext builtInEventsDeclaration(archive, &m_behaviorTreeTemplate->eventsDeclaration);

	archive(m_behaviorTreeTemplate->signalHandler, "signalHandler", "Event handles");
#endif

	archive(m_behaviorTreeTemplate->defaultTimestampCollection, "TimestampCollection", "Timestamps");
	Serialization::SContext timestampsContext(archive, &m_behaviorTreeTemplate->defaultTimestampCollection);

	archive(m_behaviorTreeTemplate->rootNode, "root", "=<>" NODE_COMBOBOX_FIXED_WIDTH ">+Tree root");
	if (!m_behaviorTreeTemplate->rootNode.get())
		archive.error(m_behaviorTreeTemplate->rootNode, "Node must be specified");
#endif
}

void BehaviorTreeDocument::NewFile(const char* behaviorTreeName, const char* absoluteFilePath)
{
	Reset();
	m_changed = true;
	m_loaded = true;
	m_behaviorTreeName = behaviorTreeName;
	m_absoluteFilePath = absoluteFilePath;

	gEnv->pAISystem->GetIBehaviorTreeManager()->GetMetaExtensionFactory().CreateMetaExtensions(m_behaviorTreeTemplate->metaExtensionTable);
}

bool BehaviorTreeDocument::OpenFile(const char* behaviorTreeName, const char* absoluteFilePath, const BehaviorTree::NodeSerializationHints::EventsFlags& eventsFlags)
{
	XmlNodeRef behaviorTreeXmlFile = GetISystem()->LoadXmlFromFile(absoluteFilePath);
	if (!behaviorTreeXmlFile)
		return false;

	Reset();

	const bool isLoadingFromEditor = true;
	BehaviorTree::BehaviorTreeTemplate newBehaviorTreeTemplate;
	newBehaviorTreeTemplate.eventsDeclaration.SetEventsFlags(eventsFlags);

	gEnv->pAISystem->GetIBehaviorTreeManager()->GetMetaExtensionFactory().CreateMetaExtensions(newBehaviorTreeTemplate.metaExtensionTable);

	if (XmlNodeRef metaExtensionsXml = behaviorTreeXmlFile->findChild("MetaExtensions"))
	{
		if (!newBehaviorTreeTemplate.metaExtensionTable.LoadFromXml(metaExtensionsXml))
		{
			return false;
		}
	}

	if (XmlNodeRef variablesXml = behaviorTreeXmlFile->findChild("Variables"))
	{
		if (!newBehaviorTreeTemplate.variableDeclarations.LoadFromXML(variablesXml, behaviorTreeName))
		{
			return false;
		}
	}

	if (XmlNodeRef eventsXml = behaviorTreeXmlFile->findChild("Events"))
	{
		if (!newBehaviorTreeTemplate.eventsDeclaration.LoadFromXML(eventsXml, behaviorTreeName))
		{
			return false;
		}
	}

	if (XmlNodeRef signalVariablesXml = behaviorTreeXmlFile->findChild("SignalVariables"))
	{
		if (!newBehaviorTreeTemplate.signalHandler.LoadFromXML(newBehaviorTreeTemplate.variableDeclarations, newBehaviorTreeTemplate.eventsDeclaration, signalVariablesXml, behaviorTreeName, isLoadingFromEditor))
		{
			return false;
		}
	}

	if (XmlNodeRef timestampsXml = behaviorTreeXmlFile->findChild("Timestamps"))
	{
		if (!newBehaviorTreeTemplate.defaultTimestampCollection.LoadFromXml(newBehaviorTreeTemplate.eventsDeclaration, timestampsXml, behaviorTreeName, isLoadingFromEditor))
		{
			return false;
		}
	}

	BehaviorTree::INodeFactory& factory = gEnv->pAISystem->GetIBehaviorTreeManager()->GetNodeFactory();
	BehaviorTree::LoadContext context(factory, behaviorTreeName, newBehaviorTreeTemplate.variableDeclarations, newBehaviorTreeTemplate.eventsDeclaration, newBehaviorTreeTemplate.defaultTimestampCollection);
	newBehaviorTreeTemplate.rootNode = BehaviorTree::XmlLoader().CreateBehaviorTreeRootNodeFromBehaviorTreeXml(behaviorTreeXmlFile, context, isLoadingFromEditor);

	if (!newBehaviorTreeTemplate.rootNode.get())
	{
		return false;
	}

	m_loaded = true;
	m_changed = false;
	m_behaviorTreeName = behaviorTreeName;
	m_absoluteFilePath = absoluteFilePath;
	*m_behaviorTreeTemplate = newBehaviorTreeTemplate;

	return true;
}

bool BehaviorTreeDocument::Save()
{
	return SaveToFile(m_behaviorTreeName, m_absoluteFilePath);
}

bool BehaviorTreeDocument::SaveToFile(const char* behaviorTreeName, const char* absoluteFilePath)
{
	XmlNodeRef behaviorTreeXml = GenerateBehaviorTreeXml();
	if (!behaviorTreeXml)
		return false;

	if (!behaviorTreeXml->saveToFile(absoluteFilePath))
		return false;

	m_changed = false;
	m_behaviorTreeName = behaviorTreeName;
	m_absoluteFilePath = absoluteFilePath;

	return true;
}

XmlNodeRef BehaviorTreeDocument::GenerateBehaviorTreeXml()
{
	if (!m_behaviorTreeTemplate || !m_behaviorTreeTemplate->rootNode)
		return XmlNodeRef();

	XmlNodeRef behaviorTreeXml = GetISystem()->CreateXmlNode("BehaviorTree");

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION

	if (XmlNodeRef extensionsXml = m_behaviorTreeTemplate->metaExtensionTable.CreateXmlDescription())
		behaviorTreeXml->addChild(extensionsXml);

#ifdef USING_VARIABLE_COLLECTION_XML_DESCRIPTION_CREATION
	if (XmlNodeRef variablesXml = m_behaviorTreeTemplate->variableDeclarations.CreateXmlDescription())
		behaviorTreeXml->addChild(variablesXml);

	if (XmlNodeRef eventsXml = m_behaviorTreeTemplate->eventsDeclaration.CreateXmlDescription())
		behaviorTreeXml->addChild(eventsXml);

	if (XmlNodeRef signalHandlerXml = m_behaviorTreeTemplate->signalHandler.CreateXmlDescription())
		behaviorTreeXml->addChild(signalHandlerXml);
#endif

	if (XmlNodeRef timestampsXml = m_behaviorTreeTemplate->defaultTimestampCollection.CreateXmlDescription())
		behaviorTreeXml->addChild(timestampsXml);

	XmlNodeRef rootXml = behaviorTreeXml->newChild("Root");
	XmlNodeRef rootFirstNodeXml = m_behaviorTreeTemplate->rootNode->CreateXmlDescription();
	if (!rootFirstNodeXml)
		return XmlNodeRef();

	rootXml->addChild(rootFirstNodeXml);
#endif

	return behaviorTreeXml;
}


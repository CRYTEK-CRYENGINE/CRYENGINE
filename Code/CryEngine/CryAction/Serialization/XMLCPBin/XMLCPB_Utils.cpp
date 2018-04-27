// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#include "StdAfx.h"
#include "XMLCPB_Utils.h"
#include "Writer/XMLCPB_Writer.h"
#include "Writer/XMLCPB_NodeLiveWriter.h"
#include "Reader/XMLCPB_Reader.h"
#include "Reader/XMLCPB_NodeLiveReader.h"
#include "CryActionCVars.h"

using namespace XMLCPB;

#ifdef XMLCPB_DEBUGUTILS // this goes to the end of the file

CDebugUtils* CDebugUtils::s_pThis = NULL;

//////////////////////////////////////////////////////////////////////////

CDebugUtils::CDebugUtils()
{
	if (GetISystem() && GetISystem()->GetPlatformOS())
		GetISystem()->GetPlatformOS()->AddListener(this, "XMLCPB_DebugUtils");
}

CDebugUtils::~CDebugUtils()
{
	if (GetISystem() && GetISystem()->GetPlatformOS())
		GetISystem()->GetPlatformOS()->RemoveListener(this);
}

//////////////////////////////////////////////////////////////////////////

void CDebugUtils::RecursiveCopyAttrAndChildsIntoXmlNode(XmlNodeRef xmlNode, const CNodeLiveReaderRef& BNode)
{
	if (CCryActionCVars::Get().g_XMLCPBAddExtraDebugInfoToXmlDebugFiles)
	{
		xmlNode->setAttr("XMLCPBSize", BNode->GetSizeWithoutChilds());

		if (strcmp(BNode->GetTag(), "Entity") == 0 || strcmp(BNode->GetTag(), "BasicEntity") == 0)
		{
			if (BNode->HaveAttr("id"))
			{
				EntityId id;
				BNode->ReadAttr("id", id);
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
				if (pEntity)
				{
					string str;
					str.Format("%s - %u", pEntity->GetName(), id);
					xmlNode->setAttr("debInf_Name", str.c_str());  //pEntity->GetName());
					xmlNode->setAttr("debInf_ClassName", pEntity->GetClass()->GetName());
				}
			}
		}
	}

	for (int a = 0; a < BNode->GetNumAttrs(); ++a)
	{
		CAttrReader attr = BNode->ObtainAttr(a);

		switch (attr.GetBasicDataType())
		{
		case DT_STR:
			{
				const char* pData;
				attr.Get(pData);
				xmlNode->setAttr(attr.GetName(), pData);
				break;
			}

		case DT_F1:
			{
				float val;
				attr.Get(val);
				xmlNode->setAttr(attr.GetName(), val);
				break;
			}

		case DT_F3:
			{
				Vec3 val;
				attr.Get(val);
				xmlNode->setAttr(attr.GetName(), val);
				break;
			}

		case DT_F4:
			{
				Quat val;
				attr.Get(val);
				xmlNode->setAttr(attr.GetName(), val);
				break;
			}

		case DT_INT32:
			{
				int val;
				attr.Get(val);
				xmlNode->setAttr(attr.GetName(), val);
				break;
			}

		case DT_INT64:
			{
				int64 val;
				attr.Get(val);
				xmlNode->setAttr(attr.GetName(), val);
				break;
			}

		case DT_RAWDATA:
			{
				xmlNode->setAttr(attr.GetName(), "rawdata");
				break;
			}

		default:
			assert(false);
			break;
		}
	}

	uint numChilds = BNode->GetNumChildren();
	for (int c = 0; c < numChilds; ++c)
	{
		CNodeLiveReaderRef BNodeChild = BNode->GetChildNode(c);
		XmlNodeRef xmlNodeChild = xmlNode->createNode(BNodeChild->GetTag());
		xmlNode->addChild(xmlNodeChild);
		RecursiveCopyAttrAndChildsIntoXmlNode(xmlNodeChild, BNodeChild);
	}
}

//////////////////////////////////////////////////////////////////////////

XmlNodeRef CDebugUtils::BinaryFileToXml(const char* pBinaryFileName)
{
	_smart_ptr<IGeneralMemoryHeap> pHeap = CReader::CreateHeap();
	CReader reader(pHeap);
	bool ok = false;

	// waiting for the file to be saved. Very bruteforce, but this is just an occasional debug tool
	const int maxSleepTime = 5 * 1000;
	const int stepSleepTime = 100;
	int sleptTime = 0;
	while (!ok && sleptTime < maxSleepTime)
	{
		ok = reader.ReadBinaryFile(pBinaryFileName);
		if (!ok)
		{
			CrySleep(stepSleepTime);
			sleptTime += stepSleepTime;
		}
	}

	if (!ok)
	{
		XmlNodeRef xmlNode = GetISystem()->CreateXmlNode("error");
		return xmlNode;
	}

	CNodeLiveReaderRef BNode = reader.GetRoot();
	XmlNodeRef xmlNode = GetISystem()->CreateXmlNode(BNode->GetTag());

	RecursiveCopyAttrAndChildsIntoXmlNode(xmlNode, BNode);

	return xmlNode;
}

//////////////////////////////////////////////////////////////////////////

void CDebugUtils::DumpToXmlFile(CNodeLiveReaderRef BRoot, const char* pXmlFileName)
{
	XmlNodeRef xmlNode = GetISystem()->CreateXmlNode(BRoot->GetTag());
	RecursiveCopyAttrAndChildsIntoXmlNode(xmlNode, BRoot);
	xmlNode->saveToFile(pXmlFileName);
}

// warning: this debug function will not work properly in consoles if the string generated is bigger than 32k
void CDebugUtils::DumpToLog(CNodeLiveReaderRef BRoot)
{
	XmlNodeRef xmlNode = GetISystem()->CreateXmlNode(BRoot->GetTag());
	RecursiveCopyAttrAndChildsIntoXmlNode(xmlNode, BRoot);
	XmlString xmlString = xmlNode->getXML();
	CryLog("-------dumping XMLCPB::CNodeLiveReader---");
	CryLog("%s", xmlString.c_str());
	CryLog("-------end dump--------");
}

struct SEntityClassInfo
{
	uint32 m_entities;
	uint32 m_totalSize;
	SEntityClassInfo(uint32 entities, uint32 totalSize)
		: m_entities(entities), m_totalSize(totalSize)
	{}
};

typedef std::map<string, SEntityClassInfo> EntityClassInfoMap;

//////////////////////////////////////////////////////////////////////////

uint32 RecursiveCalculateSize(XmlNodeRef xmlNode, CNodeLiveReaderRef BNode, uint32 totSize, EntityClassInfoMap& entityClassInfoMap)
{
	uint32 nodeSize = BNode->GetSizeWithoutChilds();

	std::multimap<uint32, XmlNodeRef> noteworthyChilds; // autosort by size
	for (int c = 0; c < BNode->GetNumChildren(); ++c)
	{
		CNodeLiveReaderRef BNodeChild = BNode->GetChildNode(c);
		XmlNodeRef xmlNodeChild = xmlNode->createNode(BNodeChild->GetTag());
		xmlNode->addChild(xmlNodeChild);
		uint32 childSize = RecursiveCalculateSize(xmlNodeChild, BNodeChild, totSize, entityClassInfoMap);
		nodeSize += childSize;
		if (childSize >= CCryActionCVars::Get().g_XMLCPBSizeReportThreshold)
		{
			noteworthyChilds.insert(std::pair<uint32, XmlNodeRef>(childSize, xmlNodeChild));
		}
	}

	xmlNode->removeAllChilds();
	// iterate in descending order of size
	for (std::multimap<uint32, XmlNodeRef>::reverse_iterator it = noteworthyChilds.rbegin(), itEnd = noteworthyChilds.rend(); it != itEnd; ++it)
	{
		xmlNode->addChild(it->second);
	}

	if (strcmp(BNode->GetTag(), "Entity") == 0)
	{
		uint32 id;
		bool haveId = BNode->ReadAttr("id", id);
		if (haveId)
		{
			xmlNode->setAttr("id", id);
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
			if (pEntity)
			{
				xmlNode->setAttr("name", pEntity->GetName());
				std::pair<EntityClassInfoMap::iterator, bool> isInserted = entityClassInfoMap.insert(std::pair<string, SEntityClassInfo>(pEntity->GetClass()->GetName(), SEntityClassInfo(1, nodeSize)));
				if (!isInserted.second)
				{

					EntityClassInfoMap::iterator iter = entityClassInfoMap.find(pEntity->GetClass()->GetName());
					iter->second.m_totalSize += nodeSize;
					iter->second.m_entities++;
				}
			}
		}
	}

	if (nodeSize >= CCryActionCVars::Get().g_XMLCPBSizeReportThreshold)
	{
		string perc;
		perc.Format("%u - %3.1f%%", nodeSize, (nodeSize * 100.f) / totSize);
		xmlNode->setAttr("size", perc.c_str());
	}
	return nodeSize;
}

//////////////////////////////////////////////////////////////////////////

void CDebugUtils::GenerateXmlFileWithSizeInformation(const char* pBinaryFileName, const char* pXmlFileName)
{
	_smart_ptr<IGeneralMemoryHeap> pHeap = CReader::CreateHeap();
	CReader reader(pHeap);
	bool ok = reader.ReadBinaryFile(pBinaryFileName);

	if (!ok)
	{
		XmlNodeRef xmlNode = GetISystem()->CreateXmlNode("error");
		xmlNode->saveToFile(pXmlFileName);
		return;
	}

	EntityClassInfoMap entityClassInfoMap;

	CNodeLiveReaderRef BNode = reader.GetRoot();

	XmlNodeRef xmlNode = GetISystem()->CreateXmlNode(BNode->GetTag());

	RecursiveCalculateSize(xmlNode, BNode, reader.GetNodesDataSize(), entityClassInfoMap);

	// write the entity class sizes results into the xml
	XmlNodeRef xmlNodeEntityClasses = xmlNode->createNode("_______XMLCPB info__________________EntityClassSizes______________________XMLCPB info________________");
	xmlNode->addChild(xmlNodeEntityClasses);
	EntityClassInfoMap::iterator iter = entityClassInfoMap.begin();
	while (iter != entityClassInfoMap.end())
	{
		XmlNodeRef xmlNodeClass = xmlNode->createNode(iter->first.c_str());
		xmlNodeEntityClasses->addChild(xmlNodeClass);
		xmlNodeClass->setAttr("Entities", iter->second.m_entities);
		string perc;
		perc.Format("%u - %3.1f%%", iter->second.m_totalSize, (iter->second.m_totalSize * 100.f) / reader.GetNodesDataSize());
		xmlNodeClass->setAttr("size", perc.c_str());
		++iter;
	}

	xmlNode->saveToFile(pXmlFileName);
}

//////////////////////////////////////////////////////////////////////////

void CDebugUtils::SetLastFileNameSaved(const char* pFileName)
{
	Create();

	s_pThis->m_lastFileNameSaved = pFileName;
}

//////////////////////////////////////////////////////////////////////////

void CDebugUtils::OnPlatformEvent(const IPlatformOS::SPlatformEvent& event)
{
	if (CCryActionCVars::Get().g_XMLCPBGenerateXmlDebugFiles == 1)
	{
		if (event.m_eEventType == IPlatformOS::SPlatformEvent::eET_FileWrite)
		{
			if (event.m_uParams.m_fileWrite.m_type == IPlatformOS::SPlatformEvent::eFWT_SaveEnd)
			{
				const char* pFullFileName = m_lastFileNameSaved.c_str();
				const char* pSlashPosition = strrchr(pFullFileName, '/');
				stack_string XMLFileName = pSlashPosition ? pSlashPosition + 1 : pFullFileName;
				uint32 extensionPos = XMLFileName.rfind('.');
				if (extensionPos != -1)
					XMLFileName.resize(extensionPos);
				XMLFileName.append(".xml");
				stack_string XMLFileNameSizes = XMLFileName;
				XMLFileNameSizes.insert(extensionPos, "_sizesInfo");

				BinaryFileToXml(pFullFileName)->saveToFile(XMLFileName.c_str());
				GenerateXmlFileWithSizeInformation(m_lastFileNameSaved.c_str(), XMLFileNameSizes.c_str());
			}
		}
	}
}

#endif // XMLCPB_DEBUGUTILS

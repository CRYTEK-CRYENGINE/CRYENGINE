// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#pragma once

#ifndef XMLCPB_UTILS_H
	#define XMLCPB_UTILS_H

	#include "Reader/XMLCPB_NodeLiveReader.h"
	#include <CryCore/Platform/IPlatformOS.h>

namespace XMLCPB {

	#ifdef XMLCPB_DEBUGUTILS

class CDebugUtils : public IPlatformOS::IPlatformListener
{
public:

	static void Create()
	{
		if (!s_pThis)
		{
			s_pThis = new CDebugUtils();
		}
	}

	static void Destroy()
	{
		SAFE_DELETE(s_pThis);
	}

	static XmlNodeRef BinaryFileToXml(const char* pBinaryFileName);
	static void       DumpToXmlFile(CNodeLiveReaderRef BRoot, const char* pXmlFileName);
	static void       DumpToLog(CNodeLiveReaderRef BRoot);
	static void       SetLastFileNameSaved(const char* pFileName);

	// IPlatformOS::IPlatformListener
	virtual void OnPlatformEvent(const IPlatformOS::SPlatformEvent& event);
	// ~IPlatformOS::IPlatformListener

private:

	CDebugUtils();
	virtual ~CDebugUtils();
	static void RecursiveCopyAttrAndChildsIntoXmlNode(XmlNodeRef xmlNode, const CNodeLiveReaderRef& BNode);
	static void GenerateXMLFromLastSaveCmd(IConsoleCmdArgs* args);
	static void GenerateXmlFileWithSizeInformation(const char* pBinaryFileName, const char* pXmlFileName);

private:
	string              m_lastFileNameSaved;
	static CDebugUtils* s_pThis;
};

	#else //XMLCPB_DEBUGUTILS
class CDebugUtils
{
public:
	static void Create()  {}
	static void Destroy() {};
};
	#endif
} // end namespace

#endif

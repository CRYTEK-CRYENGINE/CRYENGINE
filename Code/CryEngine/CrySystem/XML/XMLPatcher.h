// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   XMLPatcher.h
//  Created:     30/01/2012 by Rob Jessop.
//  Description:
// -------------------------------------------------------------------------
//  History:	Split from GameDLL/DataPatcher so it could be applied to every
//						XML file on load
//
////////////////////////////////////////////////////////////////////////////

#ifndef __XML_PATCHER_HEADER__
#define __XML_PATCHER_HEADER__

#if CRY_PLATFORM_WINDOWS && !defined(_RELEASE)
	#define DATA_PATCH_DEBUG 1
#else
	#define DATA_PATCH_DEBUG 0
#endif

class CXMLPatcher
{
protected:
#if DATA_PATCH_DEBUG
	ICVar * m_pDumpFilesCVar;
#endif

	XmlNodeRef m_patchXML;
	const char* m_pFileBeingPatched;

	void       PatchFail(
	  const char* pInReason);
	XmlNodeRef ApplyPatchToNode(
	  const XmlNodeRef& inNode,
	  const XmlNodeRef& inPatch);

	XmlNodeRef DuplicateForPatching(
	  const XmlNodeRef& inOrig,
	  bool inShareChildren);

	bool CompareTags(
	  const XmlNodeRef& inA,
	  const XmlNodeRef& inB);
	XmlNodeRef GetMatchTag(
	  const XmlNodeRef& inNode);
	XmlNodeRef GetReplaceTag(
	  const XmlNodeRef& inNode,
	  bool* outShouldReplaceChildren);
	XmlNodeRef GetInsertTag(
	  const XmlNodeRef& inNode);
	XmlNodeRef GetDeleteTag(
	  const XmlNodeRef& inNode);
	XmlNodeRef FindPatchForFile(
	  const char* pInFileToPatch);

#if DATA_PATCH_DEBUG
	void DumpXMLNodes(
	  FILE* pInFile,
	  int inIndent,
	  const XmlNodeRef& inNode,
	  CryFixedStringT<512>* ioTempString);
	void DumpFiles(
	  const char* pInXMLFileName,
	  const XmlNodeRef& inBefore,
	  const XmlNodeRef& inAfter);
	void DumpXMLFile(
	  const char* pInFilePath,
	  const XmlNodeRef& inNode);
#endif

public:
	CXMLPatcher(XmlNodeRef& patchXML);
	~CXMLPatcher();

	XmlNodeRef ApplyXMLDataPatch(
	  const XmlNodeRef& inNode,
	  const char* pInXMLFileName);
};

#endif //__XML_PATCHER_HEADER__

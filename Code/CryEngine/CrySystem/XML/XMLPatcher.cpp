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

#include "StdAfx.h"
#include "XMLPatcher.h"
#include <CryString/StringUtils.h>

CXMLPatcher::CXMLPatcher(XmlNodeRef& patchXML)
{
	m_patchXML = patchXML;
	m_pFileBeingPatched = NULL;

#if DATA_PATCH_DEBUG
	m_pDumpFilesCVar = REGISTER_INT("g_datapatcher_dumpfiles", 0, NULL, "will dump a copy of every file data patched, before and after patching");
#endif
}

CXMLPatcher::~CXMLPatcher()
{
#if DATA_PATCH_DEBUG
	if (IConsole* pIC = gEnv->pConsole)
	{
		pIC->UnregisterVariable(m_pDumpFilesCVar->GetName());
	}
#endif
}

XmlNodeRef CXMLPatcher::DuplicateForPatching(
  const XmlNodeRef& inOrig,
  bool inShareChildren)
{
	XmlNodeRef newNode(0);

	if (m_patchXML)
	{
		newNode = m_patchXML->createNode(inOrig->getTag());
		if (newNode)
		{
			// copy attributes in a safe way, copyAttributes() itself assumes the node being copied from is of the same type
			int numAttr = inOrig->getNumAttributes();

			for (int i = 0; i < numAttr; i++)
			{
				const char* pKey, * pValue;
				if (inOrig->getAttributeByIndex(i, &pKey, &pValue))
				{
					newNode->setAttr(pKey, pValue);
				}
			}

			if (inShareChildren)
			{
				newNode->shareChildren(inOrig);
			}
		}
	}

	return newNode;
}

void CXMLPatcher::PatchFail(
  const char* pInReason)
{
	CryLogAlways("Failed to apply data patch for file '%s' - reason '%s'", m_pFileBeingPatched, pInReason);
}

XmlNodeRef CXMLPatcher::FindPatchForFile(
  const char* pInFileToPatch)
{
	XmlNodeRef result;

	if (m_patchXML)
	{
		for (int i = 0, m = m_patchXML->getChildCount(); i < m; i++)
		{
			XmlNodeRef child = m_patchXML->getChild(i);

			if (child->isTag("patch"))
			{
				const char* pForFile = child->getAttr("forfile");

				if (pForFile && CryStringUtils::stristr(pForFile, pInFileToPatch) != 0)
				{
					result = child;
					break;
				}
			}
		}
	}

	return result;
}

XmlNodeRef CXMLPatcher::ApplyPatchToNode(
  const XmlNodeRef& inNode,
  const XmlNodeRef& inPatch)
{
	XmlNodeRef result = inNode;

	for (int i = 0, m = inPatch->getChildCount(); i < m; i++)
	{
		XmlNodeRef patchNode = inPatch->getChild(i);
		if (!patchNode || stricmp(patchNode->getTag(), "patchnode") != 0)
		{
			continue;
		}

		int indexToPatch;
		if (!patchNode->getAttr("index", indexToPatch))
		{
			PatchFail("found patchnode missing index");
			continue;
		}

		int maxChildren = result->getChildCount();

		if ((indexToPatch < 0 || indexToPatch >= maxChildren) && indexToPatch != -1)
		{
			PatchFail("patchnode index out of valid range");
			continue;
		}

		XmlNodeRef childToPatch = (indexToPatch != -1) ? result->getChild(indexToPatch) : XmlNodeRef(0);
		XmlNodeRef matchTag = GetMatchTag(patchNode);

		if (childToPatch && matchTag && !CompareTags(matchTag, childToPatch))
		{
			PatchFail("patch failed to apply, data did not match what was expected");
			continue;
		}

		// we need to apply a patch to this child, make it patchable by duplicating the node

		if (inNode == result)
		{
			// make parent patchable if not already
			result = DuplicateForPatching(inNode, true);
		}

		if (XmlNodeRef insertTag = GetInsertTag(patchNode))
		{
			// insert a new child after this node
			XmlNodeRef newChild = DuplicateForPatching(insertTag, true);            // have to duplicate it as we don't have an 'insert shared child' function
			result->insertChild(indexToPatch + 1, newChild);
		}
		else
		{
			if (indexToPatch == -1)
			{
				PatchFail("child indices of -1 can only be used when inserting new nodes");
				continue;
			}
		}

		bool shouldReplaceChildren = false;

		if (XmlNodeRef replaceTag = GetReplaceTag(patchNode, &shouldReplaceChildren))
		{
			XmlNodeRef newChild = DuplicateForPatching(replaceTag, false);

			if (!shouldReplaceChildren)
			{
				newChild->shareChildren(childToPatch);
			}
			else
			{
				// note: this is inserting children that belong to the data patcher into the data being patched
				// this is fine to do, as long as the caller doesn't make any permanent changes to the xml tree
				// returned. if they did they would alter the patcher's nodes and thus affect future patches
				// applied using the same patch
				// as most callers are working with binary xmls they don't try and modify them - as this is not
				// a supported operation
				// note, if a second patch was applied to this patched tree containing the patch nodes, it
				// wouldn't mess up the patch, as patching a tree never modifies it, it always returns a new tree
				// that may share parts of the original tree
				newChild->shareChildren(replaceTag);
			}

			result->replaceChild(indexToPatch, newChild);

			childToPatch = newChild;
		}

		if (XmlNodeRef deleteTag = GetDeleteTag(patchNode))
		{
			result->deleteChildAt(indexToPatch);
			childToPatch = 0;   // deleted - don't recurse into it
		}

		if (childToPatch)
		{
			// Apply recursively
			XmlNodeRef newChild = ApplyPatchToNode(childToPatch, patchNode);

			// child has been patched, insert new child into parent
			if (newChild != childToPatch)
			{
				result->replaceChild(indexToPatch, newChild);
			}
		}
	}

	return result;
}

XmlNodeRef CXMLPatcher::ApplyXMLDataPatch(
  const XmlNodeRef& inNode,
  const char* pInXMLFileName)
{
	XmlNodeRef result = inNode;

	if (m_patchXML)
	{
		XmlNodeRef patchForFile = FindPatchForFile(pInXMLFileName);
		if (patchForFile)
		{
			m_pFileBeingPatched = pInXMLFileName;
			CryLog("Applying game data patch to %s", pInXMLFileName);
			XmlNodeRef containerNode = m_patchXML->createNode("");
			containerNode->addChild(inNode);
			containerNode = ApplyPatchToNode(containerNode, patchForFile);
			result = containerNode->getChild(0);
			m_pFileBeingPatched = NULL;

#if DATA_PATCH_DEBUG
			if (inNode != result)
			{
				DumpFiles(pInXMLFileName, inNode, result);
			}
#endif
		}
	}

	return result;
}

XmlNodeRef CXMLPatcher::GetMatchTag(
  const XmlNodeRef& inNode)
{
	XmlNodeRef result;
	XmlNodeRef nr = inNode->findChild("match");
	if (nr && nr->getChildCount() == 1)
	{
		result = nr->getChild(0);
	}
	return result;
}

XmlNodeRef CXMLPatcher::GetReplaceTag(
  const XmlNodeRef& inNode,
  bool* outShouldReplaceChildren)
{
	XmlNodeRef result;
	XmlNodeRef nr = inNode->findChild("replacewith");
	if (nr && nr->getChildCount() == 1)
	{
		if (!nr->getAttr("replaceChildren", *outShouldReplaceChildren))
		{
			*outShouldReplaceChildren = false;
		}
		result = nr->getChild(0);
	}
	return result;
}

XmlNodeRef CXMLPatcher::GetInsertTag(
  const XmlNodeRef& inNode)
{
	XmlNodeRef result;
	XmlNodeRef nr = inNode->findChild("insertAfter");
	if (nr && nr->getChildCount() == 1)
	{
		result = nr->getChild(0);
	}
	return result;
}

XmlNodeRef CXMLPatcher::GetDeleteTag(
  const XmlNodeRef& inNode)
{
	XmlNodeRef result = inNode->findChild("delete");
	return result;
}

// compares the two tags for equality of tag and attributes
// used to ensure the source data being patched meets the patches expectations
// only compares tag and attribs, doesn't do deep compare of children
bool CXMLPatcher::CompareTags(
  const XmlNodeRef& inA,
  const XmlNodeRef& inB)
{
	bool result = true;

	if (inA != inB)
	{
		result = false;

		if (stricmp(inA->getTag(), inB->getTag()) == 0)
		{
			if (inA->getNumAttributes() == inB->getNumAttributes())
			{
				result = true;

				for (int i = 0, m = inA->getNumAttributes(); i < m; i++)
				{
					const char* pAKey, * pBKey;
					const char* pAValue, * pBValue;

					inA->getAttributeByIndex(i, &pAKey, &pAValue);
					inB->getAttributeByIndex(i, &pBKey, &pBValue);

					if (stricmp(pAKey, pBKey) || stricmp(pAValue, pBValue))
					{
						result = false;
						break;
					}
				}
			}
		}
	}

	return result;
}

#if DATA_PATCH_DEBUG

static const char* k_lotsOfTabs = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";

	#define INDENT()                                   \
	  if (inIndent > 0)                                \
	  {                                                \
	    pPak->FWrite(k_lotsOfTabs, inIndent, pInFile); \
	  }

void CXMLPatcher::DumpXMLNodes(
  FILE* pInFile,
  int inIndent,
  const XmlNodeRef& inNode,
  CryFixedStringT<512>* ioTempString)
{
	ICryPak* pPak = gEnv->pCryPak;

	inIndent = min(inIndent, int(sizeof(k_lotsOfTabs) - 1));

	INDENT();

	ioTempString->Format("<%s ", inNode->getTag());

	pPak->FWrite(ioTempString->c_str(), ioTempString->length(), pInFile);

	for (int i = 0, m = inNode->getNumAttributes(); i < m; i++)
	{
		const char* pKey, * pVal;
		inNode->getAttributeByIndex(i, &pKey, &pVal);
		ioTempString->Format("%s=\"%s\" ", pKey, pVal);
		pPak->FWrite(ioTempString->c_str(), ioTempString->length(), pInFile);
	}
	pPak->FWrite(">\n", 2, pInFile);

	for (int i = 0, m = inNode->getChildCount(); i < m; i++)
	{
		DumpXMLNodes(pInFile, inIndent + 1, inNode->getChild(i), ioTempString);
	}

	INDENT();
	ioTempString->Format("</%s>\n", inNode->getTag());
	pPak->FWrite(ioTempString->c_str(), ioTempString->length(), pInFile);
}

void CXMLPatcher::DumpFiles(
  const char* pInXMLFileName,
  const XmlNodeRef& inBefore,
  const XmlNodeRef& inAfter)
{
	if (m_pDumpFilesCVar->GetIVal())
	{
		CryLog("Dumping before and after data files for '%s'", pInXMLFileName);

		const char* pOrigFileName;

		if (pOrigFileName = strrchr(pInXMLFileName, '/'))
		{
			pOrigFileName++;

			DumpXMLFile(string().Format("PATCH_%s", pOrigFileName), inBefore);

			CryFixedStringT<128> newFileName(pOrigFileName);
			newFileName.replace(".xml", "_patched.xml");

			DumpXMLFile(string().Format("PATCH_%s", newFileName.c_str()), inAfter);
		}
		else
		{
			CryLog("Couldn't determine file name for path '%s' can't output diffs", pInXMLFileName);
		}
	}
}

void CXMLPatcher::DumpXMLFile(
  const char* pInFilePath,
  const XmlNodeRef& inNode)
{

	ICryPak* pIPak = GetISystem()->GetIPak();
	FILE* pFile = pIPak->FOpen(pInFilePath, "wb");

	if (pFile)
	{
		CryFixedStringT<512> tempStr;

		DumpXMLNodes(pFile, 0, inNode, &tempStr);

		pIPak->FClose(pFile);
	}
}
#endif

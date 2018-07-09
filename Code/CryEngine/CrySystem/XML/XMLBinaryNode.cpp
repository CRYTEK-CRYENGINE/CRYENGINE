// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "XMLBinaryNode.h"
#include <CryMemory/CrySizer.h>
#include <cctype>

#pragma warning(disable : 6031) // Return value ignored: 'sscanf'

//////////////////////////////////////////////////////////////////////////
CBinaryXmlData::CBinaryXmlData()
	: pNodes(0)
	, pAttributes(0)
	, pChildIndices(0)
	, pStringData(0)
	, pFileContents(0)
	, nFileSize(0)
	, bOwnsFileContentsMemory(true)
	, pBinaryNodes(0)
	, nRefCount(0)
{
}

//////////////////////////////////////////////////////////////////////////
CBinaryXmlData::~CBinaryXmlData()
{
	if (bOwnsFileContentsMemory)
	{
		delete[] pFileContents;
	}
	pFileContents = 0;

	delete[] pBinaryNodes;
	pBinaryNodes = 0;
}

void CBinaryXmlData::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(pFileContents, nFileSize);
	const XMLBinary::BinaryFileHeader* pHeader = reinterpret_cast<const XMLBinary::BinaryFileHeader*>(pFileContents);
	pSizer->AddObject(pBinaryNodes, sizeof(CBinaryXmlNode) * pHeader->nNodeCount);
}

//////////////////////////////////////////////////////////////////////////
// CBinaryXmlNode implementation.
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// collect allocated memory  informations
void CBinaryXmlNode::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(m_pData);
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CBinaryXmlNode::getParent() const
{
	const XMLBinary::Node* const pNode = _node();
	if (pNode->nParentIndex != (XMLBinary::NodeIndex)-1)
	{
		return &m_pData->pBinaryNodes[pNode->nParentIndex];
	}
	return XmlNodeRef();
}

XmlNodeRef CBinaryXmlNode::createNode(const char* tag)
{
	assert(0);
	return 0;
}

//////////////////////////////////////////////////////////////////////////
bool CBinaryXmlNode::isTag(const char* tag) const
{
	return g_pXmlStrCmp(tag, getTag()) == 0;
}

const char* CBinaryXmlNode::getAttr(const char* key) const
{
	const char* svalue = GetValue(key);
	if (svalue)
		return svalue;
	return "";
}

bool CBinaryXmlNode::getAttr(const char* key, const char** value) const
{
	const char* svalue = GetValue(key);
	if (svalue)
	{
		*value = svalue;
		return true;
	}
	else
	{
		*value = "";
		return false;
	}
}

bool CBinaryXmlNode::haveAttr(const char* key) const
{
	return (GetValue(key) != 0);
}

//////////////////////////////////////////////////////////////////////////
bool CBinaryXmlNode::getAttr(const char* key, int& value) const
{
	const char* svalue = GetValue(key);
	if (svalue)
	{
		value = atoi(svalue);
		return true;
	}
	return false;
}

bool CBinaryXmlNode::getAttr(const char* key, unsigned int& value) const
{
	const char* svalue = GetValue(key);
	if (svalue)
	{
		value = strtoul(svalue, NULL, 10);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CBinaryXmlNode::getAttr(const char* key, int64& value) const
{
	const char* svalue = GetValue(key);
	if (svalue)
	{
		sscanf(svalue, "%" PRId64, &value);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CBinaryXmlNode::getAttr(const char* key, uint64& value, bool useHexFormat) const
{
	const char* svalue = GetValue(key);
	if (svalue)
	{
		if (useHexFormat)
		{
			sscanf(svalue, "%" PRIX64, &value);
		}
		else
		{
			sscanf(svalue, "%" PRIu64, &value);
		}
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CBinaryXmlNode::getAttr(const char* key, bool& value) const
{
	bool isSuccess = false;
	char const* const szValue = GetValue(key);

	if (szValue != nullptr && szValue[0] != '\0')
	{
		if (std::isalpha(*szValue) != 0)
		{
			if (g_pXmlStrCmp(szValue, "false") == 0)
			{
				value = false;
				isSuccess = true;
			}
			else if (g_pXmlStrCmp(szValue, "true") == 0)
			{
				value = true;
				isSuccess = true;
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Encountered invalid value during CBinaryXmlNode::getAttr! Value: %s Tag: %s", szValue, getTag());
			}
		}
		else
		{
			int const number = std::atoi(szValue);

			if (number == 0)
			{
				value = false;
				isSuccess = true;
			}
			else if (number == 1)
			{
				value = true;
				isSuccess = true;
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Encountered invalid value during CBinaryXmlNode::getAttr! Value: %s Tag: %s", szValue, getTag());
			}
		}
	}

	return isSuccess;
}

bool CBinaryXmlNode::getAttr(const char* key, float& value) const
{
	const char* svalue = GetValue(key);
	if (svalue)
	{
		value = (float)atof(svalue);
		return true;
	}
	return false;
}

bool CBinaryXmlNode::getAttr(const char* key, double& value) const
{
	const char* svalue = GetValue(key);
	if (svalue)
	{
		value = atof(svalue);
		return true;
	}
	return false;
}

bool CBinaryXmlNode::getAttr(const char* key, Ang3& value) const
{
	const char* svalue = GetValue(key);
	if (svalue)
	{
		float x, y, z;
		if (sscanf(svalue, "%f,%f,%f", &x, &y, &z) == 3)
		{
			value(x, y, z);
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CBinaryXmlNode::getAttr(const char* key, Vec3& value) const
{
	const char* svalue = GetValue(key);
	if (svalue)
	{
		float x, y, z;
		if (sscanf(svalue, "%f,%f,%f", &x, &y, &z) == 3)
		{
			value = Vec3(x, y, z);
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CBinaryXmlNode::getAttr(const char* key, Vec4& value) const
{
	const char* svalue = GetValue(key);
	if (svalue)
	{
		float x, y, z, w;
		if (sscanf(svalue, "%f,%f,%f,%f", &x, &y, &z, &w) == 4)
		{
			value = Vec4(x, y, z, w);
			return true;
		}
	}
	return false;
}

bool CBinaryXmlNode::getAttr(const char* key, Vec3d& value) const
{
	const char* svalue = GetValue(key);
	if (svalue)
	{
		double x, y, z;
		if (sscanf(svalue, "%lf,%lf,%lf", &x, &y, &z) == 3)
		{
			value = Vec3d(x, y, z);
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CBinaryXmlNode::getAttr(const char* key, Vec2& value) const
{
	const char* svalue = GetValue(key);
	if (svalue)
	{
		float x, y;
		if (sscanf(svalue, "%f,%f", &x, &y) == 2)
		{
			value = Vec2(x, y);
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CBinaryXmlNode::getAttr(const char* key, Vec2d& value) const
{
	const char* svalue = GetValue(key);
	if (svalue)
	{
		double x, y;
		if (sscanf(svalue, "%lf,%lf", &x, &y) == 2)
		{
			value = Vec2d(x, y);
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CBinaryXmlNode::getAttr(const char* key, Quat& value) const
{
	const char* svalue = GetValue(key);
	if (svalue)
	{
		float w, x, y, z;
		if (sscanf(svalue, "%f,%f,%f,%f", &w, &x, &y, &z) == 4)
		{
			value = Quat(w, x, y, z);
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CBinaryXmlNode::getAttr(const char* key, ColorB& value) const
{
	const char* svalue = GetValue(key);
	if (svalue)
	{
		unsigned int r, g, b, a = 255;
		int numFound = sscanf(svalue, "%u,%u,%u,%u", &r, &g, &b, &a);
		if (numFound == 3 || numFound == 4)
		{
			// If we only found 3 values, a should be unchanged, and still be 255
			if (r < 256 && g < 256 && b < 256 && a < 256)
			{
				value = ColorB(r, g, b, a);
				return true;
			}
		}
	}
	return false;
}

void CBinaryXmlNode::setAttr(const char* key, const CryGUID& value)
{
	setAttr(key, value.ToString());
}

bool CBinaryXmlNode::getAttr(const char* key, CryGUID& value) const
{
	const char* svalue = GetValue(key);
	if (svalue)
	{
		value = CryGUID::FromString(svalue);
		if ((value.hipart >> 32) == 0)
		{
			memset(&value, 0, sizeof(value));
			// If bad GUID, use old guid system.
			// Not sure if this will apply well in CryGUID!
			value.hipart = (uint64)atoi(svalue) << 32;
		}
		return true;
	}
	return false;
}

XmlNodeRef CBinaryXmlNode::findChild(const char* tag) const
{
	const XMLBinary::Node* const pNode = _node();
	const uint32 nFirst = pNode->nFirstChildIndex;
	const uint32 nAfterLast = pNode->nFirstChildIndex + pNode->nChildCount;
	for (uint32 i = nFirst; i < nAfterLast; ++i)
	{
		const char* sChildTag = m_pData->pStringData + m_pData->pNodes[m_pData->pChildIndices[i]].nTagStringOffset;
		if (g_pXmlStrCmp(tag, sChildTag) == 0)
		{
			return m_pData->pBinaryNodes + m_pData->pChildIndices[i];
		}
	}
	return 0;
}

//! Get XML Node child nodes.
XmlNodeRef CBinaryXmlNode::getChild(int i) const
{
	const XMLBinary::Node* const pNode = _node();
	assert(i >= 0 && i < (int)pNode->nChildCount);
	return m_pData->pBinaryNodes + m_pData->pChildIndices[pNode->nFirstChildIndex + i];
}

//////////////////////////////////////////////////////////////////////////
bool CBinaryXmlNode::getAttributeByIndex(int index, const char** key, const char** value)
{
	const XMLBinary::Node* const pNode = _node();
	if (index >= 0 && index < pNode->nAttributeCount)
	{
		const XMLBinary::Attribute& attr = m_pData->pAttributes[pNode->nFirstAttributeIndex + index];
		*key = _string(attr.nKeyStringOffset);
		*value = _string(attr.nValueStringOffset);
		return true;
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////
bool CBinaryXmlNode::getAttributeByIndex(int index, XmlString& key, XmlString& value)
{
	const XMLBinary::Node* const pNode = _node();
	if (index >= 0 && index < pNode->nAttributeCount)
	{
		const XMLBinary::Attribute& attr = m_pData->pAttributes[pNode->nFirstAttributeIndex + index];
		key = _string(attr.nKeyStringOffset);
		value = _string(attr.nValueStringOffset);
		return true;
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/yasli/JSONIArchive.h>
#include <CrySerialization/yasli/JSONOArchive.h>
#include <CrySerialization/yasli/BinArchive.h>
#include "XmlIArchive.h"
#include "XmlOArchive.h"
#include <CrySerialization/BlackBox.h>
#include <CrySerialization/ClassFactory.h>
#include <CrySystem/File/ICryPak.h>

namespace Serialization
{
constexpr const char* kXmlVersionAttribute = "CryXmlVersion";
constexpr ECryXmlVersion kXmlVersionCurrent = ECryXmlVersion::Version2;

static int g_sys_archive_host_xml_version = (int)kXmlVersionCurrent;


bool LoadFile(std::vector<char>& content, const char* filename, bool bFileCanBeOnDisk)
{
	FILE* f = gEnv->pCryPak->FOpen(filename, "rb", bFileCanBeOnDisk ? ICryPak::FOPEN_ONDISK : 0);
	if (!f)
		return false;

	gEnv->pCryPak->FSeek(f, 0, SEEK_END);
	size_t size = gEnv->pCryPak->FTell(f);
	gEnv->pCryPak->FSeek(f, 0, SEEK_SET);

	content.resize(size);
	bool result = true;
	if (size != 0)
		result = gEnv->pCryPak->FRead(&content[0], size, f) == size;
	gEnv->pCryPak->FClose(f);
	return result;
}

class CArchiveHost : public IArchiveHost
{
public:
	bool LoadJsonFile(const SStruct& outObj, const char* filename, bool bCanBeOnDisk) override
	{
		std::vector<char> content;
		if (!LoadFile(content, filename, true))
			return false;
		yasli::JSONIArchive ia;
		if (!ia.open(content.data(), content.size()))
			return false;
		return ia(outObj);
	}

	bool SaveJsonFile(const char* gameFilename, const SStruct& obj) override
	{
		char buffer[ICryPak::g_nMaxPath];
		const char* filename = gEnv->pCryPak->AdjustFileName(gameFilename, buffer, ICryPak::FLAGS_FOR_WRITING);
		yasli::JSONOArchive oa;
		if (!oa(obj))
			return false;
		return oa.save(filename);
	}

	bool LoadJsonBuffer(const SStruct& obj, const char* buffer, size_t bufferLength) override
	{
		if (bufferLength == 0)
			return false;
		yasli::JSONIArchive ia;
		if (!ia.open(buffer, bufferLength))
			return false;
		return ia(obj);
	}

	bool SaveJsonBuffer(DynArray<char>& buffer, const SStruct& obj) override
	{
		yasli::JSONOArchive oa;
		if (!oa(obj))
			return false;
		buffer.assign(oa.buffer(), oa.buffer() + oa.length());
		return true;
	}

	bool LoadBinaryFile(const SStruct& obj, const char* filename) override
	{
		std::vector<char> content;
		if (!LoadFile(content, filename, false))
			return false;
		yasli::BinIArchive ia;
		if (!ia.open(content.data(), content.size()))
			return false;
		return ia(obj);
	}

	bool SaveBinaryFile(const char* gameFilename, const SStruct& obj) override
	{
		char buffer[ICryPak::g_nMaxPath];
		const char* filename = gEnv->pCryPak->AdjustFileName(gameFilename, buffer, ICryPak::FLAGS_FOR_WRITING);
		yasli::BinOArchive oa;
		obj(oa);
		return oa.save(filename);
	}

	bool LoadBinaryBuffer(const SStruct& obj, const char* buffer, size_t bufferLength) override
	{
		if (bufferLength == 0)
			return false;
		yasli::BinIArchive ia;
		if (!ia.open(buffer, bufferLength))
			return false;
		return ia(obj);
	}

	bool SaveBinaryBuffer(DynArray<char>& buffer, const SStruct& obj) override
	{
		yasli::BinOArchive oa;
		obj(oa);
		buffer.assign(oa.buffer(), oa.buffer() + oa.length());
		return true;
	}

	bool CloneBinary(const SStruct& dest, const SStruct& src) override
	{
		yasli::BinOArchive oa;
		src(oa);
		yasli::BinIArchive ia;
		if (!ia.open(oa.buffer(), oa.length()))
			return false;
		dest(ia);
		return true;
	}

	bool CompareBinary(const SStruct& lhs, const SStruct& rhs) override
	{
		yasli::BinOArchive oa1;
		lhs(oa1);
		yasli::BinOArchive oa2;
		rhs(oa2);
		if (oa1.length() != oa2.length())
			return false;
		return memcmp(oa1.buffer(), oa2.buffer(), oa1.length()) == 0;
	}

	bool SaveXmlFile(const char* filename, const SStruct& obj, const char* rootNodeName, ECryXmlVersion forceVersion = ECryXmlVersion::Auto) override
	{
		XmlNodeRef node = SaveXmlNode(obj, rootNodeName, forceVersion);
		if (!node)
			return false;
		return node->saveToFile(filename);
	}

	bool LoadXmlFile(const SStruct& obj, const char* filename, ECryXmlVersion forceVersion = ECryXmlVersion::Auto) override
	{
		XmlNodeRef node = gEnv->pSystem->LoadXmlFromFile(filename);
		if (!node)
			return false;
		return LoadXmlNode(obj, node, forceVersion);
	}

	static ECryXmlVersion SelectCryXmlVersionToSave(ECryXmlVersion forceVersion)
	{
		ECryXmlVersion version = ECryXmlVersion::Auto;
		if (forceVersion == ECryXmlVersion::Auto)
		{
			version = (ECryXmlVersion)g_sys_archive_host_xml_version;
		}
		if (version == ECryXmlVersion::Auto || version >= ECryXmlVersion::Last)
		{
			version = kXmlVersionCurrent;
		}
		return version;
	}

	XmlNodeRef SaveXmlNode(const SStruct& obj, const char* nodeName, ECryXmlVersion forceVersion = ECryXmlVersion::Auto) override
	{
		const ECryXmlVersion version = SelectCryXmlVersionToSave(forceVersion);
		switch (version)
		{
		case ECryXmlVersion::Version1: return SaveXmlNodeVer1(obj, nodeName);
		case ECryXmlVersion::Version2: return SaveXmlNodeVer2(obj, nodeName);
		}
		return XmlNodeRef();
	}

	bool SaveXmlNode(XmlNodeRef& node, const SStruct& obj, ECryXmlVersion forceVersion = ECryXmlVersion::Auto) override
	{
		const ECryXmlVersion version = SelectCryXmlVersionToSave(forceVersion);
		switch (version)
		{
		case ECryXmlVersion::Version1: return SaveXmlNodeVer1(node, obj);
		case ECryXmlVersion::Version2: return SaveXmlNodeVer2(node, obj);
		}
		return false;
	}


	XmlNodeRef SaveXmlNodeVer2(const SStruct& obj, const char* nodeName)
	{
		CXmlOutputArchive oa;
		XmlNodeRef node = gEnv->pSystem->CreateXmlNode(nodeName);
		if (!node)
			return XmlNodeRef();
		node->setAttr(kXmlVersionAttribute, (int)ECryXmlVersion::Version2);
		oa.SetXmlNode(node);
		if (!obj(oa))
			return XmlNodeRef();
		return oa.GetXmlNode();
	}

	bool SaveXmlNodeVer2(XmlNodeRef& node, const SStruct& obj)
	{
		if (!node)
			return false;
		CXmlOutputArchive oa;
		node->setAttr(kXmlVersionAttribute, (int)ECryXmlVersion::Version2);
		oa.SetXmlNode(node);
		return obj(oa);
	}

	XmlNodeRef SaveXmlNodeVer1(const SStruct& obj, const char* nodeName)
	{
		CXmlOArchiveVer1 oa;
		XmlNodeRef node = gEnv->pSystem->CreateXmlNode(nodeName);
		if (!node)
			return XmlNodeRef();
		oa.SetXmlNode(node);
		if (!obj(oa))
			return XmlNodeRef();
		return oa.GetXmlNode();
	}

	bool SaveXmlNodeVer1(XmlNodeRef& node, const SStruct& obj)
	{
		if (!node)
			return false;
		CXmlOArchiveVer1 oa;
		oa.SetXmlNode(node);
		return obj(oa);
	}

	static ECryXmlVersion SelectCryXmlVersionToLoad(const XmlNodeRef& node, ECryXmlVersion forceVersion)
	{
		ECryXmlVersion version = ECryXmlVersion::Version1;   // Default to old version
		if (forceVersion == ECryXmlVersion::Auto)
		{
			int iVersion = (int)version;
			node->getAttr(kXmlVersionAttribute, iVersion);
			version = (ECryXmlVersion)iVersion;
		}
		else
		{
			version = forceVersion;
		}
		return version;
	}

	bool LoadXmlNode(const SStruct& obj, const XmlNodeRef& node, ECryXmlVersion forceVersion = ECryXmlVersion::Auto) override
	{
		const ECryXmlVersion version = SelectCryXmlVersionToLoad(node, forceVersion);
		switch (version)
		{
		case ECryXmlVersion::Version1: return LoadXmlNodeVer1(obj, node);
		case ECryXmlVersion::Version2: return LoadXmlNodeVer2(obj, node);
		}
		return false;
	}

	bool LoadXmlNodeVer1(const SStruct& obj, const XmlNodeRef& node)
	{
		CXmlIArchiveVer1 ia;
		ia.SetXmlNode(node);
		return obj(ia);
	}

	bool LoadXmlNodeVer2(const SStruct& obj, const XmlNodeRef& node)
	{
		CXmlInputArchive ia;
		ia.SetXmlNode(node);
		return obj(ia);
	}

	bool LoadBlackBox(const SStruct& outObj, SBlackBox& box) override
	{
		if (box.format && box.data)
		{
			if (strcmp(box.format, "json") == 0)
			{
				return LoadJsonBuffer(outObj, static_cast<const char*>(box.data), box.size);
			}
			else if (strcmp(box.format, "xml") == 0)
			{
				return LoadXmlNode(outObj, *static_cast<const XmlNodeRef*>(box.data), (ECryXmlVersion)box.xmlVersion);
			}
		}
		return false;
	}
};

IArchiveHost* CreateArchiveHost()
{
	return new CArchiveHost;
}

void RegisterArchiveHostCVars()
{
	REGISTER_CVAR2("sys_archive_host_xml_version", &g_sys_archive_host_xml_version, g_sys_archive_host_xml_version, VF_NULL,
		"Selects default CryXmlVersion");
}


}

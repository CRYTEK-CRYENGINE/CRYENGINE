// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Containers/CryArray.h>
#include <CrySerialization/Forward.h>
#include <CrySerialization/IArchive.h>
#include <CrySystem/XML/IXml.h>

class XmlNodeRef;

namespace Serialization
{
//! Specifies the version of xml archive implementation used by the IArchiveHost
enum class ECryXmlVersion : uint32
{
	Auto = 0,            //!< Automatically select version to use
	Version1 = 1,        //!< Use xml archive version 1
	Version2 = 2,        //!< Use xml archive version 2
	Last                 //!< Keep this value as the last version
};


//! IArchiveHost serves a purpose of sharing IArchive implementations among diffferent modules.
//! Example of usage:
//! struct SType
//! {
//!    void Serialize(Serialization::IArchive& ar);
//! };
//!
//! SType instanceToSave;
//! bool saved = Serialization::SaveJsonFile("Scripts/instance.json", instanceToSave);
//!
//! SType instanceToLoad;
//! bool loaded = Serialization::LoadJsonFile(instanceToLoad, "Scripts/instance.json");
struct IArchiveHost
{
	virtual ~IArchiveHost() {}
	//! Parses JSON from file into the specified object
	//! \par Example
	//! \include CrySystem/Examples/JsonSerialization.cpp
	virtual bool LoadJsonFile(const SStruct& outObj, const char* filename, bool bCanBeOnDisk) = 0;
	//! Saves JSON into a file, reading data from the specified object
	//! \par Example
	//! \include CrySystem/Examples/JsonSerialization.cpp
	virtual bool SaveJsonFile(const char* filename, const SStruct& obj) = 0;
	//! Parses JSON from buffer into the specified object
	//! \par Example
	//! \include CrySystem/Examples/JsonSerialization.cpp
	virtual bool LoadJsonBuffer(const SStruct& outObj, const char* buffer, size_t bufferLength) = 0;
	//! Saves JSON into a buffer, reading data from the specified object
	virtual bool SaveJsonBuffer(DynArray<char>& outBuffer, const SStruct& obj) = 0;

	virtual bool LoadBinaryFile(const SStruct& outObj, const char* filename) = 0;
	virtual bool SaveBinaryFile(const char* filename, const SStruct& obj) = 0;
	virtual bool LoadBinaryBuffer(const SStruct& outObj, const char* buffer, size_t bufferLength) = 0;
	virtual bool SaveBinaryBuffer(DynArray<char>& outBuffer, const SStruct& obj) = 0;
	virtual bool CloneBinary(const SStruct& dest, const SStruct& source) = 0;

	//! Compares two instances in serialized form through binary archive
	virtual bool       CompareBinary(const SStruct& lhs, const SStruct& rhs) = 0;

	virtual bool       LoadXmlFile(const SStruct& outObj, const char* filename, ECryXmlVersion forceVersion = ECryXmlVersion::Auto) = 0;
	virtual bool       SaveXmlFile(const char* filename, const SStruct& obj, const char* rootNodeName, ECryXmlVersion forceVersion = ECryXmlVersion::Auto) = 0;
	virtual bool       LoadXmlNode(const SStruct& outObj, const XmlNodeRef& node, ECryXmlVersion forceVersion = ECryXmlVersion::Auto) = 0;
	virtual XmlNodeRef SaveXmlNode(const SStruct& obj, const char* nodeName, ECryXmlVersion forceVersion = ECryXmlVersion::Auto) = 0;
	virtual bool       SaveXmlNode(XmlNodeRef& node, const SStruct& obj, ECryXmlVersion forceVersion = ECryXmlVersion::Auto) = 0;

	virtual bool       LoadBlackBox(const SStruct& outObj, SBlackBox& box) = 0;
};
} // namespace Serialization

#include <CrySystem/ISystem.h> // gEnv

namespace Serialization
{
//! Syntactic sugar.
template<class T> bool LoadJsonFile(T& instance, const char* filename)
{
	return gEnv->pSystem->GetArchiveHost()->LoadJsonFile(Serialization::SStruct(instance), filename, false);
}

template<class T> bool SaveJsonFile(const char* filename, const T& instance)
{
	return gEnv->pSystem->GetArchiveHost()->SaveJsonFile(filename, Serialization::SStruct(instance));
}

template<class T> bool LoadJsonBuffer(T& instance, const char* buffer, size_t bufferLength)
{
	return gEnv->pSystem->GetArchiveHost()->LoadJsonBuffer(Serialization::SStruct(instance), buffer, bufferLength);
}

template<class T> bool SaveJsonBuffer(DynArray<char>& outBuffer, const T& instance)
{
	return gEnv->pSystem->GetArchiveHost()->SaveJsonBuffer(outBuffer, Serialization::SStruct(instance));
}

// ---------------------------------------------------------------------------

template<class T> bool LoadBinaryFile(T& outInstance, const char* filename)
{
	return gEnv->pSystem->GetArchiveHost()->LoadBinaryFile(Serialization::SStruct(outInstance), filename);
}

template<class T> bool SaveBinaryFile(const char* filename, const T& instance)
{
	return gEnv->pSystem->GetArchiveHost()->SaveBinaryFile(filename, Serialization::SStruct(instance));
}

template<class T> bool LoadBinaryBuffer(T& outInstance, const char* buffer, size_t bufferLength)
{
	return gEnv->pSystem->GetArchiveHost()->LoadBinaryBuffer(Serialization::SStruct(outInstance), buffer, bufferLength);
}

template<class T> bool SaveBinaryBuffer(DynArray<char>& outBuffer, const T& instance)
{
	return gEnv->pSystem->GetArchiveHost()->SaveBinaryBuffer(outBuffer, Serialization::SStruct(instance));
}

template<class T> bool CloneBinary(T& outInstance, const T& inInstance)
{
	return gEnv->pSystem->GetArchiveHost()->CloneBinary(Serialization::SStruct(outInstance), Serialization::SStruct(inInstance));
}

template<class T> bool CompareBinary(const T& lhs, const T& rhs)
{
	return gEnv->pSystem->GetArchiveHost()->CompareBinary(Serialization::SStruct(lhs), Serialization::SStruct(rhs));
}

// ---------------------------------------------------------------------------

template<class T> bool LoadXmlFile(T& outInstance, const char* filename, ECryXmlVersion forceVersion = ECryXmlVersion::Auto)
{
	return gEnv->pSystem->GetArchiveHost()->LoadXmlFile(Serialization::SStruct(outInstance), filename, forceVersion);
}

template<class T> bool SaveXmlFile(const char* filename, const T& instance, const char* rootNodeName, ECryXmlVersion forceVersion = ECryXmlVersion::Auto)
{
	return gEnv->pSystem->GetArchiveHost()->SaveXmlFile(filename, Serialization::SStruct(instance), rootNodeName, forceVersion);
}

template<class T> bool LoadXmlNode(T& outInstance, const XmlNodeRef& node, ECryXmlVersion forceVersion = ECryXmlVersion::Auto)
{
	return gEnv->pSystem->GetArchiveHost()->LoadXmlNode(Serialization::SStruct(outInstance), node, forceVersion);
}

template<class T> XmlNodeRef SaveXmlNode(const T& instance, const char* nodeName, ECryXmlVersion forceVersion = ECryXmlVersion::Auto)
{
	return gEnv->pSystem->GetArchiveHost()->SaveXmlNode(Serialization::SStruct(instance), nodeName, forceVersion);
}

template<class T> bool SaveXmlNode(XmlNodeRef& node, const T& instance, ECryXmlVersion forceVersion = ECryXmlVersion::Auto)
{
	return gEnv->pSystem->GetArchiveHost()->SaveXmlNode(node, Serialization::SStruct(instance), forceVersion);
}

// ---------------------------------------------------------------------------

template<class T> bool LoadBlackBox(T& outInstance, SBlackBox& box)
{
	return gEnv->pSystem->GetArchiveHost()->LoadBlackBox(Serialization::SStruct(outInstance), box);
}

}

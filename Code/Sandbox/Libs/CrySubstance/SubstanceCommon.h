// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "CrySerialization/Serializer.h"
#include "CrySubstanceAPI.h"
#include "substance/framework/typedefs.h"



struct CRY_SUBSTANCE_API IFileManipulator
{
	virtual bool ReadFile(const string& filePath, std::vector<char>& buffer, size_t& readSize, const string& mode) = 0;
	virtual string GetAbsolutePath(const string& filename) const = 0;
};

extern IFileManipulator* g_fileManipulator;

void CRY_SUBSTANCE_API InitCrySubstanceLib(IFileManipulator* fileManipulator);




struct CRY_SUBSTANCE_API SSubstanceOutputChannelMapping
{
	SSubstanceOutputChannelMapping();
	char sourceId; // if -1 channel is not used
	char channel;
	void Reset();
	virtual void Serialize(Serialization::IArchive& ar);
};

struct CRY_SUBSTANCE_API SSubstanceOutput
{
	enum ESubstanceOutputResolution {
		Original = 0,
		Half = 1,
		Quarter = 2,
		Eighth = 3,
	};

	SSubstanceOutput();
	void SetAllSources(int value = 0);
	void SetAllChannels(int value = -1);
	void RemoveSource(int sourceID);
	bool IsValid();
	void UpdateState(const std::vector<string>& availableSourceOutputs);
	bool enabled;
	string preset;
	string name;
	int flags;
	std::vector<string> sourceOutputs;
	ESubstanceOutputResolution resolution;
	SSubstanceOutputChannelMapping channels[4];

	virtual void Serialize(Serialization::IArchive& ar);
};


struct CRY_SUBSTANCE_API SSubstanceRenderData
{
	SSubstanceRenderData()
		: format(0)
		, useMips(false)
		, skipAlpha(false)
		, swapRG(false)
		, customData(0)
		, name("")
	{}
	SSubstanceOutput output;
	string name;
	unsigned int format;
	bool useMips;
	bool skipAlpha;
	bool swapRG;
	size_t customData;
};


CRY_SUBSTANCE_API extern std::vector<std::pair<string, SSubstanceOutput::ESubstanceOutputResolution>> resolutionNamesMap;

namespace SubstanceSerialization {
	template<class T> bool LoadJsonFile(T& instance, const string& filename);
	template<class T> bool SaveJsonFile(const string& filename, const T& instance);
	
	template<class T> bool Save(const string& filename, const T& instance)
	{
		return SaveJsonFile<T>(filename, instance);
	}
	
	template<class T> bool Load(T& instance, const string& filename)
	{
		return LoadJsonFile<T>(instance, filename);
	}
}

struct CRY_SUBSTANCE_API ISubstancePresetSerializer
{
	virtual void Serialize(Serialization::IArchive& ar) = 0;
};

class CRY_SUBSTANCE_API ISubstancePreset
{
public:
	virtual void Save() = 0;
	static ISubstancePreset* Load(const string& filePath);
	static ISubstancePreset* Instantiate(const string& archiveName, const string& graphName);
	virtual void Reload() = 0;
	virtual void Reset() = 0;
	virtual const string& GetGraphName() const = 0;
	virtual const string& GetSubstanceArchive() const = 0;
	virtual const string GetFileName() const = 0;
	virtual ISubstancePresetSerializer* GetSerializer() = 0;
	virtual std::vector<SSubstanceOutput>& GetOutputs() = 0;
	virtual const std::vector<string> GetInputImages() const = 0;
	virtual const std::vector<SSubstanceOutput>& GetDefaultOutputs() const = 0;
	virtual void SetGraphResolution(const int& x, const int& y) = 0;
	virtual const string GetOutputPath(const SSubstanceOutput* output) const = 0;
	virtual const SubstanceAir::UInt& GetInstanceID() const = 0;
	virtual ISubstancePreset* Clone() const = 0;
	virtual void Destroy() = 0;
};

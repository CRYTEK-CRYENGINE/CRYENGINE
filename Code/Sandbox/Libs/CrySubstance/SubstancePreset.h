// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "CrySerialization/Serializer.h"
#include "CrySerialization/ClassFactory.h"
#include "substance/framework/graph.h"
#include "CrySubstanceAPI.h"
#include "SubstanceCommon.h"
#include "ISubstanceInstanceRenderer.h"
#include <type_traits>

namespace SubstanceAir
{
	class InputInstanceBase;
}

class CAsset;

class CSubstancePreset : public ISubstancePreset
{
public:

	// ISubstancePreset
	void Save() override;
	void Reload() override;
	void Reset() override;
	
	std::vector<SSubstanceOutput>& GetOutputs()  override { return m_outputs; }
	const std::vector<SSubstanceOutput>& GetDefaultOutputs() const  override { return m_tempOriginalOutputs; }
	void RemoveOutput(SSubstanceOutput* output);
	SubstanceAir::Vec2Int GetGraphResolution() const;
	void SetGraphResolution(const int& x, const int& y) override;
	const string& GetGraphName() const override { return m_graphName; }
	const string& GetSubstanceArchive() const override { return m_substanceArchive; }
	const string GetFileName() const override;
	const string GetOutputPath(const SSubstanceOutput* output) const override;
	const SubstanceAir::UInt& GetInstanceID() const override;
	SubstanceAir::GraphInstance& PrepareRenderInstance(ISubstanceInstanceRenderer* renderer);
	const SubstanceAir::GraphInstance* GetGraphInstance() const { return m_pGraphInstance; }
	ISubstancePreset* Clone() const override;
	void Destroy() override { delete this; };
	virtual ISubstancePresetSerializer* GetSerializer() override;
	virtual const std::vector<string> GetInputImages() const override;
	//////////////////////////////////////////////////////////////////////////
	static CSubstancePreset* Load(const string& filePath);
	static CSubstancePreset* Instantiate(const string& archiveName, const string& graphName);

	CSubstancePreset(const string& fileName,const string& archive, const string& graphName, const std::vector<SSubstanceOutput>& outputs, const Vec2i& resolution);
	~CSubstancePreset();
protected:
	struct SSerializer : public ISubstancePresetSerializer
	{
	public:

		struct SInputCategory
		{
			string name;
			string display_name;
			CSubstancePreset* preset;
			std::vector<SubstanceAir::InputInstanceBase*> inputs;
			SInputCategory(CSubstancePreset* preset, string name, string display_name);
			virtual void Serialize(Serialization::IArchive& ar);
		};
		typedef std::map<string, SInputCategory> CategoryMap;

		CSubstancePreset* preset;
		CategoryMap categories;
		SSerializer(CSubstancePreset* preset);

		void Serialize(Serialization::IArchive& ar) override;
	};

	CSubstancePreset();
	void LoadGraphInstance();
	void FillFormatForRender(const SSubstanceRenderData& renderData, const SubstanceAir::Vec2Int& baseResolution, SubstanceAir::OutputFormat& newFormat);
	void SetSubstanceArchive(const string& archive) { m_substanceArchive = archive; }
	void SetGraphName(const string& graph) { m_graphName = graph; }

private:
	string m_substanceArchive;
	string m_graphName;
	string m_fileName;
	SubstanceAir::GraphInstance* m_pGraphInstance;
	SubstanceAir::Vec2Int m_resolutionBackup;
	SubstanceAir::UInt m_resolutionId;
	std::vector<SSubstanceOutput> m_tempOriginalOutputs;
	std::vector<SSubstanceOutput> m_outputs;
	std::map<string /*name*/, SubstanceAir::VirtualOutputInstance*> m_VirtualOutputsCache;
	std::map<string, SubstanceAir::UInt> m_originalOutputsIds;
	std::map<SubstanceAir::UInt /*input ID*/, string/*texture path*/> m_usedImages;
	std::unique_ptr<SSerializer> m_serializer;

	bool m_uniformResolution;
};



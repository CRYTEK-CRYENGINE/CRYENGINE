// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SubstancePreset.h"
#include "SubstanceManager.h"
#include "SubstanceCommon.h"

#include <substance/framework/framework.h>
#include <substance/framework/input.h>

#include "CrySerialization/Serializer.h"
#include <CrySerialization/SmartPtr.h>
#include <CrySerialization/IArchiveHost.h> 
#include <CrySerialization/Math.h>
#include <CrySerialization/Color.h>
#include <CrySerialization/StringList.h>
#include <CrySerialization/Decorators/Resources.h>
#include <CrySerialization/Decorators/ActionButton.h>

#include "CryMath/Cry_Math.h"
#include "CryMath/Random.h"
#include <algorithm>

#include <CrySerialization/IArchive.h>
#include <functional>

CRndGen gRandomGenerator;

namespace Serialization
{

	typedef std::function<void()> StdFunctionActionButtonCalback;

	struct StdFunctionActionButton : public IActionButton
	{
		StdFunctionActionButtonCalback callback;
		string                         icon;

		explicit StdFunctionActionButton(const StdFunctionActionButtonCalback& callback, const char* icon = "")
			: callback(callback)
			, icon(icon)
		{
		}

		// IActionButton

		virtual void Callback() const override
		{
			if (callback)
			{
				callback();
			}
		}

		virtual const char* Icon() const override
		{
			return icon.c_str();
		}

		virtual IActionButtonPtr Clone() const override
		{
			return IActionButtonPtr(new StdFunctionActionButton(callback, icon.c_str()));
		}

		// ~IActionButton
	};

	inline bool Serialize(Serialization::IArchive& ar, StdFunctionActionButton& button, const char* name, const char* label)
	{
		if (ar.isEdit())
			return ar(Serialization::SStruct::forEdit(static_cast<Serialization::IActionButton&>(button)), name, label);
		else
			return false;
	}

	inline StdFunctionActionButton LambdaActionButton(const StdFunctionActionButtonCalback& callback, const char* icon = "")
	{
		return StdFunctionActionButton(callback, icon);
	}

}

CSubstancePreset::SSerializer::SInputCategory::SInputCategory(CSubstancePreset* preset, string name, string display_name)
	: name(name), display_name(display_name), preset(preset)
{}


bool ShouldSkipInput(SubstanceAir::string name)
{
	return name == "$pixelsize" || name == "$time" || name == "$normalformat";
}

void CSubstancePreset::SSerializer::SInputCategory::Serialize(Serialization::IArchive& ar)
{
	for each (SubstanceAir::InputInstanceBase* var in inputs)
	{
		const SubstanceAir::InputDescBase& desc = var->mDesc;
		if (ShouldSkipInput(desc.mIdentifier))
			continue;

		// handle special cases, yay!
		if (desc.mIdentifier == "$randomseed")
		{
			SubstanceAir::InputInstanceInt* inst = static_cast<SubstanceAir::InputInstanceInt*>(var);
			int value = inst->getValue();
			if (ar.openBlock("randomseed", "<Random Seed"))
			{
				
				ar(value, "randomSeed", "^!");
				ar(Serialization::LambdaActionButton([inst, &ar]() {
					inst->setValue(gRandomGenerator.GetRandom(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
				}), "randomize", "^Randomize");

				ar.closeBlock();
			}

		}
		else if (desc.mIdentifier == "$outputsize")
		{

			SubstanceAir::InputInstanceInt2* inst = static_cast<SubstanceAir::InputInstanceInt2*>(var);
			const SubstanceAir::InputDescInt2& tDesc = inst->getDesc();

	
			if (ar.isEdit())
			{
				SubstanceAir::Vec2Int sValue = preset->m_resolutionBackup;
				Serialization::StringList stringList;
				for (int i = 2; i < 14; i++)
				{
					stringList.push_back(std::to_string(int(pow(2, i))).c_str());
				}
				Serialization::StringList stringListY(stringList);
				Serialization::StringListValue stringListValue(stringList, std::to_string(int(pow(2, sValue.x))).c_str());
				Serialization::StringListValue stringListValueY(stringListY, std::to_string(int(pow(2, sValue.y))).c_str());
				if (ar.openBlock("outputsize", "<Output Resolution"))
				{
					ar(stringListValue, "outputSizeX", "^^");
					ar(preset->m_uniformResolution, "uniform", "^^>");
					if (!preset->m_uniformResolution)
					{
						ar(stringListValueY, "outputSizeY", "^^");
					}

					ar.closeBlock();
				}

				if (ar.isInput())
				{
					SubstanceAir::string strValue(stringListValue.c_str());
					int val = (int)log2(std::stoi(strValue.c_str()));
					SubstanceAir::string strValueY(stringListValueY.c_str());
					int valY = (int)log2(std::stoi(strValueY.c_str()));
					if (!preset->m_uniformResolution)
					{
						preset->m_resolutionBackup = SubstanceAir::Vec2Int(val, valY);
					}
					else {
						preset->m_resolutionBackup = SubstanceAir::Vec2Int(val, val);
					}

			
				}
			}
			else {
				ar(preset->m_resolutionBackup.x, "outputSizeX");
				ar(preset->m_uniformResolution, "uniform");
				if (!preset->m_uniformResolution)
				{
					ar(preset->m_resolutionBackup.y, "outputSizeY");
				}
				else {
					preset->m_resolutionBackup.y = preset->m_resolutionBackup.x;
				}

				if (ar.isInput())
				{
					inst->setValue(preset->m_resolutionBackup);
				}
			
			}


		}
		else {
			switch (desc.mType)
			{
			case Substance_IType_Image:
			{
				SubstanceAir::InputInstanceImage* inst = static_cast<SubstanceAir::InputInstanceImage*>(var);
				const SubstanceAir::InputDescImage& tDesc = inst->getDesc();
				string& fileName = preset->m_usedImages[tDesc.mUid];
				ar(Serialization::TextureFilename(fileName), desc.mIdentifier.c_str(), desc.mLabel.c_str());
			}
			break;
			case Substance_IType_Float:
			{
				SubstanceAir::InputInstanceFloat* inst = static_cast<SubstanceAir::InputInstanceFloat*>(var);
				const SubstanceAir::InputDescFloat& tDesc = inst->getDesc();
				float value = inst->getValue();
				if (tDesc.mGuiWidget == SubstanceAir::Input_Color)
				{
					ar(Serialization::Range(value, 0.f, 1.f), desc.mIdentifier.c_str(), desc.mLabel.c_str());
				}
				else {
					ar(Serialization::Range(value, tDesc.mMinValue, tDesc.mMaxValue), desc.mIdentifier.c_str(), desc.mLabel.c_str());

				}
				if (ar.isInput())
				{
					inst->setValue(value);
				}				
			}
				
				break;
			case Substance_IType_Float2:
			{
				SubstanceAir::InputInstanceFloat2* inst = static_cast<SubstanceAir::InputInstanceFloat2*>(var);
				SubstanceAir::Vec2Float sValue = inst->getValue();
				Vec2 value(sValue.x, sValue.y);
				ar(value, desc.mIdentifier.c_str(), desc.mLabel.c_str());
				if (ar.isInput())
				{
					inst->setValue(SubstanceAir::Vec2Float(value.x, value.y));
				}
			}	
				break;
			case Substance_IType_Float3:
			{
				SubstanceAir::InputInstanceFloat3* inst = static_cast<SubstanceAir::InputInstanceFloat3*>(var);
				SubstanceAir::Vec3Float sValue = inst->getValue();
				const SubstanceAir::InputDescFloat3& tDesc = inst->getDesc();
				if (tDesc.mGuiWidget == SubstanceAir::Input_Color)
				{
					ColorF color = ColorF(sValue.x, sValue.y, sValue.z);
					ar(color, desc.mIdentifier.c_str(), desc.mLabel.c_str());
					if (ar.isInput())
					{
						inst->setValue(SubstanceAir::Vec3Float(color.r, color.g, color.b));
					}
				}
				else {
					Vec3 value(sValue.x, sValue.y, sValue.z);
					ar(value, desc.mIdentifier.c_str(), desc.mLabel.c_str());
					if (ar.isInput())
					{
						inst->setValue(SubstanceAir::Vec3Float(value.x, value.y, value.z));
					}
				}
				
			}
			break;
			case Substance_IType_Float4:
			{
				SubstanceAir::InputInstanceFloat4* inst = static_cast<SubstanceAir::InputInstanceFloat4*>(var);
				SubstanceAir::Vec4Float sValue = inst->getValue();
				const SubstanceAir::InputDescFloat4& tDesc = inst->getDesc();			
				if (tDesc.mGuiWidget == SubstanceAir::Input_Color)
				{
					ColorF color = ColorF(sValue.x, sValue.y, sValue.z, sValue.w);
					ar(color, desc.mIdentifier.c_str(), desc.mLabel.c_str());
					if (ar.isInput())
					{
						inst->setValue(SubstanceAir::Vec4Float(color.r, color.g, color.b, color.a));
					}
				}
				else {
					Vec4 value(sValue.x, sValue.y, sValue.z, sValue.w);
					ar(value, desc.mIdentifier.c_str(), desc.mLabel.c_str());
					if (ar.isInput())
					{
						inst->setValue(SubstanceAir::Vec4Float(value.x, value.y, value.z, value.w));
					}
				}
				
			}
				break;
			case Substance_IType_Integer:
			{
				SubstanceAir::InputInstanceInt* inst = static_cast<SubstanceAir::InputInstanceInt*>(var);
				const SubstanceAir::InputDescInt& tDesc = inst->getDesc();
				int value = inst->getValue();

				if (tDesc.mGuiWidget == SubstanceAir::Input_Combobox)
				{
					Serialization::StringList stringList;
					SubstanceAir::string current = "";
					for each (auto var in tDesc.mEnumValues)
					{
						if (var.first == value)
							current = var.second;
						stringList.push_back(var.second.c_str());
					}
					Serialization::StringListValue stringListValue(stringList, current.c_str());
					ar(stringListValue, desc.mIdentifier.c_str(), desc.mLabel.c_str());
					
					if (ar.isInput())
					{
						SubstanceAir::string sValue(stringListValue.c_str());;
						auto it = std::find_if(tDesc.mEnumValues.begin(), tDesc.mEnumValues.end(),
							[&sValue](const std::pair<int, SubstanceAir::string>& p)
						{ return p.second == sValue; });
						if (it != tDesc.mEnumValues.end())
						{
							inst->setValue(it->first);
						}
						
					}
				}
				else {
					ar(Serialization::Range(value, tDesc.mMinValue, tDesc.mMaxValue), desc.mIdentifier.c_str(), desc.mLabel.c_str());
					if (ar.isInput())
					{
						inst->setValue(value);
					}
				}
			}

			break;
			case Substance_IType_Integer2:
			{
				SubstanceAir::InputInstanceInt2* inst = static_cast<SubstanceAir::InputInstanceInt2*>(var);
				SubstanceAir::Vec2Int sValue = inst->getValue();
				Vec2i value(sValue.x, sValue.y);
				ar(value, desc.mIdentifier.c_str(), desc.mLabel.c_str());
				if (ar.isInput())
				{
					inst->setValue(SubstanceAir::Vec2Int(value.x, value.y));
				}
			}
			break;
			case Substance_IType_Integer3:
			{
				SubstanceAir::InputInstanceInt3* inst = static_cast<SubstanceAir::InputInstanceInt3*>(var);
				SubstanceAir::Vec3Int sValue = inst->getValue();
				Vec3i value(sValue.x, sValue.y, sValue.z);
				ar(value, desc.mIdentifier.c_str(), desc.mLabel.c_str());
				if (ar.isInput())
				{
					inst->setValue(SubstanceAir::Vec3Int(value.x, value.y, value.z));
				}
			}
				break;
			case Substance_IType_Integer4:
			{
				SubstanceAir::InputInstanceInt4* inst = static_cast<SubstanceAir::InputInstanceInt4*>(var);
				SubstanceAir::Vec4Int sValue = inst->getValue();
				Vec4i value(sValue.x, sValue.y, sValue.z, sValue.w);
				ar(value, desc.mIdentifier.c_str(), desc.mLabel.c_str());
				if (ar.isInput())
				{
					inst->setValue(SubstanceAir::Vec4Int(value.x, value.y, value.z, value.w));
				}
			}
				break;
			default:
				break;
			}
		}
		
	}

}

CSubstancePreset::SSerializer::SSerializer(CSubstancePreset* preset)
	: preset(preset)
{


}


void CSubstancePreset::SSerializer::Serialize(Serialization::IArchive& ar)
{
	ar(preset->m_graphName, "graph");
	ar(preset->m_substanceArchive, "archive");
	if (!preset->m_pGraphInstance)
	{
		preset->LoadGraphInstance();
	}

	if (categories.empty())
	{
		SubstanceAir::GraphInstance::Inputs instanceInputs = preset->m_pGraphInstance->getInputs();
		SSerializer::SInputCategory root(preset, "", "");
		categories.emplace("", std::move(root));
		for each (SubstanceAir::InputInstanceBase* var in instanceInputs)
		{
			if (ShouldSkipInput(var->mDesc.mIdentifier))
			{
				continue;
			}

			string group = var->mDesc.mGuiGroup.c_str();
			CategoryMap::iterator it = categories.find(group);

			if (it != categories.end())
			{
				it->second.inputs.push_back(var);
			}
			else {
				SInputCategory targetCategory(preset, group, group);
				targetCategory.inputs.push_back(var);
				categories.emplace(group, std::move(targetCategory));

			}
		}
	}

	for (auto &category : categories) {
		if (!category.second.name.IsEmpty())
		{
			ar.openBlock(category.second.name, category.second.display_name);
		}
		SInputCategory& cat = category.second;

		cat.Serialize(ar);

		if (!category.second.name.IsEmpty())
		{
			ar.closeBlock();
		}
	}
	if (!ar.isEdit())
	{
		ar(preset->m_outputs, "outputs");
	}
	
}

void CSubstancePreset::SetGraphResolution(const int& x, const int& y)
{
	SubstanceAir::InputInstanceInt2* pInput = static_cast<SubstanceAir::InputInstanceInt2*>(m_pGraphInstance->findInput(m_resolutionId));
	if (pInput)
	{
		pInput->setValue(SubstanceAir::Vec2Int(x, y));
	}
	m_uniformResolution = x == y;
}

SubstanceAir::Vec2Int CSubstancePreset::GetGraphResolution() const
{
	return m_resolutionBackup;
}

const SubstanceAir::UInt& CSubstancePreset::GetInstanceID() const
{
	return m_pGraphInstance->mInstanceUid;
}

SubstanceAir::GraphInstance& CSubstancePreset::PrepareRenderInstance(ISubstanceInstanceRenderer* renderer)
{
	SubstanceAir::Vec2Int baseResolution = m_resolutionBackup;
	SubstanceAir::InputInstanceInt2* pInput = static_cast<SubstanceAir::InputInstanceInt2*>(m_pGraphInstance->findInput(m_resolutionId));
	if (pInput)
	{
		baseResolution = pInput->getValue();
	}

	for (auto pair : m_usedImages)
	{
		SubstanceAir::InputInstanceImage* input = static_cast<SubstanceAir::InputInstanceImage*>(m_pGraphInstance->findInput(pair.first));
		input->setImage(renderer->GetInputImage(this, pair.second));
	}

	for (auto output : m_pGraphInstance->getOutputs())
	{
		output->mEnabled = false;
	}

	for (auto output : m_pGraphInstance->getVirtualOutputs())
	{
		output->mEnabled = false;
	}
	if (renderer->SupportsOriginalOutputs())
	{
		for (SSubstanceOutput& existingOutput : m_tempOriginalOutputs)
		{
			std::vector<SSubstanceRenderData> outputsRenderData;
			renderer->FillOriginalOutputRenderData(this, existingOutput, outputsRenderData);

			for (SSubstanceRenderData& renderData : outputsRenderData)
			{
				if (!renderData.output.enabled)
				{
					continue;
				}
				auto it = std::find_if(m_pGraphInstance->getOutputs().begin(), m_pGraphInstance->getOutputs().end(), [=](SubstanceAir::OutputInstance* s) { return string(s->mDesc.mIdentifier.c_str()).compareNoCase(renderData.name) == 0; });
				if (it != m_pGraphInstance->getOutputs().end())
				{
					SubstanceAir::OutputInstance* currentOutput = *it;
					currentOutput->mEnabled = renderData.output.enabled;
			
					SubstanceAir::OutputFormat newFormat;
					FillFormatForRender(renderData, baseResolution, newFormat);
					currentOutput->overrideFormat(newFormat);
					currentOutput->mUserData = renderData.customData;
				}
			}
		}
	}
	
	for (SSubstanceOutput& existingOutput : m_outputs)
	{
		std::vector<SSubstanceRenderData> outputsRenderData;
		renderer->FillVirtualOutputRenderData(this, existingOutput, outputsRenderData);

		for (SSubstanceRenderData& renderData : outputsRenderData)
		{
			if (!renderData.output.enabled || renderData.output.sourceOutputs.size() == 0)
			{
				continue;
			}
			
			if (!m_VirtualOutputsCache.count(renderData.name))
			{
				m_VirtualOutputsCache[renderData.name] = m_pGraphInstance->addVirtualOutput(renderData.name.c_str());
			}

			SubstanceAir::VirtualOutputInstance* newVirtual = m_VirtualOutputsCache[renderData.name];
			newVirtual->mEnabled = renderData.output.enabled;
			SubstanceAir::OutputFormat newFormat;
			FillFormatForRender(renderData, baseResolution, newFormat);
			newVirtual->setFormat(newFormat);
			newVirtual->mUserData = renderData.customData;
		}
	}

	return *m_pGraphInstance;
}

void CSubstancePreset::RemoveOutput(SSubstanceOutput* output)
{


}



void CSubstancePreset::Reset()
{
	for (auto input: m_pGraphInstance->getInputs())
	{
		input->reset();
	}
}

void CSubstancePreset::Reload()
{
	SSerializer ar(this);
	ISubstancePresetSerializer* iar = static_cast<ISubstancePresetSerializer*>(&ar);
	if (!SubstanceSerialization::Load(*iar, m_fileName))
	{
		return;
	}
	if (!m_pGraphInstance)
	{
		return;
	}

	SubstanceAir::InputInstanceInt2* pInput = static_cast<SubstanceAir::InputInstanceInt2*>(m_pGraphInstance->findInput(m_resolutionId));
	if (pInput)
	{
		m_resolutionBackup = pInput->getValue();
	}

	std::vector<string> origOutputNames;
	origOutputNames.resize(m_tempOriginalOutputs.size());

	std::transform(m_tempOriginalOutputs.begin(), m_tempOriginalOutputs.end(), origOutputNames.begin(), [](const SSubstanceOutput& o) { return o.name; });
	for (SSubstanceOutput& output : m_outputs)
	{
		output.UpdateState(origOutputNames);
	}
}


CSubstancePreset::CSubstancePreset(const string& fileName, const string& archive, const string& graphName, const std::vector<SSubstanceOutput>& outputs, const Vec2i& resolution)
	: m_graphName(graphName)
	, m_fileName(fileName)
	, m_substanceArchive(archive)
	, m_outputs(outputs)
	, m_resolutionId(-1)
{
	LoadGraphInstance();
	SubstanceAir::Vec2Int res(std::min(resolution.x, 12), std::min(resolution.y, 12));
	SetGraphResolution(res.x, res.y);
	m_resolutionBackup = res;
}

CSubstancePreset::CSubstancePreset()
	: m_pGraphInstance(0)
	, m_resolutionBackup(0, 0)
	, m_resolutionId(-1)
	, m_uniformResolution(true)
{
}

CSubstancePreset::~CSubstancePreset()
{
	delete m_pGraphInstance;
	CSubstanceManager::Instance()->RemovePresetInstance(this);
}

void CSubstancePreset::LoadGraphInstance()
{
	m_pGraphInstance = CSubstanceManager::Instance()->InstantiateGraph(m_substanceArchive, m_graphName);
	if (!m_pGraphInstance)
	{
		return;
	}

	for each (SubstanceAir::InputInstanceBase* inputInstance in m_pGraphInstance->getInputs())
	{
		if (inputInstance->mDesc.mIdentifier == "$outputsize")
		{
			m_resolutionId = inputInstance->mDesc.mUid;

		}
		else if (inputInstance->mDesc.mType == Substance_IType_Image)
		{			
			SubstanceAir::InputInstanceImage* inst = static_cast<SubstanceAir::InputInstanceImage*>(inputInstance);
			const SubstanceAir::InputDescImage& tDesc = inst->getDesc();
			m_usedImages[inputInstance->mDesc.mUid] = string();
		}
	}

	for each (SubstanceAir::OutputInstance* outputInstance in m_pGraphInstance->getOutputs())
	{
		string nameLow(outputInstance->mDesc.mIdentifier.c_str());
		nameLow.MakeLower();
		m_originalOutputsIds[nameLow] = outputInstance->mDesc.mUid;
		SSubstanceOutput tempOutput;
		tempOutput.enabled = false;
		tempOutput.name = outputInstance->mDesc.mIdentifier.c_str();
		m_tempOriginalOutputs.push_back(tempOutput);
	}

}


void CSubstancePreset::Save()
{
	SSerializer ar(this);
	SubstanceSerialization::Save(m_fileName, *static_cast<ISubstancePresetSerializer*>(&ar));
}


void CSubstancePreset::FillFormatForRender(const SSubstanceRenderData& renderData, const SubstanceAir::Vec2Int& baseResolution, SubstanceAir::OutputFormat& newFormat)
{

	newFormat.mipmapLevelsCount = renderData.useMips ? SubstanceAir::OutputFormat::MipmapFullPyramid : SubstanceAir::OutputFormat::MipmapNone;	
	newFormat.forceWidth = renderData.output.resolution == 0 ? SubstanceAir::OutputFormat::UseDefault : (int)pow(2, baseResolution.x - renderData.output.resolution);
	newFormat.forceHeight = renderData.output.resolution == 0 ? SubstanceAir::OutputFormat::UseDefault : (int)pow(2, baseResolution.y - renderData.output.resolution);
	newFormat.format = renderData.format;
	// we don't need to create channel mapping sources are empty
	if (renderData.output.sourceOutputs.size() == 0)
	{ 
		return;
	}

	for (size_t i = 0; i <= (renderData.skipAlpha ? 2 : 3); i++)
	{
		SubstanceAir::UInt outputIndex = 0;
		SubstanceAir::UInt channel = renderData.output.channels[i].channel == -1 ? i : max((int)renderData.output.channels[i].channel, 0);
		int sourceId = renderData.output.channels[i].sourceId;
		if (sourceId > -1)
		{
			string sourceOut(renderData.output.sourceOutputs[sourceId]);
			sourceOut.MakeLower();
			if (sourceId < renderData.output.sourceOutputs.size() && m_originalOutputsIds.count(sourceOut) == 1)
			{
				outputIndex = m_originalOutputsIds[sourceOut];
			}
		}

		int target = i;
		if (renderData.swapRG && i == 0)
			target = 1;
		else if (renderData.swapRG && i == 1)
			target = 0;

		newFormat.perComponent[target].outputUid = outputIndex;
		newFormat.perComponent[target].shuffleIndex = channel;
	}

	if (renderData.skipAlpha)
	{
		newFormat.perComponent[3].outputUid = 0; // making sure alpha is not used
	}
}


const std::vector<string> CSubstancePreset::GetInputImages() const
{
	std::vector<string> result;
	for (auto& img: m_usedImages)
	{
		result.push_back(img.second);
	}
	return result;
}

ISubstancePresetSerializer* CSubstancePreset::GetSerializer()
{
	if (!m_serializer)
	{
		m_serializer = std::make_unique<SSerializer>(this);
	}
	return m_serializer.get();
}

CSubstancePreset* CSubstancePreset::Load(const string& filePath)
{
	CSubstancePreset* preset = new CSubstancePreset();
	preset->m_fileName = filePath;
	preset->Reload();
	return preset;
}

CSubstancePreset* CSubstancePreset::Instantiate(const string& archiveName, const string& graphName)
{
	CSubstancePreset* preset = new CSubstancePreset();
	preset->m_substanceArchive = archiveName;
	preset->m_graphName = graphName;
	preset->LoadGraphInstance();
	return preset;
}

const string CSubstancePreset::GetFileName() const
{
	return m_fileName;
}

ISubstancePreset* CSubstancePreset::Clone() const
{
	CSubstancePreset* target = new CSubstancePreset();
	target->m_substanceArchive = m_substanceArchive;
	target->m_graphName = m_graphName;
	target->LoadGraphInstance();
	target->m_outputs.assign(m_outputs.begin(), m_outputs.end());
	target->m_resolutionBackup = m_resolutionBackup;
	target->m_usedImages = m_usedImages;
	SubstanceAir::Preset sPreset;
	sPreset.fill(*m_pGraphInstance);
	sPreset.apply(*target->m_pGraphInstance);
	return target;
}

const string CSubstancePreset::GetOutputPath(const SSubstanceOutput* output) const
{
	string suffix = output->name;
	string finalName = PathUtil::GetPathWithoutFilename(m_fileName);
	finalName += PathUtil::GetFileName(m_fileName) + "_" + suffix;
	return finalName;
}

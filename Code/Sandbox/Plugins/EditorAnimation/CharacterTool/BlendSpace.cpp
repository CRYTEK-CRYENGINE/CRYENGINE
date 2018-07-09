// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <algorithm>
#include <type_traits>

#include "BlendSpace.h"
#include <CryAnimation/ICryAnimation.h>
#include <CrySerialization/StringList.h>


namespace Serialization
{
	template<typename T, typename TContainer>
	struct CListSelectorDecorator
	{
		CListSelectorDecorator(T& value, const TContainer& items, const StringList& labels, const T fallback)
			: m_value(value)
			, m_items(items)
			, m_labels(labels)
			, m_fallback(fallback)
		{
		}

		T& m_value;
		const TContainer& m_items;
		const StringList& m_labels;
		const T m_fallback;
	};

	template<typename T, typename TContainer>
	struct CAutoLabeledListSelectorDecorator : public CListSelectorDecorator<T, TContainer>
	{
		CAutoLabeledListSelectorDecorator(T& value, const TContainer& items, const T fallback)
			: CListSelectorDecorator(value, items, m_autoLabels, std::move(fallback))
			, m_autoLabels()
		{
			m_autoLabels.reserve(std::distance(std::begin(items), std::end(items)));
			std::transform(std::begin(items), std::end(items), std::back_inserter(m_autoLabels), [](const T& x) { return string(std::to_string(x).c_str()); });
		}

		StringList m_autoLabels;
	};

	template<typename T, typename TContainer>
	inline bool Serialize(Serialization::IArchive& ar, CListSelectorDecorator<T, TContainer>& value, const char* name, const char* label)
	{
		if (ar.isEdit())
		{
			if (ar.isInput())
			{
				StringListValue selectedLabel(value.m_labels, StringList::npos);
				if (!ar(selectedLabel, "", label))
				{
					return false;
				}

				if (selectedLabel.index() >= 0)
				{
					value.m_value = *std::next(std::begin(value.m_items), selectedLabel.index());
				}
				else
				{
					value.m_value = value.m_fallback;
				}

				return true;
			}
			else
			{
				CRY_ASSERT(ar.isOutput());

				Serialization::StringListValue selectedLabel;

				const auto itSearch = std::find_if(std::begin(value.m_items), std::end(value.m_items), [&value](const auto& x) { return x == value.m_value; });
				if (itSearch != std::end(value.m_items))
				{
					selectedLabel = Serialization::StringListValue(value.m_labels, std::distance(std::begin(value.m_items), itSearch));
				}
				else
				{
					selectedLabel = Serialization::StringListValue(value.m_labels, StringList::npos);
				}

				return ar(selectedLabel, "", label);
			}
		}
		else
		{
			return ar(value.m_value, name, label);
		}
	}

	//! Decorates given serializable value with a drop-down list selector.
	//! \param value Reference to the serialized value.
	//! \param items Forward-iterable container of items to choose from. Must provide values of the same type as "value".
	//! \param labels StringList containing user-readable labels to represent entires in the GUI. Sizes of "labels" and "items" must match.
	//! \param fallback Fallback value to be used when chosen entry cannot be found in the items container.
	template<typename T, typename TContainer>
	inline auto ListSelector(T& value, const TContainer& items, const StringList& labels, const T fallback) -> CListSelectorDecorator<T, TContainer>
	{
		using TContainerValue = std::remove_reference_t<decltype(*std::begin(std::declval<TContainer&>()))>;
		static_assert(std::is_same<TContainerValue, T>::value, "TContainer is expected to hold values of type T");

		CRY_ASSERT(labels.size() == std::distance(std::begin(items), std::end(items)));

		return CListSelectorDecorator<T, TContainer>(value, items, labels, std::move(fallback));
	}

	//! Decorates given serializable value with a drop-down list selector. This overloads generates item labels automatically by invoking std::to_string on the list of values.
	//! \param value Reference to the serialized value.
	//! \param items Forward-iterable container of items to choose from. Must provide values of the same type as "value".
	//! \param fallback Fallback value to be used when chosen entry cannot be found in the items container.
	template<typename T, typename TContainer>
	inline auto ListSelector(T& value, const TContainer& items, const T fallback) -> CAutoLabeledListSelectorDecorator<T, TContainer>
	{
		using TContainerValue = std::remove_reference_t<decltype(*std::begin(std::declval<TContainer&>()))>;
		static_assert(std::is_same<TContainerValue, T>::value, "TContainer is expected to hold values of type T");

		return CAutoLabeledListSelectorDecorator<T, TContainer>(value, items, std::move(fallback));
	}
}

namespace CharacterTool
{

struct SMotionParametersPool
{
	SMotionParametersPool()
	{
		allParams.reserve(eMotionParamID_COUNT);
		allLabels.reserve(eMotionParamID_COUNT);

		extractionFlaggedParams.reserve(eMotionParamID_COUNT);
		extractionFlaggedLabels.reserve(eMotionParamID_COUNT);

		for (int32 i = 0; i < eMotionParamID_COUNT; ++i)
		{
			SMotionParameterDetails details;
			gEnv->pCharacterManager->GetMotionParameterDetails(details, EMotionParamID(i));

			allParams.push_back(i);
			allLabels.push_back(details.humanReadableName);

			if (details.flags & SMotionParameterDetails::ADDITIONAL_EXTRACTION)
			{
				extractionFlaggedParams.push_back(i);
				extractionFlaggedLabels.push_back(details.humanReadableName);
			}
		}
	}

	struct SSelector
	{
		SSelector(int32& value, bool onlyParamsWithExtractionFlag = false)
			: value(value)
			, onlyParamsWithExtractionFlag(onlyParamsWithExtractionFlag)
		{
		}

		int32& value;
		bool onlyParamsWithExtractionFlag;
	};

	std::vector<int32> allParams;
	Serialization::StringList allLabels;

	std::vector<int32> extractionFlaggedParams;
	Serialization::StringList extractionFlaggedLabels;
};

static bool Serialize(Serialization::IArchive& ar, SMotionParametersPool::SSelector& self, const char* name, const char* label)
{
	CRY_ASSERT(ar.context<SMotionParametersPool>());
	auto& pool = *ar.context<SMotionParametersPool>();

	const bool result = self.onlyParamsWithExtractionFlag
		? ar(Serialization::ListSelector(self.value, pool.extractionFlaggedParams, pool.extractionFlaggedLabels, int32(eMotionParamID_INVALID)), name, label)
		: ar(Serialization::ListSelector(self.value, pool.allParams, pool.allLabels, int32(eMotionParamID_INVALID)), name, label);

	// Remove selected parameter from the pool, to make sure subsequent entries do not duplicate it.
	const auto itParams = std::find(pool.allParams.begin(), pool.allParams.end(), self.value);
	if (itParams != pool.allParams.end())
	{
		pool.allLabels.erase(std::next(pool.allLabels.begin(), std::distance(pool.allParams.begin(), itParams)));
		pool.allParams.erase(itParams);
	}

	const auto itExtractionFlaggedParams = std::find(pool.extractionFlaggedParams.begin(), pool.extractionFlaggedParams.end(), self.value);
	if (itExtractionFlaggedParams != pool.extractionFlaggedParams.end())
	{
		pool.extractionFlaggedLabels.erase(std::next(pool.extractionFlaggedLabels.begin(), std::distance(pool.extractionFlaggedParams.begin(), itExtractionFlaggedParams)));
		pool.extractionFlaggedParams.erase(itExtractionFlaggedParams);
	}

	return result;
}

static const char* ParameterNameById(EMotionParamID id, bool additionalExtraction)
{
	SMotionParameterDetails details;
	gEnv->pCharacterManager->GetMotionParameterDetails(details, id);

	if (!details.name)
	{
		return "";
	}

	if (additionalExtraction && !(details.flags & SMotionParameterDetails::ADDITIONAL_EXTRACTION))
	{
		return "";
	}

	return details.name;
}

static EMotionParamID ParameterIdByName(const char* name, bool additionalExtraction)
{
	for (int32 i = 0; i < eMotionParamID_COUNT; ++i)
	{
		SMotionParameterDetails details;
		gEnv->pCharacterManager->GetMotionParameterDetails(details, EMotionParamID(i));

		if (additionalExtraction && !(details.flags & SMotionParameterDetails::ADDITIONAL_EXTRACTION))
		{
			continue;
		}

		if (strcmp(details.name, name) == 0)
		{
			return EMotionParamID(i);
		}
	}
	return eMotionParamID_INVALID;
}

void BlendSpaceAnnotation::Serialize(IArchive& ar)
{
	static const size_t allowedSizes[] = { 2, 3, 4, 5, 6, 7, 8 };

	std::vector<CryGUID> exampleList;
	Serialization::StringList exampleLabels;
	if (ar.isEdit())
	{
		CRY_ASSERT(ar.context<BlendSpace>());
		const auto& blendSpace = *ar.context<BlendSpace>();

		size_t entryIndex = 0;
		size_t pseudoIndex = 0;

		for (const auto& example : blendSpace.m_examples)
		{
			const auto& label = std::to_string(entryIndex++) + " (" + example.animation.c_str() + ")";
			exampleList.push_back(example.runtimeGuid);
			exampleLabels.push_back(label.c_str());
		}
		for (const auto& example : blendSpace.m_pseudoExamples)
		{
			const auto& label = std::to_string(entryIndex++) + " (Pseudo Example " + std::to_string(pseudoIndex++) + ")";
			exampleList.push_back(example.runtimeGuid);
			exampleLabels.push_back(label.c_str());
		}
	}

	size_t exampleCount = exampleGuids.size();
	ar(Serialization::ListSelector(exampleCount, allowedSizes, allowedSizes[0]), "size", "^>");
	exampleGuids.resize(exampleCount);

	for (auto& guid : exampleGuids)
	{
		ar(Serialization::ListSelector(guid, exampleList, exampleLabels, CryGUID::Null()), "", "<");

		// Remove selected item from the list, to make sure subsequent entries don't duplicate it.
		const auto itSelected = std::find(exampleList.begin(), exampleList.end(), guid);
		if (itSelected != exampleList.end())
		{
			exampleLabels.erase(std::next(exampleLabels.begin(), std::distance(exampleList.begin(), itSelected)));
			exampleList.erase(itSelected);
		}
	}
}

void BlendSpaceExample::Serialize(IArchive& ar)
{
	ar(AnimationAlias(animation), "animation", "<^");
	if (ar.isEdit() && ar.isOutput())
	{
		if (animation.empty())
		{
			ar.warning(animation, "Example doesn't specify an animation name.");
		}
		else
		{
			if (ICharacterInstance* characterInstance = ar.context<ICharacterInstance>())
			{
				IAnimationSet* animationSet = characterInstance->GetIAnimationSet();
				int animationId = animationSet->GetAnimIDByName(animation.c_str());
				if (animationId < 0)
				{
					ar.error(animation, "Animation name is missing from the animation set.");
				}
				else
				{
					if (animationSet->IsAimPose(animationId, characterInstance->GetIDefaultSkeleton()) ||
					    animationSet->IsLookPose(animationId, characterInstance->GetIDefaultSkeleton()))
					{
						ar.error(animation, "BlendSpace example can not refer to Aim/LookPose");
					}
					else
					{
						int flags = animationSet->GetAnimationFlags(animationId);
						if (flags & CA_ASSET_LMG)
							ar.error(animation, "BlendSpace example can not refer to another BlendSpace.");
					}
				}
			}
		}
	}

	CRY_ASSERT(ar.context<BlendSpace>());
	const auto& dimensions = ar.context<BlendSpace>()->m_dimensions;

	for (int32 iParam = 0; iParam < eMotionParamID_COUNT; ++iParam)
	{
		if (ar.isEdit() && std::none_of(dimensions.begin(), dimensions.end(), [iParam](const auto& dimension) { return dimension.parameterId == iParam; }))
		{
			continue;
		}

		SMotionParameterDetails details;
		gEnv->pCharacterManager->GetMotionParameterDetails(details, EMotionParamID(iParam));

		if (ar.openBlock(details.name, details.humanReadableName))
		{
			ar(parameters[iParam].userDefined, "", "^");
			ar(parameters[iParam].value, "", parameters[iParam].userDefined ? "^" : "!^");
			ar.closeBlock();
		}
	}

	ar(playbackScale, "playbackScale", "Playback Scale");
}
// ---------------------------------------------------------------------------

void BlendSpaceAdditionalExtraction::Serialize(IArchive& ar)
{
	ar(SMotionParametersPool::SSelector(parameterId, true), "parameterId", "^");
}

bool Serialize(IArchive& ar, BlendSpaceReference& ref, const char* name, const char* label)
{
	return ar(AnimationOrBlendSpacePath(ref.path), name, label);
}

void BlendSpacePseudoExample::Serialize(IArchive& ar)
{
	std::vector<CryGUID> exampleList;
	Serialization::StringList exampleLabels;
	if (ar.isEdit())
	{
		CRY_ASSERT(ar.context<BlendSpace>());
		const auto& blendSpace = *ar.context<BlendSpace>();

		for (const auto& example : blendSpace.m_examples)
		{
			exampleList.push_back(example.runtimeGuid);
			exampleLabels.push_back(example.animation);
		}
	}

	ar(Serialization::ListSelector(guid0, exampleList, exampleLabels, CryGUID::Null()), "guid0", "^<");
	ar(weight0, "weight0", ">^");
	ar(Serialization::ListSelector(guid1, exampleList, exampleLabels, CryGUID::Null()), "guid1", "^<");
	ar(weight1, "weight1", ">^");
}

// ---------------------------------------------------------------------------{

void BlendSpaceDimension::Serialize(IArchive& ar)
{
	ar(SMotionParametersPool::SSelector(parameterId), "parameterId", "<^");
	ar(minimal, "min", "^>Min");
	ar(Serialization::Decorators::Range(maximal, minimal + 0.01f, std::numeric_limits<float>::max()), "max", "^>Max");
	ar(cellCount, "cellCount", "Cell Count");
	ar(locked, "locked", "Locked");
}

// ---------------------------------------------------------------------------

bool BlendSpace::LoadFromXml(string& errorMessage, XmlNodeRef root, IAnimationSet* pAnimationSet)
{
	bool result = true;
	//---------------------------------------------------------
	//--- parse and verify XML
	//---------------------------------------------------------
	const char* XMLTAG = root->getTag();
	if (strcmp(XMLTAG, "ParaGroup") != 0)
	{
		((errorMessage += "Error: The XMLTAG is '") += XMLTAG) += "'. It is expected to be 'ParaGroup'\n";
		result = false;
	}

	uint32 numPseudo = 0;
	uint32 numExamples = 0;
	uint32 numChilds = root->getChildCount();
	for (uint32 c = 0; c < numChilds; c++)
	{
		XmlNodeRef nodeList = root->getChild(c);
		const char* ListTag = nodeList->getTag();

		//load example-list
		if (strcmp(ListTag, "ExampleList") == 0)
		{
			numExamples = nodeList->getChildCount();
			if (numExamples == 0)
			{
				errorMessage += "Error: no examples in this ParaGroup.\n";
				result = false;
			}

			// TODO: MAX_LMG_ANIMS
			if (numExamples >= 40)
			{
				errorMessage += "Error: too many examples in one ParaGroup. Only 40 are currently allowed!\n";
				result = false;
			}
		}

		//load pseudo example-list
		if (strcmp(ListTag, "ExamplePseudo") == 0)
			numPseudo = nodeList->getChildCount();
	}

	//----------------------------------------------------------------------

	for (uint32 c = 0; c < numChilds; c++)
	{
		XmlNodeRef nodeList = root->getChild(c);
		const char* ListTag = nodeList->getTag();

		//----------------------------------------------------------------------------------
		//---   temporary helper flags to ensure compatibility with the old system       ---
		//---               they will disappear sooner or later                          ---
		//----------------------------------------------------------------------------------
		if (strcmp(ListTag, "THRESHOLD") == 0)
		{
			nodeList->getAttr("tz", m_threshold);    //will go as soon as we have the Blend Nodes to combine VEGs
			continue;
		}
		if (strcmp(ListTag, "VEGPARAMS") == 0)
		{
			uint32 nFlags = 0;
			nodeList->getAttr("Idle2Move", nFlags);                       //will go as soon as CryMannegin is up and running
			if (nFlags)
				m_idleToMove = true; //this is a case for CryMannequin
			continue;
		}

		//-----------------------------------------------------------
		//---  define dimensions of the LMG                       ---
		//-----------------------------------------------------------
		if (strcmp(ListTag, "Dimensions") == 0)
		{
			int numDimensions = nodeList->getChildCount();
			m_dimensions.resize(numDimensions);
			if (m_dimensions.size() > 3)
			{
				errorMessage += "Error: More then 3 dimensions per Blend-Space are not supported\n";
				result = false;
			}

			for (uint32 d = 0; d < m_dimensions.size(); d++)
			{
				XmlNodeRef nodeExample = nodeList->getChild(d);
				const char* ExampleTag = nodeExample->getTag();
				if (strcmp(ExampleTag, "Param") == 0)
				{
					//each dimension must have a parameter-name
					const char* parameterName = nodeExample->getAttr("name");
					m_dimensions[d].parameterId = ParameterIdByName(parameterName, false);

					//check if the parameter-name is supported by the system
					if (m_dimensions[d].parameterId == eMotionParamID_INVALID)
					{
						((errorMessage += "Error: The parameter '") += parameterName) += "' is currently not supported\n";
						result = false;
					}

					//define the scope of the blend-space for each dimension
					nodeExample->getAttr("min", m_dimensions[d].minimal);
					nodeExample->getAttr("max", m_dimensions[d].maximal);
					m_dimensions[d].maximal = (m_dimensions[d].maximal - m_dimensions[d].minimal < 0.01f) ? (m_dimensions[d].minimal + 0.01f) : (m_dimensions[d].maximal);

					nodeExample->getAttr("cells", m_dimensions[d].cellCount);
					m_dimensions[d].cellCount = m_dimensions[d].cellCount < 3 ? 3 : m_dimensions[d].cellCount;

					nodeExample->getAttr("scale", m_dimensions[d].debugVisualScale);   //just for visual-debugging
					m_dimensions[d].debugVisualScale = max(0.01f, m_dimensions[d].debugVisualScale);

					nodeExample->getAttr("skey", m_dimensions[d].startKey);
					nodeExample->getAttr("ekey", m_dimensions[d].endKey);

					//special flags per-dimension
					nodeExample->getAttr("locked", m_dimensions[d].locked);
				}
			}
			continue;
		}

		//-----------------------------------------------------------
		//---  define the additional extraction parameters        ---
		//-----------------------------------------------------------
		if (strcmp(ListTag, "AdditionalExtraction") == 0)
		{
			int numExtractionParams = nodeList->getChildCount();

			if (numExtractionParams > 4)
			{
				errorMessage += "Error: More then 4 additional extraction parameters are not supported\n";
				result = false;
			}
			m_additionalExtraction.resize(numExtractionParams);
			for (uint32 d = 0; d < numExtractionParams; d++)
			{
				XmlNodeRef nodeExample = nodeList->getChild(d);
				const char* ExampleTag = nodeExample->getTag();
				if (strcmp(ExampleTag, "Param") == 0)
				{
					const string parameterName = nodeExample->getAttr("name");
					m_additionalExtraction[d].parameterId = ParameterIdByName(parameterName.c_str(), true);

					// Check if the parameter-name is supported by the system
					if (m_additionalExtraction[d].parameterId == eMotionParamID_INVALID)
					{
						((errorMessage += "Error: The parameter '") += parameterName.c_str()) += "' is currently not supported\n";
						result = false;
					}
				}
			}
			continue;
		}

		//-----------------------------------------------------------
		//load example-list
		//-----------------------------------------------------------
		if (strcmp(ListTag, "ExampleList") == 0)
		{
			int numExamples = nodeList->getChildCount();
			m_examples.resize(numExamples);
			for (uint32 i = 0; i < numExamples; i++)
			{
				XmlNodeRef nodeExample = nodeList->getChild(i);
				const char* ExampleTAG = nodeExample->getTag();
				if (strcmp(ExampleTAG, "Example"))
				{
					((errorMessage += "Error: The ExampleTAG for an example is '") += ExampleTAG) += "'. It is expected to be 'Example'\n";
					result = false;
				}

				if (strcmp(ExampleTAG, "Example") == 0)
				{
					m_examples[i].animation = nodeExample->getAttr("AName");

					nodeExample->getAttr("PlaybackScale", m_examples[i].playbackScale);

					// Pre-initialized parameters should be an exception. Only use them if real extraction is impossible
					for (size_t p = 0; p < m_dimensions.size(); ++p)
					{
						auto& parameter = m_examples[i].parameters[m_dimensions[p].parameterId];

						const auto valueNode = "SetPara" + std::to_string(p);
						parameter.userDefined = nodeExample->getAttr(valueNode.c_str(), parameter.value);

						const auto deltaMotionNode = "UseDirectlyForDeltaMotion" + std::to_string(p);
						nodeExample->getAttr(deltaMotionNode.c_str(), parameter.useDirectlyForDeltaMotion);
					}
				}
			}
			continue;
		}

		//-----------------------------------------------------------
		//load pseudo example-list
		//-----------------------------------------------------------
		if (strcmp(ListTag, "ExamplePseudo") == 0)
		{
			uint32 num = nodeList->getChildCount();
			m_pseudoExamples.resize(num);
			for (uint32 i = 0; i < num; i++)
			{
				XmlNodeRef nodeExample = nodeList->getChild(i);
				const char* ExampleTag = nodeExample->getTag();
				if (strcmp(ExampleTag, "Pseudo") == 0)
				{
					int32 i0 = -1;
					f32 w0 = 1.0f;
					int32 i1 = -1;
					f32 w1 = 1.0f;

					nodeExample->getAttr("p0", i0);
					nodeExample->getAttr("p1", i1);
					nodeExample->getAttr("w0", w0);
					nodeExample->getAttr("w1", w1);

					if (i0 < 0 || uint32(i0) >= numExamples)
					{
						stack_string message;
						errorMessage += message.Format("Error: Pseudo example %d contains a reference to non-existing example with index %d.\n", i, i0).c_str();
						result = false;
					}

					if (i1 < 0 || uint32(i1) >= numExamples)
					{
						stack_string message;
						errorMessage += message.Format("Error: Pseudo example %d contains a reference to non-existing example with index %d.\n", i, i1).c_str();
						result = false;
					}

					const f32 wSum = w0 + w1;
					if (fabsf(wSum) > 0.00001f)
					{
						w0 /= wSum;
						w1 /= wSum;
					}
					else
					{
						w0 = 0.5f;
						w1 = 0.5f;
					}

					const auto exampleIndex2Guid = [&](int32 index) -> CryGUID
					{
						return (index >= 0 && size_t(index) < m_examples.size()) ? m_examples[index].runtimeGuid : CryGUID::Null();
					};

					m_pseudoExamples[i].guid0 = exampleIndex2Guid(i0);
					m_pseudoExamples[i].weight0 = w0;
					m_pseudoExamples[i].guid1 = exampleIndex2Guid(i1);
					m_pseudoExamples[i].weight1 = w1;
				}
			}
			continue;
		}

		//-----------------------------------------------------------
		//---  load blend-annotation-list                          --
		//-----------------------------------------------------------
		if (strcmp(ListTag, "Blendable") == 0)
		{
			const auto exampleIndex2Guid = [&](int32 index) -> CryGUID
			{
				if (index >= 0 && size_t(index) < m_examples.size())
				{
					return m_examples[index].runtimeGuid;
				}

				if (index >= 0 && size_t(index) < (m_examples.size() + m_pseudoExamples.size()))
				{
					return m_pseudoExamples[size_t(index) - m_examples.size()].runtimeGuid;
				}
				
				return CryGUID::Null();
			};

			const char* facePointNames[] = { "p0", "p1", "p2", "p3", "p4", "p5", "p6", "p7" };

			const uint32 annotationCount = nodeList->getChildCount();
			m_annotations.clear();
			m_annotations.reserve(annotationCount);
			for (uint32 i = 0; i < annotationCount; i++)
			{
				XmlNodeRef nodeAnnotation = nodeList->getChild(i);
				if (strcmp(nodeAnnotation->getTag(), "Face") == 0)
				{
					BlendSpaceAnnotation face;
					for (size_t i = 0; i < CRY_ARRAY_COUNT(facePointNames); ++i)
					{
						int32 exampleIndex;
						if (nodeAnnotation->getAttr(facePointNames[i], exampleIndex))
						{
							face.exampleGuids.push_back(exampleIndex2Guid(exampleIndex));
						}
					}
					m_annotations.push_back(face);
				}
			}
			continue;
		}

		//-----------------------------------------------------------
		//---  load precomputed example grid                       --
		//-----------------------------------------------------------
		// TODO
		// if ( strcmp(ListTag,"VGrid")==0 )
		// {
		//  VirtualExampleXML::ReadVGrid(nodeList, *this);
		//  continue;
		// }

		//-----------------------------------------------------------
		//---    check of Motion-Combination examples             ---
		//-----------------------------------------------------------
		if (strcmp(ListTag, "MotionCombination") == 0)
		{
			uint32 num = nodeList->getChildCount();
			m_motionCombinations.resize(num);
			for (uint32 i = 0; i < num; i++)
			{
				XmlNodeRef nodeExample = nodeList->getChild(i);
				const char* ExampleTag = nodeExample->getTag();

				if (strcmp(ExampleTag, "NewStyle") == 0)
				{
					m_motionCombinations[i].animation = nodeExample->getAttr("Style");
				}
				else
				{
					((errorMessage += "Error: The XML-Tag '") += ExampleTag) += "' is currently not supported\n";
					result = false;
				}
			}
			continue;
		}

		//-----------------------------------------------------------
		//-- joint mask
		//-----------------------------------------------------------
		if (strcmp(ListTag, "JointList") == 0)
		{
			uint32 num = nodeList->getChildCount();
			m_joints.resize(num);
			for (uint32 i = 0; i < num; ++i)
			{
				XmlNodeRef node = nodeList->getChild(i);
				const char* tag = node->getTag();
				if (strcmp(tag, "Joint") != 0)
				{
					((errorMessage += "Error: The XML-Tag '") += tag) += "' is currently not supported\n";
					result = false;
				}

				m_joints[i].name = node->getAttr("Name");
			}

			std::sort(m_joints.begin(), m_joints.end());
			continue;
		}

		if (strcmp(ListTag, "VGrid") == 0)
		{
			if (m_dimensions.size() == 1)
			{
				uint32 num = nodeList->getChildCount();
				m_virtualExamples1d.resize(num);
				for (uint32 i = 0; i < num; ++i)
				{
					BlendSpaceVirtualExample1D& example = m_virtualExamples1d[i];
					XmlNodeRef node = nodeList->getChild(i);
					const char* tag = node->getTag();
					if (strcmp(tag, "VExample") != 0)
					{
						((errorMessage += "Error: The XML-Tag '") += tag) += "' is currently not supported\n";
						result = false;
					}

					node->getAttr("i0", example.i0);
					node->getAttr("i1", example.i1);
					node->getAttr("w0", example.w0);
					node->getAttr("w1", example.w1);
				}
			}
			if (m_dimensions.size() == 2)
			{
				uint32 num = nodeList->getChildCount();
				m_virtualExamples2d.resize(num);
				for (uint32 i = 0; i < num; ++i)
				{
					BlendSpaceVirtualExample2D& example = m_virtualExamples2d[i];
					XmlNodeRef node = nodeList->getChild(i);
					const char* tag = node->getTag();
					if (strcmp(tag, "VExample") != 0)
					{
						((errorMessage += "Error: The XML-Tag '") += tag) += "' is currently not supported\n";
						result = false;
					}

					node->getAttr("i0", example.i0);
					node->getAttr("i1", example.i1);
					node->getAttr("i2", example.i2);
					node->getAttr("i3", example.i3);
					node->getAttr("w0", example.w0);
					node->getAttr("w1", example.w1);
					node->getAttr("w2", example.w2);
					node->getAttr("w3", example.w3);
				}
			}
			if (m_dimensions.size() == 3)
			{
				uint32 num = nodeList->getChildCount();
				m_virtualExamples3d.resize(num);
				for (uint32 i = 0; i < num; ++i)
				{
					BlendSpaceVirtualExample3D& example = m_virtualExamples3d[i];
					XmlNodeRef node = nodeList->getChild(i);
					const char* tag = node->getTag();
					if (strcmp(tag, "VExample") != 0)
					{
						((errorMessage += "Error: The XML-Tag '") += tag) += "' is currently not supported\n";
						result = false;
					}

					node->getAttr("i0", example.i0);
					node->getAttr("i1", example.i1);
					node->getAttr("i2", example.i2);
					node->getAttr("i3", example.i3);
					node->getAttr("i4", example.i4);
					node->getAttr("i5", example.i5);
					node->getAttr("i6", example.i6);
					node->getAttr("i7", example.i7);
					node->getAttr("w0", example.w0);
					node->getAttr("w1", example.w1);
					node->getAttr("w2", example.w2);
					node->getAttr("w3", example.w3);
					node->getAttr("w4", example.w4);
					node->getAttr("w5", example.w5);
					node->getAttr("w6", example.w6);
					node->getAttr("w7", example.w7);
				}
			}
		}
	}

	if (m_dimensions.size() == 0)
	{
		errorMessage += "Error: ParaGroup has no m_dimensions specified\n";
		result = false;
	}

	return result;
}

XmlNodeRef BlendSpace::SaveToXml() const
{
	XmlNodeRef root = gEnv->pSystem->CreateXmlNode("ParaGroup");

	if (m_threshold > -99999.0f)
	{
		XmlNodeRef threshold = root->newChild("THRESHOLD");
		threshold->setAttr("tz", m_threshold);
	}

	if (m_idleToMove)
	{
		XmlNodeRef vegparams = root->newChild("VEGPARAMS");
		vegparams->setAttr("Idle2Move", "1");
	}

	XmlNodeRef nodeList = root->newChild("Dimensions");
	uint32 numDimensions = m_dimensions.size();
	for (uint32 d = 0; d < numDimensions; ++d)
	{
		const auto& dim = m_dimensions[d];
		XmlNodeRef nodeExample = nodeList->newChild("Param");
		nodeExample->setAttr("Name", ParameterNameById(EMotionParamID(dim.parameterId), false));
		nodeExample->setAttr("Min", dim.minimal);
		nodeExample->setAttr("Max", dim.maximal);
		nodeExample->setAttr("Cells", dim.cellCount);
		if (dim.debugVisualScale != 1.0f) nodeExample->setAttr("Scale", dim.debugVisualScale);
		if (dim.startKey != 0.0f) nodeExample->setAttr("SKey", dim.startKey);
		if (dim.endKey != 1.0f) nodeExample->setAttr("EKey", dim.endKey);
		if (dim.locked) nodeExample->setAttr("locked", m_dimensions[d].locked);
	}

	if (uint32 numExtractionParams = m_additionalExtraction.size())
	{
		nodeList = root->newChild("AdditionalExtraction");
		for (uint32 d = 0; d < numExtractionParams; d++)
		{
			SMotionParameterDetails details;
			gEnv->pCharacterManager->GetMotionParameterDetails(details, EMotionParamID(m_additionalExtraction[d].parameterId));

			XmlNodeRef nodeExample = nodeList->newChild("Param");
			nodeExample->setAttr("name", details.name ? details.name : "");
		}
	}

	nodeList = root->newChild("ExampleList");
	int numExamples = m_examples.size();
	for (uint32 i = 0; i < numExamples; ++i)
	{
		const auto& e = m_examples[i];
		XmlNodeRef nodeExample = nodeList->newChild("Example");

		for (size_t p = 0; p < m_dimensions.size(); ++p)
		{
			const auto& parameter = m_examples[i].parameters[m_dimensions[p].parameterId];

			if (parameter.userDefined)
			{
				const auto valueNode = "SetPara" + std::to_string(p);
				nodeExample->setAttr(valueNode.c_str(), parameter.value);
			}

			if (parameter.useDirectlyForDeltaMotion)
			{
				const auto deltaMotionNode = "UseDirectlyForDeltaMotion" + std::to_string(p);
				nodeExample->setAttr(deltaMotionNode.c_str(), true);
			}
		}

		nodeExample->setAttr("AName", e.animation.c_str());
		if (e.playbackScale != 1.0f)
		{
			nodeExample->setAttr("PlaybackScale", e.playbackScale);
		}
	}

	if (uint32 pseudoCount = m_pseudoExamples.size())
	{
		nodeList = root->newChild("ExamplePseudo");
		for (uint32 i = 0; i < pseudoCount; ++i)
		{
			const auto exampleGuid2Index = [&](const CryGUID& guid) -> int32
			{
				const auto itSearch = std::find_if(m_examples.begin(), m_examples.end(), [&](const auto& example) { return example.runtimeGuid == guid; });
				return (itSearch != m_examples.end()) ? int32(std::distance(m_examples.begin(), itSearch)) : int32(-1);
			};

			XmlNodeRef nodeExample = nodeList->newChild("Pseudo");
			nodeExample->setAttr("p0", exampleGuid2Index(m_pseudoExamples[i].guid0));
			nodeExample->setAttr("p1", exampleGuid2Index(m_pseudoExamples[i].guid1));
			nodeExample->setAttr("w0", m_pseudoExamples[i].weight0);
			nodeExample->setAttr("w1", m_pseudoExamples[i].weight1);
		}
	}

	if (const uint32 annotationCount = m_annotations.size())
	{
		const auto exampleGuid2Index = [&](const CryGUID& guid) -> int32
		{
			const auto itSearch = std::find_if(m_examples.begin(), m_examples.end(), [&](const auto& example) { return example.runtimeGuid == guid; });
			if (itSearch != m_examples.end())
			{
				return int32(std::distance(m_examples.begin(), itSearch));
			}

			const auto itSearchPseudo = std::find_if(m_pseudoExamples.begin(), m_pseudoExamples.end(), [&](const auto& pseudoExample) { return pseudoExample.runtimeGuid == guid; });
			if (itSearchPseudo != m_pseudoExamples.end())
			{
				return int32(m_examples.size() + std::distance(m_pseudoExamples.begin(), itSearchPseudo));
			}

			return int32(-1);
		};

		const char* facePointNames[] = { "p0", "p1", "p2", "p3", "p4", "p5", "p6", "p7" };

		nodeList = root->newChild("Blendable");
		for (uint32 i = 0; i < annotationCount; ++i)
		{
			XmlNodeRef nodeFace = nodeList->newChild("Face");
			const BlendSpaceAnnotation& face = m_annotations[i];

			CRY_ASSERT(face.exampleGuids.size() <= CRY_ARRAY_COUNT(facePointNames));
			for (uint32 k = 0, pointCount = face.exampleGuids.size(); k < pointCount; ++k)
			{
				nodeFace->setAttr(facePointNames[k], exampleGuid2Index(face.exampleGuids[k]));
			}
		}
	}

	if (uint32 motionCombinationCount = m_motionCombinations.size())
	{
		nodeList = root->newChild("MotionCombination");
		for (uint32 i = 0; i < motionCombinationCount; ++i)
		{
			XmlNodeRef nodeExample = nodeList->newChild("NewStyle");
			nodeExample->setAttr("Style", m_motionCombinations[i].animation.c_str());
		}
	}

	if (uint32 jointCount = m_joints.size())
	{
		nodeList = root->newChild("JointList");
		for (uint32 i = 0; i < jointCount; ++i)
		{
			XmlNodeRef node = nodeList->newChild("Joint");
			node->setAttr("Name", m_joints[i].name.c_str());
		}
	}

	if (!m_virtualExamples1d.empty() ||
	    !m_virtualExamples2d.empty() ||
	    !m_virtualExamples3d.empty())
	{
		nodeList = root->newChild("VGrid");
		if (!m_virtualExamples1d.empty())
		{
			for (uint32 i = 0; i < m_virtualExamples1d.size(); ++i)
			{
				const BlendSpaceVirtualExample1D& example = m_virtualExamples1d[i];
				XmlNodeRef node = nodeList->newChild("VExample");
				node->setAttr("i0", example.i0);
				node->setAttr("i1", example.i1);
				node->setAttr("w0", example.w0);
				node->setAttr("w1", example.w1);
			}
		}
		else if (!m_virtualExamples2d.empty())
		{
			for (uint32 i = 0; i < m_virtualExamples2d.size(); ++i)
			{
				const BlendSpaceVirtualExample2D& example = m_virtualExamples2d[i];
				XmlNodeRef node = nodeList->newChild("VExample");
				node->setAttr("i0", example.i0);
				node->setAttr("i1", example.i1);
				node->setAttr("i2", example.i2);
				node->setAttr("i3", example.i3);
				node->setAttr("w0", example.w0);
				node->setAttr("w1", example.w1);
				node->setAttr("w2", example.w2);
				node->setAttr("w3", example.w3);
			}
		}
		else if (!m_virtualExamples3d.empty())
		{
			for (uint32 i = 0; i < m_virtualExamples3d.size(); ++i)
			{
				const BlendSpaceVirtualExample3D& example = m_virtualExamples3d[i];
				XmlNodeRef node = nodeList->newChild("VExample");
				node->setAttr("i0", example.i0);
				node->setAttr("i1", example.i1);
				node->setAttr("i2", example.i2);
				node->setAttr("i3", example.i3);
				node->setAttr("i4", example.i4);
				node->setAttr("i5", example.i5);
				node->setAttr("i6", example.i6);
				node->setAttr("i7", example.i7);
				node->setAttr("w0", example.w0);
				node->setAttr("w1", example.w1);
				node->setAttr("w2", example.w2);
				node->setAttr("w3", example.w3);
				node->setAttr("w4", example.w4);
				node->setAttr("w5", example.w5);
				node->setAttr("w6", example.w6);
				node->setAttr("w7", example.w7);
			}
		}
	}

	return root;
}

void BlendSpace::Serialize(Serialization::IArchive& ar)
{
	Serialization::SContext bspaceContext(ar, this);

	SMotionParametersPool parameterPool;
	Serialization::SContext parameterPoolContext(ar, &parameterPool);

	ar(m_threshold, "threshold", 0);
	ar(m_idleToMove, "idleToMove", "Idle To Move");

	ar(m_dimensions, "dimensions", "Dimensions");
	if (m_dimensions.empty())
		ar.warning(m_dimensions, "At least one dimension is required for the BlendSpace");
	else if (m_dimensions.size() > 3)
		ar.error(m_dimensions, "At most 3 dimensions are supported");

	ar(m_examples, "examples", "Examples");
	const int maxBlendSpaceAnimations = 40;
	if (m_examples.size() > maxBlendSpaceAnimations)
		ar.error(m_examples, "Number of examples should not excceed %d.", maxBlendSpaceAnimations);

	ar(m_pseudoExamples, "pseudoExamples", "[+]Pseudo Examples");

	ar(m_additionalExtraction, "additionalExtraction", "Additional Extraction");
	if (m_additionalExtraction.size() > 4)
		ar.error(m_additionalExtraction, "More than 4 additional extraction parameters are not supported");

	ar(m_motionCombinations, "motionCombinations", "Motion Combinations");
	ar(m_joints, "joints", "Joints");
	ar(m_annotations, "annotations", "Annotations");

	if (!ar.isEdit())
	{
		ar(m_virtualExamples1d, "virtualExamples1d", "Virtual Examples (1D)");
		ar(m_virtualExamples2d, "virtualExamples2d", "Virtual Examples (2D)");
		ar(m_virtualExamples3d, "virtualExamples3d", "Virtual Examples (3D)");
	}
}

// ---------------------------------------------------------------------------

void CombinedBlendSpaceDimension::Serialize(IArchive& ar)
{
	ar(SMotionParametersPool::SSelector(parameterId), "parameterId", "<^");
	ar(locked, "locked", "^Locked");
	ar(chooseBlendSpace, "chooseBlendSpace", "^Selects BSpace");
	ar(parameterScale, "parameterScale", ">^ x");
}

void CombinedBlendSpace::Serialize(IArchive& ar)
{
	SMotionParametersPool parameterPool;
	Serialization::SContext parameterPoolContext(ar, &parameterPool);

	ar(m_idleToMove, "idleToMove", "Idle To Move");
	ar(m_dimensions, "dimensions", "Dimensions");
	ar(m_additionalExtraction, "m_additionalExtraction", "Additional Extraction");
	ar(m_blendSpaces, "blendSpaces", "Blend Spaces");
	ar(m_motionCombinations, "motionCombinations", "Motion Combinations");
	ar(m_joints, "joints", "Joints");
}

bool CombinedBlendSpace::LoadFromXml(string& errorMessage, XmlNodeRef root, IAnimationSet* pAnimationSet)
{
	bool result = false;
	//---------------------------------------------------------
	//--- parse and verify XML
	//---------------------------------------------------------
	const char* XMLTAG = root->getTag();
	if (strcmp(XMLTAG, "CombinedBlendSpace") != 0)
	{
		((errorMessage += "Error: The XMLTAG is '") += XMLTAG) += "'. It is expected to be 'ParaGroup' or 'CombinedBlendSpace'\n";
		result = false;
	}

	//----------------------------------------------------------------------
	//----      check of this is a combined Blend-Space          -----------
	//----------------------------------------------------------------------
	uint32 numChilds = root->getChildCount();
	for (uint32 c = 0; c < numChilds; c++)
	{
		XmlNodeRef nodeList = root->getChild(c);
		const char* ListTag = nodeList->getTag();

		if (strcmp(ListTag, "VEGPARAMS") == 0)
		{
			uint32 nFlags = 0;
			nodeList->getAttr("Idle2Move", nFlags);     //will go as soon as CryMannegin is up and running
			m_idleToMove = nFlags != 0;
			continue;
		}

		//-----------------------------------------------------------
		//---  define m_dimensions of the LMG                       ---
		//-----------------------------------------------------------
		if (strcmp(ListTag, "Dimensions") == 0)
		{
			int numDimensions = nodeList->getChildCount();
			if (numDimensions > 4)
			{
				errorMessage += "Error: More then 4 m_dimensions per ParaGroup are not supported\n";
				result = false;
			}

			m_dimensions.resize(numDimensions);
			for (uint32 d = 0; d < numDimensions; d++)
			{
				XmlNodeRef nodeExample = nodeList->getChild(d);
				const char* ExampleTag = nodeExample->getTag();
				if (strcmp(ExampleTag, "Param") == 0)
				{
					//each dimension must have a parameter-name
					const char* parameterName = nodeExample->getAttr("name");
					m_dimensions[d].parameterId = ParameterIdByName(parameterName, false);

					if (m_dimensions[d].parameterId == eMotionParamID_INVALID)
					{
						((errorMessage += "Error: The parameter '") += parameterName) += "' is currently not supported\n";
						result = false;
					}

					if (!nodeExample->getAttr("ParaScale", m_dimensions[d].parameterScale))
						m_dimensions[d].parameterScale = 1.0f;
					if (!nodeExample->getAttr("ChooseBlendSpace", m_dimensions[d].chooseBlendSpace))
						m_dimensions[d].chooseBlendSpace = false;
					//special flags per-dimension
					if (!nodeExample->getAttr("locked", m_dimensions[d].locked))
						m_dimensions[d].locked = false;
				}
			}
			continue;
		}

		//-----------------------------------------------------------
		//---  define the additional extraction parameters        ---
		//-----------------------------------------------------------
		if (strcmp(ListTag, "AdditionalExtraction") == 0)
		{
			int numExtractionParams = nodeList->getChildCount();
			if (numExtractionParams > 4)
			{
				errorMessage += "Error: More then 4 additional extraction parameters are not supported\n";
				result = false;
			}
			m_additionalExtraction.resize(numExtractionParams);
			for (uint32 d = 0; d < numExtractionParams; d++)
			{
				XmlNodeRef nodeExample = nodeList->getChild(d);
				const char* ExampleTag = nodeExample->getTag();
				if (strcmp(ExampleTag, "Param") == 0)
				{
					const string parameterName = nodeExample->getAttr("name");
					m_additionalExtraction[d].parameterId = ParameterIdByName(parameterName.c_str(), true);

					// Check if the parameter-name is supported by the system
					if (m_additionalExtraction[d].parameterId == eMotionParamID_INVALID)
					{
						((errorMessage += "Error: The parameter '") += parameterName.c_str()) += "' is currently not supported\n";
						result = false;
					}
				}
			}
			continue;
		}

		if (strcmp(ListTag, "BlendSpaces") == 0)
		{
			uint32 num = nodeList->getChildCount();
			m_blendSpaces.resize(num);
			for (uint32 i = 0; i < num; i++)
			{
				XmlNodeRef nodeExample = nodeList->getChild(i);
				const char* ExampleTag = nodeExample->getTag();
				if (strcmp(ExampleTag, "BlendSpace") == 0)
				{
					string strFilePath = nodeExample->getAttr("aname");
					m_blendSpaces[i].path = strFilePath;
				}
				else
				{
					((errorMessage += "Error: The XML-Tag '") += ExampleTag) += "' is currently not supported";
					result = false;
				}
			}
			continue;
		}

		//-----------------------------------------------------------
		//---    check of Motion-Combination examples             ---
		//-----------------------------------------------------------
		if (strcmp(ListTag, "MotionCombination") == 0)
		{
			uint32 num = nodeList->getChildCount();
			m_motionCombinations.resize(num);
			for (uint32 i = 0; i < num; i++)
			{
				XmlNodeRef nodeExample = nodeList->getChild(i);
				const char* ExampleTag = nodeExample->getTag();

				if (strcmp(ExampleTag, "NewStyle") == 0)
				{
					m_motionCombinations[i].animation = nodeExample->getAttr("Style");
				}
				else
				{
					((errorMessage += "Error: The XML-Tag '") += ExampleTag) += "' is currently not supported\n";
					result = false;
				}
			}
			continue;
		}

		//-----------------------------------------------------------
		//-- joint mask
		//-----------------------------------------------------------
		if (strcmp(ListTag, "JointList") == 0)
		{
			uint32 num = nodeList->getChildCount();
			m_joints.resize(num);
			for (uint32 i = 0; i < num; ++i)
			{
				XmlNodeRef node = nodeList->getChild(i);
				const char* tag = node->getTag();
				if (strcmp(tag, "Joint") != 0)
				{
					((errorMessage += "Error: The XML-Tag '") += tag) += "' is currently not supported\n";
					result = false;
				}

				m_joints[i].name = node->getAttr("Name");
			}

			std::sort(m_joints.begin(), m_joints.end());
			continue;
		}
	}

	return result;
}

XmlNodeRef CombinedBlendSpace::SaveToXml() const
{
	XmlNodeRef root = gEnv->pSystem->CreateXmlNode("CombinedBlendSpace");

	if (m_idleToMove)
	{
		XmlNodeRef node = root->newChild("VEGPARAMS");
		node->setAttr("Idle2Move", "1");
	}

	if (uint32 num = m_dimensions.size())
	{
		XmlNodeRef dimensions = root->newChild("Dimensions");
		for (uint32 d = 0; d < num; d++)
		{
			XmlNodeRef node = dimensions->newChild("Param");
			node->setAttr("Name", ParameterNameById(EMotionParamID(m_dimensions[d].parameterId), false));
			node->setAttr("ParaScale", m_dimensions[d].parameterScale);
			if (m_dimensions[d].chooseBlendSpace)
				node->setAttr("ChooseBlendSpace", m_dimensions[d].chooseBlendSpace);
			if (m_dimensions[d].locked)
				node->setAttr("Locked", m_dimensions[d].locked);
		}
	}

	if (uint32 num = m_additionalExtraction.size())
	{
		XmlNodeRef nodeList = root->newChild("AdditionalExtraction");
		for (uint32 d = 0; d < num; ++d)
		{
			SMotionParameterDetails details;
			gEnv->pCharacterManager->GetMotionParameterDetails(details, EMotionParamID(m_additionalExtraction[d].parameterId));

			XmlNodeRef nodeExample = nodeList->newChild("Param");
			nodeExample->setAttr("Name", details.name ? details.name : "");
		}
	}

	if (uint32 num = m_blendSpaces.size())
	{
		XmlNodeRef nodeList = root->newChild("BlendSpaces");
		for (uint32 i = 0; i < num; ++i)
		{
			XmlNodeRef node = nodeList->newChild("BlendSpace");
			node->setAttr("AName", m_blendSpaces[i].path.c_str());
		}
	}

	if (uint32 num = m_motionCombinations.size())
	{
		XmlNodeRef nodeList = root->newChild("MotionCombination");
		for (uint32 i = 0; i < num; ++i)
		{
			XmlNodeRef node = nodeList->newChild("NewStyle");
			node->setAttr("Style", m_motionCombinations[i].animation.c_str());
		}
	}

	if (uint32 num = m_joints.size())
	{
		XmlNodeRef nodeList = root->newChild("JointList");
		for (uint32 i = 0; i < num; ++i)
		{
			XmlNodeRef node = nodeList->newChild("Joint");
			node->setAttr("Name", m_joints[i].name.c_str());
		}
	}

	return root;
}

}


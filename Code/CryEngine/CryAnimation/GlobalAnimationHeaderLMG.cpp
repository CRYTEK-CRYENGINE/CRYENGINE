// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "GlobalAnimationHeaderLMG.h"

#include "SkeletonAnim.h"
#include "ParametricSampler.h"
#include "Model.h"
#include "CharacterManager.h"

namespace VirtualExampleXML
{
void ReadFromXmlNode(const XmlNodeRef& xmlNode, VirtualExample1D& vOut)
{
	CRY_ASSERT(xmlNode);

	xmlNode->getAttr("i0", vOut.i0);
	xmlNode->getAttr("i1", vOut.i1);

	xmlNode->getAttr("w0", vOut.w0);
	xmlNode->getAttr("w1", vOut.w1);
}

void ReadFromXmlNode(const XmlNodeRef& xmlNode, VirtualExample2D& vOut)
{
	CRY_ASSERT(xmlNode);

	xmlNode->getAttr("i0", vOut.i0);
	xmlNode->getAttr("i1", vOut.i1);
	xmlNode->getAttr("i2", vOut.i2);
	xmlNode->getAttr("i3", vOut.i3);

	xmlNode->getAttr("w0", vOut.w0);
	xmlNode->getAttr("w1", vOut.w1);
	xmlNode->getAttr("w2", vOut.w2);
	xmlNode->getAttr("w3", vOut.w3);
}

void ReadFromXmlNode(const XmlNodeRef& xmlNode, VirtualExample3D& vOut)
{
	CRY_ASSERT(xmlNode);

	xmlNode->getAttr("i0", vOut.i0);
	xmlNode->getAttr("i1", vOut.i1);
	xmlNode->getAttr("i2", vOut.i2);
	xmlNode->getAttr("i3", vOut.i3);
	xmlNode->getAttr("i4", vOut.i4);
	xmlNode->getAttr("i5", vOut.i5);
	xmlNode->getAttr("i6", vOut.i6);
	xmlNode->getAttr("i7", vOut.i7);

	xmlNode->getAttr("w0", vOut.w0);
	xmlNode->getAttr("w1", vOut.w1);
	xmlNode->getAttr("w2", vOut.w2);
	xmlNode->getAttr("w3", vOut.w3);
	xmlNode->getAttr("w4", vOut.w4);
	xmlNode->getAttr("w5", vOut.w5);
	xmlNode->getAttr("w6", vOut.w6);
	xmlNode->getAttr("w7", vOut.w7);
}

void WriteToXmlNode(const XmlNodeRef& xmlNode, const VirtualExample1D& vOut)
{
	CRY_ASSERT(xmlNode);

	xmlNode->setAttr("i0", vOut.i0);
	xmlNode->setAttr("i1", vOut.i1);

	xmlNode->setAttr("w0", vOut.w0);
	xmlNode->setAttr("w1", vOut.w1);
}

void WriteToXmlNode(const XmlNodeRef& xmlNode, const VirtualExample2D& vOut)
{
	CRY_ASSERT(xmlNode);

	xmlNode->setAttr("i0", vOut.i0);
	xmlNode->setAttr("i1", vOut.i1);
	xmlNode->setAttr("i2", vOut.i2);
	xmlNode->setAttr("i3", vOut.i3);

	xmlNode->setAttr("w0", vOut.w0);
	xmlNode->setAttr("w1", vOut.w1);
	xmlNode->setAttr("w2", vOut.w2);
	xmlNode->setAttr("w3", vOut.w3);
}

void WriteToXmlNode(const XmlNodeRef& xmlNode, const VirtualExample3D& vOut)
{
	CRY_ASSERT(xmlNode);

	xmlNode->setAttr("i0", vOut.i0);
	xmlNode->setAttr("i1", vOut.i1);
	xmlNode->setAttr("i2", vOut.i2);
	xmlNode->setAttr("i3", vOut.i3);
	xmlNode->setAttr("i4", vOut.i4);
	xmlNode->setAttr("i5", vOut.i5);
	xmlNode->setAttr("i6", vOut.i6);
	xmlNode->setAttr("i7", vOut.i7);

	xmlNode->setAttr("w0", vOut.w0);
	xmlNode->setAttr("w1", vOut.w1);
	xmlNode->setAttr("w2", vOut.w2);
	xmlNode->setAttr("w3", vOut.w3);
	xmlNode->setAttr("w4", vOut.w4);
	xmlNode->setAttr("w5", vOut.w5);
	xmlNode->setAttr("w6", vOut.w6);
	xmlNode->setAttr("w7", vOut.w7);
}

template<typename TVirtualExample>
bool ReadVGrid(const XmlNodeRef& xmlNode, const uint32 expectedVirtualExampleCount, DynArray<TVirtualExample>& virtualExampleGrid)
{
	CRY_ASSERT(xmlNode);

	const uint32 virtualExampleCount = xmlNode->getChildCount();
	if (virtualExampleCount != expectedVirtualExampleCount)
	{
		return false;
	}

	virtualExampleGrid.resize(virtualExampleCount);
	for (uint32 i = 0; i < virtualExampleCount; ++i)
	{
		XmlNodeRef xmlExampleNode = xmlNode->getChild(i);

		TVirtualExample& virtualExample = virtualExampleGrid[i];
		ReadFromXmlNode(xmlExampleNode, virtualExample);
	}

	return true;
}

template<typename TVirtualExample>
bool WriteVGrid(const XmlNodeRef& xmlNode, const DynArray<TVirtualExample>& virtualExampleGrid)
{
	CRY_ASSERT(xmlNode);

	const uint32 virtualExampleCount = virtualExampleGrid.size();
	for (uint32 i = 0; i < virtualExampleCount; ++i)
	{
		const TVirtualExample& virtualExample = virtualExampleGrid[i];

		XmlNodeRef xmlExampleNode = xmlNode->createNode("VExample");
		WriteToXmlNode(xmlExampleNode, virtualExample);
		xmlNode->addChild(xmlExampleNode);
	}
	return true;
}

bool ReadVGrid(const XmlNodeRef& xmlNode, GlobalAnimationHeaderLMG& gah)
{
	CRY_ASSERT(xmlNode);

	if (gah.m_Dimensions == 1)
	{
		const uint32 expectedVirtualExampleCount = gah.m_DimPara[0].m_cells;
		return ReadVGrid(xmlNode, expectedVirtualExampleCount, gah.m_VirtualExampleGrid1D);
	}
	else if (gah.m_Dimensions == 2)
	{
		const uint32 expectedVirtualExampleCount = gah.m_DimPara[0].m_cells * gah.m_DimPara[1].m_cells;
		return ReadVGrid(xmlNode, expectedVirtualExampleCount, gah.m_VirtualExampleGrid2D);
	}
	else if (gah.m_Dimensions == 3)
	{
		const uint32 expectedVirtualExampleCount = gah.m_DimPara[0].m_cells * gah.m_DimPara[1].m_cells * gah.m_DimPara[2].m_cells;
		return ReadVGrid(xmlNode, expectedVirtualExampleCount, gah.m_VirtualExampleGrid3D);
	}
	return false;
}

bool WriteVGrid(const XmlNodeRef& xmlNode, const GlobalAnimationHeaderLMG& gah)
{
	CRY_ASSERT(xmlNode);

	if (gah.m_Dimensions == 1)
	{
		return WriteVGrid(xmlNode, gah.m_VirtualExampleGrid1D);
	}
	else if (gah.m_Dimensions == 2)
	{
		return WriteVGrid(xmlNode, gah.m_VirtualExampleGrid2D);
	}
	else if (gah.m_Dimensions == 3)
	{
		return WriteVGrid(xmlNode, gah.m_VirtualExampleGrid3D);
	}
	return false;
}
}

bool GlobalAnimationHeaderLMG::LoadAndParseXML(CAnimationSet* pAnimationSet, bool nForceReloading)
{
	if (IsAssetLMG() == 0)
		CryFatalError("CryAnimation: data mismatch");

	if (nForceReloading == 0)
	{
		uint32 numAnims = m_numExamples;//m_arrBSAnimations.size();
		if (numAnims)
			return true; //already loaded and initialized
	}

	const char* pathnameLMG = GetFilePath();
	const char* fileExt = PathUtil::GetExt(pathnameLMG);

	uint32 IsLMG = (stricmp(fileExt, "lmg") == 0);
	if (IsLMG)
		CryFatalError("CryAnimation:  LMGs not supported any more: %s! please use only 'BSpace' and 'COMB' from now on", pathnameLMG);

	XmlNodeRef root = g_pISystem->LoadXmlFromFile(pathnameLMG);
	return LoadFromXML(pAnimationSet, root);
}

bool GlobalAnimationHeaderLMG::LoadFromXML(CAnimationSet* pAnimationSet, XmlNodeRef root)
{
	const char* pathnameLMG = GetFilePath();
	const char* fileExt = PathUtil::GetExt(pathnameLMG);

	InvalidateAssetCreated();
	InvalidateAssetLMG();
	m_numExamples = 0;
	m_Dimensions = 0;
	m_ExtractionParams = 0;
	m_Status.clear();
	m_arrParameter.clear();
	m_arrCombinedBlendSpaces.clear();
	m_arrBSAnnotations.clear();
	m_VirtualExampleGrid1D.clear();
	m_VirtualExampleGrid2D.clear();
	m_VirtualExampleGrid3D.clear();
	m_DimPara[0].init();
	m_DimPara[1].init();
	m_DimPara[2].init();
	m_DimPara[3].init();
	m_ExtPara[0].init();
	m_ExtPara[1].init();
	m_ExtPara[2].init();
	m_ExtPara[3].init();

	if (root == 0)
	{
		int32 bPakPriority = gEnv->pCryPak->GetPakPriority();
		bool bIsOnDisk = gEnv->pCryPak->IsFileExist(pathnameLMG, ICryPak::eFileLocation_OnDisk);
		bool bIsInPak = gEnv->pCryPak->IsFileExist(pathnameLMG, ICryPak::eFileLocation_InPak);
		if (bPakPriority == 0)
		{
			//try to load from disk first
			if (bIsOnDisk)
			{
				m_Status = "Error: Parametric-Group loaded from Disk, but it has an XML-Syntax Error";
				gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
				return false;
			}
			if (bIsInPak)
			{
				m_Status = "Error: Parametric-Group loaded from PAK, but it has an XML-Syntax Error";
				gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
				return false;
			}
		}
		else
		{
			//try to load from PAK first
			if (bIsInPak)
			{
				m_Status = "Error: Parametric-Group loaded from PAK, but it has an XML-Syntax Error";
				gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
				return false;
			}
			if (bIsOnDisk)
			{
				m_Status = "Error: Parametric-Group loaded from Disk, but it has an XML-Syntax Error";
				gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
				return false;
			}
		}

		OnAssetNotFound();
		m_Status = "Error: Parametric-Group not found";
		gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
		//CryFatalError("CryAnimation:  Parametric-Group not found: %s", pathnameLMG);
		return false;
	}

	//the asset for the ParaGroup exists and has no Syntax Error
	OnAssetCreated();

	//---------------------------------------------------------
	//--- parse and verify XML
	//---------------------------------------------------------
	const char* XMLTAG = root->getTag();
	if (strcmp(XMLTAG, "ParaGroup") && strcmp(XMLTAG, "CombinedBlendSpace"))
	{
		((m_Status = "Error: The XMLTAG is '") += XMLTAG) += "'. It is expected to be 'ParaGroup' or 'CombinedBlendSpace'";
		gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
		return false;
	}

	if (strcmp(XMLTAG, "ParaGroup") == 0)
	{
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
					m_Status = "Error: no examples in this ParaGroup.";
					gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
					return false;
				}

				if (numExamples >= MAX_LMG_ANIMS)
				{
					m_Status = "Error: too many examples in one ParaGroup. Only 40 are currently allowed! ";
					gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
					return false;
				}
			}

			//load pseudo example-list
			if (strcmp(ListTag, "ExamplePseudo") == 0)
				numPseudo = nodeList->getChildCount();
		}
		//reserve real and pseudo parameters
		m_arrParameter.resize(numPseudo + numExamples);

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
				nodeList->getAttr("tz", m_fThreshold);   //will go as soon as we have the Blend Nodes to combine VEGs
				continue;
			}
			if (strcmp(ListTag, "VEGPARAMS") == 0)
			{
				uint32 nFlags = 0;
				nodeList->getAttr("Idle2Move", nFlags);                       //will go as soon as CryMannegin is up and running
				if (nFlags)
					m_VEG_Flags |= CA_VEG_I2M; //this is a case for CryMannequin
				continue;
			}

			//-----------------------------------------------------------
			//---  define dimensions of the LMG                       ---
			//-----------------------------------------------------------
			if (strcmp(ListTag, "Dimensions") == 0)
			{
				m_Dimensions = nodeList->getChildCount();
				if (m_Dimensions > 3)
				{
					m_Status = "Error: More than 3 dimensions per Blend-Space are not supported";
					gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
					return false;
				}

				for (uint32 d = 0; d < m_Dimensions; d++)
				{
					XmlNodeRef nodeExample = nodeList->getChild(d);
					const char* ExampleTag = nodeExample->getTag();
					if (strcmp(ExampleTag, "Param") == 0)
					{
						// Each dimension must have a parameter-name
						m_DimPara[d].m_strParaName = nodeExample->getAttr("name");

						// Check if the parameter-name is supported by the system
						bool supported = false;
						for (int iParam = 0; iParam < eMotionParamID_COUNT; ++iParam)
						{
							SMotionParameterDetails details;
							gEnv->pCharacterManager->GetMotionParameterDetails(details, static_cast<EMotionParamID>(iParam));

							if (details.name == m_DimPara[d].m_strParaName)
							{
								m_DimPara[d].m_ParaID = static_cast<EMotionParamID>(iParam);
								supported = true;
								break;
							}
						}

						if (!supported)
						{
							((m_Status = "Error: The parameter '") += m_DimPara[d].m_strParaName.c_str()) += "' is currently not supported";
							gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
							return false;
						}

						//define the scope of the blend-space for each dimension
						nodeExample->getAttr("min", m_DimPara[d].m_min);
						nodeExample->getAttr("max", m_DimPara[d].m_max);
						m_DimPara[d].m_max = (m_DimPara[d].m_max - m_DimPara[d].m_min < 0.01f) ? (m_DimPara[d].m_min + 0.01f) : (m_DimPara[d].m_max);

						nodeExample->getAttr("cells", m_DimPara[d].m_cells);
						m_DimPara[d].m_cells = m_DimPara[d].m_cells < 3 ? 3 : m_DimPara[d].m_cells;

						nodeExample->getAttr("scale", m_DimPara[d].m_scale);   //just for visual-debugging
						m_DimPara[d].m_scale = max(0.01f, m_DimPara[d].m_scale);

						nodeExample->getAttr("skey", m_DimPara[d].m_skey);
						nodeExample->getAttr("ekey", m_DimPara[d].m_ekey);

						//special flags per-dimension
						m_DimPara[d].m_nDimensionFlags = 0;
						uint32 flag = 0;
						nodeExample->getAttr("locked", flag);
						if (flag)
							m_DimPara[d].m_nDimensionFlags |= CA_Dim_LockedParameter;

					}
				}
				continue;
			}

			//-----------------------------------------------------------
			//---  define the additioanl extraction parameters        ---
			//-----------------------------------------------------------
			if (strcmp(ListTag, "AdditionalExtraction") == 0)
			{
				m_ExtractionParams = nodeList->getChildCount();
				if (m_ExtractionParams > 4)
				{
					m_Status = "Error: More than 4 additional extraction parameters are not supported";
					gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
					return false;
				}
				for (uint32 d = 0; d < m_ExtractionParams; d++)
				{
					XmlNodeRef nodeExample = nodeList->getChild(d);
					const char* ExampleTag = nodeExample->getTag();
					if (strcmp(ExampleTag, "Param") == 0)
					{
						// Each dimension must have a parameter-name
						m_ExtPara[d].m_strParaName = nodeExample->getAttr("name");

						// Check if the parameter-name is supported by the system
						bool supported = false;
						for (int iParam = 0; iParam < eMotionParamID_COUNT; ++iParam)
						{
							SMotionParameterDetails details;
							gEnv->pCharacterManager->GetMotionParameterDetails(details, static_cast<EMotionParamID>(iParam));

							if ((details.flags & SMotionParameterDetails::ADDITIONAL_EXTRACTION) && (details.name == m_ExtPara[d].m_strParaName))
							{
								m_ExtPara[d].m_ParaID = static_cast<EMotionParamID>(iParam);
								supported = true;
								break;
							}
						}

						if (!supported)
						{
							((m_Status = "Error: The parameter '") += m_ExtPara[d].m_strParaName.c_str()) += "' is currently not supported";
							gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
							return false;
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
				m_numExamples = nodeList->getChildCount();
				bool isAdditive = false;
				for (uint32 i = 0; i < m_numExamples; i++)
				{
					XmlNodeRef nodeExample = nodeList->getChild(i);
					const char* ExampleTAG = nodeExample->getTag();
					if (strcmp(ExampleTAG, "Example"))
					{
						((m_Status = "Error: The ExampleTAG for an example is '") += ExampleTAG) += "'. It is expected to be 'Example'";
						gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
						return false;
					}

					if (strcmp(ExampleTAG, "Example") == 0)
					{
						m_arrParameter[i].m_animName.SetName(nodeExample->getAttr("AName"));

						nodeExample->getAttr("PlaybackScale", m_arrParameter[i].m_fPlaybackScale);

						//Pre-initialized parameters should be an exception. Only use them if real extraction is impossible
						m_arrParameter[i].m_PreInitialized[0] = nodeExample->getAttr("SetPara0", m_arrParameter[i].m_Para.x);

						m_arrParameter[i].m_PreInitialized[1] = nodeExample->getAttr("SetPara1", m_arrParameter[i].m_Para.y);
						if (m_arrParameter[i].m_PreInitialized[1] && m_Dimensions < 2)
						{
							m_Status = "Error: SetPara1 is not allowed on BSpaces with less than 2 dimensions";
							gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
							return false;
						}

						m_arrParameter[i].m_PreInitialized[2] = nodeExample->getAttr("SetPara2", m_arrParameter[i].m_Para.z);
						if (m_arrParameter[i].m_PreInitialized[2] && m_Dimensions < 3)
						{
							m_Status = "Error: SetPara1 is not allowed on BSpaces with less than 3 dimensions";
							gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
							return false;
						}

						m_arrParameter[i].m_PreInitialized[3] = nodeExample->getAttr("SetPara3", m_arrParameter[i].m_Para.w);
						if (m_arrParameter[i].m_PreInitialized[3] && m_Dimensions < 4)
						{
							m_Status = "Error: SetPara1 is not allowed on BSpaces with less than 4 dimensions";
							gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
							return false;
						}

						nodeExample->getAttr("UseDirectlyForDeltaMotion0", m_arrParameter[i].m_UseDirectlyForDeltaMotion[0]);
						nodeExample->getAttr("UseDirectlyForDeltaMotion1", m_arrParameter[i].m_UseDirectlyForDeltaMotion[1]);
						nodeExample->getAttr("UseDirectlyForDeltaMotion2", m_arrParameter[i].m_UseDirectlyForDeltaMotion[2]);
						nodeExample->getAttr("UseDirectlyForDeltaMotion3", m_arrParameter[i].m_UseDirectlyForDeltaMotion[3]);

						m_arrParameter[i].i0 = i;
						m_arrParameter[i].w0 = 1.0f;  //real examples always have a weight of 1.0f
						m_arrParameter[i].i1 = 0;
						m_arrParameter[i].w1 = 0.0f;

						//---------------------------------------------------
						//---        error handling                       ---
						//---------------------------------------------------
						const SCRCName& aName = m_arrParameter[i].m_animName;
						//	const char* pDebugName2 = aName.GetName_DEBUG();
						//	int32 id2=pAnimationSet->GetAnimIDByName(pDebugName2);
						int32 id = pAnimationSet->GetAnimIDByCRC(aName.m_CRC32);
						if (id < 0)
						{
							//name not found in animation-list (chrparams file error)
							const char* pDebugName = aName.GetName_DEBUG();
							((m_Status = "Error: The animation '") += pDebugName) += "' is not in the chrparams file";
							gEnv->pLog->LogError("The ParaGroup '%s' is invalid! The animation '%s' is not in the chrparams file. Model: %s", pathnameLMG, pDebugName, pAnimationSet->GetSkeletonFilePathDebug());
							return false;
						}
						else
						{
							const ModelAnimationHeader* pAnim = pAnimationSet->GetModelAnimationHeader(id);
							assert(pAnim->m_nAssetType == CAF_File);
							int32 gaid = pAnim->m_nGlobalAnimId;
							GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[gaid];
							if (rCAF.IsAssetNotFound())
							{
								((m_Status = "Error: The asset for the animation '") += aName.GetName_DEBUG()) += "' does not exist";
								gEnv->pLog->LogError("The ParaGroup '%s' is invalid! The the asset for animation '%s' does not exist. Model: %s", pathnameLMG, aName.GetName_DEBUG(), pAnimationSet->GetSkeletonFilePathDebug());
								return false;
							}
							else
							{
								if (i == 0)
								{
									isAdditive = 0 != rCAF.IsAssetAdditive();
								}
								else if (isAdditive != (0 != rCAF.IsAssetAdditive()))
								{
									m_Status.Format("Animation asset '%s' is an %s, though other assets are %s. Skeleton: %s", aName.m_name.c_str(), isAdditive ? "override" : "additive", isAdditive ? "additive" : "override", pAnimationSet->GetSkeletonFilePathDebug());
									gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
									return false;
								}
							}
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
				for (uint32 i = 0; i < num; i++)
				{
					XmlNodeRef nodeExample = nodeList->getChild(i);
					const char* ExampleTag = nodeExample->getTag();
					if (strcmp(ExampleTag, "Pseudo") == 0)
					{
						uint32 i0 = -1;
						f32 w0 = 1.0f;
						uint32 i1 = -1;
						f32 w1 = 1.0f;

						nodeExample->getAttr("p0", i0);
						nodeExample->getAttr("p1", i1);
						nodeExample->getAttr("w0", w0);
						nodeExample->getAttr("w1", w1);

						if (i0 >= numExamples)
						{
							m_Status.Format("Error: Pseudo example %d contains a reference to non-existing example with index %d.", i, i0);
							gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
							return false;
						}

						if (i1 >= numExamples)
						{
							m_Status.Format("Error: Pseudo example %d contains a reference to non-existing example with index %d.", i, i1);
							gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
							return false;
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

						m_arrParameter[numExamples + i].i0 = i0;
						m_arrParameter[numExamples + i].w0 = w0;
						m_arrParameter[numExamples + i].i1 = i1;
						m_arrParameter[numExamples + i].w1 = w1;
					}
				}
				continue;
			}

			//-----------------------------------------------------------
			//---  load blend-annotation-list                          --
			//-----------------------------------------------------------
			if (strcmp(ListTag, "Blendable") == 0)
			{
				uint32 num = nodeList->getChildCount();
				m_arrBSAnnotations.reserve(num);
				for (uint32 i = 0; i < num; i++)
				{
					XmlNodeRef nodeExample = nodeList->getChild(i);
					const char* ExampleTag = nodeExample->getTag();
					if (strcmp(ExampleTag, "Face") == 0)
					{
						BSBlendable face;

						const char* facePointNames[] = { "p0", "p1", "p2", "p3", "p4", "p5", "p6", "p7" };
						static_assert(CRY_ARRAY_COUNT(facePointNames) == CRY_ARRAY_COUNT(face.idx), "facePointNames[]'s size is expected to match BSBlendable::idx[]");

						for (size_t p = 0; p < CRY_ARRAY_COUNT(facePointNames); ++p)
						{
							if (nodeExample->getAttr(facePointNames[p], face.idx[p]))
							{
								if (face.idx[p] >= (numExamples + numPseudo))
								{
									m_Status.Format("Error: Blend annotation %d contains a reference to non-existing example with index %d.", int32(i), int32(face.idx[p]));
									gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
									return false;
								}
								face.num++;
							}
						}

						m_arrBSAnnotations.push_back(face);
					}
				}

				continue;
			}

			//-----------------------------------------------------------
			//---  load precomputed example grid                       --
			//-----------------------------------------------------------
			if (strcmp(ListTag, "VGrid") == 0)
			{
				VirtualExampleXML::ReadVGrid(nodeList, *this);
				continue;
			}

			//-----------------------------------------------------------
			//-- joint mask
			//-----------------------------------------------------------
			if (strcmp(ListTag, "JointList") == 0)
			{
				uint32 num = nodeList->getChildCount();
				m_jointList.resize(num);
				for (uint32 i = 0; i < num; ++i)
				{
					XmlNodeRef node = nodeList->getChild(i);
					const char* tag = node->getTag();
					if (strcmp(tag, "Joint") != 0)
					{
						((m_Status = "Error: The XML-Tag '") += tag) += "' is currently not supported";
						gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
						return 0;
					}

					const char* name = node->getAttr("Name");
					m_jointList[i] = CCrc32::Compute(name);
				}

				std::sort(m_jointList.begin(), m_jointList.end());
				continue;
			}

			if (strcmp(ListTag, "MotionCombination") == 0)
			{
				gEnv->pLog->LogError("CryAnimation: <MotionCombination> element found in bspace '%s'. Those are not supported anymore. Please use dedicated animation layers to achieve the same result instead.", pathnameLMG);
				continue;
			}
		}

	} //Paragroup

	//----------------------------------------------------------------------
	//----      check of this is a combined Blend-Space          -----------
	//----------------------------------------------------------------------
	if (strcmp(XMLTAG, "CombinedBlendSpace") == 0)
	{
		uint32 numChilds = root->getChildCount();
		for (uint32 c = 0; c < numChilds; c++)
		{
			XmlNodeRef nodeList = root->getChild(c);
			const char* ListTag = nodeList->getTag();

			if (strcmp(ListTag, "VEGPARAMS") == 0)
			{
				uint32 nFlags = 0;
				nodeList->getAttr("Idle2Move", nFlags);     //will go as soon as CryMannegin is up and running
				if (nFlags)
					m_VEG_Flags |= CA_VEG_I2M; //this is a case for CryMannequin
				continue;
			}

			//-----------------------------------------------------------
			//---  define dimensions of the LMG                       ---
			//-----------------------------------------------------------
			if (strcmp(ListTag, "Dimensions") == 0)
			{
				m_Dimensions = nodeList->getChildCount();
				if (m_Dimensions > 4)
				{
					m_Status = "Error: More than 4 dimensions per ParaGroup are not supported";
					gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
					return false;
				}

				for (uint32 d = 0; d < m_Dimensions; d++)
				{
					XmlNodeRef nodeExample = nodeList->getChild(d);
					const char* ExampleTag = nodeExample->getTag();
					if (strcmp(ExampleTag, "Param") == 0)
					{
						// Each dimension must have a parameter-name
						m_DimPara[d].m_strParaName = nodeExample->getAttr("name");

						// Check if the parameter-name is supported by the system
						bool supported = false;
						for (int iParam = 0; iParam < eMotionParamID_COUNT; ++iParam)
						{
							SMotionParameterDetails details;
							gEnv->pCharacterManager->GetMotionParameterDetails(details, static_cast<EMotionParamID>(iParam));

							if (details.name == m_DimPara[d].m_strParaName)
							{
								m_DimPara[d].m_ParaID = static_cast<EMotionParamID>(iParam);
								supported = true;
								break;
							}
						}

						if (!supported)
						{
							((m_Status = "Error: The parameter '") += m_DimPara[d].m_strParaName.c_str()) += "' is currently not supported";
							gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
							return false;
						}

						nodeExample->getAttr("ParaScale", m_DimPara[d].m_ParaScale);
						nodeExample->getAttr("ChooseBlendSpace", m_DimPara[d].m_ChooseBlendSpace);
						//special flags per-dimension
						m_DimPara[d].m_nDimensionFlags = 0;
						uint32 flag = 0;
						nodeExample->getAttr("locked", flag);
						if (flag)
							m_DimPara[d].m_nDimensionFlags |= CA_Dim_LockedParameter;
					}
				}
				continue;
			}

			//-----------------------------------------------------------
			//---  define the additional extraction parameters        ---
			//-----------------------------------------------------------
			if (strcmp(ListTag, "AdditionalExtraction") == 0)
			{
				m_ExtractionParams = nodeList->getChildCount();
				if (m_ExtractionParams > 4)
				{
					m_Status = "Error: More than 4 additional extraction parameters are not supported";
					gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
					return false;
				}
				for (uint32 d = 0; d < m_ExtractionParams; d++)
				{
					XmlNodeRef nodeExample = nodeList->getChild(d);
					const char* ExampleTag = nodeExample->getTag();
					if (strcmp(ExampleTag, "Param") == 0)
					{
						// Each dimension must have a parameter-name
						m_ExtPara[d].m_strParaName = nodeExample->getAttr("name");

						// Check if the parameter-name is supported by the system
						bool supported = false;
						for (int iParam = 0; iParam < eMotionParamID_COUNT; ++iParam)
						{
							SMotionParameterDetails details;
							gEnv->pCharacterManager->GetMotionParameterDetails(details, static_cast<EMotionParamID>(iParam));

							if ((details.flags & SMotionParameterDetails::ADDITIONAL_EXTRACTION) && (details.name == m_ExtPara[d].m_strParaName))
							{
								m_ExtPara[d].m_ParaID = static_cast<EMotionParamID>(iParam);
								supported = true;
								break;
							}
						}

						if (!supported)
						{
							((m_Status = "Error: The parameter '") += m_ExtPara[d].m_strParaName.c_str()) += "' is currently not supported";
							gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
							return false;
						}
					}
				}
				continue;
			}

			if (strcmp(ListTag, "BlendSpaces") == 0)
			{
				uint32 num = nodeList->getChildCount();
				m_arrCombinedBlendSpaces.resize(num);
				for (uint32 i = 0; i < num; i++)
				{
					XmlNodeRef nodeExample = nodeList->getChild(i);
					const char* ExampleTag = nodeExample->getTag();
					if (strcmp(ExampleTag, "BlendSpace") == 0)
					{
						string strFilePath = nodeExample->getAttr("aname");
						m_arrCombinedBlendSpaces[i].m_FilePath = strFilePath;
						uint32 nCRC32 = CCrc32::ComputeLowercase(strFilePath.c_str());
						m_arrCombinedBlendSpaces[i].m_FilePathCRC32 = nCRC32;
						uint32 numLMG = g_AnimationManager.m_arrGlobalLMG.size();
						for (uint32 id = 0; id < numLMG; id++)
						{
							if (g_AnimationManager.m_arrGlobalLMG[id].m_FilePathCRC32 == nCRC32)
							{
								uint32 isBSValid = g_AnimationManager.m_arrGlobalLMG[id].IsAssetLMGValid();
								if (isBSValid == 0)
								{
									const char* strBSFilePath = g_AnimationManager.m_arrGlobalLMG[id].GetFilePath();
									((m_Status = "Error: The included blend-space '") += strBSFilePath) += "' is not valid";
									gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
									return 0;
								}
								m_arrCombinedBlendSpaces[i].m_ParaGroupID = id;
								break;
							}
						}

						if (m_arrCombinedBlendSpaces[i].m_ParaGroupID < 0)
						{
							((m_Status = "Error: The included file '") += strFilePath) += "' was not found";
							gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
							return 0;
						}
					}
					else
					{
						((m_Status = "Error: The XML-Tag '") += ExampleTag) += "' is currently not supported";
						gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
						return 0;
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
				m_jointList.resize(num);
				for (uint32 i = 0; i < num; ++i)
				{
					XmlNodeRef node = nodeList->getChild(i);
					const char* tag = node->getTag();
					if (strcmp(tag, "Joint") != 0)
					{
						((m_Status = "Error: The XML-Tag '") += tag) += "' is currently not supported";
						gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
						return 0;
					}

					const char* name = node->getAttr("Name");
					m_jointList[i] = CCrc32::Compute(name);
				}

				std::sort(m_jointList.begin(), m_jointList.end());
				continue;
			}
		}
	}

	if (m_Dimensions == 0)
	{
		m_Status = "Error: ParaGroup has no dimensions specified";
		gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
		return false;
	}

	if (m_numExamples == 0 && m_arrCombinedBlendSpaces.size() == 0)
	{
		m_Status = "Error: ParaGroup has no examples specified";
		gEnv->pLog->LogError("CryAnimation %s: %s", m_Status.c_str(), pathnameLMG);
		return false;
	}

	OnAssetLMGValid(); //everything seems to be fine!!!
	return true;
}

#if BLENDSPACE_VISUALIZATION
void GlobalAnimationHeaderLMG::CreateInternalType_Para1D()
{
	//init one dimension
	m_Dimensions = 1;
	m_DimPara[0].m_strParaName = "BlendWeight";
	m_DimPara[0].m_ParaID = eMotionParamID_BlendWeight; //this is just a custom "Blend-Node"
	m_DimPara[0].m_min = -3.0f;
	m_DimPara[0].m_max = +4.0f;
	m_DimPara[0].m_cells = 29;

	//example-list
	m_numExamples = 2;
	m_arrParameter.resize(m_numExamples + 2); //2 examples + 2 pseudos
	//m_arrParameter[0].m_animName.SetName("stand_tac_walk_rifle_fwd_slow_3p_01");
	m_arrParameter[0].m_PreInitialized[0] = 1;
	m_arrParameter[0].m_Para.x = 0.0f;
	m_arrParameter[0].i0 = 0;
	m_arrParameter[0].w0 = 1.0f;
	m_arrParameter[0].i1 = 0;
	m_arrParameter[0].w1 = 0.0f;
	//m_arrParameter[1].m_animName.SetName("stand_tac_runTurn_rifle_lft_fast_3p_01");
	m_arrParameter[1].m_PreInitialized[0] = 1;
	m_arrParameter[1].m_Para.x = 1.0f;
	m_arrParameter[1].i0 = 1;
	m_arrParameter[1].w0 = 1.0f;
	m_arrParameter[1].i1 = 0;
	m_arrParameter[1].w1 = 0.0f;

	//pseudo example-list
	m_arrParameter[2].i0 = 0;
	m_arrParameter[2].i1 = 1;
	m_arrParameter[2].w0 = 4.2f;
	m_arrParameter[2].w1 = -3.2f;
	m_arrParameter[3].i0 = 1;
	m_arrParameter[3].i1 = 0;
	m_arrParameter[3].w0 = 4.2f;
	m_arrParameter[3].w1 = -3.2f;

	//set blend-annotation-list                          --
	m_arrBSAnnotations.reserve(3);
	BSBlendable face;
	face.idx[0] = 2;
	face.idx[1] = 0;
	face.num = 2;
	m_arrBSAnnotations.push_back(face);
	face.idx[0] = 0;
	face.idx[1] = 1;
	face.num = 2;
	m_arrBSAnnotations.push_back(face);
	face.idx[0] = 1;
	face.idx[1] = 3;
	face.num = 2;
	m_arrBSAnnotations.push_back(face);

	m_ExtractionParams = 4;
	m_ExtPara[0].m_ParaID = eMotionParamID_TravelSpeed;
	m_ExtPara[1].m_ParaID = eMotionParamID_TurnSpeed;
	m_ExtPara[2].m_ParaID = eMotionParamID_TravelSlope;
	m_ExtPara[3].m_ParaID = eMotionParamID_TravelAngle;

	OnAssetInternalType();
	OnAssetCreated();
	OnAssetLMG();
	OnAssetLMGValid();
}
#endif

#ifdef EDITOR_PCDEBUGCODE
bool GlobalAnimationHeaderLMG::Export2HTR(const char* szAnimationName, const char* savePath, const CDefaultSkeleton* pDefaultSkeleton, const CSkeletonAnim* pSkeletonAnim) const
{
	uint32 num = pSkeletonAnim->GetNumAnimsInFIFO(0);
	if (num == 0)
		return 0; //fail

	const CAnimation& Animation = pSkeletonAnim->GetAnimFromFIFO(0, 0);
	if (Animation.GetParametricSampler() == 0)
		return 0; //fail

	//-------------------------------------------------------------------

	std::vector<string> jointNameArray;
	std::vector<string> jointParentArray;

	const QuatT* parrDefJoints = &pDefaultSkeleton->m_poseDefaultData.GetJointsRelative()[0];
	uint32 numJoints = pDefaultSkeleton->m_arrModelJoints.size();
	for (uint32 j = 0; j < numJoints; j++)
	{
		const CDefaultSkeleton::SJoint* pJoint = &pDefaultSkeleton->m_arrModelJoints[j];
		assert(pJoint);
		jointNameArray.push_back(pJoint->m_strJointName.c_str());

		int16 parentID = pJoint->m_idxParent;
		if (parentID == -1)
			jointParentArray.push_back("INVALID");
		else
		{
			const CDefaultSkeleton::SJoint* parentJoint = &pDefaultSkeleton->m_arrModelJoints[parentID];
			if (parentJoint)
				jointParentArray.push_back(parentJoint->m_strJointName.c_str());
		}
	}

	//fetch and sum up all CAFs in this LMG
	const CAnimationSet* pAnimationSet = pDefaultSkeleton->m_pAnimationSet;
	SParametricSamplerInternal& lmg = *(SParametricSamplerInternal*)(Animation.GetParametricSampler());
	f32 fTWNDeltaTime = lmg.Parameterizer(pAnimationSet, pDefaultSkeleton, Animation, 1.0f / ANIMATION_30Hz, 1.0f, 0);

	uint32 nFrames = uint32(1.0f / fTWNDeltaTime + 1.5f);
	f32 timestep = 1.0f / f32(nFrames - 1);

	std::vector<DynArray<QuatT>> arrAnimation;

	arrAnimation.resize(numJoints);
	for (uint32 j = 0; j < numJoints; j++)
		arrAnimation[j].resize(nFrames);

	for (uint32 j = 0; j < numJoints; j++)
	{
		for (uint32 k = 0; k < nFrames; k++)
		{
			arrAnimation[j][k].q.w = 0.0f;
			arrAnimation[j][k].q.v.x = 0.0f;
			arrAnimation[j][k].q.v.y = 0.0f;
			arrAnimation[j][k].q.v.z = 0.0f;
			arrAnimation[j][k].t.x = 0.0f;
			arrAnimation[j][k].t.y = 0.0f;
			arrAnimation[j][k].t.z = 0.0f;
		}
	}

	for (uint32 a = 0; a < lmg.m_numExamples; a++)
	{
		int nAnimID = lmg.m_nAnimID[a];
		f32 fWeight = lmg.m_fBlendWeight[a];
		if (fWeight == 0.0f)
			continue;
		const ModelAnimationHeader* pAnim = pAnimationSet->GetModelAnimationHeader(nAnimID);
		assert(pAnim->m_nAssetType == CAF_File);
		GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[pAnim->m_nGlobalAnimId];

		const CDefaultSkeleton::SJoint* pJointRoot = &pDefaultSkeleton->m_arrModelJoints[0];
		assert(pJointRoot);
		IController* pControllerRoot = rCAF.GetControllerByJointCRC32(pJointRoot->m_nJointCRC32);
		f32 t = timestep;
		arrAnimation[0][0].q.SetIdentity();
		for (uint32 k = 1; k < nFrames; k++)
		{
			QuatT qtold = parrDefJoints[0];
			QuatT qtnew = parrDefJoints[0];
			if (pControllerRoot)
			{
				pControllerRoot->GetOP(rCAF.NTime2KTime(t - timestep), qtold.q, qtold.t);
				qtold.q *= fsgnnz(parrDefJoints[0].q | qtold.q);  //this could be optimized at loading-time
				pControllerRoot->GetOP(rCAF.NTime2KTime(t), qtnew.q, qtnew.t);
				qtnew.q *= fsgnnz(parrDefJoints[0].q | qtnew.q);  //this could be optimized at loading-time
			}
			QuatT rel = qtold.GetInverted() * qtnew;
			arrAnimation[0][k].q += fWeight * rel.q;
			arrAnimation[0][k].t += fWeight * rel.t;
			t += timestep;
		}

		for (uint32 j = 1; j < numJoints; j++)
		{
			const CDefaultSkeleton::SJoint* pJoint = &pDefaultSkeleton->m_arrModelJoints[j];
			assert(pJoint);
			IController* pController = rCAF.GetControllerByJointCRC32(pJoint->m_nJointCRC32);
			t = 0.0f;
			for (uint32 k = 0; k < nFrames; k++)
			{
				QuatT qt = parrDefJoints[j];
				if (pController)
				{
					pController->GetOP(rCAF.NTime2KTime(t), qt.q, qt.t);
					qt.q *= fsgnnz(parrDefJoints[j].q | qt.q);  //this could be optimized at loading-time
				}
				arrAnimation[j][k].q += fWeight * qt.q;
				arrAnimation[j][k].t += fWeight * qt.t;
				t += timestep;
			}
		}
	}

	for (uint32 j = 0; j < numJoints; j++)
	{
		for (uint32 k = 0; k < nFrames; k++)
			arrAnimation[j][k].q.Normalize();
	}

	for (uint32 k = 1; k < nFrames; k++)
	{
		Vec3 abs = arrAnimation[0][k - 1].t;
		Vec3 rel = arrAnimation[0][k].t;
		arrAnimation[0][k] = arrAnimation[0][k - 1] * arrAnimation[0][k];
		Vec3 abs1 = arrAnimation[0][k].t;
		uint32 ddd = 0;
	}

	bool htr = GlobalAnimationHeaderCAF::SaveHTR(szAnimationName, savePath, jointNameArray, jointParentArray, arrAnimation, parrDefJoints);
	bool caf = GlobalAnimationHeaderCAF::SaveICAF(szAnimationName, savePath, jointNameArray, arrAnimation);
	return true;
}

bool GlobalAnimationHeaderLMG::ExportVGrid() const
{
	const bool isCombBSpace = (!m_arrCombinedBlendSpaces.empty());
	if (isCombBSpace)
	{
		return false;
	}

	const char* const filepath = GetFilePath();
	XmlNodeRef xmlRoot = gEnv->pSystem->LoadXmlFromFile(filepath);
	if (!xmlRoot)
	{
		gEnv->pLog->LogError("CryAnimation: Failed to load xml file '%s', will try to create it", filepath);
		return false;
	}

	XmlNodeRef xmlVGrid = xmlRoot->findChild("VGrid");
	if (xmlVGrid)
	{
		xmlRoot->removeChild(xmlVGrid);
	}

	xmlVGrid = xmlRoot->createNode("VGrid");
	xmlRoot->addChild(xmlVGrid);
	const bool writeVGridSuccess = VirtualExampleXML::WriteVGrid(xmlVGrid, *this);
	if (!writeVGridSuccess)
	{
		return false;
	}

	const bool saveFileSuccess = xmlRoot->saveToFile(filepath);
	if (!saveFileSuccess)
	{
		gEnv->pLog->LogError("CryAnimation: Failed to save xml file '%s'", filepath);
		return false;
	}

	return true;
}
#endif

void GlobalAnimationHeaderLMG::ParameterExtraction(const CAnimationSet* pAnimationSet, const CDefaultSkeleton* pDefaultSkeleton, uint32 nParaID, uint32 d)
{
	if (nParaID == eMotionParamID_TravelSpeed)
		Init_MoveSpeed(pAnimationSet, pDefaultSkeleton, d);
	if (nParaID == eMotionParamID_TurnSpeed)
		Init_TurnSpeed(pAnimationSet, pDefaultSkeleton, d);
	if (nParaID == eMotionParamID_TurnAngle)
		Init_TurnAngle(pAnimationSet, pDefaultSkeleton, d);
	if (nParaID == eMotionParamID_TravelAngle)
		Init_TravelAngle(pAnimationSet, pDefaultSkeleton, d);
	if (nParaID == eMotionParamID_TravelSlope)
		Init_SlopeAngle(pAnimationSet, pDefaultSkeleton, d);
	if (nParaID == eMotionParamID_TravelDist)
		Init_TravelDist(pAnimationSet, pDefaultSkeleton, d);
}

void GlobalAnimationHeaderLMG::Init_MoveSpeed(const CAnimationSet* pAnimationSet, const CDefaultSkeleton* pDefaultSkeleton, uint32 dim)
{
	uint32 init_count = 0;
	uint32 numAssets = m_numExamples;//m_arrBSAnimations2.size();
	for (uint32 i = 0; i < numAssets; i++)
	{
		int32 animID = pAnimationSet->GetAnimIDByCRC(m_arrParameter[i].m_animName.m_CRC32);
		assert(animID >= 0);
		int32 globalID = pAnimationSet->GetGlobalIDByAnimID_Fast(animID);
		assert(globalID >= 0);
		if (globalID < 0)
			break;
		GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[globalID];
		assert(rCAF.IsAssetLoaded());
		if (rCAF.IsAssetLoaded() == 0)
		{
			CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, "Asset for parameter-extraction not in memory: '%s'", rCAF.GetFilePath());
			break;
		}

		const CDefaultSkeleton::SJoint* pRootJoint = &pDefaultSkeleton->m_arrModelJoints[0];
		IController* pController = rCAF.GetControllerByJointCRC32(pRootJoint->m_nJointCRC32);
		if (pController == 0)
			continue;
		f32 duration = rCAF.m_fEndSec - rCAF.m_fStartSec;
		uint32 numKeys = uint32(duration * ANIMATION_30Hz + 1);
		if (numKeys == 1)
			continue;
		init_count++;
		if (m_arrParameter[i].m_PreInitialized[dim])
			continue;  //already pre-initialized
		DynArray<Vec3> root_keys;
		root_keys.resize(numKeys);

		f32 fk = rCAF.m_fStartSec * ANIMATION_30Hz;
		for (uint32 k = 0; k < numKeys; k++, fk += 1.0f)
			pController->GetP(fk, root_keys[k]);

		uint32 skey = uint32(m_DimPara[dim].m_skey * (numKeys - 1));
		uint32 ekey = uint32(m_DimPara[dim].m_ekey * (numKeys - 1));

		uint32 poses = 0;
		f32 movespeed = 0.0f;
		for (uint32 k = skey; k < ekey; k++)
		{
			movespeed += (root_keys[k + 0] - root_keys[k + 1]).GetLength() * ANIMATION_30Hz;
			poses++;
		}
		if (poses)
			movespeed /= poses;

		m_arrParameter[i].m_Para[dim] = movespeed * m_arrParameter[i].m_fPlaybackScale;
	}
	if (init_count == numAssets)
		m_DimPara[dim].m_nInitialized = true;
}

void GlobalAnimationHeaderLMG::Init_TurnSpeed(const CAnimationSet* pAnimationSet, const CDefaultSkeleton* pDefaultSkeleton, uint32 dim)
{
	uint32 init_count = 0;
	uint32 numAssets = m_numExamples;//m_arrBSAnimations2.size();
	for (uint32 i = 0; i < numAssets; i++)
	{
		int32 animID = pAnimationSet->GetAnimIDByCRC(m_arrParameter[i].m_animName.m_CRC32);
		assert(animID >= 0);
		int32 globalID = pAnimationSet->GetGlobalIDByAnimID_Fast(animID);
		assert(globalID >= 0);
		if (globalID < 0)
			break;
		GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[globalID];
		assert(rCAF.IsAssetLoaded());
		if (rCAF.IsAssetLoaded() == 0)
		{
			CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, "Asset for parameter-extraction not in memory: '%s'", rCAF.GetFilePath());
			break;
		}

		const CDefaultSkeleton::SJoint* pRootJoint = &pDefaultSkeleton->m_arrModelJoints[0];
		IController* pController = rCAF.GetControllerByJointCRC32(pRootJoint->m_nJointCRC32);
		if (pController == 0)
			continue;
		f32 duration = rCAF.m_fEndSec - rCAF.m_fStartSec;
		uint32 numKeys = uint32(duration * ANIMATION_30Hz + 1);
		if (numKeys == 1)
			continue;
		init_count++;
		if (m_arrParameter[i].m_PreInitialized[dim])
			continue;  //already pre-initialized

		DynArray<Vec3> pos_keys;
		DynArray<Quat> rot_keys;
		pos_keys.resize(numKeys);
		rot_keys.resize(numKeys);
		f32 fk = rCAF.m_fStartSec * ANIMATION_30Hz;
		for (uint32 k = 0; k < numKeys; k++, fk += 1.0f)
			pController->GetOP(fk, rot_keys[k], pos_keys[k]);

		uint32 skey = uint32(m_DimPara[dim].m_skey * (numKeys - 1));
		uint32 ekey = uint32(m_DimPara[dim].m_ekey * (numKeys - 1));
		uint32 poses = 0;
		f32 turnspeed = 0.0f;
		for (uint32 k = 0; k < ekey; k++)
		{
			Vec3 v0 = rot_keys[k + 0].GetColumn1();
			Vec3 v1 = rot_keys[k + 1].GetColumn1();
			turnspeed += Ang3::CreateRadZ(v0, v1) * ANIMATION_30Hz;
			poses++;
		}
		if (poses)
			turnspeed /= poses;

		m_arrParameter[i].m_Para[dim] = turnspeed;
	}
	if (init_count == numAssets)
		m_DimPara[dim].m_nInitialized = true;
}

void GlobalAnimationHeaderLMG::Init_TurnAngle(const CAnimationSet* pAnimationSet, const CDefaultSkeleton* pDefaultSkeleton, uint32 dim)
{
	uint32 init_count = 0;
	uint32 numAssets = m_numExamples; //m_arrBSAnimations2.size();
	for (uint32 i = 0; i < numAssets; i++)
	{
		int32 animID = pAnimationSet->GetAnimIDByCRC(m_arrParameter[i].m_animName.m_CRC32);
		assert(animID >= 0);
		int32 globalID = pAnimationSet->GetGlobalIDByAnimID_Fast(animID);
		assert(globalID >= 0);
		if (globalID < 0)
			break;

		GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[globalID];
		assert(rCAF.IsAssetLoaded());
		if (rCAF.IsAssetLoaded() == 0)
		{
			CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, "Asset for parameter-extraction not in memory: '%s'", rCAF.GetFilePath());
			break;
		}

		const CDefaultSkeleton::SJoint* pRootJoint = &pDefaultSkeleton->m_arrModelJoints[0];
		IController* pController = rCAF.GetControllerByJointCRC32(pRootJoint->m_nJointCRC32);
		if (pController == 0)
			continue;

		init_count++;
		if (m_arrParameter[i].m_PreInitialized[dim])
			continue;  //already pre-initialized

		f32 duration = rCAF.m_fEndSec - rCAF.m_fStartSec;
		uint32 numKeys = uint32(duration * ANIMATION_30Hz + 1);
		if (numKeys == 1)
			continue;

		DynArray<Quat> rot_keys;
		rot_keys.resize(numKeys);
		f32 fk = rCAF.m_fStartSec * ANIMATION_30Hz;
		for (uint32 k = 0; k < numKeys; k++, fk += 1.0f)
			pController->GetO(fk, rot_keys[k]);

		uint32 skey = uint32(m_DimPara[dim].m_skey * (numKeys - 1));
		uint32 ekey = uint32(m_DimPara[dim].m_ekey * (numKeys - 1));
		f32 turnangle = 0.0f;
		for (uint32 k = 0; k < ekey; k++)
		{
			Vec3 v0 = rot_keys[k + 0].GetColumn1();
			Vec3 v1 = rot_keys[k + 1].GetColumn1();
			turnangle += Ang3::CreateRadZ(v0, v1);
		}

		m_arrParameter[i].m_Para[dim] = turnangle;
	}

	if (init_count == numAssets)
		m_DimPara[dim].m_nInitialized = true;
}

void GlobalAnimationHeaderLMG::Init_TravelAngle(const CAnimationSet* pAnimationSet, const CDefaultSkeleton* pDefaultSkeleton, uint32 dim)
{
	uint32 init_count = 0;
	uint32 numAssets = m_numExamples;//m_arrBSAnimations2.size();
	for (uint32 i = 0; i < numAssets; i++)
	{
		int32 animID = pAnimationSet->GetAnimIDByCRC(m_arrParameter[i].m_animName.m_CRC32);
		assert(animID >= 0);
		int32 globalID = pAnimationSet->GetGlobalIDByAnimID_Fast(animID);
		assert(globalID >= 0);
		if (globalID < 0)
			break;
		GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[globalID];
		assert(rCAF.IsAssetLoaded());
		if (rCAF.IsAssetLoaded() == 0)
		{
			CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, "Asset for parameter-extraction not in memory: '%s'", rCAF.GetFilePath());
			break;
		}

		const CDefaultSkeleton::SJoint* pRootJoint = &pDefaultSkeleton->m_arrModelJoints[0];
		IController* pController = rCAF.GetControllerByJointCRC32(pRootJoint->m_nJointCRC32);
		if (pController == 0)
			continue;
		f32 duration = rCAF.m_fEndSec - rCAF.m_fStartSec;
		uint32 numKeys = uint32(duration * ANIMATION_30Hz + 1);
		if (numKeys == 1)
			continue;
		init_count++;
		if (m_arrParameter[i].m_PreInitialized[dim])
			continue;  //already pre-initialized
		DynArray<Vec3> pos_keys;
		DynArray<Quat> rot_keys;
		pos_keys.resize(numKeys);
		rot_keys.resize(numKeys);
		f32 fk = rCAF.m_fStartSec * ANIMATION_30Hz;
		for (uint32 k = 0; k < numKeys; k++, fk += 1.0f)
			pController->GetOP(fk, rot_keys[k], pos_keys[k]);

		uint32 skey = uint32(m_DimPara[dim].m_skey * (numKeys - 1));
		uint32 ekey = uint32(m_DimPara[dim].m_ekey * (numKeys - 1));
		f32 fTravelAngle = 0.0f;
		Vec3 totalMovement(0.0f, 0.0f, 0.0f);
		for (uint32 k = skey; k < ekey; k++)
		{
			const Vec3 movement = pos_keys[k + 1] - pos_keys[k + 0];
			totalMovement += !rot_keys[k + 1] * movement;
		}

		Vec3 v0(0, 1, 0);
		Vec3 v1 = (totalMovement).GetNormalizedSafe(Vec3(0, 1, 0));
		m_arrParameter[i].m_Para[dim] = Ang3::CreateRadZ(v0, v1);
	}
	if (init_count == numAssets)
		m_DimPara[dim].m_nInitialized = true;
}

void GlobalAnimationHeaderLMG::Init_SlopeAngle(const CAnimationSet* pAnimationSet, const CDefaultSkeleton* pDefaultSkeleton, uint32 dim)
{
	uint32 init_count = 0;
	uint32 numAssets = m_numExamples;//m_arrBSAnimations2.size();
	for (uint32 i = 0; i < numAssets; i++)
	{
		int32 animID = pAnimationSet->GetAnimIDByCRC(m_arrParameter[i].m_animName.m_CRC32);
		assert(animID >= 0);
		int32 globalID = pAnimationSet->GetGlobalIDByAnimID_Fast(animID);
		assert(globalID >= 0);
		if (globalID < 0)
			break;
		GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[globalID];
		assert(rCAF.IsAssetLoaded());
		if (rCAF.IsAssetLoaded() == 0)
		{
			CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, "Asset for parameter-extraction not in memory: '%s'", rCAF.GetFilePath());
			break;
		}

		const CDefaultSkeleton::SJoint* pRootJoint = &pDefaultSkeleton->m_arrModelJoints[0];
		IController* pController = rCAF.GetControllerByJointCRC32(pRootJoint->m_nJointCRC32);
		if (pController == 0)
			continue;
		f32 duration = rCAF.m_fEndSec - rCAF.m_fStartSec;
		uint32 numKeys = uint32(duration * ANIMATION_30Hz + 1);
		if (numKeys == 1)
			continue;
		init_count++;
		if (m_arrParameter[i].m_PreInitialized[dim])
			continue;  //already pre-initialized
		DynArray<Vec3> pos_keys;
		DynArray<Quat> rot_keys;
		pos_keys.resize(numKeys);
		rot_keys.resize(numKeys);
		f32 fk = rCAF.m_fStartSec * ANIMATION_30Hz;
		;
		for (uint32 k = 0; k < numKeys; k++, fk += 1.0f)
			pController->GetOP(fk, rot_keys[k], pos_keys[k]);

		uint32 skey = uint32(m_DimPara[dim].m_skey * (numKeys - 1));
		uint32 ekey = uint32(m_DimPara[dim].m_ekey * (numKeys - 1));
		uint32 poses = 0;
		f32 fSlopeAngle = 0.0f;
		for (uint32 k = 0; k < ekey; k++)
		{
			Vec3 rel = (pos_keys[k + 1] - pos_keys[k + 0]).GetNormalizedSafe(Vec3(0, 1, 0));
			f32 tdir = atan2_tpl(-rel.x, rel.y);
			Vec3 v = rel * Matrix33::CreateRotationZ(tdir);
			fSlopeAngle += atan2_tpl(v.z, v.y);
			poses++;
		}
		if (poses)
			fSlopeAngle /= poses;

		m_arrParameter[i].m_Para[dim] = fSlopeAngle;
	}
	if (init_count == numAssets)
		m_DimPara[dim].m_nInitialized = true;
}

void GlobalAnimationHeaderLMG::Init_TravelDist(const CAnimationSet* pAnimationSet, const CDefaultSkeleton* pDefaultSkeleton, uint32 dim)
{
	uint32 init_count = 0;
	uint32 numAssets = m_numExamples; //m_arrBSAnimations.size();
	for (uint32 i = 0; i < numAssets; i++)
	{
		int32 animID = pAnimationSet->GetAnimIDByCRC(m_arrParameter[i].m_animName.m_CRC32);
		assert(animID >= 0);
		int32 globalID = pAnimationSet->GetGlobalIDByAnimID_Fast(animID);
		assert(globalID >= 0);
		if (globalID < 0)
			break;
		GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[globalID];
		assert(rCAF.IsAssetLoaded());
		if (rCAF.IsAssetLoaded() == 0)
		{
			CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_ERROR, "Asset for parameter-extraction not in memory: '%s'", rCAF.GetFilePath());
			break;
		}

		const CDefaultSkeleton::SJoint* pRootJoint = &pDefaultSkeleton->m_arrModelJoints[0];
		IController* pController = rCAF.GetControllerByJointCRC32(pRootJoint->m_nJointCRC32);
		if (pController == 0)
			continue;
		f32 duration = rCAF.m_fEndSec - rCAF.m_fStartSec;
		uint32 numKeys = uint32(duration * ANIMATION_30Hz + 1);
		if (numKeys == 1)
			continue;
		init_count++;
		if (m_arrParameter[i].m_PreInitialized[dim])
			continue;  //already pre-initialized

		QuatT key0;
		pController->GetOP(0.0f, key0.q, key0.t);
		QuatT key1;
		pController->GetOP(rCAF.NTime2KTime(1), key1.q, key1.t);

		f32 fTravelDist = (key1.t - key0.t).GetLength();

		f32 anf3 = m_arrParameter[i].m_Para[dim];
		m_arrParameter[i].m_Para[dim] = fTravelDist;
	}
	if (init_count == numAssets)
		m_DimPara[dim].m_nInitialized = true;
}

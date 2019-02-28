// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlowGraphVariables.h"
#include "FlowGraph.h"
#include "Objects/EntityObject.h"
#include "Util/Variable.h"

#include "UIEnumsDatabase.h"

#include <CryMovie/IMovieSystem.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryAudio/Dialog/IDialogSystem.h>

#include <CryGame/IGameFramework.h>
#include <ICryMannequin.h>
#include <CryAnimation/IAttachment.h>
#include <CryFlowGraph/IFlowGraphModuleManager.h>
#include <CryAISystem/ICommunicationManager.h>
#include <CrySystem/Scaleform/IFlashUI.h>
#include <IVehicleSystem.h>

namespace
{
typedef IVariable::IGetCustomItems::SItem SItem;

//////////////////////////////////////////////////////////////////////////
bool ChooseDialog(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	IDialogSystem* pDS = gEnv->pDialogSystem;
	if (!pDS)
		return false;

	IDialogScriptIteratorPtr pIter = pDS->CreateScriptIterator();
	if (!pIter)
		return false;

	IDialogScriptIterator::SDialogScript script;
	while (pIter->Next(script))
	{
		SItem item;
		item.name = script.id;
		item.desc = _T("Script: ");
		item.desc += script.id;
		if (script.bIsLegacyExcel)
		{
			item.desc += _T(" [Excel]");
		}
		item.desc.AppendFormat(" - Required Actors: %d", script.numRequiredActors);
		if (script.desc && script.desc[0] != '\0')
		{
			item.desc += _T("\r\nDescription: ");
			item.desc += script.desc;
		}
		outItems.push_back(item);
	}
	outText = _T("Choose Dialog");
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool ChooseSequence(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	IMovieSystem* pMovieSys = GetIEditorImpl()->GetMovieSystem();
	for (int i = 0; i < pMovieSys->GetNumSequences(); ++i)
	{
		IAnimSequence* pSeq = pMovieSys->GetSequence(i);
		string seqName = pSeq->GetName();
		outItems.push_back(SItem(seqName.c_str()));
	}
	outText = _T("Choose Sequence");
	return true;
}

//////////////////////////////////////////////////////////////////////////
template<typename T>
bool GetValue(CFlowNodeGetCustomItemsBase* pGetCustomItems, const string& portName, T& value)
{
	CFlowNode* pFlowNode = pGetCustomItems->GetFlowNode();
	if (pFlowNode == 0)
		return false;

	assert(pFlowNode != 0);

	// Get the CryAction FlowGraph
	IFlowGraph* pGameFG = pFlowNode->GetIFlowGraph();

	CHyperNodePort* pPort = pFlowNode->FindPort(portName, true);
	if (!pPort)
		return false;
	const TFlowInputData* pFlowData = pGameFG->GetInputValue(pFlowNode->GetFlowNodeId(), pPort->nPortIndex);
	if (!pFlowData)
		return false;
	assert(pFlowData != 0);

	const bool success = pFlowData->GetValueWithConversion(value);
	return success;
}

typedef std::map<string, string> TParamMap;
bool GetParamMap(CFlowNodeGetCustomItemsBase* pGetCustomItems, TParamMap& outMap)
{
	CFlowNode* pFlowNode = pGetCustomItems->GetFlowNode();
	assert(pFlowNode != 0);

	const string& uiConfig = pGetCustomItems->GetUIConfig();
	// fill in Enum Pairs
	const string& values = uiConfig;
	int pos = 0;
	string resToken = TokenizeString(values, " ,", pos);
	while (!resToken.IsEmpty())
	{
		string str = resToken;
		string value = str;
		int pos_e = str.Find('=');
		if (pos_e >= 0)
		{
			value = str.Mid(pos_e + 1);
			str = str.Mid(0, pos_e);
		}
		outMap[str] = value;
		resToken = TokenizeString(values, " ,", pos);
	}
	;
	return outMap.empty() == false;
}

IEntity* GetRefEntityByUIConfig(CFlowNodeGetCustomItemsBase* pGetCustomItems, const string& refEntity)
{
	CFlowNode* pFlowNode = pGetCustomItems->GetFlowNode();
	assert(pFlowNode != 0);

	// If the node is part of an AI Action Graph we always use the selected entity
	CHyperFlowGraph* pFlowGraph = static_cast<CHyperFlowGraph*>(pFlowNode->GetGraph());
	if (pFlowGraph && pFlowGraph->GetAIAction())
	{
		IEntity* pSelEntity = 0;
		CBaseObject* pObject = GetIEditorImpl()->GetSelectedObject();
		if (pObject != 0 && pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* pCEntity = static_cast<CEntityObject*>(pObject);
			pSelEntity = pCEntity->GetIEntity();
		}
		if (pSelEntity == 0)
		{
			Warning(_T("FlowGraph: Please select first an Entity to be used as reference for this AI Action."));
		}
		return pSelEntity;
	}

	// Get the CryAction FlowGraph
	IFlowGraph* pGameFG = pGetCustomItems->GetFlowNode()->GetIFlowGraph();

	EntityId targetEntityId;
	string targetEntityPort;
	const string& uiConfig = pGetCustomItems->GetUIConfig();
	int pos = uiConfig.Find(refEntity);
	if (pos >= 0)
	{
		targetEntityPort = uiConfig.Mid(pos + refEntity.GetLength());
	}
	else
	{
		targetEntityPort = "entityId";
	}

	if (targetEntityPort == "entityId")
	{
		// use normal target entity of flownode as target
		targetEntityId = pGameFG->GetEntityId(pFlowNode->GetFlowNodeId());
		if (targetEntityId == 0)
		{
			Warning(_T("FlowGraph: No valid Target Entity assigned to node"));
			return 0;
		}
	}
	else
	{
		// use entity in port targetEntity as target
		CHyperNodePort* pPort = pFlowNode->FindPort(targetEntityPort, true);
		if (!pPort)
		{
			Warning(_T("FlowGraphVariables.cpp: Internal error - Cannot resolve port '%s' on ref-lookup. Check C++ Code!"), targetEntityPort.GetString());
			return 0;
		}
		const TFlowInputData* pFlowData = pGameFG->GetInputValue(pFlowNode->GetFlowNodeId(), pPort->nPortIndex);
		assert(pFlowData != 0);
		const bool success = pFlowData->GetValueWithConversion(targetEntityId);
		if (!success || targetEntityId == 0)
		{
			Warning(_T("FlowGraph: No valid Target Entity set on port '%s'"), targetEntityPort.GetString());
			return 0;
		}
	}

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(targetEntityId);
	if (!pEntity)
	{
		Warning(_T("FlowGraph: Cannot find entity with id %u, set on port '%s'"), targetEntityId, targetEntityPort.GetString());
		return 0;
	}

	return pEntity;
}

IEntity* GetNodeEntity(IVariable* pVar, CFlowNodeGetCustomItemsBase*& pGetCustomItems)
{
	pGetCustomItems = static_cast<CFlowNodeGetCustomItemsBase*>(pVar->GetUserData());
	CRY_ASSERT(pGetCustomItems != NULL);

	return GetRefEntityByUIConfig(pGetCustomItems, "ref_entity=");
}

//////////////////////////////////////////////////////////////////////////
bool ChooseCharacterAnimation(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	CFlowNodeGetCustomItemsBase* pGetCustomItems;
	IEntity* pEntity = GetNodeEntity(pVar, pGetCustomItems);
	if (!pEntity)
	{
		return false;
	}

	ICharacterInstance* pCharacter = pEntity->GetCharacter(0);
	if (pCharacter == 0)
		return false;

	IAnimationSet* pAnimSet = pCharacter->GetIAnimationSet();
	if (pAnimSet == 0)
		return false;

	const uint32 numAnims = pAnimSet->GetAnimationCount();
	for (int i = 0; i < numAnims; ++i)
	{
		const char* pName = pAnimSet->GetNameByAnimID(i);
		if (pName != 0)
		{
			outItems.push_back(SItem(pName));
		}
	}
	outText = _T("Choose Animation");
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool ChooseBone(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	CFlowNodeGetCustomItemsBase* pGetCustomItems;
	IEntity* pEntity = GetNodeEntity(pVar, pGetCustomItems);
	if (!pEntity)
	{
		return false;
	}

	ICharacterInstance* pCharacter = pEntity->GetCharacter(0);
	if (pCharacter == 0)
		return false;

	IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
	const uint32 numJoints = rIDefaultSkeleton.GetJointCount();
	for (uint32 i = 0; i < numJoints; ++i)
	{
		const char* pName = rIDefaultSkeleton.GetJointNameByID(i);
		if (pName != 0)
		{
			outItems.push_back(SItem(pName));
		}
	}
	outText = _T("Choose Bone");
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool ChooseAttachment(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	CFlowNodeGetCustomItemsBase* pGetCustomItems;
	IEntity* pEntity = GetNodeEntity(pVar, pGetCustomItems);
	if (!pEntity)
	{
		return false;
	}

	ICharacterInstance* pCharacter = pEntity->GetCharacter(0);
	if (pCharacter == 0)
		return false;

	IAttachmentManager* pMgr = pCharacter->GetIAttachmentManager();
	if (pMgr == 0)
		return false;

	const int32 numAttachment = pMgr->GetAttachmentCount();
	for (uint32 i = 0; i < numAttachment; ++i)
	{
		IAttachment* pAttachment = pMgr->GetInterfaceByIndex(i);
		if (pAttachment != 0)
		{
			outItems.push_back(SItem(pAttachment->GetName()));
		}
	}
	outText = _T("Choose Attachment");
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool FragmentIDContainsLoopingFragments(const IActionController* pActionController, FragmentID fragmentID)
{
	const IScope* pScope = pActionController->GetScope(0);
	if (!pScope)
		return false;
	const IAnimationDatabase* pDatabase = &pScope->GetDatabase();
	if (!pDatabase)
		return false;

	const uint32 numTagSets = pDatabase->GetTotalTagSets(fragmentID);
	for (uint32 tagSetIndex = 0; tagSetIndex < numTagSets; ++tagSetIndex)
	{
		SFragTagState tagSetTagState;
		uint32 numOptions = pDatabase->GetTagSetInfo(fragmentID, tagSetIndex, tagSetTagState);
		if (tagSetTagState.fragmentTags == TAG_STATE_EMPTY)
		{
			for (uint32 optionIndex = 0; optionIndex < numOptions; ++optionIndex)
			{
				const CFragment* pFragment = pDatabase->GetEntry(fragmentID, tagSetIndex, optionIndex);
				if (!pFragment->m_animLayers.empty())
				{
					const TAnimClipSequence& firstClipSequence = pFragment->m_animLayers[0];
					if (!firstClipSequence.empty())
					{
						const SAnimClip& lastAnimClip = firstClipSequence[firstClipSequence.size() - 1];
						if (lastAnimClip.animation.flags & CA_LOOP_ANIMATION)
						{
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
// method is one of [0->All FragmentIDs, 1->OneShot, 2->Looping]
bool IsFragmentIDAllowedForMethod(const IActionController* pActionController, FragmentID fragmentID, unsigned int method)
{
	const SControllerDef& controllerDef = pActionController->GetContext().controllerDef;
	const CTagDefinition& fragmentIDs = controllerDef.m_fragmentIDs;

	if (!fragmentIDs.IsValidTagID(fragmentID))
		return false;

	if (method == 0)
		return true;

	if (method > 2)
		return false;

	const bool isPersistent = controllerDef.GetFragmentDef(fragmentID).flags & SFragmentDef::PERSISTENT;
	const bool isLooping = isPersistent || FragmentIDContainsLoopingFragments(pActionController, fragmentID);
	const bool shouldBeLooping = (method == 2);
	return (shouldBeLooping == isLooping);
}

// Chooses from AnimationState
// UIConfig can contain a 'ref_entity=[portname]' entry to specify the entity for AnimationGraph lookups
// if bUseEx is true, an optional [signal] port will be queried to choose from [0->States/FragmentIDs, 1->Signal, 2->Action]
bool DoChooseAnimationState(IVariable* pVar, std::vector<SItem>& outItems, string& outText, bool bUseEx)
{
	CFlowNodeGetCustomItemsBase* pGetCustomItems;
	IEntity* pEntity = GetNodeEntity(pVar, pGetCustomItems);
	if (!pEntity)
	{
		return false;
	}

	int method = 0;
	if (bUseEx)
	{
		if (!GetValue(pGetCustomItems, "signal", method))
		{
			bool oneShot = true;
			GetValue(pGetCustomItems, "OneShot", oneShot);
			method = oneShot ? 1 : 2;
		}
	}

	if (gEnv->pGameFramework)
	{
		IMannequin& mannequin = gEnv->pGameFramework->GetMannequinInterface();
		IActionController* pActionController = mannequin.FindActionController(*pEntity);
		if (pActionController)
		{
			const CTagDefinition& fragmentIDs = pActionController->GetContext().controllerDef.m_fragmentIDs;
			for (FragmentID fragmentID = 0; fragmentID < fragmentIDs.GetNum(); ++fragmentID)
			{
				if (IsFragmentIDAllowedForMethod(pActionController, fragmentID, method))
				{
					const char* szFragmentName = fragmentIDs.GetTagName(fragmentID);
					outItems.push_back(SItem(szFragmentName));
				}
			}
			outText = _T("Choose Mannequin FragmentID");
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool ChooseAnimationState(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	return DoChooseAnimationState(pVar, outItems, outText, false);
}

//////////////////////////////////////////////////////////////////////////
bool ChooseAnimationStateEx(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	return DoChooseAnimationState(pVar, outItems, outText, true);
}

//////////////////////////////////////////////////////////////////////////
bool ChooseEntityProperties(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	CFlowNodeGetCustomItemsBase* pGetCustomItems;
	IEntity* pEntity = GetNodeEntity(pVar, pGetCustomItems);
	if (!pEntity)
	{
		return false;
	}

	CBaseObject* pObj = GetIEditorImpl()->GetObjectManager()->FindObject(pEntity->GetName());

	if (pObj)
	{
		CEntityObject* pEntityObject = (CEntityObject*)pObj;
		if (pEntityObject->GetProperties())
		{
			CVarBlock* pEntityProperties = pEntityObject->GetProperties();
			if (pEntityObject->GetPrototype())
				pEntityProperties = pEntityObject->GetPrototype()->GetProperties();

			for (int i = 0; i < pEntityProperties->GetNumVariables(); i++)
			{
				IVariable* currVar = pEntityProperties->GetVariable(i);

				if (currVar->GetNumVariables() > 0)
				{
					for (int j = 0; j < currVar->GetNumVariables(); j++)
					{
						outItems.push_back(SItem(string().Format("Properties:%s:%s", currVar->GetName(), currVar->GetVariable(j)->GetName()).c_str()));
					}
				}
				else
					outItems.push_back(SItem(string().Format("Properties:%s", pEntityProperties->GetVariable(i)->GetName()).c_str()));
			}

			if (pEntityObject->GetProperties2())
			{
				CVarBlock* pEntityProperties2 = pEntityObject->GetProperties2();

				for (int i = 0; i < pEntityProperties2->GetNumVariables(); i++)
				{
					outItems.push_back(SItem(string().Format("Properties2:%s", pEntityProperties->GetVariable(i)->GetName()).c_str()));
				}
			}
		}
	}
	outText = _T("Choose Entity Property");
	return true;
}

//////////////////////////////////////////////////////////////////////////
IVehicle* GetNodeVehicle(IVariable* pVar)
{
	CFlowNodeGetCustomItemsBase* pGetCustomItems;
	IEntity* pEntity = GetNodeEntity(pVar, pGetCustomItems);
	if (!pEntity)
	{
		return false;
	}

	IVehicleSystem* pVehicleSystem = gEnv->pGameFramework->GetIVehicleSystem();
	if (!pVehicleSystem)
	{
		return false;
	}

	return pVehicleSystem->GetVehicle(pEntity->GetId());
}

bool ChooseVehicleParts(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	IVehicle* pVehicle = GetNodeVehicle(pVar);
	if (!pVehicle)
	{
		return false;
	}

	// Components
	if (pVehicle->GetComponentCount() > 0)
	{
		outItems.push_back(SItem("Components:All"));
	}
	for (size_t i = 0; i < pVehicle->GetComponentCount(); ++i)
	{
		outItems.push_back(SItem(string().Format("Components:%s", pVehicle->GetComponent(i)->GetComponentName())));
	}

	// Wheels
	if (pVehicle->GetWheelCount() > 0)
	{
		outItems.push_back(SItem("Wheels:All"));
	}
	for (size_t i = 0; i < pVehicle->GetWheelCount(); ++i)
	{
		outItems.push_back(SItem(string().Format("Wheels:%s", pVehicle->GetWheelPart(i)->GetName())));
	}

	// Weapons
	if (pVehicle->GetWeaponCount() > 0)
	{
		outItems.push_back(SItem("Weapons:All"));
	}
	for (size_t i = 0; i < pVehicle->GetWeaponCount(); ++i)
	{
		outItems.push_back(SItem(string().Format("Weapons:%s", gEnv->pEntitySystem->GetEntity(pVehicle->GetWeaponId(i))->GetName())));
	}

	// Seats
	if (pVehicle->GetSeatCount() > 0)
	{
		outItems.push_back(SItem("Seats:All"));
	}
	for (size_t i = 0; i < pVehicle->GetSeatCount(); ++i)
	{
		outItems.push_back(SItem(string().Format("Seats:%s", pVehicle->GetSeatById(i + 1)->GetSeatName())));
	}

	outText = _T("Choose Vehicle Parts");
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool ChooseActionFilter(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	IActionMapManager* pActionMapMgr = gEnv->pGameFramework->GetIActionMapManager();
	if (pActionMapMgr)
	{
		IActionFilterIteratorPtr pFilterIter = pActionMapMgr->CreateActionFilterIterator();
		while (IActionFilter* pFilter = pFilterIter->Next())
		{
			outItems.push_back(SItem(pFilter->GetName()));
		}
		outText = _T("Choose ActionFilter");
	}
	return (outItems.size() > 0);
}

//////////////////////////////////////////////////////////////////////////
bool ChooseActionMap(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	IActionMapManager* pActionMapMgr = gEnv->pGameFramework->GetIActionMapManager();
	if (pActionMapMgr)
	{
		IActionMapIteratorPtr pActionMapIter = pActionMapMgr->CreateActionMapIterator();
		while (IActionMap* pActionMap = pActionMapIter->Next())
		{
			outItems.push_back(SItem(pActionMap->GetName()));
		}
		outText = _T("Choose ActionMap");
	}
	return (outItems.size() > 0);
}

//////////////////////////////////////////////////////////////////////////
bool ChooseActionMapAction(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	IActionMapManager* pActionMapMgr = gEnv->pGameFramework->GetIActionMapManager();
	if (pActionMapMgr)
	{
		IActionMapIteratorPtr pActionMapIter = pActionMapMgr->CreateActionMapIterator();
		while (IActionMap* pActionMap = pActionMapIter->Next())
		{
			IActionMapActionIteratorPtr pActionMapActionIter = pActionMap->CreateActionIterator();
			while (const IActionMapAction* pAction = pActionMapActionIter->Next())
			{
				outItems.push_back(SItem(string().Format("%s:%s", pActionMap->GetName(), pAction->GetActionId())));
			}
		}
	}

	outText = _T("Choose ActionMap Action");

	return (outItems.size() > 0);
}

bool ChooseVehicleSeats(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	IVehicle* pVehicle = GetNodeVehicle(pVar);
	if (!pVehicle)
	{
		return false;
	}

	for (size_t i = 0; i < pVehicle->GetSeatCount(); ++i)
	{
		IVehicleSeat* pSeat = pVehicle->GetSeatById(i + 1);
		CRY_ASSERT(pSeat);
		outItems.push_back(SItem(pSeat->GetSeatName()));
	}

	outText = _T("Choose Vehicle Seats");
	return true;
}

bool ChooseVehicleSeatViews(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	IVehicle* pVehicle = GetNodeVehicle(pVar);
	if (!pVehicle)
	{
		return false;
	}

	for (size_t i = 0; i < pVehicle->GetSeatCount(); ++i)
	{
		IVehicleSeat* pSeat = pVehicle->GetSeatById(i + 1);
		CRY_ASSERT(pSeat);

		for (size_t j = 0; j < pSeat->GetViewCount(); ++j)
		{
			IVehicleView* pView = pSeat->GetView(j + 1);
			if (!pView->IsDebugView())
			{
				outItems.push_back(SItem(string().Format("%s:%s (%d)", pSeat->GetSeatName(), pView->GetName(), (j + 1))));
			}
		}
	}

	outText = _T("Choose Vehicle Seat View");
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool GetMatParams(IEntity* pEntity, int slot, int subMtlId, DynArrayRef<SShaderParam>** outParams)
{
	IEntityRender* pIEntityRender = pEntity->GetRenderInterface();
	if (pIEntityRender == 0)
		return false;

	IMaterial* pMtl = pIEntityRender->GetRenderMaterial(slot);
	if (pMtl == 0)
	{
		Warning("EntityMaterialShaderParam: Entity '%s' [%d] has no material at slot %d", pEntity->GetName(), pEntity->GetId(), slot);
		return false;
	}

	pMtl = (subMtlId > 0 && pMtl->GetSubMtlCount() > 0) ? pMtl->GetSubMtl(subMtlId) : pMtl;
	if (pMtl == 0)
	{
		Warning("EntityMaterialShaderParam: Entity '%s' [%d] has no sub-material %d at slot %d", pEntity->GetName(), pEntity->GetId(), subMtlId, slot);
		return false;
	}

	const SShaderItem& shaderItem = pMtl->GetShaderItem();
	IRenderShaderResources* pRendRes = shaderItem.m_pShaderResources;
	if (pRendRes == 0)
	{
		Warning("EntityMaterialShaderParam: Material '%s' has no shader resources ", pMtl->GetName());
		return false;
	}
	*outParams = &pRendRes->GetParameters();
	return true;
}

//////////////////////////////////////////////////////////////////////////
void FillOutParamsByShaderParams(std::vector<SItem>& outItems, DynArrayRef<SShaderParam>* params, int limit)
{
	if (limit == 1)
	{
		outItems.push_back(SItem("diffuse"));
		outItems.push_back(SItem("specular"));
		outItems.push_back(SItem("emissive_color"));
	}
	else if (limit == 2)
	{
		outItems.push_back(SItem("emissive_intensity"));
		outItems.push_back(SItem("shininess"));
		outItems.push_back(SItem("alpha"));
		outItems.push_back(SItem("opacity"));
	}

	// fill in params
	SItem item;
	for (int i = 0; i < params->size(); ++i)
	{
		SShaderParam& param = (*params)[i];
		EParamType paramType = param.m_Type;
		if (limit == 1 && paramType != eType_VECTOR && paramType != eType_FCOLOR)
			continue;
		else if (limit == 2 && paramType != eType_FLOAT && paramType != eType_INT && paramType != eType_SHORT && paramType != eType_BOOL)
			continue;
		item.name = param.m_Name;
		outItems.push_back(item);
	}
}

//////////////////////////////////////////////////////////////////////////
bool ChooseMatParamSlot(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	CFlowNodeGetCustomItemsBase* pGetCustomItems;
	IEntity* pEntity = GetNodeEntity(pVar, pGetCustomItems);
	if (!pEntity)
	{
		return false;
	}

	TParamMap paramMap;
	GetParamMap(pGetCustomItems, paramMap);

	string& slotString = paramMap["slot_ref"];
	string& subString = paramMap["sub_ref"];
	string& paramTypeString = paramMap["param"];
	int slot = -1;
	int subMtlId = -1;
	if (GetValue(pGetCustomItems, slotString, slot) == false)
	{
		return false;
	}
	if (GetValue(pGetCustomItems, subString, subMtlId) == false)
	{
		return false;
	}

	int limit = 0;
	if (paramTypeString == "vec")
	{
		limit = 1;
	}
	else if (paramTypeString == "float")
	{
		limit = 2;
	}

	DynArrayRef<SShaderParam>* params;
	if (GetMatParams(pEntity, slot, subMtlId, &params) == false)
	{
		return false;
	}

	FillOutParamsByShaderParams(outItems, params, limit);

	outText = _T("Choose Param");
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool ChooseMatParamName(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	CFlowNodeGetCustomItemsBase* pGetCustomItems = static_cast<CFlowNodeGetCustomItemsBase*>(pVar->GetUserData());
	assert(pGetCustomItems != 0);

	TParamMap paramMap;
	GetParamMap(pGetCustomItems, paramMap);

	string& nameString = paramMap["name_ref"];
	string& subString = paramMap["sub_ref"];
	string& paramTypeString = paramMap["param"];
	string matName;
	int subMtlId = -1;
	if (GetValue(pGetCustomItems, nameString, matName) == false || matName.empty())
	{
		Warning("MaterialParam: No/Invalid Material given.");
		return false;
	}
	if (GetValue(pGetCustomItems, subString, subMtlId) == false)
	{
		return false;
	}

	int limit = 0;
	if (paramTypeString == "vec")
	{
		limit = 1;
	}
	else if (paramTypeString == "float")
	{
		limit = 2;
	}

	IMaterial* pMtl = gEnv->p3DEngine->GetMaterialManager()->FindMaterial(matName);
	if (pMtl == 0)
	{
		Warning("MaterialParam: Material '%s' not found.", matName.c_str());
		return false;
	}

	pMtl = (subMtlId >= 0 && pMtl->GetSubMtlCount() > 0) ? pMtl->GetSubMtl(subMtlId) : pMtl;
	if (pMtl == 0)
	{
		Warning("MaterialParam: Material '%s' has no sub-material %d ", matName.c_str(), subMtlId);
		return false;
	}

	const SShaderItem& shaderItem = pMtl->GetShaderItem();
	IRenderShaderResources* pRendRes = shaderItem.m_pShaderResources;
	if (pRendRes == 0)
	{
		Warning("MaterialParam: Material '%s' has no shader resources ", pMtl->GetName());
		return false;
	}
	DynArrayRef<SShaderParam>* params = &pRendRes->GetParameters();

	FillOutParamsByShaderParams(outItems, params, limit);

	outText = _T("Choose Param");
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool ChooseMatParamCharAttachment(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	CFlowNodeGetCustomItemsBase* pGetCustomItems;
	IEntity* pEntity = GetNodeEntity(pVar, pGetCustomItems);
	if (!pEntity)
	{
		return false;
	}

	TParamMap paramMap;
	GetParamMap(pGetCustomItems, paramMap);

	string& slotString = paramMap["charslot_ref"];
	string& attachString = paramMap["attachment_ref"];
	string& paramTypeString = paramMap["param"];
	string& subString = paramMap["sub_ref"];
	int charSlot = -1;
	int subMtlId = -1;
	if (GetValue(pGetCustomItems, slotString, charSlot) == false)
	{
		return false;
	}
	if (GetValue(pGetCustomItems, subString, subMtlId) == false)
	{
		return false;
	}
	string attachmentName;
	if (GetValue(pGetCustomItems, attachString, attachmentName) == false || attachmentName.empty())
	{
		Warning("MaterialParam: No/Invalid Attachment given.");
		return false;
	}

	ICharacterInstance* pCharacter = pEntity->GetCharacter(charSlot);
	if (pCharacter == 0)
		return false;

	IAttachmentManager* pMgr = pCharacter->GetIAttachmentManager();
	if (pMgr == 0)
		return false;

	IAttachment* pAttachment = pMgr->GetInterfaceByName(attachmentName.c_str());
	if (pAttachment == 0)
		return false;
	IAttachmentObject* pAttachmentObject = pAttachment->GetIAttachmentObject();
	if (pAttachmentObject == 0)
		return false;

	IMaterial* pMtl = (IMaterial*)pAttachmentObject->GetBaseMaterial();
	if (pMtl == 0)
		return false;

	int limit = 0;
	if (paramTypeString == "vec")
	{
		limit = 1;
	}
	else if (paramTypeString == "float")
	{
		limit = 2;
	}

	string matName = pMtl->GetName();

	pMtl = (subMtlId >= 0 && pMtl->GetSubMtlCount() > 0) ? pMtl->GetSubMtl(subMtlId) : pMtl;
	if (pMtl == 0)
	{
		Warning("MaterialParam: Material '%s' has no sub-material %d ", matName.c_str(), subMtlId);
		return false;
	}

	const SShaderItem& shaderItem = pMtl->GetShaderItem();
	IRenderShaderResources* pRendRes = shaderItem.m_pShaderResources;
	if (pRendRes == 0)
	{
		Warning("MaterialParam: Material '%s' has no shader resources ", pMtl->GetName());
		return false;
	}
	DynArrayRef<SShaderParam>* params = &pRendRes->GetParameters();

	FillOutParamsByShaderParams(outItems, params, limit);

	outText = _T("Choose Param");
	return true;
}

bool ChooseFormationName(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	unsigned int formationNameCount = 0;
	const char* formationNames[32];
	gEnv->pAISystem->EnumerateFormationNames(32, formationNames, &formationNameCount);
	for (unsigned int i = 0; i < formationNameCount; ++i)
	{
		outItems.push_back(SItem(formationNames[i]));
	}
	outText = _T("Choose Formation Name");
	return true;
}

bool ChooseCommunicationManagerVariableName(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	size_t variableNameCount = 0;
	const static size_t kMaxVariableNamesAllowed = 64;
	const char* variableNames[kMaxVariableNamesAllowed];
	gEnv->pAISystem->GetCommunicationManager()->GetVariablesNames(variableNames, kMaxVariableNamesAllowed, variableNameCount);
	for (unsigned int i = 0; i < variableNameCount; ++i)
	{
		outItems.push_back(SItem(variableNames[i]));
	}
	outText = _T("Choose Communication Variable Name");
	return true;
}

bool ChooseFGModules(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	IModuleIteratorPtr moduleIter = gEnv->pFlowSystem->GetIModuleManager()->CreateModuleIterator();
	for (IFlowGraphModule* pModule; pModule = moduleIter->Next();)
	{
		outItems.push_back(pModule->GetName());
	}
	outText = _T("Choose FlowGraph Module");
	return true;
}

bool ChooseUIElements(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	if (gEnv->pFlashUI)
	{
		const int count = gEnv->pFlashUI->GetUIElementCount();
		for (int i = 0; i < count; ++i)
		{
			IUIElement* pElement = gEnv->pFlashUI->GetUIElement(i);
			if (!pElement->IsValid()) continue;

			outItems.push_back(pElement->GetName());
		}
	}
	outText = _T("Choose UIElement");
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool ChooseUIActions(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	const int count = gEnv->pFlashUI->GetUIActionCount();
	for (int i = 0; i < count; ++i)
	{
		IUIAction* pAction = gEnv->pFlashUI->GetUIAction(i);
		if (!pAction->IsValid()) continue;

		outItems.push_back(pAction->GetName());
	}
	outText = _T("Choose Action");
	return true;
}

//////////////////////////////////////////////////////////////////////////
template<EUIObjectType T> bool IsForTemplates()                     { return false; }
template<> bool                IsForTemplates<eUOT_MovieClipTmpl>() { return true; }

template<EUIObjectType T1, EUIObjectType T2, class Item>
void FillUISubItems(std::vector<SItem>& outItems, Item* pItem, const string& itemname)
{
	if (!IsForTemplates<T2>())
	{
		const int count = SUIGetDesc<T1, Item, int>::GetCount(pItem);
		for (int i = 0; i < count; ++i)
		{
			const typename SUIDescTypeOf<T1>::TType * pDesc = SUIGetDesc<T1, Item, int>::GetDesc(pItem, i);
			string desc;
			desc.Format("%s:%s", itemname.c_str(), pDesc->sDisplayName);
			outItems.push_back(SItem(desc.c_str(), pDesc->sDesc));
		}
	}

	const int subcount = SUIGetDesc<T2, Item, int>::GetCount(pItem);
	for (int i = 0; i < subcount; ++i)
	{
		const typename SUIDescTypeOf<T2>::TType * pDesc = SUIGetDesc<T2, Item, int>::GetDesc(pItem, i);
		string subitemname;
		subitemname.Format("%s:%s", itemname.c_str(), pDesc->sDisplayName);
		FillUISubItems<T1, eUOT_MovieClip>(outItems, pDesc, subitemname);
	}
}

template<EUIObjectType T>
bool ChooseUIVariables(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	outText = _T("Choose Variables");
	if (gEnv->pFlashUI)
	{
		const int count = gEnv->pFlashUI->GetUIElementCount();
		for (int i = 0; i < count; ++i)
		{
			IUIElement* pElement = gEnv->pFlashUI->GetUIElement(i);
			if (!pElement->IsValid()) continue;

			FillUISubItems<eUOT_Variable, T>(outItems, pElement, pElement->GetName());
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
template<EUIObjectType T>
bool ChooseUIArrays(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	outText = _T("Choose Array");
	if (gEnv->pFlashUI)
	{
		const int count = gEnv->pFlashUI->GetUIElementCount();
		for (int i = 0; i < count; ++i)
		{
			IUIElement* pElement = gEnv->pFlashUI->GetUIElement(i);
			if (!pElement->IsValid()) continue;

			FillUISubItems<eUOT_Array, T>(outItems, pElement, pElement->GetName());
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
template<EUIObjectType T>
bool ChooseUIMovieClips(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	outText = _T("Choose MovieClip");
	if (gEnv->pFlashUI)
	{
		const int count = gEnv->pFlashUI->GetUIElementCount();
		for (int i = 0; i < count; ++i)
		{
			IUIElement* pElement = gEnv->pFlashUI->GetUIElement(i);
			const int count = pElement->GetMovieClipCount();
			if (!pElement->IsValid()) continue;

			FillUISubItems<eUOT_MovieClip, T>(outItems, pElement, pElement->GetName());
			if (IsForTemplates<T>())
				FillUISubItems<eUOT_MovieClipTmpl, eUOT_MovieClip>(outItems, pElement, pElement->GetName());
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool ChooseUITemplate(IVariable* pVar, std::vector<SItem>& outItems, string& outText)
{
	outText = _T("Choose MovieClip Template");
	if (gEnv->pFlashUI)
	{
		const int count = gEnv->pFlashUI->GetUIElementCount();
		for (int i = 0; i < count; ++i)
		{
			IUIElement* pElement = gEnv->pFlashUI->GetUIElement(i);
			const int count = pElement->GetMovieClipTmplCount();
			if (!pElement->IsValid()) continue;

			FillUISubItems<eUOT_MovieClipTmpl, eUOT_MovieClip>(outItems, pElement, pElement->GetName());
		}
	}
	return true;
}
};

struct CFlowNodeCustomItemsImpl : public CFlowNodeGetCustomItemsBase
{
	typedef bool (* TGetItemsFct) (IVariable*, std::vector<SItem>&, string&);
	CFlowNodeCustomItemsImpl(const TGetItemsFct& fct, bool tree, const char* sep) : m_fct(fct), m_tree(tree), m_sep(sep) {}
	virtual bool        GetItems(IVariable* pVar, std::vector<SItem>& items, string& outDialogTitle) { return m_fct(pVar, items, outDialogTitle); }
	virtual bool        UseTree()                                                                     { return m_tree; }
	virtual const char* GetTreeSeparator()                                                            { return m_sep; }
	TGetItemsFct m_fct;
	bool         m_tree;
	const char*  m_sep;
};

#ifdef FGVARIABLES_REAL_CLONE
	#define DECL_CHOOSE_EX_IMPL(NAME, FCT, TREE, SEP)                                                                                                                       \
	  struct  C ## NAME : public CFlowNodeCustomItemsImpl                                                                                                                   \
	  {                                                                                                                                                                     \
	    C ## NAME() : CFlowNodeCustomItemsImpl(&FCT, TREE, SEP) {}                                                                                                          \
	    virtual CFlowNodeGetCustomItemsBase* Clone() const { C ## NAME * pNew = new C ## NAME(); pNew->m_pFlowNode = m_pFlowNode; pNew->m_config = m_config; return pNew; } \
	  };
#else
	#define DECL_CHOOSE_EX_IMPL(NAME, FCT, TREE, SEP)                                               \
	  struct  C ## NAME : public CFlowNodeCustomItemsImpl                                           \
	  {                                                                                             \
	    C ## NAME() : CFlowNodeCustomItemsImpl(&FCT, TREE, SEP) {}                                  \
	    virtual CFlowNodeGetCustomItemsBase* Clone() const { return const_cast<C ## NAME*>(this); } \
	  };
#endif

#define DECL_CHOOSE_EX(FOO, TREE, SEP) DECL_CHOOSE_EX_IMPL(FOO, FOO, TREE, SEP)
#define DECL_CHOOSE(FOO)               DECL_CHOOSE_EX(FOO, false, "")

DECL_CHOOSE(ChooseCharacterAnimation);
DECL_CHOOSE(ChooseAnimationState);
DECL_CHOOSE(ChooseAnimationStateEx);
DECL_CHOOSE(ChooseBone);
DECL_CHOOSE(ChooseAttachment);
DECL_CHOOSE_EX(ChooseDialog, true, ".");
DECL_CHOOSE(ChooseMatParamSlot);
DECL_CHOOSE(ChooseMatParamName);
DECL_CHOOSE(ChooseMatParamCharAttachment);
DECL_CHOOSE(ChooseFormationName);
DECL_CHOOSE(ChooseCommunicationManagerVariableName);
DECL_CHOOSE(ChooseUIElements);
DECL_CHOOSE(ChooseUIActions);
DECL_CHOOSE(ChooseActionFilter);
DECL_CHOOSE(ChooseActionMap);
DECL_CHOOSE_EX(ChooseActionMapAction, true, ":");
DECL_CHOOSE_EX_IMPL(ChooseUIVariables, ChooseUIVariables<eUOT_MovieClip>, true, ":");
DECL_CHOOSE_EX_IMPL(ChooseUIArrays, ChooseUIArrays<eUOT_MovieClip>, true, ":");
DECL_CHOOSE_EX_IMPL(ChooseUIMovieClips, ChooseUIMovieClips<eUOT_MovieClip>, true, ":");
DECL_CHOOSE_EX_IMPL(ChooseUIVariablesTmpl, ChooseUIVariables<eUOT_MovieClipTmpl>, true, ":");
DECL_CHOOSE_EX_IMPL(ChooseUIArraysTmpl, ChooseUIArrays<eUOT_MovieClipTmpl>, true, ":");
DECL_CHOOSE_EX_IMPL(ChooseUIMovieClipsTmpl, ChooseUIMovieClips<eUOT_MovieClipTmpl>, true, ":");
DECL_CHOOSE_EX_IMPL(ChooseUITemplate, ChooseUITemplate, true, ":");
DECL_CHOOSE(ChooseFGModules);
DECL_CHOOSE_EX(ChooseVehicleParts, true, ":");
DECL_CHOOSE_EX(ChooseVehicleSeats, true, ":");
DECL_CHOOSE_EX(ChooseVehicleSeatViews, true, ":");
DECL_CHOOSE_EX(ChooseEntityProperties, true, ":");

/*
   And here's the map used to specify specialized property editors for input ports.

   The editor should be specified in the UIConfig, as a parameter to the SInputPortConfig.
   eg: _UICONFIG("dt=entityProperties,ref_entity=entityId")

   Previously the editor was combined with the port name as a prefix.
   If a port editor needs to be changed, use UIConfig, it will override what
   was previously defined in the name and not break script compatibility.
   The name will always be shown in the node without the prefix.

   If, in either way, an editor name is provided, the data type
   of the port will be changed to allow the specialized editor.

   if datatype is IVariable::DT_USERITEMCB a GenericSelectItem dialog is used
   and the callback should provide the selectable items (optionally context
   [==node]-sensitive items) e.g. Animation Lookup
 */
static const FlowGraphVariables::MapEntry prefix_dataType_table[] =
{
	{ "clr_",                   IVariable::DT_COLOR,                   0                                                                      },
	{ "color_",                 IVariable::DT_COLOR,                   0                                                                      },

	{ "tex_",                   IVariable::DT_TEXTURE,                 0                                                                      },
	{ "texture_",               IVariable::DT_TEXTURE,                 0                                                                      },

	{ "obj_",                   IVariable::DT_OBJECT,                  0                                                                      },
	{ "object_",                IVariable::DT_OBJECT,                  0                                                                      },

	{ "file_",                  IVariable::DT_FILE,                    0                                                                      },

	{ "text_",                  IVariable::DT_LOCAL_STRING,            0                                                                      },
	{ "equip_",                 IVariable::DT_EQUIP,                   0                                                                      },
	{ "reverbpreset_",          IVariable::DT_REVERBPRESET,            0                                                                      },

	{ "aianchor_",              IVariable::DT_AI_ANCHOR,               0                                                                      },
	{ "aibehavior_",            IVariable::DT_AI_BEHAVIOR,             0                                                                      },
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
	{ "aicharacter_",           IVariable::DT_AI_CHARACTER,            0                                                                      },
#endif
	{ "aipfpropertieslist_",    IVariable::DT_AI_PFPROPERTIESLIST,     0                                                                      },
	{ "aientityclasses_",       IVariable::DT_AIENTITYCLASSES,         0                                                                      },
	{ "aiterritory_",           IVariable::DT_AITERRITORY,             0                                                                      },
	{ "aiwave_",                IVariable::DT_AIWAVE,                  0                                                                      },
	{ "soclass_",               IVariable::DT_SOCLASS,                 0                                                                      },
	{ "soclasses_",             IVariable::DT_SOCLASSES,               0                                                                      },
	{ "sostate_",               IVariable::DT_SOSTATE,                 0                                                                      },
	{ "sostates_",              IVariable::DT_SOSTATES,                0                                                                      },
	{ "sopattern_",             IVariable::DT_SOSTATEPATTERN,          0                                                                      },
	{ "soaction_",              IVariable::DT_SOACTION,                0                                                                      },
	{ "sohelper_",              IVariable::DT_SOHELPER,                0                                                                      },
	{ "sonavhelper_",           IVariable::DT_SONAVHELPER,             0                                                                      },
	{ "soanimhelper_",          IVariable::DT_SOANIMHELPER,            0                                                                      },
	{ "soevent_",               IVariable::DT_SOEVENT,                 0                                                                      },
	{ "customaction_",          IVariable::DT_CUSTOMACTION,            0                                                                      },
	{ "gametoken_",             IVariable::DT_GAMETOKEN,               0                                                                      },
	{ "mat_",                   IVariable::DT_MATERIAL,                0                                                                      },
	{ "seq_",                   IVariable::DT_SEQUENCE,                0                                                                      },
	{ "mission_",               IVariable::DT_MISSIONOBJ,              0                                                                      },
	{ "anim_",                  IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseCharacterAnimation>               },
	{ "animstate_",             IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseAnimationState>                   },
	{ "animstateEx_",           IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseAnimationStateEx>                 },
	{ "bone_",                  IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseBone>                             },
	{ "attachment_",            IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseAttachment>                       },
	{ "dialog_",                IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseDialog>                           },
	{ "matparamslot_",          IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseMatParamSlot>                     },
	{ "matparamname_",          IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseMatParamName>                     },
	{ "matparamcharatt_",       IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseMatParamCharAttachment>           },
	{ "seqid_",                 IVariable::DT_SEQUENCE_ID,             0                                                                      },
	{ "lightanimation_",        IVariable::DT_LIGHT_ANIMATION,         0                                                                      },
	{ "formation_",             IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseFormationName>                    },
	{ "communicationVariable_", IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseCommunicationManagerVariableName> },
	{ "uiElements_",            IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseUIElements>                       },
	{ "uiActions_",             IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseUIActions>                        },
	{ "uiVariables_",           IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseUIVariables>                      },
	{ "uiArrays_",              IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseUIArrays>                         },
	{ "uiMovieclips_",          IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseUIMovieClips>                     },
	{ "uiVariablesTmpl_",       IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseUIVariablesTmpl>                  },
	{ "uiArraysTmpl_",          IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseUIArraysTmpl>                     },
	{ "uiMovieclipsTmpl_",      IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseUIMovieClipsTmpl>                 },
	{ "uiTemplates_",           IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseUITemplate>                       },
	{ "vehicleParts_",          IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseVehicleParts>                     },
	{ "vehicleSeats_",          IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseVehicleSeats>                     },
	{ "vehicleSeatViews_",      IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseVehicleSeatViews>                 },
	{ "entityProperties_",      IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseEntityProperties>                 },
	{ "actionFilter_",          IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseActionFilter>                     },
	{ "actionMaps_",            IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseActionMap>                        },
	{ "actionMapActions_",      IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseActionMapAction>                  },
	{ "geomcache_",             IVariable::DT_GEOM_CACHE,              0                                                                      },
	{ "audioTrigger_",          IVariable::DT_AUDIO_TRIGGER,           0                                                                      },
	{ "audioSwitch_",           IVariable::DT_AUDIO_SWITCH,            0                                                                      },
	{ "audioSwitchState_",      IVariable::DT_AUDIO_SWITCH_STATE,      0                                                                      },
	{ "audioRTPC_",             IVariable::DT_AUDIO_RTPC,              0                                                                      },
	{ "audioEnvironment_",      IVariable::DT_AUDIO_ENVIRONMENT,       0                                                                      },
	{ "audioPreloadRequest_",   IVariable::DT_AUDIO_PRELOAD_REQUEST,   0                                                                      },
	{ "dynamicResponseSignal_", IVariable::DT_DYNAMIC_RESPONSE_SIGNAL, 0                                                                      },
	{ "fgmodules_",             IVariable::DT_USERITEMCB,              &FlowGraphVariables::CreateIt<CChooseFGModules>                        },
};
static const int prefix_dataType_numEntries = sizeof(prefix_dataType_table) / sizeof(FlowGraphVariables::MapEntry);

namespace FlowGraphVariables
{
const MapEntry* FindEntry(const char* sPrefix)
{
	for (int i = 0; i < prefix_dataType_numEntries; ++i)
	{
		if (strstr(sPrefix, prefix_dataType_table[i].sPrefix) == sPrefix)
		{
			return &prefix_dataType_table[i];
		}
	}
	return 0;
}
};

//////////////////////////////////////////////////////////////////////////
CUIEnumsDatabase_SEnum* CVariableFlowNodeDynamicEnum::GetSEnum()
{
	CFlowNodeGetCustomItemsBase* pGetCustomItems = static_cast<CFlowNodeGetCustomItemsBase*>(this->GetUserData());
	if (pGetCustomItems == 0)
		return 0;

	assert(pGetCustomItems != 0);
	// CryLogAlways("CVariableFlowNodeDynamicEnum::GetSEnum 0x%p pGetCustomItems=0x%p FlowNode=0x%p", this, pGetCustomItems, pGetCustomItems->GetFlowNode());

	string enumName;
	bool ok = ::GetValue(pGetCustomItems, m_refPort, enumName);
	if (!ok)
		return 0;

	string enumKey;
	enumKey.Format(m_refFormatString, enumName.c_str());
	CUIEnumsDatabase_SEnum* pEnum = GetIEditorImpl()->GetUIEnumsDatabase()->FindEnum(enumKey);
	return pEnum;
}

//////////////////////////////////////////////////////////////////////////
CUIEnumsDatabase_SEnum* CVariableFlowNodeDefinedEnum::GetSEnum()
{
	CFlowNodeGetCustomItemsBase* pGetCustomItems = static_cast<CFlowNodeGetCustomItemsBase*>(this->GetUserData());
	if (pGetCustomItems == 0)
		return 0;

	assert(pGetCustomItems != 0);
	// CryLogAlways("CVariableFlowNodeDefinedEnum::GetSEnum 0x%p pGetCustomItems=0x%p FlowNode=0x%p", this, pGetCustomItems, pGetCustomItems->GetFlowNode());

	CFlowNode* pCFlowNode = pGetCustomItems->GetFlowNode();
	if (pCFlowNode)
	{
		IFlowGraph* pGraph = pCFlowNode->GetIFlowGraph();
		IFlowNodeData* pData = pGraph ? pGraph->GetNodeData(pCFlowNode->GetFlowNodeId()) : NULL;
		IFlowNode* pIFlowNode = pData ? pData->GetNode() : NULL;

		if (pIFlowNode)
		{
			CEntityObject* pNodeEntity = pCFlowNode->GetEntity();
			IEntity* pNodeIEntity = pNodeEntity ? pNodeEntity->GetIEntity() : NULL;

			SFlowNodeConfig config;
			pData->GetConfiguration(config);
			const uint32 portId = m_portId - ((config.nFlags & EFLN_TARGET_ENTITY) ? 1 : 0);

			string outEnumName;
			if (!pIFlowNode->GetPortGlobalEnum(portId, pNodeIEntity, m_refFormatString.GetString(), outEnumName))
				outEnumName = m_refFormatString.GetString();

			CUIEnumsDatabase_SEnum* pEnum = GetIEditorImpl()->GetUIEnumsDatabase()->FindEnum(outEnumName.c_str());
			return pEnum;
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
CVariableFlowNodeCustomEnumBase::CDynamicEnumList::CDynamicEnumList(CVariableFlowNodeCustomEnumBase* pDynEnum)
{
	m_pDynEnum = pDynEnum;
	// CryLogAlways("CDynamicEnumList::ctor 0x%p", this);
}

//////////////////////////////////////////////////////////////////////////
CVariableFlowNodeCustomEnumBase::CDynamicEnumList::~CDynamicEnumList()
{
	// CryLogAlways("CDynamicEnumList::dtor 0x%p", this);
}

//! Get the name of specified value in enumeration.
const char* CVariableFlowNodeCustomEnumBase::CDynamicEnumList::GetItemName(uint index)
{
	struct CUIEnumsDatabase_SEnum* pEnum = m_pDynEnum->GetSEnum();
	if (!pEnum || index >= pEnum->strings.size())
		return NULL;
	return pEnum->strings[index];
}

//////////////////////////////////////////////////////////////////////////
string CVariableFlowNodeCustomEnumBase::CDynamicEnumList::NameToValue(const string& name)
{
	struct CUIEnumsDatabase_SEnum* pEnum = m_pDynEnum->GetSEnum();
	if (pEnum == 0)
		return "";
	return pEnum->NameToValue(name);
}

//////////////////////////////////////////////////////////////////////////
string CVariableFlowNodeCustomEnumBase::CDynamicEnumList::ValueToName(const string& name)
{
	struct CUIEnumsDatabase_SEnum* pEnum = m_pDynEnum->GetSEnum();
	if (pEnum == 0)
		return "";
	return pEnum->ValueToName(name);
}


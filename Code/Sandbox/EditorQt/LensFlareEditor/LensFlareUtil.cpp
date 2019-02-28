// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Objects/EntityObject.h"
#include "LensFlareUtil.h"
#include "Util/Clipboard.h"
#include "Objects/EntityObject.h"
#include <../../CryPlugins/CryDefaultEntities/Module/DefaultComponents/Lights/ILightComponent.h>

namespace LensFlareUtil
{
IOpticsElementBasePtr CreateOptics(XmlNodeRef& xmlNode)
{
	IOpticsElementBasePtr pOpticsElement = NULL;

	const char* typeName;
	if (!xmlNode->getAttr("Type", &typeName))
		return NULL;

	bool bEnable(true);
	xmlNode->getAttr("Enable", bEnable);

	EFlareType flareType;
	if (!GetFlareType(typeName, flareType))
		return NULL;

	pOpticsElement = gEnv->pOpticsManager->Create(flareType);
	if (pOpticsElement == NULL)
		return NULL;

	pOpticsElement->SetEnabled(bEnable);

	if (!FillOpticsFromXML(pOpticsElement, xmlNode))
		return NULL;

	return pOpticsElement;
}

IOpticsElementBasePtr CreateOptics(IOpticsElementBasePtr pOptics, bool bForceTypeToGroup)
{
	if (pOptics == NULL)
		return NULL;
	IOpticsElementBasePtr pNewOptics = NULL;
	if (bForceTypeToGroup)
	{
		if (pOptics->GetType() == eFT_Root)
			pNewOptics = gEnv->pOpticsManager->Create(eFT_Group);
	}
	if (pNewOptics == NULL)
	{
		pNewOptics = gEnv->pOpticsManager->Create(pOptics->GetType());
		if (pNewOptics == NULL)
			return NULL;
	}
	CopyOptics(pOptics, pNewOptics, true);
	return pNewOptics;
}

bool FillOpticsFromXML(IOpticsElementBasePtr pOpticsElement, XmlNodeRef& xmlNode)
{
	const char* name;
	if (!xmlNode->getAttr("Name", &name))
		return false;

	const char* typeName;
	if (!xmlNode->getAttr("Type", &typeName))
		return NULL;
	EFlareType flareType;
	if (!GetFlareType(typeName, flareType))
		return NULL;
	if (flareType != pOpticsElement->GetType())
		return NULL;

	pOpticsElement->SetName(name);

	bool bEnable(true);
	xmlNode->getAttr("Enable", bEnable);
	pOpticsElement->SetEnabled(bEnable);

	XmlNodeRef pParamNode = xmlNode->findChild("Params");
	if (pParamNode == NULL)
		return false;

	LensFlareUtil::FillParams(pParamNode, pOpticsElement);
	return true;
}

bool CreateXmlData(IOpticsElementBasePtr pOptics, XmlNodeRef& pOutNode)
{
	pOutNode = gEnv->pSystem->CreateXmlNode("FlareItem");

	if (pOptics == NULL || pOutNode == NULL)
		return false;

	string typeName;
	if (!GetFlareTypeName(typeName, pOptics))
		return false;

	pOutNode->setAttr("Name", pOptics->GetName());
	pOutNode->setAttr("Type", typeName);
	pOutNode->setAttr("Enable", pOptics->IsEnabled());

	XmlNodeRef pParamNode = pOutNode->createNode("Params");

	CVarBlockPtr pVar;
	SetVariablesTemplateFromOptics(pOptics, pVar);

	pVar->Serialize(pParamNode, false);
	pOutNode->addChild(pParamNode);

	for (int i = 0, iElementCount(pOptics->GetElementCount()); i < iElementCount; ++i)
	{
		XmlNodeRef pNode = NULL;
		IOpticsElementBasePtr pChildOptics = pOptics->GetElementAt(i);
		if (!CreateXmlData(pChildOptics, pNode))
			continue;

		pOutNode->addChild(pNode);
	}

	return true;
}

void SetVariablesTemplateFromOptics(IOpticsElementBasePtr pOptics, CVarBlockPtr& pRootVar, std::vector<IVariable::OnSetCallback>& funcs)
{
	if (pOptics == NULL)
		return;

	SetVariablesTemplateFromOptics(pOptics, pRootVar);

	for (int i = 0, iNumVariables(pRootVar->GetNumVariables()); i < iNumVariables; ++i)
	{
		IVariable* pVariable = pRootVar->GetVariable(i);
		if (pVariable == NULL)
			continue;
		for (int k = 0, iNumVariables2(pVariable->GetNumVariables()); k < iNumVariables2; ++k)
		{
			IVariable* pChildVariable(pVariable->GetVariable(k));
			if (pChildVariable == NULL)
				continue;
			for (int i = 0, iSize(funcs.size()); i < iSize; ++i)
				pChildVariable->AddOnSetCallback(funcs[i]);
		}
	}
}

extern void SetVariablesTemplateFromOptics(IOpticsElementBasePtr pOptics, CVarBlockPtr& pRootVar)
{
	if (pOptics == NULL)
		return;

	DynArray<FuncVariableGroup> paramGroups = pOptics->GetEditorParamGroups();
	pRootVar = new CVarBlock;

	for (int i = 0; i < paramGroups.size(); ++i)
	{
		CSmartVariableArray variableArray;
		FuncVariableGroup* pGroup = &paramGroups[i];
		if (pGroup == NULL)
			continue;

		string displayGroupName(pGroup->GetHumanName());
		if (displayGroupName == "Common")
		{
			FlareInfoArray::Props flareInfo = FlareInfoArray::Get();
			int nTypeIndex = (int)pOptics->GetType();
			if (nTypeIndex >= 0 && nTypeIndex < flareInfo.size)
			{
				displayGroupName += " : ";
				displayGroupName += flareInfo.p[nTypeIndex].name;
			}
		}

		AddVariable(pRootVar, variableArray, pGroup->GetName(), displayGroupName, "");

		for (int k = 0; k < pGroup->GetVariableCount(); ++k)
		{
			IFuncVariable* pFuncVar = pGroup->GetVariable(k);
			bool bHardMinLimitation = false;

			const std::pair<float, float> range(pFuncVar->GetMin(), pFuncVar->GetMax());

			if (pFuncVar->paramType == e_FLOAT)
			{
				CSmartVariable<float> floatVar;
				AddVariable(variableArray, floatVar, pFuncVar->name.c_str(), pFuncVar->humanName.c_str(), pFuncVar->description.c_str());
				floatVar->SetLimits(range.first, range.second, 0, bHardMinLimitation, false);
				floatVar->Set(pFuncVar->GetFloat());
				floatVar->SetUserData((void*)(intptr_t)MakeFuncKey(i, k));
			}
			else if (pFuncVar->paramType == e_INT)
			{
				CSmartVariable<int> intVar;
				AddVariable(variableArray, intVar, pFuncVar->name.c_str(), pFuncVar->humanName.c_str(), pFuncVar->description.c_str());

				bHardMinLimitation = HaveParameterLowBoundary(pFuncVar->name.c_str());
				intVar->SetLimits(range.first, range.second, 0, bHardMinLimitation, false);

				intVar->Set(pFuncVar->GetInt());
				intVar->SetUserData((void*)(intptr_t)MakeFuncKey(i, k));
			}
			else if (pFuncVar->paramType == e_BOOL)
			{
				CSmartVariable<bool> boolVar;
				AddVariable(variableArray, boolVar, pFuncVar->name.c_str(), pFuncVar->humanName.c_str(), pFuncVar->description.c_str());
				boolVar->Set(pFuncVar->GetBool());
				boolVar->SetUserData((void*)(intptr_t)MakeFuncKey(i, k));
			}
			else if (pFuncVar->paramType == e_VEC2)
			{
				CSmartVariable<Vec2> vec2Var;
				AddVariable(variableArray, vec2Var, pFuncVar->name.c_str(), pFuncVar->humanName.c_str(), pFuncVar->description.c_str());
				vec2Var->SetLimits(range.first, range.second, 0, bHardMinLimitation, false);
				vec2Var->Set(pFuncVar->GetVec2());
				vec2Var->SetUserData((void*)(intptr_t)MakeFuncKey(i, k));
			}
			else if (pFuncVar->paramType == e_VEC3)
			{
				CSmartVariable<Vec3> vec3Var;
				AddVariable(variableArray, vec3Var, pFuncVar->name.c_str(), pFuncVar->humanName.c_str(), pFuncVar->description.c_str());
				vec3Var->SetLimits(range.first, range.second, 0, bHardMinLimitation, false);
				vec3Var->Set(pFuncVar->GetVec3());
				vec3Var->SetUserData((void*)(intptr_t)MakeFuncKey(i, k));
			}
			else if (pFuncVar->paramType == e_VEC4)
			{
				CSmartVariable<Vec4> vec4Var;
				AddVariable(variableArray, vec4Var, pFuncVar->name.c_str(), pFuncVar->humanName.c_str(), pFuncVar->description.c_str());
				vec4Var->SetLimits(range.first, range.second, 0, bHardMinLimitation, false);
				vec4Var->Set(pFuncVar->GetVec4());
				vec4Var->SetUserData((void*)(intptr_t)MakeFuncKey(i, k));
			}
			else if (pFuncVar->paramType == e_COLOR)
			{
				CSmartVariable<Vec3> colorVar;
				AddVariable(variableArray, colorVar, pFuncVar->name.c_str(), pFuncVar->humanName.c_str(), pFuncVar->description.c_str(), IVariable::DT_COLOR);
				ColorF color(pFuncVar->GetColorF());
				Vec3 colorVec3(color.r, color.g, color.b);
				colorVar->Set(colorVec3);
				colorVar->SetUserData((void*)(intptr_t)MakeFuncKey(i, k));

				CSmartVariable<int> alphaVar;
				string alphaName(pFuncVar->name.c_str());
				alphaName += ".alpha";
				string alphaHumanName(pFuncVar->humanName.c_str());
				alphaHumanName += " [alpha]";
				AddVariable(variableArray, alphaVar, alphaName, alphaHumanName, pFuncVar->description.c_str());
				alphaVar->SetLimits(0, 255, 0, bHardMinLimitation, false);
				alphaVar->Set(color.a * 255.0f);
				alphaVar->SetUserData((void*)(intptr_t)MakeFuncKey(i, k));
			}
			else if (pFuncVar->paramType == e_MATRIX33)
			{
				// Reserved part
				// This part is for future because Matrix33 type is suppose to be provided in a renderer side
			}
			else if (pFuncVar->paramType == e_TEXTURE2D || pFuncVar->paramType == e_TEXTURE3D || pFuncVar->paramType == e_TEXTURE_CUBE)
			{
				CSmartVariable<string> textureVar;
				ITexture* pTexture = pFuncVar->GetTexture();
				if (pTexture)
				{
					string textureName = pTexture->GetName();
					textureVar->Set(textureName);
				}
				AddVariable(variableArray, textureVar, pFuncVar->name.c_str(), pFuncVar->humanName.c_str(), pFuncVar->description.c_str(), IVariable::DT_TEXTURE);
				textureVar->SetUserData((void*)(intptr_t)MakeFuncKey(i, k));
			}
		}
	}
}

void AddOptics(IOpticsElementBasePtr pParentOptics, XmlNodeRef& xmlNode)
{
	if (xmlNode == NULL)
		return;

	if (strcmp(xmlNode->getTag(), "FlareItem"))
		return;

	IOpticsElementBasePtr pOptics = CreateOptics(xmlNode);
	if (pOptics == NULL)
		return;
	pParentOptics->AddElement(pOptics);

	for (int i = 0, iChildCount(xmlNode->getChildCount()); i < iChildCount; ++i)
		AddOptics(pOptics, xmlNode->getChild(i));
}

void CopyVariable(IFuncVariable* pSrcVar, IFuncVariable* pDestVar)
{
	if (!pSrcVar || !pDestVar)
		return;

	if (pSrcVar->paramType != pDestVar->paramType)
		return;

	if (pSrcVar->paramType == e_FLOAT)
	{
		float value = pSrcVar->GetFloat();
		pDestVar->InvokeSetter((void*)&value);
	}
	else if (pSrcVar->paramType == e_BOOL)
	{
		bool value = pSrcVar->GetBool();
		pDestVar->InvokeSetter((void*)&value);
	}
	else if (pSrcVar->paramType == e_INT)
	{
		int value = pSrcVar->GetInt();
		if (HaveParameterLowBoundary(pSrcVar->name.c_str()))
			BoundaryProcess(value);
		pDestVar->InvokeSetter((void*)&value);
	}
	else if (pSrcVar->paramType == e_VEC2)
	{
		Vec2 value = pSrcVar->GetVec2();
		pDestVar->InvokeSetter((void*)&value);
	}
	else if (pSrcVar->paramType == e_VEC3)
	{
		Vec3 value = pSrcVar->GetVec3();
		pDestVar->InvokeSetter((void*)&value);
	}
	else if (pSrcVar->paramType == e_VEC4)
	{
		Vec4 value = pSrcVar->GetVec4();
		pDestVar->InvokeSetter((void*)&value);
	}
	else if (pSrcVar->paramType == e_COLOR)
	{
		ColorF value = pSrcVar->GetColorF();
		pDestVar->InvokeSetter((void*)&value);
	}
	else if (pSrcVar->paramType == e_MATRIX33)
	{
		Matrix33 value = pSrcVar->GetMatrix33();
		pDestVar->InvokeSetter((void*)&value);
	}
	else if (pSrcVar->paramType == e_TEXTURE2D || pSrcVar->paramType == e_TEXTURE3D || pSrcVar->paramType == e_TEXTURE_CUBE)
	{
		ITexture* value = pSrcVar->GetTexture();
		pDestVar->InvokeSetter((void*)value);
	}
}

void CopyOpticsAboutSameOpticsType(IOpticsElementBasePtr pSrcOptics, IOpticsElementBasePtr pDestOptics)
{
	if (pSrcOptics->GetType() != pDestOptics->GetType())
		return;

	DynArray<FuncVariableGroup> srcVarGroups = pSrcOptics->GetEditorParamGroups();
	DynArray<FuncVariableGroup> destVarGroups = pDestOptics->GetEditorParamGroups();

	if (srcVarGroups.size() != destVarGroups.size())
		return;

	pDestOptics->SetEnabled(pSrcOptics->IsEnabled());

	for (int i = 0, iVarGroupSize(srcVarGroups.size()); i < iVarGroupSize; ++i)
	{
		FuncVariableGroup srcVarGroup = srcVarGroups[i];
		FuncVariableGroup destVarGroup = destVarGroups[i];

		if (srcVarGroup.GetVariableCount() != destVarGroup.GetVariableCount())
			continue;

		for (int k = 0, iVarSize(srcVarGroup.GetVariableCount()); k < iVarSize; ++k)
		{
			IFuncVariable* pSrcVar = srcVarGroup.GetVariable(k);
			IFuncVariable* pDestVar = destVarGroup.GetVariable(k);
			CopyVariable(pSrcVar, pDestVar);
		}
	}
}

bool FindGroupByName(DynArray<FuncVariableGroup>& groupList, const string& name, int& outIndex)
{
	for (int i = 0, iVarGroupSize(groupList.size()); i < iVarGroupSize; ++i)
	{
		if (name == groupList[i].GetName())
		{
			outIndex = i;
			return true;
		}
	}
	return false;
}

void CopyOpticsAboutDifferentOpticsType(IOpticsElementBasePtr pSrcOptics, IOpticsElementBasePtr pDestOptics)
{
	if (pSrcOptics->GetType() == pDestOptics->GetType())
		return;

	DynArray<FuncVariableGroup> srcVarGroups = pSrcOptics->GetEditorParamGroups();
	DynArray<FuncVariableGroup> destVarGroups = pDestOptics->GetEditorParamGroups();

	pDestOptics->SetEnabled(pSrcOptics->IsEnabled());

	for (int i = 0, iVarGroupSize(destVarGroups.size()); i < iVarGroupSize; ++i)
	{
		FuncVariableGroup destVarGroup = destVarGroups[i];
		int nIndex = 0;
		if (!FindGroupByName(srcVarGroups, destVarGroup.GetName(), nIndex))
			continue;
		FuncVariableGroup srcVarGroup = srcVarGroups[nIndex];

		for (int k = 0, iVarSize(destVarGroup.GetVariableCount()); k < iVarSize; ++k)
		{
			IFuncVariable* pDestVar = destVarGroup.GetVariable(k);
			IFuncVariable* pSrcVar = srcVarGroup.FindVariable(pDestVar->name.c_str());
			CopyVariable(pSrcVar, pDestVar);
		}
	}
}

void CopyOptics(IOpticsElementBasePtr pSrcOptics, IOpticsElementBasePtr pDestOptics, bool bReculsiveCopy)
{
	if (pSrcOptics->GetType() == pDestOptics->GetType())
		CopyOpticsAboutSameOpticsType(pSrcOptics, pDestOptics);
	else
		CopyOpticsAboutDifferentOpticsType(pSrcOptics, pDestOptics);

	if (bReculsiveCopy)
	{
		pDestOptics->RemoveAll();
		for (int i = 0, iChildCount(pSrcOptics->GetElementCount()); i < iChildCount; ++i)
		{
			IOpticsElementBasePtr pSrcChildOptics = pSrcOptics->GetElementAt(i);
			IOpticsElementBasePtr pNewOptics = gEnv->pOpticsManager->Create(pSrcChildOptics->GetType());
			if (pNewOptics == NULL)
				continue;
			pNewOptics->SetName(pSrcChildOptics->GetName());
			CopyOptics(pSrcChildOptics, pNewOptics, bReculsiveCopy);
			pDestOptics->AddElement(pNewOptics);
		}
	}
}

CEntityObject* GetSelectedLightEntity()
{
	CBaseObject* pSelectedObj = GetIEditorImpl()->GetSelectedObject();
	if (pSelectedObj == NULL)
		return NULL;
	if (!pSelectedObj->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		return NULL;
	CEntityObject* pEntity = (CEntityObject*)pSelectedObj;
	if (!pEntity->IsLight())
		return NULL;
	return pEntity;
}

void GetSelectedLightEntities(std::vector<CEntityObject*>& outLightEntities)
{
	const CSelectionGroup* pSelectionGroup = GetIEditorImpl()->GetSelection();
	if (pSelectionGroup == NULL)
		return;
	int nSelectionCount = pSelectionGroup->GetCount();
	outLightEntities.reserve(nSelectionCount);
	for (int i = 0; i < nSelectionCount; ++i)
	{
		CBaseObject* pSelectedObj = pSelectionGroup->GetObject(i);
		if (!pSelectedObj->IsKindOf(RUNTIME_CLASS(CEntityObject)))
			continue;
		CEntityObject* pEntity = (CEntityObject*)pSelectedObj;
		if (!pEntity->IsLight())
			continue;
		outLightEntities.push_back(pEntity);
	}
}

// Workaround: This is a workaround for new light components to also be able to be selected in the lens flare editor
void ApplyOpticsToSelectedEntityWithComponent(const string& opticsFullName, IOpticsElementBasePtr pOptics)
{
	const CSelectionGroup* pSelectionGroup = GetIEditorImpl()->GetSelection();
	if (pSelectionGroup == nullptr)
		return;

	const int32 selectionCount = pSelectionGroup->GetCount();
	for (int32 i = 0; i < selectionCount; ++i)
	{
		CBaseObject* pSelectedObj = pSelectionGroup->GetObject(i);

		if (const IEntity* pEntity = pSelectedObj->GetIEntity())
		{
			// Get either the point light component or projector light component
			IEntityComponent* pLightComponent = pEntity->GetComponentByTypeId("0A86908D-642F-4590-ACEF-484E8E39F31B"_cry_guid);
			pLightComponent = pLightComponent == nullptr ? pEntity->GetComponentByTypeId("07D0CAD1-8E79-4177-9ADD-A2464A009FA5"_cry_guid) : pLightComponent;

			if (pLightComponent)
			{
				static_cast<Cry::DefaultComponents::ILightComponent*>(pLightComponent)->SetOptics(opticsFullName);
			}
		}
	}

	GetIEditor()->GetObjectManager()->EmitPopulateInspectorEvent();
}
//~Workaround

IOpticsElementBasePtr GetSelectedLightOptics()
{
	CEntityObject* pEntity = GetSelectedLightEntity();
	if (pEntity == NULL)
		return NULL;
	IOpticsElementBasePtr pEntityOptics = pEntity->GetOpticsElement();
	if (pEntityOptics == NULL)
		return NULL;
	return pEntityOptics;
}

IOpticsElementBasePtr FindOptics(IOpticsElementBasePtr pStartOptics, const string& name)
{
	if (pStartOptics == NULL)
		return NULL;

	if (!strcmp(pStartOptics->GetName(), name))
		return pStartOptics;

	for (int i = 0, iElementCount(pStartOptics->GetElementCount()); i < iElementCount; ++i)
	{
		IOpticsElementBasePtr pFoundOptics = FindOptics(pStartOptics->GetElementAt(i), name);
		if (pFoundOptics)
			return pFoundOptics;
	}

	return NULL;
}

void RemoveOptics(IOpticsElementBasePtr pOptics)
{
	if (pOptics == NULL)
		return;

	IOpticsElementBasePtr pParent = pOptics->GetParent();
	if (pParent == NULL)
		return;

	for (int i = 0, iElementCount(pParent->GetElementCount()); i < iElementCount; ++i)
	{
		if (pOptics == pParent->GetElementAt(i))
		{
			pParent->Remove(i);
			break;
		}
	}
}

void ChangeOpticsRootName(IOpticsElementBasePtr pOptics, const string& newRootName)
{
	if (pOptics == NULL)
		return;

	string opticsName = pOptics->GetName();
	string opticsRootName;

	int nPos = opticsName.Find(".");
	if (nPos == -1)
		opticsRootName = opticsName;
	else
		opticsRootName = opticsName.Left(nPos);

	opticsName.Replace(opticsRootName, newRootName);
	pOptics->SetName(opticsName);

	for (int i = 0, iElementSize(pOptics->GetElementCount()); i < iElementSize; ++i)
		ChangeOpticsRootName(pOptics->GetElementAt(i), newRootName);
}

void UpdateClipboard(const string& type, const string& groupName, bool bPasteAtSameLevel, std::vector<SClipboardData>& dataList)
{
	XmlNodeRef rootNode = CreateXMLFromClipboardData(type, groupName, bPasteAtSameLevel, dataList);
	CClipboard clipboard;
	clipboard.Put(rootNode);
}

XmlNodeRef CreateXMLFromClipboardData(const string& type, const string& groupName, bool bPasteAtSameLevel, std::vector<SClipboardData>& dataList)
{
	XmlNodeRef rootNode = gEnv->pSystem->CreateXmlNode();
	rootNode->setTag("FlareDB");
	rootNode->setAttr("Type", type);
	rootNode->setAttr("GroupName", groupName);
	rootNode->setAttr("PasteAtSameLevel", bPasteAtSameLevel);

	for (int i = 0, iDataSize(dataList.size()); i < iDataSize; ++i)
	{
		XmlNodeRef xmlNode = gEnv->pSystem->CreateXmlNode();
		xmlNode->setTag("Data");
		dataList[i].FillXmlNode(xmlNode);
		rootNode->addChild(xmlNode);
	}

	return rootNode;
}

void UpdateOpticsName(IOpticsElementBasePtr pOptics)
{
	if (pOptics == NULL)
		return;

	IOpticsElementBasePtr pParent = pOptics->GetParent();
	if (pParent)
	{
		string oldName = pOptics->GetName();
		string parentName = pParent->GetName();
		string updatedName = parentName + string(".") + GetShortName(oldName);
		pOptics->SetName(updatedName);
	}

	for (int i = 0, iElementCount(pOptics->GetElementCount()); i < iElementCount; ++i)
		UpdateOpticsName(pOptics->GetElementAt(i));
}

string ReplaceLastName(const string& fullName, const string& shortName)
{
	int nPos = fullName.ReverseFind('.');
	if (nPos == -1)
		return shortName;
	string newName = fullName.Left(nPos + 1);
	newName += shortName;
	return newName;
}

IFuncVariable* GetFuncVariable(IOpticsElementBasePtr pOptics, int nFuncKey)
{
	if (pOptics == NULL)
		return NULL;

	int nGroupIndex = (nFuncKey & 0xFFFF0000) >> 16;
	int nVarIndex = (nFuncKey & 0x0000FFFF);

	DynArray<FuncVariableGroup> paramGroups = pOptics->GetEditorParamGroups();

	if (nGroupIndex >= paramGroups.size())
		return NULL;

	FuncVariableGroup* pGroup = &paramGroups[nGroupIndex];
	if (pGroup == NULL)
		return NULL;

	if (nVarIndex >= pGroup->GetVariableCount())
		return NULL;

	return pGroup->GetVariable(nVarIndex);
}

void GetLightEntityObjects(std::vector<CEntityObject*>& outEntityLights)
{
	std::vector<CBaseObject*> pEntityObjects;
	GetIEditorImpl()->GetObjectManager()->FindObjectsOfType(RUNTIME_CLASS(CEntityObject), pEntityObjects);
	for (int i = 0, iObjectSize(pEntityObjects.size()); i < iObjectSize; ++i)
	{
		CEntityObject* pEntity = (CEntityObject*)pEntityObjects[i];
		if (pEntity == NULL)
			continue;
		if (pEntity->CheckFlags(OBJFLAG_DELETED))
			continue;
		if (!pEntity->IsLight())
			continue;
		outEntityLights.push_back(pEntity);
	}
}

void OutputOpticsDebug(IOpticsElementBasePtr pOptics)
{
	string opticsName;
	opticsName.Format("Optics Name : %s\n", pOptics->GetName());
	OutputDebugString(opticsName);

	DynArray<FuncVariableGroup> groupArray(pOptics->GetEditorParamGroups());
	for (int i = 0, iGroupCount(groupArray.size()); i < iGroupCount; ++i)
	{
		FuncVariableGroup* pGroup = &groupArray[i];

		string groupStr;
		groupStr.Format("\tGroup : %s\n", pGroup->GetName());
		OutputDebugString(groupStr);

		for (int k = 0, iParamCount(pGroup->GetVariableCount()); k < iParamCount; ++k)
		{
			IFuncVariable* pVar = pGroup->GetVariable(k);
			if (pVar == NULL)
				continue;
			string str;
			if (pVar->paramType == e_FLOAT)
			{
				str.Format("\t\t%s : %f\n", pVar->name.c_str(), pVar->GetFloat());
			}
			else if (pVar->paramType == e_INT)
			{
				str.Format("\t\t%s : %d\n", pVar->name.c_str(), pVar->GetInt());
			}
			else if (pVar->paramType == e_BOOL)
			{
				str.Format("\t\t%s : %s\n", pVar->name.c_str(), pVar->GetBool() ? "TRUE" : "FALSE");
			}
			else if (pVar->paramType == e_VEC2)
			{
				str.Format("\t\t%s : %f,%f\n", pVar->name.c_str(), pVar->GetVec2().x, pVar->GetVec2().y);
			}
			else if (pVar->paramType == e_VEC3)
			{
				str.Format("\t\t%s : %f,%f,%f\n", pVar->name.c_str(), pVar->GetVec3().x, pVar->GetVec3().y, pVar->GetVec3().z);
			}
			else if (pVar->paramType == e_VEC4)
			{
				str.Format("\t\t%s : %f,%f,%f,%f\n", pVar->name.c_str(), pVar->GetVec4().x, pVar->GetVec4().y, pVar->GetVec4().z);
			}
			else if (pVar->paramType == e_COLOR)
			{
				str.Format("\t\t%s : %f,%f,%f,%f\n", pVar->name.c_str(), pVar->GetColorF().r, pVar->GetColorF().g, pVar->GetColorF().b, pVar->GetColorF().a);
			}
			else if (pVar->paramType == e_TEXTURE2D || pVar->paramType == e_TEXTURE3D || pVar->paramType == e_TEXTURE_CUBE)
			{
				if (pVar->GetTexture())
					str.Format("\t\t%s : %s\n", pVar->name.c_str(), pVar->GetTexture()->GetName());
				else
					str.Format("\t\t%s : NULL\n", pVar->name.c_str());
			}
			OutputDebugString(str);
		}
	}

	for (int i = 0, iChildCount(pOptics->GetElementCount()); i < iChildCount; ++i)
	{
		OutputOpticsDebug(pOptics->GetElementAt(i));
	}
}

void FillParams(XmlNodeRef& paramNode, IOpticsElementBase* pOptics)
{
	if (pOptics == NULL)
		return;
	DynArray<FuncVariableGroup> groupArray(pOptics->GetEditorParamGroups());
	for (int i = 0, iGroupCount(paramNode->getChildCount()); i < iGroupCount; ++i)
	{
		XmlNodeRef groupNode = paramNode->getChild(i);
		if (groupNode == NULL)
			continue;

		int nGroupIndex(0);
		if (!FindGroup(groupNode->getTag(), pOptics, nGroupIndex))
			continue;

		FuncVariableGroup* pGroup = &groupArray[nGroupIndex];
		for (int k = 0, iParamCount(pGroup->GetVariableCount()); k < iParamCount; ++k)
		{
			IFuncVariable* pVar = pGroup->GetVariable(k);
			if (pVar == NULL)
				continue;

			if (pVar->paramType == e_FLOAT)
			{
				float fValue(0);
				if (groupNode->getAttr(pVar->name.c_str(), fValue))
					pVar->InvokeSetter((void*)&fValue);
			}
			else if (pVar->paramType == e_INT)
			{
				int nValue(0);
				if (groupNode->getAttr(pVar->name.c_str(), nValue))
					pVar->InvokeSetter((void*)&nValue);
			}
			else if (pVar->paramType == e_BOOL)
			{
				bool bValue(false);
				if (groupNode->getAttr(pVar->name.c_str(), bValue))
					pVar->InvokeSetter((void*)&bValue);
			}
			else if (pVar->paramType == e_VEC2)
			{
				Vec2 vec2Value;
				if (groupNode->getAttr(pVar->name.c_str(), vec2Value))
					pVar->InvokeSetter((void*)&vec2Value);
			}
			else if (pVar->paramType == e_VEC3)
			{
				Vec3 vec3Value;
				if (groupNode->getAttr(pVar->name.c_str(), vec3Value))
					pVar->InvokeSetter((void*)&vec3Value);
			}
			else if (pVar->paramType == e_VEC4)
			{
				const char* strVec4Value;
				if (groupNode->getAttr(pVar->name.c_str(), &strVec4Value))
				{
					Vec4 vec4Value;
					ExtractVec4FromString(strVec4Value, vec4Value);
					pVar->InvokeSetter((void*)&vec4Value);
				}
			}
			else if (pVar->paramType == e_COLOR)
			{
				Vec3 vec3Value;
				if (!groupNode->getAttr(pVar->name.c_str(), vec3Value))
					continue;
				string alphaStr = string(pVar->name.c_str()) + ".alpha";
				int alpha;
				if (!groupNode->getAttr(alphaStr.c_str(), alpha))
					continue;
				ColorF color;
				color.r = vec3Value.x;
				color.g = vec3Value.y;
				color.b = vec3Value.z;
				color.a = (float)alpha / 255.0f;
				pVar->InvokeSetter((void*)&color);
			}
			else if (pVar->paramType == e_MATRIX33)
			{
				//reserved
			}
			else if (pVar->paramType == e_TEXTURE2D || pVar->paramType == e_TEXTURE3D || pVar->paramType == e_TEXTURE_CUBE)
			{
				const char* textureName;
				if (!groupNode->getAttr(pVar->name.c_str(), &textureName))
					continue;
				ITexture* pTexture = NULL;
				if (textureName && strlen(textureName) > 0)
				{
					pTexture = gEnv->pRenderer->EF_LoadTexture(textureName);
				}
				pVar->InvokeSetter((void*)pTexture);
				if (pTexture)
				{
					pTexture->Release();
				}
			}
		}
	}
}

void GetExpandedItemNames(CBaseLibraryItem* item, const string& groupName, const string& itemName, string& outNameWithGroup, string& outFullName)
{
	string name;
	if (!groupName.IsEmpty())
		name = groupName + ".";
	name += itemName;
	string fullName = name;
	if (item->GetLibrary())
		fullName = item->GetLibrary()->GetName() + "." + name;
	outNameWithGroup = name;
	outFullName = fullName;
}

bool CreateDragImage(CTreeCtrl& treeCtrl, HTREEITEM hItem, CImageList& imageList)
{
	if (hItem == NULL)
		return false;

	CBitmap bitmap;
	CDC* pDC = treeCtrl.GetDC();
	if (pDC == NULL)
		return false;

	CString itemText = treeCtrl.GetItemText(hItem);

	CSize stringSize = pDC->GetTextExtent(itemText);
	CRect itemRect(0, 0, stringSize.cx, stringSize.cy);
	if (!bitmap.CreateCompatibleBitmap(pDC, itemRect.Width(), itemRect.Height()))
		return false;

	CDC memDC;
	memDC.CreateCompatibleDC(pDC);
	memDC.SelectObject(bitmap);
	memDC.SetBkColor(RGB(255, 255, 255));
	memDC.Rectangle(itemRect);
	memDC.DrawText(itemText, itemRect, DT_NOCLIP);

	imageList.DeleteImageList();
	imageList.Create(itemRect.Width(), itemRect.Height(), ILC_COLOR, 1, 2);
	if (imageList.Add(&bitmap, RGB(255, 255, 255)) < 0)
		return false;

	return true;
}

CPoint GetDragCursorPos(CWnd* pWnd, const CPoint& point)
{
	CRect rc;
	AfxGetMainWnd()->GetWindowRect(rc);
	CPoint p(point);
	pWnd->ClientToScreen(&p);
	p.x -= rc.left;
	p.y -= rc.top;
	return p;
}

HTREEITEM GetTreeItemByHitTest(CTreeCtrl& treeCtrl)
{
	CPoint point;
	GetCursorPos(&point);
	treeCtrl.ScreenToClient(&point);
	UINT uFlags;
	HTREEITEM hItem = treeCtrl.HitTest(point, &uFlags);
	if ((hItem != NULL) && (TVHT_ONITEM & uFlags))
		return hItem;
	return NULL;
}

void BeginDragDrop(CTreeCtrl& treeCtrl, const CPoint& startPos, XmlNodeRef xmlData)
{
	HTREEITEM hSelectedItem = treeCtrl.GetSelectedItem();

	if (hSelectedItem == NULL)
		return;

	LensFlareUtil::SDragAndDropInfo& dragInfo = LensFlareUtil::GetDragDropInfo();

	dragInfo.m_hDraggedItem = hSelectedItem;
	dragInfo.m_hPrevSelectedItem = hSelectedItem;
	dragInfo.m_XMLContents = xmlData;

	if (LensFlareUtil::CreateDragImage(treeCtrl, dragInfo.m_hDraggedItem, dragInfo.m_Image))
	{
		IMAGEINFO imageInfo;
		if (dragInfo.m_Image.GetImageInfo(0, &imageInfo))
		{
			dragInfo.m_Image.EndDrag();
			int half_width = (imageInfo.rcImage.right - imageInfo.rcImage.left) / 2;
			int half_height = (imageInfo.rcImage.bottom - imageInfo.rcImage.top) / 2;
			if (dragInfo.m_Image.BeginDrag(0, CPoint(half_width, half_height)))
			{
				if (dragInfo.m_Image.DragEnter(AfxGetMainWnd(), LensFlareUtil::GetDragCursorPos(&treeCtrl, startPos)))
				{
					dragInfo.m_bDragging = true;
					::SetCapture(treeCtrl.GetSafeHwnd());
					GetIEditorImpl()->GetIUndoManager()->Suspend();
				}
			}
		}
	}

	if (!dragInfo.m_bDragging)
		dragInfo.Reset();
}

void MoveDragDrop(CTreeCtrl& treeCtrl, const CPoint& cursorPos, Functor2<HTREEITEM, const CPoint&>* pUpdateFunctor)
{
	LensFlareUtil::SDragAndDropInfo& dragInfo = LensFlareUtil::GetDragDropInfo();
	if (dragInfo.m_bDragging)
	{
		dragInfo.m_Image.DragMove(LensFlareUtil::GetDragCursorPos(&treeCtrl, cursorPos));
		HTREEITEM hItem = LensFlareUtil::GetTreeItemByHitTest(treeCtrl);

		CPoint screenPos(cursorPos);
		treeCtrl.ClientToScreen(&screenPos);

		if (hItem)
		{
			if (treeCtrl.ItemHasChildren(hItem))
			{
				treeCtrl.Expand(hItem, TVE_EXPAND);
				treeCtrl.RedrawWindow();
			}
			if (dragInfo.m_hPrevSelectedItem != hItem)
			{
				if (pUpdateFunctor)
					(*pUpdateFunctor)(hItem, screenPos);
				dragInfo.m_hPrevSelectedItem = hItem;
			}
		}
		else if (pUpdateFunctor)
		{
			(*pUpdateFunctor)(hItem, screenPos);
		}
	}
}

void EndDragDrop(CTreeCtrl& treeCtrl, const CPoint& cursorPos, Functor2<XmlNodeRef, const CPoint&>* pPasteFunctor)
{
	LensFlareUtil::SDragAndDropInfo& dragInfo = LensFlareUtil::GetDragDropInfo();
	if (!dragInfo.m_bDragging)
		return;
	dragInfo.m_Image.DragLeave(AfxGetMainWnd());
	treeCtrl.SelectItem(dragInfo.m_hPrevSelectedItem);
	GetIEditorImpl()->GetIUndoManager()->Resume();
	if (pPasteFunctor)
	{
		CPoint screenPos(cursorPos);
		treeCtrl.ClientToScreen(&screenPos);
		(*pPasteFunctor)(dragInfo.m_XMLContents, screenPos);
	}
	dragInfo.m_Image.EndDrag();
	dragInfo.Reset();
	::ReleaseCapture();
}

bool IsPointInWindow(const CWnd* pWindow, const CPoint& screenPos)
{
	if (pWindow == NULL)
		return false;

	CRect windowRect;
	pWindow->GetWindowRect(&windowRect);

	return windowRect.PtInRect(screenPos) ? true : false;
}

int FindOpticsIndexUnderParentOptics(IOpticsElementBasePtr pOptics, IOpticsElementBasePtr pParentOptics)
{
	for (int i = 0, iElementSize(pParentOptics->GetElementCount()); i < iElementSize; ++i)
	{
		if (pParentOptics->GetElementAt(i) == pOptics)
			return i + 1;
	}
	return -1;
}
}


// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2012.
// -------------------------------------------------------------------------
//  File name:   FlareUtil.h
//  Created:     3/April/2012 by Jaesik.
////////////////////////////////////////////////////////////////////////////

#include <CryRenderer/IFlares.h>
class CEntityObject;

#define LENSFLARE_ELEMENT_TREE  "LensFlareElementTree"
#define LENSFLARE_ITEM_TREE     "LensFlareItemTree"
#define FLARECLIPBOARDTYPE_COPY "Copy"
#define FLARECLIPBOARDTYPE_CUT  "Cut"

namespace LensFlareUtil
{
inline bool IsElement(EFlareType type)
{
	return type != eFT__Base__ && type != eFT_Root && type != eFT_Group;
}
inline bool IsGroup(EFlareType type)
{
	return type == eFT_Root || type == eFT_Group;
}
inline bool IsValidFlare(EFlareType type)
{
	return type >= eFT__Base__ && type < eFT_Max;
}

inline void AddVariable(CVariableBase& varArray, CVariableBase& var, const char* varName, const char* humanVarName, const char* description, char dataType = IVariable::DT_SIMPLE)
{
	if (varName)
		var.SetName(varName);
	if (humanVarName)
		var.SetHumanName(humanVarName);
	if (description)
		var.SetDescription(description);
	var.SetDataType(dataType);
	varArray.AddVariable(&var);
}

inline void AddVariable(CVarBlock* vars, CVariableBase& var, const char* varName, const char* humanVarName, const char* description, unsigned char dataType = IVariable::DT_SIMPLE)
{
	if (varName)
		var.SetName(varName);
	if (humanVarName)
		var.SetHumanName(humanVarName);
	if (description)
		var.SetDescription(description);
	var.SetDataType(dataType);
	vars->AddVariable(&var);
}

inline bool GetFlareType(const string& typeName, EFlareType& outType)
{
	FlareInfoArray::Props array = FlareInfoArray::Get();
	for (size_t i = 0; i < array.size; ++i)
	{
		if (typeName == array.p[i].name)
		{
			outType = array.p[i].type;
			return true;
		}
	}
	return false;
}

inline bool GetFlareTypeName(string& outTypeName, IOpticsElementBasePtr pOptics)
{
	if (pOptics == NULL)
		return false;

	const FlareInfoArray::Props array = FlareInfoArray::Get();
	for (size_t i = 0; i < array.size; ++i)
	{
		if (array.p[i].type == pOptics->GetType())
		{
			outTypeName = array.p[i].name;
			return true;
		}
	}

	return false;
}

inline string GetShortName(const string& name)
{
	int nPos = name.ReverseFind('.');
	if (nPos == -1)
		return name;
	return name.Right(name.GetLength() - nPos - 1);
}

inline string GetGroupNameFromName(const string& name)
{
	int nPos = name.ReverseFind('.');
	if (nPos == -1)
		return name;
	return name.Left(nPos);
}

inline string GetGroupNameFromFullName(const string& fullItemName)
{
	int nLastDotPos = fullItemName.ReverseFind('.');
	if (nLastDotPos == -1)
		return fullItemName;

	string name = GetGroupNameFromName(fullItemName);

	int nFirstDotPos = name.Find('.');
	if (nFirstDotPos == -1)
		return name;

	return name.Right(name.GetLength() - nFirstDotPos - 1);
}

inline bool HaveParameterLowBoundary(const string& paramName)
{
	if (!_stricmp(paramName, "Noiseseed") || !_stricmp(paramName, "translation") || !_stricmp(paramName, "position") || !_stricmp(paramName, "rotation"))
		return false;
	return true;
}

inline int MakeFuncKey(int nGroupIndex, int nVarIndex)
{
	return (nGroupIndex << 16) | nVarIndex;
}

template<class T>
inline void BoundaryProcess(T& fValue)
{
	if (fValue < 0)
		fValue = 0;
}

inline void BoundaryProcess(Vec2& v)
{
	if (v.x < 0)
		v.x = 0;
	if (v.y < 0)
		v.y = 0;
}

inline void BoundaryProcess(Vec3& v)
{
	if (v.x < 0)
		v.x = 0;
	if (v.y < 0)
		v.y = 0;
	if (v.z < 0)
		v.z = 0;
}

inline void BoundaryProcess(Vec4& v)
{
	if (v.x < 0)
		v.x = 0;
	if (v.y < 0)
		v.y = 0;
	if (v.z < 0)
		v.z = 0;
	if (v.w < 0)
		v.w = 0;
}

inline bool ExtractVec4FromString(const string& buffer, Vec4& outVec4)
{
	int nCount(0);
	string copiedBuffer(buffer);
	int commaPos(0);
	while (nCount < 4 || commaPos != -1)
	{
		commaPos = copiedBuffer.find(",");
		string strV;
		if (commaPos == -1)
		{
			strV = copiedBuffer;
		}
		else
		{
			strV = copiedBuffer.Left(commaPos);
			strV += ",";
		}
		copiedBuffer.replace(0, commaPos + 1, "");
		outVec4[nCount++] = (float)atof(strV.c_str());
	}
	return nCount == 4;
}

void        FillParams(XmlNodeRef& paramNode, IOpticsElementBase* pOptics);

inline bool FindGroup(const char* groupName, IOpticsElementBase* pOptics, int& nOutGroupIndex)
{
	if (groupName == NULL)
		return NULL;

	DynArray<FuncVariableGroup> groupArray = pOptics->GetEditorParamGroups();
	for (int i = 0, iCount(groupArray.size()); i < iCount; ++i)
	{
		if (!stricmp(groupArray[i].GetName(), groupName))
		{
			nOutGroupIndex = i;
			return true;
		}
	}
	return false;
}

struct SClipboardData
{
	SClipboardData(){}
	~SClipboardData(){}
	SClipboardData(const string& from, const string& lensFlareFullPath, const string& lensOpticsPath) :
		m_From(from),
		m_LensFlareFullPath(lensFlareFullPath),
		m_LensOpticsPath(lensOpticsPath)
	{
	}

	void FillXmlNode(XmlNodeRef node)
	{
		if (node == NULL)
			return;
		node->setAttr("From", m_From);
		node->setAttr("FlareFullPath", m_LensFlareFullPath);
		node->setAttr("OpticsPath", m_LensOpticsPath);
	}

	void FillThisFromXmlNode(XmlNodeRef node)
	{
		if (node == NULL)
			return;
		node->getAttr("From", m_From);
		node->getAttr("FlareFullPath", m_LensFlareFullPath);
		node->getAttr("OpticsPath", m_LensOpticsPath);
	}

	string m_From;
	string m_LensFlareFullPath;
	string m_LensOpticsPath;
};

struct SDragAndDropInfo
{
	SDragAndDropInfo()
	{
		m_hDraggedItem = NULL;
		m_hPrevSelectedItem = NULL;
		m_XMLContents = NULL;
		m_bDragging = false;
	}
	~SDragAndDropInfo()
	{
		Reset();
	}
	void Reset()
	{
		m_Image.DeleteImageList();
		m_hDraggedItem = NULL;
		m_XMLContents = NULL;
		m_bDragging = false;
	}
	CImageList m_Image;
	bool       m_bDragging;
	XmlNodeRef m_XMLContents;
	HTREEITEM  m_hDraggedItem;
	HTREEITEM  m_hPrevSelectedItem;
};

inline SDragAndDropInfo& GetDragDropInfo()
{
	static SDragAndDropInfo dragDropInfo;
	return dragDropInfo;
}

void                  BeginDragDrop(CTreeCtrl& treeCtrl, const CPoint& cursorPos, XmlNodeRef xmlData);
void                  MoveDragDrop(CTreeCtrl& treeCtrl, const CPoint& cursorPos, Functor2<HTREEITEM, const CPoint&>* pUpdateFunctor);
void                  EndDragDrop(CTreeCtrl& treeCtrl, const CPoint& cursorPos, Functor2<XmlNodeRef, const CPoint&>* pPasteFunctor);

int                   FindOpticsIndexUnderParentOptics(IOpticsElementBasePtr pOptics, IOpticsElementBasePtr pParentOptics);
bool                  IsPointInWindow(const CWnd* pWindow, const CPoint& screenPos);
HTREEITEM             GetTreeItemByHitTest(CTreeCtrl& treeCtrl);
CPoint                GetDragCursorPos(CWnd* pWnd, const CPoint& point);
bool                  CreateDragImage(CTreeCtrl& treeCtrl, HTREEITEM hItem, CImageList& imageList);
void                  GetExpandedItemNames(CBaseLibraryItem* item, const string& groupName, const string& itemName, string& outNameWithGroup, string& outFullName);
void                  GetSelectedLightEntities(std::vector<CEntityObject*>& outLightEntities);
void                  ApplyOpticsToSelectedEntityWithComponent(const string& opticsFullName, IOpticsElementBasePtr pOptics);
void                  GetLightEntityObjects(std::vector<CEntityObject*>& outEntityLights);
IFuncVariable*        GetFuncVariable(IOpticsElementBasePtr pOptics, int nFuncKey);
string                ReplaceLastName(const string& fullName, const string& shortName);
void                  UpdateOpticsName(IOpticsElementBasePtr pOptics);
XmlNodeRef            CreateXMLFromClipboardData(const string& type, const string& groupName, bool bPasteAtSameLevel, std::vector<SClipboardData>& dataList);
void                  UpdateClipboard(const string& type, const string& groupName, bool bPasteAtSameLevel, std::vector<SClipboardData>& dataList);
void                  ChangeOpticsRootName(IOpticsElementBasePtr pOptics, const string& newRootName);
void                  RemoveOptics(IOpticsElementBasePtr pOptics);
IOpticsElementBasePtr FindOptics(IOpticsElementBasePtr pStartOptics, const string& name);
CEntityObject*        GetSelectedLightEntity();
IOpticsElementBasePtr GetSelectedLightOptics();
IOpticsElementBasePtr CreateOptics(XmlNodeRef& xmlNode);
IOpticsElementBasePtr CreateOptics(IOpticsElementBasePtr pOptics, bool bForceTypeToGroup = false);
bool                  FillOpticsFromXML(IOpticsElementBasePtr pOptics, XmlNodeRef& xmlNode);
bool                  CreateXmlData(IOpticsElementBasePtr pOptics, XmlNodeRef& pOutNode);
void                  SetVariablesTemplateFromOptics(IOpticsElementBasePtr pOptics, CVarBlockPtr& pRootVar, std::vector<IVariable::OnSetCallback>& funcs);
void                  SetVariablesTemplateFromOptics(IOpticsElementBasePtr pOptics, CVarBlockPtr& pRootVar);
void                  CopyOptics(IOpticsElementBasePtr pSrcOptics, IOpticsElementBasePtr pDestOptics, bool bReculsiveCopy = true);
void                  OutputOpticsDebug(IOpticsElementBasePtr pOptics);
};


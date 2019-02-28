// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BoostPythonHelpers.h"
#include <CryCore/Containers/CryListenerSet.h>
#include "Commands/PythonManager.h"
#include "FilePathUtil.h"

#include <boost/python.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>

#include "boost/python/object.hpp"
#include "boost/python/tuple.hpp"
#include "boost/python/dict.hpp"
#include "boost/python/to_python_converter.hpp"
#include "boost/python/converter/registry.hpp"
#include "boost/python/suite/indexing/vector_indexing_suite.hpp"
#include <CryCore/ToolsHelpers/GuidUtil.h>
#include "Objects/ObjectLayer.h"
#include "Objects/BaseObject.h"
#include "Objects/BrushObject.h"
#include "Objects/CameraObject.h"
#include "Objects/EntityObject.h"
#include "Objects/PrefabObject.h"
#include "Objects/Group.h"
#include "Prefabs/PrefabItem.h"
#include "Prefabs/PrefabManager.h"
#include "Material/Material.h"
#include "Vegetation/VegetationObject.h"
#include "Vegetation/VegetationMap.h"
#include "Objects/GeomEntity.h"
#include "Util/MFCUtil.h"
#include "CryEditDoc.h"

struct CryLogPythonOutput : public PyScript::IPyScriptListener
{

	virtual void OnStdOut(const char* pString) override
	{
		Log(pString);
	}

	virtual void OnStdErr(const char* pString) override
	{
		Warning(pString);
	}

};

//  Commonly used Dynamic Property Setters.
void SPyWrappedProperty::SetProperties(IVariable* pVar)
{
	switch (pVar->GetType())
	{
	case IVariable::BOOL:
		this->type = SPyWrappedProperty::eType_Bool;
		pVar->Get(this->property.boolValue);
		break;

	case IVariable::INT:
		this->type = SPyWrappedProperty::eType_Int;
		pVar->Get(this->property.boolValue);
		break;

	case IVariable::FLOAT:
		this->type = SPyWrappedProperty::eType_Float;
		pVar->Get(this->property.floatValue);
		break;

	case IVariable::STRING:
		this->type = SPyWrappedProperty::eType_String;
		pVar->Get(this->stringValue);
		break;

	case IVariable::VECTOR:
		Vec3 tempVec3;
		pVar->Get(tempVec3);

		if (pVar->GetDataType() == IVariable::DT_COLOR)
		{
			this->type = SPyWrappedProperty::eType_Color;
			COLORREF col = CMFCUtils::ColorLinearToGamma(ColorF(tempVec3[0], tempVec3[1], tempVec3[2]));
			this->property.colorValue.r = (int)GetRValue(col);
			this->property.colorValue.g = (int)GetGValue(col);
			this->property.colorValue.b = (int)GetBValue(col);
		}
		else
		{
			this->type = SPyWrappedProperty::eType_Vec3;
			this->property.vecValue.x = tempVec3[0];
			this->property.vecValue.y = tempVec3[1];
			this->property.vecValue.z = tempVec3[2];
		}

		break;
	}
}

// Wrapped Engine Classes
PyGameLayer::PyGameLayer(void* layerPtr)
{
	m_layerPtr = layerPtr;

	if (m_layerPtr == NULL)
	{
		m_layerName = "";
		m_layerPath = "";
		m_layerGUID = CryGUID::Null();
		m_layerVisible = false;
		m_layerFrozen = false;
		m_layerExportable = false;
		m_layerExportLayerPak = false;
		m_layerDefaultLoaded = false;
		m_layerPhysics = false;
		return;
	}

	CObjectLayer* pLayer = static_cast<CObjectLayer*>(m_layerPtr);

	m_layerName = pLayer->GetName();
	m_layerPath = pLayer->GetLayerFilepath();
	m_layerPath.Replace("\\", "/");
	m_layerGUID = pLayer->GetGUID();
	m_layerVisible = pLayer->IsVisible();
	m_layerFrozen = pLayer->IsFrozen();
	m_layerExportable = pLayer->IsExportable();
	m_layerExportLayerPak = pLayer->IsExporLayerPak();
	m_layerDefaultLoaded = pLayer->IsDefaultLoaded();
	m_layerPhysics = pLayer->HasPhysics();

	for (int i = 0; i < pLayer->GetChildCount(); i++)
	{
		m_layerChildren.push_back(PyScript::CreatePyGameLayer(pLayer->GetChild(i)));
	}
}

PyGameLayer::~PyGameLayer()
{
	m_layerPtr = NULL;
	m_layerChildren.clear();
}

void PyGameLayer::UpdateLayer()
{
	if (m_layerPtr == NULL)
	{
		return;
	}

	CObjectLayer* pLayer = static_cast<CObjectLayer*>(m_layerPtr);

	if (m_layerName != pLayer->GetName())
	{
		pLayer->SetName(m_layerName);
	}

	if (m_layerVisible != pLayer->IsVisible())
	{
		pLayer->SetVisible(m_layerVisible, true);
	}

	if (m_layerFrozen != pLayer->IsFrozen())
	{
		pLayer->SetFrozen(m_layerFrozen, true);
	}

	if (m_layerExportable != pLayer->IsExportable())
	{
		pLayer->SetExportable(m_layerExportable);
	}

	if (m_layerExportLayerPak != pLayer->IsExporLayerPak())
	{
		pLayer->SetExportable(m_layerExportLayerPak);
	}

	if (m_layerDefaultLoaded != pLayer->IsDefaultLoaded())
	{
		pLayer->SetDefaultLoaded(m_layerDefaultLoaded);
	}

	if (m_layerPhysics != pLayer->HasPhysics())
	{
		pLayer->SetHavePhysics(m_layerPhysics);
	}
}

PyGameMaterial::PyGameMaterial(void* pMat)
{
	m_matPtr = pMat;

	if (m_matPtr == NULL)
	{
		m_matName = "";
		m_matPath = "";
		return;
	}

	CMaterial* pMaterial = static_cast<CMaterial*>(m_matPtr);

	m_matName = pMaterial->GetName();
	m_matPath = pMaterial->GetFilename();
	m_matPath.Replace("\\", "/");

	for (int i = 0; i < pMaterial->GetSubMaterialCount(); i++)
	{
		m_matSubMaterials.push_back(PyScript::CreatePyGameSubMaterial(pMaterial->GetSubMaterial(i), i));
	}
}

PyGameMaterial::~PyGameMaterial()
{
	m_matPtr = NULL;
	m_matSubMaterials.clear();
}

void PyGameMaterial::UpdateMaterial()
{
	if (m_matPtr == NULL)
	{
		return;
	}

	CMaterial* pMaterial = static_cast<CMaterial*>(m_matPtr);

	if (m_matName != pMaterial->GetName())
	{
		pMaterial->SetName(m_matName);
	}

	for (std::vector<pPyGameSubMaterial>::iterator iter = m_matSubMaterials.begin(); iter != m_matSubMaterials.end(); iter++)
	{
		iter->get()->UpdateSubMaterial();
	}
}

PyGameSubMaterial::PyGameSubMaterial(void* pMat, int id)
{
	m_matPtr = pMat;

	if (m_matPtr == NULL)
	{
		m_matId = 0;
		m_matName = "";
		m_matSurfaceType = "";
		m_matShader = "";
		return;
	}

	m_matId = id;
	CMaterial* pMaterial = static_cast<CMaterial*>(m_matPtr);

	m_matName = pMaterial->GetName();
	m_matSurfaceType = pMaterial->GetSurfaceTypeName();
	m_matShader = pMaterial->GetShaderName();

	IRenderShaderResources* pResources = pMaterial->GetShaderItem().m_pShaderResources;

	if (pResources == NULL)
	{
		return;
	}

	// Grab all ColorF resources
	std::map<string, ColorF> tMap1;
	std::map<string, ColorF>::iterator iter1;
	tMap1["Diffuse"] = pResources->GetColorValue(EFTT_DIFFUSE);
	tMap1["Specular"] = pResources->GetColorValue(EFTT_SPECULAR);
	tMap1["EmissiveColor"] = pResources->GetColorValue(EFTT_EMITTANCE);
	pSPyWrappedProperty value1(new SPyWrappedProperty);

	for (iter1 = tMap1.begin(); iter1 != tMap1.end(); ++iter1)
	{
		value1->type = SPyWrappedProperty::eType_Color;
		COLORREF color = CMFCUtils::ColorLinearToGamma(iter1->second);
		value1->property.colorValue.r = (int)GetRValue(color);
		value1->property.colorValue.g = (int)GetGValue(color);
		value1->property.colorValue.b = (int)GetBValue(color);
		m_matParams[iter1->first] = value1;
	}

	// Grab all float resources
	std::map<string, float> tMap2;
	std::map<string, float>::iterator iter2;
	tMap2["Opacity"] = pResources->GetStrengthValue(EFTT_OPACITY);
	tMap2["SpecShininess"] = pResources->GetStrengthValue(EFTT_SMOOTHNESS);
	tMap2["EmissiveIntensity"] = pResources->GetStrengthValue(EFTT_EMITTANCE);
	pSPyWrappedProperty value2(new SPyWrappedProperty);

	for (iter2 = tMap2.begin(); iter2 != tMap2.end(); ++iter2)
	{
		value2->type = SPyWrappedProperty::eType_Float;
		value2->property.floatValue = iter2->second;
		m_matParams[iter2->first] = value2;
	}

	for (int i = 0; i < EFTT_MAX; i++)
	{
		if (!pMaterial->GetShaderResources().m_Textures[i].m_Name.empty())
			m_matTextures.push_back(PyScript::CreatePyGameTexture(&pMaterial->GetShaderResources().m_Textures[i]));
	}

	// Grab all Public Variables.
	CVarBlock* varBlockPublic = pMaterial->GetPublicVars(pMaterial->GetShaderResources());

	if (varBlockPublic != NULL && !varBlockPublic->IsEmpty())
	{
		for (int i = 0; i < varBlockPublic->GetNumVariables(); i++)
		{
			IVariable* pVar = varBlockPublic->GetVariable(i);

			pSPyWrappedProperty value(new SPyWrappedProperty);
			value->SetProperties(pVar);
			m_matParams[pVar->GetName()] = value;
		}
	}

	// Grab all of the Shader Gen Params
	CVarBlock* varBlockGen = pMaterial->GetShaderGenParamsVars();

	if (varBlockPublic != NULL && !varBlockGen->IsEmpty())
	{
		for (int i = 0; i < varBlockGen->GetNumVariables(); i++)
		{
			IVariable* pVar = varBlockGen->GetVariable(i);

			pSPyWrappedProperty value(new SPyWrappedProperty);
			value->SetProperties(pVar);
			m_matParams[pVar->GetName()] = value;
		}
	}
}

PyGameSubMaterial::~PyGameSubMaterial()
{
	m_matPtr = NULL;
	m_matTextures.clear();
	m_matParams.clear();
}

void PyGameSubMaterial::UpdateSubMaterial()
{
	CMaterial* pMaterial = static_cast<CMaterial*>(m_matPtr);

	// Check for updates on all material base variables.
	if (strcmp(m_matName, pMaterial->GetName()) != 0)
	{
		pMaterial->SetName(m_matName);
	}

	if (strcmp(m_matShader, pMaterial->GetShaderName()) != 0)
	{
		pMaterial->SetShaderName(m_matShader);
	}

	if (strcmp(m_matSurfaceType, pMaterial->GetSurfaceTypeName()) != 0)
	{
		pMaterial->SetSurfaceTypeName(m_matSurfaceType);
	}

	// Check for updates on all material shader resources.
	SInputShaderResources& shdResources = pMaterial->GetShaderResources();

	shdResources.m_LMaterial.m_Opacity = m_matParams["Opacity"]->property.floatValue;
	shdResources.m_LMaterial.m_Smoothness = m_matParams["SpecShininess"]->property.floatValue;

	shdResources.m_LMaterial.m_Diffuse.r = m_matParams["Diffuse"]->property.colorValue.r;
	shdResources.m_LMaterial.m_Diffuse.g = m_matParams["Diffuse"]->property.colorValue.g;
	shdResources.m_LMaterial.m_Diffuse.b = m_matParams["Diffuse"]->property.colorValue.b;

	shdResources.m_LMaterial.m_Emittance.r = m_matParams["EmissiveColor"]->property.colorValue.r;
	shdResources.m_LMaterial.m_Emittance.g = m_matParams["EmissiveColor"]->property.colorValue.g;
	shdResources.m_LMaterial.m_Emittance.b = m_matParams["EmissiveColor"]->property.colorValue.b;
	shdResources.m_LMaterial.m_Emittance.a = m_matParams["EmissiveIntensity"]->property.floatValue;

	shdResources.m_LMaterial.m_Specular.r = m_matParams["Specular"]->property.colorValue.r;
	shdResources.m_LMaterial.m_Specular.g = m_matParams["Specular"]->property.colorValue.g;
	shdResources.m_LMaterial.m_Specular.b = m_matParams["Specular"]->property.colorValue.b;

	// Update all modify texture paths
	for (int i = 0; i < m_matTextures.size(); i++)
	{
		if (strcmp(shdResources.m_Textures[i].m_Name.c_str(), m_matTextures[i]->GetName()) != 0)
		{
			shdResources.m_Textures[i].m_Name = m_matTextures[i]->GetName();
		}
	}

	// Check for updates to all material public variables.
	CVarBlock* varBlockPublic = pMaterial->GetPublicVars(shdResources);

	if (varBlockPublic != NULL && !varBlockPublic->IsEmpty())
	{
		for (int i = 0; i < varBlockPublic->GetNumVariables(); i++)
		{
			IVariable* pVar = varBlockPublic->GetVariable(i);
			SShaderParam* pParam = NULL;

			for (int j = 0; j < shdResources.m_ShaderParams.size(); j++)
			{
				if (strcmp(pVar->GetName(), shdResources.m_ShaderParams[j].m_Name) == 0)
				{
					pParam = &shdResources.m_ShaderParams[j];

					if (pParam != NULL)
					{
						break;
					}
				}
			}

			switch (pParam->m_Type)
			{
			case eType_BYTE:
				if (pVar->GetType() == IVariable::INT)
				{
					pParam->m_Value.m_Byte = m_matParams[pVar->GetName()]->property.boolValue;
				}

				break;

			case eType_SHORT:
				if (pVar->GetType() == IVariable::INT)
				{
					pParam->m_Value.m_Short = m_matParams[pVar->GetName()]->property.intValue;
				}

				break;

			case eType_INT:
				if (pVar->GetType() == IVariable::INT)
				{
					pParam->m_Value.m_Int = m_matParams[pVar->GetName()]->property.intValue;
				}

				break;

			case eType_FLOAT:
				if (pVar->GetType() == IVariable::FLOAT)
				{
					pParam->m_Value.m_Float = m_matParams[pVar->GetName()]->property.floatValue;
				}

				break;

			case eType_FCOLOR:
				if (pVar->GetType() == IVariable::VECTOR)
				{
					pParam->m_Value.m_Color[0] = m_matParams[pVar->GetName()]->property.colorValue.r;
					pParam->m_Value.m_Color[1] = m_matParams[pVar->GetName()]->property.colorValue.g;
					pParam->m_Value.m_Color[2] = m_matParams[pVar->GetName()]->property.colorValue.b;
				}

				break;

			case eType_VECTOR:
				if (pVar->GetType() == IVariable::VECTOR)
				{
					pParam->m_Value.m_Vector[0] = m_matParams[pVar->GetName()]->property.vecValue.x;
					pParam->m_Value.m_Vector[1] = m_matParams[pVar->GetName()]->property.vecValue.y;
					pParam->m_Value.m_Vector[2] = m_matParams[pVar->GetName()]->property.vecValue.z;
				}
			}
		}
	}

	// Send all modifications from the Update call to the material.
	pMaterial->SetPublicVars(varBlockPublic, pMaterial);

	// Update all of the Shader Gen Params
	CVarBlock* varBlockGen = pMaterial->GetShaderGenParamsVars();

	if (varBlockPublic != NULL && !varBlockGen->IsEmpty())
	{
		for (int i = 0; i < varBlockGen->GetNumVariables(); i++)
		{
			IVariable* pVar = varBlockGen->GetVariable(i);
			string varName = pVar->GetName();

			// Verify the current param is stored before trying to set it.
			if (m_matParams.find(varName) == m_matParams.end())
			{
				continue;
			}

			switch (pVar->GetType())
			{
			case IVariable::BOOL:
				pVar->Set(m_matParams[varName]->property.boolValue);
				break;

			case IVariable::INT:
				pVar->Set(m_matParams[varName]->property.intValue);
				break;

			case IVariable::FLOAT:
				pVar->Set(m_matParams[varName]->property.floatValue);
				break;

			case IVariable::STRING:
				pVar->Set(m_matParams[varName]->stringValue);
				break;

			case IVariable::VECTOR:
				Vec3 value;

				if (m_matParams[varName]->type == SPyWrappedProperty::eType_Vec3)
				{
					value.x = m_matParams[varName]->property.vecValue.x;
					value.y = m_matParams[varName]->property.vecValue.y;
					value.z = m_matParams[varName]->property.vecValue.z;
				}
				else if (m_matParams[varName]->type == SPyWrappedProperty::eType_Color)
				{
					value.x = m_matParams[varName]->property.colorValue.r;
					value.y = m_matParams[varName]->property.colorValue.g;
					value.z = m_matParams[varName]->property.colorValue.b;
				}

				pVar->Set(value);
				break;
			}
		}
	}

	// Send all modifications from the Update call to the material.
	pMaterial->SetShaderGenParamsVars(varBlockGen);
}

PyGameTexture::PyGameTexture(void* pTex)
{
	m_texPtr = pTex;

	if (m_texPtr == NULL)
	{
		m_texName = "";
		return;
	}

	SEfResTexture* pTexture = static_cast<SEfResTexture*>(m_texPtr);
	m_texName = pTexture->m_Name.c_str();
}

PyGameTexture::~PyGameTexture()
{
	m_texPtr = NULL;
}

void PyGameTexture::UpdateTexture()
{
	SEfResTexture* pTexture = static_cast<SEfResTexture*>(m_texPtr);

	if (strcmp(m_texName, pTexture->m_Name) != 0)
	{
		pTexture->m_Name = m_texName;
	}
}

PyGameObject::PyGameObject(void* objPtr)
{
	m_objPtr = objPtr;
	CBaseObject* pBaseObject = static_cast<CBaseObject*>(m_objPtr);

	if (m_objPtr == NULL)
	{
		m_objGUID = CryGUID::Null();
		m_objName = "";
		m_objType = "";
		m_objPosition = Vec3(ZERO);
		m_objRotation = Vec3(ZERO);
		m_objScale = Vec3(ZERO);
		m_objBounds = AABB(AABB::RESET);
		m_objInGroup = false;
		m_objVisible = false;
		m_objFrozen = false;
		m_objSelected = false;
		return;
	}

	m_objName = pBaseObject->GetName();
	m_objType = pBaseObject->GetTypeDescription();
	m_objGUID = pBaseObject->GetId();
	m_objPosition = pBaseObject->GetPos();

	Ang3 ang = RAD2DEG(Ang3(pBaseObject->GetRotation()));
	m_objRotation = Vec3(ang.x, ang.y, ang.z);
	m_objScale = pBaseObject->GetScale();
	AABB bbox;
	pBaseObject->GetBoundBox(bbox);
	m_objBounds = bbox;

	if (pBaseObject->GetGroup() != NULL)
	{
		m_objInGroup = true;
	}
	else
	{
		m_objInGroup = false;
	}

	m_objVisible = pBaseObject->IsHidden();
	m_objFrozen = pBaseObject->IsFrozen();
	m_objSelected = pBaseObject->IsSelected();

	m_objMaterial = PyScript::CreatePyGameMaterial((CMaterial*)pBaseObject->GetMaterial());

	m_objLayer = PyScript::CreatePyGameLayer((CObjectLayer*)pBaseObject->GetLayer());

	m_objParent = PyScript::CreatePyGameObject(static_cast<CBaseObject*>(pBaseObject->GetParent()));

	// Identify what class this object is and store its pointer in a struct used for easy conversion to PyClasses
	pSPyWrappedClass cls(new SPyWrappedClass);

	if (pBaseObject->IsKindOf(RUNTIME_CLASS(CBrushObject)))
	{
		cls->type = SPyWrappedClass::eType_Brush;
		cls->ptr.reset(new PyGameBrush(m_objPtr, cls));
	}
	else if (pBaseObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		cls->type = SPyWrappedClass::eType_Entity;
		cls->ptr.reset(new PyGameEntity(m_objPtr, cls));
	}
	else if (pBaseObject->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
	{
		cls->type = SPyWrappedClass::eType_Prefab;
		cls->ptr.reset(new PyGamePrefab(m_objPtr, cls));
	}
	else if (pBaseObject->IsKindOf(RUNTIME_CLASS(CGroup)))
	{
		cls->type = SPyWrappedClass::eType_Group;
		cls->ptr.reset(new PyGameGroup(m_objPtr, cls));
	}
	else if (pBaseObject->IsKindOf(RUNTIME_CLASS(CCameraObject)))
	{
		cls->type = SPyWrappedClass::eType_Camera;
		cls->ptr.reset(new PyGameCamera(m_objPtr, cls));
	}
	else
	{
		cls->type = SPyWrappedClass::eType_None;
	}

	m_objClass = cls;
}

pPyGameObject PyGameObject::GetParent()
{
	if (m_objParent != NULL)
	{
		return m_objParent;
	}
	else
	{
		return PyScript::CreatePyGameObject(static_cast<CBaseObject*>(this->GetPtr())->GetParent());
	}
}

void PyGameObject::SetParent(pPyGameObject pParent)
{
	if (pParent->GetPtr() != NULL)
	{
		CBaseObject* pSrc = static_cast<CBaseObject*>(this->GetPtr());

		if (pSrc != NULL)
		{
			CBaseObject* pSrcParent = pSrc->GetParent();

			if (pParent->GetClassObject() != NULL)
			{
				CBaseObject* pDstParent = static_cast<CBaseObject*>(pParent->GetPtr());

				if (pSrcParent == NULL)
				{
					if (static_cast<CGroup*>(pDstParent)->IsKindOf(RUNTIME_CLASS(CGroup)))
					{
						static_cast<CGroup*>(pDstParent)->AddMember(pSrc, false);
					}

					PyScript::CreatePyGameObject(pDstParent);
				}
				else if (pDstParent != pSrcParent)
				{
					if (pSrcParent->IsKindOf(RUNTIME_CLASS(CGroup)))
					{
						static_cast<CGroup*>(pSrcParent)->RemoveMember(pSrc, false);
						static_cast<CGroup*>(pDstParent)->AddMember(pSrc, false);
					}

					PyScript::CreatePyGameObject(pDstParent);
				}
			}
		}
	}
}

void PyGameObject::UpdateObject()
{
	CBaseObject* pBaseObject = static_cast<CBaseObject*>(m_objPtr);

	GetIEditorImpl()->SetModifiedFlag();

	if (strcmp(pBaseObject->GetName(), m_objName) != 0)
	{
		pBaseObject->SetName(m_objName);
	}

	if (m_objPosition != pBaseObject->GetPos())
	{
		pBaseObject->SetPos(m_objPosition);
	}

	Ang3 ang = RAD2DEG(Ang3(pBaseObject->GetRotation()));

	if (m_objRotation != Vec3(ang.x, ang.y, ang.z))
	{
		pBaseObject->SetRotation(Quat(DEG2RAD(Ang3(m_objRotation))));
	}

	if (m_objScale != pBaseObject->GetScale())
	{
		pBaseObject->SetScale(m_objScale);
	}

	if (m_objLayer->GetName() != pBaseObject->GetLayer()->GetName())
	{
		pBaseObject->SetLayer(static_cast<CObjectLayer*>(this->GetLayer()->GetPtr()));
		m_objLayer = PyScript::CreatePyGameLayer((CObjectLayer*)pBaseObject->GetLayer());
	}

	if (m_objMaterial->GetName() != pBaseObject->GetMaterialName())
	{
		pBaseObject->SetMaterial(m_objMaterial->GetName());
		m_objMaterial = PyScript::CreatePyGameMaterial((CMaterial*)pBaseObject->GetMaterial());
	}

	pBaseObject->SetHidden(m_objVisible);
	pBaseObject->SetFrozen(m_objFrozen);

	// Lets assume since an update was requested that it is desired to run on the subobject also.
	if (pBaseObject->IsKindOf(RUNTIME_CLASS(CBrushObject)))
	{
		boost::static_pointer_cast<PyGameBrush>(m_objClass->ptr).get()->UpdateBrush();
	}
	else if (pBaseObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		boost::static_pointer_cast<PyGameEntity>(m_objClass->ptr).get()->UpdateEntity();
	}
	else if (pBaseObject->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
	{
		boost::static_pointer_cast<PyGamePrefab>(m_objClass->ptr).get()->UpdatePrefab();
	}
	else if (pBaseObject->IsKindOf(RUNTIME_CLASS(CGroup)))
	{
		boost::static_pointer_cast<PyGameGroup>(m_objClass->ptr).get()->UpdateGroup();
	}
	else if (pBaseObject->IsKindOf(RUNTIME_CLASS(CCameraObject)))
	{
		boost::static_pointer_cast<PyGameCamera>(m_objClass->ptr).get()->UpdateCamera();
	}
}

PyGamePrefab::PyGamePrefab(void* prefabPtr, pSPyWrappedClass sharedPtr)
{
	m_prefabPtr = prefabPtr;
	CPrefabObject* pPrefab = static_cast<CPrefabObject*>(m_prefabPtr);

	if (pPrefab == nullptr)
	{
		return;
	}

	// Store the Prefabs Name.
	if (pPrefab->GetPrefabItem() != nullptr)
	{
		m_prefabName = pPrefab->GetPrefabItem()->GetFullName();
	}
	else
	{
		m_prefabName = "";
	}
}

PyGamePrefab::~PyGamePrefab()
{
	CPrefabObject* pPrefab = static_cast<CPrefabObject*>(m_prefabPtr);
}

std::vector<pPyGameObject> PyGamePrefab::GetChildren()
{
	CPrefabObject* pPrefab = static_cast<CPrefabObject*>(m_prefabPtr);

	if (pPrefab != NULL)
	{
		// Return all of the Prefabs Children.
		std::vector<_smart_ptr<CBaseObject>>::iterator iter;
		std::vector<_smart_ptr<CBaseObject>> children;
		pPrefab->GetAllChildren(children);

		for (iter = children.begin(); iter != children.end(); ++iter)
		{
			m_prefabChildren.push_back(PyScript::CreatePyGameObject(*iter));
		}
	}

	return m_prefabChildren;
}

void PyGamePrefab::AddChild(pPyGameObject pObj)
{
	CPrefabObject* pPrefab = static_cast<CPrefabObject*>(m_prefabPtr);
	CBaseObject* pObject = static_cast<CBaseObject*>(pObj->GetPtr());

	if (pPrefab != NULL && pObject != NULL)
	{
		pPrefab->AddMember(pObject, true);
	}

	// Probably should add this as the parent of pObject now.
}

void PyGamePrefab::RemoveChild(pPyGameObject pObj)
{
	CPrefabObject* pPrefab = static_cast<CPrefabObject*>(m_prefabPtr);
	CBaseObject* pObject = static_cast<CBaseObject*>(pObj->GetPtr());

	if (pPrefab != NULL && pObject != NULL)
	{
		pPrefab->RemoveMember(pObject);
	}

	// Probably should remove this as the parent of pObject now.
}

bool PyGamePrefab::IsOpen()
{
	CPrefabObject* pPrefab = static_cast<CPrefabObject*>(m_prefabPtr);
	return pPrefab->IsOpen();
}

void PyGamePrefab::Open()
{
	CPrefabObject* pPrefab = static_cast<CPrefabObject*>(m_prefabPtr);
	pPrefab->Open();
}

void PyGamePrefab::Close()
{
	CPrefabObject* pPrefab = static_cast<CPrefabObject*>(m_prefabPtr);
	pPrefab->Close();
}

void PyGamePrefab::ExtractAll()
{
	CPrefabObject* pPrefab = static_cast<CPrefabObject*>(m_prefabPtr);
	std::vector<CPrefabObject*> prefabs;
	prefabs.push_back(pPrefab);
	GetIEditorImpl()->GetPrefabManager()->ExtractAllFromPrefabs(prefabs);
}

void PyGamePrefab::UpdatePrefab()
{
	CPrefabObject* pPrefab = static_cast<CPrefabObject*>(m_prefabPtr);

	if (pPrefab != NULL)
	{
		if (strcmp(m_prefabName, "") != 0 && strcmp(m_prefabName, pPrefab->GetPrefabItem()->GetFullName()) != 0)
		{
			pPrefab->SetName(m_prefabName);
		}
	}
}

PyGameGroup::PyGameGroup(void* groupPtr, pSPyWrappedClass sharedPtr)
{
	m_groupPtr = groupPtr;
	CGroup* pGroup = static_cast<CGroup*>(m_groupPtr);
}

PyGameGroup::~PyGameGroup()
{
}

std::vector<pPyGameObject> PyGameGroup::GetChildren()
{
	CGroup* pGroup = static_cast<CGroup*>(m_groupPtr);

	// Return all of the Prefabs Children.
	std::vector<_smart_ptr<CBaseObject>>::iterator iter;
	std::vector<_smart_ptr<CBaseObject>> children;
	pGroup->GetAllChildren(children);

	for (iter = children.begin(); iter != children.end(); ++iter)
	{
		m_groupChildren.push_back(PyScript::CreatePyGameObject(*iter));
	}

	return m_groupChildren;
}

void PyGameGroup::AddChild(pPyGameObject pObj)
{
	CGroup* pGroup = static_cast<CGroup*>(m_groupPtr);
	CBaseObject* pObject = static_cast<CBaseObject*>(pObj->GetPtr());

	if (pGroup != NULL && pObject != NULL)
	{
		pGroup->AddMember(pObject, true);
	}

	// Probably should add this as the parent of pObject now.
}

void PyGameGroup::RemoveChild(pPyGameObject pObj)
{
	CGroup* pGroup = static_cast<CGroup*>(m_groupPtr);
	CBaseObject* pObject = static_cast<CBaseObject*>(pObj->GetPtr());

	if (pGroup != NULL && pObject != NULL)
	{
		pGroup->RemoveMember(pObject, true);
	}

	// Probably should remove this as the parent of pObject now.
}

bool PyGameGroup::IsOpen()
{
	CGroup* pGroup = static_cast<CGroup*>(m_groupPtr);
	return pGroup->IsOpen();
}

void PyGameGroup::Open()
{
	CGroup* pGroup = static_cast<CGroup*>(m_groupPtr);
	pGroup->Open();
}

void PyGameGroup::Close()
{
	CGroup* pGroup = static_cast<CGroup*>(m_groupPtr);
	pGroup->Close();
}

void PyGameGroup::UpdateGroup()
{
}

PyGameCamera::PyGameCamera(void* cameraPtr, pSPyWrappedClass sharedPtr)
{
	m_cameraPtr = cameraPtr;
	CCameraObject* pCamera = static_cast<CCameraObject*>(m_cameraPtr);
}

PyGameCamera::~PyGameCamera()
{

}

void PyGameCamera::UpdateCamera()
{
}

PyGameBrush::PyGameBrush(void* brushPtr, pSPyWrappedClass sharedPtr)
{
	m_brushPtr = brushPtr;
	CBrushObject* pBrush = static_cast<CBrushObject*>(m_brushPtr);

	if (pBrush == NULL)
	{
		return;
	}

	if (!pBrush->GetGeometryFile().IsEmpty())
	{
		m_brushModel = pBrush->GetGeometryFile();

		if (m_brushModel.Find("\\", 0) == 0)
		{
			m_brushModel.Replace("\\", "/");
		}
	}

	m_brushRatioLod = pBrush->GetRatioLod();
	m_brushRatioviewDist = pBrush->GetRatioViewDist();

	IStatObj* pStatObj = pBrush->GetIStatObj();
	m_brushLodCount = 0;

	if (pStatObj != NULL)
	{
		IStatObj::SStatistics objectStats;
		pStatObj->GetStatistics(objectStats);
		m_brushLodCount = objectStats.nLods;
	}
}

PyGameBrush::~PyGameBrush()
{
}

void PyGameBrush::Reload()
{
	CBrushObject* pBrush = static_cast<CBrushObject*>(m_brushPtr);
	pBrush->ReloadGeometry();
}

void PyGameBrush::UpdateBrush()
{
	CBrushObject* pBrush = static_cast<CBrushObject*>(m_brushPtr);

	if (!pBrush->GetGeometryFile().IsEmpty())
	{
		pBrush->SetGeometryFile(m_brushModel);
	}

	IRenderNode* pRenderNode = pBrush->GetEngineNode();

	if (m_brushRatioLod != pBrush->GetRatioLod())
	{
		pRenderNode->SetLodRatio(m_brushRatioLod);
	}

	if (m_brushRatioviewDist != pBrush->GetRatioViewDist())
	{
		pRenderNode->SetViewDistRatio(m_brushRatioviewDist);
	}
}

PyGameEntity::PyGameEntity(void* entityPtr, pSPyWrappedClass sharedPtr)
{
	m_entityPtr = entityPtr;
	CEntityObject* pEntity = static_cast<CEntityObject*>(m_entityPtr);

	if (pEntity == NULL)
	{
		return;
	}

	m_entityId = pEntity->GetEntityId();

	if (!pEntity->GetEntityClass().IsEmpty())
	{
		m_entitySubClass = pEntity->GetEntityClass();
	}

	if (!pEntity->GetEntityPropertyString("object_Model").IsEmpty())
	{
		m_entityModel = pEntity->GetEntityPropertyString("object_Model");
		m_entityModel.Replace("\\", "/");
	}

	if (pEntity->IsKindOf(RUNTIME_CLASS(CGeomEntity)))
	{
		CGeomEntity* pGeomEntity = static_cast<CGeomEntity*>(pEntity);

		if (!pGeomEntity->GetGeometryFile().IsEmpty())
		{
			m_entityModel = pGeomEntity->GetGeometryFile();
		}
	}

	m_entityRatioLod = pEntity->GetRatioLod();
	m_entityRatioviewDist = pEntity->GetRatioViewDist();

	// Grab all of entities properties in Block 1.
	std::vector<IVariable*> vars;

	CVarBlock* varBlockProps1 = pEntity->GetProperties();

	if (varBlockProps1 != NULL && !varBlockProps1->IsEmpty())
	{
		for (int i = 0; i < varBlockProps1->GetNumVariables(); i++)
		{
			IVariable* pVar = varBlockProps1->GetVariable(i);

			if (pVar->GetNumVariables() > 0)
			{
				for (int j = 0; j < pVar->GetNumVariables(); j++)
				{
					vars.push_back(pVar->GetVariable(j));
				}
			}
			else
			{
				vars.push_back(varBlockProps1->GetVariable(i));
			}
		}
	}

	// Now the properties in Block 2.
	CVarBlock* varBlockProps2 = pEntity->GetProperties2();

	if (varBlockProps2 != NULL && !varBlockProps2->IsEmpty())
	{
		for (int i = 0; i < varBlockProps2->GetNumVariables(); i++)
		{
			IVariable* pVar = varBlockProps2->GetVariable(i);

			if (pVar->GetNumVariables() > 0)
			{
				for (int j = 0; j < pVar->GetNumVariables(); j++)
				{
					vars.push_back(pVar->GetVariable(j));
				}
			}
			else
			{
				vars.push_back(varBlockProps2->GetVariable(i));
			}
		}
	}

	// Output all stored property iVariables by [key] = value so they can be easily converted to a PyDict.
	std::vector<IVariable*>::iterator iter;

	for (iter = vars.begin(); iter != vars.end(); ++iter)
	{
		IVariable* pVar = *iter;

		if (strcmp(pVar->GetName(), "object_Model") == 0)
		{
			pVar->Get(m_entityModel);
		}

		if (strcmp(pVar->GetName(), "") != 0)
		{
			pSPyWrappedProperty value(new SPyWrappedProperty);
			value->SetProperties(pVar);
			m_entityProps[pVar->GetName()] = value;
		}
	}
}

PyGameEntity::~PyGameEntity()
{
}

void PyGameEntity::Reload()
{
	CEntityObject* pEntity = static_cast<CEntityObject*>(m_entityPtr);
	pEntity->Reload();
}

void PyGameEntity::UpdateEntity()
{
	CEntityObject* pEntity = static_cast<CEntityObject*>(m_entityPtr);

	IRenderNode* pRenderNode = pEntity->GetEngineNode();

	if (m_entityRatioLod != pEntity->GetRatioLod())
	{
		pRenderNode->SetLodRatio(m_entityRatioLod);
	}

	if (m_entityRatioviewDist != pEntity->GetRatioViewDist())
	{
		pRenderNode->SetViewDistRatio(m_entityRatioviewDist);
	}

	std::map<string, pSPyWrappedProperty>::iterator iter;

	for (iter = m_entityProps.begin(); iter != m_entityProps.end(); ++iter)
	{
		if (iter->second->type == SPyWrappedProperty::eType_Bool)
		{
			if (iter->second->property.boolValue != pEntity->GetEntityPropertyBool(iter->first))
			{
				pEntity->SetEntityPropertyBool(iter->first, iter->second->property.boolValue);
			}
		}
		else if (iter->second->type == SPyWrappedProperty::eType_Int)
		{
			if (iter->first.Compare("Mass") == 0)
			{
				pEntity->SetEntityPropertyFloat(iter->first, (float)iter->second->property.intValue);
			}
			else
			{
				if (iter->second->property.intValue != pEntity->GetEntityPropertyInteger(iter->first))
				{
					pEntity->SetEntityPropertyInteger(iter->first, iter->second->property.intValue);
				}
			}
		}
		else if (iter->second->type == SPyWrappedProperty::eType_Float)
		{
			if (iter->second->property.floatValue != pEntity->GetEntityPropertyFloat(iter->first))
			{
				pEntity->SetEntityPropertyFloat(iter->first, iter->second->property.floatValue);
			}
		}
		else if (iter->second->type == SPyWrappedProperty::eType_String)
		{
			if (iter->second->stringValue != pEntity->GetEntityPropertyString(iter->first))
			{
				pEntity->SetEntityPropertyString(iter->first, iter->second->stringValue);
			}
		}
	}

	// Setting Model last to ensure it overwrites the property map that often tries to reset it to a default value, if it exists.
	if (m_entityProps.find("object_Model") != m_entityProps.end())
	{
		pEntity->SetEntityPropertyString("object_Model", m_entityModel);
	}
}

PyGameVegetationInstance::PyGameVegetationInstance(void* vegPtr)
{
	if (vegPtr == NULL)
	{
		m_vegPtr = NULL;
		m_vegPosition = Vec3(NULL);
		m_vegAngle = 0.0f;
		m_vegScale = 0.0f;
		m_vegBrightness = 0.0f;
		return;
	}

	m_vegPtr = vegPtr;
	CVegetationInstance* pVegInst = static_cast<CVegetationInstance*>(m_vegPtr);

	m_vegPosition = pVegInst->pos;
	m_vegAngle = pVegInst->angle;
	m_vegScale = pVegInst->scale;
	m_vegBrightness = pVegInst->brightness;
}

PyGameVegetationInstance::~PyGameVegetationInstance()
{
}

void PyGameVegetationInstance::UpdateVegetationInstance()
{
	if (m_vegPtr == NULL)
	{
		return;
	}

	CVegetationInstance* pVegInst = static_cast<CVegetationInstance*>(m_vegPtr);

	pVegInst->pos = m_vegPosition;
	pVegInst->angle = m_vegAngle;
	pVegInst->scale = m_vegScale;
	pVegInst->brightness = m_vegBrightness;
}

PyGameVegetation::PyGameVegetation(void* vegPtr)
{
	if (vegPtr == NULL)
	{
		m_vegPtr = NULL;
		m_vegName = "";
		m_vegGroup = "";
		m_vegID = 0;
		m_vegInstCount = 0;
		m_vegSelected = false;
		m_vegVisible = false;
		m_vegFrozen = false;
		m_vegCastShadows = false;
		m_vegGIMode = true;
		m_vegAutoMerged = false;
		m_vegHideable = false;
		return;
	}

	m_vegPtr = vegPtr;
	CVegetationObject* pVegObject = static_cast<CVegetationObject*>(m_vegPtr);

	m_vegName = pVegObject->GetFileName();
	m_vegGroup = pVegObject->GetGroup();
	m_vegID = pVegObject->GetId();
	m_vegInstCount = pVegObject->GetNumInstances();
	m_vegSelected = pVegObject->IsSelected();
	m_vegVisible = !pVegObject->IsHidden();
	m_vegFrozen = pVegObject->GetFrozen();
	m_vegCastShadows = pVegObject->IsCastShadow();
	m_vegGIMode = pVegObject->GetGIMode();
	m_vegAutoMerged = pVegObject->IsAutoMerged();
	m_vegHideable = pVegObject->IsHideable();

	std::vector<CVegetationInstance*>::iterator iter;
	std::vector<CVegetationInstance*> vegInstances;
	GetIEditorImpl()->GetVegetationMap()->GetAllInstances(vegInstances);

	for (iter = vegInstances.begin(); iter != vegInstances.end(); ++iter)
	{
		const CVegetationObject* pVegInstObj = (*iter)->object;

		if (strcmp(pVegInstObj->GetFileName(), m_vegName) == 0)
		{
			pPyGameVegetationInstance vegInst(new PyGameVegetationInstance(*iter));
			m_vegInstances.push_back(vegInst);
		}
	}
}

PyGameVegetation::~PyGameVegetation()
{
}

void PyGameVegetation::Load()
{
	CVegetationObject* pVegObject = static_cast<CVegetationObject*>(m_vegPtr);
	pVegObject->LoadObject();
}

void PyGameVegetation::Unload()
{
	CVegetationObject* pVegObject = static_cast<CVegetationObject*>(m_vegPtr);
	pVegObject->UnloadObject();
}

void PyGameVegetation::UpdateVegetation()
{
	if (m_vegPtr == NULL)
	{
		return;
	}

	CVegetationObject* pVegObject = static_cast<CVegetationObject*>(m_vegPtr);

	// veg cannot be selected or visible when issuing changes. revert afterwards.
	GetIEditorImpl()->GetVegetationMap()->HideObject(pVegObject, true);

	if (pVegObject->IsSelected())
	{
		pVegObject->SetSelected(false);
	}

	if (m_vegName != pVegObject->GetFileName())
	{
		pVegObject->SetFileName(m_vegName);
	}

	if (m_vegGroup != pVegObject->GetGroup())
	{
		pVegObject->SetGroup(m_vegGroup);
	}

	if (m_vegSelected != pVegObject->IsSelected())
	{
		pVegObject->SetSelected(m_vegName);
	}

	if (m_vegVisible != !pVegObject->IsHidden())
	{
		pVegObject->SetHidden(m_vegName);
	}

	if (m_vegFrozen != pVegObject->GetFrozen())
	{
		pVegObject->SetFrozen(m_vegName);
	}

	if (m_vegCastShadows != pVegObject->IsCastShadow())
	{
		pVegObject->SetCastShadow(m_vegName);
	}

	if (m_vegGIMode != pVegObject->GetGIMode())
	{
		pVegObject->SetGIMode(m_vegName);
	}

	if (m_vegHideable != pVegObject->IsHideable())
	{
		pVegObject->SetHideable(m_vegName);
	}

	// since the object was deselected at top of function, re-select if needed.
	pVegObject->SetSelected(m_vegSelected);

	GetIEditorImpl()->GetVegetationMap()->HideObject(pVegObject, false);
}

namespace PyScript
{
CListenerSet<IPyScriptListener*> s_listeners(2);

template<typename T>
T GetPyValue(const char* varName)
{
	using namespace boost::python;
	object module(handle<>(borrowed(PyImport_AddModule("__main__"))));
	object dictionary = module.attr("__dict__");
	object result = dictionary[varName];
	return extract<T>(result);
}

/// Stuff for converting between string and python string
class CCryStringToPythonConverter
{
public:
	static PyObject* convert(string const& s)
	{
		return boost::python::incref(boost::python::object(s.c_str()).ptr());
	}
};

template<class TString>
class CPythonToStringConverter
{
public:
	CPythonToStringConverter()
	{
		boost::python::converter::registry::push_back(&convertible, &construct, boost::python::type_id<string>());
	}

	static void* convertible(PyObject* pObject)
	{
		if (!PyString_Check(pObject))
		{
			return NULL;
		}

		return pObject;
	}

	static void construct(PyObject* pObject, boost::python::converter::rvalue_from_python_stage1_data* pData)
	{
		const char* pString = PyString_AsString(pObject);
		if (!pString)
		{
			boost::python::throw_error_already_set();
		}

		void* pStorage = ((boost::python::converter::rvalue_from_python_storage<string>*)pData)->storage.bytes;
		new(pStorage) TString(pString);
		pData->convertible = pStorage;
	}
};

/// Stuff for converting between string and python string
class CMFCStringToPythonConverter
{
public:
	static PyObject* convert(string const& s)
	{
		return boost::python::incref(boost::python::object(s.GetString()).ptr());
	}
};

/////////////////////////////////////////////////////////////////////////
// Converter for Python type <-> Property Type
class CPropertyToPythonConverter
{
public:
	static PyObject* convert(pSPyWrappedProperty const& wrappedProperty)
	{
		return convert(*(wrappedProperty.get()));
	}

	static PyObject* convert(SPyWrappedProperty const& wrappedProperty)
	{
		using namespace boost::python;

		try
		{
			switch (wrappedProperty.type)
			{
			case SPyWrappedProperty::eType_Bool:
				return incref(object(wrappedProperty.property.boolValue).ptr());

			case SPyWrappedProperty::eType_Int:
				return incref(object(wrappedProperty.property.intValue).ptr());

			case SPyWrappedProperty::eType_Float:
				return incref(object(wrappedProperty.property.floatValue).ptr());

			case SPyWrappedProperty::eType_String:
				return incref(object(wrappedProperty.stringValue).ptr());

			case SPyWrappedProperty::eType_Vec3:
				return incref(object(make_tuple(wrappedProperty.property.vecValue.x, wrappedProperty.property.vecValue.y, wrappedProperty.property.vecValue.z)).ptr());

			case SPyWrappedProperty::eType_Color:
				return incref(object(make_tuple(wrappedProperty.property.colorValue.r, wrappedProperty.property.colorValue.g, wrappedProperty.property.colorValue.b)).ptr());

			case SPyWrappedProperty::eType_Time:
				return incref(object(make_tuple(wrappedProperty.property.timeValue.hour, wrappedProperty.property.timeValue.min)).ptr());
			}
		}
		catch (const error_already_set&)
		{
			throw std::logic_error("No registered converter found for SPyWrappedProperty.");
		}

		return NULL;
	}
};

class CPythonToPropertyConverter
{
public:
	static void* convertible(PyObject* pObj)
	{
		try
		{
			if (PyString_Check(pObj) || PyInt_Check(pObj) || PyFloat_Check(pObj) || PyBool_Check(pObj) || PyTuple_Check(pObj))
			{
				return pObj;
			}
		}
		catch (const boost::python::error_already_set&)
		{
			throw std::logic_error("SPyWrappedProperty is not convertible.");
		}

		return NULL;
	}

	static void construct_obj(PyObject* pObj, boost::python::converter::rvalue_from_python_stage1_data* pData)
	{
		try
		{
			void* storage = ((boost::python::converter::rvalue_from_python_storage<SPyWrappedProperty>*)pData)->storage.bytes;
			new(storage) SPyWrappedProperty(construct(pObj));

			pData->convertible = storage;
		}
		catch (const boost::python::error_already_set&)
		{
			throw std::logic_error("Error met constructing SPyWrappedProperty.");
		}
	}

	static void construct_ptr(PyObject* pObj, boost::python::converter::rvalue_from_python_stage1_data* pData)
	{
		try
		{
			pSPyWrappedProperty value(new SPyWrappedProperty(construct(pObj)));

			void* storage = ((boost::python::converter::rvalue_from_python_storage<pSPyWrappedProperty>*)pData)->storage.bytes;
			new(storage) pSPyWrappedProperty(value);

			pData->convertible = storage;
		}
		catch (const boost::python::error_already_set&)
		{
			throw std::logic_error("Error met constructing pSPyWrappedProperty.");
		}
	}

private:
	static SPyWrappedProperty construct(PyObject* pObj)
	{
		SPyWrappedProperty value;

		if (PyBool_Check(pObj))
		{
			value.property.boolValue = (bool)PyInt_AS_LONG(pObj);
			value.type = SPyWrappedProperty::eType_Bool;
		}
		else if (PyInt_Check(pObj))
		{
			value.property.intValue = (int)PyInt_AS_LONG(pObj);
			value.type = SPyWrappedProperty::eType_Int;
		}
		else if (PyFloat_Check(pObj))
		{
			value.property.floatValue = (float)PyFloat_AS_DOUBLE(pObj);
			value.type = SPyWrappedProperty::eType_Float;
		}
		else if (PyString_Check(pObj))
		{
			value.stringValue = PyString_AsString(pObj);
			value.type = SPyWrappedProperty::eType_String;
		}
		else if (PyTuple_Check(pObj))
		{
			if (PyTuple_Size(pObj) == 3 && PyFloat_Check(PyTuple_GetItem(pObj, 0)) && PyFloat_Check(PyTuple_GetItem(pObj, 1)) && PyFloat_Check(PyTuple_GetItem(pObj, 2)))
			{
				value.type = SPyWrappedProperty::eType_Vec3;
				value.property.vecValue.x = (float)PyFloat_AS_DOUBLE(PyTuple_GetItem(pObj, 0));
				value.property.vecValue.y = (float)PyFloat_AS_DOUBLE(PyTuple_GetItem(pObj, 1));
				value.property.vecValue.z = (float)PyFloat_AS_DOUBLE(PyTuple_GetItem(pObj, 2));
			}
			else if (PyTuple_Size(pObj) == 3 && PyInt_Check(PyTuple_GetItem(pObj, 0)) && PyInt_Check(PyTuple_GetItem(pObj, 1)) && PyInt_Check(PyTuple_GetItem(pObj, 2)))
			{
				value.type = SPyWrappedProperty::eType_Color;
				value.property.colorValue.r = (int)PyInt_AS_LONG(PyTuple_GetItem(pObj, 0));
				value.property.colorValue.g = (int)PyInt_AS_LONG(PyTuple_GetItem(pObj, 1));
				value.property.colorValue.b = (int)PyInt_AS_LONG(PyTuple_GetItem(pObj, 2));
			}
			else if (PyTuple_Size(pObj) == 2 && PyInt_Check(PyTuple_GetItem(pObj, 0)) && PyInt_Check(PyTuple_GetItem(pObj, 1)))
			{
				value.type = SPyWrappedProperty::eType_Time;
				value.property.timeValue.hour = (int)PyInt_AS_LONG(PyTuple_GetItem(pObj, 0));
				value.property.timeValue.min = (int)PyInt_AS_LONG(PyTuple_GetItem(pObj, 1));
			}
		}
		return value;
	}
};

// Converter for CPP Class <-> Python Class
class CClassToPythonConverter
{
public:
	static PyObject* convert(pSPyWrappedClass wrappedClass)
	{
		if (wrappedClass->type == SPyWrappedClass::eType_Brush)
		{
			boost::python::type_info info = boost::python::type_id<PyGameBrush>();
			const boost::python::converter::registration* reg = boost::python::converter::registry::query(info);

			//BOOST NOTE: While registry conversion for shared_ptr's is automatic via from_python, to_python requires the raw pointer.
			if (reg != NULL && reg->m_to_python != NULL)
			{
				return boost::python::incref(reg->to_python(boost::static_pointer_cast<PyGameBrush>(wrappedClass->ptr).get()));
			}
			else
			{
				throw std::logic_error("No registered converter found for PyGameBrush class.");
			}
		}
		else if (wrappedClass->type == SPyWrappedClass::eType_Camera)
		{
			boost::python::type_info info = boost::python::type_id<PyGameCamera>();
			const boost::python::converter::registration* reg = boost::python::converter::registry::query(info);

			if (reg != NULL && reg->m_to_python != NULL)
			{
				return boost::python::incref(reg->to_python(boost::static_pointer_cast<PyGameCamera>(wrappedClass->ptr).get()));
			}
			else
			{
				throw std::logic_error("No registered converter found for PyGameCamera class.");
			}
		}
		else if (wrappedClass->type == SPyWrappedClass::eType_Entity)
		{
			boost::python::type_info info = boost::python::type_id<PyGameEntity>();
			const boost::python::converter::registration* reg = boost::python::converter::registry::query(info);

			if (reg != NULL && reg->m_to_python != NULL)
			{
				return boost::python::incref(reg->to_python(boost::static_pointer_cast<PyGameEntity>(wrappedClass->ptr).get()));
			}
			else
			{
				throw std::logic_error("No registered converter found for PyGameEntity class.");
			}
		}
		else if (wrappedClass->type == SPyWrappedClass::eType_Group)
		{
			boost::python::type_info info = boost::python::type_id<PyGameGroup>();
			const boost::python::converter::registration* reg = boost::python::converter::registry::query(info);

			if (reg != NULL && reg->m_to_python != NULL)
			{
				return boost::python::incref(reg->to_python(boost::static_pointer_cast<PyGameGroup>(wrappedClass->ptr).get()));
			}
			else
			{
				throw std::logic_error("No registered converter found for PyGameGroup class.");
			}
		}
		else if (wrappedClass->type == SPyWrappedClass::eType_Prefab)
		{
			boost::python::type_info info = boost::python::type_id<PyGamePrefab>();
			const boost::python::converter::registration* reg = boost::python::converter::registry::query(info);

			if (reg != NULL && reg->m_to_python != NULL)
			{
				return boost::python::incref(reg->to_python(boost::static_pointer_cast<PyGamePrefab>(wrappedClass->ptr).get()));
			}
			else
			{
				throw std::logic_error("No registered converter found for PyGamePrefab class.");
			}
		}

		return NULL;
	}
};

class CPythonToClassConverter
{
public:
	static void* convertible(PyObject* pObjPtr)
	{
		if (PyClass_Check(pObjPtr))
		{
			if (PyObject_IsInstance(pObjPtr, boost::python::object(boost::python::type_id<PyGameBrush>()).ptr()) ||
			    PyObject_IsInstance(pObjPtr, boost::python::object(boost::python::type_id<PyGameCamera>()).ptr()) ||
			    PyObject_IsInstance(pObjPtr, boost::python::object(boost::python::type_id<PyGameEntity>()).ptr()) ||
			    PyObject_IsInstance(pObjPtr, boost::python::object(boost::python::type_id<PyGameGroup>()).ptr()) ||
			    PyObject_IsInstance(pObjPtr, boost::python::object(boost::python::type_id<PyGamePrefab>()).ptr()))
			{
				return pObjPtr;
			}
		}

		return 0;
	}

	static void construct(PyObject* pObjPtr, boost::python::converter::rvalue_from_python_stage1_data* pData)
	{
		pSPyWrappedClass cls(new SPyWrappedClass);

		void* storage = ((boost::python::converter::rvalue_from_python_storage<pSPyWrappedClass>*)pData)->storage.bytes;
		new(storage) pSPyWrappedClass(cls);

		pData->convertible = storage;
	}
};

// Converter for Python list <-> std::vector<std::string>
class CStdStringVectorToPythonConverter
{
public:
	static PyObject* convert(std::vector<std::string> const& stringVector)
	{
		boost::python::list pyList;

		for (int i = 0; i < stringVector.size(); i++)
		{
			pyList.insert(i, stringVector[i]);
		}
		return boost::python::incref(boost::python::object(pyList).ptr());
	}
};

class CPythonToStdStringVectorConverter
{
public:
	static void* convertible(PyObject* pObjPtr)
	{
		if (!PyList_Check(pObjPtr))
		{
			for (int i = 0; i < PyList_GET_SIZE(pObjPtr); i++)
			{
				if (!PyString_Check(PyList_GetItem(pObjPtr, i)))
				{
					return NULL;
				}
			}
		}

		return pObjPtr;
	}

	static void construct(PyObject* pObjPtr, boost::python::converter::rvalue_from_python_stage1_data* pData)
	{
		std::vector<std::string> value;

		if (PyList_Check(pObjPtr))
		{
			for (int i = 0; i < PyList_GET_SIZE(pObjPtr); i++)
			{
				value.push_back(PyString_AsString(PyList_GetItem(pObjPtr, i)));
			}
		}

		void* storage = (
		  (boost::python::converter::rvalue_from_python_storage<std::vector<std::string>>*)
		  pData)->storage.bytes;

		new(storage) std::vector<std::string>(value);

		pData->convertible = storage;
	}
};

// Converter for Python String list <-> std::vector<string>
class CCStringVectorToPythonConverter
{
public:
	static PyObject* convert(std::vector<string> const& stringVector)
	{
		boost::python::list pyList;

		for (int i = 0; i < stringVector.size(); i++)
		{
			pyList.insert(i, stringVector[i]);
		}

		return boost::python::incref(boost::python::object(pyList).ptr());
	}
};

class CPythonToCStringVectorConverter
{
public:
	static void* convertible(PyObject* pObjPtr)
	{
		if (!PyList_Check(pObjPtr))
		{
			for (int i = 0; i < PyList_GET_SIZE(pObjPtr); i++)
			{
				if (!PyString_Check(PyList_GetItem(pObjPtr, i)))
				{
					return NULL;
				}
			}
		}

		return pObjPtr;
	}

	static void construct(PyObject* pObjPtr, boost::python::converter::rvalue_from_python_stage1_data* pData)
	{
		std::vector<string> value;

		if (PyList_Check(pObjPtr))
		{
			for (int i = 0; i < PyList_GET_SIZE(pObjPtr); i++)
			{
				value.push_back(PyString_AsString(PyList_GetItem(pObjPtr, i)));
			}
		}

		void* storage = (
		  (boost::python::converter::rvalue_from_python_storage<std::vector<string>>*)
		  pData)->storage.bytes;

		new(storage) std::vector<string>(value);

		pData->convertible = storage;
	}
};

// Converter for Vec3 <-> Python Tuple (x, y, z)
class CVec3ToPythonConverter
{
public:
	static PyObject* convert(Vec3 vValue)
	{
		return boost::python::incref(
		  boost::python::object(
		    boost::python::make_tuple(vValue.x, vValue.y, vValue.z)).ptr());
	}
};

class CPythonToVec3Converter
{
public:
	static void* convertible(PyObject* pObjPtr)
	{
		if (PyTuple_Check(pObjPtr) &&
		    PyTuple_Size(pObjPtr) == 3 &&
		    PyFloat_Check(PyTuple_GetItem(pObjPtr, 0)) &&
		    PyFloat_Check(PyTuple_GetItem(pObjPtr, 1)) &&
		    PyFloat_Check(PyTuple_GetItem(pObjPtr, 2)))
		{
			return pObjPtr;
		}

		return NULL;
	}

	static void construct(PyObject* pObjPtr, boost::python::converter::rvalue_from_python_stage1_data* pData)
	{
		Vec3 value;

		value.x = (float)PyFloat_AS_DOUBLE(PyTuple_GetItem(pObjPtr, 0));
		value.y = (float)PyFloat_AS_DOUBLE(PyTuple_GetItem(pObjPtr, 1));
		value.z = (float)PyFloat_AS_DOUBLE(PyTuple_GetItem(pObjPtr, 2));

		void* storage = (
		  (boost::python::converter::rvalue_from_python_storage<Vec3>*)
		  pData)->storage.bytes;

		new(storage) Vec3(value);

		pData->convertible = storage;
	}
};

// Converter for AABB <-> Python Tuple
class CAABBToPythonConverter
{
public:
	static PyObject* convert(AABB bounds)
	{
		return boost::python::incref(
		  boost::python::object(
		    boost::python::make_tuple(
		      boost::python::make_tuple(bounds.min.x, bounds.min.y, bounds.min.z),
		      boost::python::make_tuple(bounds.max.x, bounds.max.y, bounds.max.z))).ptr());
	}
};

class CPythonToAABBConverter
{
public:
	static void* convertible(PyObject* pObjPtr)
	{
		// Verify the inputed value matches the structure ((min, max), (min, max))
		if (PyTuple_Size(pObjPtr) == 2 &&
		    PyTuple_Check(PyTuple_GetItem(pObjPtr, 0)) &&
		    PyTuple_Size(PyTuple_GetItem(pObjPtr, 0)) == 2 &&
		    PyTuple_Check(PyTuple_GetItem(pObjPtr, 1)) &&
		    PyTuple_Size(PyTuple_GetItem(pObjPtr, 1)) == 2)
		{
			return pObjPtr;
		}

		return NULL;
	}
	static void construct(PyObject* pObjPtr, boost::python::converter::rvalue_from_python_stage1_data* pData)
	{
		AABB bound;

		if (PyTuple_Check(pObjPtr))
		{
			if (!PyTuple_Check(PyTuple_GetItem(pObjPtr, 0)))
			{
				throw std::logic_error("Expected first tuple index to contain minimums tuple value ");
			}

			if (!PyTuple_Check(PyTuple_GetItem(pObjPtr, 1)))
			{
				throw std::logic_error("Expected first tuple index to contain maximums tuple value ");
			}

			boost::python::tuple tMin = boost::python::extract<boost::python::tuple>(pObjPtr);
			boost::python::tuple tMax = boost::python::extract<boost::python::tuple>(pObjPtr);

			bound = AABB(
			  Vec3(boost::python::extract<float>(tMin[0]),
			       boost::python::extract<float>(tMin[1]),
			       boost::python::extract<float>(tMin[2])),
			  Vec3(boost::python::extract<float>(tMax[0]),
			       boost::python::extract<float>(tMax[1]),
			       boost::python::extract<float>(tMax[2])));
		}

		void* storage = (
		  (boost::python::converter::rvalue_from_python_storage<AABB>*)
		  pData)->storage.bytes;

		new(storage) AABB(bound);

		pData->convertible = storage;
	}
};

class CGUIDToPythonConverter
{
public:
	static PyObject* convert(GUID guid)
	{
		return boost::python::incref(boost::python::object(GuidUtil::ToString(guid)).ptr());
	}
};

class CPythonToGUIDConverter
{
public:
	static void* convertible(PyObject* pObjPtr)
	{
		if (!PyString_Check(pObjPtr))
		{
			return NULL;
		}

		char* value = (char*)PyString_AsString(pObjPtr);

		// Check for a standard GUID len of 38 and that the string starts and ends with curly braces.
		if (strlen(value) != 38 ||
		    strcmp((const char*)value[0], "{") != 0 ||
		    strcmp((const char*)value[strlen(value) - 1], "}") != 0)
		{
			return NULL;
		}

		return pObjPtr;
	}
	static void construct(PyObject* pObjPtr, boost::python::converter::rvalue_from_python_stage1_data* pData)
	{
		GUID guid;

		if (PyString_Check(pObjPtr))
		{
			guid = static_cast<GUID>(GuidUtil::FromString((const char*)PyString_AsString));
		}

		void* storage = (
		  (boost::python::converter::rvalue_from_python_storage<GUID>*)
		  pData)->storage.bytes;

		new(storage) GUID(guid);

		pData->convertible = storage;
	}
};

// Converter for Python list <-> std::vector<std::string>
class CStringVectorToPythonConverter
{
public:
	static PyObject* convert(std::vector<std::string> const& stringVector)
	{
		boost::python::list pyList;

		for (int i = 0; i < stringVector.size(); i++)
		{
			pyList.insert(i, stringVector[i]);
		}

		return boost::python::incref(boost::python::object(pyList).ptr());
	}
};

class CPythonToStringVectorConverter
{
public:
	static void* convertible(PyObject* pObjPtr)
	{
		if (!PyList_Check(pObjPtr))
		{
			return 0;
		}

		return pObjPtr;
	}

	static void construct(PyObject* pObjPtr, boost::python::converter::rvalue_from_python_stage1_data* pData)
	{
		std::vector<std::string> value;
		bool checkvalue(false);

		if (PyList_Check(pObjPtr))
		{
			for (int i = 0; i < PyList_GET_SIZE(pObjPtr); i++)
			{
				if (!PyString_Check(PyList_GetItem(pObjPtr, i)))
				{
					checkvalue = false;
					break;
				}

				value.push_back(PyString_AsString(PyList_GetItem(pObjPtr, i)));
				checkvalue = true;
			}
		}

		if (!checkvalue)
		{
			throw std::logic_error("Invalid data type.");
		}

		void* storage = (
		  (boost::python::converter::rvalue_from_python_storage<std::vector<std::string>>*)
		  pData)->storage.bytes;

		new(storage) std::vector<std::string>(value);

		pData->convertible = storage;
	}
};

// Initialize converters
void InitializePyConverters()
{
	// Register CPP SPyWrappedProperty <-> Python type
	boost::python::to_python_converter<
	  pSPyWrappedProperty,
	  CPropertyToPythonConverter>();

	boost::python::converter::registry::push_back(
	  &CPythonToPropertyConverter::convertible,
	  &CPythonToPropertyConverter::construct_ptr,
	  boost::python::type_id<pSPyWrappedProperty>());

	boost::python::to_python_converter<
	  SPyWrappedProperty,
	  CPropertyToPythonConverter>();

	boost::python::converter::registry::push_back(
	  &CPythonToPropertyConverter::convertible,
	  &CPythonToPropertyConverter::construct_obj,
	  boost::python::type_id<SPyWrappedProperty>());

	// Register CPP SPyWrappedClass Casting Function for PreRegistered Converters
	boost::python::to_python_converter<
	  pSPyWrappedClass,
	  CClassToPythonConverter>();

	boost::python::converter::registry::push_back(
	  &CPythonToClassConverter::convertible,
	  &CPythonToClassConverter::construct,
	  boost::python::type_id<pSPyWrappedClass>());

	// Register CPP string <-> Python String
	boost::python::to_python_converter<
	  string,
	  CCryStringToPythonConverter>();
	// cppcheck-suppress unusedScopedObject
	CPythonToStringConverter<string>();

	// Register CPP std::vector<str::string> <-> Python string list
	boost::python::to_python_converter<
	  std::vector<std::string>,
	  CStdStringVectorToPythonConverter>();
	CPythonToStdStringVectorConverter();

	boost::python::converter::registry::push_back(
	  &CPythonToStdStringVectorConverter::convertible,
	  &CPythonToStdStringVectorConverter::construct,
	  boost::python::type_id<std::vector<std::string>>());

	// Register CPP std::vector<string> <-> Python string list
	boost::python::to_python_converter<
	  std::vector<string>,
	  CCStringVectorToPythonConverter>();
	CPythonToCStringVectorConverter();

	boost::python::converter::registry::push_back(
	  &CPythonToCStringVectorConverter::convertible,
	  &CPythonToCStringVectorConverter::construct,
	  boost::python::type_id<std::vector<string>>());

	// Register CPP Vec3 <-> Python Tuple
	boost::python::to_python_converter<
	  Vec3,
	  CVec3ToPythonConverter>();
	CPythonToVec3Converter();

	boost::python::converter::registry::push_back(
	  &CPythonToVec3Converter::convertible,
	  &CPythonToVec3Converter::construct,
	  boost::python::type_id<Vec3>());

	//Register Converter for CPP AABB <-> Python Tuple
	boost::python::to_python_converter<
	  AABB,
	  CAABBToPythonConverter>();
	CPythonToAABBConverter();

	boost::python::converter::registry::push_back(
	  &CPythonToGUIDConverter::convertible,
	  &CPythonToGUIDConverter::construct,
	  boost::python::type_id<AABB>());

	//Register Converter for CPP GUID <-> Python String
	boost::python::to_python_converter<
	  GUID,
	  CGUIDToPythonConverter>();
	CPythonToGUIDConverter();

	boost::python::converter::registry::push_back(
	  &CPythonToGUIDConverter::convertible,
	  &CPythonToGUIDConverter::construct,
	  boost::python::type_id<GUID>());
}

typedef std::function<void (const char* output)> TRedirectWriteFunc;

struct SRedirectFunc
{
	PyObject_HEAD
	TRedirectWriteFunc write;
};

// Map all objects necessary for redirecting python output.
PyObject* RedirectWrite(PyObject* pSelf, PyObject* pArgs)
{
	std::size_t written(0);
	SRedirectFunc* pSelfImpl = reinterpret_cast<SRedirectFunc*>(pSelf);

	if (pSelfImpl->write)
	{
		char* pData;

		if (!PyArg_ParseTuple(pArgs, "s", &pData))
		{
			return nullptr;
		}

		pSelfImpl->write(pData);
		written = strlen(pData);
	}

	return PyLong_FromSize_t(written);
}

PyObject* RedirectFlush(PyObject* pSelf, PyObject* pArgs)
{
	return Py_BuildValue("");
}

PyMethodDef s_redirectMethods[] =
{
	{ "write", RedirectWrite, METH_VARARGS, "sys.stdout.write" },
	{ "flush", RedirectFlush, METH_VARARGS, "sys.stdout.flush" },
	{ "write", RedirectWrite, METH_VARARGS, "sys.stderr.write" },
	{ "flush", RedirectFlush, METH_VARARGS, "sys.stderr.flush" },
	{ 0,       0,             0,            0                  }   // sentinel
};

PyTypeObject s_redirectType =
{
	PyVarObject_HEAD_INIT(0, 0)
	"redirect.RedirectType", sizeof(SRedirectFunc),
	0,                       0,                          0,  0, 0, 0, 0,                 0, 0, 0, 0, 0, 0, 0, 0, 0,
	Py_TPFLAGS_DEFAULT,      "redirect.Redirect objects",
	0,                       0,                          0,  0, 0, 0, s_redirectMethods, 0, 0, 0,
	0,                       0,                          0,  0, 0, 0, 0
};

// Internal state
PyObject* s_pStdOut;
PyObject* s_pStdOutSaved;
PyObject* s_pStdErr;
PyObject* s_pStdErrSaved;

PyMODINIT_FUNC PyInitRedirecting(void)
{
	s_pStdOut = NULL;
	s_pStdOutSaved = NULL;

	s_redirectType.tp_new = PyType_GenericNew;

	if (PyType_Ready(&s_redirectType) < 0)
	{
		return;
	}

	PyObject* pModule = Py_InitModule3("redirect", 0, 0);

	if (pModule)
	{
		Py_INCREF(&s_redirectType);
		PyModule_AddObject(pModule, "Redirect", reinterpret_cast<PyObject*>(&s_redirectType));
	}

	return;
}

void SetStdOutCallback(TRedirectWriteFunc func)
{
	if (!s_pStdOut)
	{
		s_pStdOutSaved = PySys_GetObject("stdout");
		s_pStdOut = s_redirectType.tp_new(&s_redirectType, NULL, NULL);
	}

	SRedirectFunc* pImpl = reinterpret_cast<SRedirectFunc*>(s_pStdOut);
	pImpl->write = func;
	PySys_SetObject("stdout", s_pStdOut);
}

void RemoveStdOutCallback()
{
	if (s_pStdOutSaved)
	{
		PySys_SetObject("stdout", s_pStdOutSaved);
	}

	Py_XDECREF(s_pStdOut);
	s_pStdOut = NULL;
}

void SetStdErrCallback(TRedirectWriteFunc func)
{
	if (!s_pStdErr)
	{
		s_pStdErrSaved = PySys_GetObject("stderr");
		s_pStdErr = s_redirectType.tp_new(&s_redirectType, NULL, NULL);
	}

	SRedirectFunc* pImpl = reinterpret_cast<SRedirectFunc*>(s_pStdErr);
	pImpl->write = func;
	PySys_SetObject("stderr", s_pStdErr);
}

void RemoveStdErrCallback()
{
	if (s_pStdErrSaved)
	{
		PySys_SetObject("stderr", s_pStdErrSaved);
	}

	Py_XDECREF(s_pStdErr);
	s_pStdErr = nullptr;
}

#ifdef USE_PYTHON_SCRIPTING
BOOST_PYTHON_MODULE(sandbox)
{
	boost::python::object package = boost::python::scope();
	package.attr("__path__") = "sandbox";
	CAutoRegisterPythonModuleHelper::RegisterAll();
}
#endif
void InitCppModules()
{
	CAutoRegisterPythonCommandHelper::RegisterAll();
	//	CAutoRegisterPythonModuleHelper::RegisterAll();
#ifdef USE_PYTHON_SCRIPTING
	initsandbox();
#endif
}

// Registers all wrapped engine classes, and add them to the indexing suites to allow for them to be imbedded in vectors and maps.
void InitCppClasses()
{
	using namespace boost::python;

	// Register some common containers.
	class_<std::vector<int>>("int_vector")
	.def(vector_indexing_suite<std::vector<int>>());
	class_<std::vector<float>>("float_vector")
	.def(vector_indexing_suite<std::vector<float>>());
	class_<std::vector<pPyGameObject>>("PyGameObjectVector")
	.def(vector_indexing_suite<std::vector<pPyGameObject>, true>());
	class_<std::vector<pPyGameLayer>>("PyGameLayerVector")
	.def(vector_indexing_suite<std::vector<pPyGameLayer>, true>());
	class_<std::vector<pPyGameTexture>>("PyGameTextureVector")
	.def(vector_indexing_suite<std::vector<pPyGameTexture>, true>());
	class_<std::vector<pPyGameSubMaterial>>("PyGameSubMaterialVector")
	.def(vector_indexing_suite<std::vector<pPyGameSubMaterial>, true>());
	class_<std::vector<pPyGameVegetationInstance>>("PyGameVegetationInstance")
	.def(vector_indexing_suite<std::vector<pPyGameVegetationInstance>, true>());

	// Custom map_indexing_suite wrapper to avoid issues with Map <-> Dict conversion
	// http://boost.2283326.n4.nabble.com/Problem-with-map-indexing-suite-td2703134.html
	STL_MAP_WRAPPING(string, pSPyWrappedProperty, "SPyWrappedPropertyMap");

	//// CORT: Extending the common containers
	// no_init disallows python to create new instances of this class, as i'd rather let the editor create new objects.
	// noncopyable - stops boost from automatically adding a converter to the registry.
	class_<PyGameObject, pPyGameObject>("PyGameObject", no_init)       //, boost::noncopyable
	.add_property("name", &PyGameObject::GetName, &PyGameObject::SetName)
	.add_property("type", &PyGameObject::GetType)
	.add_property("id", &PyGameObject::GetGUID)
	.add_property("position", &PyGameObject::GetPosition, &PyGameObject::SetPosition)
	.add_property("rotation", &PyGameObject::GetRotation, &PyGameObject::SetRotation)
	.add_property("scale", &PyGameObject::GetScale, &PyGameObject::SetScale)
	.add_property("bounds", &PyGameObject::GetBounds)
	.add_property("selected", &PyGameObject::IsSelected, &PyGameObject::SetSelected)
	.add_property("grouped", &PyGameObject::IsInGroup)
	.add_property("visible", &PyGameObject::IsVisible, &PyGameObject::SetVisible)
	.add_property("frozen", &PyGameObject::IsFrozen, &PyGameObject::SetFrozen)
	.def("get_cls", &PyGameObject::GetClassObject)
	.def("get_material", &PyGameObject::GetMaterial)
	.def("set_material", &PyGameObject::SetMaterial)
	.def("get_layer", &PyGameObject::GetLayer)
	.def("set_layer", &PyGameObject::SetLayer)
	.def("get_parent", &PyGameObject::GetParent)
	.def("set_parent", &PyGameObject::SetParent)
	.def("update", &PyGameObject::UpdateObject)
	;

	class_<PyGameBrush>("PyGameBrush", no_init)
	.add_property("model", &PyGameBrush::GetModel, &PyGameBrush::SetModel)
	.add_property("lodRatio", &PyGameBrush::GetRatioLod, &PyGameBrush::SetRatioLod)
	.add_property("viewDistRatio", &PyGameBrush::GetRatioViewDist, &PyGameBrush::SetRatioViewDist)
	.add_property("lodCount", &PyGameBrush::GetLodCount)
	.def("reload", &PyGameBrush::Reload)
	.def("update", &PyGameBrush::UpdateBrush)
	;

	class_<PyGameEntity>("PyGameEntity", no_init)
	.add_property("model", &PyGameEntity::GetModel, &PyGameEntity::SetModel)
	.add_property("lodRatio", &PyGameEntity::GetRatioLod, &PyGameEntity::SetRatioLod)
	.add_property("viewDistRatio", &PyGameEntity::GetRatioViewDist, &PyGameEntity::SetRatioViewDist)
	.def("get_props", &PyGameEntity::GetProperties)
	.def("set_props", &PyGameEntity::SetProperties)
	.def("reload", &PyGameEntity::Reload)
	.def("update", &PyGameEntity::UpdateEntity)
	;

	class_<PyGamePrefab>("PyGamePrefab", no_init)
	.add_property("name", &PyGamePrefab::GetName, &PyGamePrefab::SetName)
	.add_property("opened", &PyGamePrefab::IsOpen)
	.def("get_children", &PyGamePrefab::GetChildren)
	.def("add_child", &PyGamePrefab::AddChild)
	.def("remove_child", &PyGamePrefab::RemoveChild)
	.def("open", &PyGamePrefab::Open)
	.def("close", &PyGamePrefab::Close)
	.def("extract_all", &PyGamePrefab::ExtractAll)
	.def("update", &PyGamePrefab::UpdatePrefab)
	;

	class_<PyGameGroup>("PyGameGroup", no_init)
	.add_property("opened", &PyGamePrefab::IsOpen)
	.def("get_children", &PyGameGroup::GetChildren)
	.def("add_child", &PyGameGroup::AddChild)
	.def("remove_child", &PyGameGroup::RemoveChild)
	.def("open", &PyGameGroup::Open)
	.def("close", &PyGameGroup::Close)
	.def("update", &PyGameGroup::UpdateGroup)
	;

	class_<PyGameCamera>("PyGameCamera", no_init)
	.def("update", &PyGameCamera::UpdateCamera)
	;

	class_<PyGameMaterial, pPyGameMaterial>("PyGameMaterial", no_init)
	.add_property("name", &PyGameMaterial::GetName, &PyGameMaterial::SetName)
	.add_property("path", &PyGameMaterial::GetPath)
	.def("get_sub_materials", &PyGameMaterial::GetSubMaterials)
	.def("update", &PyGameMaterial::UpdateMaterial)
	;

	class_<PyGameSubMaterial, pPyGameSubMaterial>("PyGameSubMaterial", no_init)
	.add_property("name", &PyGameSubMaterial::GetName, &PyGameSubMaterial::SetName)
	.add_property("shader", &PyGameSubMaterial::GetShader, &PyGameSubMaterial::SetShader)
	.add_property("surfaceType", &PyGameSubMaterial::GetSurfaceType, &PyGameSubMaterial::SetSurfaceType)
	.def("get_textures", &PyGameSubMaterial::GetTextures)
	.def("get_params", &PyGameSubMaterial::GetMatParams)
	.def("update", &PyGameSubMaterial::UpdateSubMaterial)
	;

	class_<PyGameTexture, pPyGameTexture>("PyGameTexture", no_init)
	.add_property("name", &PyGameTexture::GetName)
	.def("update", &PyGameTexture::UpdateTexture)
	;

	class_<PyGameLayer, pPyGameLayer>("PyGameLayer", no_init)
	.add_property("name", &PyGameLayer::GetName, &PyGameLayer::SetName)
	.add_property("path", &PyGameLayer::GetPath)
	.add_property("id", &PyGameLayer::GetGUID)
	.add_property("visible", &PyGameLayer::IsVisible, &PyGameLayer::SetVisible)
	.add_property("frozen", &PyGameLayer::IsFrozen, &PyGameLayer::SetFrozen)
	.add_property("exportable", &PyGameLayer::IsExportable, &PyGameLayer::SetExportable)
	.add_property("packed", &PyGameLayer::IsExportLayerPak, &PyGameLayer::SetExportLayerPak)
	.add_property("defaultLoaded", &PyGameLayer::IsDefaultLoaded, &PyGameLayer::SetDefaultLoaded)
	.add_property("physics", &PyGameLayer::IsPhysics, &PyGameLayer::SetHavePhysics)
	.def("get_children", &PyGameLayer::GetChildren)
	.def("update", &PyGameLayer::UpdateLayer)
	;

	class_<PyGameVegetation, pPyGameVegetation>("PyGameVegetation", no_init)       //, boost::noncopyable
	.add_property("name", &PyGameVegetation::GetName, &PyGameVegetation::SetName)
	.add_property("category", &PyGameVegetation::GetGroup)
	.add_property("id", &PyGameVegetation::GetID)
	.add_property("selected", &PyGameVegetation::IsSelected, &PyGameVegetation::SetSelected)
	.add_property("visible", &PyGameVegetation::IsVisible, &PyGameVegetation::SetVisible)
	.add_property("frozen", &PyGameVegetation::IsFrozen, &PyGameVegetation::SetFrozen)
	.add_property("castShadows", &PyGameVegetation::IsCastShadow, &PyGameVegetation::SetCastShadow)
	.add_property("giMode", &PyGameVegetation::GetGIMode, &PyGameVegetation::SetGIMode)
	.add_property("autoMerged", &PyGameVegetation::IsAutoMerged)
	.add_property("hideable", &PyGameVegetation::IsHideable, &PyGameVegetation::SetHideable)
	.def("get_instances", &PyGameVegetation::GetInstances)
	.def("load", &PyGameVegetation::Load)
	.def("unload", &PyGameVegetation::Unload)
	.def("update", &PyGameVegetation::UpdateVegetation)
	;

	class_<PyGameVegetationInstance, pPyGameVegetationInstance>("PyGameVegetationInstance", no_init)       //, boost::noncopyable
	.add_property("position", &PyGameVegetationInstance::GetPosition, &PyGameVegetationInstance::SetPosition)
	.add_property("angle", &PyGameVegetationInstance::GetAngle, &PyGameVegetationInstance::SetAngle)
	.add_property("scale", &PyGameVegetationInstance::GetScale, &PyGameVegetationInstance::SetScale)
	.add_property("brightness", &PyGameVegetationInstance::GetBrightness, &PyGameVegetationInstance::SetBrightness)
	.def("update", &PyGameVegetationInstance::UpdateVegetationInstance)
	;
}

// Pass any commands entered into script terminal to Python.
void Execute(const char* format, ...)
{
	char buffer[1024];

	va_list args;
	va_start(args, format);
	cry_vsprintf(buffer, format, args);
	va_end(args);

	PyRun_SimpleString(buffer);
}

void PrintMessage(const char* pString)
{
	for (CListenerSet<IPyScriptListener*>::Notifier notifier(s_listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnStdOut(pString);
	}
}

void PrintError(const char* pString)
{
	for (CListenerSet<IPyScriptListener*>::Notifier notifier(s_listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnStdErr(pString);
	}
}

const char* GetAsString(const char* varName)
{
	try
	{
		return GetPyValue<const char*>(varName);
	}
	catch (boost::python::error_already_set)
	{
		PyErr_Print();
		return NULL;
	}
}

int GetAsInt(const char* varName)
{
	try
	{
		return GetPyValue<int>(varName);
	}
	catch (boost::python::error_already_set)
	{
		PyErr_Print();
		return 0;
	}
}

float GetAsFloat(const char* varName)
{
	try
	{
		return GetPyValue<float>(varName);
	}
	catch (boost::python::error_already_set)
	{
		PyErr_Print();
		return 0.0f;
	}
}

bool GetAsBool(const char* varName)
{
	try
	{
		return GetPyValue<bool>(varName);
	}
	catch (boost::python::error_already_set)
	{
		PyErr_Print();
		return false;
	}
}

// Initialize Python, register path relative to editor location, and register our update function.
PyObjCache m_objCache;
PyLyrCache m_lyrCache;
PyMatCache m_matCache;
PySubMatCache m_subMatCache;
PyTextureCache m_textureCache;
PyVegCache m_vegCache;

pPyGameObject CreatePyGameObject(CBaseObject* pObj)
{
	if (m_objCache.IsCached(pObj))
	{
		return m_objCache.GetCachedSharedPtr(pObj);
	}
	else
	{
		const pPyGameObject result = pPyGameObject(new PyGameObject(pObj));
		m_objCache.AddToCache(result);
		return result;
	}
}

pPyGameLayer CreatePyGameLayer(CObjectLayer* pLayer)
{
	if (m_lyrCache.IsCached(pLayer))
	{
		return m_lyrCache.GetCachedSharedPtr(pLayer);
	}
	else
	{
		const pPyGameLayer result = pPyGameLayer(new PyGameLayer(pLayer));
		m_lyrCache.AddToCache(result);
		return result;
	}
}

pPyGameMaterial CreatePyGameMaterial(CMaterial* pMat)
{
	if (m_matCache.IsCached(pMat))
	{
		return m_matCache.GetCachedSharedPtr(pMat);
	}
	else
	{
		const pPyGameMaterial result = pPyGameMaterial(new PyGameMaterial(pMat));
		m_matCache.AddToCache(result);
		return result;
	}
}

pPyGameSubMaterial CreatePyGameSubMaterial(CMaterial* pSubMat, int id)
{
	if (m_subMatCache.IsCached(pSubMat))
	{
		return m_subMatCache.GetCachedSharedPtr(pSubMat);
	}
	else
	{
		const pPyGameSubMaterial result = pPyGameSubMaterial(new PyGameSubMaterial(pSubMat, id));
		m_subMatCache.AddToCache(result);
		return result;
	}
}

pPyGameTexture CreatePyGameTexture(SEfResTexture* pTex)
{
	if (m_textureCache.IsCached(pTex))
	{
		return m_textureCache.GetCachedSharedPtr(pTex);
	}
	else
	{
		const pPyGameTexture result = pPyGameTexture(new PyGameTexture(pTex));
		m_textureCache.AddToCache(result);
		return result;
	}
}

pPyGameVegetation CreatePyGameVegetation(CVegetationObject* pVeg)
{
	if (m_vegCache.IsCached(pVeg))
	{
		return m_vegCache.GetCachedSharedPtr(pVeg);
	}
	else
	{
		const pPyGameVegetation result = pPyGameVegetation(new PyGameVegetation(pVeg));
		m_vegCache.AddToCache(result);
		return result;
	}
}

CryLogPythonOutput* s_pLogOutput;

string GetSandboxPythonPath()
{
	return PathUtil::Make(PathUtil::GetEnginePath(), "Editor\\Python");
}

void InitializePython()
{
	s_pLogOutput = new CryLogPythonOutput();
	RegisterListener(s_pLogOutput);

	PyImport_AppendInittab("redirectstdout", PyScript::PyInitRedirecting);

	char szExecFilePath[_MAX_PATH];
	CryGetExecutableFolder(CRY_ARRAY_COUNT(szExecFilePath), szExecFilePath);
	// python27.zip or python27_d.zip has to exists
	string zipPath;
#ifdef _DEBUG
	zipPath = PathUtil::Make(szExecFilePath, "python27_d.zip");
#else
	zipPath = PathUtil::Make(szExecFilePath, "python27.zip");
#endif // _DEBUG
	zipPath.replace('/', '\\');
	if (!CFileUtil::FileExists(zipPath))
	{
		Error("Python standard library zip ( %s ) is missing. Cannot initialize Python. Sandbox will crash.", zipPath);
		return;
	}

	string binPath(PathUtil::RemoveSlash(szExecFilePath));
	// need to add this and Editor/Python to path so python can find Qt dlls etc easilly
	string pythonPath = PathUtil::Make(GetSandboxPythonPath(), string(PathUtil::GetCurrentPlatformFolder()));
	pythonPath.replace('/', '\\');
	string currentEnvPath(getenv("PATH"));
	currentEnvPath = binPath + ";" + pythonPath + ";" + currentEnvPath;
	_putenv_s("PATH", currentEnvPath);
	// and set python home to "Editor/Python" so everything works as it should
	Py_SetPythonHome((char*)pythonPath.c_str());

	Py_Initialize();

	PyImport_ImportModule("redirectstdout");
	InitCppClasses();
	InitCppModules();
	InitializePyConverters();

	PyScript::SetStdOutCallback(&PrintMessage);
	PyScript::SetStdErrCallback(&PrintError);
}

void ShutdownPython()
{
	PyScript::RemoveStdOutCallback();
	PyScript::RemoveStdErrCallback();

	CAutoRegisterPythonCommandHelper::UnregisterAll();
	CAutoRegisterPythonModuleHelper::UnregisterAll();

	RemoveListener(s_pLogOutput);
	delete s_pLogOutput;
	Py_Finalize();
}

void RegisterListener(IPyScriptListener* pListener)
{
	s_listeners.Add(pListener);
}

void RemoveListener(IPyScriptListener* pListener)
{
	s_listeners.Remove(pListener);
}

void LoadPluginFromPath(string path)
{
	LOADING_TIME_PROFILE_SECTION_ARGS(path.c_str());
	CFileEnum fileEnum;
	__finddata64_t fileData;
	if (fileEnum.StartEnumeration(path, "startup.py", &fileData))
	{
		string fullFilePath = PathUtil::Make(path, fileData.name);
		fullFilePath.replace('\\', '/');
		GetIEditorImpl()->ExecuteCommand("general.run_file '%s'", fullFilePath);
	}
}

void LoadPythonPlugins()
{
	LOADING_TIME_PROFILE_SECTION;
	string userFolder(PathUtil::GetUserSandboxFolder());
	boost::python::object mainModule(boost::python::handle<>(boost::python::borrowed(PyImport_ImportModule("sandbox"))));
	boost::python::scope main_scope = mainModule;
	main_scope.attr("user_folder") = userFolder;

	string pluginsPath = PathUtil::Make(GetSandboxPythonPath(), "plugins");
	pluginsPath.replace('/', '\\');

	string fix_error_output(R"(
import traceback
import sys
def __process_error(etype, value, tb) :
	exc = traceback.format_exception(etype, value, tb)
	sys.stderr.write("".join(exc))

sys.excepthook = __process_error
)");
	PyRun_SimpleString(fix_error_output);

	// for safety reasons we just run crytools plugin first.
	string mainPluginName = "crytools";
	LoadPluginFromPath(PathUtil::Make(pluginsPath, mainPluginName));

	CFileEnum folderEnum;
	__finddata64_t folderData;

	for (bool bFoundFile = folderEnum.StartEnumeration(pluginsPath, "*", &folderData);
	     bFoundFile; bFoundFile = folderEnum.GetNextFile(&folderData))
	{
		const string fileName = folderData.name;

		// Have we found a folder?
		if (folderData.attrib & _A_SUBDIR)
		{
			// Skip the parent folder entries
			if (fileName == "." || fileName == ".." || fileName == mainPluginName)
				continue;
			string pluginPath = PathUtil::Make(pluginsPath, fileName);
			LoadPluginFromPath(pluginPath);

		}
	}
}

}


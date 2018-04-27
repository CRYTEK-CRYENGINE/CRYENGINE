// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EntityPrototype.h"
#include "Objects\EntityScript.h"
#include "Objects\EntityObject.h"

#include <CryEntitySystem/IEntitySystem.h>
#include <CrySerialization/IArchiveHost.h>

//////////////////////////////////////////////////////////////////////////
// CEntityPrototype implementation.
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CEntityPrototype::CEntityPrototype()
{
	m_pArchetype = 0;
}

//////////////////////////////////////////////////////////////////////////
CEntityPrototype::~CEntityPrototype()
{
}

//////////////////////////////////////////////////////////////////////////
void CEntityPrototype::SetEntityClassName(const string& className)
{
	if (className != m_className)
	{
		GetLibrary()->SetModified(true);
	}

	m_className = className;

	// clear old properties.
	m_properties = 0;

	// Find this script in registry.
	m_script = CEntityScriptRegistry::Instance()->Find(m_className);
	if (m_script)
	{
		// Assign properties.
		if (!m_script->IsValid())
		{
			if (!m_script->Load())
			{
				Error("Failed to initialize Entity Script %s from %s", (const char*)m_className, (const char*)m_script->GetFile());
				m_script = 0;
			}
		}
	}

	if (m_script && !m_script->GetClass())
	{
		Error("No Script Class %s", (const char*)m_className);
		m_script = 0;
	}

	if (m_script)
	{
		CVarBlock* scriptProps = m_script->GetDefaultProperties();
		if (scriptProps)
			m_properties = scriptProps->Clone(true);

		// Create a game entity archetype.
		gEnv->pEntitySystem->UnloadEntityArchetype(GetFullName());
		m_pArchetype = gEnv->pEntitySystem->CreateEntityArchetype(m_script->GetClass(), GetFullName());
	}

	_smart_ptr<CBaseObject> pBaseObj = (CBaseObject*)RUNTIME_CLASS(CEntityObject)->CreateObject();
	if (pBaseObj)
	{
		pBaseObj->InitVariables();

		if (CVarObject* pVarObject = pBaseObj->GetVarObject())
		{
			m_pObjectVarBlock = pVarObject->Clone(true);
		}
	}

	if (m_properties)
	{
		CVariableArray* mv_AdditionalProperties = new CVariableArray();
		CVariable<string>* mv_prototypeMaterial = new CVariable<string>();

		// Sets the user interface parameters for the additional properties.
		mv_AdditionalProperties->SetName("AdditionalArchetypeProperties");
		mv_AdditionalProperties->SetHumanName("Additional Archetype Properties");

		// Here we setup the prototype material AND add it to the additional
		// properties.
		mv_prototypeMaterial->SetDataType(IVariable::DT_MATERIAL);
		mv_prototypeMaterial->SetHumanName("Prototype Material");
		mv_prototypeMaterial->SetName("PrototypeMaterial");
		mv_AdditionalProperties->AddVariable(mv_prototypeMaterial);

		// Here we add the array of variables we added.
		m_properties->AddVariable(mv_AdditionalProperties);
	}
};

//////////////////////////////////////////////////////////////////////////
void CEntityPrototype::Reload()
{
	if (m_script)
	{
		CVarBlockPtr oldProperties = m_properties;
		m_properties = 0;
		m_script->Reload();
		CVarBlock* scriptProps = m_script->GetDefaultProperties();
		if (scriptProps)
			m_properties = scriptProps->Clone(true);

		if (m_properties != NULL && oldProperties != NULL)
			m_properties->CopyValuesByName(oldProperties);
	}
	Update();
}

//////////////////////////////////////////////////////////////////////////
CVarBlock* CEntityPrototype::GetProperties()
{
	return m_properties;
}

//////////////////////////////////////////////////////////////////////////
CEntityScript* CEntityPrototype::GetScript()
{
	return m_script.get();
}

//////////////////////////////////////////////////////////////////////////
QString CEntityPrototype::GetMaterial() const
{
	if (!m_properties)
	{
		return QString();
	}

	auto pPrototypeMaterial = m_properties->FindVariable("PrototypeMaterial");
	if (pPrototypeMaterial)
	{
		string resultString;
		pPrototypeMaterial->Get(resultString);
		return QString(resultString);
	}
	return QString();
}

//////////////////////////////////////////////////////////////////////////
QString CEntityPrototype::GetVisualObject() const
{
	static auto getString = [](IVariable* pVariable)
	{
		string resultString;
		pVariable->Get(resultString);
		return resultString;
	};

	QString result;
	if (m_properties)
	{
		auto pVar = m_properties->FindVariable("object_Model", true);
		if (pVar)
		{
			result = getString(pVar);
		}

		if (result.isEmpty())
		{
			pVar = m_properties->FindVariable("fileModel", true);
			if (pVar)
			{
				result = getString(pVar);
			}
		}
		if (result.isEmpty())
		{
			pVar = m_properties->FindVariable("objModel", true);
			if (pVar)
			{
				result = getString(pVar);
			}
		}
	}
	if (result.isEmpty() && m_script)
	{
		// Load script if its not loaded yet.
		if (m_script->IsValid() || m_script->Load())
		{
			result = m_script->GetClass()->GetEditorClassInfo().sHelper;
		}
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
void CEntityPrototype::Serialize(SerializeContext& ctx)
{
	CBaseLibraryItem::Serialize(ctx);
	SerializePrototype(ctx, true);
}

void CEntityPrototype::SerializePrototype(SerializeContext& ctx, bool bRecreateArchetype)
{
	XmlNodeRef node = ctx.node;
	if (ctx.bLoading)
	{
		// Loading

		if (bRecreateArchetype)
		{
			string className;
			node->getAttr("Class", className);
			node->getAttr("Description", m_description);

			SetEntityClassName(className);
		}

		XmlNodeRef objVarsNode = node->findChild("ObjectVars");
		XmlNodeRef propsNode = GetISystem()->CreateXmlNode("Properties");

		if (m_properties)
		{
			// Serialize properties.
			XmlNodeRef props = node->findChild("Properties");
			if (props)
			{
				m_properties->Serialize(props, ctx.bLoading);

				if (m_pArchetype)
				{
					// Reload archetype in Engine.
					m_properties->Serialize(propsNode, false);
				}
			}
		}

		if (m_pObjectVarBlock)
		{
			// Serialize object vars.
			if (objVarsNode)
			{
				m_pObjectVarBlock->Serialize(objVarsNode, ctx.bLoading);
			}
		}

		if (m_pArchetype)
		{
			m_pArchetype->LoadFromXML(propsNode, objVarsNode);

			if (IEntityArchetypeManagerExtension* pExtension = gEnv->pEntitySystem->GetEntityArchetypeManagerExtension())
			{
				pExtension->LoadFromXML(*m_pArchetype, node);
			}
		}
	}
	else
	{
		// Saving.
		node->setAttr("Class", m_className);
		node->setAttr("Description", m_description);
		if (m_properties)
		{
			// Serialize properties.
			XmlNodeRef props = node->newChild("Properties");
			m_properties->Serialize(props, ctx.bLoading);
		}

		if (m_pObjectVarBlock)
		{
			XmlNodeRef objVarsNode = node->newChild("ObjectVars");
			m_pObjectVarBlock->Serialize(objVarsNode, ctx.bLoading);
		}

		if (m_pArchetype)
		{
			if (IEntityArchetypeManagerExtension* pExtension = gEnv->pEntitySystem->GetEntityArchetypeManagerExtension())
			{
				pExtension->SaveToXML(*m_pArchetype, node);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityPrototype::AddUpdateListener(UpdateCallback cb)
{
	m_updateListeners.push_back(cb);
}

//////////////////////////////////////////////////////////////////////////
void CEntityPrototype::RemoveUpdateListener(UpdateCallback cb)
{
	std::list<UpdateCallback>::iterator it = std::find(m_updateListeners.begin(), m_updateListeners.end(), cb);
	if (it != m_updateListeners.end())
		m_updateListeners.erase(it);
}

//////////////////////////////////////////////////////////////////////////
void CEntityPrototype::Update()
{
	if (m_pArchetype)
	{
		// Reload archetype in Engine.
		const bool bRecreateArchetype = false;
		XmlNodeRef prototypeNode = GetISystem()->CreateXmlNode("EntityPrototype");
		CBaseLibraryItem::SerializeContext ctx(prototypeNode, false);
		SerializePrototype(ctx, bRecreateArchetype);
		ctx.bLoading = true;
		SerializePrototype(ctx, bRecreateArchetype);
	}

	for (std::list<UpdateCallback>::iterator pCallback = m_updateListeners.begin(); pCallback != m_updateListeners.end(); ++pCallback)
	{
		// Call callback.
		(*pCallback)();
	}

	// Reloads all entities of with this prototype.
	{
		CBaseObjectsArray cstBaseObject;
		GetIEditorImpl()->GetObjectManager()->GetObjects(cstBaseObject, NULL);

		for (CBaseObjectsArray::iterator it = cstBaseObject.begin(); it != cstBaseObject.end(); ++it)
		{
			CBaseObject* obj = *it;
			if (obj->IsKindOf(RUNTIME_CLASS(CEntityObject)))
			{
				CEntityObject* pEntity((CEntityObject*)obj);
				if (pEntity->GetPrototype() == this)
				{
					pEntity->Reload();
				}
			}
		}
	}
}


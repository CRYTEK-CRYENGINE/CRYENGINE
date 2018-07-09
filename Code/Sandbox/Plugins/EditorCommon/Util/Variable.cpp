// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>

#include "Variable.h"
#include "UIEnumsDatabase.h"
#include "UsedResources.h"

//////////////////////////////////////////////////////////////////////////
// For serialization.
#include <CrySerialization/Decorators/Range.h>
#include <CrySerialization/Enum.h>
#include <CrySerialization/Color.h>
//#include "Serialization/Decorators/Curve.h"
#include <CrySerialization/Decorators/Resources.h>
#include <CrySerialization/Math.h>
#include <CrySerialization/Decorators/ResourcesAudio.h>
//////////////////////////////////////////////////////////////////////////
#include <QtUtil.h>

//////////////////////////////////////////////////////////////////////////
void CVariableBase::SetHumanName(const string& name)
{
	m_humanName = QtUtil::CamelCaseToUIString(name).toUtf8().constData();
}

//////////////////////////////////////////////////////////////////////////
CVarBlock* CVarBlock::Clone(bool bRecursive) const
{
	CVarBlock* vb = new CVarBlock;
	for (Variables::const_iterator it = m_vars.begin(); it != m_vars.end(); ++it)
	{
		IVariable* var = *it;
		vb->AddVariable(var->Clone(bRecursive));
	}
	return vb;
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::CopyValues(const CVarBlock* fromVarBlock)
{
	// Copy all variables.
	int numSrc = fromVarBlock->GetNumVariables();
	int numTrg = GetNumVariables();
	for (int i = 0; i < numSrc && i < numTrg; i++)
	{
		GetVariable(i)->CopyValue(fromVarBlock->GetVariable(i));
	}
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::CopyValuesByName(CVarBlock* fromVarBlock)
{
	// Copy values using saving and loading to/from xml.
	XmlNodeRef node = XmlHelpers::CreateXmlNode("Temp");
	fromVarBlock->Serialize(node, false);
	Serialize(node, true);
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::OnSetValues()
{
	for (Variables::iterator it = m_vars.begin(); it != m_vars.end(); ++it)
	{
		IVariable* var = *it;
		var->OnSetValue(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::AddVariable(IVariable* var)
{
	//assert( !strstr(var->GetName(), " ") ); // spaces not allowed because of serialization
	m_vars.push_back(var);
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::AddVariable(IVariable* pVar, const char* varName, unsigned char dataType)
{
	if (varName)
		pVar->SetName(varName);
	pVar->SetDataType(dataType);
	AddVariable(pVar);
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::AddVariable(CVariableBase& var, const char* varName, unsigned char dataType)
{
	if (varName)
		var.SetName(varName);
	var.SetDataType(dataType);
	AddVariable(&var);
}

//////////////////////////////////////////////////////////////////////////
bool CVarBlock::DeleteVariable(IVariable* var, bool bRecursive)
{
	bool found = stl::find_and_erase(m_vars, var);

	if (!found && bRecursive)
	{
		for (Variables::iterator it = m_vars.begin(); it != m_vars.end(); ++it)
		{
			if ((*it)->DeleteVariable(var, bRecursive))
				return true;
		}
	}

	return found;
}

//////////////////////////////////////////////////////////////////////////
bool CVarBlock::IsContainsVariable(IVariable* pVar, bool bRecursive) const
{
	for (Variables::const_iterator it = m_vars.begin(); it != m_vars.end(); ++it)
	{
		if (*it == pVar)
		{
			return true;
		}
	}

	// If not found search childs.
	if (bRecursive)
	{
		// Search all top level variables.
		for (Variables::const_iterator it = m_vars.begin(); it != m_vars.end(); ++it)
		{
			if ((*it)->IsContainsVariable(pVar))
			{
				return true;
			}
		}
	}

	return false;
}

namespace
{
IVariable* FindVariable(const char* name, bool bRecursive, bool bHumanName, const std::vector<IVariablePtr>& vars)
{
	// Search all top level variables.
	for (std::vector<IVariablePtr>::const_iterator it = vars.begin(); it != vars.end(); ++it)
	{
		IVariable* var = *it;
		if (bHumanName && stricmp(var->GetHumanName(), name) == 0)
		{
			return var;
		}
		else if (!bHumanName && strcmp(var->GetName(), name) == 0)
		{
			return var;
		}
	}

	// If not found search childs.
	if (bRecursive)
	{
		// Search all top level variables.
		for (std::vector<IVariablePtr>::const_iterator it = vars.begin(); it != vars.end(); ++it)
		{
			IVariable* var = *it;
			IVariable* found = var->FindVariable(name, bRecursive);
			if (found)
			{
				return found;
			}
		}
	}

	return nullptr;
}
}

//////////////////////////////////////////////////////////////////////////
IVariable* CVarBlock::FindVariable(const char* name, bool bRecursive, bool bHumanName) const
{
	return ::FindVariable(name, bRecursive, bHumanName, m_vars);
}

//////////////////////////////////////////////////////////////////////////
IVariable* CVariableArray::FindVariable(const char* name, bool bRecursive, bool bHumanName) const
{
	return ::FindVariable(name, bRecursive, bHumanName, m_vars);
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::Serialize(XmlNodeRef& vbNode, bool load)
{
	if (load)
	{
		// Loading.
		string name;
		for (Variables::iterator it = m_vars.begin(); it != m_vars.end(); ++it)
		{
			IVariable* var = *it;
			if (var->GetNumVariables())
			{
				XmlNodeRef child = vbNode->findChild(var->GetName());
				if (child)
					var->Serialize(child, load);
			}
			else
				var->Serialize(vbNode, load);
		}
	}
	else
	{
		// Saving.
		for (Variables::iterator it = m_vars.begin(); it != m_vars.end(); ++it)
		{
			IVariable* var = *it;
			if (var->GetNumVariables())
			{
				XmlNodeRef child = vbNode->newChild(var->GetName());
				var->Serialize(child, load);
			}
			else
				var->Serialize(vbNode, load);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::ReserveNumVariables(int numVars)
{
	m_vars.reserve(numVars);
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::WireVar(IVariable* src, IVariable* trg, bool bWire)
{
	if (bWire)
		src->Wire(trg);
	else
		src->Unwire(trg);
	int numSrcVars = src->GetNumVariables();
	if (numSrcVars > 0)
	{
		int numTrgVars = trg->GetNumVariables();
		for (int i = 0; i < numSrcVars && i < numTrgVars; i++)
		{
			WireVar(src->GetVariable(i), trg->GetVariable(i), bWire);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::Wire(CVarBlock* toVarBlock)
{
	Variables::iterator tit = toVarBlock->m_vars.begin();
	Variables::iterator sit = m_vars.begin();
	for (; sit != m_vars.end() && tit != toVarBlock->m_vars.end(); ++sit, ++tit)
	{
		IVariable* src = *sit;
		IVariable* trg = *tit;
		WireVar(src, trg, true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::Unwire(CVarBlock* toVarBlock)
{
	Variables::iterator tit = toVarBlock->m_vars.begin();
	Variables::iterator sit = m_vars.begin();
	for (; sit != m_vars.end() && tit != toVarBlock->m_vars.end(); ++sit, ++tit)
	{
		IVariable* src = *sit;
		IVariable* trg = *tit;
		WireVar(src, trg, false);
	}
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::AddOnSetCallback(IVariable::OnSetCallback func)
{
	for (Variables::iterator it = m_vars.begin(); it != m_vars.end(); ++it)
	{
		IVariable* var = *it;
		SetCallbackToVar(func, var, true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::RemoveOnSetCallback(IVariable::OnSetCallback func)
{
	for (Variables::iterator it = m_vars.begin(); it != m_vars.end(); ++it)
	{
		IVariable* var = *it;
		SetCallbackToVar(func, var, false);
	}
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::SetCallbackToVar(IVariable::OnSetCallback func, IVariable* pVar, bool bAdd)
{
	if (bAdd)
		pVar->AddOnSetCallback(func);
	else
		pVar->RemoveOnSetCallback(func);
	int numVars = pVar->GetNumVariables();
	if (numVars > 0)
	{
		for (int i = 0; i < numVars; i++)
		{
			SetCallbackToVar(func, pVar->GetVariable(i), bAdd);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::GatherUsedResources(CUsedResources& resources)
{
	for (int i = 0; i < GetNumVariables(); i++)
	{
		IVariable* pVar = GetVariable(i);
		GatherUsedResourcesInVar(pVar, resources);
	}
}
//////////////////////////////////////////////////////////////////////////
void CVarBlock::EnableUpdateCallbacks(bool boEnable)
{
	for (int i = 0; i < GetNumVariables(); i++)
	{
		IVariable* pVar = GetVariable(i);
		pVar->EnableUpdateCallbacks(boEnable);
	}
}
//////////////////////////////////////////////////////////////////////////
void CVarBlock::GatherUsedResourcesInVar(IVariable* pVar, CUsedResources& resources)
{
	int type = pVar->GetDataType();
	if (type == IVariable::DT_FILE || type == IVariable::DT_OBJECT || type == IVariable::DT_TEXTURE)
	{
		// this is file.
		string filename;
		pVar->Get(filename);
		if (!filename.IsEmpty())
			resources.Add(filename);
	}

	for (int i = 0; i < pVar->GetNumVariables(); i++)
	{
		GatherUsedResourcesInVar(pVar->GetVariable(i), resources);
	}
}

//////////////////////////////////////////////////////////////////////////
inline bool CompareNames(IVariable* pVar1, IVariable* pVar2)
{
	return (stricmp(pVar1->GetHumanName(), pVar2->GetHumanName()) < 0);
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::Sort()
{
	std::sort(m_vars.begin(), m_vars.end(), CompareNames);
}

//////////////////////////////////////////////////////////////////////////
void CVarObject::AddVariable(CVariableBase& var, const string& varName, VarOnSetCallback cb, unsigned char dataType)
{
	var.AddRef(); // Variables are local and must not be released by CVarBlock.
	var.SetName(varName);
	var.SetHumanName(varName);
	var.SetDataType(dataType);
	if (cb)
		var.AddOnSetCallback(cb);
	CVarBlock::AddVariable(&var);
}

//////////////////////////////////////////////////////////////////////////
void CVarObject::AddVariable(CVariableBase& var, const string& varName, const string& varHumanName, VarOnSetCallback cb, unsigned char dataType)
{
	var.AddRef(); // Variables are local and must not be released by CVarBlock.
	var.SetName(varName);
	var.SetHumanName(varHumanName);
	var.SetDataType(dataType);
	if (cb)
		var.AddOnSetCallback(cb);
	CVarBlock::AddVariable(&var);
}

//////////////////////////////////////////////////////////////////////////
void CVarObject::AddVariable(CVariableArray& table, CVariableBase& var, const string& varName, const string& varHumanName, VarOnSetCallback cb, unsigned char dataType)
{
	var.AddRef(); // Variables are local and must not be released by CVarBlock.
	var.SetName(varName);
	var.SetHumanName(varHumanName);
	var.SetDataType(dataType);
	if (cb)
		var.AddOnSetCallback(cb);
	table.AddVariable(&var);
}

//////////////////////////////////////////////////////////////////////////
void CVarObject::CopyVariableValues(CVarObject* sourceObject)
{
	if (sourceObject == nullptr)
		return;

	// Check if compatible types.

	CopyValues(sourceObject);
}

CVarGlobalEnumList::CVarGlobalEnumList(const string& enumName)
{
	m_pEnum = GetIEditor()->GetUIEnumsDatabase()->FindEnum(enumName);
}

//! Get the name of specified value in enumeration.
const char* CVarGlobalEnumList::GetItemName(uint index)
{
	if (!m_pEnum || index >= m_pEnum->strings.size())
		return NULL;
	return m_pEnum->strings[index];
}

string CVarGlobalEnumList::NameToValue(const string& name)
{
	if (m_pEnum)
		return m_pEnum->NameToValue(name);
	else
		return name;
}

string CVarGlobalEnumList::ValueToName(const string& value)
{
	if (m_pEnum)
		return m_pEnum->ValueToName(value);
	else
		return value;
}

struct SVariableSerializer
{
	template<class T>
	static void SerializeT(IVariable* pVar, Serialization::IArchive& ar, const char* name, const char* label)
	{
		if (pVar->GetEnumList())
		{
			SerializeEnumT<T>(pVar, ar, name, label);
			return;
		}

		T value;
		pVar->Get(value);
		ar(value, name, label);

		if (pVar->GetDescription() && *pVar->GetDescription())
			ar.doc(pVar->GetDescription());

		if (ar.isInput())
		{
			pVar->Set(value);
		}
	}

	template<class T>
	static void SerializeNumericalT(IVariable* pVar, Serialization::IArchive& ar, const char* name, const char* label)
	{
		if (pVar->GetEnumList())
		{
			SerializeEnumT<T>(pVar, ar, name, label);
			return;
		}

		T value;
		pVar->Get(value);

		T dmin = std::numeric_limits<T>::lowest();
		T dmax = std::numeric_limits<T>::max();
		float limMin = dmin;
		float limMax = dmax;
		bool bHardMin, bHardMax;
		float fStep;
		pVar->GetLimits(limMin, limMax, fStep, bHardMin, bHardMax);
		// determine hard min/max values to have correct range decoration
		if (bHardMin && bHardMax)
		{
			ar(Serialization::Range<T>(value, limMin, limMax), name, label);
		}
		else if (bHardMin)
		{
			ar(Serialization::Range<T>(value, limMin, dmax), name, label);
		}
		else if (bHardMax)
		{
			ar(Serialization::Range<T>(value, dmin, limMax), name, label);
		}
		else
		{
			ar(Serialization::Range<T>(value, dmin, dmax), name, label);
		}

		if (pVar->GetDescription() && *pVar->GetDescription())
			ar.doc(pVar->GetDescription());

		if (ar.isInput())
		{
			pVar->Set(value);
		}
	}

	template<class T>
	static void SerializeEnumT(IVariable* pVar, Serialization::IArchive& ar, const char* name, const char* label)
	{
		T value;
		pVar->Get(value);

		CVarEnumListBase<T>* varEnums = (CVarEnumListBase<T>*)pVar->GetEnumList();
		Serialization::StringList stringList;
		for (int i = 0; i < 10000; ++i)
		{
			const char* enumStr = varEnums->GetItemName(i);
			if (!enumStr)
				break;
			stringList.push_back(enumStr);
		}

		const char* enumValue = varEnums->ValueToName(value);
		int index = std::max(stringList.find(enumValue), 0);
		Serialization::StringListValue stringListValue(stringList, index);
		ar(stringListValue, name, label);

		if (pVar->GetDescription() && *pVar->GetDescription())
			ar.doc(pVar->GetDescription());

		if (ar.isInput())
		{
			enumValue = stringListValue.c_str();
			value = varEnums->NameToValue(enumValue);
			pVar->Set(value);
		}
	}

	static void SerializeString(IVariable* pVar, Serialization::IArchive& ar, const char* name, const char* label)
	{
		if (pVar->GetEnumList())
		{
			SerializeEnumT<string>(pVar, ar, name, label);
			return;
		}

		string str;
		pVar->Get(str);
		string value = (const char*)str;

		uint8 dataType = pVar->GetDataType();
		if (dataType == IVariable::DT_TEXTURE)
		{
			ar(Serialization::TextureFilename(value), name, label);
		}
		else if (dataType == IVariable::DT_ANIMATION)
		{
			ar(Serialization::AnimationAlias(value), name, label);
		}
		else if (dataType == IVariable::DT_FILE)
		{
			ar(Serialization::GeneralFilename(value), name, label);
		}
		else if (dataType == IVariable::DT_AUDIO_TRIGGER)
		{
			ar(Serialization::AudioTrigger(value), name, label);
		}
		else if (dataType == IVariable::DT_AUDIO_SWITCH)
		{
			ar(Serialization::AudioSwitch(value), name, label);
		}
		else if (dataType == IVariable::DT_AUDIO_SWITCH_STATE)
		{
			ar(Serialization::AudioSwitchState(value), name, label);
		}
		else if (dataType == IVariable::DT_AUDIO_RTPC)
		{
			ar(Serialization::AudioRTPC(value), name, label);
		}
		else if (dataType == IVariable::DT_AUDIO_ENVIRONMENT)
		{
			ar(Serialization::AudioEnvironment(value), name, label);
		}
		else if (dataType == IVariable::DT_AUDIO_PRELOAD_REQUEST)
		{
			ar(Serialization::AudioPreloadRequest(value), name, label);
		}
		else if (dataType == IVariable::DT_OBJECT)
		{
			ar(Serialization::ModelFilename(value), name, label);
		}
		else if (dataType == IVariable::DT_SOCLASSES)
		{
			ar(Serialization::SmartObjectClasses(value), name, label);
		}
		else if (dataType == IVariable::DT_MISSIONOBJ)
		{
			ar(Serialization::MissionPicker(value), name, label);
		}
		else if (dataType == IVariable::DT_MATERIAL)
		{
			ar(Serialization::MaterialPicker(value), name, label);
		}
		else if (dataType == IVariable::DT_GEOM_CACHE)
		{
			ar(Serialization::GeomCachePicker(value), name, label);
		}
		else if (dataType == IVariable::DT_PARTICLE_EFFECT)
		{
			ar(Serialization::ParticlePickerLegacy(value), name, label);
		}
		// ... @TODO to be expanded here.
		else
		{
			ar(value, name, label);
		}

		if (pVar->GetDescription() && *pVar->GetDescription())
			ar.doc(pVar->GetDescription());

		if (ar.isInput())
		{
			str = value.c_str();
			pVar->Set(str);
		}
	}

	static void SerializeVariable(IVariable* pVar, Serialization::IArchive& ar)
	{
		const char* name = pVar->GetName();
		const char* label = pVar->GetHumanName();

		uint8 dataType = pVar->GetDataType();
		IVariable::EType type = pVar->GetType();

		// ignore invisible variables (assuming we serialize for UI)
		if (pVar->GetFlags() & IVariable::UI_INVISIBLE)
			return;

		if (type == IVariable::ARRAY)
		{
			int numv = pVar->GetNumVariables();
			if (numv > 0)
			{
				if (ar.openBlock(pVar->GetName(), pVar->GetHumanName()))
				{
					for (int i = 0; i < numv; ++i)
						SerializeVariable(pVar->GetVariable(i), ar);

					ar.closeBlock();
				}
			}
		}
		else if (type == IVariable::BOOL)
		{
			SerializeT<bool>(pVar, ar, name, label);
		}
		else if (type == IVariable::INT)
		{
			if (dataType == IVariable::DT_COLOR)
			{
				int abgr = 0;
				pVar->Get(abgr);
				ColorB color = ColorB((uint32)abgr);
				ar(color, name, label);

				if (pVar->GetDescription() && *pVar->GetDescription())
					ar.doc(pVar->GetDescription());

				if (ar.isInput())
				{
					abgr = color.pack_abgr8888();
					pVar->Set(abgr);
				}
			}
			// Copy use case from PropertyItem control
			else if (dataType == IVariable::DT_UIENUM)
			{
				CUIEnumsDatabase_SEnum* m_pEnumDBItem = GetIEditor()->GetUIEnumsDatabase()->FindEnum(label);

				// hardcoded enum copied from SerializeEnumT
				if (m_pEnumDBItem)
				{
					int value;
					pVar->Get(value);

					Serialization::StringList stringList;

					for (int i = 0; i < m_pEnumDBItem->strings.size(); i++)
					{
						stringList.push_back((LPCSTR)m_pEnumDBItem->strings[i]);
					}

					string svalue;
					svalue.Format("%d", value);

					const char* enumValue = (LPCSTR)m_pEnumDBItem->ValueToName(svalue);
					int index = std::max(stringList.find(enumValue), 0);

					Serialization::StringListValue stringListValue(stringList, index);
					ar(stringListValue, name, label);

					if (pVar->GetDescription() && *pVar->GetDescription())
						ar.doc(pVar->GetDescription());

					if (ar.isInput())
					{
						string enumValue = m_pEnumDBItem->NameToValue(stringListValue.c_str());
						int inew = atoi((LPCSTR)enumValue);
						if (value != inew)
						{
							pVar->Set(inew);
						}
					}
				}
				else
				{
					SerializeNumericalT<int>(pVar, ar, name, label);
				}
			}
			else
			{
				SerializeNumericalT<int>(pVar, ar, name, label);
			}
		}
		else if (type == IVariable::FLOAT)
		{
			SerializeNumericalT<float>(pVar, ar, name, label);
		}
		else if (type == IVariable::VECTOR2)
		{
			SerializeT<Vec2>(pVar, ar, name, label);
		}
		else if (type == IVariable::VECTOR)
		{
			if (dataType == IVariable::DT_COLOR)
			{
				Vec3 v;
				pVar->Get(v);
				ColorF color = ColorF(v.x, v.y, v.z);
				ar(color, name, label);

				if (pVar->GetDescription() && *pVar->GetDescription())
					ar.doc(pVar->GetDescription());

				if (ar.isInput())
				{
					pVar->Set(Vec3(color.r, color.g, color.b));
				}
			}
			else
			{
				SerializeT<Vec3>(pVar, ar, name, label);
			}
		}
		else if (type == IVariable::VECTOR4)
		{
			SerializeT<Vec4>(pVar, ar, name, label);
		}
		else if (type == IVariable::QUAT)
		{
			SerializeT<Quat>(pVar, ar, name, label);
		}
		else if (type == IVariable::STRING)
		{
			// Copy use case from PropertyItem control
			if (dataType == IVariable::DT_UIENUM || dataType == IVariable::DT_EQUIP)
			{	
				CUIEnumsDatabase_SEnum* m_pEnumDBItem;
				
				if (dataType == IVariable::DT_EQUIP)
				{
					// hacky solution, use internal name for enum database to support many script variable names
					// all current equip_ types use EquipmentPack name but better be safe than sorry
					m_pEnumDBItem = GetIEditor()->GetUIEnumsDatabase()->FindEnum("SandboxEquipmentDatabase");
				}
				else 
				{
					m_pEnumDBItem = GetIEditor()->GetUIEnumsDatabase()->FindEnum(label);
				}

				if (m_pEnumDBItem)
				{
					string value;
					pVar->Get(value);

					Serialization::StringList stringList;

					for (int i = 0; i < m_pEnumDBItem->strings.size(); i++)
					{
						stringList.push_back((LPCSTR)m_pEnumDBItem->strings[i]);
					}

					string svalue = m_pEnumDBItem->ValueToName(value);
					const char* enumValue = (LPCSTR)svalue;
					int index = std::max(stringList.find(enumValue), 0);

					Serialization::StringListValue stringListValue(stringList, index);
					ar(stringListValue, name, label);

					if (pVar->GetDescription() && *pVar->GetDescription())
						ar.doc(pVar->GetDescription());

					if (ar.isInput())
					{
						string newValue = m_pEnumDBItem->NameToValue(stringListValue.c_str());

						if (value != newValue)
						{
							pVar->Set(newValue);
						}
					}
				}
				else
				{
					SerializeString(pVar, ar, name, label);
				}
			}
			else
			{
				SerializeString(pVar, ar, name, label);
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////
void CVarBlock::Serialize(Serialization::IArchive& ar)
{
	for (size_t i = 0; i < m_vars.size(); ++i)
	{
		SVariableSerializer::SerializeVariable(m_vars[i], ar);
	}
}

//////////////////////////////////////////////////////////////////////////
void CVarBlock::SerializeVariable(IVariable* pVariable, Serialization::IArchive& ar)
{
	SVariableSerializer::SerializeVariable(pVariable, ar);
}

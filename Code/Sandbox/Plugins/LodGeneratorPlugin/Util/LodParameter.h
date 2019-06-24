#pragma once

#include <CrySerialization/IArchive.h>
#include <CryMath/Cry_Math.h>

enum ELodParamType
{
	eUNKownType,
	eFloatType,
	eIntType,
	eColorType,
	eStringType,
	eBoolType
};

class CGeneratorOptionsPanel;
struct IVariable;
struct SLodParameter
{
public:
	void Serialize(Serialization::IArchive& ar);

	void SetOptionsPanel(CGeneratorOptionsPanel* pCGeneratorOptionsPanel)
	{
		m_CGeneratorOptionsPanel = pCGeneratorOptionsPanel;
	}

	CGeneratorOptionsPanel* GetOptionsPanel()
	{
		return m_CGeneratorOptionsPanel;
	}

	void SetName(string name)
	{
		m_ParamName = name;
	}
	string GetName(){return m_ParamName;};

	void SetLabel(string labelName)
	{
		m_LabelName = labelName;
	}
	string GetLabel(){return m_LabelName;};

	void SetGroupName(string groupName)
	{
		m_GroupName = groupName;
	}
	string GetGroupName(){return m_GroupName;};

	void SetTODParamType(ELodParamType type){m_TODParamType=type;};
	ELodParamType GetTODParamType(){return m_TODParamType;};

	void SetParamID(int id){m_ID = id;};
	int GetParamID(){return m_ID;};

	void SetParamColor(Vec3 color){m_Color = color;};
	Vec3 GetParamColor(){return m_Color;};
	Vec3 m_Color;

	void SetParamFloat3(Vec3 paramValue){ m_valueFloat3 = paramValue;};
	Vec3 GetParamFloat3(){ return m_valueFloat3;};
	Vec3 m_valueFloat3;

	void SetParamFloat(float paramValue){ m_valueFloat = paramValue;};
	float GetParamFloat(){ return m_valueFloat;};
	float m_valueFloat;

	void SetParamBool(bool paramValue){ m_valueBool = paramValue;};
	bool GetParamBool(){ return m_valueBool;};
	bool m_valueBool;

	void SetParamInt(int paramValue){ m_valueInt = paramValue;};
	int GetParamInt(){ return m_valueInt;};
	int m_valueInt;


	void SetParamString(string paramValue){ m_valueString = paramValue;};
	string GetParamString(){ return m_valueString;};

	string m_valueString;

	string m_ParamName;
	string m_LabelName;
	string m_GroupName;
	int m_ID;
	ELodParamType m_TODParamType;

	CGeneratorOptionsPanel* m_CGeneratorOptionsPanel;

	SLodParameter()
	{
		m_ParamName = "DefaultName";
		m_LabelName = "DefaultLabel";
		m_GroupName = "DefaultGroup";
		m_ID = 0;
		m_Color= Vec3(0, 0, 0);
		m_valueFloat3= Vec3(0, 0, 0);
		m_valueFloat = 0.0;
		m_valueInt = 0;
		m_valueBool = false;
		m_valueString = "";
		m_TODParamType = eFloatType;
		m_CGeneratorOptionsPanel = NULL;
	}

	static ELodParamType GetLodParamType(IVariable* pCurVariable);
};

struct SLodParameterGroup
{
	void Serialize(Serialization::IArchive& ar)
	{
		for(size_t i = 0; i < m_Params.size() ; ++i)
		{
			ar(m_Params[i], m_Params[i].GetName(), m_Params[i].GetLabel());
		}
	}

	std::vector<SLodParameter> m_Params;
};

typedef std::map<string,SLodParameterGroup> TLodPropertyGroupMap;

struct SLodParameterGroupSet
{
	TLodPropertyGroupMap m_propertyGroups;

	void Serialize(Serialization::IArchive& ar)
	{
		for (TLodPropertyGroupMap::iterator it = m_propertyGroups.begin(); it != m_propertyGroups.end(); ++it) 
		{
			const string& name = it->first;
			SLodParameterGroup& group = it->second;
			ar(group, name, name);
		}
	}
};


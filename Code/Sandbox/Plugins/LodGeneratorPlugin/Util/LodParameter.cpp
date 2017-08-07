#include "StdAfx.h"
#include "LodParameter.h"
#include <CrySerialization/IArchive.h>
#include <CrySerialization/Callback.h>
#include <CrySerialization/Decorators/Range.h>
#include <CrySerialization/Color.h>
#include <CrySerialization/Enum.h>
#include <CrySerialization/Decorators/LocalFrame.h>
#include "Controls/PropertyItem.h"
#include "Util/Variable.h"
#include "../panel/GeneratorOptionsPanel.h"

using Serialization::Vec3AsColor;
void SLodParameter::Serialize(Serialization::IArchive& ar)
{
	ar(m_ID, "m_id");

	if (m_TODParamType == ELodParamType::eColorType)
	{
		ar(Vec3AsColor(m_Color), "color", "^");
		if (m_CGeneratorOptionsPanel != NULL)
			m_CGeneratorOptionsPanel->ParameterChanged(this);
	}
	else if (m_TODParamType == ELodParamType::eFloatType)
	{
		auto callback = [&](float v) 
		{
			m_valueFloat = v;

			if(m_CGeneratorOptionsPanel != NULL)
				m_CGeneratorOptionsPanel->ParameterChanged(this); 
		};

		ar(Serialization::Callback(m_valueFloat, callback), "Float", "^");	
	}
	else if (m_TODParamType == ELodParamType::eIntType)
	{
		auto callback = [&](int v) 
		{
			m_valueInt = v;

			if(m_CGeneratorOptionsPanel != NULL)
				m_CGeneratorOptionsPanel->ParameterChanged(this); 
		};

		ar(Serialization::Callback(m_valueInt, callback), "I32", "^");	
	}
	else if (m_TODParamType == ELodParamType::eBoolType)
	{
		auto callback = [&](bool v) 
		{
			m_valueBool = v;

			if(m_CGeneratorOptionsPanel != NULL)
				m_CGeneratorOptionsPanel->ParameterChanged(this); 
		};

		ar(Serialization::Callback(m_valueBool, callback), "bool", "^");	
	} 
	else if (m_TODParamType == ELodParamType::eStringType)
	{
		auto callback = [&](string v) 
		{
			m_valueString = v;

			if(m_CGeneratorOptionsPanel != NULL)
				m_CGeneratorOptionsPanel->ParameterChanged(this); 
		};

		ar(Serialization::Callback(m_valueString, callback), "string", "^");
	}
	else
	{
		//....
	}
	
}



static struct {
	int dataType;
	const char *name;
	PropertyType type;
	int image;
} s_propertyTypeNames[] =
{
	{ IVariable::DT_SIMPLE,"Bool",PropertyType::ePropertyBool,2 },
	{ IVariable::DT_SIMPLE,"Int",PropertyType::ePropertyInt,0 },
	{ IVariable::DT_SIMPLE,"Float",PropertyType::ePropertyFloat,0 },
	{ IVariable::DT_SIMPLE,"Vector",PropertyType::ePropertyVector2,10 },
	{ IVariable::DT_SIMPLE,"Vector",PropertyType::ePropertyVector,10 },
	{ IVariable::DT_SIMPLE,"Vector",PropertyType::ePropertyVector4,10 },
	{ IVariable::DT_SIMPLE,"String",PropertyType::ePropertyString,3 },
	{ IVariable::DT_PERCENT,"Float",PropertyType::ePropertyInt,13 },
	{ IVariable::DT_BOOLEAN,"Boolean",PropertyType::ePropertyBool,2 },
	{ IVariable::DT_COLOR,"Color",PropertyType::ePropertyColor,1 },
	{ IVariable::DT_CURVE|IVariable::DT_PERCENT,"FloatCurve",PropertyType::ePropertyFloatCurve,13 },
	{ IVariable::DT_CURVE|IVariable::DT_COLOR,"ColorCurve",PropertyType::ePropertyColorCurve,1 },
	{ IVariable::DT_ANGLE,"Angle",PropertyType::ePropertyAngle,0 },
	{ IVariable::DT_FILE,"File",PropertyType::ePropertyFile,7 },
	{ IVariable::DT_TEXTURE,"Texture",PropertyType::ePropertyTexture,4 },
	{ IVariable::DT_ANIMATION,"Animation",PropertyType::ePropertyAnimation,-1 },
	{ IVariable::DT_OBJECT,"Model",PropertyType::ePropertyModel,5 },
	{ IVariable::DT_SIMPLE,"Selection",PropertyType::ePropertySelection,-1 },
	{ IVariable::DT_SIMPLE,"List",PropertyType::ePropertyList,-1 },
	{ IVariable::DT_SHADER,"Shader",PropertyType::ePropertyShader,9 },
	{ IVariable::DT_MATERIAL,"Material",PropertyType::ePropertyMaterial,14 },
	{ IVariable::DT_AI_BEHAVIOR,"AIBehavior",PropertyType::ePropertyAiBehavior,8 },
	{ IVariable::DT_AI_ANCHOR,"AIAnchor",PropertyType::ePropertyAiAnchor,8 },
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
	{ IVariable::DT_AI_CHARACTER,"AICharacter",PropertyType::ePropertyAiCharacter,8 },
#endif
	{ IVariable::DT_AI_PFPROPERTIESLIST,"AgentTypeList",PropertyType::ePropertyAiPFPropertiesList,8 },
	{ IVariable::DT_AIENTITYCLASSES,"AI Entity Classes",PropertyType::ePropertyAiEntityClasses,8 },
	{ IVariable::DT_EQUIP,"Equip",PropertyType::ePropertyEquip,11 },
	{ IVariable::DT_REVERBPRESET,"ReverbPreset",PropertyType::ePropertyReverbPreset,11 },
	{ IVariable::DT_LOCAL_STRING,"LocalString",PropertyType::ePropertyLocalString,3 },
	{ IVariable::DT_SOCLASS,"Smart Object Class",PropertyType::ePropertySOClass,8 },
	{ IVariable::DT_SOCLASSES,"Smart Object Classes",PropertyType::ePropertySOClasses,8 },
	{ IVariable::DT_SOSTATE,"Smart Object State",PropertyType::ePropertySOState,8 },
	{ IVariable::DT_SOSTATES,"Smart Object States",PropertyType::ePropertySOStates,8 },
	{ IVariable::DT_SOSTATEPATTERN,"Smart Object State Pattern",PropertyType::ePropertySOStatePattern,8 },
	{ IVariable::DT_SOACTION,"AI Action",PropertyType::ePropertySOAction,8 },
	{ IVariable::DT_SOHELPER,"Smart Object Helper",PropertyType::ePropertySOHelper,8 },
	{ IVariable::DT_SONAVHELPER,"Smart Object Navigation Helper",PropertyType::ePropertySONavHelper,8 },
	{ IVariable::DT_SOANIMHELPER,"Smart Object Animation Helper",PropertyType::ePropertySOAnimHelper,8 },
	{ IVariable::DT_SOEVENT,"Smart Object Event",PropertyType::ePropertySOEvent,8 },
	{ IVariable::DT_SOTEMPLATE,"Smart Object Template",PropertyType::ePropertySOTemplate,8 },
	{ IVariable::DT_CUSTOMACTION,"Custom Action",PropertyType::ePropertyCustomAction,7 },
	{ IVariable::DT_VEEDHELPER,"Vehicle Helper",PropertyType::ePropertySelection,-1 },
	{ IVariable::DT_VEEDPART,"Vehicle Part",PropertyType::ePropertySelection,-1 },
	{ IVariable::DT_VEEDCOMP,"Vehicle Component",PropertyType::ePropertySelection,-1 },
	{ IVariable::DT_GAMETOKEN,"Game Token",PropertyType::ePropertyGameToken, -1 },
	{ IVariable::DT_SEQUENCE,"Sequence",PropertyType::ePropertySequence, -1 },
	{ IVariable::DT_MISSIONOBJ,"Mission Objective",PropertyType::ePropertyMissionObj, -1 },
	{ IVariable::DT_USERITEMCB,"User",PropertyType::ePropertyUser, -1 },
	{ IVariable::DT_AITERRITORY,"AITerritory",PropertyType::ePropertyAiTerritory,8 },
	{ IVariable::DT_AIWAVE,"AIWave",PropertyType::ePropertyAiWave,8 },
	{ IVariable::DT_SEQUENCE_ID,"SequenceId",PropertyType::ePropertySequenceId, -1 },
	//{ IVariable::DT_LIGHT_ANIMATION,"LightAnimation",PropertyType::ePropertyLightAnimation, -1 },
	{ IVariable::DT_FLARE,"Flare",PropertyType::ePropertyFlare,7 },
	{ IVariable::DT_PARTICLE_EFFECT,"ParticleEffect",PropertyType::ePropertyParticleName, 3 },
	{ IVariable::DT_GEOM_CACHE,"Geometry Cache",PropertyType::ePropertyGeomCache,5 },
	{ IVariable::DT_AUDIO_TRIGGER,"Audio Trigger",PropertyType::ePropertyAudioTrigger,6 },
	{ IVariable::DT_AUDIO_SWITCH,"Audio Switch",PropertyType::ePropertyAudioSwitch,6 },
	{ IVariable::DT_AUDIO_SWITCH_STATE,"Audio Switch",PropertyType::ePropertyAudioSwitchState,6 },
	{ IVariable::DT_AUDIO_RTPC,"Audio Realtime Parameter Control",PropertyType::ePropertyAudioRTPC,6 },
	{ IVariable::DT_AUDIO_ENVIRONMENT,"Audio Environment",PropertyType::ePropertyAudioEnvironment,6 },
	{ IVariable::DT_AUDIO_PRELOAD_REQUEST,"Audio Preload Request",PropertyType::ePropertyAudioPreloadRequest,6 },
};
static int NumPropertyTypes = sizeof(s_propertyTypeNames)/sizeof(s_propertyTypeNames[0]);

void GetVariableParams( IVariable *var ,PropertyType& pType,float& fMin,float& fMax,float& fStep);

ELodParamType SLodParameter::GetLodParamType(IVariable* pCurVariable)
{
	ELodParamType eLodParamType = eUNKownType;
	PropertyType pType;
	float fMin;
	float fMax;
	float fStep;
	GetVariableParams(pCurVariable,pType,fMin,fMax,fStep);

	switch (pType)
	{
	case ePropertyInvalid:
		break;
	case ePropertyTable:
		break;
	case ePropertyBool:
		eLodParamType = eBoolType;
		break;
	case ePropertyInt:
		eLodParamType = eIntType;
		break;
	case ePropertyFloat:
		eLodParamType = eFloatType;
		break;
	case ePropertyVector2:
		break;
	case ePropertyVector:
		break;
	case ePropertyVector4:
		break;
	case ePropertyString:
		eLodParamType = eStringType;
		break;
	case ePropertyColor:
		eLodParamType = eColorType;
		break;
	case ePropertyAngle:
		break;
	case ePropertyFloatCurve:
		break;
	case ePropertyColorCurve:
		break;
	case ePropertyFile:
		break;
	case ePropertyTexture:
		break;
	case ePropertyAnimation:
		break;
	case ePropertyModel:
		break;
	case ePropertySelection:
		break;
	case ePropertyList:
		break;
	case ePropertyShader:
		break;
	case ePropertyMaterial:
		break;
	case ePropertyAiBehavior:
		break;
	case ePropertyAiAnchor:
		break;
	case ePropertyAiPFPropertiesList:
		break;
	case ePropertyAiEntityClasses:
		break;
	case ePropertyAiTerritory:
		break;
	case ePropertyAiWave:
		break;
	case ePropertyEquip:
		break;
	case ePropertyReverbPreset:
		break;
	case ePropertyLocalString:
		break;
	case ePropertySOClass:
		break;
	case ePropertySOClasses:
		break;
	case ePropertySOState:
		break;
	case ePropertySOStates:
		break;
	case ePropertySOStatePattern:
		break;
	case ePropertySOAction:
		break;
	case ePropertySOHelper:
		break;
	case ePropertySONavHelper:
		break;
	case ePropertySOAnimHelper:
		break;
	case ePropertySOEvent:
		break;
	case ePropertySOTemplate:
		break;
	case ePropertyCustomAction:
		break;
	case ePropertyGameToken:
		break;
	case ePropertySequence:
		break;
	case ePropertyMissionObj:
		break;
	case ePropertyUser:
		break;
	case ePropertySequenceId:
		break;
	/*case ePropertyLightAnimation:
		break;*/
	case ePropertyFlare:
		break;
	case ePropertyParticleName:
		break;
	case ePropertyGeomCache:
		break;
	case ePropertyAudioTrigger:
		break;
	case ePropertyAudioSwitch:
		break;
	case ePropertyAudioSwitchState:
		break;
	case ePropertyAudioRTPC:
		break;
	case ePropertyAudioEnvironment:
		break;
	case ePropertyAudioPreloadRequest:
		break;
	default:
		break;
	}

	return eLodParamType;
}

void GetVariableParams( IVariable *var ,PropertyType& pType,float& fMin,float& fMax,float& fStep)
{
	_smart_ptr<IVariable> pInputVar = var;

	int dataType = var->GetDataType();
	string m_name = var->GetHumanName();
	pType = ePropertyInvalid;
	int i;

	if (dataType != IVariable::DT_SIMPLE)
	{
		for (i = 0; i < NumPropertyTypes; i++)
		{
			if (dataType == s_propertyTypeNames[i].dataType)
			{
				pType = s_propertyTypeNames[i].type;
				break;
			}
		}
	}

	IVarEnumList* enumList = var->GetEnumList();
	if (enumList != NULL)
	{
		pType = ePropertySelection;
	}

	if (pType == ePropertyInvalid)
	{
		switch(var->GetType()) 
		{
		case IVariable::INT:
			pType = ePropertyInt;
			break;
		case IVariable::BOOL:
			pType = ePropertyBool;
			break;
		case IVariable::FLOAT:
			pType = ePropertyFloat;
			break;		
		case IVariable::VECTOR2:
			pType = ePropertyVector2;
			break;
		case IVariable::VECTOR4:
			pType = ePropertyVector4;
			break;
		case IVariable::VECTOR:
			pType = ePropertyVector;
			break;
		case IVariable::STRING:
			pType = ePropertyString;
			break;
		}
	}

	float m_valueMultiplier = 1;
	float m_rangeMin = 0;
	float m_rangeMax = 100;
	float m_step = 0.f;
	bool m_bHardMin  = false;
	bool m_bHardMax = false;
	// Get variable limits.
	var->GetLimits( m_rangeMin, m_rangeMax, m_step, m_bHardMin, m_bHardMax );

	// Check if value is percents.
	if (dataType == IVariable::DT_PERCENT)
	{
		// Scale all values by 100.
		m_valueMultiplier = 100;
	}
	else if (dataType == IVariable::DT_ANGLE)
	{
		// Scale radians to degrees.
		m_valueMultiplier = RAD2DEG(1);
		m_rangeMin = -360;
		m_rangeMax =  360;
	}
	else if (dataType == IVariable::DT_UIENUM)
	{
		//CUIEnumsDatabase_SEnum* m_pEnumDBItem = GetIEditor()->GetUIEnumsDatabase()->FindEnum(m_name);
	}


	const bool useExplicitStep = (var->GetFlags() & IVariable::UI_EXPLICIT_STEP);
	if (!useExplicitStep)
	{
		// Limit step size to 1000.
		int nPrec = max(3 - int(log(m_rangeMax - m_rangeMin) / log(10.f)), 0);
		m_step = max(m_step, powf(10.f, -nPrec));
	}

}

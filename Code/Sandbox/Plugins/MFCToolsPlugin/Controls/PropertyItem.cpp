// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*
   Relationship between Variable data, PropertyItem value string, and draw string

    Variable:
      holds raw data
    PropertyItem.m_value string:
      Converted via Variable.Get/SetDisplayValue
      = Same as Variable.Get(string) and Variable.Set(string), EXCEPT:
        Enums converted to enum values
        Color reformated to 8-bit range
        SOHelper removes prefix before :
    Draw string:
      Same as .m_value, EXCEPT
        .valueMultiplier conversion applied
        Bool reformated to "True/False"
        Curves replaced with "[Curve]"
        bShowChildren vars add child draw strings
    Input string:
      Set to .m_value, EXCEPT
        Bool converted to "1/0"
        File text replace \ with /
        Vector expand single elem to 3 repeated
 */

#include "StdAfx.h"
#include "PropertyItem.h"
#include "PropertyCtrl.h"
#include "InPlaceEdit.h"
#include "InPlaceComboBox.h"
#include "InPlaceButton.h"
#include "FillSliderCtrl.h"
#include "SplineCtrl.h"
#include "ColorGradientCtrl.h"
#include "SliderCtrlEx.h"

#include "Dialogs/CustomColorDialog.h"

#include "Controls/SharedFonts.h"
#include "Controls/NumberCtrl.h"

#include "UIEnumsDatabase.h"
#include "Dialogs/SelectSequenceDialog.h"
#include "Dialogs/GenericSelectItemDialog.h"
#include "Dialogs/SelectMissionObjectiveDialog.h"

#include <CrySystem/ITimer.h>
#include <CrySystem/ILocalizationManager.h>

#include "ISourceControl.h"

#include <CryString/CryName.h>

#include "Util/UIEnumerations.h"
#include "IDataBaseItem.h"
#include "IDataBaseManager.h"
#include "Objects/BaseObject.h"

#include "IResourceSelectorHost.h"
#include <CryString/UnicodeFunctions.h>
#include "Controls/QuestionDialog.h"
#include "Util/MFCUtil.h"
#include "Util/FileUtil.h"
#include <CryMovie/IMovieSystem.h>

//////////////////////////////////////////////////////////////////////////
#define CMD_ADD_CHILD_ITEM 100
#define CMD_ADD_ITEM       101
#define CMD_DELETE_ITEM    102

#define BUTTON_WIDTH       (16)
#define NUMBER_CTRL_WIDTH  60

//////////////////////////////////////////////////////////////////////////
//! Undo object for Variable in property control..
class CUndoVariableChange : public IUndoObject
{
public:
	CUndoVariableChange(IVariable* var, const char* undoDescription)
	{
		// Stores the current state of this object.
		assert(var != 0);
		m_undoDescription = undoDescription;
		m_var = var;
		m_undo = m_var->Clone(false);
	}
protected:
	virtual int GetSize()
	{
		int size = sizeof(*this);
		//if (m_var)
		//size += m_var->GetSize();
		if (m_undo)
			size += m_undo->GetSize();
		if (m_redo)
			size += m_redo->GetSize();
		return size;
	}
	virtual const char* GetDescription() { return m_undoDescription; };
	virtual void        Undo(bool bUndo)
	{
		if (bUndo)
		{
			m_redo = m_var->Clone(false);
		}
		m_var->CopyValue(m_undo);
	}
	virtual void Redo()
	{
		if (m_redo)
			m_var->CopyValue(m_redo);
	}

private:
	CString              m_undoDescription;
	_smart_ptr<IVariable> m_undo;
	_smart_ptr<IVariable> m_redo;
	_smart_ptr<IVariable> m_var;
};

//////////////////////////////////////////////////////////////////////////
namespace {
struct
{
	int          dataType;
	const char*  name;
	PropertyType type;
	int          image;
} s_propertyTypeNames[] =
{
	{ IVariable::DT_SIMPLE,                        "Bool",                             ePropertyBool,                  2  },
	{ IVariable::DT_SIMPLE,                        "Int",                              ePropertyInt,                   0  },
	{ IVariable::DT_SIMPLE,                        "Float",                            ePropertyFloat,                 0  },
	{ IVariable::DT_SIMPLE,                        "Vector",                           ePropertyVector2,               10 },
	{ IVariable::DT_SIMPLE,                        "Vector",                           ePropertyVector,                10 },
	{ IVariable::DT_SIMPLE,                        "Vector",                           ePropertyVector4,               10 },
	{ IVariable::DT_SIMPLE,                        "String",                           ePropertyString,                3  },
	{ IVariable::DT_PERCENT,                       "Float",                            ePropertyInt,                   13 },
	{ IVariable::DT_BOOLEAN,                       "Boolean",                          ePropertyBool,                  2  },
	{ IVariable::DT_COLOR,                         "Color",                            ePropertyColor,                 1  },
	{ IVariable::DT_CURVE | IVariable::DT_PERCENT, "FloatCurve",                       ePropertyFloatCurve,            13 },
	{ IVariable::DT_CURVE | IVariable::DT_COLOR,   "ColorCurve",                       ePropertyColorCurve,            1  },
	{ IVariable::DT_ANGLE,                         "Angle",                            ePropertyAngle,                 0  },
	{ IVariable::DT_FILE,                          "File",                             ePropertyFile,                  7  },
	{ IVariable::DT_TEXTURE,                       "Texture",                          ePropertyTexture,               4  },
	{ IVariable::DT_ANIMATION,                     "Animation",                        ePropertyAnimation,             -1 },
	{ IVariable::DT_OBJECT,                        "Model",                            ePropertyModel,                 5  },
	{ IVariable::DT_SIMPLE,                        "Selection",                        ePropertySelection,             -1 },
	{ IVariable::DT_SIMPLE,                        "List",                             ePropertyList,                  -1 },
	{ IVariable::DT_SHADER,                        "Shader",                           ePropertyShader,                9  },
	{ IVariable::DT_MATERIAL,                      "Material",                         ePropertyMaterial,              14 },
	{ IVariable::DT_AI_BEHAVIOR,                   "AIBehavior",                       ePropertyAiBehavior,            8  },
	{ IVariable::DT_AI_ANCHOR,                     "AIAnchor",                         ePropertyAiAnchor,              8  },
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
	{ IVariable::DT_AI_CHARACTER,                  "AICharacter",                      ePropertyAiCharacter,           8  },
#endif
	{ IVariable::DT_AI_PFPROPERTIESLIST,           "AgentTypeList",                    ePropertyAiPFPropertiesList,    8  },
	{ IVariable::DT_AIENTITYCLASSES,               "AI Entity Classes",                ePropertyAiEntityClasses,       8  },
	{ IVariable::DT_EQUIP,                         "Equip",                            ePropertyEquip,                 11 },
	{ IVariable::DT_REVERBPRESET,                  "ReverbPreset",                     ePropertyReverbPreset,          11 },
	{ IVariable::DT_LOCAL_STRING,                  "LocalString",                      ePropertyLocalString,           3  },
	{ IVariable::DT_SOCLASS,                       "Smart Object Class",               ePropertySOClass,               8  },
	{ IVariable::DT_SOCLASSES,                     "Smart Object Classes",             ePropertySOClasses,             8  },
	{ IVariable::DT_SOSTATE,                       "Smart Object State",               ePropertySOState,               8  },
	{ IVariable::DT_SOSTATES,                      "Smart Object States",              ePropertySOStates,              8  },
	{ IVariable::DT_SOSTATEPATTERN,                "Smart Object State Pattern",       ePropertySOStatePattern,        8  },
	{ IVariable::DT_SOACTION,                      "AI Action",                        ePropertySOAction,              8  },
	{ IVariable::DT_SOHELPER,                      "Smart Object Helper",              ePropertySOHelper,              8  },
	{ IVariable::DT_SONAVHELPER,                   "Smart Object Navigation Helper",   ePropertySONavHelper,           8  },
	{ IVariable::DT_SOANIMHELPER,                  "Smart Object Animation Helper",    ePropertySOAnimHelper,          8  },
	{ IVariable::DT_SOEVENT,                       "Smart Object Event",               ePropertySOEvent,               8  },
	{ IVariable::DT_SOTEMPLATE,                    "Smart Object Template",            ePropertySOTemplate,            8  },
	{ IVariable::DT_CUSTOMACTION,                  "Custom Action",                    ePropertyCustomAction,          7  },
	{ IVariable::DT_VEEDHELPER,                    "Vehicle Helper",                   ePropertySelection,             -1 },
	{ IVariable::DT_VEEDPART,                      "Vehicle Part",                     ePropertySelection,             -1 },
	{ IVariable::DT_VEEDCOMP,                      "Vehicle Component",                ePropertySelection,             -1 },
	{ IVariable::DT_GAMETOKEN,                     "Game Token",                       ePropertyGameToken,             -1 },
	{ IVariable::DT_SEQUENCE,                      "Sequence",                         ePropertySequence,              -1 },
	{ IVariable::DT_MISSIONOBJ,                    "Mission Objective",                ePropertyMissionObj,            -1 },
	{ IVariable::DT_USERITEMCB,                    "User",                             ePropertyUser,                  -1 },
	{ IVariable::DT_AITERRITORY,                   "AITerritory",                      ePropertyAiTerritory,           8  },
	{ IVariable::DT_AIWAVE,                        "AIWave",                           ePropertyAiWave,                8  },
	{ IVariable::DT_SEQUENCE_ID,                   "SequenceId",                       ePropertySequenceId,            -1 },
	{ IVariable::DT_FLARE,                         "Flare",                            ePropertyFlare,                 7  },
	{ IVariable::DT_PARTICLE_EFFECT,               "ParticleEffect",                   ePropertyParticleName,          3  },
	{ IVariable::DT_GEOM_CACHE,                    "Geometry Cache",                   ePropertyGeomCache,             5  },
	{ IVariable::DT_AUDIO_TRIGGER,                 "Audio Trigger",                    ePropertyAudioTrigger,          6  },
	{ IVariable::DT_AUDIO_SWITCH,                  "Audio Switch",                     ePropertyAudioSwitch,           6  },
	{ IVariable::DT_AUDIO_SWITCH_STATE,            "Audio Switch",                     ePropertyAudioSwitchState,      6  },
	{ IVariable::DT_AUDIO_RTPC,                    "Audio Realtime Parameter Control", ePropertyAudioRTPC,             6  },
	{ IVariable::DT_AUDIO_ENVIRONMENT,             "Audio Environment",                ePropertyAudioEnvironment,      6  },
	{ IVariable::DT_AUDIO_PRELOAD_REQUEST,         "Audio Preload Request",            ePropertyAudioPreloadRequest,   6  },
	{ IVariable::DT_DYNAMIC_RESPONSE_SIGNAL,       "Dynamic Response Signal",          ePropertyDynamicResponseSignal, 6  },
};
static int NumPropertyTypes = sizeof(s_propertyTypeNames) / sizeof(s_propertyTypeNames[0]);

const char* DISPLAY_NAME_ATTR = "DisplayName";
const char* VALUE_ATTR = "Value";
const char* TYPE_ATTR = "Type";
const char* TIP_ATTR = "Tip";
const char* TIP_CVAR_ATTR = "TipCVar";
const char* FILEFILTER_ATTR = "FileFilters";
const int FLOAT_NUM_DIGITS = 6;

static const char* PropertyTypeToResourceType(PropertyType type)
{
	// The strings below are names used together with
	// REGISTER_RESOURCE_SELECTOR. See IResourceSelector.h.
	switch (type)
	{
	case ePropertyAnimation:
		return "CharacterAnimation";
	case ePropertyModel:
		return "Model";
	case ePropertyGeomCache:
		return "GeometryCache";
	case ePropertyAudioTrigger:
		return "AudioTrigger";
	case ePropertyAudioSwitch:
		return "AudioSwitch";
	case ePropertyAudioSwitchState:
		return "AudioSwitchState";
	case ePropertyAudioRTPC:
		return "AudioRTPC";
	case ePropertyAudioEnvironment:
		return "AudioEnvironment";
	case ePropertyAudioPreloadRequest:
		return "AudioPreloadRequest";
	case ePropertyDynamicResponseSignal:
		return "DynamicResponseSignal";
	default:
		return 0;
	}
}

};

HDWP CPropertyItem::s_HDWP = 0;

//////////////////////////////////////////////////////////////////////////

namespace PropertyItem_Private
{
	inline void FormatFloatForUICString(CString& outstr, int significantDigits, double value)
	{
		string crystr = outstr.GetString();
		FormatFloatForUI(crystr, significantDigits, value);
		outstr = crystr.GetString();
	}
}

//////////////////////////////////////////////////////////////////////////
// CPropertyItem implementation.
//////////////////////////////////////////////////////////////////////////
CPropertyItem::CPropertyItem(CPropertyCtrl* pCtrl)
{
	m_propertyCtrl = pCtrl;
	m_pControlsHostWnd = 0;
	m_parent = 0;
	m_bExpandable = false;
	m_bExpanded = false;
	m_bEditChildren = false;
	m_bShowChildren = false;
	m_bSelected = false;
	m_bNoCategory = false;

	m_pStaticText = 0;
	m_cNumber = 0;
	m_cNumber1 = 0;
	m_cNumber2 = 0;
	m_cNumber3 = 0;

	m_cFillSlider = 0;
	m_cEdit = 0;
	m_cCombo = 0;
	m_cButton = 0;
	m_cButton2 = 0;
	m_cButton3 = 0;
	m_cButton4 = 0;
	m_cButton5 = 0;
	m_cExpandButton = 0;
	m_cCheckBox = 0;
	m_cSpline = 0;
	m_cColorSpline = 0;
	m_cColorButton = 0;

	m_image = -1;
	m_bIgnoreChildsUpdate = false;
	m_value = "";
	m_type = ePropertyInvalid;

	m_modified = false;
	m_bMoveControls = false;
	m_valueMultiplier = 1;
	m_pEnumDBItem = 0;

	m_bForceModified = false;

	m_nHeight = 14;

	m_nCategoryPageId = -1;

	m_strNoScriptDefault = "<<undefined>>";
	m_strScriptDefault = m_strNoScriptDefault;
}

CPropertyItem::~CPropertyItem()
{
	// just to make sure we dont double (or infinitely recurse...) delete
	AddRef();

	DestroyInPlaceControl();

	if (m_pVariable)
		ReleaseVariable();

	for (int i = 0; i < m_childs.size(); i++)
	{
		m_childs[i]->m_parent = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::SetXmlNode(XmlNodeRef& node)
{
	m_node = node;
	// No Undo while just creating properties.
	//GetIEditor()->GetIUndoManager()->Suspend();
	ParseXmlNode();
	//GetIEditor()->GetIUndoManager()->Resume();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::ParseXmlNode(bool bRecursive /* =true  */)
{
	if (!m_node->getAttr(DISPLAY_NAME_ATTR, m_name))
	{
		m_name = m_node->getTag();
	}

	CString value;
	bool bHasValue = m_node->getAttr(VALUE_ATTR, value);

	CString type;
	m_node->getAttr(TYPE_ATTR, type);

	m_tip = "";
	m_node->getAttr(TIP_ATTR, m_tip);

	// read description from associated console variable
	CString tipCVarName;
	m_node->getAttr(TIP_CVAR_ATTR, tipCVarName);
	if (!tipCVarName.IsEmpty())
	{
		tipCVarName.Replace("*", m_name);
		if (ICVar* pCVar = gEnv->pConsole->GetCVar(tipCVarName))
			m_tip = pCVar->GetHelp();
	}

	m_image = -1;
	m_type = ePropertyInvalid;
	for (int i = 0; i < NumPropertyTypes; i++)
	{
		if (stricmp(type, s_propertyTypeNames[i].name) == 0)
		{
			m_type = s_propertyTypeNames[i].type;
			m_image = s_propertyTypeNames[i].image;
			break;
		}
	}

	m_rangeMin = 0;
	m_rangeMax = 100;
	m_step = 0.1f;
	m_bHardMin = m_bHardMax = false;
	if (m_type == ePropertyFloat || m_type == ePropertyInt)
	{
		if (m_node->getAttr("Min", m_rangeMin))
			m_bHardMin = true;
		if (m_node->getAttr("Max", m_rangeMax))
			m_bHardMax = true;

		int nPrecision;
		if (!m_node->getAttr("Precision", nPrecision))
			nPrecision = max(3 - int(log(m_rangeMax - m_rangeMin) / log(10.f)), 0);
		m_step = powf(10.f, -nPrecision);
	}

	if (bHasValue)
		SetValue(value, false);

	m_bNoCategory = false;

	if ((m_type == ePropertyVector || m_type == ePropertyVector2 || m_type == ePropertyVector4) && !m_propertyCtrl->IsExtenedUI())
	{
		m_bEditChildren = true;
		bRecursive = false;
		m_childs.clear();

		if (m_type == ePropertyVector)
		{
			Vec3 vec;
			m_node->getAttr(VALUE_ATTR, vec);
			// Create 3 sub elements.
			XmlNodeRef x = m_node->createNode("X");
			XmlNodeRef y = m_node->createNode("Y");
			XmlNodeRef z = m_node->createNode("Z");
			x->setAttr(TYPE_ATTR, "Float");
			y->setAttr(TYPE_ATTR, "Float");
			z->setAttr(TYPE_ATTR, "Float");

			x->setAttr(VALUE_ATTR, vec.x);
			y->setAttr(VALUE_ATTR, vec.y);
			z->setAttr(VALUE_ATTR, vec.z);

			// Start ignoring all updates comming from childs. (Initializing childs).
			m_bIgnoreChildsUpdate = true;

			CPropertyItemPtr itemX = new CPropertyItem(m_propertyCtrl);
			itemX->SetXmlNode(x);
			AddChild(itemX);

			CPropertyItemPtr itemY = new CPropertyItem(m_propertyCtrl);
			itemY->SetXmlNode(y);
			AddChild(itemY);

			CPropertyItemPtr itemZ = new CPropertyItem(m_propertyCtrl);
			itemZ->SetXmlNode(z);
			AddChild(itemZ);
		}
		else if (m_type == ePropertyVector2)
		{
			Vec2 vec;
			m_node->getAttr(VALUE_ATTR, vec);
			// Create 3 sub elements.
			XmlNodeRef x = m_node->createNode("X");
			XmlNodeRef y = m_node->createNode("Y");
			x->setAttr(TYPE_ATTR, "Float");
			y->setAttr(TYPE_ATTR, "Float");

			x->setAttr(VALUE_ATTR, vec.x);
			y->setAttr(VALUE_ATTR, vec.y);

			// Start ignoring all updates comming from childs. (Initializing childs).
			m_bIgnoreChildsUpdate = true;

			CPropertyItemPtr itemX = new CPropertyItem(m_propertyCtrl);
			itemX->SetXmlNode(x);
			AddChild(itemX);

			CPropertyItemPtr itemY = new CPropertyItem(m_propertyCtrl);
			itemY->SetXmlNode(y);
			AddChild(itemY);
		}
		else if (m_type == ePropertyVector4)
		{
			Vec4 vec;
			m_node->getAttr(VALUE_ATTR, vec);
			// Create 3 sub elements.
			XmlNodeRef x = m_node->createNode("X");
			XmlNodeRef y = m_node->createNode("Y");
			XmlNodeRef z = m_node->createNode("Z");
			XmlNodeRef w = m_node->createNode("W");
			x->setAttr(TYPE_ATTR, "Float");
			y->setAttr(TYPE_ATTR, "Float");
			z->setAttr(TYPE_ATTR, "Float");
			w->setAttr(TYPE_ATTR, "Float");

			x->setAttr(VALUE_ATTR, vec.x);
			y->setAttr(VALUE_ATTR, vec.y);
			z->setAttr(VALUE_ATTR, vec.z);
			w->setAttr(VALUE_ATTR, vec.w);

			// Start ignoring all updates comming from childs. (Initializing childs).
			m_bIgnoreChildsUpdate = true;

			CPropertyItemPtr itemX = new CPropertyItem(m_propertyCtrl);
			itemX->SetXmlNode(x);
			AddChild(itemX);

			CPropertyItemPtr itemY = new CPropertyItem(m_propertyCtrl);
			itemY->SetXmlNode(y);
			AddChild(itemY);

			CPropertyItemPtr itemZ = new CPropertyItem(m_propertyCtrl);
			itemZ->SetXmlNode(z);
			AddChild(itemZ);

			CPropertyItemPtr itemW = new CPropertyItem(m_propertyCtrl);
			itemW->SetXmlNode(w);
			AddChild(itemW);
		}
		m_bNoCategory = true;
		m_bExpandable = true;
		m_bIgnoreChildsUpdate = false;
	}
	else if (bRecursive)
	{
		// If recursive and not vector.

		m_bExpandable = false;
		// Create sub-nodes.
		for (int i = 0; i < m_node->getChildCount(); i++)
		{
			m_bIgnoreChildsUpdate = true;

			XmlNodeRef child = m_node->getChild(i);
			CPropertyItemPtr item = new CPropertyItem(m_propertyCtrl);
			item->SetXmlNode(m_node->getChild(i));
			AddChild(item);
			m_bExpandable = true;

			m_bIgnoreChildsUpdate = false;
		}
	}
	m_modified = false;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::SetVariable(IVariable* var)
{
	if (var == m_pVariable)
		return;

	_smart_ptr<IVariable> pInputVar = var;

	// Release previous variable.
	if (m_pVariable)
		ReleaseVariable();

	m_pVariable = pInputVar;
	assert(m_pVariable != NULL);

	m_pVariable->AddOnSetCallback(functor(*this, &CPropertyItem::OnVariableChange));
	m_pVariable->EnableNotifyWithoutValueChange(m_bForceModified);

	m_tip = "";
	m_name = m_pVariable->GetHumanName();

	int dataType = m_pVariable->GetDataType();

	m_image = -1;
	m_type = ePropertyInvalid;
	int i;

	if (dataType != IVariable::DT_SIMPLE)
	{
		for (i = 0; i < NumPropertyTypes; i++)
		{
			if (dataType == s_propertyTypeNames[i].dataType)
			{
				m_type = s_propertyTypeNames[i].type;
				m_image = s_propertyTypeNames[i].image;
				break;
			}
		}
	}

	m_enumList = m_pVariable->GetEnumList();
	if (m_enumList != NULL)
	{
		m_type = ePropertySelection;
	}

	if (m_type == ePropertyInvalid)
	{
		switch (m_pVariable->GetType())
		{
		case IVariable::INT:
			m_type = ePropertyInt;
			break;
		case IVariable::BOOL:
			m_type = ePropertyBool;
			break;
		case IVariable::FLOAT:
			m_type = ePropertyFloat;
			break;
		case IVariable::VECTOR2:
			m_type = ePropertyVector2;
			break;
		case IVariable::VECTOR4:
			m_type = ePropertyVector4;
			break;
		case IVariable::VECTOR:
			m_type = ePropertyVector;
			break;
		case IVariable::STRING:
			m_type = ePropertyString;
			break;
		}
		for (i = 0; i < NumPropertyTypes; i++)
		{
			if (m_type == s_propertyTypeNames[i].type)
			{
				m_image = s_propertyTypeNames[i].image;
				break;
			}
		}
	}

	m_valueMultiplier = 1;
	m_rangeMin = 0;
	m_rangeMax = 100;
	m_step = 0.f;
	m_bHardMin = m_bHardMax = false;

	// Get variable limits.
	m_pVariable->GetLimits(m_rangeMin, m_rangeMax, m_step, m_bHardMin, m_bHardMax);

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
		m_rangeMax = 360;
	}
	else if (dataType == IVariable::DT_UIENUM)
	{
		m_pEnumDBItem = GetIEditor()->GetUIEnumsDatabase()->FindEnum(m_name.GetString());
	}

	const bool useExplicitStep = (m_pVariable->GetFlags() & IVariable::UI_EXPLICIT_STEP);
	if (!useExplicitStep)
	{
		// Limit step size to 1000.
		int nPrec = max(3 - int(log(m_rangeMax - m_rangeMin) / log(10.f)), 0);
		m_step = max(m_step, powf(10.f, -nPrec));
	}

	//////////////////////////////////////////////////////////////////////////
	VarToValue();

	m_bNoCategory = false;

	switch (m_type)
	{
	case ePropertyAiPFPropertiesList:
		m_bEditChildren = true;
		AddChildrenForPFProperties();
		m_bNoCategory = true;
		m_bExpandable = true;
		break;

	case ePropertyAiEntityClasses:
		m_bEditChildren = true;
		AddChildrenForAIEntityClasses();
		m_bNoCategory = true;
		m_bExpandable = true;
		break;

	case ePropertyVector2:
		if (!m_propertyCtrl->IsExtenedUI())
		{
			m_bEditChildren = true;
			m_childs.clear();

			Vec2 vec;
			m_pVariable->Get(vec);
			IVariable* pVX = new CVariable<float>;
			pVX->SetName("x");
			pVX->SetLimits(m_rangeMin, m_rangeMax, m_step, m_bHardMin, m_bHardMax);
			pVX->Set(vec.x);
			IVariable* pVY = new CVariable<float>;
			pVY->SetName("y");
			pVY->SetLimits(m_rangeMin, m_rangeMax, m_step, m_bHardMin, m_bHardMax);
			pVY->Set(vec.y);

			// Start ignoring all updates coming from childs. (Initializing childs).
			m_bIgnoreChildsUpdate = true;

			CPropertyItemPtr itemX = new CPropertyItem(m_propertyCtrl);
			itemX->SetVariable(pVX);
			AddChild(itemX);

			CPropertyItemPtr itemY = new CPropertyItem(m_propertyCtrl);
			itemY->SetVariable(pVY);
			AddChild(itemY);

			m_bNoCategory = true;
			m_bExpandable = true;

			m_bIgnoreChildsUpdate = false;
		}
		break;

	case ePropertyVector4:
		if (!m_propertyCtrl->IsExtenedUI())
		{
			m_bEditChildren = true;
			m_childs.clear();

			Vec4 vec;
			m_pVariable->Get(vec);
			IVariable* pVX = new CVariable<float>;
			pVX->SetName("x");
			pVX->SetLimits(m_rangeMin, m_rangeMax, m_step, m_bHardMin, m_bHardMax);
			pVX->Set(vec.x);
			IVariable* pVY = new CVariable<float>;
			pVY->SetName("y");
			pVY->SetLimits(m_rangeMin, m_rangeMax, m_step, m_bHardMin, m_bHardMax);
			pVY->Set(vec.y);
			IVariable* pVZ = new CVariable<float>;
			pVZ->SetName("z");
			pVZ->SetLimits(m_rangeMin, m_rangeMax, m_step, m_bHardMin, m_bHardMax);
			pVZ->Set(vec.z);
			IVariable* pVW = new CVariable<float>;
			pVW->SetName("w");
			pVW->SetLimits(m_rangeMin, m_rangeMax, m_step, m_bHardMin, m_bHardMax);
			pVW->Set(vec.w);

			// Start ignoring all updates coming from childs. (Initializing childs).
			m_bIgnoreChildsUpdate = true;

			CPropertyItemPtr itemX = new CPropertyItem(m_propertyCtrl);
			itemX->SetVariable(pVX);
			AddChild(itemX);

			CPropertyItemPtr itemY = new CPropertyItem(m_propertyCtrl);
			itemY->SetVariable(pVY);
			AddChild(itemY);

			CPropertyItemPtr itemZ = new CPropertyItem(m_propertyCtrl);
			itemZ->SetVariable(pVZ);
			AddChild(itemZ);

			CPropertyItemPtr itemW = new CPropertyItem(m_propertyCtrl);
			itemZ->SetVariable(pVW);
			AddChild(itemW);

			m_bNoCategory = true;
			m_bExpandable = true;

			m_bIgnoreChildsUpdate = false;
		}

	case ePropertyVector:
		if (!m_propertyCtrl->IsExtenedUI())
		{
			m_bEditChildren = true;
			m_childs.clear();

			Vec3 vec;
			m_pVariable->Get(vec);
			IVariable* pVX = new CVariable<float>;
			pVX->SetName("x");
			pVX->SetLimits(m_rangeMin, m_rangeMax, m_step, m_bHardMin, m_bHardMax);
			pVX->Set(vec.x);
			IVariable* pVY = new CVariable<float>;
			pVY->SetName("y");
			pVY->SetLimits(m_rangeMin, m_rangeMax, m_step, m_bHardMin, m_bHardMax);
			pVY->Set(vec.y);
			IVariable* pVZ = new CVariable<float>;
			pVZ->SetName("z");
			pVZ->SetLimits(m_rangeMin, m_rangeMax, m_step, m_bHardMin, m_bHardMax);
			pVZ->Set(vec.z);

			// Start ignoring all updates coming from childs. (Initializing childs).
			m_bIgnoreChildsUpdate = true;

			CPropertyItemPtr itemX = new CPropertyItem(m_propertyCtrl);
			itemX->SetVariable(pVX);
			AddChild(itemX);

			CPropertyItemPtr itemY = new CPropertyItem(m_propertyCtrl);
			itemY->SetVariable(pVY);
			AddChild(itemY);

			CPropertyItemPtr itemZ = new CPropertyItem(m_propertyCtrl);
			itemZ->SetVariable(pVZ);
			AddChild(itemZ);

			m_bNoCategory = true;
			m_bExpandable = true;

			m_bIgnoreChildsUpdate = false;
		}
		break;
	}

	if (m_pVariable->GetNumVariables() > 0)
	{
		if (m_type == ePropertyInvalid)
			m_type = ePropertyTable;
		m_bExpandable = true;
		if (m_pVariable->GetFlags() & IVariable::UI_SHOW_CHILDREN)
			m_bShowChildren = true;
		m_bIgnoreChildsUpdate = true;
		for (i = 0; i < m_pVariable->GetNumVariables(); i++)
		{
			CPropertyItemPtr item = new CPropertyItem(m_propertyCtrl);
			item->SetVariable(m_pVariable->GetVariable(i));
			AddChild(item);
		}
		m_bIgnoreChildsUpdate = false;
	}

	m_modified = false;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::ReleaseVariable()
{
	if (m_pVariable)
	{
		// Unwire all from variable.
		m_pVariable->RemoveOnSetCallback(functor(*this, &CPropertyItem::OnVariableChange));
	}
	m_pVariable = 0;
}

//////////////////////////////////////////////////////////////////////////
//! Find item that reference specified property.
CPropertyItem* CPropertyItem::FindItemByVar(IVariable* pVar)
{
	if (m_pVariable == pVar)
		return this;
	for (int i = 0; i < m_childs.size(); i++)
	{
		CPropertyItem* pFound = m_childs[i]->FindItemByVar(pVar);
		if (pFound)
			return pFound;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
CString CPropertyItem::GetFullName() const
{
	if (m_parent)
	{
		return m_parent->GetFullName() + "::" + m_name;
	}
	else
		return m_name;
}

//////////////////////////////////////////////////////////////////////////
CPropertyItem* CPropertyItem::FindItemByFullName(const CString& name)
{
	if (GetFullName() == name)
		return this;
	for (int i = 0; i < m_childs.size(); i++)
	{
		CPropertyItem* pFound = m_childs[i]->FindItemByFullName(name);
		if (pFound)
			return pFound;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::AddChild(CPropertyItem* item)
{
	assert(item);
	m_bExpandable = true;
	item->m_parent = this;
	m_childs.push_back(item);
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::RemoveChild(CPropertyItem* item)
{
	// Find item and erase it from childs array.
	for (int i = 0; i < m_childs.size(); i++)
	{
		if (item == m_childs[i])
		{
			m_childs.erase(m_childs.begin() + i);
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::RemoveAllChildren()
{
	m_childs.clear();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnChildChanged(CPropertyItem* child)
{
	if (m_bIgnoreChildsUpdate)
		return;

	if (m_bEditChildren)
	{
		// Update parent.
		CString sValue;
		if ((m_type == ePropertyAiPFPropertiesList) || (m_type == ePropertyAiEntityClasses))
		{
			for (int i = 0; i < m_childs.size(); i++)
			{
				CString token;
				int index = 0;
				CString value = m_childs[i]->GetValue();
				while (!(token = value.Tokenize(" ,", index)).IsEmpty())
				{
					if (!sValue.IsEmpty())
						sValue += ", ";
					sValue += token;
				}
			}
		}
		else
		{
			for (int i = 0; i < m_childs.size(); i++)
			{
				if (i > 0)
					sValue += ", ";
				sValue += m_childs[i]->GetValue();
			}
		}
		SetValue(sValue, false);
	}
	if (m_bEditChildren || m_bShowChildren)
		SendToControl();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::SetSelected(bool selected)
{
	m_bSelected = selected;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::SetExpanded(bool expanded)
{
	if (IsDisabled())
	{
		m_bExpanded = false;
	}
	else
		m_bExpanded = expanded;
}

bool CPropertyItem::IsDisabled() const
{
	if (m_pVariable)
	{
		return m_pVariable->GetFlags() & IVariable::UI_DISABLED;
	}
	return false;
}

bool CPropertyItem::IsInvisible() const
{
	if (m_pVariable)
	{
		return m_pVariable->GetFlags() & IVariable::UI_INVISIBLE;
	}
	return false;
}

bool CPropertyItem::IsBold() const
{
	if (m_pVariable)
	{
		return m_pVariable->GetFlags() & IVariable::UI_BOLD;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::CreateInPlaceControl(CWnd* pWndParent, CRect& ctrlRect)
{
	if (IsDisabled())
		return;

	m_controls.clear();

	m_bMoveControls = true;
	CRect nullRc(0, 0, 0, 0);
	DWORD style;
	switch (m_type)
	{
	case ePropertyFloat:
	case ePropertyInt:
	case ePropertyAngle:
		{
			if (m_pEnumDBItem)
			{
				// Combo box.
				m_cCombo = new CInPlaceComboBox();
				m_cCombo->SetReadOnly(true);
				int unsorted = m_pEnumDBItem->strings.size() && !strcmp(m_pEnumDBItem->strings[0], "#unsorted");
				DWORD nSorting = (unsorted || m_pVariable && m_pVariable->GetFlags() & IVariable::UI_UNSORTED) ? 0 : LBS_SORT;
				m_cCombo->Create(NULL, "", WS_CHILD | WS_VISIBLE | nSorting, nullRc, pWndParent, 2);
				m_cCombo->SetUpdateCallback(functor(*this, &CPropertyItem::OnComboSelection));

				for (int i = unsorted; i < m_pEnumDBItem->strings.size(); i++)
					m_cCombo->AddString(m_pEnumDBItem->strings[i]);
			}
			else
			{
				m_cNumber = new CNumberCtrl;
				m_cNumber->SetUpdateCallback(functor(*this, &CPropertyItem::OnNumberCtrlUpdate));
				m_cNumber->EnableUndo(m_name + " Modified");
				m_cNumber->EnableNotifyWithoutValueChange(m_bForceModified);
				//m_cNumber->SetBeginUpdateCallback( functor(*this,OnNumberCtrlBeginUpdate) );
				//m_cNumber->SetEndUpdateCallback( functor(*this,OnNumberCtrlEndUpdate) );

				// (digits behind the comma, only used for floats)
				if (m_type == ePropertyFloat)
				{
					m_cNumber->SetStep(m_step);
					m_cNumber->SetFloatFormatPrecision(FLOAT_NUM_DIGITS);
				}
				else if (m_type == ePropertyAngle)
				{
					m_cNumber->SetFloatFormatPrecision(FLOAT_NUM_DIGITS);
				}

				// Only for integers.
				if (m_type == ePropertyInt)
					m_cNumber->SetInteger(true);
				m_cNumber->SetMultiplier(m_valueMultiplier);
				m_cNumber->SetRange(m_bHardMin ? m_rangeMin : -FLT_MAX, m_bHardMax ? m_rangeMax : FLT_MAX);

				m_cNumber->Create(pWndParent, nullRc, 1, CNumberCtrl::NOBORDER | CNumberCtrl::LEFTALIGN);
				m_cNumber->SetLeftAlign(true);

				if (m_bHardMin || m_bHardMax)
				{
					m_cFillSlider = new CFillSliderCtrl;
					m_cFillSlider->EnableUndo(m_name + " Modified");
					m_cFillSlider->Create(WS_VISIBLE | WS_CHILD, nullRc, pWndParent, 2);
					m_cFillSlider->SetUpdateCallback(functor(*this, &CPropertyItem::OnFillSliderCtrlUpdate));
					m_cFillSlider->SetRangeFloat(m_rangeMin / m_valueMultiplier, m_rangeMax / m_valueMultiplier, m_step / m_valueMultiplier);
				}
			}
		}
		break;

	case ePropertyTable:
		if (!m_bEditChildren)
			break;

	//These used to be a combo box but right now defaulting back to editbox
	case ePropertyAiTerritory:
	case ePropertyAiWave:

	case ePropertyString:
		if (m_pEnumDBItem)
		{
			// Combo box.
			m_cCombo = new CInPlaceComboBox();
			m_cCombo->SetReadOnly(true);
			DWORD nSorting = (m_pVariable && m_pVariable->GetFlags() & IVariable::UI_UNSORTED) ? 0 : LBS_SORT;
			m_cCombo->Create(NULL, "", WS_CHILD | WS_VISIBLE | nSorting, nullRc, pWndParent, 2);
			m_cCombo->SetUpdateCallback(functor(*this, &CPropertyItem::OnComboSelection));

			for (int i = 0; i < m_pEnumDBItem->strings.size(); i++)
				m_cCombo->AddString(m_pEnumDBItem->strings[i]);
			break;
		}
		else
		{
			// Here we must check the global table to see if this enum should be loaded from the XML.
			TDValues* pcValue = (m_pVariable) ? GetEnumValues(m_pVariable->GetName()) : NULL;

			// If there is not even this, we THEN fall back to create the edit box.
			if (pcValue)
			{
				m_cCombo = new CInPlaceComboBox();
				m_cCombo->SetReadOnly(true);
				DWORD nSorting = (m_pVariable && m_pVariable->GetFlags() & IVariable::UI_UNSORTED) ? 0 : LBS_SORT;
				m_cCombo->Create(NULL, "", WS_CHILD | WS_VISIBLE | nSorting, nullRc, pWndParent, 2);
				m_cCombo->SetUpdateCallback(functor(*this, &CPropertyItem::OnComboSelection));

				for (int i = 0; i < pcValue->size(); i++)
				{
					m_cCombo->AddString((*pcValue)[i]);
				}
				break;
			}
		}
	// ... else, fall through to create edit box.

	case ePropertyVector:
	case ePropertyVector2:
	case ePropertyVector4:
		m_cEdit = new CInPlaceEdit(m_value, functor(*this, &CPropertyItem::OnEditChanged));
		m_cEdit->Create(WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT, nullRc, pWndParent, 2);
		m_cEdit->EnableUpdateOnKillFocus(!m_bForceModified);
		break;

	case ePropertyMaterial:
	case ePropertyAiBehavior:
	case ePropertyAiAnchor:
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
	case ePropertyAiCharacter:
#endif
	case ePropertyAiPFPropertiesList:
	case ePropertyAiEntityClasses:
	case ePropertyEquip:
	case ePropertyReverbPreset:
	case ePropertySOClass:
	case ePropertySOClasses:
	case ePropertySOState:
	case ePropertySOStates:
	case ePropertySOStatePattern:
	case ePropertySOAction:
	case ePropertySOHelper:
	case ePropertySONavHelper:
	case ePropertySOAnimHelper:
	case ePropertySOEvent:
	case ePropertySOTemplate:
	case ePropertyCustomAction:
	case ePropertyGameToken:
	case ePropertyMissionObj:
	case ePropertySequence:
	case ePropertySequenceId:
	case ePropertyUser:
	case ePropertyLocalString:
	case ePropertyParticleName:
	case ePropertyFlare:
		m_cEdit = new CInPlaceEdit(m_value, functor(*this, &CPropertyItem::OnEditChanged));
		style = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT;
		if (m_type == ePropertySequenceId)
			style |= ES_READONLY;
		m_cEdit->Create(style, nullRc, pWndParent, 2);
		m_cEdit->EnableUpdateOnKillFocus(!m_bForceModified);
		if (m_type == ePropertyMaterial)
		{
			m_cButton = new CInPlaceButton(functor(*this, &CPropertyItem::OnMaterialBrowseButton));
			m_cButton->Create("...", WS_CHILD | WS_VISIBLE, nullRc, pWndParent, 4);

			m_cButton2 = new CInPlaceButton(functor(*this, &CPropertyItem::OnMaterialPickSelectedButton));
			m_cButton2->Create("<", WS_CHILD | WS_VISIBLE, nullRc, pWndParent, 5);
		}
		else if (m_type == ePropertySequence)
		{
			m_cButton = new CInPlaceButton(functor(*this, &CPropertyItem::OnSequenceBrowseButton));
			m_cButton->Create("...", WS_CHILD | WS_VISIBLE, nullRc, pWndParent, 4);
		}
		else if (m_type == ePropertySequenceId)
		{
			m_cButton = new CInPlaceButton(functor(*this, &CPropertyItem::OnSequenceIdBrowseButton));
			m_cButton->Create("...", WS_CHILD | WS_VISIBLE, nullRc, pWndParent, 4);
		}
		else if (m_type == ePropertyMissionObj)
		{
			m_cButton = new CInPlaceButton(functor(*this, &CPropertyItem::OnMissionObjButton));
			m_cButton->Create("...", WS_CHILD | WS_VISIBLE, nullRc, pWndParent, 4);
		}
		else if (m_type == ePropertyUser)
		{
			m_cButton = new CInPlaceButton(functor(*this, &CPropertyItem::OnUserBrowseButton));
			m_cButton->Create("...", WS_CHILD | WS_VISIBLE, nullRc, pWndParent, 4);
		}
		else if (m_type == ePropertyLocalString)
		{
			m_cButton = new CInPlaceButton(functor(*this, &CPropertyItem::OnLocalStringBrowseButton));
			m_cButton->Create("...", WS_CHILD | WS_VISIBLE, nullRc, pWndParent, 4);
		}
		else if (GetIEditor()->CanEditDeprecatedProperty(m_type))
		{
			//handle with deprecated picker registration system
			//this only works for button-style pickers, improve system if necessary
			m_cButton = new CInPlaceButton(functor(*this, &CPropertyItem::OnEditDeprecatedProperty));
			m_cButton->Create("...", WS_CHILD | WS_VISIBLE, nullRc, pWndParent, 4);
		}
		break;

	case ePropertySelection:
		{
			// Combo box.
			m_cCombo = new CInPlaceComboBox();
			m_cCombo->SetReadOnly(true);
			DWORD nSorting = (m_pVariable && m_pVariable->GetFlags() & IVariable::UI_UNSORTED) ? 0 : LBS_SORT;
			m_cCombo->Create(NULL, "", WS_CHILD | WS_VISIBLE | nSorting, nullRc, pWndParent, 2);
			m_cCombo->SetUpdateCallback(functor(*this, &CPropertyItem::OnComboSelection));

			if (m_enumList)
			{
				for (uint i = 0; cstr sEnumName = m_enumList->GetItemName(i); i++)
				{
					m_cCombo->AddString(sEnumName);
				}
			}
			else
				m_cCombo->SetReadOnly(false);
		}
		break;

	case ePropertyColor:
		{
			m_cEdit = new CInPlaceEdit(m_value, functor(*this, &CPropertyItem::OnEditChanged));
			m_cEdit->Create(WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT, nullRc, pWndParent, 2);
			m_cEdit->EnableUpdateOnKillFocus(!m_bForceModified);
			m_cColorButton = new CInPlaceColorButton(functor(*this, &CPropertyItem::OnColorBrowseButton));
			m_cColorButton->Create("", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW, nullRc, pWndParent, 4);
		}
		break;

	case ePropertyFloatCurve:
		{
			m_cSpline = new CSplineCtrl;
			m_cSpline->Create(WS_VISIBLE | WS_CHILD, nullRc, pWndParent, 2);
			m_cSpline->SetUpdateCallback((CSplineCtrl::UpdateCallback const&)functor(*this, &CPropertyItem::ReceiveFromControl));
			m_cSpline->SetTimeRange(0, 1);
			m_cSpline->SetValueRange(0, 1);
			m_cSpline->SetGrid(12, 12);
			m_cSpline->SetSpline(m_pVariable->GetSpline(true));
		}
		break;

	case ePropertyColorCurve:
		{
			m_cColorSpline = new CColorGradientCtrl;
			m_cColorSpline->Create(WS_VISIBLE | WS_CHILD, nullRc, pWndParent, 2);
			m_cColorSpline->SetUpdateCallback((CColorGradientCtrl::UpdateCallback const&)functor(*this, &CPropertyItem::ReceiveFromControl));
			m_cColorSpline->SetTimeRange(0, 1);
			m_cColorSpline->SetSpline(m_pVariable->GetSpline(true));
		}
		break;

	case ePropertyTexture:
		{
			m_cEdit = new CInPlaceEdit(m_value, functor(*this, &CPropertyItem::OnEditChanged));
			m_cEdit->Create(WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT, nullRc, pWndParent, 2);
			m_cEdit->EnableUpdateOnKillFocus(!m_bForceModified);

			HICON hButtonIcon(NULL);

			hButtonIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_FILE_BROWSE), IMAGE_ICON, 16, 15, LR_DEFAULTCOLOR | LR_SHARED);
			m_cButton3 = new CInPlaceButton(functor(*this, &CPropertyItem::OnFileBrowseButton));
			m_cButton3->Create("...", WS_CHILD | WS_VISIBLE, nullRc, pWndParent, 4);
			m_cButton3->SetIcon(CSize(16, 15), hButtonIcon);
			m_cButton3->SetBorderGap(6);
		}
		break;

	case ePropertyAnimation:
	case ePropertyModel:
	case ePropertyGeomCache:
	case ePropertyAudioTrigger:
	case ePropertyAudioSwitch:
	case ePropertyAudioSwitchState:
	case ePropertyAudioRTPC:
	case ePropertyAudioEnvironment:
	case ePropertyAudioPreloadRequest:
	case ePropertyDynamicResponseSignal:
		{
			m_cEdit = new CInPlaceEdit(m_value, functor(*this, &CPropertyItem::OnEditChanged));
			m_cEdit->Create(WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT, nullRc, pWndParent, 2);
			m_cEdit->EnableUpdateOnKillFocus(!m_bForceModified);

			m_cButton = new CInPlaceButton(functor(*this, &CPropertyItem::OnResourceSelectorButton));
			m_cButton->Create("...", WS_CHILD | WS_VISIBLE, nullRc, pWndParent, 4);

			// Use file browse icon.
			HICON hOpenIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_FILE_BROWSE), IMAGE_ICON, 16, 15, LR_DEFAULTCOLOR | LR_SHARED);
			m_cButton->SetIcon(CSize(16, 15), hOpenIcon);
			m_cButton->SetBorderGap(0);
		}
		break;
	case ePropertyFile:
		m_cEdit = new CInPlaceEdit(m_value, functor(*this, &CPropertyItem::OnEditChanged));
		m_cEdit->Create(WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT, nullRc, pWndParent, 2);
		m_cEdit->EnableUpdateOnKillFocus(!m_bForceModified);

		m_cButton = new CInPlaceButton(functor(*this, &CPropertyItem::OnFileBrowseButton));
		m_cButton->Create("...", WS_CHILD | WS_VISIBLE, nullRc, pWndParent, 4);

		// Use file browse icon.
		HICON hOpenIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_FILE_BROWSE), IMAGE_ICON, 16, 15, LR_DEFAULTCOLOR | LR_SHARED);
		//m_cButton->SetIcon( CSize(16,15),IDI_FILE_BROWSE );
		m_cButton->SetIcon(CSize(16, 15), hOpenIcon);
		m_cButton->SetBorderGap(0);
		break;
		/*
		    case ePropertyList:
		      {
		        AddButton( "Add", CWDelegate(this,(TDelegate)OnAddItemButton) );
		      }
		      break;
		 */
	}

	MoveInPlaceControl(ctrlRect);
	SendToControl();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::CreateControls(CWnd* pWndParent, CRect& textRect, CRect& ctrlRect)
{
	m_pControlsHostWnd = pWndParent;
	m_rcText = textRect;
	m_rcControl = ctrlRect;

	m_bMoveControls = false;
	CRect nullRc(0, 0, 0, 0);
	DWORD style;

	if (IsExpandable() && !m_propertyCtrl->IsCategory(this))
	{
		m_cExpandButton = new CInPlaceButton(functor(*this, &CPropertyItem::OnExpandButton));
		m_cExpandButton->Create("", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(textRect.left, textRect.top + 2, textRect.left + 12, textRect.top + 14), pWndParent, 5);
		m_cExpandButton->SetFlatStyle(TRUE);
		if (!IsExpanded())
			m_cExpandButton->SetIcon(CSize(8, 8), IDI_EXPAND1);
		else
			m_cExpandButton->SetIcon(CSize(8, 8), IDI_EXPAND2);
		textRect.left += 12;
		RegisterCtrl(m_cExpandButton);
	}

	// Create static text. Update: put text on left of Bools as well, for UI consistency.
	// if (m_type != ePropertyBool)
	{
		m_pStaticText = new CColorCtrl<CStatic>;
		CString text = GetName();
		if (m_type != ePropertyTable)
		{
			text += " . . . . . . . . . . . . . . . . . . . . . . . . . .";
		}
		m_pStaticText->Create(text, WS_CHILD | WS_VISIBLE | SS_LEFT, textRect, pWndParent);
		m_pStaticText->SetFont(CFont::FromHandle((HFONT)gMFCFonts.hSystemFont));
		RegisterCtrl(m_pStaticText);
	}

	switch (m_type)
	{
	case ePropertyBool:
		{
			m_cCheckBox = new CInPlaceCheckBox(functor(*this, &CPropertyItem::OnCheckBoxButton));
			m_cCheckBox->Create("", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, ctrlRect, pWndParent, 0);
			m_cCheckBox->SetFont(m_propertyCtrl->GetFont());
			RegisterCtrl(m_cCheckBox);
		}
		break;
	case ePropertyFloat:
	case ePropertyInt:
	case ePropertyAngle:
		{
			m_cNumber = new CNumberCtrl;
			m_cNumber->SetUpdateCallback(functor(*this, &CPropertyItem::OnNumberCtrlUpdate));
			m_cNumber->EnableUndo(m_name + " Modified");
			RegisterCtrl(m_cNumber);

			// (digits behind the comma, only used for floats)
			if (m_type == ePropertyFloat)
				m_cNumber->SetStep(m_step);

			// Only for integers.
			if (m_type == ePropertyInt)
				m_cNumber->SetInteger(true);
			m_cNumber->SetMultiplier(m_valueMultiplier);
			m_cNumber->SetRange(m_bHardMin ? m_rangeMin : -FLT_MAX, m_bHardMax ? m_rangeMax : FLT_MAX);

			m_cNumber->Create(pWndParent, nullRc, 1);
			m_cNumber->SetLeftAlign(true);

			/*
			   CSliderCtrlEx *pSlider = new CSliderCtrlEx;
			   //pSlider->EnableUndo( m_name + " Modified" );
			   CRect rcSlider = ctrlRect;
			   rcSlider.left += NUMBER_CTRL_WIDTH;
			   pSlider->Create( WS_VISIBLE|WS_CHILD,rcSlider,pWndParent,2);
			 */
			if (m_bHardMin || m_bHardMax)
			{
				m_cFillSlider = new CFillSliderCtrl;
				m_cFillSlider->SetFilledLook(true);
				//m_cFillSlider->SetFillStyle(CFillSliderCtrl::eFillStyle_Background);
				m_cFillSlider->EnableUndo(m_name + " Modified");
				m_cFillSlider->Create(WS_VISIBLE | WS_CHILD, nullRc, pWndParent, 2);
				m_cFillSlider->SetUpdateCallback(functor(*this, &CPropertyItem::OnFillSliderCtrlUpdate));
				m_cFillSlider->SetRangeFloat(m_rangeMin / m_valueMultiplier, m_rangeMax / m_valueMultiplier, m_step / m_valueMultiplier);
				RegisterCtrl(m_cFillSlider);
			}
		}
		break;

	case ePropertyVector:
	case ePropertyVector2:
	case ePropertyVector4:
		{
			int numberOfElements = 3;
			if (m_type == ePropertyVector2)
				numberOfElements = 2;
			else if (m_type == ePropertyVector4)
				numberOfElements = 4;

			CNumberCtrl** cNumber[4] = { &m_cNumber, &m_cNumber1, &m_cNumber2, &m_cNumber3 };

			for (int a = 0; a < numberOfElements; a++)
			{
				CNumberCtrl* pNumber = *cNumber[a] = new CNumberCtrl;
				pNumber->Create(pWndParent, nullRc, 1);
				pNumber->SetLeftAlign(true);
				pNumber->SetUpdateCallback(functor(*this, &CPropertyItem::OnNumberCtrlUpdate));
				pNumber->EnableUndo(m_name + " Modified");
				pNumber->SetMultiplier(m_valueMultiplier);
				pNumber->SetRange(m_bHardMin ? m_rangeMin : -FLT_MAX, m_bHardMax ? m_rangeMax : FLT_MAX);
				RegisterCtrl(pNumber);
			}
		}
		break;

	case ePropertyTable:
		if (!m_bEditChildren)
			break;
	//These used to be a combo box but right now defaulting back to editbox
	case ePropertyAiTerritory:
	case ePropertyAiWave:

	case ePropertyString:
		//case ePropertyVector:

		m_cEdit = new CInPlaceEdit(m_value, functor(*this, &CPropertyItem::OnEditChanged));
		m_cEdit->Create(WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT | WS_TABSTOP, nullRc, pWndParent, 2);
		m_cEdit->EnableUpdateOnKillFocus(!m_bForceModified);
		RegisterCtrl(m_cEdit);
		break;

	case ePropertyMaterial:
	case ePropertyAiBehavior:
	case ePropertyAiAnchor:
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
	case ePropertyAiCharacter:
#endif
	case ePropertyAiPFPropertiesList:
	case ePropertyAiEntityClasses:
	case ePropertyEquip:
	case ePropertyReverbPreset:
	case ePropertySOClass:
	case ePropertySOClasses:
	case ePropertySOState:
	case ePropertySOStates:
	case ePropertySOStatePattern:
	case ePropertySOAction:
	case ePropertySOHelper:
	case ePropertySONavHelper:
	case ePropertySOAnimHelper:
	case ePropertyCustomAction:
	case ePropertyGameToken:
	case ePropertySequence:
	case ePropertySequenceId:
	case ePropertyUser:
	case ePropertyLocalString:
		m_cEdit = new CInPlaceEdit(m_value, functor(*this, &CPropertyItem::OnEditChanged));
		style = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT | WS_TABSTOP;
		if (m_type == ePropertySequenceId)
			style |= ES_READONLY;
		m_cEdit->Create(style, nullRc, pWndParent, 2);
		m_cEdit->EnableUpdateOnKillFocus(!m_bForceModified);
		RegisterCtrl(m_cEdit);

		if (m_type == ePropertyMaterial)
		{
			m_cButton = new CInPlaceButton(functor(*this, &CPropertyItem::OnMaterialBrowseButton));
			m_cButton->Create("...", WS_CHILD | WS_VISIBLE, nullRc, pWndParent, 4);

			m_cButton2 = new CInPlaceButton(functor(*this, &CPropertyItem::OnMaterialPickSelectedButton));
			m_cButton2->Create("<", WS_CHILD | WS_VISIBLE, nullRc, pWndParent, 5);
		}
		else if (m_type == ePropertySequence)
		{
			m_cButton = new CInPlaceButton(functor(*this, &CPropertyItem::OnSequenceBrowseButton));
			m_cButton->Create("...", WS_CHILD | WS_VISIBLE, nullRc, pWndParent, 4);
		}
		else if (m_type == ePropertySequenceId)
		{
			m_cButton = new CInPlaceButton(functor(*this, &CPropertyItem::OnSequenceIdBrowseButton));
			m_cButton->Create("...", WS_CHILD | WS_VISIBLE, nullRc, pWndParent, 4);
		}
		else if (m_type == ePropertyUser)
		{
			m_cButton = new CInPlaceButton(functor(*this, &CPropertyItem::OnUserBrowseButton));
			m_cButton->Create("...", WS_CHILD | WS_VISIBLE, nullRc, pWndParent, 4);
		}
		else if (m_type == ePropertyLocalString)
		{
			m_cButton = new CInPlaceButton(functor(*this, &CPropertyItem::OnLocalStringBrowseButton));
			m_cButton->Create("...", WS_CHILD | WS_VISIBLE, nullRc, pWndParent, 4);
		}
		else if (GetIEditor()->CanEditDeprecatedProperty(m_type))
		{
			//handle with deprecated picker registration system
			//this only works for button-style pickers, improve system if necessary
			m_cButton = new CInPlaceButton(functor(*this, &CPropertyItem::OnEditDeprecatedProperty));
			m_cButton->Create("...", WS_CHILD | WS_VISIBLE, nullRc, pWndParent, 4);
		}

		if (m_cButton)
		{
			RegisterCtrl(m_cButton);
		}
		if (m_cButton2)
		{
			RegisterCtrl(m_cButton2);
		}
		break;

	case ePropertySelection:
		{

			// Combo box.
			m_cCombo = new CInPlaceComboBox();
			m_cCombo->SetReadOnly(true);
			DWORD nSorting = (m_pVariable && m_pVariable->GetFlags() & IVariable::UI_UNSORTED) ? 0 : LBS_SORT;
			m_cCombo->Create(NULL, "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | nSorting, nullRc, pWndParent, 2);
			m_cCombo->SetUpdateCallback(functor(*this, &CPropertyItem::OnComboSelection));
			RegisterCtrl(m_cCombo);

			if (m_enumList)
			{
				for (uint i = 0; cstr sEnumName = m_enumList->GetItemName(i); i++)
				{
					m_cCombo->AddString(sEnumName);
				}
			}
			else
				m_cCombo->SetReadOnly(false);
		}
		break;

	case ePropertyColor:
		{
			m_cEdit = new CInPlaceEdit(m_value, functor(*this, &CPropertyItem::OnEditChanged));
			m_cEdit->Create(WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT | WS_TABSTOP, nullRc, pWndParent, 2);
			m_cEdit->EnableUpdateOnKillFocus(!m_bForceModified);
			RegisterCtrl(m_cEdit);

			//m_cButton = new CInPlaceButton( functor(*this,OnColorBrowseButton) );
			//m_cButton->Create( "",WS_CHILD|WS_VISIBLE,nullRc,pWndParent,4 );
			m_cColorButton = new CInPlaceColorButton(functor(*this, &CPropertyItem::OnColorBrowseButton));
			m_cColorButton->Create("", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW, nullRc, pWndParent, 4);
			RegisterCtrl(m_cColorButton);
		}
		break;

	case ePropertyFloatCurve:
		{
			m_cSpline = new CSplineCtrl;
			m_cSpline->Create(WS_VISIBLE | WS_CHILD, nullRc, pWndParent, 2);
			m_cSpline->SetUpdateCallback((CSplineCtrl::UpdateCallback const&)functor(*this, &CPropertyItem::ReceiveFromControl));
			m_cSpline->SetTimeRange(0, 1);
			m_cSpline->SetValueRange(0, 1);
			m_cSpline->SetGrid(12, 12);
			m_cSpline->SetSpline(m_pVariable->GetSpline(true));
			RegisterCtrl(m_cSpline);
		}
		break;

	case ePropertyColorCurve:
		{
			m_cColorSpline = new CColorGradientCtrl;
			m_cColorSpline->Create(WS_VISIBLE | WS_CHILD, nullRc, pWndParent, 2);
			m_cColorSpline->SetUpdateCallback((CColorGradientCtrl::UpdateCallback const&)functor(*this, &CPropertyItem::ReceiveFromControl));
			m_cColorSpline->SetTimeRange(0, 1);
			m_cColorSpline->SetSpline(m_pVariable->GetSpline(true));
			RegisterCtrl(m_cColorSpline);
		}
		break;

	case ePropertyTexture:
		{
			m_cEdit = new CInPlaceEdit(m_value, functor(*this, &CPropertyItem::OnEditChanged));
			m_cEdit->Create(WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT | WS_TABSTOP, nullRc, pWndParent, 2);
			m_cEdit->EnableUpdateOnKillFocus(!m_bForceModified);
			RegisterCtrl(m_cEdit);

			HICON hButtonIcon(NULL);

			hButtonIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_FILE_BROWSE), IMAGE_ICON, 16, 15, LR_DEFAULTCOLOR | LR_SHARED);
			m_cButton3 = new CInPlaceButton(functor(*this, &CPropertyItem::OnFileBrowseButton));
			m_cButton3->Create("...", WS_CHILD | WS_VISIBLE, nullRc, pWndParent, 4);
			RegisterCtrl(m_cButton3);
			//m_cButton3->SetIcon( CSize(16,15),hButtonIcon);
			//m_cButton3->SetBorderGap(6);

			hButtonIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_TIFF), IMAGE_ICON, 16, 15, LR_DEFAULTCOLOR | LR_SHARED);
			m_cButton4 = new CInPlaceButton(functor(*this, &CPropertyItem::OnTextureEditButton));
			m_cButton4->Create("", WS_CHILD | WS_VISIBLE, nullRc, pWndParent, 7); // edit selected texture semantics.
			m_cButton4->SetIcon(CSize(16, 15), hButtonIcon);
			m_cButton4->SetBorderGap(6);
			RegisterCtrl(m_cButton4);

			hButtonIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_PSD), IMAGE_ICON, 16, 15, LR_DEFAULTCOLOR | LR_SHARED);
			m_cButton5 = new CInPlaceButton(functor(*this, &CPropertyItem::OnPsdEditButton));
			m_cButton5->Create("", WS_CHILD | WS_VISIBLE, nullRc, pWndParent, 8); // edit selected texture semantics.
			m_cButton5->SetIcon(CSize(16, 15), hButtonIcon);
			m_cButton5->SetBorderGap(6);
			RegisterCtrl(m_cButton5);
		}
		break;

	case ePropertyAnimation:
	case ePropertyModel:
	case ePropertyAudioTrigger:
	case ePropertyAudioSwitch:
	case ePropertyAudioSwitchState:
	case ePropertyAudioRTPC:
	case ePropertyAudioEnvironment:
	case ePropertyAudioPreloadRequest:
	case ePropertyDynamicResponseSignal:
		{
			m_cEdit = new CInPlaceEdit(m_value, functor(*this, &CPropertyItem::OnEditChanged));
			m_cEdit->Create(WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT | WS_TABSTOP, nullRc, pWndParent, 2);
			m_cEdit->EnableUpdateOnKillFocus(!m_bForceModified);
			RegisterCtrl(m_cEdit);

			m_cButton = new CInPlaceButton(functor(*this, &CPropertyItem::OnResourceSelectorButton));
			m_cButton->Create("", WS_CHILD | WS_VISIBLE, nullRc, pWndParent, 4);
			RegisterCtrl(m_cButton);

			HICON hOpenIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_FILE_BROWSE), IMAGE_ICON, 16, 15, LR_DEFAULTCOLOR | LR_SHARED);
			m_cButton->SetIcon(CSize(16, 15), hOpenIcon);
			m_cButton->SetBorderGap(6);
		}
		break;
	case ePropertyFile:
		m_cEdit = new CInPlaceEdit(m_value, functor(*this, &CPropertyItem::OnEditChanged));
		m_cEdit->Create(WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT | WS_TABSTOP, nullRc, pWndParent, 2);
		m_cEdit->EnableUpdateOnKillFocus(!m_bForceModified);
		RegisterCtrl(m_cEdit);

		m_cButton = new CInPlaceButton(functor(*this, &CPropertyItem::OnFileBrowseButton));
		m_cButton->Create("", WS_CHILD | WS_VISIBLE, nullRc, pWndParent, 4);
		RegisterCtrl(m_cButton);

		// Use file browse icon.
		HICON hOpenIcon = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_FILE_BROWSE), IMAGE_ICON, 16, 15, LR_DEFAULTCOLOR | LR_SHARED);
		m_cButton->SetIcon(CSize(16, 15), hOpenIcon);
		m_cButton->SetBorderGap(6);
		break;
	}

	if (m_cEdit)
	{
		m_cEdit->ModifyStyleEx(0, WS_EX_CLIENTEDGE);
	}
	if (m_cCombo)
	{
		//m_cCombo->ModifyStyleEx( 0,WS_EX_CLIENTEDGE );
	}

	MoveInPlaceControl(ctrlRect);
	SendToControl();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::DestroyInPlaceControl(bool bRecursive)
{
	if (!m_propertyCtrl->IsExtenedUI() && !m_bForceModified)
		ReceiveFromControl();

	SAFE_DELETE(m_pStaticText);
	SAFE_DELETE(m_cNumber);
	SAFE_DELETE(m_cNumber1);
	SAFE_DELETE(m_cNumber2);
	SAFE_DELETE(m_cNumber3);
	SAFE_DELETE(m_cFillSlider);
	SAFE_DELETE(m_cSpline);
	SAFE_DELETE(m_cColorSpline);
	SAFE_DELETE(m_cEdit);
	SAFE_DELETE(m_cCombo);
	SAFE_DELETE(m_cButton);
	SAFE_DELETE(m_cButton2);
	SAFE_DELETE(m_cButton3);
	SAFE_DELETE(m_cButton4);
	SAFE_DELETE(m_cButton5);
	SAFE_DELETE(m_cExpandButton);
	SAFE_DELETE(m_cCheckBox);
	SAFE_DELETE(m_cColorButton);

	if (bRecursive)
	{
		int num = GetChildCount();
		for (int i = 0; i < num; i++)
		{
			GetChild(i)->DestroyInPlaceControl(bRecursive);
		}
		m_controls.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::MoveInPlaceControl(const CRect& rect)
{
	m_rcControl = rect;

	int nButtonWidth = BUTTON_WIDTH;
	if (m_propertyCtrl->IsExtenedUI())
		nButtonWidth += 10;

	CRect rc = rect;

	if (m_cButton)
	{
		if (m_type == ePropertyColor)
		{
			rc.right = rc.left + nButtonWidth + 2;
			RepositionWindow(m_cButton, rc);
			rc = rect;
			rc.left += nButtonWidth + 2 + 4;
			if (rc.right > rc.left - 100)
				rc.right = rc.left + 100;
		}
		else
		{
			rc.left = rc.right - nButtonWidth;
			RepositionWindow(m_cButton, rc);
			rc = rect;
			rc.right -= nButtonWidth;
		}
	}
	if (m_cColorButton)
	{
		rc.right = rc.left + nButtonWidth + 2;
		RepositionWindow(m_cColorButton, rc);
		rc = rect;
		rc.left += nButtonWidth + 2 + 4;
		if (rc.right > rc.left - 80)
			rc.right = rc.left + 80;
	}
	if (m_cButton4)
	{
		CRect brc(rc);
		brc.left = brc.right - nButtonWidth - 2;
		m_cButton4->MoveWindow(brc, FALSE);
		rc.right -= nButtonWidth + 4;
		m_cButton4->SetFont(m_propertyCtrl->GetFont());
	}
	if (m_cButton5)
	{
		CRect brc(rc);
		brc.left = brc.right - nButtonWidth - 2;
		m_cButton5->MoveWindow(brc, FALSE);
		rc.right -= nButtonWidth + 4;
		m_cButton5->SetFont(m_propertyCtrl->GetFont());
	}
	if (m_cButton2)
	{
		CRect brc(rc);
		brc.left = brc.right - nButtonWidth - 2;
		RepositionWindow(m_cButton2, brc);
		rc.right -= nButtonWidth + 4;
	}
	if (m_cButton3)
	{
		CRect brc(rc);
		brc.left = brc.right - nButtonWidth - 2;
		RepositionWindow(m_cButton3, brc);
		rc.right -= nButtonWidth + 4;
	}

	if (m_cNumber)
	{
		CRect rcn = rc;
		if (m_cNumber1 && m_cNumber2 && m_cNumber3)
		{
			int x = NUMBER_CTRL_WIDTH + 4;
			CRect rc0, rc1, rc2, rc3;
			rc0 = CRect(rc.left, rc.top, rc.left + NUMBER_CTRL_WIDTH, rc.bottom);
			rc1 = CRect(rc.left + x, rc.top, rc.left + x + NUMBER_CTRL_WIDTH, rc.bottom);
			rc2 = CRect(rc.left + x * 2, rc.top, rc.left + x * 2 + NUMBER_CTRL_WIDTH, rc.bottom);
			rc3 = CRect(rc.left + x * 3, rc.top, rc.left + x * 3 + NUMBER_CTRL_WIDTH, rc.bottom);
			RepositionWindow(m_cNumber, rc0);
			RepositionWindow(m_cNumber1, rc1);
			RepositionWindow(m_cNumber2, rc2);
			RepositionWindow(m_cNumber3, rc3);
		}
		else if (m_cNumber1 && m_cNumber2)
		{
			int x = NUMBER_CTRL_WIDTH + 4;
			CRect rc0, rc1, rc2;
			rc0 = CRect(rc.left, rc.top, rc.left + NUMBER_CTRL_WIDTH, rc.bottom);
			rc1 = CRect(rc.left + x, rc.top, rc.left + x + NUMBER_CTRL_WIDTH, rc.bottom);
			rc2 = CRect(rc.left + x * 2, rc.top, rc.left + x * 2 + NUMBER_CTRL_WIDTH, rc.bottom);
			RepositionWindow(m_cNumber, rc0);
			RepositionWindow(m_cNumber1, rc1);
			RepositionWindow(m_cNumber2, rc2);
		}
		else if (m_cNumber1)
		{
			int x = NUMBER_CTRL_WIDTH + 4;
			CRect rc0, rc1;
			rc0 = CRect(rc.left, rc.top, rc.left + NUMBER_CTRL_WIDTH, rc.bottom);
			rc1 = CRect(rc.left + x, rc.top, rc.left + x + NUMBER_CTRL_WIDTH, rc.bottom);
			RepositionWindow(m_cNumber, rc0);
			RepositionWindow(m_cNumber1, rc1);
		}
		else
		{
			//rcn.right = rc.left + NUMBER_CTRL_WIDTH;
			if (rcn.Width() > NUMBER_CTRL_WIDTH)
				rcn.right = rc.left + NUMBER_CTRL_WIDTH;
			RepositionWindow(m_cNumber, rcn);
		}
		if (m_cFillSlider)
		{
			CRect rcSlider = rc;
			rcSlider.left = rcn.right + 1;
			RepositionWindow(m_cFillSlider, rcSlider);
		}
	}
	if (m_cEdit)
	{
		RepositionWindow(m_cEdit, rc);
	}
	if (m_cCombo)
	{
		RepositionWindow(m_cCombo, rc);
	}
	if (m_cSpline)
	{
		// Grow beyond current line.
		rc.bottom = rc.top + 48;
		rc.right -= 4;
		RepositionWindow(m_cSpline, rc);
	}
	if (m_cColorSpline)
	{
		// Grow beyond current line.
		rc.bottom = rc.top + 32;
		rc.right -= 4;
		RepositionWindow(m_cColorSpline, rc);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::SetFocus()
{
	if (m_cNumber)
	{
		m_cNumber->SetFocus();
	}
	if (m_cEdit)
	{
		m_cEdit->SetFocus();
	}
	if (m_cCombo)
	{
		m_cCombo->SetFocus();
	}
	if (!m_cNumber && !m_cEdit && !m_cCombo)
		m_propertyCtrl->SetFocus();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::ReceiveFromControl()
{
	if (IsDisabled())
		return;
	if (m_cEdit)
	{
		CString str;
		m_cEdit->GetWindowText(str);
		SetValue(str);
	}
	if (m_cCombo)
	{
		if (!CUndo::IsRecording())
		{
			if (m_pEnumDBItem)
				SetValue(m_pEnumDBItem->NameToValue(m_cCombo->GetSelectedString()));
			else
				SetValue(m_cCombo->GetSelectedString());
		}
		else
		{
			if (m_pEnumDBItem)
				SetValue(m_pEnumDBItem->NameToValue(m_cCombo->GetSelectedString()));
			else
				SetValue(m_cCombo->GetSelectedString());
		}
	}
	if (m_cNumber)
	{
		if (m_cNumber1 && m_cNumber2 && m_cNumber3)
		{
			CString val;
			val.Format("%g,%g,%g,%g", m_cNumber->GetValue(), m_cNumber1->GetValue(), m_cNumber2->GetValue(), m_cNumber3->GetValue());
			SetValue(val);
		}
		if (m_cNumber1 && m_cNumber2)
		{
			CString val;
			val.Format("%g,%g,%g", m_cNumber->GetValue(), m_cNumber1->GetValue(), m_cNumber2->GetValue());
			SetValue(val);
		}
		else if (m_cNumber1)
		{
			CString val;
			val.Format("%g,%g", m_cNumber->GetValue(), m_cNumber1->GetValue());
			SetValue(val);
		}
		else
		{
			SetValue(m_cNumber->GetValueAsString());
		}
	}
	if (m_cSpline != 0 || m_cColorSpline != 0)
	{
		// Variable was already directly updated.
		//OnVariableChange(m_pVariable);
		m_modified = true;
		m_pVariable->Get(m_value);
		m_propertyCtrl->OnItemChange(this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::SendToControl()
{
	bool bInPlaceCtrl = m_propertyCtrl->IsExtenedUI();
	if (m_cButton)
	{
		if (m_type == ePropertyColor)
		{
			COLORREF clr = StringToColor(m_value);
			m_cButton->SetColorFace(clr);
			m_cButton->RedrawWindow();
			bInPlaceCtrl = true;
		}
		m_cButton->Invalidate();
	}
	if (m_cColorButton)
	{
		COLORREF clr = StringToColor(m_value);
		m_cColorButton->SetColor(clr);
		m_cColorButton->Invalidate();
		bInPlaceCtrl = true;
	}
	if (m_cCheckBox)
	{
		m_cCheckBox->SetChecked(GetBoolValue() ? BST_CHECKED : BST_UNCHECKED);
		m_cCheckBox->Invalidate();
	}
	if (m_cEdit)
	{
		m_cEdit->SetText(m_value);
		bInPlaceCtrl = true;
		m_cEdit->Invalidate();
	}
	if (m_cCombo)
	{
		if (m_type == ePropertyBool)
			m_cCombo->SetCurSel(GetBoolValue() ? 0 : 1);
		else
		{
			if (m_pEnumDBItem)
				m_cCombo->SelectString(-1, m_pEnumDBItem->ValueToName(m_value));
			else
				m_cCombo->SelectString(-1, m_value);
		}
		bInPlaceCtrl = true;
		m_cCombo->Invalidate();
	}
	if (m_cNumber)
	{
		if (m_cNumber1 && m_cNumber2 && m_cNumber3)
		{
			float x, y, z, w;
			sscanf(m_value, "%f,%f,%f,%f", &x, &y, &z, &w);
			m_cNumber->SetValue(x);
			m_cNumber1->SetValue(y);
			m_cNumber2->SetValue(z);
			m_cNumber3->SetValue(w);
		}
		else if (m_cNumber1 && m_cNumber2)
		{
			float x, y, z;
			sscanf(m_value, "%f,%f,%f", &x, &y, &z);
			m_cNumber->SetValue(x);
			m_cNumber1->SetValue(y);
			m_cNumber2->SetValue(z);
		}
		else if (m_cNumber1)
		{
			float x, y;
			sscanf(m_value, "%f,%f", &x, &y);
			m_cNumber->SetValue(x);
			m_cNumber1->SetValue(y);
		}
		else
		{
			m_cNumber->SetValue(atof(m_value));
		}
		bInPlaceCtrl = true;
		m_cNumber->Invalidate();
	}
	if (m_cFillSlider)
	{
		m_cFillSlider->SetValue(atof(m_value));
		bInPlaceCtrl = true;
		m_cFillSlider->Invalidate();
	}
	if (!bInPlaceCtrl)
	{
		CRect rc;
		m_propertyCtrl->GetItemRect(this, rc);
		m_propertyCtrl->InvalidateRect(rc, TRUE);

		CWnd* pFocusWindow = CWnd::GetFocus();
		if (pFocusWindow && m_propertyCtrl->IsChild(pFocusWindow))
			m_propertyCtrl->SetFocus();
	}
	if (m_cSpline)
	{
		m_cSpline->SetSpline(m_pVariable->GetSpline(true), TRUE);
	}
	if (m_cColorSpline)
	{
		m_cColorSpline->SetSpline(m_pVariable->GetSpline(true), TRUE);
	}
	if (m_pVariable && m_pVariable->GetFlags() & IVariable::UI_HIGHLIGHT_EDITED)
		CheckControlActiveColor();
}

//////////////////////////////////////////////////////////////////////////
bool CPropertyItem::HasDefaultValue(bool bChildren) const
{
	if (m_pVariable && !m_pVariable->HasDefaultValue())
		return false;

	if (bChildren)
	{
		// Evaluate children.
		for (int i = 0; i < m_childs.size(); i++)
		{
			if (!m_childs[i]->HasDefaultValue(true))
				return false;
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::CheckControlActiveColor()
{
	if (m_pStaticText)
	{
		COLORREF clrStaticText = XTPColorManager()->LightColor(GetXtremeColor(COLOR_3DFACE), GetXtremeColor(COLOR_BTNTEXT), 30);
		COLORREF nTextColor = HasDefaultValue(true) ? clrStaticText : GetXtremeColor(COLOR_HIGHLIGHT);
		if (m_pStaticText->GetTextColor() != nTextColor)
			m_pStaticText->SetTextColor(nTextColor);
	}

	if (m_cExpandButton)
	{
		static COLORREF nDefColor = ::GetSysColor(COLOR_BTNFACE);
		static COLORREF nDefTextColor = ::GetSysColor(COLOR_BTNTEXT);
		static COLORREF nNondefColor = GetXtremeColor(COLOR_HIGHLIGHT);
		static COLORREF nNondefTextColor = GetXtremeColor(COLOR_HIGHLIGHTTEXT);
		bool bDefault = HasDefaultValue(true);
		COLORREF nColor = bDefault ? nDefColor : nNondefColor;

		if (m_cExpandButton->GetColor() != nColor)
		{
			m_cExpandButton->SetColor(nColor);
			m_cExpandButton->SetTextColor(bDefault ? nDefTextColor : nNondefTextColor);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnComboSelection()
{
	ReceiveFromControl();
	SendToControl();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::DrawValue(CDC* dc, CRect rect)
{
	// Setup text.
	dc->SetBkMode(TRANSPARENT);

	CString val = GetDrawValue();

	if (m_type == ePropertyBool)
	{
		int sz = rect.bottom - rect.top - 1;
		int borderwidth = (int)(sz * 0.13f);                            //white frame border
		int borderoffset = (int)(sz * 0.1f);                            //offset from the frameborder
		CPoint p1(rect.left, rect.top + 1);                             // box
		CPoint p2(rect.left + borderwidth, rect.top + 1 + borderwidth); //check-mark

		const bool bPropertyCtrlIsDisabled = m_propertyCtrl->IsReadOnly() || m_propertyCtrl->IsGrayed() || (m_propertyCtrl->IsWindowEnabled() != TRUE);
		const bool bShowInactiveFrame = IsDisabled() || bPropertyCtrlIsDisabled;

		CRect rc(p1.x, p1.y, p1.x + sz, p1.y + sz);
		dc->DrawFrameControl(rc, DFC_BUTTON, DFCS_BUTTON3STATE | (bShowInactiveFrame ? DFCS_INACTIVE : 0));
		if (GetBoolValue())
		{
			COLORREF color = StringToColor(val);
			if (bShowInactiveFrame)
			{
				color = (COLORREF)ColorB::ComputeAvgCol_Fast(RGB(255, 255, 255), color);
			}
			CPen pen(PS_SOLID, 1, color);
			CPen* pPen = dc->SelectObject(&pen);

			//Draw bold check-mark
			int number_of_lines = sz / 8;
			number_of_lines += 1;
			for (int i = 0; i < number_of_lines; i++)
			{
				CPoint start = p2 + CPoint(i, 0);
				start.y += ((sz - borderwidth * 2) / 2);
				start.x += borderoffset;

				CPoint middle = p2 + CPoint(i, 0);
				middle.y += sz - borderwidth * 2;
				middle.x += (sz - borderwidth - number_of_lines) / 2;
				middle.y -= borderoffset;

				CPoint end = p2 + CPoint(i, 0);
				end.x += sz - borderwidth * 2 - number_of_lines;
				end.x -= borderoffset;
				end.y += borderoffset;

				dc->MoveTo(start);
				dc->LineTo(middle);
				dc->LineTo(end);
			}
		}

		//offset text with scaling
		rect.left += (int)(sz * 1.25f);
	}
	else if (m_type == ePropertyFile || m_type == ePropertyTexture || m_type == ePropertyModel)
	{
		// Any file.
		// Check if file name fits into the designated rectangle.
		CSize textSize = dc->GetTextExtent(val, val.GetLength());
		if (textSize.cx > rect.Width())
		{
			// Cut file name...
			if (strcmp(PathUtil::GetExt(val), "") != 0)
				val = CString("...\\") + PathUtil::GetFile(val);
		}
	}
	else if (m_type == ePropertyColor)
	{
		//CRect rc( CPoint(rect.right-BUTTON_WIDTH,rect.top),CSize(BUTTON_WIDTH,rect.bottom-rect.top) );
		CRect rc(CPoint(rect.left, rect.top + 1), CSize(BUTTON_WIDTH + 2, rect.bottom - rect.top - 2));
		//CPen pen( PS_SOLID,1,RGB(128,128,128));
		CPen pen(PS_SOLID, 1, RGB(0, 0, 0));
		CBrush brush(StringToColor(val));
		CPen* pOldPen = dc->SelectObject(&pen);
		CBrush* pOldBrush = dc->SelectObject(&brush);
		dc->Rectangle(rc);
		//COLORREF col = StringToColor(m_value);
		//rc.DeflateRect( 1,1 );
		//dc->FillSolidRect( rc,col );
		dc->SelectObject(pOldPen);
		dc->SelectObject(pOldBrush);
		rect.left = rect.left + BUTTON_WIDTH + 2 + 4;
	}
	else if (m_pVariable != 0 && m_pVariable->GetSpline(false) && m_pVariable->GetSpline(false)->GetKeyCount() > 0)
	{
		// Draw mini-curve or gradient.
		CPen* pOldPen = 0;

		ISplineInterpolator* pSpline = m_pVariable->GetSpline(true);
		int width = min(rect.Width() - 1, 128);
		for (int x = 0; x < width; x++)
		{
			float time = float(x) / (width - 1);
			ISplineInterpolator::ValueType val;
			pSpline->Interpolate(time, val);

			if (m_type == ePropertyColorCurve)
			{
				COLORREF col = RGB(pos_round(val[0] * 255), pos_round(val[1] * 255), pos_round(val[2] * 255));
				CPen pen(PS_SOLID, 1, col);
				if (!pOldPen)
					pOldPen = dc->SelectObject(&pen);
				else
					dc->SelectObject(&pen);
				dc->MoveTo(CPoint(rect.left + x, rect.bottom));
				dc->LineTo(CPoint(rect.left + x, rect.top));
			}
			else if (m_type == ePropertyFloatCurve)
			{
				CPoint point;
				point.x = rect.left + x;
				point.y = int_round((rect.bottom - 1) * (1.f - val[0]) + (rect.top + 1) * val[0]);
				if (x == 0)
					dc->MoveTo(point);
				else
					dc->LineTo(point);
			}

			if (pOldPen)
				dc->SelectObject(pOldPen);
		}

		// No text.
		return;
	}

	/*
	   //////////////////////////////////////////////////////////////////////////
	   // Draw filled bar like in CFillSliderCtrl.
	   //////////////////////////////////////////////////////////////////////////
	   if (m_type == ePropertyFloat || m_type == ePropertyInt || m_type == ePropertyAngle)
	   {
	   CRect rc = rect;
	   rc.left += NUMBER_CTRL_WIDTH;
	   rc.top += 1;
	   rc.bottom -= 1;

	   float value = atof(m_value);
	   float min = m_rangeMin/m_valueMultiplier;
	   float max = m_rangeMax/m_valueMultiplier;
	   if (min == max)
	    max = min + 1;
	   float pos = (value-min) / fabs(max-min);
	   int splitPos = rc.left + pos * rc.Width();

	   // Paint filled rect.
	   CRect fillRc = rc;
	   fillRc.right = splitPos;
	   dc->FillRect(fillRc,CBrush::FromHandle((HBRUSH)GetStockObject(LTGRAY_BRUSH)) );

	   // Paint empty rect.
	   CRect emptyRc = rc;
	   emptyRc.left = splitPos+1;
	   emptyRc.IntersectRect(emptyRc,rc);
	   dc->FillRect(emptyRc,CBrush::FromHandle((HBRUSH)GetStockObject(WHITE_BRUSH)) );
	   }
	 */

	CRect textRc;
	textRc = rect;
	::DrawTextEx(dc->GetSafeHdc(), val.GetBuffer(), val.GetLength(), textRc, DT_END_ELLIPSIS | DT_LEFT | DT_SINGLELINE | DT_VCENTER, NULL);
}

COLORREF CPropertyItem::StringToColor(const CString& value)
{
	float r, g, b;
	int res = 0;
	if (res != 3)
		res = sscanf(value, "%f,%f,%f", &r, &g, &b);
	if (res != 3)
		res = sscanf(value, "R:%f,G:%f,B:%f", &r, &g, &b);
	if (res != 3)
		res = sscanf(value, "R:%f G:%f B:%f", &r, &g, &b);
	if (res != 3)
		res = sscanf(value, "%f %f %f", &r, &g, &b);
	if (res != 3)
	{
		sscanf(value, "%f", &r);
		return r;
	}
	int ir = r;
	int ig = g;
	int ib = b;

	return RGB(ir, ig, ib);
}

bool CPropertyItem::GetBoolValue()
{
	if (stricmp(m_value, "true") == 0 || atoi(m_value) != 0)
		return true;
	else
		return false;
}

//////////////////////////////////////////////////////////////////////////
const char* CPropertyItem::GetValue() const
{
	return m_value;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::SetValue(const char* sValue, bool bRecordUndo, bool bForceModified)
{
	if (bRecordUndo && IsDisabled())
		return;

	_smart_ptr<CPropertyItem> holder = this; // Make sure we are not released during this function.

	CString value = sValue;

	switch (m_type)
	{
	case ePropertyAiTerritory:
#ifdef USE_SIMPLIFIED_AI_TERRITORY_SHAPE
		if ((value == "<None>") || m_value.Compare(value))
#else
		if ((value == "<Auto>") || (value == "<None>") || m_value.Compare(value))
#endif
		{
			CPropertyItem* pPropertyItem = GetParent()->FindItemByFullName("::AITerritoryAndWave::Wave");
			if (pPropertyItem)
			{
				pPropertyItem->SetValue("<None>");
			}
		}
		break;

	case ePropertyBool:
		if (stricmp(value, "true") == 0 || atof(value) != 0)
			value = "1";
		else
			value = "0";
		break;

	case ePropertyVector2:
		if (value.Find(',') < 0)
			value = value + ", " + value;
		break;

	case ePropertyVector4:
		if (value.Find(',') < 0)
			value = value + ", " + value + ", " + value + ", " + value;
		break;

	case ePropertyVector:
		if (value.Find(',') < 0)
			value = value + ", " + value + ", " + value;
		break;

	case ePropertyTexture:
	case ePropertyModel:
	case ePropertyMaterial:
		value.Replace('\\', '/');
		break;
	}

	// correct the length of value
	switch (m_type)
	{
	case ePropertyTexture:
	case ePropertyModel:
	case ePropertyMaterial:
	case ePropertyFile:
		if (value.GetLength() >= MAX_PATH)
			value = value.Left(MAX_PATH);
		break;
	}

	bool bModified = m_bForceModified || bForceModified || m_value.Compare(value) != 0;
	bool bStoreUndo = (m_value.Compare(value) != 0 || bForceModified) && bRecordUndo;

	std::auto_ptr<CUndo> undo;
	if (bStoreUndo && !CUndo::IsRecording())
	{
		if (!m_propertyCtrl->CallUndoFunc(this))
			undo.reset(new CUndo(GetName() + " Modified"));
	}

	m_value = value;

	if (m_pVariable)
	{
		if (bModified)
		{
			if (m_propertyCtrl->IsStoreUndoByItems() && bStoreUndo && CUndo::IsRecording())
				CUndo::Record(new CUndoVariableChange(m_pVariable, "PropertyChange"));

			if (m_bForceModified || bForceModified)
				m_pVariable->SetForceModified(true);
			ValueToVar();
		}
	}
	else
	{
		//////////////////////////////////////////////////////////////////////////
		// DEPRICATED (For XmlNode).
		//////////////////////////////////////////////////////////////////////////
		if (m_node)
			m_node->setAttr(VALUE_ATTR, m_value);
		//CString xml = m_node->getXML();

		SendToControl();

		if (bModified)
		{
			m_modified = true;
			if (m_bEditChildren)
			{
				// Extract child components.
				int pos = 0;
				for (int i = 0; i < m_childs.size(); i++)
				{
					CString elem = m_value.Tokenize(", ", pos);
					m_childs[i]->m_value = elem;
					m_childs[i]->SendToControl();
				}
			}

			if (m_parent)
				m_parent->OnChildChanged(this);
			// If Value changed mark document modified.
			// Notify parent that this Item have been modified.
			m_propertyCtrl->OnItemChange(this);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::VarToValue()
{
	assert(m_pVariable != 0);

	if (m_type == ePropertyColor)
	{
		if (m_pVariable->GetType() == IVariable::VECTOR)
		{
			Vec3 v(0, 0, 0);
			m_pVariable->Get(v);
			COLORREF col = CMFCUtils::ColorLinearToGamma(ColorF(v.x, v.y, v.z));
			m_value.Format("%d,%d,%d", GetRValue(col), GetGValue(col), GetBValue(col));
		}
		else
		{
			int col(0);
			m_pVariable->Get(col);
			m_value.Format("%d,%d,%d", GetRValue((uint32)col), GetGValue((uint32)col), GetBValue((uint32)col));
		}
		return;
	}

	if (m_type == ePropertyFloat)
	{
		float value;
		m_pVariable->Get(value);

		PropertyItem_Private::FormatFloatForUICString(m_value, FLOAT_NUM_DIGITS, value);
	}
	else
	{
		m_value = m_pVariable->GetDisplayValue();
	}

	if (m_type == ePropertySOHelper || m_type == ePropertySONavHelper || m_type == ePropertySOAnimHelper)
	{
		// hide smart object class part
		int f = m_value.Find(':');
		if (f >= 0)
			m_value.Delete(0, f + 1);
	}
}

CString CPropertyItem::GetDrawValue()
{
	CString value = m_value;

	if (m_pEnumDBItem)
		value = m_pEnumDBItem->ValueToName(value);

	if (m_valueMultiplier != 1.f)
	{
		float f = atof(m_value) * m_valueMultiplier;
		if (m_type == ePropertyInt)
			value.Format("%d", int_round(f));
		else
			value.Format("%g", f);
		if (m_valueMultiplier == 100)
			value += "%";
	}
	else if (m_type == ePropertyBool)
	{
		return "";
	}
	else if (m_type == ePropertyFloatCurve || m_type == ePropertyColorCurve)
	{
		// Don't display actual values in field.
		if (!value.IsEmpty())
			value = "[Curve]";
	}
	else if (m_type == ePropertySequenceId)
	{
		uint32 id = (uint32)atoi(value);
		IAnimSequence* pSeq = GetIEditor()->GetMovieSystem()->FindSequenceById(id);
		if (pSeq) // Show its human-readable name instead of its ID.
			return pSeq->GetName();
	}

	return value;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::ValueToVar()
{
	assert(m_pVariable != NULL);

	_smart_ptr<CPropertyItem> holder = this; // Make sure we are not released during the call to variable Set.

	if (m_type == ePropertyColor)
	{
		COLORREF col = StringToColor(m_value);
		if (m_pVariable->GetType() == IVariable::VECTOR)
		{
			ColorF colLin = CMFCUtils::ColorGammaToLinear(col);
			m_pVariable->Set(Vec3(colLin.r, colLin.g, colLin.b));
		}
		else
			m_pVariable->Set((int)col);
	}
	else if (m_type == ePropertySOHelper || m_type == ePropertySONavHelper || m_type == ePropertySOAnimHelper)
	{
		// keep smart object class part

		CString oldValue;
		m_pVariable->Get(oldValue);
		int f = oldValue.Find(':');
		if (f >= 0)
			oldValue.Truncate(f + 1);

		CString newValue(m_value);
		f = newValue.Find(':');
		if (f >= 0)
			newValue.Delete(0, f + 1);

		m_pVariable->Set(oldValue + newValue);
	}
	else if (m_type != ePropertyInvalid)
	{
		m_pVariable->SetDisplayValue(m_value);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnVariableChange(IVariable* pVar)
{
	assert(pVar != 0 && pVar == m_pVariable);

	// When variable changes, invalidate UI.
	m_modified = true;

	VarToValue();

	// update enum list
	if (m_type == ePropertySelection)
		m_enumList = m_pVariable->GetEnumList();

	SendToControl();

	if (m_bEditChildren)
	{
		switch (m_type)
		{
		case ePropertyAiPFPropertiesList:
			AddChildrenForPFProperties();
			break;

		case ePropertyAiEntityClasses:
			AddChildrenForAIEntityClasses();
			break;

		default:
			{
				// Parse comma-separated values, set children.
				bool bPrevIgnore = m_bIgnoreChildsUpdate;
				m_bIgnoreChildsUpdate = true;

				int pos = 0;
				for (int i = 0; i < m_childs.size(); i++)
				{
					CString sElem = pos >= 0 ? m_value.Tokenize(", ", pos) : CString();
					m_childs[i]->SetValue(sElem, false);
				}
				m_bIgnoreChildsUpdate = bPrevIgnore;
			}
		}
	}

	if (m_parent)
		m_parent->OnChildChanged(this);

	// If Value changed mark document modified.
	// Notify parent that this Item have been modified.
	// This may delete this control...
	m_propertyCtrl->OnItemChange(this);
}
//////////////////////////////////////////////////////////////////////////
CPropertyItem::TDValues* CPropertyItem::GetEnumValues(const char* strPropertyName)
{
	TDValuesContainer::iterator itIterator;
	TDValuesContainer& cEnumContainer = CUIEnumerations::GetUIEnumerationsInstance().GetStandardNameContainer();

	itIterator = cEnumContainer.find(strPropertyName);
	if (itIterator == cEnumContainer.end())
	{
		return NULL;
	}

	return &itIterator->second;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnMouseWheel(UINT nFlags, short zDelta, CPoint point)
{
	if (m_propertyCtrl->IsReadOnly())
		return;

	if (m_cCombo)
	{
		int sel = m_cCombo->GetCurSel();
		if (zDelta > 0)
		{
			sel++;
			if (m_cCombo->SetCurSel(sel) == CB_ERR)
				m_cCombo->SetCurSel(0);
		}
		else
		{
			sel--;
			if (m_cCombo->SetCurSel(sel) == CB_ERR)
				m_cCombo->SetCurSel(m_cCombo->GetCount() - 1);
		}
	}
	else if (m_cNumber)
	{
		if (zDelta > 0)
		{
			m_cNumber->SetValue(m_cNumber->GetValue() + m_cNumber->GetStep());
		}
		else
		{
			m_cNumber->SetValue(m_cNumber->GetValue() - m_cNumber->GetStep());
		}
		ReceiveFromControl();
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	if (m_propertyCtrl->IsReadOnly())
		return;

	if (IsDisabled())
		return;

	if (m_type == ePropertyBool)
	{
		// Swap boolean value.
		if (GetBoolValue())
			SetValue("0");
		else
			SetValue("1");
	}
	else
	{
		// Simulate button click.
		if (m_cButton)
			m_cButton->Click();
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (m_propertyCtrl->IsReadOnly())
		return;

	if (m_type == ePropertyBool)
	{
		CRect rect;
		m_propertyCtrl->GetItemRect(this, rect);
		rect = m_propertyCtrl->GetItemValueRect(rect);

		CPoint p(rect.left - 2, rect.top + 1);
		int sz = rect.bottom - rect.top;
		rect = CRect(p.x, p.y, p.x + sz, p.y + sz);

		if (rect.PtInRect(point))
		{
			// Swap boolean value.
			if (GetBoolValue())
				SetValue("0");
			else
				SetValue("1");
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnCheckBoxButton()
{
	if (m_cCheckBox)
		if (m_cCheckBox->GetChecked() == BST_CHECKED)
			SetValue("1");
		else
			SetValue("0");
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnColorBrowseButton()
{
	/*
	   COLORREF clr = StringToColor(m_value);
	   if (GetIEditor()->SelectColor(clr,m_propertyCtrl))
	   {
	   int r,g,b;
	   r = GetRValue(clr);
	   g = GetGValue(clr);
	   b = GetBValue(clr);
	   //val.Format( "R:%d G:%d B:%d",r,g,b );
	   CString val;
	   val.Format( "%d,%d,%d",r,g,b );
	   SetValue( val );
	   m_propertyCtrl->Invalidate();
	   //RedrawWindow( OwnerProperties->hWnd,NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN );
	   }
	 */
	if (IsDisabled())
		return;

	COLORREF orginalColor = StringToColor(m_value);
	COLORREF clr = orginalColor;
	CCustomColorDialog dlg(orginalColor, CC_FULLOPEN, m_propertyCtrl);
	dlg.SetColorChangeCallback(functor(*this, &CPropertyItem::OnColorChange));
	if (dlg.DoModal() == IDOK)
	{
		clr = dlg.GetColor();
		if (clr != orginalColor)
		{
			int r, g, b;
			CString val;
			r = GetRValue(orginalColor);
			g = GetGValue(orginalColor);
			b = GetBValue(orginalColor);
			val.Format("%d,%d,%d", r, g, b);
			SetValue(val, false);

			r = GetRValue(clr);
			g = GetGValue(clr);
			b = GetBValue(clr);
			val.Format("%d,%d,%d", r, g, b);
			SetValue(val);
			m_propertyCtrl->InvalidateCtrl();
		}
	}
	else
	{
		if (StringToColor(m_value) != orginalColor)
		{
			int r, g, b;
			r = GetRValue(clr);
			g = GetGValue(clr);
			b = GetBValue(clr);
			CString val;
			val.Format("%d,%d,%d", r, g, b);
			SetValue(val);
			m_propertyCtrl->InvalidateCtrl();
		}
	}
	if (m_cButton)
		m_cButton->Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnColorChange(COLORREF clr)
{
	GetIEditor()->GetIUndoManager()->Suspend();
	int r, g, b;
	r = GetRValue(clr);
	g = GetGValue(clr);
	b = GetBValue(clr);
	//val.Format( "R:%d G:%d B:%d",r,g,b );
	CString val;
	val.Format("%d,%d,%d", r, g, b);
	SetValue(val);
	GetIEditor()->UpdateViews(eRedrawViewports);
	//GetIEditor()->Notify( eNotify_OnIdleUpdate );
	m_propertyCtrl->InvalidateCtrl();
	GetIEditor()->GetIUndoManager()->Resume();

	if (m_cButton)
		m_cButton->Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnFileBrowseButton()
{
	CString tempValue("");
	CString ext("");
	if (m_value.IsEmpty() == false)
	{
		if (strcmp(PathUtil::GetExt(m_value), "") == 0)
		{
			tempValue = "";
		}
		else
		{
			tempValue = m_value;
		}
	}

	CString startPath = PathUtil::GetPathWithoutFilename(tempValue);

	m_propertyCtrl->HideBitmapTooltip();

	if (m_type == ePropertyTexture)
	{
		dll_string newValue = GetIEditor()->GetResourceSelectorHost()->SelectResource("Texture", tempValue);
		SetValue(newValue.c_str(), true, false);
	}
	else if (m_type == ePropertyGeomCache)
	{
		dll_string newValue = GetIEditor()->GetResourceSelectorHost()->SelectResource("GeometryCache", tempValue);
		SetValue(newValue.c_str(), true, true);
	}
	else
	{
		CString relativeFilename = tempValue;
		if (CFileUtil::SelectSingleFile(EFILE_TYPE_ANY, relativeFilename, "", startPath))
		{
			SetValue(relativeFilename, true, false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnResourceSelectorButton()
{
	m_propertyCtrl->HideBitmapTooltip();

	dll_string newValue = GetIEditor()->GetResourceSelectorHost()->SelectResource(PropertyTypeToResourceType(m_type), GetValue(), nullptr, m_pVariable->GetUserData());
	if (strcmp(GetValue(), newValue.c_str()) != 0)
	{
		SetValue(newValue.c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnTextureEditButton()
{
	CFileUtil::EditTextureFile(m_value, true);
}
//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnPsdEditButton()
{
	CString dccFilename;
	if (CFileUtil::CalculateDccFilename(m_value, dccFilename))
	{
		CFileUtil::EditTextureFile(dccFilename, true);
	}
	else
	{
		char buff[1024];
		cry_sprintf(buff, "Failed to find psd file for texture: '%s'", m_value);
		CQuestionDialog::SCritical("Error", buff);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnAnimationApplyButton()
{
	CUIEnumerations& roGeneralProxy = CUIEnumerations::GetUIEnumerationsInstance();
	std::vector<string> cSelectedAnimations;
	size_t nTotalAnimations(0);
	size_t nCurrentAnimation(0);

	string combinedString = GetIEditor()->GetResourceSelectorHost()->GetGlobalSelection("animation");
	SplitString(combinedString, cSelectedAnimations, ',');

	nTotalAnimations = cSelectedAnimations.size();
	for (nCurrentAnimation = 0; nCurrentAnimation < nTotalAnimations; ++nCurrentAnimation)
	{
		string& rstrCurrentAnimAction = cSelectedAnimations[nCurrentAnimation];
		if (rstrCurrentAnimAction.GetLength())
		{
			m_pVariable->Set(rstrCurrentAnimAction);
			return;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::ReloadValues()
{
	m_modified = false;
	if (m_node)
		ParseXmlNode(false);
	if (m_pVariable)
		SetVariable(m_pVariable);
	for (int i = 0; i < GetChildCount(); i++)
	{
		GetChild(i)->ReloadValues();
	}
	SendToControl();
}

//////////////////////////////////////////////////////////////////////////
CString CPropertyItem::GetTip() const
{
	if (!m_tip.IsEmpty() || m_name.Left(1) == "_")
		return m_tip;

	CString type;
	for (int i = 0; i < NumPropertyTypes; i++)
	{
		if (m_type == s_propertyTypeNames[i].type)
		{
			type = s_propertyTypeNames[i].name;
			break;
		}
	}

	CString tip = CString("[") + type + "] " + m_name + " = " + m_value;

	if (HasScriptDefault())
	{
		tip += " [Script Default: ";
		if (m_strScriptDefault.IsEmpty())
			tip += "<blank>";
		else
			tip += m_strScriptDefault;
		tip += "]";
	}

	if (m_pVariable)
	{
		CString description = m_pVariable->GetDescription();
		if (!description.IsEmpty())
		{
			tip += CString(" ") + description;
		}
	}
	return tip;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnEditChanged()
{
	ReceiveFromControl();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnNumberCtrlUpdate(CNumberCtrl* ctrl)
{
	ReceiveFromControl();
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnFillSliderCtrlUpdate(CSliderCtrlEx* ctrl)
{
	if (m_cFillSlider)
	{
		float fValue = m_cFillSlider->GetValue();

		if (m_step != 0.f)
		{
			// Round to next power of 10 below step.
			float fRound = pow(10.f, floor(log(m_step) / log(10.f))) / m_valueMultiplier;
			fValue = int_round(fValue / fRound) * fRound;
		}
		fValue = clamp_tpl(fValue, m_rangeMin, m_rangeMax);

		CString val;
		val.Format("%g", fValue);
		SetValue(val, true, true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnEditDeprecatedProperty()
{
	string new_value;
	if (GetIEditor()->EditDeprecatedProperty(m_type, m_value.GetString(), new_value))
	{
		SetValue(new_value);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnMaterialBrowseButton()
{
	// Open material browser dialog.
	CString name = GetValue();
	IDataBaseItem* pItem = GetIEditor()->GetDBItemManager(EDB_TYPE_MATERIAL)->FindItemByName(name.GetBuffer());
	GetIEditor()->OpenAndFocusDataBase(EDB_TYPE_MATERIAL, pItem);
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnMaterialPickSelectedButton()
{
	// Open material browser dialog.
	IDataBaseItem* pItem = GetIEditor()->GetDBItemManager(EDB_TYPE_MATERIAL)->GetSelectedItem();
	if (pItem)
		SetValue(pItem->GetName());
	else
		SetValue("");
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnSequenceBrowseButton()
{
	CSelectSequenceDialog gtDlg(m_propertyCtrl);
	gtDlg.PreSelectItem(GetValue());
	if (gtDlg.DoModal() == IDOK)
		SetValue(gtDlg.GetSelectedItem());
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnSequenceIdBrowseButton()
{
	CSelectSequenceDialog gtDlg(m_propertyCtrl);
	uint32 id = (uint32)atoi(GetValue());
	IAnimSequence* pSeq = GetIEditor()->GetMovieSystem()->FindSequenceById(id);
	if (pSeq)
		gtDlg.PreSelectItem(pSeq->GetName());
	if (gtDlg.DoModal() == IDOK)
	{
		pSeq = GetIEditor()->GetMovieSystem()->FindSequence(gtDlg.GetSelectedItem());
		assert(pSeq);
		if (pSeq->GetId() > 0)  // This sequence is a new one with a valid ID.
		{
			CString buf;
			buf.Format("%d", pSeq->GetId());
			SetValue(buf);
		}
		else                    // This sequence is an old one without an ID.
		{
			CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("This is an old sequence without an ID.\nSo it cannot be used with the new ID-based linking."));
		}
	}
}

void CPropertyItem::OnMissionObjButton()
{
	CSelectMissionObjectiveDialog gtDlg(m_propertyCtrl);
	gtDlg.PreSelectItem(GetValue());
	if (gtDlg.DoModal() == IDOK)
		SetValue(gtDlg.GetSelectedItem());
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnUserBrowseButton()
{
	IVariable::IGetCustomItems* pGetCustomItems = static_cast<IVariable::IGetCustomItems*>(m_pVariable->GetUserData());
	if (pGetCustomItems != 0)
	{
		std::vector<IVariable::IGetCustomItems::SItem> items;
		string dlgTitle;
		// call the user supplied callback to fill-in items and get dialog title
		bool bShowIt = pGetCustomItems->GetItems(m_pVariable, items, dlgTitle);
		if (bShowIt) // if func didn't veto, show the dialog
		{
			CGenericSelectItemDialog gtDlg(m_propertyCtrl);
			if (pGetCustomItems->UseTree())
			{
				gtDlg.SetMode(CGenericSelectItemDialog::eMODE_TREE);
				const char* szSep = pGetCustomItems->GetTreeSeparator();
				if (szSep)
				{
					CString sep(szSep);
					gtDlg.SetTreeSeparator(sep);
				}
			}
			gtDlg.SetItems(items);
			if (dlgTitle.IsEmpty() == false)
				gtDlg.SetTitle(CString(dlgTitle.GetString()));
			gtDlg.PreSelectItem(GetValue());
			if (gtDlg.DoModal() == IDOK)
			{
				CString selectedItemStr = gtDlg.GetSelectedItem();

				if (selectedItemStr.IsEmpty() == false)
				{
					SetValue(selectedItemStr);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnLocalStringBrowseButton()
{
	std::vector<IVariable::IGetCustomItems::SItem> items;
	ILocalizationManager* pMgr = gEnv->pSystem->GetLocalizationManager();
	if (!pMgr)
		return;
	int nCount = pMgr->GetLocalizedStringCount();
	if (nCount <= 0)
		return;
	items.reserve(nCount);
	IVariable::IGetCustomItems::SItem item;
	SLocalizedInfoEditor sInfo;
	for (int i = 0; i < nCount; ++i)
	{
		if (pMgr->GetLocalizedInfoByIndex(i, sInfo))
		{
			item.desc = _T("English Text:\r\n");
			item.desc += Unicode::Convert<wstring>(sInfo.sUtf8TranslatedText).c_str();
			item.name = sInfo.sKey;
			items.push_back(item);
		}
	}
	CString dlgTitle;
	CGenericSelectItemDialog gtDlg(m_propertyCtrl);
	const bool bUseTree = true;
	if (bUseTree)
	{
		gtDlg.SetMode(CGenericSelectItemDialog::eMODE_TREE);
		gtDlg.SetTreeSeparator("/");
	}
	gtDlg.SetItems(items);
	gtDlg.SetTitle(_T("Choose Localized String"));
	CString preselect = GetValue();
	if (!preselect.IsEmpty() && preselect.GetAt(0) == '@')
		preselect = preselect.Mid(1);
	gtDlg.PreSelectItem(preselect);
	if (gtDlg.DoModal() == IDOK)
	{
		preselect = "@";
		preselect += gtDlg.GetSelectedItem();
		SetValue(preselect);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::OnExpandButton()
{
	m_propertyCtrl->Expand(this, !IsExpanded(), true);
}

//////////////////////////////////////////////////////////////////////////
int CPropertyItem::GetHeight()
{
	if (m_propertyCtrl->IsExtenedUI())
	{
		//m_nHeight = 20;
		switch (m_type)
		{
		case ePropertyFloatCurve:
			//m_nHeight = 52;
			return 52;
			break;
		case ePropertyColorCurve:
			//m_nHeight = 36;
			return 36;
		}
		return 20;
	}
	return m_nHeight;
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::AddChildrenForPFProperties()
{
	assert(m_type == ePropertyAiPFPropertiesList);

	for (Childs::iterator it = m_childs.begin(); it != m_childs.end(); ++it)
		m_propertyCtrl->DestroyControls(*it);

	RemoveAllChildren();
	m_propertyCtrl->InvalidateCtrl();

	std::set<CString> setSelectedPathTypeNames;

	CString token;
	int index = 0;
	while (!(token = m_value.Tokenize(" ,", index)).IsEmpty())
		setSelectedPathTypeNames.insert(token);

	int N = 1;
	char propertyN[100];
	for (std::set<CString>::iterator it = setSelectedPathTypeNames.begin(); it != setSelectedPathTypeNames.end(); ++it)
	{
		IVariable* pVar = new CVariable<CString>;
		cry_sprintf(propertyN, "AgentType %d", N++);
		pVar->SetName(propertyN);
		pVar->Set(*it);

		CPropertyItemPtr pItem = new CPropertyItem(m_propertyCtrl);
		pItem->SetVariable(pVar);
		AddChild(pItem);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::AddChildrenForAIEntityClasses()
{
	assert(m_type == ePropertyAiEntityClasses);

	for (Childs::iterator it = m_childs.begin(); it != m_childs.end(); ++it)
	{
		m_propertyCtrl->DestroyControls(*it);
	}

	RemoveAllChildren();
	m_propertyCtrl->InvalidateCtrl();

	std::set<CString> setSelectedAIEntityClasses;

	CString token;
	int index = 0;
	while (!(token = m_value.Tokenize(" ,", index)).IsEmpty())
		setSelectedAIEntityClasses.insert(token);

	int N = 1;
	char propertyN[100];
	for (std::set<CString>::iterator it = setSelectedAIEntityClasses.begin(); it != setSelectedAIEntityClasses.end(); ++it)
	{
		IVariable* pVar = new CVariable<CString>;
		cry_sprintf(propertyN, "Entity Class %d", N++);
		pVar->SetName(propertyN);
		pVar->Set(*it);

		CPropertyItemPtr pItem = new CPropertyItem(m_propertyCtrl);
		pItem->SetVariable(pVar);
		AddChild(pItem);
	}
}

static inline bool AlphabeticalBaseObjectLess(const CBaseObject* p1, const CBaseObject* p2)
{
	return p1->GetName() < p2->GetName();
}

//////////////////////////////////////////////////////////////////////////
/*
void CPropertyItem::PopulateAITerritoriesList()
{
	CVariableEnum<string>* pVariable = static_cast<CVariableEnum<string>*>(&*m_pVariable);

	pVariable->SetEnumList(0);
#ifndef USE_SIMPLIFIED_AI_TERRITORY_SHAPE
	pVariable->AddEnumItem("<Auto>", "<Auto>");
#endif
	pVariable->AddEnumItem("<None>", "<None>");

	std::vector<CBaseObject*> vTerritories;
	GetIEditor()->GetObjectManager()->FindObjectsOfType(RUNTIME_CLASS(CAITerritoryObject), vTerritories);
	std::sort(vTerritories.begin(), vTerritories.end(), AlphabeticalBaseObjectLess);

	for (std::vector<CBaseObject*>::iterator it = vTerritories.begin(); it != vTerritories.end(); ++it)
	{
		const string& name = (*it)->GetName();
		pVariable->AddEnumItem(name, name);
	}

	m_enumList = pVariable->GetEnumList();
}*/

//////////////////////////////////////////////////////////////////////////
/*
void CPropertyItem::PopulateAIWavesList()
{
	CVariableEnum<string>* pVariable = static_cast<CVariableEnum<string>*>(&*m_pVariable);

	pVariable->SetEnumList(0);
	pVariable->AddEnumItem("<None>", "<None>");

	CPropertyItem* pPropertyItem = GetParent()->FindItemByFullName("::AITerritoryAndWave::Territory");
	if (pPropertyItem)
	{
		CString sTerritoryName = pPropertyItem->GetValue();
#ifdef USE_SIMPLIFIED_AI_TERRITORY_SHAPE
		if (sTerritoryName != "<None>")
#else
		if ((sTerritoryName != "<Auto>") && (sTerritoryName != "<None>"))
#endif
		{
			std::vector<CAIWaveObject*> vLinkedAIWaves;

			CBaseObject* pBaseObject = GetIEditor()->GetObjectManager()->FindObject(sTerritoryName);
			if (pBaseObject && pBaseObject->IsKindOf(RUNTIME_CLASS(CAITerritoryObject)))
			{
				CAITerritoryObject* pTerritory = static_cast<CAITerritoryObject*>(pBaseObject);
				pTerritory->GetLinkedWaves(vLinkedAIWaves);
			}

			std::sort(vLinkedAIWaves.begin(), vLinkedAIWaves.end(), AlphabeticalBaseObjectLess);

			for (std::vector<CAIWaveObject*>::iterator it = vLinkedAIWaves.begin(); it != vLinkedAIWaves.end(); ++it)
			{
				const string& name = (*it)->GetName();
				pVariable->AddEnumItem(name, name);
			}
		}
	}

	m_enumList = pVariable->GetEnumList();
}*/

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::RepositionWindow(CWnd* pWnd, CRect rc)
{
	if (!s_HDWP)
	{
		pWnd->MoveWindow(rc, FALSE);
	}
	else
	{
		s_HDWP = DeferWindowPos(s_HDWP, pWnd->GetSafeHwnd(), 0, rc.left, rc.top, rc.Width(), rc.Height(), SWP_NOACTIVATE | SWP_NOZORDER);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::RegisterCtrl(CWnd* pCtrl)
{
	if (pCtrl)
	{
		m_controls.push_back(pCtrl);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::EnableControls(bool bEnable)
{
	for (int i = 0, num = m_controls.size(); i < num; i++)
	{
		CWnd* pWnd = m_controls[i];
		if (pWnd && pWnd->GetSafeHwnd())
		{
			pWnd->EnableWindow((bEnable) ? TRUE : FALSE);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPropertyItem::EnableNotifyWithoutValueChange(bool bFlag)
{
	for (int i = 0; i < GetChildCount(); ++i)
	{
		CPropertyItem* item = GetChild(i);
		item->EnableNotifyWithoutValueChange(bFlag);
	}
	m_bForceModified = bFlag;
	if (m_pVariable)
		m_pVariable->EnableNotifyWithoutValueChange(m_bForceModified);
	if (m_cEdit)
		m_cEdit->EnableUpdateOnKillFocus(!m_bForceModified);
}


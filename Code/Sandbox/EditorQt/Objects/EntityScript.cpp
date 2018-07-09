// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EntityScript.h"
#include "EntityObject.h"
#include "CryEdit.h"
#include <CryScriptSystem/IScriptSystem.h>
#include <CryEntitySystem/IEntitySystem.h>
#include "LuaCommentParser.h"
#include "CryEdit.h"

struct CScriptMethodsDump : public IScriptTableDumpSink
{
	std::vector<string> methods;
	std::vector<string> events;

	virtual void OnElementFound(int nIdx, ScriptVarType type) {}

	virtual void OnElementFound(const char* sName, ScriptVarType type)
	{
		if (type == svtFunction)
		{
			if (strncmp(sName, EVENT_PREFIX, 6) == 0)
			{
				events.push_back(sName + 6);
			}
			else
			{
				methods.push_back(sName);
			}
		}
	}
};

enum EScriptParamFlags
{
	SCRIPTPARAM_POSITIVE = 0x01,
};

//////////////////////////////////////////////////////////////////////////
static struct
{
	const char*                           prefix;
	IVariable::EType                      type;
	IVariable::EDataType                  dataType;
	int  flags;
	bool bExactName;
} s_scriptParamTypes[] =
{
	{ "n",                   IVariable::INT,    IVariable::DT_SIMPLE,                SCRIPTPARAM_POSITIVE, false },
	{ "i",                   IVariable::INT,    IVariable::DT_SIMPLE,                0,                    false },
	{ "b",                   IVariable::BOOL,   IVariable::DT_SIMPLE,                0,                    false },
	{ "f",                   IVariable::FLOAT,  IVariable::DT_SIMPLE,                0,                    false },
	{ "s",                   IVariable::STRING, IVariable::DT_SIMPLE,                0,                    false },

	{ "ei",                  IVariable::INT,    IVariable::DT_UIENUM,                0,                    false },
	{ "es",                  IVariable::STRING, IVariable::DT_UIENUM,                0,                    false },

	{ "shader",              IVariable::STRING, IVariable::DT_SHADER,                0,                    false },
	{ "clr",                 IVariable::VECTOR, IVariable::DT_COLOR,                 0,                    false },
	{ "color",               IVariable::VECTOR, IVariable::DT_COLOR,                 0,                    false },

	{ "vector",              IVariable::VECTOR, IVariable::DT_SIMPLE,                0,                    false },

	{ "tex",                 IVariable::STRING, IVariable::DT_TEXTURE,               0,                    false },
	{ "texture",             IVariable::STRING, IVariable::DT_TEXTURE,               0,                    false },

	{ "obj",                 IVariable::STRING, IVariable::DT_OBJECT,                0,                    false },
	{ "object",              IVariable::STRING, IVariable::DT_OBJECT,                0,                    false },

	{ "file",                IVariable::STRING, IVariable::DT_FILE,                  0,                    false },
	{ "aibehavior",          IVariable::STRING, IVariable::DT_AI_BEHAVIOR,           0,                    false },
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
	{ "aicharacter",         IVariable::STRING, IVariable::DT_AI_CHARACTER,          0,                    false },
#endif
	{ "aipfpropertieslist",  IVariable::STRING, IVariable::DT_AI_PFPROPERTIESLIST,   0,                    false },
	{ "aientityclasses",     IVariable::STRING, IVariable::DT_AIENTITYCLASSES,       0,                    false },
	{ "aiterritory",         IVariable::STRING, IVariable::DT_AITERRITORY,           0,                    false },
	{ "aiwave",              IVariable::STRING, IVariable::DT_AIWAVE,                0,                    false },

	{ "text",                IVariable::STRING, IVariable::DT_LOCAL_STRING,          0,                    false },
	{ "equip",               IVariable::STRING, IVariable::DT_EQUIP,                 0,                    false },
	{ "reverbpreset",        IVariable::STRING, IVariable::DT_REVERBPRESET,          0,                    false },
	{ "eaxpreset",           IVariable::STRING, IVariable::DT_REVERBPRESET,          0,                    false },

	{ "aianchor",            IVariable::STRING, IVariable::DT_AI_ANCHOR,             0,                    false },

	{ "soclass",             IVariable::STRING, IVariable::DT_SOCLASS,               0,                    false },
	{ "soclasses",           IVariable::STRING, IVariable::DT_SOCLASSES,             0,                    false },
	{ "sostate",             IVariable::STRING, IVariable::DT_SOSTATE,               0,                    false },
	{ "sostates",            IVariable::STRING, IVariable::DT_SOSTATES,              0,                    false },
	{ "sopattern",           IVariable::STRING, IVariable::DT_SOSTATEPATTERN,        0,                    false },
	{ "soaction",            IVariable::STRING, IVariable::DT_SOACTION,              0,                    false },
	{ "sohelper",            IVariable::STRING, IVariable::DT_SOHELPER,              0,                    false },
	{ "sonavhelper",         IVariable::STRING, IVariable::DT_SONAVHELPER,           0,                    false },
	{ "soanimhelper",        IVariable::STRING, IVariable::DT_SOANIMHELPER,          0,                    false },
	{ "soevent",             IVariable::STRING, IVariable::DT_SOEVENT,               0,                    false },
	{ "sotemplate",          IVariable::STRING, IVariable::DT_SOTEMPLATE,            0,                    false },
	{ "customaction",        IVariable::STRING, IVariable::DT_CUSTOMACTION,          0,                    false },
	{ "gametoken",           IVariable::STRING, IVariable::DT_GAMETOKEN,             0,                    false },
	{ "seq_",                IVariable::STRING, IVariable::DT_SEQUENCE,              0,                    false },
	{ "mission_",            IVariable::STRING, IVariable::DT_MISSIONOBJ,            0,                    false },
	{ "seqid_",              IVariable::INT,    IVariable::DT_SEQUENCE_ID,           0,                    false },
	{ "flare_",              IVariable::STRING, IVariable::DT_FLARE,                 0,                    false },
	{ "ParticleEffect",      IVariable::STRING, IVariable::DT_PARTICLE_EFFECT,       0,                    true  },
	{ "geomcache",           IVariable::STRING, IVariable::DT_GEOM_CACHE,            0,                    false },
	{ "material",            IVariable::STRING, IVariable::DT_MATERIAL,              0 },
	{ "audioTrigger",        IVariable::STRING, IVariable::DT_AUDIO_TRIGGER,         0 },
	{ "audioSwitch",         IVariable::STRING, IVariable::DT_AUDIO_SWITCH,          0 },
	{ "audioSwitchState",    IVariable::STRING, IVariable::DT_AUDIO_SWITCH_STATE,    0 },
	{ "audioRTPC",           IVariable::STRING, IVariable::DT_AUDIO_RTPC,            0 },
	{ "audioEnvironment",    IVariable::STRING, IVariable::DT_AUDIO_ENVIRONMENT,     0 },
	{ "audioPreloadRequest", IVariable::STRING, IVariable::DT_AUDIO_PRELOAD_REQUEST, 0 },
};

//////////////////////////////////////////////////////////////////////////
struct CScriptPropertiesDump : public IScriptTableDumpSink
{
private:
	struct Variable
	{
		string        name;
		ScriptVarType type;
	};

	std::vector<Variable> m_elements;

	CVarBlock*            m_varBlock;
	IVariable*            m_parentVar;

public:
	explicit CScriptPropertiesDump(CVarBlock* pVarBlock, IVariable* pParentVar = NULL)
	{
		m_varBlock = pVarBlock;
		m_parentVar = pParentVar;
	}

	//////////////////////////////////////////////////////////////////////////
	inline bool IsPropertyTypeMatch(const char* type, const char* name, int nameLen, bool exactName)
	{
		if (exactName)
		{
			return strcmp(type, name) == 0;
		}

		// if starts from capital no type encoded.
		int typeLen = strlen(type);
		if (typeLen < nameLen && name[0] == tolower(name[0]))
		{
			// After type name Must follow Upper case or _.
			if (name[typeLen] != tolower(name[typeLen]) || name[typeLen] == '_')
			{
				if (strncmp(name, type, strlen(type)) == 0)
				{
					return true;
				}
			}
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	IVariable* CreateVarByType(IVariable::EType type)
	{
		switch (type)
		{
		case IVariable::FLOAT:
			return new CVariable<float>;
		case IVariable::INT:
			return new CVariable<int>;
		case IVariable::STRING:
			return new CVariable<string>;
		case IVariable::BOOL:
			return new CVariable<bool>;
		case IVariable::VECTOR:
			return new CVariable<Vec3>;
		case IVariable::QUAT:
			return new CVariable<Quat>;
		default:
			assert(0);
		}
		return NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	IVariable* CreateVar(const char* name, IVariable::EType defaultType, const char*& displayName)
	{
		displayName = name;
		// Resolve type from variable name.
		int nameLen = strlen(name);

		int nFlags = 0;
		int iStartIndex = 0;
		if (name[0] == '_')
		{
			nFlags |= IVariable::UI_INVISIBLE;
			iStartIndex++;
		}

		{
			// Try to detect type.
			for (int i = 0; i < CRY_ARRAY_COUNT(s_scriptParamTypes); i++)
			{
				if (IsPropertyTypeMatch(s_scriptParamTypes[i].prefix, name + iStartIndex, nameLen - iStartIndex, s_scriptParamTypes[i].bExactName))
				{
					displayName = name + strlen(s_scriptParamTypes[i].prefix) + iStartIndex;
					if (displayName[0] == '_')
					{
						displayName++;
					}
					if (displayName[0] == '\0')
					{
						displayName = name;
					}

					IVariable::EType type = s_scriptParamTypes[i].type;
					IVariable::EDataType dataType = s_scriptParamTypes[i].dataType;

					IVariable* var;
					if (type == IVariable::STRING &&
					    (dataType == IVariable::DT_AITERRITORY || dataType == IVariable::DT_AIWAVE))
					{
						var = new CVariableEnum<string>;
					}
					else
					{
						var = CreateVarByType(type);
					}

					if (!var)
					{
						continue;
					}

					var->SetName(name);
					var->SetHumanName(displayName);
					var->SetDataType(s_scriptParamTypes[i].dataType);
					var->SetFlags(nFlags);
					if (s_scriptParamTypes[i].flags & SCRIPTPARAM_POSITIVE)
					{
						float lmin = 0, lmax = 10000;
						var->GetLimits(lmin, lmax);
						// set min Limit to 0 hard, to make it positive only value.
						var->SetLimits(0, lmax, true, false);
					}
					return var;
				}
			}
		}
		if (defaultType != IVariable::UNKNOWN)
		{
			IVariable* var = CreateVarByType(defaultType);
			var->SetName(name);
			return var;
		}
		return 0;
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void OnElementFound(int nIdx, ScriptVarType type)
	{
		/* ignore non string indexed values */
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void OnElementFound(const char* sName, ScriptVarType type)
	{
		if (sName && sName[0] != 0)
		{
			Variable var;
			var.name = sName;
			var.type = type;
			m_elements.push_back(var);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void Dump(IScriptTable* pObject, string sTablePath)
	{
		m_elements.reserve(20);
		pObject->Dump(this);
		typedef std::map<string, IVariablePtr, stl::less_stricmp<string>> NodesMap;
		NodesMap nodes;
		NodesMap listNodes;

		for (int i = 0; i < m_elements.size(); i++)
		{
			const char* sName = m_elements[i].name;
			ScriptVarType type = m_elements[i].type;

			const char* sDisplayName = sName;

			if (type == svtNumber)
			{
				float fVal;
				pObject->GetValue(sName, fVal);
				IVariable* var = CreateVar(sName, IVariable::FLOAT, sDisplayName);
				if (var)
				{
					var->Set(fVal);

					//use lua comment parser to read comment about limit settings
					float minVal, maxVal, stepVal;
					string desc;
					if (LuaCommentParser::GetInstance()->ParseComment(sTablePath, sName, &minVal, &maxVal, &stepVal, &desc))
					{
						var->SetLimits(minVal, maxVal, stepVal);
						var->SetDescription(desc);
					}

					nodes[sDisplayName] = var;
				}
			}
			else if (type == svtString)
			{
				const char* sVal;
				pObject->GetValue(sName, sVal);
				IVariable* var = CreateVar(sName, IVariable::STRING, sDisplayName);
				if (var)
				{
					var->Set(sVal);
					nodes[sDisplayName] = var;
				}
			}
			else if (type == svtBool)
			{
				bool val = false;
				pObject->GetValue(sName, val);
				IVariable* pVar = CreateVar(sName, IVariable::BOOL, sDisplayName);
				if (pVar)
				{
					pVar->Set(val);
					nodes[sDisplayName] = pVar;
				}
			}
			else if (type == svtFunction)
			{
				// Ignore functions.
			}
			else if (type == svtObject)
			{
				// Some Table.
				SmartScriptTable pTable(GetIEditorImpl()->GetSystem()->GetIScriptSystem(), true);
				if (pObject->GetValue(sName, pTable))
				{
					IVariable* var = CreateVar(sName, IVariable::UNKNOWN, sDisplayName);
					if (var && var->GetType() == IVariable::VECTOR)
					{
						nodes[sDisplayName] = var;
						float x, y, z;
						if (pTable->GetValue("x", x) && pTable->GetValue("y", y) && pTable->GetValue("z", z))
						{
							var->Set(Vec3(x, y, z));
						}
						else
						{
							pTable->GetAt(1, x);
							pTable->GetAt(2, y);
							pTable->GetAt(3, z);
							var->Set(Vec3(x, y, z));
						}
					}
					else
					{
						if (var)
						{
							var->Release();
						}

						var = new CVariableArray;
						var->SetName(sName);
						listNodes[sName] = var;

						CScriptPropertiesDump dump(m_varBlock, var);
						dump.Dump(*pTable, sTablePath + "." + string(sName));
					}
				}
			}
		}

		for (NodesMap::iterator nit = nodes.begin(); nit != nodes.end(); nit++)
		{
			if (m_parentVar)
			{
				m_parentVar->AddVariable(nit->second);
			}
			else
			{
				m_varBlock->AddVariable(nit->second);
			}
		}

		for (NodesMap::iterator nit1 = listNodes.begin(); nit1 != listNodes.end(); nit1++)
		{
			if (m_parentVar)
			{
				m_parentVar->AddVariable(nit1->second);
			}
			else
			{
				m_varBlock->AddVariable(nit1->second);
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CEntityScript::CEntityScript(IEntityClass* pClass)
{
	SetClass(pClass);
	m_bValid = false;
	m_haveEventsTable = false;
	m_visibilityMask = 0;

	m_nFlags = 0;
	m_pOnPropertyChangedFunc = 0;
}

//////////////////////////////////////////////////////////////////////////
CEntityScript::~CEntityScript()
{
	if (m_pOnPropertyChangedFunc)
		gEnv->pScriptSystem->ReleaseFunc(m_pOnPropertyChangedFunc);
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::SetClass(IEntityClass* pClass)
{
	assert(pClass);
	m_pClass = pClass;
	m_usable = !(pClass->GetFlags() & ECLF_INVISIBLE);
}

//////////////////////////////////////////////////////////////////////////
string CEntityScript::GetName() const
{
	return m_pClass->GetName();
}

//////////////////////////////////////////////////////////////////////////
string CEntityScript::GetFile() const
{
	if (IEntityScriptFileHandler* pScriptFileHandler = m_pClass->GetScriptFileHandler())
		return pScriptFileHandler->GetScriptFile();

	return m_pClass->GetScriptFile();
}

//////////////////////////////////////////////////////////////////////////
int CEntityScript::GetEventCount()
{
	if (IEntityEventHandler* pEventHandler = m_pClass->GetEventHandler())
		return pEventHandler->GetEventCount();

	return (int)m_events.size();
}

//////////////////////////////////////////////////////////////////////////
const char* CEntityScript::GetEvent(int i)
{
	if (IEntityEventHandler* pEventHandler = m_pClass->GetEventHandler())
	{
		IEntityEventHandler::SEventInfo info;
		pEventHandler->GetEventInfo(i, info);

		return info.name;
	}

	return m_events[i];
}

//////////////////////////////////////////////////////////////////////////
int CEntityScript::GetEmptyLinkCount()
{
	return (int)m_emptyLinks.size();
}

//////////////////////////////////////////////////////////////////////////
const string& CEntityScript::GetEmptyLink(int i)
{
	return m_emptyLinks[i];
}

//////////////////////////////////////////////////////////////////////////
bool CEntityScript::Load()
{
	m_bValid = true;

	if (strlen(m_pClass->GetScriptFile()) > 0)
	{
		if (m_pClass->LoadScript(false))
		{
			//Feed script file to the lua comment parser for processing
			LuaCommentParser::GetInstance()->OpenScriptFile(m_pClass->GetScriptFile());

			// If class have script parse this script.
			m_bValid = ParseScript();

			LuaCommentParser::GetInstance()->CloseScriptFile();
		}
		else
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Entity class %s failed to load, (script: %s could not be loaded)", m_pClass->GetName(), m_pClass->GetScriptFile());
	}
	else
	{
		// No script file: parse the script tables
		// which should have been specified by game code.
		ParseScript();
	}

	return m_bValid;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityScript::ParseScript()
{
	// Parse .lua file.
	IScriptSystem* script = GetIEditorImpl()->GetSystem()->GetIScriptSystem();

	SmartScriptTable pEntity(script, true);
	if (!script->GetGlobalValue(GetName(), pEntity))
	{
		return false;
	}

	m_bValid = true;

	pEntity->GetValue("OnPropertyChange", m_pOnPropertyChangedFunc);

	CScriptMethodsDump dump;
	pEntity->Dump(&dump);
	m_methods = dump.methods;
	m_events = dump.events;

	//! Sort methods and events.
	std::sort(m_methods.begin(), m_methods.end());
	std::sort(m_events.begin(), m_events.end());

	m_emptyLinks.clear();

	{
		// Normal properties.
		m_pDefaultProperties = 0;
		SmartScriptTable pProps(script, true);
		if (pEntity->GetValue(PROPERTIES_TABLE, pProps))
		{
			// Properties found in entity.
			m_pDefaultProperties = new CVarBlock;
			CScriptPropertiesDump dump(m_pDefaultProperties);
			dump.Dump(*pProps, PROPERTIES_TABLE);
		}
	}

	{
		// Second set of properties.
		m_pDefaultProperties2 = 0;
		SmartScriptTable pProps(script, true);
		if (pEntity->GetValue(PROPERTIES2_TABLE, pProps))
		{
			// Properties found in entity.
			m_pDefaultProperties2 = new CVarBlock;
			CScriptPropertiesDump dump(m_pDefaultProperties2);
			dump.Dump(*pProps, PROPERTIES2_TABLE);
		}
	}

	// Destroy variable block if empty.
	/*if ( m_pDefaultProperties && m_pDefaultProperties->IsEmpty() )
	   {
	   m_pDefaultProperties = nullptr;
	   }

	   // Destroy variable block if empty.
	   if ( m_pDefaultProperties2 && m_pDefaultProperties2->IsEmpty() )
	   {
	   m_pDefaultProperties2 = nullptr;
	   }*/

	if (m_pDefaultProperties != 0 && m_pDefaultProperties->GetNumVariables() < 1)
	{
		m_pDefaultProperties = 0;
	}

	// Destroy variable block if empty.
	if (m_pDefaultProperties2 != 0 && m_pDefaultProperties2->GetNumVariables() < 1)
	{
		m_pDefaultProperties2 = 0;
	}

	// Load visual object.
	SmartScriptTable pEditorTable(script, true);
	if (pEntity->GetValue("Editor", pEditorTable))
	{
		SEditorClassInfo classInfo = m_pClass->GetEditorClassInfo();

		const char* modelName;
		if (pEditorTable->GetValue("Model", modelName))
		{
			classInfo.sHelper = modelName;
		}

		bool bShowBounds = false;
		pEditorTable->GetValue("ShowBounds", bShowBounds);
		if (bShowBounds)
		{
			m_nFlags |= ENTITY_SCRIPT_SHOWBOUNDS;
		}
		else
		{
			m_nFlags &= ~ENTITY_SCRIPT_SHOWBOUNDS;
		}

		bool isScalable = true;
		pEditorTable->GetValue("IsScalable", isScalable);
		if (!isScalable)
		{
			m_nFlags |= ENTITY_SCRIPT_ISNOTSCALABLE;
		}
		else
		{
			m_nFlags &= ~ENTITY_SCRIPT_ISNOTSCALABLE;
		}

		bool isRotatable = true;
		pEditorTable->GetValue("IsRotatable", isRotatable);
		if (!isRotatable)
		{
			m_nFlags |= ENTITY_SCRIPT_ISNOTROTATABLE;
		}
		else
		{
			m_nFlags &= ~ENTITY_SCRIPT_ISNOTROTATABLE;
		}

		bool bAbsoluteRadius = false;
		pEditorTable->GetValue("AbsoluteRadius", bAbsoluteRadius);
		if (bAbsoluteRadius)
		{
			m_nFlags |= ENTITY_SCRIPT_ABSOLUTERADIUS;
		}
		else
		{
			m_nFlags &= ~ENTITY_SCRIPT_ABSOLUTERADIUS;
		}

		const char* iconName = "";
		if (pEditorTable->GetValue("Icon", iconName))
		{
			classInfo.sIcon = PathUtil::Make("%EDITOR%/ObjectIcons", iconName);
		}

		classInfo.bIconOnTop = false;
		pEditorTable->GetValue("IconOnTop", classInfo.bIconOnTop);

		bool bArrow = false;
		pEditorTable->GetValue("DisplayArrow", bArrow);
		if (bArrow)
		{
			m_nFlags |= ENTITY_SCRIPT_DISPLAY_ARROW;
		}
		else
		{
			m_nFlags &= ~ENTITY_SCRIPT_DISPLAY_ARROW;
		}

		SmartScriptTable pLinksTable(script, true);
		if (pEditorTable->GetValue("Links", pLinksTable))
		{
			IScriptTable::Iterator iter = pLinksTable->BeginIteration();
			while (pLinksTable->MoveNext(iter))
			{
				if (iter.value.GetType() == EScriptAnyType::String)
				{
					const char* sLinkName = iter.value.GetString();
					m_emptyLinks.push_back(sLinkName);
				}
			}
			pLinksTable->EndIteration(iter);
		}

		const char* editorPath = "";
		if (pEditorTable->GetValue("EditorPath", editorPath))
		{
			m_userPath = editorPath;
		}

		m_pClass->SetEditorClassInfo(classInfo);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::Reload()
{
	if (IEntityScriptFileHandler* pScriptFileHandler = m_pClass->GetScriptFileHandler())
		pScriptFileHandler->ReloadScriptFile();

	bool bReloaded = false;
	IEntityScript* pScript = m_pClass->GetIEntityScript();
	if (pScript)
	{
		// First try compiling script and see if it have any errors.
		bool bLoadScript = CFileUtil::CompileLuaFile(m_pClass->GetScriptFile());
		if (bLoadScript)
		{
			bReloaded = m_pClass->LoadScript(true);
		}
	}

	if (bReloaded)
	{
		// Script compiled successfully.
		Load();
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::GotoMethod(const string& method)
{
	string line;
	line.Format("%s:%s", (const char*)GetName(), (const char*)method);

	// Search this line in script file.
	int lineNum = FindLineNum(line);
	if (lineNum >= 0)
	{
		// Open UltraEdit32 with this line.
		CFileUtil::EditTextFile(GetFile(), lineNum);
	}
}

void CEntityScript::AddMethod(const string& method)
{
	// Add a new method to the file. and start Editing it.
	FILE* f = fopen(GetFile(), "at");
	if (f)
	{
		fprintf(f, "\n");
		fprintf(f, "-------------------------------------------------------\n");
		fprintf(f, "function %s:%s()\n", (const char*)m_pClass->GetName(), (const char*)method);
		fprintf(f, "end\n");
		fclose(f);
	}
}

const string& CEntityScript::GetDisplayPath()
{
	if (m_userPath.IsEmpty())
	{
		IScriptSystem* script = GetIEditorImpl()->GetSystem()->GetIScriptSystem();

		SmartScriptTable pEntity(script, true);
		if (!script->GetGlobalValue(GetName(), pEntity))
		{
			m_userPath = "Default/" + GetName();
			return m_userPath;
		}

		SmartScriptTable pEditorTable(script, true);
		if (pEntity->GetValue("Editor", pEditorTable))
		{
			const char* editorPath = "";
			if (pEditorTable->GetValue("EditorPath", editorPath))
			{
				m_userPath = editorPath;
			}
		}
	}

	return m_userPath;
}

//////////////////////////////////////////////////////////////////////////
int CEntityScript::FindLineNum(const string& line)
{
	FILE* fileHandle = fopen(GetFile(), "rb");
	if (!fileHandle)
	{
		return -1;
	}

	int lineFound = -1;
	int lineNum = 1;

	fseek(fileHandle, 0, SEEK_END);
	int size = ftell(fileHandle);
	fseek(fileHandle, 0, SEEK_SET);

	char* text = new char[size + 16];
	fread(text, size, 1, fileHandle);
	text[size] = 0;

	char* token = strtok(text, "\n");
	while (token)
	{
		if (strstr(token, line) != 0)
		{
			lineFound = lineNum;
			break;
		}
		token = strtok(NULL, "\n");
		lineNum++;
	}

	fclose(fileHandle);
	delete[]text;

	return lineFound;
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::CopyPropertiesToScriptTable(IEntity* pEntity, CVarBlock* pVarBlock, bool bCallUpdate)
{
	CopyPropertiesToScriptInternal(pEntity, pVarBlock, bCallUpdate, PROPERTIES_TABLE);
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::CopyProperties2ToScriptTable(IEntity* pEntity, CVarBlock* pVarBlock, bool bCallUpdate)
{
	CopyPropertiesToScriptInternal(pEntity, pVarBlock, bCallUpdate, PROPERTIES2_TABLE);
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::CopyPropertiesToScriptInternal(IEntity* pEntity, CVarBlock* pVarBlock, bool bCallUpdate, const char* tableKey)
{
	if (!IsValid())
	{
		return;
	}

	assert(pEntity != 0);
	assert(pVarBlock != 0);

	IScriptTable* scriptTable = pEntity->GetScriptTable();
	if (!scriptTable)
	{
		// ENTITY TODO: Don't create CEntityScript in this case
		return;
	}

	IScriptSystem* pScriptSystem = GetIEditorImpl()->GetSystem()->GetIScriptSystem();

	SmartScriptTable table(pScriptSystem, true);
	if (!scriptTable->GetValue(tableKey, table))
	{
		return;
	}

	for (int i = 0; i < pVarBlock->GetNumVariables(); i++)
	{
		VarToScriptTable(pVarBlock->GetVariable(i), table);
	}

	if (bCallUpdate)
	{
		CallOnPropertyChange(pEntity);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::CallOnPropertyChange(IEntity* pEntity)
{
	if (!IsValid())
	{
		return;
	}

	if (m_pOnPropertyChangedFunc)
	{
		assert(pEntity != 0);
		IScriptTable* pScriptObject = pEntity->GetScriptTable();
		if (!pScriptObject)
			return;
		Script::CallMethod(pScriptObject, m_pOnPropertyChangedFunc);
	}

	SEntityEvent entityEvent(ENTITY_EVENT_EDITOR_PROPERTY_CHANGED);
	pEntity->SendEvent(entityEvent);
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::VarToScriptTable(IVariable* pVariable, IScriptTable* pScriptTable)
{
	assert(pVariable);

	IScriptSystem* pScriptSystem = GetIEditorImpl()->GetSystem()->GetIScriptSystem();

	if (pVariable->GetType() == IVariable::ARRAY)
	{
		int type = pScriptTable->GetValueType(pVariable->GetName());
		if (type != svtObject)
		{
			return;
		}

		SmartScriptTable table(pScriptSystem, true);
		if (pScriptTable->GetValue(pVariable->GetName(), table))
		{
			for (int i = 0; i < pVariable->GetNumVariables(); i++)
			{
				IVariable* child = pVariable->GetVariable(i);
				VarToScriptTable(child, table);
			}
		}
		return;
	}

	const char* name = pVariable->GetName();
	int type = pScriptTable->GetValueType(name);

	if (type == svtString)
	{
		string value;
		pVariable->Get(value);
		pScriptTable->SetValue(name, (const char*)value);
	}
	else if (type == svtNumber)
	{
		float val = 0;
		pVariable->Get(val);
		pScriptTable->SetValue(name, val);
	}
	else if (type == svtBool)
	{
		bool val = false;
		pVariable->Get(val);
		pScriptTable->SetValue(name, val);
	}
	else if (type == svtObject)
	{
		// Probably Color/Vector.
		SmartScriptTable table(pScriptSystem, true);
		if (pScriptTable->GetValue(name, table))
		{
			if (pVariable->GetType() == IVariable::VECTOR)
			{
				Vec3 vec;
				pVariable->Get(vec);

				float temp;
				if (table->GetValue("x", temp))
				{
					// Named vector.
					table->SetValue("x", vec.x);
					table->SetValue("y", vec.y);
					table->SetValue("z", vec.z);
				}
				else
				{
					// Indexed vector.
					table->SetAt(1, vec.x);
					table->SetAt(2, vec.y);
					table->SetAt(3, vec.z);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::CopyPropertiesFromScriptTable(IEntity* pEntity, CVarBlock* pVarBlock)
{
	CopyPropertiesFromScriptInternal(pEntity, pVarBlock, PROPERTIES_TABLE);
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::CopyProperties2FromScriptTable(IEntity* pEntity, CVarBlock* pVarBlock)
{
	CopyPropertiesFromScriptInternal(pEntity, pVarBlock, PROPERTIES2_TABLE);
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::CopyPropertiesFromScriptInternal(IEntity* pEntity, CVarBlock* pVarBlock, const char* tableKey)
{
	if (!IsValid())
	{
		return;
	}

	assert(pEntity != 0);
	assert(pVarBlock != 0);

	IScriptTable* scriptTable = pEntity->GetScriptTable();
	if (!scriptTable)
	{
		return;
	}

	IScriptSystem* pScriptSystem = GetIEditorImpl()->GetSystem()->GetIScriptSystem();

	SmartScriptTable table(pScriptSystem, true);
	if (!scriptTable->GetValue(tableKey, table))
	{
		return;
	}

	for (int i = 0; i < pVarBlock->GetNumVariables(); i++)
	{
		ScriptTableToVar(table, pVarBlock->GetVariable(i));
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::ScriptTableToVar(IScriptTable* pScriptTable, IVariable* pVariable)
{
	assert(pVariable);

	IScriptSystem* pScriptSystem = GetIEditorImpl()->GetSystem()->GetIScriptSystem();

	if (pVariable->GetType() == IVariable::ARRAY)
	{
		int type = pScriptTable->GetValueType(pVariable->GetName());
		if (type != svtObject)
		{
			return;
		}

		SmartScriptTable table(pScriptSystem, true);
		if (pScriptTable->GetValue(pVariable->GetName(), table))
		{
			for (int i = 0; i < pVariable->GetNumVariables(); i++)
			{
				IVariable* child = pVariable->GetVariable(i);
				ScriptTableToVar(table, child);
			}
		}
		return;
	}

	const char* name = pVariable->GetName();
	int type = pScriptTable->GetValueType(name);

	if (type == svtString)
	{
		const char* value;
		pScriptTable->GetValue(name, value);
		pVariable->Set(value);
	}
	else if (type == svtNumber)
	{
		float val = 0;
		pScriptTable->GetValue(name, val);
		pVariable->Set(val);
	}
	else if (type == svtBool)
	{
		bool val = false;
		pScriptTable->GetValue(name, val);
		pVariable->Set(val);
	}
	else if (type == svtObject)
	{
		// Probably Color/Vector.
		SmartScriptTable table(pScriptSystem, true);
		if (pScriptTable->GetValue(name, table))
		{
			if (pVariable->GetType() == IVariable::VECTOR)
			{
				Vec3 vec;

				float temp;
				if (table->GetValue("x", temp))
				{
					// Named vector.
					table->GetValue("x", vec.x);
					table->GetValue("y", vec.y);
					table->GetValue("z", vec.z);
				}
				else
				{
					// Indexed vector.
					table->GetAt(1, vec.x);
					table->GetAt(2, vec.y);
					table->GetAt(3, vec.z);
				}

				pVariable->Set(vec);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::RunMethod(IEntity* pEntity, const string& method)
{
	if (!IsValid())
	{
		return;
	}

	assert(pEntity != 0);

	IScriptTable* scriptTable = pEntity->GetScriptTable();
	if (!scriptTable)
	{
		return;
	}

	IScriptSystem* scriptSystem = GetIEditorImpl()->GetSystem()->GetIScriptSystem();

	Script::CallMethod(scriptTable, (const char*)method);
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::SendEvent(IEntity* pEntity, const string& method)
{
	if (IEntityEventHandler* pEventHandler = m_pClass->GetEventHandler())
		pEventHandler->SendEvent(pEntity, method);

	// Fire event on Entity.
	IEntityScriptComponent* pScriptProxy = (IEntityScriptComponent*)pEntity->GetProxy(ENTITY_PROXY_SCRIPT);
	if (pScriptProxy)
	{
		pScriptProxy->CallEvent(method);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::SetEventsTable(CEntityObject* pEntity)
{
	if (!IsValid())
	{
		return;
	}
	assert(pEntity != 0);

	IEntity* ientity = pEntity->GetIEntity();
	if (!ientity)
	{
		return;
	}

	IScriptTable* scriptTable = ientity->GetScriptTable();
	if (!scriptTable)
	{
		return;
	}

	// If events target table is null, set event table to null either.
	if (pEntity->GetEventTargetCount() == 0)
	{
		if (m_haveEventsTable)
		{
			scriptTable->SetToNull("Events");
		}
		m_haveEventsTable = false;
		return;
	}

	IScriptSystem* scriptSystem = GetIEditorImpl()->GetSystem()->GetIScriptSystem();
	SmartScriptTable pEvents(scriptSystem, false);

	scriptTable->SetValue("Events", *pEvents);
	m_haveEventsTable = true;

	std::set<string> sourceEvents;
	for (int i = 0; i < pEntity->GetEventTargetCount(); i++)
	{
		CEntityEventTarget& et = pEntity->GetEventTarget(i);
		sourceEvents.insert(et.sourceEvent);
	}
	for (std::set<string>::iterator it = sourceEvents.begin(); it != sourceEvents.end(); it++)
	{
		SmartScriptTable pTrgEvents(scriptSystem, false);
		string sourceEvent = *it;

		pEvents->SetValue(sourceEvent, *pTrgEvents);

		// Put target events to table.
		int trgEventIndex = 1;
		for (int i = 0; i < pEntity->GetEventTargetCount(); i++)
		{
			CEntityEventTarget& et = pEntity->GetEventTarget(i);
			if (stricmp(et.sourceEvent, sourceEvent) == 0)
			{
				EntityId entityId = 0;
				if (et.target)
				{
					if (et.target->IsKindOf(RUNTIME_CLASS(CEntityObject)))
					{
						entityId = ((CEntityObject*)et.target)->GetEntityId();
					}
				}

				SmartScriptTable pTrgEvent(scriptSystem, false);
				pTrgEvents->SetAt(trgEventIndex, *pTrgEvent);
				trgEventIndex++;

				ScriptHandle idHandle;
				idHandle.n = entityId;

				pTrgEvent->SetAt(1, idHandle);
				pTrgEvent->SetAt(2, (const char*)et.event);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::UpdateTextureIcon(IEntity* pEntity)
{
	if (pEntity)
	{
		// Try to call a function GetEditorIcon on the script, to give it a change to return
		// a custom icon.
		IScriptTable* pScriptTable = pEntity->GetScriptTable();
		if (pScriptTable != NULL && pScriptTable->GetValueType("GetEditorIcon") == svtFunction)
		{
			SmartScriptTable iconData(gEnv->pScriptSystem);
			if (Script::CallMethod(pScriptTable, "GetEditorIcon", iconData))
			{
				const char* iconName = NULL;
				iconData->GetValue("Icon", iconName);

				SEditorClassInfo classInfo = m_pClass->GetEditorClassInfo();
				classInfo.sIcon = PathUtil::Make("%EDITOR%/ObjectIcons", iconName);
				m_pClass->SetEditorClassInfo(classInfo);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// CEntityScriptRegistry implementation.
//////////////////////////////////////////////////////////////////////////
CEntityScriptRegistry* CEntityScriptRegistry::m_instance = 0;

CEntityScriptRegistry::CEntityScriptRegistry() :
	m_needsScriptReload(false)
{
	gEnv->pEntitySystem->GetClassRegistry()->RegisterListener(this);
	GetIEditorImpl()->RegisterNotifyListener(this);
}

CEntityScriptRegistry::~CEntityScriptRegistry()
{
	m_instance = 0;
	m_scripts.clear();
	gEnv->pEntitySystem->GetClassRegistry()->UnregisterListener(this);
	GetIEditorImpl()->UnregisterNotifyListener(this);
}

//////////////////////////////////////////////////////////////////////////
void CEntityScriptRegistry::OnEntityClassRegistryEvent(EEntityClassRegistryEvent event, const IEntityClass* pEntityClass)
{
	switch (event)
	{
	case ECRE_CLASS_REGISTERED:
	case ECRE_CLASS_MODIFIED:
		{
			auto pEntityScript = Find(pEntityClass->GetName());
			if (pEntityScript != nullptr)
			{
				pEntityScript->SetClass(const_cast<IEntityClass*>(pEntityClass));
			}
			else
			{
				pEntityScript = Insert(const_cast<IEntityClass*>(pEntityClass));
			}

			if (pEntityScript)
			{
				if (event == ECRE_CLASS_REGISTERED)
				{
					m_scriptAdded(pEntityScript.get());
				}
				else
				{
					m_scriptChanged(pEntityScript.get());
				}
			}

				m_needsScriptReload = true;
		}
		break;
	case ECRE_CLASS_UNREGISTERED:
		{
			auto pEntityScript = Find(pEntityClass->GetName());

			m_scriptRemoved(pEntityScript.get());
			m_scripts.erase(pEntityClass->GetName());
		}
		break;
	}
}

void CEntityScriptRegistry::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	if (event == eNotify_OnIdleUpdate && m_needsScriptReload)
	{
		m_needsScriptReload = false;
		CCryEditApp::GetInstance()->OnReloadEntityScripts();
	}
}

std::shared_ptr<CEntityScript> CEntityScriptRegistry::Find(const char* szName) const
{
	auto it = m_scripts.find(szName);
	if (it != m_scripts.end())
	{
		return it->second;
	}

	return nullptr;
}

std::shared_ptr<CEntityScript> CEntityScriptRegistry::Insert(IEntityClass* pClass, const char* szAlias)
{
	if (strlen(szAlias) == 0)
	{
		szAlias = pClass->GetName();
	}

	auto scriptIt = m_scripts.find(szAlias);

	// Check if inserting already exist script, if so ignore.
	if (scriptIt != m_scripts.end())
	{
		Error("Inserting duplicate Entity Script %s", szAlias);
		return scriptIt->second;
	}

	scriptIt = m_scripts.emplace(std::make_pair(string(szAlias), std::make_shared<CEntityScript>(pClass))).first;

	SetClassCategory(scriptIt->second.get());
	return scriptIt->second;
}

void CEntityScriptRegistry::SetClassCategory(CEntityScript* script)
{
	IScriptSystem* scriptSystem = GetIEditorImpl()->GetSystem()->GetIScriptSystem();

	SmartScriptTable pEntity(scriptSystem, true);
	if (!scriptSystem->GetGlobalValue(script->GetName(), pEntity))
		return;

	SmartScriptTable pEditor(scriptSystem, true);
	if (pEntity->GetValue("Editor", pEditor))
	{
		const char* clsCategory;
		if (pEditor->GetValue("Category", clsCategory))
		{
			SEditorClassInfo editorClassInfo(script->GetClass()->GetEditorClassInfo());
			editorClassInfo.sCategory = clsCategory;
			script->GetClass()->SetEditorClassInfo(editorClassInfo);
		}
	}
}

void CEntityScriptRegistry::IterateOverScripts(std::function<void(CEntityScript&)> callback)
{
	for (auto it = m_scripts.begin(); it != m_scripts.end(); ++it)
	{
		callback(*it->second);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityScriptRegistry::Reload()
{
	IEntityClassRegistry* pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();
	pClassRegistry->LoadClasses("entities.xml", true);
	LoadScripts();
}

//////////////////////////////////////////////////////////////////////////
void CEntityScriptRegistry::LoadScripts()
{
	m_scripts.clear();

	IEntityClassRegistry* pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();
	IEntityClass* pClass = NULL;
	pClassRegistry->IteratorMoveFirst();
	while (pClass = pClassRegistry->IteratorNext())
	{
		Insert(pClass);
	}
}

//////////////////////////////////////////////////////////////////////////
CEntityScriptRegistry* CEntityScriptRegistry::Instance()
{
	if (!m_instance)
	{
		m_instance = new CEntityScriptRegistry;
	}
	return m_instance;
}

//////////////////////////////////////////////////////////////////////////
void CEntityScriptRegistry::Release()
{
	if (m_instance)
	{
		delete m_instance;
	}
	m_instance = 0;
}


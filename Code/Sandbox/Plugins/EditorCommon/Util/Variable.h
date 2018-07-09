// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __Variable_h__
#define __Variable_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryCore/smartptr.h>
#include <CryCore/functor.h>
#include <CrySerialization/Forward.h>

typedef Functor4<uint32, bool, const char*, const char*> TMissingAssetResolveCallback;

inline const char* to_c_str(const char* str)    { return str; }
inline const char* to_c_str(const string& str) { return str; }
#define MAX_VAR_STRING_LENGTH 4096

struct IVarEnumList;
struct ISplineInterpolator;
class CUsedResources;
struct IVariable;

// Temporarily hide warnings about virtual override hidden
// These happen because we declare "Serialize" method that hides MFC's CObject::Serialize
#pragma warning(push)
#pragma warning(disable:4264)
#pragma warning(disable:4266)

/** IVariableContainer
 *  Interface for all classes that hold child variables
 */
struct EDITOR_COMMON_API IVariableContainer : public _i_reference_target_t
{
	//! Add child variable
	virtual void AddVariable(IVariable* var) = 0;

	//! Delete specific variable
	virtual bool DeleteVariable(IVariable* var, bool recursive = false) = 0;

	//! Delete all variables
	virtual void DeleteAllVariables() = 0;

	//! Gets number of variables
	virtual int GetNumVariables() const = 0;

	//! Get variable at index
	virtual IVariable* GetVariable(int index) const = 0;

	//! Returns true if var block contains specified variable.
	virtual bool IsContainsVariable(IVariable* pVar, bool bRecursive = false) const = 0;

	//! Find variable by name.
	virtual IVariable* FindVariable(const char* name, bool bRecursive = false, bool bHumanName = false) const = 0;

	//! Return true if variable block is empty (Does not have any vars).
	virtual bool IsEmpty() const = 0;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
/** IVariable is the variant variable interface.
 */
struct EDITOR_COMMON_API IVariable : public IVariableContainer
{
	/** Type of data stored in variable.
	 */
	enum EType
	{
		UNKNOWN,  //!< Unknown parameter type.
		INT,      //!< Integer property.
		BOOL,     //!< Boolean property.
		FLOAT,    //!< Float property.
		VECTOR2,  //!< Vector property.
		VECTOR,   //!< Vector property.
		VECTOR4,  //!< Vector property.
		QUAT,     //!< Quaternion property.
		STRING,   //!< String property.
		ARRAY     //!< Array of parameters.
	};

	//! Type of data hold by variable.
	enum EDataType
	{
		DT_SIMPLE = 0, //!< Standard param type.
		DT_BOOLEAN,
		DT_PERCENT,   //!< Percent data type, (Same as simple but value is from 0-1 and UI will be from 0-100).
		DT_COLOR,
		DT_ANGLE,
		DT_FILE,
		DT_TEXTURE,
		DT_ANIMATION,
		DT_OBJECT,
		DT_SHADER,
		DT_AI_BEHAVIOR,
		DT_AI_ANCHOR,
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
		DT_AI_CHARACTER,
#endif
		DT_AI_PFPROPERTIESLIST,
		DT_AIENTITYCLASSES,
		DT_AITERRITORY,
		DT_AIWAVE,
		DT_LOCAL_STRING,
		DT_EQUIP,
		DT_REVERBPRESET,
		DT_MATERIAL,
		DT_MATERIALLOOKUP,
		DT_SOCLASS,        // Smart Object Class
		DT_SOCLASSES,      // Smart Object Classes
		DT_SOSTATE,        // Smart Object State
		DT_SOSTATES,       // Smart Object States
		DT_SOSTATEPATTERN, // Smart Object State Pattern
		DT_SOACTION,       // Smart Object Action
		DT_SOHELPER,       // Smart Object Helper
		DT_SONAVHELPER,    // Smart Object Navigation Helper
		DT_SOANIMHELPER,   // Smart Object Animation Helper
		DT_SOEVENT,        // Smart Object Event
		DT_SOTEMPLATE,     // Smart Object Template
		DT_CUSTOMACTION,   // Custom Action
		DT_EXTARRAY,       // Extendable Array
		DT_VEEDHELPER,     // Vehicle Helper
		DT_VEEDPART,       // Vehicle Part
		DT_VEEDCOMP,       // Vehicle Component
		DT_GAMETOKEN,      // Game Token
		DT_SEQUENCE,       // Movie Sequence (DEPRECATED, use DT_SEQUENCE_ID, instead.)
		DT_MISSIONOBJ,     // Mission Objective
		DT_USERITEMCB,     // Use a callback GetItemsCallback in user data of variable
		DT_UIENUM,         // Edit as enum, uses CUIEnumsDatabase to lookup the enum to value pairs and combobox in GUI.
		DT_SEQUENCE_ID,    // Movie Sequence
		DT_LIGHT_ANIMATION,// Light Animation Node in the global Light Animation Set
		DT_PARTICLE_EFFECT,
		DT_GEOM_CACHE, // Geometry cache
		DT_FLARE,
		DT_AUDIO_TRIGGER,
		DT_AUDIO_SWITCH,
		DT_AUDIO_SWITCH_STATE,
		DT_AUDIO_RTPC,
		DT_AUDIO_ENVIRONMENT,
		DT_AUDIO_PRELOAD_REQUEST,
		DT_DYNAMIC_RESPONSE_SIGNAL,
		DT_CURVE = BIT(7),  // Combined with other types
	};

	// Flags that can used with variables.
	enum EFlags
	{
		// User interface related flags.
		UI_DISABLED          = BIT(0),  //!< This variable will be disabled in UI.
		UI_BOLD              = BIT(1),  //!< Variable name in properties will be bold.
		UI_SHOW_CHILDREN     = BIT(2),  //!< Display children in parent field.
		UI_USE_GLOBAL_ENUMS  = BIT(3),  //!< Use CUIEnumsDatabase to fetch enums for this variable name.
		UI_INVISIBLE         = BIT(4),  //!< This variable will not be displayed in the UI
		UI_ROLLUP2           = BIT(5),  //!< Prefer right roll-up bar for extended property control.
		UI_COLLAPSED         = BIT(6),  //!< Category collapsed by default.
		UI_UNSORTED          = BIT(7),  //!< Do not sort list-box alphabetically.
		UI_EXPLICIT_STEP     = BIT(8),  //!< Use the step size set in the variable for the UI directly.
		UI_NOT_DISPLAY_VALUE = BIT(9),  //!< The UI will display an empty string where it would normally draw the variable value.
		UI_HIGHLIGHT_EDITED  = BIT(10), //!< Edited (non-default value) properties show highlight color.
	};

	typedef Functor1<IVariable*> OnSetCallback;

	// Store IGetCustomItems into IVariable's UserData and set datatype to
	// DT_USERITEMCB
	// RefCounting is NOT handled by IVariable!
	struct IGetCustomItems : public _i_reference_target_t
	{
		struct SItem
		{
			SItem() {}
			SItem(const string& name, const string& desc = "") : name(name), desc(desc) {}
			SItem(const char* name, const char* desc = "") : name(name), desc(desc) {}
			CString name;
			CString desc;
		};
		virtual bool        GetItems(IVariable* /* pVar */, std::vector<SItem>& /* items */, string& /* outDialogTitle */) = 0;
		virtual bool        UseTree() = 0;
		virtual const char* GetTreeSeparator() = 0;
	};

	//! Get name of parameter.
	virtual const char* GetName() const = 0;
	//! Set name of parameter.
	virtual void        SetName(const string& name) = 0;

	virtual void        SetLegacyName(const string& name) = 0;

	//! Get human readable name of parameter (Normally same as name).
	virtual const char* GetHumanName() = 0;
	//! Set human readable name of parameter (name without prefix).
	virtual void        SetHumanName(const string& name) = 0;
	virtual void        SetHumanName(const char* name) { SetHumanName(string(name)); }

	//! Get variable description.
	virtual const char* GetDescription() const = 0;
	//! Set variable description.
	virtual void        SetDescription(const char* desc) = 0;

	//! Get parameter type.
	virtual EType GetType() const = 0;
	//! Get size of parameter.
	virtual int   GetSize() const = 0;

	//! Type of data stored in this variable.
	virtual unsigned char GetDataType() const = 0;
	virtual void          SetDataType(unsigned char dataType) = 0;

	virtual void          SetUserData(void* pData) = 0;
	virtual void*         GetUserData() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Flags
	//////////////////////////////////////////////////////////////////////////
	//! Set variable flags, (Limited to 16 flags).
	virtual void SetFlags(int flags) = 0;
	virtual int  GetFlags() const = 0;

	/////////////////////////////////////////////////////////////////////////////
	// Set methods.
	/////////////////////////////////////////////////////////////////////////////
	virtual void Set(int value) = 0;
	virtual void Set(bool value) = 0;
	virtual void Set(float value) = 0;
	virtual void Set(const Vec2& value) = 0;
	virtual void Set(const Vec3& value) = 0;
	virtual void Set(const Vec4& value) = 0;
	virtual void Set(const Ang3& value) = 0;
	virtual void Set(const Quat& value) = 0;
	virtual void Set(const string& value) = 0;
	virtual void Set(const char* value) = 0;
	virtual void SetDisplayValue(const string& value) = 0;
	inline void  SetDisplayValue(const char* value) { SetDisplayValue(string(value)); }

	// Called when value updated by any means (including internally).
	virtual void OnSetValue(bool bRecursive) = 0;

	/////////////////////////////////////////////////////////////////////////////
	// Get methods.
	/////////////////////////////////////////////////////////////////////////////
	virtual void    Get(int& value) const = 0;
	virtual void    Get(bool& value) const = 0;
	virtual void    Get(float& value) const = 0;
	virtual void    Get(Vec2& value) const = 0;
	virtual void    Get(Vec3& value) const = 0;
	virtual void    Get(Vec4& value) const = 0;
	virtual void    Get(Ang3& value) const = 0;
	virtual void    Get(Quat& value) const = 0;
	virtual void    Get(string& value) const = 0;
	inline void     Get(CString& value) const // for CString conversion
	{
		string str;
		Get(str);
		value = str.GetString();
	}

	virtual string GetDisplayValue() const = 0;
	virtual bool    HasDefaultValue() const = 0;

	//! Return cloned value of variable.
	virtual IVariable* Clone(bool bRecursive) const = 0;

	//! Copy variable value from specified variable.
	//! This method executed always recursively on all sub hierarchy of variables,
	//! In Array vars, will never create new variables, only copy values of corresponding childs.
	//! @param fromVar Source variable to copy value from.
	virtual void CopyValue(IVariable* fromVar) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Value Limits.
	//////////////////////////////////////////////////////////////////////////
	//! Set value limits.
	virtual void SetLimits(float fMin, float fMax, float fStep = 0.f, bool bHardMin = true, bool bHardMax = true) {}
	//! Get value limits.
	virtual void GetLimits(float& fMin, float& fMax, float& fStep, bool& bHardMin, bool& bHardMax)                {}
	void         GetLimits(float& fMin, float& fMax)
	{
		float f;
		bool b;
		GetLimits(fMin, fMax, f, b, b);
	}

	virtual void EnableNotifyWithoutValueChange(bool bFlag) {}

	//////////////////////////////////////////////////////////////////////////
	// Wire/Unwire variables.
	//////////////////////////////////////////////////////////////////////////
	//! Wire variable, wired variable will be changed when this var changes.
	virtual void Wire(IVariable* targetVar) = 0;
	//! Unwire variable.
	virtual void Unwire(IVariable* targetVar) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Assign on set callback.
	//////////////////////////////////////////////////////////////////////////
	virtual void AddOnSetCallback(OnSetCallback func) = 0;
	virtual void RemoveOnSetCallback(OnSetCallback func) = 0;

	//////////////////////////////////////////////////////////////////////////
	//! Retrieve pointer to selection list used by variable.
	virtual IVarEnumList*        GetEnumList() const           { return 0; }
	virtual ISplineInterpolator* GetSpline(bool bCreate) const { return 0; }

	//////////////////////////////////////////////////////////////////////////
	//! Serialize variable to XML.
	//////////////////////////////////////////////////////////////////////////
	virtual void Serialize(XmlNodeRef node, bool load) = 0;

	// From CObject, (not implemented)
	virtual void Serialize(CArchive& ar) {}

	//////////////////////////////////////////////////////////////////////////
	// Disables the update callbacks for certain operations in order to avoid
	// too many function calls when not needed.
	//////////////////////////////////////////////////////////////////////////
	virtual void EnableUpdateCallbacks(bool boEnable) = 0;

	// Setup to true to force save Undo in case the value the same.
	virtual void SetForceModified(bool bForceModified) {}
};

// Smart pointer to this parameter.
typedef _smart_ptr<IVariable> IVariablePtr;

/**
 **************************************************************************************
 * CVariableBase implements IVariable interface and provide default implementation
 * for basic IVariable functionality (Name, Flags, etc...)
 * CVariableBase cannot be instantiated directly and should be used as the base class for
 * actual Variant implementation classes.
 ***************************************************************************************
 */
class EDITOR_COMMON_API CVariableBase : public IVariable
{
public:
	virtual ~CVariableBase() {}

	void        SetName(const string& name) { m_name = name; };
	//! Get name of parameter.
	const char* GetName() const              { return to_c_str(m_name); };

	void        SetLegacyName(const string& name) { m_legacyName = name; }

	const char* GetHumanName()
	{
		if (m_humanName.IsEmpty())
			SetHumanName(m_name);
		return m_humanName;
	}
	void          SetHumanName(const string& name);

	void          SetDescription(const char* desc)    { m_description = desc; };
	//! Get name of parameter.
	const char*   GetDescription() const              { return to_c_str(m_description); };

	EType         GetType() const                     { return IVariable::UNKNOWN; };
	int           GetSize() const                     { return sizeof(*this); };

	unsigned char GetDataType() const                 { return m_dataType; };
	void          SetDataType(unsigned char dataType) { m_dataType = dataType; }

	void          SetFlags(int flags)                 { m_flags = flags; }
	int           GetFlags() const                    { return m_flags; }

	void          SetUserData(void* pData)            { m_pUserData = pData; }
	void*         GetUserData()                       { return m_pUserData; }

	//////////////////////////////////////////////////////////////////////////
	// Set methods.
	//////////////////////////////////////////////////////////////////////////
	void Set(int value)                        { assert(0); }
	void Set(bool value)                       { assert(0); }
	void Set(float value)                      { assert(0); }
	void Set(const Vec2& value)                { assert(0); }
	void Set(const Vec3& value)                { assert(0); }
	void Set(const Vec4& value)                { assert(0); }
	void Set(const Ang3& value)                { assert(0); }
	void Set(const Quat& value)                { assert(0); }
	void Set(const string& value)             { assert(0); }
	void Set(const char* value)                { assert(0); }
	void SetDisplayValue(const string& value) { Set(value); }
	void SetDisplayValue(const char* value) { SetDisplayValue(string(value)); } // for CString conversion

	//////////////////////////////////////////////////////////////////////////
	// Get methods.
	//////////////////////////////////////////////////////////////////////////
	void    Get(int& value) const     { assert(0); }
	void    Get(bool& value) const    { assert(0); }
	void    Get(float& value) const   { assert(0); }
	void    Get(Vec2& value) const    { assert(0); }
	void    Get(Vec3& value) const    { assert(0); }
	void    Get(Vec4& value) const    { assert(0); }
	void    Get(Ang3& value) const    { assert(0); }
	void    Get(Quat& value) const    { assert(0); }
	void    Get(string& value) const { assert(0); }
	void	Get(CString& value) const // for CString conversion
	{
		string str;
		Get(str);
		value = str.GetString();
	}
	string GetDisplayValue() const   { string val; Get(val); return val; }

	//////////////////////////////////////////////////////////////////////////
	// IVariableContainer functions
	//////////////////////////////////////////////////////////////////////////
	virtual void       AddVariable(IVariable* var)                                                            { assert(0); }

	virtual bool       DeleteVariable(IVariable* var, bool recursive = false)                                 { return false; }
	virtual void       DeleteAllVariables()                                                                   {}

	virtual int        GetNumVariables() const                                                                { return 0; }
	virtual IVariable* GetVariable(int index) const                                                           { return nullptr; }

	virtual bool       IsContainsVariable(IVariable* pVar, bool bRecursive = false) const                     { return false; }

	virtual IVariable* FindVariable(const char* name, bool bRecursive = false, bool bHumanName = false) const { return nullptr; }

	virtual bool       IsEmpty() const                                                                        { return true; }

	//////////////////////////////////////////////////////////////////////////
	void Wire(IVariable* var)
	{
		m_wiredVars.push_back(var);
	}
	//////////////////////////////////////////////////////////////////////////
	void Unwire(IVariable* var)
	{
		if (!var)
		{
			// Unwire all.
			m_wiredVars.clear();
		}
		else
		{
			stl::find_and_erase(m_wiredVars, var);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void AddOnSetCallback(OnSetCallback func)
	{
		if (!stl::find(m_onSetFuncs, func))
		{
			m_onSetFuncs.push_back(func);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void RemoveOnSetCallback(OnSetCallback func)
	{
		stl::find_and_erase(m_onSetFuncs, func);
	}

	void OnSetValue(bool bRecursive)
	{
		// If have wired variables or OnSet callback, process them.
		// Send value to wired variable.
		for (CVariableBase::WiredList::iterator it = m_wiredVars.begin(); it != m_wiredVars.end(); ++it)
		{
			if (m_bForceModified)
			{
				(*it)->SetForceModified(true);
			}

			// Copy value to wired vars.
			(*it)->CopyValue(this);
		}

		if (!m_boUpdateCallbacksEnabled)
		{
			return;
		}

		// Call on set callback.
		for (OnSetCallbackList::iterator it = m_onSetFuncs.begin(); it != m_onSetFuncs.end(); ++it)
		{
			// Call on set callback.
			(*it)(this);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void Serialize(XmlNodeRef node, bool load)
	{
		if (load)
		{
			string data;
			if (node->getAttr(to_c_str(m_name), data) || node->getAttr(to_c_str(m_legacyName), data))
			{
				Set(data);
			}
		}
		else
		{
			// Saving.
			string str;
			Get(str);
			node->setAttr(to_c_str(m_name), to_c_str(str));
		}
	}

	virtual void EnableUpdateCallbacks(bool boEnable)  { m_boUpdateCallbacksEnabled = boEnable; };
	virtual void SetForceModified(bool bForceModified) { m_bForceModified = bForceModified; }
protected:
	// Constructor.
	CVariableBase()
	{
		m_dataType = DT_SIMPLE;
		m_flags = 0;
		m_pUserData = 0;
		m_boUpdateCallbacksEnabled = true;
		m_bForceModified = false;
	}
	// Copy constructor.
	CVariableBase(const CVariableBase& var)
	{
		m_name = var.m_name;
		m_humanName = var.m_humanName;
		SetHumanName(var.m_humanName);
		m_legacyName = var.m_legacyName;
		m_humanName = var.m_humanName;
		m_description = var.m_description;
		m_flags = var.m_flags;
		m_dataType = var.m_dataType;
		m_pUserData = var.m_pUserData;
		m_boUpdateCallbacksEnabled = true;
		m_bForceModified = var.m_bForceModified;
		// Never copy callback function or wired variables they are private to specific variable,
	}

protected:
	// Not allow.
	CVariableBase& operator=(const CVariableBase& var)
	{
		return *this;
	}

protected:
	//////////////////////////////////////////////////////////////////////////
	// Variables.
	//////////////////////////////////////////////////////////////////////////
	typedef std::vector<OnSetCallback> OnSetCallbackList;
	typedef std::vector<IVariablePtr>  WiredList;

	string m_name;
	string m_legacyName;

	string m_humanName;
	string m_description;

	//! Extended data (Extended data is never copied, it's always private to this variable).
	WiredList         m_wiredVars;
	OnSetCallbackList m_onSetFuncs;

	uint16            m_flags;
	//! Limited to 8 flags.
	unsigned char     m_dataType;

	//! Optional userdata pointer
	void* m_pUserData;

	bool  m_boUpdateCallbacksEnabled;

	bool  m_bForceModified;
};

/**
 **************************************************************************************
 * CVariableArray implements variable of type array of IVariables.
 ***************************************************************************************
 */
class EDITOR_COMMON_API CVariableArray : public CVariableBase
{
public:
	//! Get name of parameter.
	virtual EType GetType() const { return IVariable::ARRAY; };
	virtual int   GetSize() const { return sizeof(CVariableArray); };

	//////////////////////////////////////////////////////////////////////////
	// Set methods.
	//////////////////////////////////////////////////////////////////////////
	virtual void Set(const string& value)
	{
		if (m_strValue != value)
		{
			m_strValue = value;
			OnSetValue(false);
		}
	}
	virtual void Set(const char* value) // for CString conversion
	{
		Set(string(value));
	}
	void OnSetValue(bool bRecursive)
	{
		CVariableBase::OnSetValue(bRecursive);
		if (bRecursive)
		{
			for (Variables::iterator it = m_vars.begin(); it != m_vars.end(); ++it)
			{
				(*it)->OnSetValue(true);
			}
		}
	}
	//////////////////////////////////////////////////////////////////////////
	// Get methods.
	//////////////////////////////////////////////////////////////////////////
	virtual void Get(string& value) const { value = m_strValue; }
	virtual void Get(CString& value) const { value = m_strValue.GetString(); }

	virtual bool HasDefaultValue() const
	{
		for (Variables::const_iterator it = m_vars.begin(); it != m_vars.end(); ++it)
			if (!(*it)->HasDefaultValue())
				return false;
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	IVariable* Clone(bool bRecursive) const
	{
		CVariableArray* var = new CVariableArray(*this);

		// m_vars was shallow-duplicated, clone elements.
		for (int i = 0; i < m_vars.size(); i++)
		{
			var->m_vars[i] = m_vars[i]->Clone(bRecursive);
		}
		return var;
	}

	//////////////////////////////////////////////////////////////////////////
	void CopyValue(IVariable* fromVar)
	{
		assert(fromVar);
		if (fromVar->GetType() != IVariable::ARRAY)
			return;
		int numSrc = fromVar->GetNumVariables();
		int numTrg = m_vars.size();
		for (int i = 0; i < numSrc && i < numTrg; i++)
		{
			// Copy Every child variable.
			m_vars[i]->CopyValue(fromVar->GetVariable(i));
		}
		string strValue;
		fromVar->Get(strValue);
		Set(strValue);
	}

	//////////////////////////////////////////////////////////////////////////
	virtual int        GetNumVariables() const { return m_vars.size(); }

	virtual IVariable* GetVariable(int index) const
	{
		assert(index >= 0 && index < (int)m_vars.size());
		return m_vars[index];
	}

	virtual void AddVariable(IVariable* var)
	{
		m_vars.push_back(var);
	}

	virtual bool DeleteVariable(IVariable* var, bool recursive /*=false*/)
	{
		bool found = stl::find_and_erase(m_vars, var);
		if (!found && recursive)
		{
			for (Variables::iterator it = m_vars.begin(); it != m_vars.end(); ++it)
			{
				if ((*it)->DeleteVariable(var, recursive))
					return true;
			}
		}
		return found;
	}

	virtual void DeleteAllVariables()
	{
		m_vars.clear();
	}

	virtual bool IsContainsVariable(IVariable* pVar, bool bRecursive) const
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

	virtual IVariable* FindVariable(const char* name, bool bRecursive, bool bHumanName) const;

	virtual bool       IsEmpty() const
	{
		return m_vars.empty();
	}

	void Serialize(XmlNodeRef node, bool load)
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
					XmlNodeRef child = node->findChild(var->GetName());
					if (child)
					{
						var->Serialize(child, load);
					}
				}
				else
				{
					var->Serialize(node, load);
				}
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
					XmlNodeRef child = node->newChild(var->GetName());
					var->Serialize(child, load);
				}
				else
					var->Serialize(node, load);
			}
		}
	}

protected:
	typedef std::vector<IVariablePtr> Variables;
	Variables m_vars;
	//! Any string value displayed in properties.
	string   m_strValue;
};

/** var_type namespace includes type definitions needed for CVariable implementaton.
 */
namespace var_type
{
//////////////////////////////////////////////////////////////////////////
template<int TypeID, bool IsStandart, bool IsInteger, bool IsSigned>
struct type_traits_base
{
	static int  type()        { return TypeID; };
	//! Return true if standard C++ type.
	static bool is_standart() { return IsStandart; };
	static bool is_integer()  { return IsInteger; };
	static bool is_signed()   { return IsSigned; };
};

template<class Type>
struct type_traits : public type_traits_base<IVariable::UNKNOWN, false, false, false> {};

// Types specialization.
template<> struct type_traits<int> : public type_traits_base<IVariable::INT, true, true, true> {};
template<> struct type_traits<bool> : public type_traits_base<IVariable::BOOL, true, true, false> {};
template<> struct type_traits<float> : public type_traits_base<IVariable::FLOAT, true, false, false> {};
template<> struct type_traits<Vec2> : public type_traits_base<IVariable::VECTOR2, false, false, false> {};
template<> struct type_traits<Vec3> : public type_traits_base<IVariable::VECTOR, false, false, false> {};
template<> struct type_traits<Vec4> : public type_traits_base<IVariable::VECTOR4, false, false, false> {};
template<> struct type_traits<Quat> : public type_traits_base<IVariable::QUAT, false, false, false> {};
template<> struct type_traits<CString> : public type_traits_base<IVariable::STRING, false, false, false> {};
template<> struct type_traits<string> : public type_traits_base<IVariable::STRING, false, false, false> {};
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// General one type to another type convertor class.
//////////////////////////////////////////////////////////////////////////
struct type_convertor
{
	template<class From, class To>
	typename std::enable_if<(!std::is_same<From, CString>::value && !std::is_same<To, CString>::value), void>::type operator() (const From& from, To& to) const { assert(0); }

	template<class From, class To>
	typename std::enable_if<(std::is_same<From, CString>::value && std::is_same<To, CString>::value), void>::type operator() (const From& from, To& to) const { to = from; }

	template<class From>
	typename std::enable_if<(!std::is_same<From, CString>::value), void>::type operator()(const From& from, CString& to) const
	{
		string str;
		operator()(from, str);
		to = str.GetString();
	}

	template<class To>
	typename std::enable_if<(!std::is_same<To, CString>::value), void>::type operator()(const CString& from, To& to) const
	{
		string str = from.GetString();
		operator()(str, to);
	}

	void operator()(const int& from, int& to) const          { to = from; }
	void operator()(const int& from, bool& to) const         { to = from != 0; }
	void operator()(const int& from, float& to) const        { to = (float)from; }
	//////////////////////////////////////////////////////////////////////////
	void operator()(const bool& from, int& to) const         { to = from; }
	void operator()(const bool& from, bool& to) const        { to = from; }
	void operator()(const bool& from, float& to) const       { to = from; }
	//////////////////////////////////////////////////////////////////////////
	void operator()(const float& from, int& to) const        { to = (int)from; }
	void operator()(const float& from, bool& to) const       { to = from != 0; }
	void operator()(const float& from, float& to) const      { to = from; }

	void operator()(const Vec2& from, Vec2& to) const        { to = from; }
	void operator()(const Vec3& from, Vec3& to) const        { to = from; }
	void operator()(const Vec4& from, Vec4& to) const        { to = from; }
	void operator()(const Quat& from, Quat& to) const        { to = from; }
	void operator()(const string& from, string& to) const  { to = from; }

	void operator()(int value, string& to) const            { to.Format("%d", value); };
	void operator()(bool value, string& to) const           { to.Format("%d", (value) ? (int)1 : (int)0); };
	void operator()(float value, string& to) const          { to.Format("%g", value); };
	void operator()(const Vec2& value, string& to) const    { to.Format("%g,%g", value.x, value.y); };
	void operator()(const Vec3& value, string& to) const    { to.Format("%g,%g,%g", value.x, value.y, value.z); };
	void operator()(const Vec4& value, string& to) const    { to.Format("%g,%g,%g,%g", value.x, value.y, value.z, value.w); };
	void operator()(const Ang3& value, string& to) const    { to.Format("%g,%g,%g", value.x, value.y, value.z); };
	void operator()(const Quat& value, string& to) const    { to.Format("%g,%g,%g,%g", value.w, value.v.x, value.v.y, value.v.z); };

	void operator()(const string& from, int& value) const   { value = atoi((const char*)from); };
	void operator()(const string& from, bool& value) const  { value = atoi((const char*)from) != 0; };
	void operator()(const string& from, float& value) const { value = (float)atof((const char*)from); };
	void operator()(const string& from, Vec2& value) const
	{
		value.x = 0;
		value.y = 0;
		sscanf((const char*)from, "%f,%f", &value.x, &value.y);
	};
	void operator()(const string& from, Vec3& value) const
	{
		value.Set(0, 0, 0);
		sscanf((const char*)from, "%f,%f,%f", &value.x, &value.y, &value.z);
	};
	void operator()(const string& from, Vec4& value) const
	{
		value.x = 0;
		value.y = 0;
		value.z = 0;
		value.w = 0;
		sscanf((const char*)from, "%f,%f,%f,%f", &value.x, &value.y, &value.z, &value.w);
	};
	void operator()(const string& from, Ang3& value) const
	{
		value.Set(0, 0, 0);
		sscanf((const char*)from, "%f,%f,%f", &value.x, &value.y, &value.z);
	};
	void operator()(const string& from, Quat& value) const
	{
		value.SetIdentity();
		sscanf((const char*)from, "%f,%f,%f,%f", &value.w, &value.v.x, &value.v.y, &value.v.z);
	};
};

//////////////////////////////////////////////////////////////////////////
// Custom comparison functions for different variable type's values,.
//////////////////////////////////////////////////////////////////////////
template<class Type>
inline bool compare(const Type& arg1, const Type& arg2)
{
	return arg1 == arg2;
};
inline bool compare(const Vec2& v1, const Vec2& v2)
{
	return v1.x == v2.x && v1.y == v2.y;
}
inline bool compare(const Vec3& v1, const Vec3& v2)
{
	return v1.x == v2.x && v1.y == v2.y && v1.z == v2.z;
}
inline bool compare(const Vec4& v1, const Vec4& v2)
{
	return v1.x == v2.x && v1.y == v2.y && v1.z == v2.z && v1.w == v2.w;
}
inline bool compare(const Ang3& v1, const Ang3& v2)
{
	return v1.x == v2.x && v1.y == v2.y && v1.z == v2.z;
}
inline bool compare(const Quat& q1, const Quat& q2)
{
	return q1.v.x == q2.v.x && q1.v.y == q2.v.y && q1.v.z == q2.v.z && q1.w == q2.w;
}
inline bool compare(const char* s1, const char* s2)
{
	return strcmp(s1, s2) == 0;
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Custom Initialization functions for different variable type's values,.
//////////////////////////////////////////////////////////////////////////
template<class Type>
inline void init(Type& val)
{
	val = 0;
};
inline void init(Vec2& val) { val.x = 0; val.y = 0; };
inline void init(Vec3& val) { val.x = 0; val.y = 0; val.z = 0; };
inline void init(Vec4& val) { val.x = 0; val.y = 0; val.z = 0; val.w = 0;  };
inline void init(Ang3& val) { val.x = 0; val.y = 0; val.z = 0; };
inline void init(Quat& val)
{
	val.v.x = 0;
	val.v.y = 0;
	val.v.z = 0;
	val.w = 0;
};
inline void init(const char*& val)
{
	val = "";
};
inline void init(string& val)
{
	// self initializing.
};
inline void init(CString& val)
{
	// self initializing.
};
//////////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////
// Void variable does not contain any value.
//////////////////////////////////////////////////////////////////////////
class CVariableVoid : public CVariableBase
{
public:
	CVariableVoid() {};
	virtual EType      GetType() const               { return IVariable::UNKNOWN; };
	virtual IVariable* Clone(bool bRecursive) const  { return new CVariableVoid(*this); }
	virtual void       CopyValue(IVariable* fromVar) {};
	virtual bool       HasDefaultValue() const       { return true; }
protected:
	CVariableVoid(const CVariableVoid& v) : CVariableBase(v) {};
};

//////////////////////////////////////////////////////////////////////////
template<class T>
class CVariable : public CVariableBase
{
	typedef CVariable<T> Self;
public:
	// Constructor.
	CVariable()
	{
		// Initialize value to zero or empty string.
		var_type::init(m_valueDef);
		SetLimits(0.0f, 100.0f, 0.0f, false, false);
	}

	explicit CVariable(const T& set)
	{
		var_type::init(m_valueDef);   // Update F32NAN values in Debud mode
		SetValue(set);
		SetLimits(0.0f, 100.0f, 0.0f, false, false);
	}

	//! Get name of parameter.
	virtual EType GetType() const { return (EType)var_type::type_traits<T>::type(); };
	virtual int   GetSize() const { return sizeof(T); };

	//////////////////////////////////////////////////////////////////////////
	// Set methods.
	//////////////////////////////////////////////////////////////////////////
	virtual void Set(int value)            { SetValue(value); }
	virtual void Set(bool value)           { SetValue(value); }
	virtual void Set(float value)          { SetValue(value); }
	virtual void Set(const Vec2& value)    { SetValue(value); }
	virtual void Set(const Vec3& value)    { SetValue(value); }
	virtual void Set(const Vec4& value)    { SetValue(value); }
	virtual void Set(const Ang3& value)    { SetValue(value); }
	virtual void Set(const Quat& value)    { SetValue(value); }
	virtual void Set(const string& value) { SetValue(value); }
	virtual void Set(const char* value)    { SetValue(string(value)); }

	//////////////////////////////////////////////////////////////////////////
	// Get methods.
	//////////////////////////////////////////////////////////////////////////
	virtual void Get(int& value) const     { GetValue(value); }
	virtual void Get(bool& value) const    { GetValue(value); }
	virtual void Get(float& value) const   { GetValue(value); }
	virtual void Get(Vec2& value) const    { GetValue(value); }
	virtual void Get(Vec3& value) const    { GetValue(value); }
	virtual void Get(Vec4& value) const    { GetValue(value); }
	virtual void Get(Quat& value) const    { GetValue(value); }
	virtual void Get(string& value) const  { GetValue(value); }
	virtual void Get(CString& value) const // for CString conversion
	{
		string str;
		Get(str);
		value = str.GetString();
	}

	virtual bool HasDefaultValue() const
	{
		T defval;
		var_type::init(defval);
		return m_valueDef == defval;
	}

	//////////////////////////////////////////////////////////////////////////
	// Limits.
	//////////////////////////////////////////////////////////////////////////
	virtual void SetLimits(float fMin, float fMax, float fStep = 0.f, bool bHardMin = true, bool bHardMax = true)
	{
		CRY_ASSERT_MESSAGE(fMin <= fMax, "maximum value has to be bigger than minimum value!");

		m_valueMin = fMin;
		m_valueMax = fMax;
		m_valueStep = fStep;
		m_bHardMin = bHardMin;
		m_bHardMax = bHardMax;
	}
	virtual void GetLimits(float& fMin, float& fMax, float& fStep, bool& bHardMin, bool& bHardMax)
	{
		fMin = m_valueMin;
		fMax = m_valueMax;
		fStep = m_valueStep;
		bHardMin = m_bHardMin;
		bHardMax = m_bHardMax;
	}
	void ClearLimits()
	{
		SetLimits(std::numeric_limits<T>::min(), std::numeric_limits<T>::max(), 0.f, false, false);
	}

	//////////////////////////////////////////////////////////////////////////
	// Access operators.
	//////////////////////////////////////////////////////////////////////////

	//! Cast to held type.
	operator T const&() const { return m_valueDef; }

	//! Assign operator for variable.
	void operator=(const T& value) { SetValue(value); }

	//////////////////////////////////////////////////////////////////////////
	IVariable* Clone(bool bRecursive) const
	{
		Self* var = new Self(*this);
		return var;
	}

	//////////////////////////////////////////////////////////////////////////
	void CopyValue(IVariable* fromVar)
	{
		assert(fromVar);
		T val;
		fromVar->Get(val);
		SetValue(val);
	}

protected:

	//////////////////////////////////////////////////////////////////////////
	template<class P>
	void SetValue(const P& value)
	{
		T newValue = T();
		//var_type::type_convertor<P,T> convertor;
		var_type::type_convertor convertor;
		convertor(value, newValue);

		// compare old and new values.
		if (m_bForceModified || !var_type::compare(m_valueDef, newValue))
		{
			m_valueDef = newValue;
			OnSetValue(false);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	template<class P>
	void GetValue(P& value) const
	{
		var_type::type_convertor convertor;
		convertor(m_valueDef, value);
	}

protected:
	T m_valueDef;

	// Min/Max value.
	float         m_valueMin, m_valueMax, m_valueStep;
	unsigned char m_bHardMin : 1;
	unsigned char m_bHardMax : 1;
	bool          m_bResolving;
};

//////////////////////////////////////////////////////////////////////////

/**
 **************************************************************************************
 * CVariableArray implements variable of type array of IVariables.
 ***************************************************************************************
 */
template<class T>
class TVariableArray : public CVariable<T>
{
	typedef TVariableArray<T> Self;
public:
	TVariableArray() : CVariable<T>() {};
	// Copy Constructor.
	TVariableArray(const Self& var) : CVariable<T>(var)
	{}

	//! Get name of parameter.
	virtual int  GetSize() const { return sizeof(Self); };

	virtual bool HasDefaultValue() const
	{
		for (Vars::const_iterator it = m_vars.begin(); it != m_vars.end(); ++it)
			if (!(*it)->HasDefaultValue())
				return false;
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	IVariable* Clone(bool bRecursive) const
	{
		Self* var = new Self(*this);
		for (Vars::const_iterator it = m_vars.begin(); it != m_vars.end(); ++it)
		{
			var->m_vars.push_back((*it)->Clone(bRecursive));
		}
		return var;
	}
	//////////////////////////////////////////////////////////////////////////
	void CopyValue(IVariable* fromVar)
	{
		assert(fromVar);
		if (fromVar->GetType() != GetType())
			return;
		__super::CopyValue(fromVar);
		int numSrc = fromVar->GetNumVariables();
		int numTrg = m_vars.size();
		for (int i = 0; i < numSrc && i < numTrg; i++)
		{
			// Copy Every child variable.
			m_vars[i]->CopyValue(fromVar->GetVariable(i));
		}
	}

	//////////////////////////////////////////////////////////////////////////
	int        GetNumVariables() const { return m_vars.size(); }
	IVariable* GetVariable(int index) const
	{
		assert(index >= 0 && index < (int)m_vars.size());
		return m_vars[index];
	}
	void AddVariable(IVariable* var) { m_vars.push_back(var); }
	void DeleteAllVariables()        { m_vars.clear(); }

	//////////////////////////////////////////////////////////////////////////
	void Serialize(XmlNodeRef node, bool load)
	{
		__super::Serialize(node, load);
		if (load)
		{
			// Loading.
			string name;
			for (Vars::iterator it = m_vars.begin(); it != m_vars.end(); ++it)
			{
				IVariable* var = *it;
				if (var->GetNumVariables())
				{
					XmlNodeRef child = node->findChild(var->GetName());
					if (child)
						var->Serialize(child, load);
				}
				else
					var->Serialize(node, load);
			}
		}
		else
		{
			// Saving.
			for (Vars::iterator it = m_vars.begin(); it != m_vars.end(); ++it)
			{
				IVariable* var = *it;
				if (var->GetNumVariables())
				{
					XmlNodeRef child = node->newChild(var->GetName());
					var->Serialize(child, load);
				}
				else
					var->Serialize(node, load);
			}
		}
	}

protected:
	typedef std::vector<IVariablePtr> Vars;
	Vars m_vars;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//! Selection list shown in combo box, for enumerated variable.
struct IVarEnumList : public _i_reference_target_t
{
	//! Get the name of specified value in enumeration, or NULL if out of range.
	virtual const char* GetItemName(uint index) = 0;
};
typedef _smart_ptr<IVarEnumList> IVarEnumListPtr;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//! Selection list shown in combo box, for enumerated variable.
template<class T>
class CVarEnumListBase : public IVarEnumList
{
public:
	CVarEnumListBase() {}

	//////////////////////////////////////////////////////////////////////////
	virtual T NameToValue(const string& name) = 0;

	//////////////////////////////////////////////////////////////////////////
	virtual string ValueToName(T const& value) = 0;

	//! Add new item to the selection.
	virtual void AddItem(const string& name, const T& value) = 0;

	template<class TVal>
	static bool IsValueEqual(const TVal& v1, const TVal& v2)
	{
		return v1 == v2;
	}
	static bool IsValueEqual(const string& v1, const string& v2)
	{
		// Case insensitive compare.
		return stricmp(v1, v2) == 0;
	}

protected:
	virtual ~CVarEnumListBase() {};
	friend class _smart_ptr<CVarEnumListBase<T>>;
};

struct CUIEnumsDatabase_SEnum;

class EDITOR_COMMON_API CVarGlobalEnumList : public CVarEnumListBase<string>
{
public:
	CVarGlobalEnumList(CUIEnumsDatabase_SEnum* pEnum) : m_pEnum(pEnum) {}
	CVarGlobalEnumList(const string& enumName);

	//! Get the name of specified value in enumeration.
	virtual const char* GetItemName(uint index);

	virtual string     NameToValue(const string& name);
	virtual string     ValueToName(const string& value);

	//! Don't add anything to a global enum database
	virtual void AddItem(const string& name, const string& value) {}

private:
	CUIEnumsDatabase_SEnum* m_pEnum;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//! Selection list shown in combo box, for enumerated variable.
template<class T>
class CVarEnumList : public CVarEnumListBase<T>
{
public:
	struct Item
	{
		string name;
		T       value;
	};
	const char* GetItemName(uint index)
	{
		if (index >= m_items.size())
			return NULL;
		return m_items[index].name;
	};

	//////////////////////////////////////////////////////////////////////////
	T NameToValue(const string& name)
	{
		for (int i = 0; i < m_items.size(); i++)
		{
			if (name == m_items[i].name)
				return m_items[i].value;
		}
		return T();
	}

	//////////////////////////////////////////////////////////////////////////
	string ValueToName(T const& value)
	{
		for (int i = 0; i < m_items.size(); i++)
		{
			if (IsValueEqual(value, m_items[i].value))
				return m_items[i].name;
		}
		return "";
	}

	//! Add new item to the selection.
	void AddItem(const string& name, const T& value)
	{
		Item item;
		item.name = name;
		item.value = value;
		m_items.push_back(item);
	};

protected:
	~CVarEnumList() {};

private:
	std::vector<Item> m_items;
};

//////////////////////////////////////////////////////////////////////////////////
// CVariableEnum is the same as CVariable but it display enumerated values in UI
//////////////////////////////////////////////////////////////////////////////////
template<class T>
class CVariableEnum : public CVariable<T>
{
public:
	//////////////////////////////////////////////////////////////////////////
	CVariableEnum() {};

	//! Assign operator for variable.
	void operator=(const T& value) { SetValue(value); }

	//! Add new item to the enumeration.
	void AddEnumItem(const string& name, const T& value)
	{
		if (GetFlags() & UI_USE_GLOBAL_ENUMS)  // don't think adding makes sense
			return;
		if (!m_enum)
			m_enum = new CVarEnumList<T>;
		m_enum->AddItem(name, value);
	};
	void AddEnumItem(const char* name, const T& value)
	{
		if (GetFlags() & UI_USE_GLOBAL_ENUMS)  // don't think adding makes sense
			return;
		if (!m_enum)
			m_enum = new CVarEnumList<T>;
		m_enum->AddItem(name, value);
	};
	void SetEnumList(CVarEnumListBase<T>* enumList)
	{
		m_enum = enumList;
		OnSetValue(false);
	}
	IVarEnumList* GetEnumList() const
	{
		return m_enum;
	}
	//////////////////////////////////////////////////////////////////////////
	IVariable* Clone(bool bRecursive) const
	{
		CVariableEnum<T>* var = new CVariableEnum<T>(*this);
		return var;
	}

	virtual string GetDisplayValue() const
	{
		if (m_enum)
			return m_enum->ValueToName(m_valueDef);
		else
			return CVariable<T>::GetDisplayValue();
	}

	virtual void SetDisplayValue(const string& value)
	{
		if (m_enum)
			Set(m_enum->NameToValue(value));
		else
			Set(value);
	}

protected:
	// Copy Constructor.
	CVariableEnum(const CVariableEnum<T>& var) : CVariable<T>(var)
	{
		m_enum = var.m_enum;
	}
private:
	_smart_ptr<CVarEnumListBase<T>> m_enum;
};

//////////////////////////////////////////////////////////////////////////
// Smart pointers to variables.
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
template<class T, class TVar>
struct CSmartVariableBase
{
	typedef typename TVar VarType;

	CSmartVariableBase() { pVar = new VarType; }

	operator T const&() const { VarType* pV = pVar; return *pV; }
	//void operator=( const T& value ) { VarType *pV = pVar; *pV = value; }

	// Compare with pointer to variable.
	bool operator==(IVariable* pV) { return pVar == pV; }
	bool operator!=(IVariable* pV) { return pVar != pV; }

	operator VarType&() { VarType* pV = pVar; return *pV; }  // Cast to CVariableBase&
	VarType& operator*() const      { return *pVar; }
	VarType* operator->(void) const { return pVar; }

	VarType* GetVar() const         { return pVar; };

protected:
	_smart_ptr<VarType> pVar;
};

//////////////////////////////////////////////////////////////////////////
template<class T>
struct CSmartVariable : public CSmartVariableBase<T, CVariable<T>>
{
	void operator=(const T& value) { VarType* pV = pVar; *pV = value; }
};

//////////////////////////////////////////////////////////////////////////
template<class T>
struct CSmartVariableArrayT : public CSmartVariableBase<T, TVariableArray<T>>
{
	void operator=(const T& value) { VarType* pV = pVar; *pV = value; }
};

//////////////////////////////////////////////////////////////////////////
template<class T>
struct CSmartVariableEnum : public CSmartVariableBase<T, CVariableEnum<T>>
{
	void operator=(const T& value) { VarType* pV = pVar; *pV = value; }
	void AddEnumItem(const string& name, const T& value)
	{
		pVar->AddEnumItem(name, value);
	};
	void SetEnumList(CVarEnumListBase<T>* enumList)
	{
		pVar->EnableUpdateCallbacks(false);
		pVar->SetEnumList(enumList);
		pVar->EnableUpdateCallbacks(true);
	}
};

//////////////////////////////////////////////////////////////////////////
struct CSmartVariableArray
{
	typedef CVariableArray VarType;

	CSmartVariableArray() { pVar = new VarType; }

	//////////////////////////////////////////////////////////////////////////
	// Access operators.
	//////////////////////////////////////////////////////////////////////////
	operator VarType&() { VarType* pV = pVar; return *pV; }

	VarType& operator*() const      { return *pVar; }
	VarType* operator->(void) const { return pVar; }

	VarType* GetVar() const         { return pVar; };

private:
	_smart_ptr<VarType> pVar;
};
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
class EDITOR_COMMON_API CVarBlock : public IVariableContainer
{
public:
	// Dtor.
	virtual ~CVarBlock() {}
	//! Add variable to block.
	virtual void AddVariable(IVariable* var);
	//! Remove variable from block
	virtual bool DeleteVariable(IVariable* var, bool bRecursive = false);

	void         AddVariable(IVariable* pVar, const char* varName, unsigned char dataType = IVariable::DT_SIMPLE);
	// This used from smart variable pointer.
	void         AddVariable(CVariableBase& var, const char* varName, unsigned char dataType = IVariable::DT_SIMPLE);

	//! Returns number of variables in block.
	virtual int GetNumVariables() const { return m_vars.size(); }

	//! Get pointer to stored variable by index.
	virtual IVariable* GetVariable(int index) const
	{
		assert(index >= 0 && index < m_vars.size());
		return m_vars[index];
	}

	// Clear all vars from VarBlock.
	virtual void DeleteAllVariables() { m_vars.clear(); };

	//! Return true if variable block is empty (Does not have any vars).
	virtual bool IsEmpty() const { return m_vars.empty(); }

	// Returns true if var block contains specified variable.
	virtual bool IsContainsVariable(IVariable* pVar, bool bRecursive = true) const;

	//! Find variable by name.
	virtual IVariable* FindVariable(const char* name, bool bRecursive = true, bool bHumanName = false) const;

	//////////////////////////////////////////////////////////////////////////
	//! Clone var block.
	CVarBlock* Clone(bool bRecursive) const;

	//////////////////////////////////////////////////////////////////////////
	//! Copy variable values from specified var block.
	//! Do not create new variables, only copy values of existing ones.
	//! Should only be used to copy identical var blocks (eg. not Array type var copied to String type var)
	//! @param fromVarBlock Source variable block that contain copied values, must be identical to this var block.
	void CopyValues(const CVarBlock* fromVarBlock);

	//////////////////////////////////////////////////////////////////////////
	//! Copy variable values from specified var block.
	//! Do not create new variables, only copy values of existing ones.
	//! Can be used to copy slightly different var blocks, matching performed by variable name.
	//! @param fromVarBlock Source variable block that contain copied values.
	void CopyValuesByName(CVarBlock* fromVarBlock);

	void OnSetValues();

	//////////////////////////////////////////////////////////////////////////
	// Wire/Unwire other variable blocks.
	//////////////////////////////////////////////////////////////////////////
	//! Wire to other variable block.
	//! Only equivalent VarBlocks can be wired (same number of variables with same type).
	//! Recursive wiring of array variables is supported.
	void Wire(CVarBlock* toVarBlock);
	//! Unwire var block.
	void Unwire(CVarBlock* varBlock);

	//! Add this callback to every variable in block (recursively).
	void AddOnSetCallback(IVariable::OnSetCallback func);
	//! Remove this callback from every variable in block (recursively).
	void RemoveOnSetCallback(IVariable::OnSetCallback func);

	//////////////////////////////////////////////////////////////////////////
	void Serialize(XmlNodeRef& vbNode, bool load);
	void Serialize(Serialization::IArchive& ar);
	void SerializeVariable(IVariable* pVariable, Serialization::IArchive& ar);

	void ReserveNumVariables(int numVars);

	//////////////////////////////////////////////////////////////////////////
	//! Gather resources in this variable block.
	virtual void GatherUsedResources(CUsedResources& resources);

	void         EnableUpdateCallbacks(bool boEnable);
	IVariable*   FindChildVar(const char* name, IVariable* pParentVar) const;

	void         Sort();

protected:
	void SetCallbackToVar(IVariable::OnSetCallback func, IVariable* pVar, bool bAdd);
	void WireVar(IVariable* src, IVariable* trg, bool bWire);
	void GatherUsedResourcesInVar(IVariable* pVar, CUsedResources& resources);

	typedef std::vector<IVariablePtr> Variables;
	Variables m_vars;
};

typedef _smart_ptr<CVarBlock> CVarBlockPtr;

//////////////////////////////////////////////////////////////////////////
class EDITOR_COMMON_API CVarObject final : public CVarBlock
{
public:
	typedef IVariable::OnSetCallback VarOnSetCallback;

	virtual ~CVarObject() {}

	void       AddVariable(CVariableBase& var, const string& varName, VarOnSetCallback cb = NULL, unsigned char dataType = IVariable::DT_SIMPLE);
	void       AddVariable(CVariableBase& var, const string& varName, const string& varHumanName, VarOnSetCallback cb = NULL, unsigned char dataType = IVariable::DT_SIMPLE);
	void       AddVariable(CVariableArray& table, CVariableBase& var, const string& varName, const string& varHumanName, VarOnSetCallback cb = NULL, unsigned char dataType = IVariable::DT_SIMPLE);
	
	//! Copy values of variables from other VarObject.
	//! Source object must be of same type.
	void CopyVariableValues(CVarObject* sourceObject);
};

// Restore warnings about virtaul overrides hidden
#pragma warning(pop)

#endif // __Variable_h__


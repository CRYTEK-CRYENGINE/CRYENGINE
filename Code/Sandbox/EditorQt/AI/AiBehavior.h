// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __aibehavior_h__
#define __aibehavior_h__

#if _MSC_VER > 1000
	#pragma once
#endif

/** AI Behavior definition.
 */
class CAIBehavior : public _i_reference_target_t
{
public:
	CAIBehavior() {};
	virtual ~CAIBehavior() {};

	void           SetName(const string& name) { m_name = name; }
	const string& GetName()                    { return m_name; }

	//! Set name of script that implements this behavior.
	void           SetScript(const string& script) { m_script = script; };
	const string& GetScript() const                { return m_script; };

	//! Get human readable description of this goal.
	const string& GetDescription()                    { return m_description; }
	//! Set human readable description of this goal.
	void           SetDescription(const string& desc) { m_description = desc; }

	//! Force reload of script file.
	void ReloadScript();

	//! Start editing script file in Text editor.
	void Edit();

private:
	string m_name;
	string m_description;
	string m_script;
};

/** AICharacter behaviour definition.
 */
class CAICharacter : public _i_reference_target_t
{
public:
	CAICharacter() {};
	virtual ~CAICharacter() {};

	void           SetName(const string& name) { m_name = name; }
	const string& GetName()                    { return m_name; }

	//! Set name of script that implements this behavior.
	void           SetScript(const string& script) { m_script = script; };
	const string& GetScript() const                { return m_script; };

	//! Get human readable description of this goal.
	const string& GetDescription()                    { return m_description; }
	//! Set human readable description of this goal.
	void           SetDescription(const string& desc) { m_description = desc; }

	//! Force reload of script file.
	void ReloadScript();

	//! Start editing script file in Text editor.
	void Edit();

private:
	string m_name;
	string m_description;
	string m_script;
};

typedef TSmartPtr<CAIBehavior>  CAIBehaviorPtr;
typedef TSmartPtr<CAICharacter> CAICharacterPtr;

#endif // __aibehavior_h__


// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __OPTION_H__
#define __OPTION_H__

struct IPlayerProfile;

//////////////////////////////////////////////////////////////////////////


enum EProfileOptionType
{
	ePOT_Normal = 0,
	ePOT_CVar,
	ePOT_ScreenResolution,
	ePOT_SysSpec
};


class COption
{

public:

	COption();
	COption(const char* name, const char* val);

	virtual ~COption();

	void						SetPlayerProfile(IPlayerProfile* profile);
	IPlayerProfile* GetPlayerProfile() const {return m_currentProfile; }

	const string&		GetName() const { return m_currentName; }

	virtual void		InitializeFromProfile();
	virtual void		InitializeFromCVar();
	virtual void		ResetDefault();

	virtual void		SetToProfile();

	virtual void					Set(const char* param);
	virtual const string&	Get() const;

	virtual const string&	GetDefault() const;

	virtual void		SetPreview(const bool _preview);
	virtual bool		IsPreview() const { return m_preview; }

	virtual void		SetConfirmation(const bool _confirmation);
	virtual bool		IsConfirmation() const { return m_confirmation; }

	virtual void		SetRequiresRestart(const bool _restart);
	virtual bool		IsRequiresRestart() const { return m_restart; }

	virtual void		SetWriteToConfig(const bool write);
	virtual bool		IsWriteToConfig() const { return m_writeToConfig; }
	virtual void		GetWriteToConfigString(CryFixedStringT<128> &outString, ICVar* pCVar, const char* param) const {}

	virtual EProfileOptionType GetType() { return ePOT_Normal; }

protected:

	string					m_currentName;
	string					m_currentValue;
	string					m_defaultValue;
	IPlayerProfile* m_currentProfile;

private:

	bool						m_preview;
	bool						m_confirmation;
	bool						m_restart;
	bool						m_writeToConfig;

};


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CCVarOption : public COption
{

public:

	CCVarOption();
	CCVarOption(const char* name, const char* val, const char* cvarName);

	virtual ~CCVarOption();

	virtual EProfileOptionType GetType() { return ePOT_CVar; }

	virtual void		InitializeFromProfile();
	virtual void		InitializeFromCVar();

#if CRY_PLATFORM_WINDOWS
	virtual void		SetToProfile();
#endif

	virtual void		Set(const char* param);

	const string&		GetCVar() { return m_cVar; }
	virtual void		GetWriteToConfigString(CryFixedStringT<128> &outString, ICVar* pCVar, const char* param) const;

protected:

	string		m_cVar;

};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


class CScreenResolutionOption : public COption
{

public:

	CScreenResolutionOption();
	CScreenResolutionOption(const char* name, const char* val);

	virtual ~CScreenResolutionOption();

	virtual EProfileOptionType GetType() { return ePOT_ScreenResolution; }

	virtual void		InitializeFromProfile();
	virtual void		InitializeFromCVar();

	virtual void		Set(const char* param);
	virtual void		GetWriteToConfigString(CryFixedStringT<128> &outString, ICVar* pCVar, const char* param) const;

};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


class CSysSpecOption : public CCVarOption
{

public:

	CSysSpecOption();
	CSysSpecOption(const char* name, const char* val, const char* cvarName);
	virtual ~CSysSpecOption();

	virtual EProfileOptionType	GetType() { return ePOT_SysSpec; }
	virtual const string&				Get() const;
	virtual void								Set(const char* param);


	virtual void								InitializeFromProfile();
	virtual bool								IsValid() const;

protected:

	static string s_customEntry;

};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


class CSysSpecAllOption : public CSysSpecOption
{

public:

	CSysSpecAllOption();
	CSysSpecAllOption(const char* name, const char* val, const char* cvarName);
	virtual ~CSysSpecAllOption();

	virtual void								InitializeFromProfile();
	virtual bool								IsValid() const;

};


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


class CNotSavedOption : public CCVarOption
{

public:

	CNotSavedOption();
	CNotSavedOption(const char* name, const char* val, const char* cvarName);
	virtual ~CNotSavedOption();

	virtual void		InitializeFromProfile();
	virtual void		InitializeFromCVar();

	virtual void		SetToProfile();

	virtual void		Set(const char* param);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


class CAutoResetOption : public CCVarOption
{

public:

	CAutoResetOption();
	CAutoResetOption(const char* name, const char* val);

	virtual ~CAutoResetOption();

	virtual void		InitializeFromProfile();
};

//////////////////////////////////////////////////////////////////////////

#endif


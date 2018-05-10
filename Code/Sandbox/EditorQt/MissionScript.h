// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CMissionScript
{
private:
	CString              m_sFilename;
	std::vector<CString> m_methods;
	std::vector<CString> m_events;
public:
	CMissionScript();
	virtual ~CMissionScript();

	void SetScriptFile(const CString& file);
	bool Load();
	void Edit();

	//! Call on reset of mission.
	void OnReset();

	//! Get Lua filename.
	const CString& GetFilename() { return m_sFilename; }

	//////////////////////////////////////////////////////////////////////////
	int            GetMethodCount() { return m_methods.size(); }
	const CString& GetMethod(int i) { return m_methods[i]; }

	//////////////////////////////////////////////////////////////////////////
	int            GetEventCount() { return m_events.size(); }
	const CString& GetEvent(int i) { return m_events[i]; }
};


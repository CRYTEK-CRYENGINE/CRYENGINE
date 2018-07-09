// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CRcCaller
{
public:
	struct IListener
	{
		virtual void ShowWarning(const string& message) = 0;
		virtual void ShowError(const string& message) = 0;
	};

public:
	CRcCaller();

	void SetListener(IListener* pListener);
	void SetEcho(bool bEcho);

	void OverwriteExtension(const string& ext);
	void SetAdditionalOptions(const string& options);

	bool Call(const string& filename);

	static string OptionOverwriteExtension(const string& ext);
	static string OptionOverwriteFilename(const string& filename);
	static string OptionVertexPositionFormat(bool b32bit);
private:
	string GetOptionsString() const;

private:
	IListener* m_pListener;
	string m_overwriteExt;
	string m_additionalOptions;
	bool m_bEcho;
};


// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RcCaller.h"

#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>

// TODO: Consider moving this to implementation file.
// #include <CryCore/ToolsHelpers/ResourceCompilerHelper.inl>
// #include <CryCore/ToolsHelpers/SettingsManagerHelpers.inl>
// #include <CryCore/ToolsHelpers/EngineSettingsManager.inl>

namespace Private_RcCaller
{

// RC listener that parses output information into QStrings
// Note: This is not suitable for huge outputs, since all lines are kept in memory
class CRcListener : public IResourceCompilerListener
{
public:
	// Construct RC listener with specified minimum severity level to use when logging
	CRcListener(MessageSeverity minimumLevel)
		: m_minimumLevel(minimumLevel)
		, m_pLog(gEnv->pSystem->GetILog())
		, m_pListener(nullptr)
		, m_bEcho(true)
	{
		assert(m_pLog);
	}

	~CRcListener()
	{
		if (!m_pListener)
		{
			return;
		}

		if (m_firstError.empty() && !m_firstWarning.empty())
		{
			m_pListener->ShowWarning(m_firstWarning);
		}
		else if (!m_firstError.empty())
		{
			m_pListener->ShowError(m_firstError);
		}
	}

	void SetListener(CRcCaller::IListener* pListener)
	{
		m_pListener = pListener;
	}

	void SetEcho(bool bEcho)
	{
		m_bEcho = bEcho;
	}
private:
	// Handle message from RC
	virtual void OnRCMessage(MessageSeverity severity, const char* text) override
	{
		static const string failMsgPrefix = "Failed to convert file";

		if (severity >= m_minimumLevel)
		{
			static const char* const szFormat = "MeshImporter calling RC: %s";
			switch (severity)
			{
			case MessageSeverity_Warning:
				if (m_bEcho)
				{
					m_pLog->LogWarning(szFormat, text);
				}
				if (m_firstWarning.empty())
				{
					m_firstWarning = text;
				}
				break;
			case MessageSeverity_Info:
			case MessageSeverity_Debug:
				if (m_bEcho)
				{
					m_pLog->LogAlways(szFormat, text);
				}
				break;
			case MessageSeverity_Error:
				if (m_firstError.empty() && failMsgPrefix != string(text).substr(0, failMsgPrefix.length()))  // Ignore generic fail message at the end.
				{
					m_firstError = text;
				}
			// Fall-through
			default:
				if (m_bEcho)
				{
					m_pLog->LogError(szFormat, text);
				}
				break;
			}
		}
	}

	// No copy/assign
	CRcListener(const CRcListener&);
	void operator=(const CRcListener&);

	const MessageSeverity m_minimumLevel;
	ILog* const           m_pLog;
	CRcCaller::IListener* m_pListener;

	string               m_firstError;
	string               m_firstWarning;

	bool m_bEcho;
};

} // namespace Private_RcCaller

CRcCaller::CRcCaller()
	: m_pListener(nullptr)
	, m_bEcho(true)
{
}

void CRcCaller::SetListener(IListener* pListener)
{
	m_pListener = pListener;
}

void CRcCaller::SetEcho(bool bEcho)
{
	m_bEcho = bEcho;
}

void CRcCaller::OverwriteExtension(const string& ext)
{
	m_overwriteExt = ext;
}

void CRcCaller::SetAdditionalOptions(const string& options)
{
	m_additionalOptions = options;
}

string CRcCaller::GetOptionsString() const
{
	string opt = m_additionalOptions;

	if (!m_overwriteExt.empty())
	{
		opt += " " + OptionOverwriteExtension(m_overwriteExt);
	}

	return opt;
}

bool CRcCaller::Call(const string& filename)
{
	using namespace Private_RcCaller;

	const CResourceCompilerHelper::ERcExePath path = CResourceCompilerHelper::eRcExePath_editor;
	CRcListener listener(IResourceCompilerListener::MessageSeverity_Warning);
	listener.SetEcho(m_bEcho);
	if (m_pListener)
	{
		listener.SetListener(m_pListener);
	}
	const auto result = CResourceCompilerHelper::CallResourceCompiler(
	  filename.c_str(),
	  GetOptionsString().c_str(),
	  &listener,
	  false, // may show window?
	  path,
	  true,  // silent?
	  true); // no user dialog?
	if (result != CResourceCompilerHelper::eRcCallResult_success)
	{
		return false;
	}
	return true;
}

string CRcCaller::OptionOverwriteExtension(const string& ext)
{
	return string().Format("/overwriteextension=%s", ext.c_str());
}

string CRcCaller::OptionOverwriteFilename(const string& filename)
{
	return string().Format("/overwritefilename=\"%s\"", filename);
}

string CRcCaller::OptionVertexPositionFormat(bool b32bit)
{
	return string().Format("/vertexpositionformat=%s", b32bit ? "f32" : "f16");
}


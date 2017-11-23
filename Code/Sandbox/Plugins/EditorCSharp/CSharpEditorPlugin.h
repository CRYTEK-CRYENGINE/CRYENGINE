// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/File/IFileChangeMonitor.h>
#include <CryMono/IMonoRuntime.h>
#include <CrySchematyc/CoreAPI.h>
#include <CryCore/Containers/CryListenerSet.h>

#include <IPlugin.h>

struct ICSharpMessageListener
{
	virtual ~ICSharpMessageListener() {}

	virtual void OnMessagesUpdated(string messages) = 0;
};

typedef CListenerSet<ICSharpMessageListener*> CSharpMessageListeners;

class CCSharpEditorPlugin final
	: public IPlugin,
	  public Schematyc::IEnvRegistryListener,
	  public IFileChangeListener,
	  public IMonoCompileListener,
	  public ISystemEventListener
{
public:
	// IPlugin
	int32       GetPluginVersion() override     { return 1; }
	const char* GetPluginName() override        { return "CSharp Editor"; }
	const char* GetPluginDescription() override { return "CSharp Editor plugin"; }
	// ~IPlugin

	CCSharpEditorPlugin();
	virtual ~CCSharpEditorPlugin();

	virtual void   RegisterMessageListener(ICSharpMessageListener* pListener)   { m_messageListeners.Add(pListener); }
	virtual void   UnregisterMessageListener(ICSharpMessageListener* pListener) { m_messageListeners.Remove(pListener); }
	virtual string GetCompileMessage()                                          { return m_compileMessage; }

	// Opens the specified file in the editor that's set in the preferences.
	bool OpenCSharpFile(const string& filePath);
	// Opens the specified file at the specified line in the editor that's set in the preferences.
	bool OpenCSharpFile(const string& filePath, const int line);

	// Opens the CompiledMonoLibrary solution in the text editor specified in the File Preferences. Returns false if the file couldn't be opened.
	bool OpenCSharpSolution();

	// IFileChangeListener
	virtual void OnFileChange(const char* sFilename, EChangeType eType) override;
	// ~IFileChangeListener

	// IMonoCompileListener
	virtual void OnCompileFinished(const char* szCompileMessage) override;
	// ~IMonoCompileListener

	// Schematyc::IEnvRegistryListener
	virtual void OnEnvElementAdd(Schematyc::IEnvElementPtr pElement) override;
	virtual void OnEnvElementDelete(Schematyc::IEnvElementPtr pElement) override;
	// ~Schematyc::IEnvRegistryListener

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener

	static CCSharpEditorPlugin* GetInstance() { return s_pInstance; }

private:
	HANDLE                 m_textEditorHandle;
	string                 m_createdTextEditor;
	bool                   m_reloadPlugins = false;
	bool                   m_sandboxInFocus = true;
	string                 m_compileMessage;
	CSharpMessageListeners m_messageListeners;

	void UpdateSourceFiles() const;
	// Checks if m_textEditorHandle refers to a valid application, and focuses the window if it does.
	bool HasExistingTextEditor() const;
	// Opens the specified file in the C# solution based. Returns false if the file couldn't be opened.
	bool OpenFileInSolution(const string& filePath);
	// Opens the specified file on a specific line in the C# solution. Returns false if the file couldn't be opened.
	bool OpenFileInSolution(const string& filePath, const int line);
	// Opens the specified file in the text editor specified in the File Preferences. Returns false if the file couldn't be opened.
	bool OpenFileInTextEditor(const string& filePath) const;
	// Opens the specified file on a specific line in the text editor specified in the File Preferences. Returns false if the file couldn't be opened.
	bool OpenFileInTextEditor(const string& filePath, const int line) const;
	// Opens the specified file by Windows association. If that fails it will try to open it in Notepad instead.
	bool OpenCSharpFileSafe(const string& filePath) const;

private:
	static CCSharpEditorPlugin* s_pInstance;
};

REGISTER_PLUGIN(CCSharpEditorPlugin)

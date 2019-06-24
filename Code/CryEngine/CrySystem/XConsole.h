// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/IConsole.h>
#include <CryInput/IInput.h>
#include <CryCore/CryCrc32.h>
#include <CryCore/Containers/CryListenerSet.h>
#include "Timer.h"

struct IFFont;
struct IIpnut;
struct INetwork;
struct IRenderer;
struct ITexture;

class CSystem;

#define MAX_HISTORY_ENTRIES 50
#define LINE_BORDER         10

enum ScrollDir
{
	sdDOWN,
	sdUP,
	sdNONE
};

struct CConsoleCommand
{
	string             m_sName;    // Console command name
	string             m_sCommand; // lua code that is executed when this command is invoked
	string             m_sHelp;    // optional help string - can be shown in the console with "<commandname> ?"
	int                m_nFlags;   // bitmask consist of flag starting with VF_ e.g. VF_CHEAT
	ConsoleCommandFunc m_func;     // Pointer to console command.
	bool               m_isManagedExternally; // true if console command is added from C# and the notification of console commands will be through C# class method invocation via mono

	//////////////////////////////////////////////////////////////////////////
	CConsoleCommand()
		: m_func(0)
		, m_nFlags(0)
		, m_isManagedExternally(false)
	{}
	size_t sizeofThis() const { return sizeof(*this) + m_sName.capacity() + 1 + m_sCommand.capacity() + 1; }
	void   GetMemoryUsage(class ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_sName);
		pSizer->AddObject(m_sCommand);
		pSizer->AddObject(m_sHelp);
	}
};

struct CConsoleCommandArgs : public IConsoleCmdArgs
{
	CConsoleCommandArgs(string& line, std::vector<string>& args) : m_line(line), m_args(args) {}
	virtual int         GetArgCount() const { return m_args.size(); }
	// Get argument by index, nIndex must be in 0 <= nIndex < GetArgCount()
	virtual const char* GetArg(int nIndex) const
	{
		assert(nIndex >= 0 && nIndex < GetArgCount());
		if (!(nIndex >= 0 && nIndex < GetArgCount()))
			return NULL;
		return m_args[nIndex].c_str();
	}
	virtual const char* GetCommandLine() const
	{
		return m_line.c_str();
	}

private:
	std::vector<string>& m_args;
	string&              m_line;
};

struct string_nocase_lt
{
	bool operator()(const char* s1, const char* s2) const
	{
		return stricmp(s1, s2) < 0;
	}
};

/*! engine console implementation
   @see IConsole
 */
class CXConsole : public IConsole, public IInputEventListener, public IRemoteConsoleListener
{
public:

	CXConsole(CSystem& system);
	virtual ~CXConsole();

	void PreProjectSystemInit();
	void PostRendererInit();

	void SetStatus(bool bActive) { m_bConsoleActive = bActive; }
	bool GetStatus() const       { return m_bConsoleActive; }
	
	void FreeRenderResources();
	void Copy();
	void Paste();

	// interface IConsole ---------------------------------------------------------

	virtual void                   Release() override;

	virtual void                   UnregisterVariable(const char* sVarName, bool bDelete = true) override;
	virtual void                   SetScrollMax(int value) override;
	virtual void                   AddOutputPrintSink(IOutputPrintSink* inpSink) override;
	virtual void                   RemoveOutputPrintSink(IOutputPrintSink* inpSink) override;
	virtual void                   ShowConsole(bool show, const int iRequestScrollMax = -1) override;
	virtual void                   DumpCVars(ICVarDumpSink* pCallback, unsigned int nFlagsFilter = 0) override;
	virtual void                   DumpKeyBinds(IKeyBindDumpSink* pCallback) override;
	virtual void                   CreateKeyBind(const char* sCmd, const char* sRes) override;
	virtual const char*            FindKeyBind(const char* sCmd) const override;
	virtual void                   SetImage(ITexture* pImage, bool bDeleteCurrent) override;
	virtual inline ITexture*       GetImage() override                     { return m_pImage; }
	virtual void                   StaticBackground(bool bStatic) override { m_bStaticBackground = bStatic; }
	virtual bool                   GetLineNo(const int indwLineNo, char* outszBuffer, const int indwBufferSize) const override;
	virtual int                    GetLineCount() const override;
	virtual ICVar*                 GetCVar(const char* name) override;
	virtual void                   PrintLine(const char* s) override;
	virtual void                   PrintLinePlus(const char* s) override;
	virtual bool                   GetStatus() override;
	virtual void                   Clear() override;
	virtual void                   Update() override;
	virtual void                   Draw() override;
	virtual void                   RegisterListener(IManagedConsoleCommandListener* pListener, const char* name) override;
	virtual void                   UnregisterListener(IManagedConsoleCommandListener* pListener) override;
	virtual void                   RemoveCommand(const char* sName) override;
	virtual void                   ExecuteString(const char* command, const bool bSilentMode, const bool bDeferExecution = false) override;
	virtual void                   Exit(const char* command, ...) override PRINTF_PARAMS(2, 3);
	virtual bool                   IsOpened() override;
	virtual size_t                 GetNumVars(bool bIncludeCommands = false) const override;
	virtual size_t                 GetSortedVars(const char** pszArray, size_t numItems, const char* szPrefix = 0, int nListTypes = 0) const override;
	virtual int                    GetNumCheatVars() override;
	virtual void                   SetCheatVarHashRange(size_t firstVar, size_t lastVar) override;
	virtual void                   CalcCheatVarHash() override;
	virtual bool                   IsHashCalculated() override;
	virtual uint64                 GetCheatVarHash() override;
	virtual void                   FindVar(const char* substr);
	virtual const char*            AutoComplete(const char* substr) override;
	virtual const char*            AutoCompletePrev(const char* substr) override;
	virtual const char*            ProcessCompletion(const char* szInputBuffer) override;
	virtual void                   RegisterAutoComplete(const char* sVarOrCommand, IConsoleArgumentAutoComplete* pArgAutoComplete) override;
	virtual void                   UnRegisterAutoComplete(const char* sVarOrCommand) override;
	virtual void                   ResetAutoCompletion() override;
	virtual void                   GetMemoryUsage(ICrySizer* pSizer) const override;
	virtual void                   ResetProgressBar(int nProgressRange) override;
	virtual void                   TickProgressBar() override;
	virtual void                   SetLoadingImage(const char* szFilename) override;
	virtual void                   AddConsoleVarSink(IConsoleVarSink* pSink) override;
	virtual void                   RemoveConsoleVarSink(IConsoleVarSink* pSink) override;
	virtual const char*            GetHistoryElement(const bool bUpOrDown) override;
	virtual void                   AddCommandToHistory(const char* szCommand) override;
	virtual void                   SetInputLine(const char* szLine) override;
	virtual void                   LoadConfigVar(const char* sVariable, const char* sValue) override;
	virtual void                   LoadConfigCommand(const char* szCommand, const char* szArguments = nullptr) override;
	virtual ELoadConfigurationType SetCurrentConfigType(ELoadConfigurationType configType) override;
	virtual void                   EnableActivationKey(bool bEnable) override;
#if defined(DEDICATED_SERVER)
	virtual void                   SetClientDataProbeString(const char* pName, const char* pValue) override;
#endif
	virtual void                   SaveInternalState(struct IDataWriteStream& writer) const override;
	virtual void                   LoadInternalState(struct IDataReadStream& reader) override;

	virtual bool                   OnBeforeVarChange(ICVar* pVar, const char* sNewValue) override;
	virtual void                   OnAfterVarChange(ICVar* pVar) override;

	// interface IInputEventListener ------------------------------------------------------------------

	virtual bool OnInputEvent(const SInputEvent& event) override;
	virtual bool OnInputEventUI(const SUnicodeEvent& event) override;

	virtual void SetReadOnly(bool readonly) override { m_readOnly = readonly; }
	virtual bool IsReadOnly() override               { return m_readOnly; }

	// interface IRemoteConsoleListener ------------------------------------------------------------------

	virtual void OnConsoleCommand(const char* cmd) override;

	// Returns 0 if the operation failed
	ICVar*        RegisterCVarGroup(const char* sName, const char* szFileName);

	virtual void  PrintCheatVars(bool bUseLastHashRange) override;
	virtual const char* GetCheatVarAt(uint32 nOffset) override;

	void          SetProcessingGroup(bool isGroup) { m_bIsProcessingGroup = isGroup; }
	bool          GetIsProcessingGroup(void) const { return m_bIsProcessingGroup; }

	bool          ParseCVarOverridesFile(const char* szSysCVarOverridesPathConfigFile);

protected: // ----------------------------------------------------------------------------------------
	
	void RegisterVar(const string& name, ICVar* pCVar, ConsoleVarFunc pChangeFunc = 0);

	bool ProcessInput(const SInputEvent& event);
	void DrawBuffer(int nScrollPos, const char* szEffect);
	void AddLine(const char* inputStr);
	void AddLinePlus(const char* inputStr);
	void AddInputChar(const uint32 c);
	void RemoveInputChar(bool bBackSpace);
	void ExecuteInputBuffer();
	void ExecuteCommand(CConsoleCommand& cmd, string& params, bool bIgnoreDevMode = false);
	void ScrollConsole();

#if ALLOW_AUDIT_CVARS
	void AuditCVars(IConsoleCmdArgs* pArg);
#endif

#ifndef _RELEASE
	// will be removed once the HTML version is good enough
	void DumpCommandsVarsTxt(const char* prefix);
	void DumpVarsTxt(const bool includeCheat);
#endif

	void ConsoleLogInputResponse(const char* szFormat, ...) PRINTF_PARAMS(2, 3);
	void ConsoleLogInput(const char* szFormat, ...) PRINTF_PARAMS(2, 3);
	void ConsoleWarning(const char* szFormat, ...) PRINTF_PARAMS(2, 3);

	void DisplayHelp(const char* help, const char* name);
	void DisplayVarValue(ICVar* pVar);

	void SplitCommands(const char* line, std::list<string>& split);
	// bFromConsole: true=from console, false=from outside
	void ExecuteStringInternal(const char* command, const bool bFromConsole, const bool bSilentMode = false);
	void ExecuteDeferredCommands();

	static const char* GetFlagsString(const uint32 dwFlags);
	static void        CmdDumpAllAnticheatVars(IConsoleCmdArgs* pArgs);
	static void        CmdDumpLastHashedAnticheatVars(IConsoleCmdArgs* pArgs);

private: // ----------------------------------------------------------

	struct SConfigVar
	{
		string m_value;
		bool   m_partOfGroup;
		uint32 nCVarOrFlags;
	};

	struct SDeferredCommand
	{
		string command;
		bool   silentMode;

		SDeferredCommand(const string& command, bool silentMode)
			: command(command), silentMode(silentMode)
		{}
	};

	typedef std::deque<string>                                                                                              ConsoleBuffer;
	typedef std::unordered_map<string, CConsoleCommand, stl::hash_stricmp<string>, stl::hash_stricmp<string>>               ConsoleCommandsMap;
	typedef std::unordered_map<string, string, stl::hash_stricmp<string>, stl::hash_stricmp<string>>                        ConsoleBindsMap;
	typedef std::unordered_map<string, IConsoleArgumentAutoComplete*, stl::hash_stricmp<string>, stl::hash_stricmp<string>> ArgumentAutoCompleteMap;
	typedef std::unordered_map<string, SConfigVar, stl::hash_stricmp<string>, stl::hash_stricmp<string>>                    ConfigVars;
	typedef std::list<SDeferredCommand>                                                                                     TDeferredCommandList;
	typedef std::list<IConsoleVarSink*>                                                                                     ConsoleVarSinks;
	typedef CListenerSet<IManagedConsoleCommandListener*>                                                                   TManagedConsoleCommandListener;
	typedef std::unordered_map<string, ICVar*, stl::hash_stricmp<string>, stl::hash_stricmp<string>>                        ConsoleVariablesMap;
	typedef std::vector<std::pair<const char*, ICVar*>>                                                                     ConsoleVariablesVector;

	void LogChangeMessage(const char* name, const bool isConst, const bool isCheat, const bool isReadOnly, const bool isDeprecated,
	                      const char* oldValue, const char* newValue, const bool isProcessingGroup, const bool allowChange);

	void        AddCheckedCVar(ConsoleVariablesVector& vector, const ConsoleVariablesVector::value_type& value);
	void        RemoveCheckedCVar(ConsoleVariablesVector& vector, const ConsoleVariablesVector::value_type& value);
	static void AddCVarsToHash(ConsoleVariablesVector::const_iterator begin, ConsoleVariablesVector::const_iterator end, CCrc32& runningNameCrc32, CCrc32& runningNameValueCrc32);
	static bool CVarNameLess(const std::pair<const char*, ICVar*>& lhs, const std::pair<const char*, ICVar*>& rhs);


	virtual void AddCommand(const char* sCommand, ConsoleCommandFunc func, int nFlags = 0, const char* sHelp = NULL, bool bIsManagedExternally = false) override;
	virtual void AddCommand(const char* sName, const char* sScriptFunc, int nFlags = 0, const char* sHelp = NULL) override;

	virtual ICVar* RegisterString(const char* sName, const char* sValue, int nFlags, const char* help = "", ConsoleVarFunc pChangeFunc = 0) override;
	virtual ICVar* RegisterInt(const char* sName, int iValue, int nFlags, const char* help = "", ConsoleVarFunc pChangeFunc = 0) override;
	virtual ICVar* RegisterInt64(const char* sName, int64 iValue, int nFlags, const char* help = "", ConsoleVarFunc pChangeFunc = 0) override;
	virtual ICVar* RegisterFloat(const char* sName, float fValue, int nFlags, const char* help = "", ConsoleVarFunc pChangeFunc = 0) override;
	virtual ICVar* Register(const char* name, float* src, float defaultvalue, int flags = 0, const char* help = "", ConsoleVarFunc pChangeFunc = 0, bool allowModify = true) override;
	virtual ICVar* Register(const char* name, int* src, int defaultvalue, int flags = 0, const char* help = "", ConsoleVarFunc pChangeFunc = 0, bool allowModify = true) override;
	virtual ICVar* Register(const char* name, const char** src, const char* defaultvalue, int flags = 0, const char* help = "", ConsoleVarFunc pChangeFunc = 0, bool allowModify = true) override;
	virtual ICVar* Register(ICVar* pVar) override { RegisterVar(pVar->GetName(), pVar); return pVar; }

	void UnregisterVariableImpl(const ConsoleVariablesMap::iterator& iter);

	// --------------------------------------------------------------------------------

	ConsoleBuffer                  m_dqConsoleBuffer;
	ConsoleBuffer                  m_dqHistory;

	bool                           m_bStaticBackground;
	int                            m_nLoadingBackTexID;
	int                            m_nWhiteTexID;
	int                            m_nProgress;
	int                            m_nProgressRange;

	string                         m_sInputBuffer;
	string                         m_sReturnString;

	string                         m_sPrevTab;
	int                            m_nTabCount;

	ConsoleCommandsMap             m_mapCommands;             //
	ConsoleBindsMap                m_mapBinds;                //
	ConsoleVariablesMap            m_mapVariables;            //
	ConsoleVariablesVector         m_randomCheckedVariables;
	ConsoleVariablesVector         m_alwaysCheckedVariables;
	std::vector<IOutputPrintSink*> m_OutputSinks;             // objects in this vector are not released
	TManagedConsoleCommandListener m_managedConsoleCommandListeners;

	TDeferredCommandList           m_deferredCommands;        // A fifo of deferred commands
	bool                           m_deferredExecution;       // True when deferred commands are processed
	int                            m_waitFrames;              // A counter which is used by wait_frames command
	CTimeValue                     m_waitSeconds;             // An absolute timestamp which is used by wait_seconds command
	int                            m_blockCounter;            // This counter is incremented whenever a blocker command (VF_BLOCKFRAME) is executed.

	ArgumentAutoCompleteMap        m_mapArgumentAutoComplete;

	ConsoleVarSinks                m_consoleVarSinks;

	ConfigVars                     m_configVars;              // temporary data of cvars that haven't been created yet
	std::multimap<string, string>  m_configCommands;  // temporary data of commands that haven't been created yet

	int                            m_nScrollPos;
	int                            m_nTempScrollMax;          // for currently opened console, reset to m_nScrollMax
	int                            m_nScrollMax;              //
	int                            m_nScrollLine;
	int                            m_nHistoryPos;
	size_t                         m_nCursorPos;                // x position in characters
	ITexture*                      m_pImage;

	float                          m_fRepeatTimer;            // relative, next repeat even in .. decreses over time, repeats when 0, only valid if m_nRepeatEvent.keyId != eKI_Unknown
	SInputEvent                    m_nRepeatEvent;            // event that will be repeated

	float                          m_fCursorBlinkTimer;       // relative, increases over time,
	bool                           m_bDrawCursor;

	ScrollDir                      m_sdScrollDir;

	bool                           m_bConsoleActive;
	bool                           m_bActivationKeyEnable;
	bool                           m_bIsProcessingGroup;

	size_t                         m_nCheatHashRangeFirst;
	size_t                         m_nCheatHashRangeLast;
	bool                           m_bCheatHashDirty;
	uint64                         m_nCheatHash;

	CSystem&                       m_system;
	IFFont*                        m_pFont;
	IRenderer*                     m_pRenderer;
	IInput*                        m_pInput;
	ITimer*                        m_pTimer;
	INetwork*                      m_pNetwork;                // EvenBalance - M. Quinn

	ICVar*                         m_pSysDeactivateConsole;

	ELoadConfigurationType         m_currentLoadConfigType;

	bool                           m_readOnly;

	static int                     con_display_last_messages;
	static int                     con_line_buffer_size;
	static float                   con_font_size;
	static int                     con_showonload;
	static int                     con_debug;
	static int                     con_restricted;

	friend void Command_SetWaitSeconds(IConsoleCmdArgs* Cmd);
	friend void Command_SetWaitFrames(IConsoleCmdArgs* Cmd);
#if ALLOW_AUDIT_CVARS
	friend void Command_AuditCVars(IConsoleCmdArgs* pArg);
#endif // ALLOW_AUDIT_CVARS
	friend void Command_DumpCommandsVars(IConsoleCmdArgs* Cmd);
	friend void Command_DumpVars(IConsoleCmdArgs* Cmd);
	friend class CConsoleHelpGen;
};

// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(AFX_XCONSOLE_H__BA902011_5C47_4954_8E09_68598456912D__INCLUDED_)
#define AFX_XCONSOLE_H__BA902011_5C47_4954_8E09_68598456912D__INCLUDED_

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

#include <CrySystem/IConsole.h>
#include <CryInput/IInput.h>
#include <CryCore/CryCrc32.h>
#include <CryCore/Containers/CryListenerSet.h>
#include "Timer.h"

//forward declaration
struct IIpnut;
struct INetwork;
class CSystem;

#define MAX_HISTORY_ENTRIES 50
#define LINE_BORDER         10

enum ScrollDir
{
	sdDOWN,
	sdUP,
	sdNONE
};

//////////////////////////////////////////////////////////////////////////
// Console command holds information about commands registered to console.
//////////////////////////////////////////////////////////////////////////
struct CConsoleCommand
{
	string             m_sName;    // Console command name
	string             m_sCommand; // lua code that is executed when this command is invoked
	string             m_sHelp;    // optional help string - can be shown in the console with "<commandname> ?"
	int                m_nFlags;   // bitmask consist of flag starting with VF_ e.g. VF_CHEAT
	ConsoleCommandFunc m_func;     // Pointer to console command.
	bool               m_isManagedExternally;// true if console command is added from C# and the notification of console commands will be through C# class method invocation via mono

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

//////////////////////////////////////////////////////////////////////////
// Implements IConsoleCmdArgs.
//////////////////////////////////////////////////////////////////////////
struct CConsoleCommandArgs : public IConsoleCmdArgs
{
	CConsoleCommandArgs(string& line, std::vector<string>& args) : m_line(line), m_args(args) {};
	virtual int         GetArgCount() const { return m_args.size(); };
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

/* - very dangerous to use with STL containers
   struct string_nocase_lt
   {
   bool operator()( const char *s1,const char *s2 ) const
   {
    return stricmp(s1,s2) < 0;
   }
   bool operator()( const string &s1,const string &s2 ) const
   {
    return stricmp(s1.c_str(),s2.c_str()) < 0;
   }
   };
 */

//forward declarations
class ITexture;
struct IRenderer;

/*! engine console implementation
   @see IConsole
 */
class CXConsole : public IConsole, public IInputEventListener, public IRemoteConsoleListener
{
public:
	typedef std::deque<string>              ConsoleBuffer;
	typedef ConsoleBuffer::iterator         ConsoleBufferItor;
	typedef ConsoleBuffer::reverse_iterator ConsoleBufferRItor;

	// constructor
	CXConsole();
	// destructor
	virtual ~CXConsole();

	//
	void Init(CSystem* pSystem);
	//
	void SetStatus(bool bActive) { m_bConsoleActive = bActive; }
	bool GetStatus() const       { return m_bConsoleActive; }
	//
	void FreeRenderResources();
	//
	void Copy();
	//
	void Paste();

	// interface IConsole ---------------------------------------------------------

	virtual void                   Release();

	virtual void                   UnregisterVariable(const char* sVarName, bool bDelete = false);
	virtual void                   SetScrollMax(int value);
	virtual void                   AddOutputPrintSink(IOutputPrintSink* inpSink);
	virtual void                   RemoveOutputPrintSink(IOutputPrintSink* inpSink);
	virtual void                   ShowConsole(bool show, const int iRequestScrollMax = -1);
	virtual void                   DumpCVars(ICVarDumpSink* pCallback, unsigned int nFlagsFilter = 0);
	virtual void                   DumpKeyBinds(IKeyBindDumpSink* pCallback);
	virtual void                   CreateKeyBind(const char* sCmd, const char* sRes);
	virtual const char*            FindKeyBind(const char* sCmd) const;
	virtual void                   SetImage(ITexture* pImage, bool bDeleteCurrent);
	virtual inline ITexture*       GetImage()                     { return m_pImage; }
	virtual void                   StaticBackground(bool bStatic) { m_bStaticBackground = bStatic; }
	virtual bool                   GetLineNo(const int indwLineNo, char* outszBuffer, const int indwBufferSize) const;
	virtual int                    GetLineCount() const;
	virtual ICVar*                 GetCVar(const char* name);
	virtual char*                  GetVariable(const char* szVarName, const char* szFileName, const char* def_val);
	virtual float                  GetVariable(const char* szVarName, const char* szFileName, float def_val);
	virtual void                   PrintLine(const char* s);
	virtual void                   PrintLinePlus(const char* s);
	virtual bool                   GetStatus();
	virtual void                   Clear();
	virtual void                   Update();
	virtual void                   Draw();
	virtual void                   RegisterListener(IManagedConsoleCommandListener* pListener, const char* name);
	virtual void                   UnregisterListener(IManagedConsoleCommandListener* pListener);
	virtual void                   RemoveCommand(const char* sName);
	virtual void                   ExecuteString(const char* command, const bool bSilentMode, const bool bDeferExecution = false);
	virtual void                   Exit(const char* command, ...) PRINTF_PARAMS(2, 3);
	virtual bool                   IsOpened();
	virtual size_t                 GetNumVars(bool bIncludeCommands = false) const;
	virtual size_t                 GetSortedVars(const char** pszArray, size_t numItems, const char* szPrefix = 0, int nListTypes = 0) const;
	virtual int                    GetNumCheatVars();
	virtual void                   SetCheatVarHashRange(size_t firstVar, size_t lastVar);
	virtual void                   CalcCheatVarHash();
	virtual bool                   IsHashCalculated();
	virtual uint64                 GetCheatVarHash();
	virtual void                   FindVar(const char* substr);
	virtual const char*            AutoComplete(const char* substr);
	virtual const char*            AutoCompletePrev(const char* substr);
	virtual const char*            ProcessCompletion(const char* szInputBuffer);
	virtual void                   RegisterAutoComplete(const char* sVarOrCommand, IConsoleArgumentAutoComplete* pArgAutoComplete);
	virtual void                   UnRegisterAutoComplete(const char* sVarOrCommand);
	virtual void                   ResetAutoCompletion();
	virtual void                   GetMemoryUsage(ICrySizer* pSizer) const;
	virtual void                   ResetProgressBar(int nProgressRange);
	virtual void                   TickProgressBar();
	virtual void                   SetLoadingImage(const char* szFilename);
	virtual void                   AddConsoleVarSink(IConsoleVarSink* pSink);
	virtual void                   RemoveConsoleVarSink(IConsoleVarSink* pSink);
	virtual const char*            GetHistoryElement(const bool bUpOrDown);
	virtual void                   AddCommandToHistory(const char* szCommand);
	virtual void                   SetInputLine(const char* szLine);
	virtual void                   LoadConfigVar(const char* sVariable, const char* sValue);
	virtual void                   LoadConfigCommand(const char* szCommand, const char* szArguments = nullptr);
	virtual ELoadConfigurationType SetCurrentConfigType(ELoadConfigurationType configType);
	virtual void                   EnableActivationKey(bool bEnable);
#if defined(DEDICATED_SERVER)
	virtual void                   SetClientDataProbeString(const char* pName, const char* pValue);
#endif
	virtual void                   SaveInternalState(struct IDataWriteStream& writer) const;
	virtual void                   LoadInternalState(struct IDataReadStream& reader);

	// interface IInputEventListener ------------------------------------------------------------------

	virtual bool OnInputEvent(const SInputEvent& event);
	virtual bool OnInputEventUI(const SUnicodeEvent& event);

	virtual void SetReadOnly(bool readonly) { m_readOnly = readonly; }
	virtual bool IsReadOnly()               { return m_readOnly; }

	// interface IRemoteConsoleListener ------------------------------------------------------------------

	virtual void OnConsoleCommand(const char* cmd);

	// interface IConsoleVarSink ----------------------------------------------------------------------

	virtual bool OnBeforeVarChange(ICVar* pVar, const char* sNewValue);
	virtual void OnAfterVarChange(ICVar* pVar);

	//////////////////////////////////////////////////////////////////////////

	// Returns
	//   0 if the operation failed
	ICVar*        RegisterCVarGroup(const char* sName, const char* szFileName);

	virtual void  PrintCheatVars(bool bUseLastHashRange);
	virtual char* GetCheatVarAt(uint32 nOffset);

	void          SetProcessingGroup(bool isGroup) { m_bIsProcessingGroup = isGroup; }
	bool          GetIsProcessingGroup(void) const { return m_bIsProcessingGroup; }

protected: // ----------------------------------------------------------------------------------------
	void DrawBuffer(int nScrollPos, const char* szEffect);

	void RegisterVar(ICVar* pCVar, ConsoleVarFunc pChangeFunc = 0);

	bool ProcessInput(const SInputEvent& event);
	void AddLine(const char* inputStr);
	void AddLinePlus(const char* inputStr);
	void AddInputChar(const uint32 c);
	void RemoveInputChar(bool bBackSpace);
	void ExecuteInputBuffer();
	void ExecuteCommand(CConsoleCommand& cmd, string& params, bool bIgnoreDevMode = false);

	void ScrollConsole();

#if ALLOW_AUDIT_CVARS
	void AuditCVars(IConsoleCmdArgs* pArg);
#endif // ALLOW_AUDIT_CVARS

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

	// Arguments:
	//   bFromConsole - true=from console, false=from outside
	void               SplitCommands(const char* line, std::list<string>& split);
	void               ExecuteStringInternal(const char* command, const bool bFromConsole, const bool bSilentMode = false);
	void               ExecuteDeferredCommands();

	static const char* GetFlagsString(const uint32 dwFlags);

	static void        CmdDumpAllAnticheatVars(IConsoleCmdArgs* pArgs);
	static void        CmdDumpLastHashedAnticheatVars(IConsoleCmdArgs* pArgs);

private: // ----------------------------------------------------------

	typedef std::map<const char*, ICVar*, string_nocase_lt> ConsoleVariablesMap;    // key points into string stored in ICVar or in .exe/.dll
	typedef ConsoleVariablesMap::iterator                   ConsoleVariablesMapItor;

	typedef std::vector<std::pair<const char*, ICVar*>>     ConsoleVariablesVector;

	typedef CListenerSet<IManagedConsoleCommandListener*>   TManagedConsoleCommandListener;
	TManagedConsoleCommandListener m_managedConsoleCommandListeners;

	void LogChangeMessage(const char* name, const bool isConst, const bool isCheat, const bool isReadOnly, const bool isDeprecated,
	                      const char* oldValue, const char* newValue, const bool isProcessingGroup, const bool allowChange);

	void        AddCheckedCVar(ConsoleVariablesVector& vector, const ConsoleVariablesVector::value_type& value);
	void        RemoveCheckedCVar(ConsoleVariablesVector& vector, const ConsoleVariablesVector::value_type& value);
	static void AddCVarsToHash(ConsoleVariablesVector::const_iterator begin, ConsoleVariablesVector::const_iterator end, CCrc32& runningNameCrc32, CCrc32& runningNameValueCrc32);
	static bool CVarNameLess(const std::pair<const char*, ICVar*>& lhs, const std::pair<const char*, ICVar*>& rhs);


	virtual void AddCommand(const char* sCommand, ConsoleCommandFunc func, int nFlags = 0, const char* sHelp = NULL, bool bIsManagedExternally = false);
	virtual void AddCommand(const char* sName, const char* sScriptFunc, int nFlags = 0, const char* sHelp = NULL);

	virtual ICVar* RegisterString(const char* sName, const char* sValue, int nFlags, const char* help = "", ConsoleVarFunc pChangeFunc = 0);
	virtual ICVar* RegisterInt(const char* sName, int iValue, int nFlags, const char* help = "", ConsoleVarFunc pChangeFunc = 0);
	virtual ICVar* RegisterInt64(const char* sName, int64 iValue, int nFlags, const char* help = "", ConsoleVarFunc pChangeFunc = 0);
	virtual ICVar* RegisterFloat(const char* sName, float fValue, int nFlags, const char* help = "", ConsoleVarFunc pChangeFunc = 0);
	virtual ICVar* Register(const char* name, float* src, float defaultvalue, int flags = 0, const char* help = "", ConsoleVarFunc pChangeFunc = 0, bool allowModify = true);
	virtual ICVar* Register(const char* name, int* src, int defaultvalue, int flags = 0, const char* help = "", ConsoleVarFunc pChangeFunc = 0, bool allowModify = true);
	virtual ICVar* Register(const char* name, const char** src, const char* defaultvalue, int flags = 0, const char* help = "", ConsoleVarFunc pChangeFunc = 0, bool allowModify = true);
	virtual ICVar* Register(ICVar* pVar) { RegisterVar(pVar); return pVar; }

	typedef std::map<string, CConsoleCommand, string_nocase_lt>                        ConsoleCommandsMap;
	typedef ConsoleCommandsMap::iterator                                               ConsoleCommandsMapItor;

	typedef std::map<string, string>                                                   ConsoleBindsMap;
	typedef ConsoleBindsMap::iterator                                                  ConsoleBindsMapItor;

	typedef std::map<string, IConsoleArgumentAutoComplete*, stl::less_stricmp<string>> ArgumentAutoCompleteMap;

	struct SConfigVar
	{
		string m_value;
		bool   m_partOfGroup;
		uint32 nCVarOrFlags;
	};
	typedef std::map<string, SConfigVar, string_nocase_lt> ConfigVars;

	struct SDeferredCommand
	{
		string command;
		bool   silentMode;

		SDeferredCommand(const string& command, bool silentMode)
			: command(command), silentMode(silentMode)
		{}
	};
	typedef std::list<SDeferredCommand> TDeferredCommandList;

	typedef std::list<IConsoleVarSink*> ConsoleVarSinks;

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

	CSystem*                       m_pSystem;
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

#endif // !defined(AFX_XCONSOLE_H__BA902011_5C47_4954_8E09_68598456912D__INCLUDED_)

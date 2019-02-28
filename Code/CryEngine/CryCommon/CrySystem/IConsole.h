// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _ICONSOLE_H_
#define _ICONSOLE_H_

#include <CryCore/SFunctor.h>

struct ConsoleBind;

struct ICVar;
class ITexture;
class ICrySizer;
enum ELoadConfigurationType;

#define     CVAR_INT    1
#define     CVAR_FLOAT  2
#define     CVAR_STRING 3

#if defined(_RELEASE)
	#define ALLOW_AUDIT_CVARS    0
	#define LOG_CVAR_INFRACTIONS 1
	#define LOG_CVAR_USAGE       0
#else
	#define ALLOW_AUDIT_CVARS    1
	#define LOG_CVAR_INFRACTIONS 1
	#define LOG_CVAR_USAGE       1
#endif // defined(_RELEASE)

#define CVAR_GROUPS_ARE_PRIVILEGED     1                           // allows cvar groups to manipulate const/cheat cvars (turn off to log group cvar manipulation)
#define LOG_CVAR_INFRACTIONS_CALLSTACK (LOG_CVAR_INFRACTIONS && 0) // logs the callstack when certain cvar issues arise; helpful in tracking down where the problem originated

//! If this flag is set during registering a console variable, and the variable exists,
//! then the variable will store its value in memory given by src.
#define CVF_CHANGE_SOURCE (1u << 16)

//! Flags used by ICVar.
enum EVarFlags : uint32
{
	VF_NULL                    = 0x00000000,      //!< Just to have one recognizable spot where the flags are located in the Register call.
	VF_CHEAT                   = 0x00000002,      //!< Stays in the default state when cheats are disabled.
	VF_DEV_ONLY                = 0x00000004,      //!< Cvar is only registered with the console in non release builds.
	VF_DEDI_ONLY               = 0x00000008,      //!< Cvar is only registered with the console in non release or dedicated server builds.
	VF_NET_SYNCED              = 0x00000080,      //!< Synchronised between server and client(s); server is authoritative.
	VF_DUMPTODISK              = 0x00000100,
	VF_READONLY                = 0x00000800,      //!< Can not be changed by the user.
	VF_REQUIRE_LEVEL_RELOAD    = 0x00001000,
	VF_REQUIRE_APP_RESTART     = 0x00002000,
	VF_WARNING_NOTUSED         = 0x00004000,      //!< Shows warning that this var was not used in config file.
	VF_COPYNAME                = 0x00008000,      //!< Otherwise the const char * to the name will be stored without copying the memory.
	VF_MODIFIED                = 0x00010000,      //!< Set when variable value modified.
	VF_WASINCONFIG             = 0x00020000,      //!< Set when variable was present in config file.
	VF_BITFIELD                = 0x00040000,      //!< Allow bitfield setting syntax.
	VF_RESTRICTEDMODE          = 0x00080000,      //!< Is visible and usable in restricted (normal user) console mode.
	VF_INVISIBLE               = 0x00100000,      //!< Invisible to the user in console.
	VF_ALWAYSONCHANGE          = 0x00200000,      //!< Always accept variable value and call on change callback even if variable value didnt change.
	VF_BLOCKFRAME              = 0x00400000,      //!< Blocks the execution of console commands for one frame.
	VF_CONST_CVAR              = 0x00800000,      //!< Set if it is a const cvar not to be set inside cfg-files.
	VF_CHEAT_ALWAYS_CHECK      = 0x01000000,      //!< This variable is critical to check in every hash, since it's extremely vulnerable.
	VF_CHEAT_NOCHECK           = 0x02000000,      //!< This variable is set as VF_CHEAT but doesn't have to be checked/hashed since it's harmless to hack.
	VF_SYSSPEC_OVERWRITE       = 0x04000000,      //!< This variable is specified by system.cfg with the intention to overwrite all subsequent settings.
	VF_CVARGRP_IGNOREINREALVAL = 0x08000000,      //!< This variable will be ignored when cvar group's real val is checked (Needed for cvars which are in a group but change in various situations).
	VF_LIVE_CREATE_SYNCED      = 0x10000000,      //!< This variable will be synced with LiveCreate clients.
	VF_RENDERER_CVAR           = 0x20000000,      //!< The update of this variable will be done in render thread.
	VF_DEPRECATED              = 0x40000000,      //!< Deprecated cvars use default values which cannot be modified outside the code.
	VF_EXPERIMENTAL            = 0x80000000,      //!< This variable is used by WIP or experimental feature.
};

struct ICVarDumpSink
{
	// <interfuscator:shuffle>
	virtual ~ICVarDumpSink(){}
	virtual void OnElementFound(ICVar* pCVar) = 0;
	// </interfuscator:shuffle>
};

struct IKeyBindDumpSink
{
	// <interfuscator:shuffle>
	virtual ~IKeyBindDumpSink(){}
	virtual void OnKeyBindFound(const char* sBind, const char* sCommand) = 0;
	// </interfuscator:shuffle>
};

struct IOutputPrintSink
{
	// <interfuscator:shuffle>
	virtual ~IOutputPrintSink(){}
	virtual void Print(const char* inszText) = 0;
	// </interfuscator:shuffle>
};

//! Callback class to derive from when you want to receive callbacks when console var changes.
struct IConsoleVarSink
{
	// <interfuscator:shuffle>
	virtual ~IConsoleVarSink(){}

	//! Called by Console before changing console var value, to validate if var can be changed.
	//! \return true if ok to change value, false if should not change value.
	virtual bool OnBeforeVarChange(ICVar* pVar, const char* sNewValue) = 0;

	//! Called by Console after variable has changed value.
	virtual void OnAfterVarChange(ICVar* pVar) = 0;

	//! Called by Console after variable has been unregistered.
	virtual void OnVarUnregister(ICVar* pVar) = 0;
	// </interfuscator:shuffle>
};

#if defined(GetCommandLine)
	#undef GetCommandLine
#endif

//! Interface to the arguments of the console command.
struct IConsoleCmdArgs
{
	// <interfuscator:shuffle>
	virtual ~IConsoleCmdArgs(){}

	//! Gets number of arguments supplied to the command (including the command itself).
	virtual int GetArgCount() const = 0;

	//! Gets argument by index, nIndex must be in 0 <= nIndex < GetArgCount().
	virtual const char* GetArg(int nIndex) const = 0;

	//! Gets complete command line.
	virtual const char* GetCommandLine() const = 0;
	// </interfuscator:shuffle>
};

//! Interface to the arguments of the console command.
struct IConsoleArgumentAutoComplete
{
	// <interfuscator:shuffle>
	virtual ~IConsoleArgumentAutoComplete(){}

	//! Gets number of matches for the argument to auto complete.
	virtual int GetCount() const = 0;

	//! Gets argument value by index, nIndex must be in 0 <= nIndex < GetCount().
	virtual const char* GetValue(int nIndex) const = 0;
	// </interfuscator:shuffle>
};

//! This a definition of the console command function that can be added to console with AddCommand.
typedef void (* ConsoleCommandFunc)(IConsoleCmdArgs*);

//! This a definition of the callback function that is called when variable change.
typedef void (* ConsoleVarFunc)(ICVar*);

struct IManagedConsoleCommandListener
{
	virtual ~IManagedConsoleCommandListener() {};
	virtual void OnManagedConsoleCommandEvent(const char* commandName, IConsoleCmdArgs* consoleCommandArguments) = 0;
};

namespace Detail { struct ConsoleRegistrationHelper; }

//! Interface to the engine console.
//! The engine console allow to manipulate the internal engine parameters and to invoke commands.
//! This interface allow external modules to integrate their functionalities into the console as commands or variables.
//! \note The console takes advantage of the script engine to store the console variables,
//!		 this mean that all variables visible through script and console.
struct IConsole
{
	// <interfuscator:shuffle>
	virtual ~IConsole(){}

	//! Deletes the console.
	virtual void Release() = 0;

	//! Remove a variable from the console.
	//! \param sVarName Console variable name.
	//! \param bDelete If true, the variable is deleted.
	//! \see ICVar.
	//! \par Example
	//! \include CrySystem/Examples/ConsoleVariable.cpp
	virtual void UnregisterVariable(const char* sVarName, bool bDelete = false) = 0;

	//! Set the y coordinate where the console will stop to scroll when is dropped.
	//! \param value Y in screen coordinates.
	virtual void SetScrollMax(int value) = 0;

	//! Add output sink (clases which are interested in the output) - order is not guaranteed.
	//! \param inpSink Must not be 0 and is not allowed to be added twice.
	virtual void AddOutputPrintSink(IOutputPrintSink* inpSink) = 0;

	//! Remove output sink (clases which are interested in the output) - order is not guaranteed.
	//! \param inpSink Must not be 0 and has to be added before.
	virtual void RemoveOutputPrintSink(IOutputPrintSink* inpSink) = 0;

	//! Show/hide the console.
	//! \param specifies if the window must be (true=show,false=hide).
	//! \param specifies iRequestScrollMax <=0 if not used, otherwise it sets SetScrollMax temporary to the given value.
	virtual void ShowConsole(bool show, const int iRequestScrollMax = -1) = 0;

	//! Dump all console-variables to a callback-interface.
	//! \param Callback Callback-interface which needs to be called for each element.
	virtual void DumpCVars(ICVarDumpSink* pCallback, unsigned int nFlagsFilter = 0) = 0;

	//! Bind a console command to a key.
	//! \param sCmd console command that must be executed.
	//! \param sRes name of the key to invoke the command.
	virtual void CreateKeyBind(const char* sCmd, const char* sRes) = 0;

	//! Sets the background-image.
	//! \param pImage background-image.
	virtual void SetImage(ITexture* pImage, bool bDeleteCurrent) = 0;

	//! Retrieves the background-image.
	//! \return background-image.
	virtual ITexture* GetImage() = 0;

	//! Sets static/scroll background-mode.
	//! \param bStatic true if static.
	virtual void StaticBackground(bool bStatic) = 0;

	//! Sets the loading-background-image.
	//! \param pImage Background image.
	virtual void SetLoadingImage(const char* szFilename) = 0;

	//! Iterate through the lines - used for dedicated server (truncated if needed).
	//! \param indwLineNo 0.. counted from the last printed line on.
	//! \param outszBuffer Pointer to the destination string buffer (zero terminated afterwards), must not be 0.
	//! \param indwBufferSize 1.. size of the buffer.
	//! \return true if line was returned, false=there are no more lines.

	virtual bool GetLineNo(const int indwLineNo, char* outszBuffer, const int indwBufferSize) const = 0;

	//! \return Current number of lines in the console.
	virtual int GetLineCount() const = 0;

	//! Retrieve a console variable by name - not case sensitive.
	//! \param name variable name.
	//! \return a pointer to the ICVar interface, NULL if is not found.
	//! \see ICVar.
	//! \par Example
	//! \include CrySystem/Examples/ConsoleVariable.cpp
	virtual ICVar* GetCVar(const char* name) = 0;

	//! Read a value from a configuration file (.ini) and return the value.
	//! \param szVarName Variable name.
	//! \param szFileName Source configuration file.
	//! \param def_val Default value (if the variable is not found into the file).
	//! \return The variable value.
	virtual char* GetVariable(const char* szVarName, const char* szFileName, const char* def_val) = 0;

	//! Read a value from a configuration file (.ini) and return the value.
	//! \param szVarName variable name.
	//! \param szFileName source configuration file.
	//! \param def_val default value (if the variable is not found into the file).
	//! \return the variable value.
	virtual float GetVariable(const char* szVarName, const char* szFileName, float def_val) = 0;

	//! Print a string in the console and go to the new line.
	//! \param s The string to print.
	virtual void PrintLine(const char* s) = 0;

	//! Append a string in the last console line.
	//! \param s The string to print.
	virtual void PrintLinePlus(const char* s) = 0;

	//! Retrieve the status of the console (active/not active).
	//! \return the variable value(true = active/false = not active).
	virtual bool GetStatus() = 0;

	//! Clear the console text.
	virtual void Clear() = 0;

	//! Update the console.
	virtual void Update() = 0;

	//! Draw the console.
	virtual void Draw() = 0;

	virtual void RegisterListener(IManagedConsoleCommandListener* pListener, const char* name) = 0;
	virtual void UnregisterListener(IManagedConsoleCommandListener* pListener) = 0;

	//! Removes a console command which was previously registered with AddCommand.
	//! \param sCommand Command name.
	//! \par Example
	//! \include CrySystem/Examples/ConsoleCommand.cpp
	virtual void RemoveCommand(const char* sName) = 0;

	//! Execute a string in the console.
	//! \param command Console command e.g. "map testy" - no leading slash
	//! \param bSilentMode true=suppresses log in error case and logging the command to the console
	//! \param bDeferExecution true=the command is stored in special fifo that allows delayed execution by using wait_seconds and wait_frames commands
	//! \par Example
	//! \include CrySystem/Examples/ConsoleCommand.cpp
	virtual void ExecuteString(const char* command, const bool bSilentMode = false, const bool bDeferExecution = false) = 0;

	//! Print a message into the log and abort the execution of the application.
	//! \param message error string to print in the log.
	virtual void Exit(const char* command, ...) PRINTF_PARAMS(2, 3) = 0;

	//! Return true if the console is opened.
	//! \return the variable value(true = opened/false = closed).
	virtual bool IsOpened() = 0;

	//! Auto completion.
	virtual size_t GetNumVars(bool bIncludeCommands = false) const = 0;

	//! \param szPrefix - 0 or prefix e.g. "sys_spec_".
	//! \return Used size.
	//! nListType = 0 get all values, nListTypes=1 return only cvars, nListTypes=2 return only console commands
	virtual size_t      GetSortedVars(const char** pszArray, size_t numItems, const char* szPrefix = 0, int nListTypes = 0) const = 0;
	virtual const char* AutoComplete(const char* substr) = 0;
	virtual const char* AutoCompletePrev(const char* substr) = 0;
	virtual const char* ProcessCompletion(const char* szInputBuffer) = 0;
	virtual void        RegisterAutoComplete(const char* sVarOrCommand, IConsoleArgumentAutoComplete* pArgAutoComplete) = 0;
	virtual void        UnRegisterAutoComplete(const char* sVarOrCommand) = 0;
	virtual void        ResetAutoCompletion() = 0;

	//! Calculation of the memory used by the whole console system.
	virtual void GetMemoryUsage(ICrySizer* pSizer) const = 0;

	//! Function related to progress bar.
	virtual void ResetProgressBar(int nProgressRange) = 0;

	//! Function related to progress bar.
	virtual void TickProgressBar() = 0;

	//! \param szLine Must not be 0.
	virtual void SetInputLine(const char* szLine) = 0;

	//! Set Console Editable
	virtual void SetReadOnly(bool readonly) = 0;
	virtual bool IsReadOnly() = 0;

	//! Dump all key bindings to a callback-interface.
	//! \param Callback callback-interface which needs to be called for each element.
	virtual void        DumpKeyBinds(IKeyBindDumpSink* pCallback) = 0;
	virtual const char* FindKeyBind(const char* sCmd) const = 0;

	// Hashing of cvars (for anti-cheat). Separates setting of range, calculation,
	// and retrieval of result so calculation can be done at a known safe point
	// (e.g end of frame) when we know cvars won't be modified or in temporary state.

	//! Get Number of cvars that can be hashed.
	virtual int GetNumCheatVars() = 0;

	//! Set the range of cvars.
	virtual void SetCheatVarHashRange(size_t firstVar, size_t lastVar) = 0;

	//! Calculate the hash from current cvars.
	virtual void CalcCheatVarHash() = 0;

	//! Since hash is calculated async, check if it's completed.
	virtual bool IsHashCalculated() = 0;

	//! Get the hash calculated.
	virtual uint64 GetCheatVarHash() = 0;
	virtual void   PrintCheatVars(bool bUseLastHashRange) = 0;
	virtual char*  GetCheatVarAt(uint32 nOffset) = 0;

	//! Console variable sink.
	//! Adds a new console variables sink callback.
	virtual void AddConsoleVarSink(IConsoleVarSink* pSink) = 0;

	//! Removes a console variables sink callback.
	virtual void RemoveConsoleVarSink(IConsoleVarSink* pSink) = 0;

	//////////////////////////////////////////////////////////////////////////
	// History
	//////////////////////////////////////////////////////////////////////////

	//! \param bUpOrDown true=after pressed "up", false=after pressed "down".
	//! \return 0 if there is no history line or pointer to the null terminated history line.
	virtual const char* GetHistoryElement(const bool bUpOrDown) = 0;

	//! \param szCommand Must not be 0.
	virtual void                   AddCommandToHistory(const char* szCommand) = 0;

	//! Sets the value of a CVar as loaded from a config
	//! Will defer setting of the value until the CVar is registered if it hasn't been already
	virtual void                   LoadConfigVar(const char* sVariable, const char* sValue) = 0;
	//! Executes a command with optional arguments
	//! Will defer setting of the value until the command is registered if it hasn't been already
	virtual void                   LoadConfigCommand(const char* szCommand, const char* szArguments = nullptr) = 0;

	// Sets how to treat calls to LoadConfigVar, return previous configuration type.
	virtual ELoadConfigurationType SetCurrentConfigType(ELoadConfigurationType configType) = 0;

	//! Enable or disable the activation key (tilde by default).
	//! This is useful when user is in a text field and want to be able to enter the default key.
	//! \param BEnable If true, console will show.
	virtual void EnableActivationKey(bool bEnable) = 0;

	//! Save/Load internal state (CVars) - used by LiveCreate.
	virtual void SaveInternalState(struct IDataWriteStream& writer) const = 0;
	virtual void LoadInternalState(struct IDataReadStream& reader) = 0;

	// </interfuscator:shuffle>

#if defined(DEDICATED_SERVER)
	virtual void SetClientDataProbeString(const char* pName, const char* pValue) = 0;
#endif

protected:
	friend struct ConsoleRegistrationHelper;

	//! Register a new console command.
	//! \param sCommand Command name.
	//! \param func     Pointer to the console command function to be called when command is invoked.
	//! \param nFlags   Bitfield consisting of VF_ flags (e.g. VF_CHEAT).
	//! \param sHelp    Help string, will be displayed when typing in console "command ?".
	//! \par Example
	//! \include CrySystem/Examples/ConsoleCommand.cpp
	virtual void AddCommand(const char* sCommand, ConsoleCommandFunc func, int nFlags = 0, const char* sHelp = NULL, bool bIsManaged = false) = 0;

	//! Register a new console command that execute script function.
	//! EG "Game.Connect(%1)" the symbol "%1" will be replaced with the command parameter 1
	//! writing in the console "connect 127.0.0.1" will invoke Game.Connect("127.0.0.1").
	//! \param sCommand    Command name.
	//! \param sScriptFunc Script function to be executed when command is invoked.
	//! \param nFlags      Bitfield consist of VF_ flags (e.g. VF_CHEAT).
	//! \param sHelp       Help string, will be displayed when typing in console "command ?".
	//! \par Example
	//! \include CrySystem/Examples/ConsoleCommand.cpp
	virtual void AddCommand(const char* sName, const char* sScriptFunc, int nFlags = 0, const char* sHelp = NULL) = 0;

	//! Create a new console variable that store the value in a string.
	//! \param sName Console variable name.
	//! \param sValue Default value.
	//! \param nFlags User defined flag, this parameter is used by other subsystems and doesn't affect the console variable (basically of user data).
	//! \param help Help text that is shown when you use "<sName> ?" in the console.
	//! \return Pointer to the interface ICVar.
	virtual ICVar* RegisterString(const char* sName, const char* sValue, int nFlags, const char* help = "", ConsoleVarFunc pChangeFunc = 0) = 0;

	//! Create a new console variable that store the value in a int.
	//! \param sName Console variable name.
	//! \param iValue Default value.
	//! \param nFlags User defined flag, this parameter is used by other subsystems and doesn't affect the console variable (basically of user data).
	//! \param Help help text that is shown when you use "<sName> ?" in the console.
	//! \return Pointer to the interface ICVar.
	//! \par Example
	//! \include CrySystem/Examples/ConsoleVariable.cpp
	virtual ICVar* RegisterInt(const char* sName, int iValue, int nFlags, const char* help = "", ConsoleVarFunc pChangeFunc = 0) = 0;

	//! Create a new console variable that store the value in a int64.
	//! \param sName Console variable name.
	//! \param iValue Default value.
	//! \param nFlags Her defined flag, this parameter is used by other subsystems and doesn't affect the console variable (basically of user data).
	//! \param help - help text that is shown when you use "<sName> ?" in the console.
	//! \return Pointer to the interface ICVar.
	//! \par Example
	//! \include CrySystem/Examples/ConsoleVariable.cpp
	virtual ICVar* RegisterInt64(const char* sName, int64 iValue, int nFlags, const char* help = "", ConsoleVarFunc pChangeFunc = 0) = 0;

	//! Create a new console variable that store the value in a float.
	//! \param sName Console variable name.
	//! \param fValue Default value.
	//! \param nFlags User defined flag, this parameter is used by other subsystems and doesn't affect the console variable (basically of user data).
	//! \param help help text that is shown when you use "<sName> ?" in the console.
	//! \return Pointer to the interface ICVar.
	//! \par Example
	//! \include CrySystem/Examples/ConsoleVariable.cpp
	virtual ICVar* RegisterFloat(const char* sName, float fValue, int nFlags, const char* help = "", ConsoleVarFunc pChangeFunc = 0) = 0;

	//! Create a new console variable that will update the user defined float.
	//! \param sName Console variable name.
	//! \param src Pointer to the memory that will be updated.
	//! \param nFlags User defined flag, this parameter is used by other subsystems and doesn't affect the console variable (basically of user data).
	//! \param help Help text that is shown when you use "<sName> ?" in the console.
	//! \param allowModify Allow modification through config vars, prevents missing modifications in release mode.
	//! \return pointer to the interface ICVar.
	//! \par Example
	//! \include CrySystem/Examples/ConsoleVariable.cpp
	virtual ICVar* Register(const char* name, float* src, float defaultvalue, int nFlags = 0, const char* help = "", ConsoleVarFunc pChangeFunc = 0, bool allowModify = true) = 0;

	//! Create a new console variable that will update the user defined float.
	//! \param sName Console variable name.
	//! \param src Pointer to the memory that will be updated.
	//! \param nFlags User defined flag, this parameter is used by other subsystems and doesn't affect the console variable (basically of user data).
	//! \param help Help text that is shown when you use "<sName> ?" in the console.
	//! \param allowModify Allow modification through config vars, prevents missing modifications in release mode.
	//! \return pointer to the interface ICVar.
	//! \par Example
	//! \include CrySystem/Examples/ConsoleVariable.cpp
	virtual ICVar* Register(const char* name, int* src, int defaultvalue, int nFlags = 0, const char* help = "", ConsoleVarFunc pChangeFunc = 0, bool allowModify = true) = 0;

	//! Create a new console variable that will update the user defined float.
	//! \param sName Console variable name.
	//! \param src Pointer to the memory that will be updated.
	//! \param nFlags User defined flag, this parameter is used by other subsystems and doesn't affect the console variable (basically of user data).
	//! \param help Help text that is shown when you use "<sName> ?" in the console.
	//! \param allowModify Allow modification through config vars, prevents missing modifications in release mode.
	//! \return pointer to the interface ICVar.
	//! \par Example
	//! \include CrySystem/Examples/ConsoleVariable.cpp
	virtual ICVar* Register(const char* name, const char** src, const char* defaultvalue, int nFlags = 0, const char* help = "", ConsoleVarFunc pChangeFunc = 0, bool allowModify = true) = 0;

	//! Registers an existing console variable.
	//! Should only be used with static duration objects, object is never freed.
	//! \param pVar The existing console variable.
	//! \return Pointer to the interface ICVar (that was passed in)
	virtual ICVar* Register(ICVar* pVar) = 0;
};

//! \cond INTERNAL
//! This interface for the remote console.
struct IRemoteConsoleListener
{
	virtual ~IRemoteConsoleListener() {}

	virtual void OnConsoleCommand(const char* cmd)  {};
	virtual void OnGameplayCommand(const char* cmd) {};
};

struct IRemoteConsole
{
	virtual ~IRemoteConsole() {};

	virtual void RegisterConsoleVariables() = 0;
	virtual void UnregisterConsoleVariables() = 0;

	virtual void Start() = 0;
	virtual void Stop() = 0;
	virtual bool IsStarted() const = 0;

	virtual void AddLogMessage(const char* log) = 0;
	virtual void AddLogWarning(const char* log) = 0;
	virtual void AddLogError(const char* log) = 0;

	virtual void Update() = 0;

	virtual void RegisterListener(IRemoteConsoleListener* pListener, const char* name) = 0;
	virtual void UnregisterListener(IRemoteConsoleListener* pListener) = 0;
};
//! \endcond

//! This interface is the 1:1 "C++ representation" of a console variable.
//! \note A console variable is accessible in C++ trough this interface and in all
//! scripts as global variable (with the same name of the variable in the console).
struct ICVar
{
	enum EConsoleLogMode
	{
		eCLM_Off,               //!< Off.
		eCLM_ConsoleAndFile,    //!< Normal info to console and file.
		eCLM_FileOnly,          //!< Normal info to file only.
		eCLM_FullInfo           //!< Full info to file only.
	};
	typedef std::function<void(void)> CallbackFunction;

	// <interfuscator:shuffle>
	// TODO make protected;
	virtual ~ICVar() {}

	//! Delete the variable.
	//! \note The variable will automatically unregister itself from the console.
	virtual void Release() = 0;

	//! \return Value of the variable as an integer.
	virtual int GetIVal() const = 0;

	//! \return The 64-bit integer value of the variable.
	virtual int64 GetI64Val() const = 0;

	//! \return The float value of the variable.
	virtual float GetFVal() const = 0;

	//! \note Don't store pointer as multiple calls to this function might return same memory ptr.
	//! \return The string value of the variable.
	virtual const char* GetString() const = 0;

	//! \note Don't store pointer as multiple calls to this function might return same memory ptr.
	//! \return The data probe string value of the variable.
	virtual const char* GetDataProbeString() const = 0;

	//! Set the string value of the variable.
	//! \param s String representation the value.
	virtual void Set(const char* s) = 0;

	//! Force to set the string value of the variable - can only be called from inside code.
	//! \param s String representation the value.
	virtual void ForceSet(const char* s) = 0;

	//! Set the float value of the variable.
	//! \param s Float representation the value.
	virtual void Set(const float f) = 0;

	//! Set the float value of the variable.
	//! \param s integer representation the value.
	virtual void Set(const int i) = 0;

	//! Clear the specified bits in the flag field.
	virtual void ClearFlags(int flags) = 0;

	//! Return the variable's flags.
	//! \return the variable's flags.
	virtual int GetFlags() const = 0;

	//! Set the variable's flags.
	virtual int SetFlags(int flags) = 0;

	//! \return the primary variable's type, e.g. CVAR_INT, CVAR_FLOAT, CVAR_STRING.
	virtual int GetType() = 0;

	//! \return The variable's name.
	virtual const char* GetName() const = 0;

	//! \return The variable's help text, can be NULL if no help is available.
	virtual const char* GetHelp() = 0;

	//! \return true if the variable may be modified in config files.
	virtual bool IsConstCVar() const = 0;

	//! Set a new on change function callback.
	//! \note Deprecated function. The functor should be preferred.
	virtual void SetOnChangeCallback(ConsoleVarFunc pChangeFunc) = 0;

	//! Adds a new on change functor to the list.
	//! It will add from index 1 on (0 is reserved).
	virtual uint64 AddOnChangeFunctor(const SFunctor& pChangeFunctor) = 0;

	//! Adds a new on change callback function to the cvar.
	//! It will add from index 1 on (0 is reserved).
	uint64 AddOnChange(const CallbackFunction &callback) { return AddOnChangeFunctor(SFunctor(callback)); };

	//!  \return The number of registered on change functos.
	virtual uint64 GetNumberOfOnChangeFunctors() const = 0;

	//! Returns the number of registered on change functors.
	virtual const SFunctor& GetOnChangeFunctor(uint64 nFunctorIndex) const = 0;

	//! Removes an on change functor.
	//! \return true if removal was successful.
	virtual bool RemoveOnChangeFunctor(const uint64 nElement) = 0;

	//! Get the current callback function.
	virtual ConsoleVarFunc GetOnChangeCallback() const = 0;

	virtual void           GetMemoryUsage(class ICrySizer* pSizer) const = 0;

	//! Only useful for CVarGroups, other types return GetIVal().
	//! CVarGroups set multiple other CVars and this function returns
	//! the integer value the CVarGroup should have, when looking at the controlled cvars.
	//! \return Value that would represent the state, -1 if the state cannot be found
	virtual int GetRealIVal() const = 0;

	//! Log difference between expected state and real state. Only useful for CVarGroups.
	virtual void DebugLog(const int iExpectedValue, const EConsoleLogMode mode) const {}
	// </interfuscator:shuffle>

#if defined(DEDICATED_SERVER)
	//! Set the data probe string value of the variable.
	virtual void SetDataProbeString(const char* pDataProbeString) = 0;
#endif
};

struct ScopedConsoleLoadConfigType
{
	ScopedConsoleLoadConfigType(IConsole* pConsole, ELoadConfigurationType configType)
		: m_pConsole(pConsole)
	{
		m_prevType = pConsole->SetCurrentConfigType(configType);
	};
	~ScopedConsoleLoadConfigType() { m_pConsole->SetCurrentConfigType(m_prevType); }
private:
	IConsole*              m_pConsole;
	ELoadConfigurationType m_prevType;
};

#endif //_ICONSOLE_H_

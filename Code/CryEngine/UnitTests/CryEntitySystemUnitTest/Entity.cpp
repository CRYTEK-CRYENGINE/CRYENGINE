// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma warning(push)
#pragma warning(disable: 4996) //disabled the deprecation warning for now

#include <CryCore/Platform/platform.h>
#include <UnitTest.h>

#include <CryPhysics/IPhysics.h>
#include "EntitySystem.h"
#include "Entity.h"
#if !CRY_PLATFORM_ORBIS //for monolithic orbis unit testing we don't need to include this, otherwise it is required to be included once for each module
	#include <CryParticleSystem/ParticleParams_TypeInfo.h>
#endif
#include "EntityClassRegistry.h"
#include "ScriptBind_Entity.h"
#include "EntityIt.h"
#include "CryGame/IGameFramework.h"
#include "CryAnimation/ICryMannequin.h"
#include <CryEntitySystem/IEntityComponent.h>

using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;

class CMockConsole : public IConsole
{
	// Inherited via IConsole
	virtual void Release() override
	{
	}
	virtual void UnregisterVariable(const char * sVarName, bool bDelete = false) override
	{
	}
	virtual void SetScrollMax(int value) override
	{
	}
	virtual void AddOutputPrintSink(IOutputPrintSink * inpSink) override
	{
	}
	virtual void RemoveOutputPrintSink(IOutputPrintSink * inpSink) override
	{
	}
	virtual void ShowConsole(bool show, const int iRequestScrollMax = -1) override
	{
	}
	virtual void DumpCVars(ICVarDumpSink * pCallback, unsigned int nFlagsFilter = 0) override
	{
	}
	virtual void CreateKeyBind(const char * sCmd, const char * sRes) override
	{
	}
	virtual void SetImage(ITexture * pImage, bool bDeleteCurrent) override
	{
	}
	virtual ITexture * GetImage() override
	{
		return nullptr;
	}
	virtual void StaticBackground(bool bStatic) override
	{
	}
	virtual void SetLoadingImage(const char * szFilename) override
	{
	}
	virtual bool GetLineNo(const int indwLineNo, char * outszBuffer, const int indwBufferSize) const override
	{
		return false;
	}
	virtual int GetLineCount() const override
	{
		return 0;
	}
	virtual ICVar * GetCVar(const char * name) override
	{
		return nullptr;
	}
	virtual void PrintLine(const char * s) override
	{
	}
	virtual void PrintLinePlus(const char * s) override
	{
	}
	virtual bool GetStatus() override
	{
		return false;
	}
	virtual void Clear() override
	{
	}
	virtual void Update() override
	{
	}
	virtual void Draw() override
	{
	}
	virtual void RegisterListener(IManagedConsoleCommandListener * pListener, const char * name) override
	{
	}
	virtual void UnregisterListener(IManagedConsoleCommandListener * pListener) override
	{
	}
	virtual void RemoveCommand(const char * sName) override
	{
	}
	virtual void ExecuteString(const char * command, const bool bSilentMode = false, const bool bDeferExecution = false) override
	{
	}
	virtual bool IsOpened() override
	{
		return false;
	}
	virtual size_t GetNumVars(bool bIncludeCommands = false) const override
	{
		return size_t();
	}
	virtual size_t GetSortedVars(const char ** pszArray, size_t numItems, const char * szPrefix = 0, int nListTypes = 0) const override
	{
		return size_t();
	}
	virtual const char * AutoComplete(const char * substr) override
	{
		return nullptr;
	}
	virtual const char * AutoCompletePrev(const char * substr) override
	{
		return nullptr;
	}
	virtual const char * ProcessCompletion(const char * szInputBuffer) override
	{
		return nullptr;
	}
	virtual void RegisterAutoComplete(const char * sVarOrCommand, IConsoleArgumentAutoComplete * pArgAutoComplete) override
	{
	}
	virtual void UnRegisterAutoComplete(const char * sVarOrCommand) override
	{
	}
	virtual void ResetAutoCompletion() override
	{
	}
	virtual void GetMemoryUsage(ICrySizer * pSizer) const override
	{
	}
	virtual void ResetProgressBar(int nProgressRange) override
	{
	}
	virtual void TickProgressBar() override
	{
	}
	virtual void SetInputLine(const char * szLine) override
	{
	}
	virtual void SetReadOnly(bool readonly) override
	{
	}
	virtual bool IsReadOnly() override
	{
		return false;
	}
	virtual void DumpKeyBinds(IKeyBindDumpSink * pCallback) override
	{
	}
	virtual const char * FindKeyBind(const char * sCmd) const override
	{
		return nullptr;
	}
	virtual int GetNumCheatVars() override
	{
		return 0;
	}
	virtual void SetCheatVarHashRange(size_t firstVar, size_t lastVar) override
	{
	}
	virtual void CalcCheatVarHash() override
	{
	}
	virtual bool IsHashCalculated() override
	{
		return false;
	}
	virtual uint64 GetCheatVarHash() override
	{
		return uint64();
	}
	virtual void PrintCheatVars(bool bUseLastHashRange) override
	{
	}
	virtual const char * GetCheatVarAt(uint32 nOffset) override
	{
		return nullptr;
	}
	virtual void AddConsoleVarSink(IConsoleVarSink * pSink) override
	{
	}
	virtual void RemoveConsoleVarSink(IConsoleVarSink * pSink) override
	{
	}
	virtual bool OnBeforeVarChange(ICVar* pVar, const char* sNewValue) override 
	{
		return false;
	}
	virtual void OnAfterVarChange(ICVar* pVar) override 
	{
	}
	virtual const char * GetHistoryElement(const bool bUpOrDown) override
	{
		return nullptr;
	}
	virtual void AddCommandToHistory(const char * szCommand) override
	{
	}
	virtual void LoadConfigVar(const char * sVariable, const char * sValue) override
	{
	}
	virtual void LoadConfigCommand(const char * szCommand, const char * szArguments = nullptr) override
	{
	}
	virtual ELoadConfigurationType SetCurrentConfigType(ELoadConfigurationType configType) override
	{
		return ELoadConfigurationType();
	}
	virtual void EnableActivationKey(bool bEnable) override
	{
	}
	virtual void SaveInternalState(IDataWriteStream & writer) const override
	{
	}
	virtual void LoadInternalState(IDataReadStream & reader) override
	{
	}
	virtual void AddCommand(const char * sCommand, ConsoleCommandFunc func, int nFlags = 0, const char * sHelp = NULL, bool bIsManaged = false) override
	{
	}
	virtual void AddCommand(const char * sName, const char * sScriptFunc, int nFlags = 0, const char * sHelp = NULL) override
	{
	}
	virtual ICVar * RegisterString(const char * sName, const char * sValue, int nFlags, const char * help = "", ConsoleVarFunc pChangeFunc = 0) override
	{
		return nullptr;
	}
	virtual ICVar * RegisterInt(const char * sName, int iValue, int nFlags, const char * help = "", ConsoleVarFunc pChangeFunc = 0) override
	{
		return nullptr;
	}
	virtual ICVar * RegisterInt64(const char * sName, int64 iValue, int nFlags, const char * help = "", ConsoleVarFunc pChangeFunc = 0) override
	{
		return nullptr;
	}
	virtual ICVar * RegisterFloat(const char * sName, float fValue, int nFlags, const char * help = "", ConsoleVarFunc pChangeFunc = 0) override
	{
		return nullptr;
	}
	virtual ICVar * Register(const char * name, float * src, float defaultvalue, int nFlags = 0, const char * help = "", ConsoleVarFunc pChangeFunc = 0, bool allowModify = true) override
	{
		return nullptr;
	}
	virtual ICVar * Register(const char * name, int * src, int defaultvalue, int nFlags = 0, const char * help = "", ConsoleVarFunc pChangeFunc = 0, bool allowModify = true) override
	{
		return nullptr;
	}
	virtual ICVar * Register(const char * name, const char ** src, const char * defaultvalue, int nFlags = 0, const char * help = "", ConsoleVarFunc pChangeFunc = 0, bool allowModify = true) override
	{
		return nullptr;
	}
	virtual ICVar * Register(ICVar * pVar) override
	{
		return nullptr;
	}
	virtual void Exit(const char *command, ...) override
	{
	}
};

class CMockCryPak : public ICryPak
{
	// Inherited via ICryPak
	virtual void AdjustFileName(const char * src, CryPathString& dst, unsigned nFlags) override
	{
	}
	virtual bool Init(const char * szBasePath) override
	{
		return false;
	}
	virtual void Release() override
	{
	}
	virtual bool IsInstalledToHDD(const char * acFilePath = 0) const override
	{
		return false;
	}
	virtual bool OpenPack(const char * pName, unsigned nFlags = FLAGS_PATH_REAL, IMemoryBlock * pData = 0, CryPathString* pFullPath = 0) override
	{
		return false;
	}
	virtual bool OpenPack(const char * pBindingRoot, const char * pName, unsigned nFlags = FLAGS_PATH_REAL, IMemoryBlock * pData = 0, CryPathString* pFullPath = 0) override
	{
		return false;
	}
	virtual bool ClosePack(const char * pName, unsigned nFlags = FLAGS_PATH_REAL) override
	{
		return false;
	}
	virtual bool OpenPacks(const char * pWildcard, unsigned nFlags = FLAGS_PATH_REAL, std::vector<CryPathString>* pFullPaths = NULL) override
	{
		return false;
	}
	virtual bool OpenPacks(const char * pBindingRoot, const char * pWildcard, unsigned nFlags = FLAGS_PATH_REAL, std::vector<CryPathString>* pFullPaths = NULL) override
	{
		return false;
	}
	virtual bool ClosePacks(const char * pWildcard, unsigned nFlags = FLAGS_PATH_REAL) override
	{
		return false;
	}
	virtual bool FindPacks(const char * pWildcardIn) override
	{
		return false;
	}
	virtual bool SetPacksAccessible(bool bAccessible, const char * pWildcard, unsigned nFlags = FLAGS_PATH_REAL) override
	{
		return false;
	}
	virtual bool SetPackAccessible(bool bAccessible, const char * pName, unsigned nFlags = FLAGS_PATH_REAL) override
	{
		return false;
	}
	virtual void SetPacksAccessibleForLevel(const char * sLevelName) override
	{
	}
	virtual bool LoadPakToMemory(const char * pName, EInMemoryPakLocation eLoadToMemory, IMemoryBlock * pMemoryBlock = NULL) override
	{
		return false;
	}
	virtual void LoadPaksToMemory(int nMaxPakSize, bool bLoadToMemory) override
	{
	}
	virtual void AddMod(const char * szMod, EModAccessPriority modAccessPriority = EModAccessPriority::BeforeSource) override
	{
	}
	virtual void RemoveMod(const char * szMod) override
	{
	}
	virtual const char * GetMod(int index) override
	{
		return nullptr;
	}
	virtual void ParseAliases(const char * szCommandLine) override
	{
	}
	virtual void SetAlias(const char * szName, const char * szAlias, bool bAdd) override
	{
	}
	virtual const char * GetAlias(const char * szName, bool bReturnSame = true) override
	{
		return nullptr;
	}
	virtual void Lock() override
	{
	}
	virtual void Unlock() override
	{
	}
	virtual void LockReadIO(bool bValue) override
	{
	}
	virtual void SetGameFolder(const char * szFolder) override
	{
	}
	virtual const char * GetGameFolder() const override
	{
		return nullptr;
	}
	virtual void SetLocalizationFolder(char const * const sLocalizationFolder) override
	{
	}
	virtual char const * const GetLocalizationFolder() const override
	{
		return nullptr;
	}
	virtual void GetCachedPakCDROffsetSize(const char * szName, uint32 & offset, uint32 & size) override
	{
	}
	virtual ICryPak::PakInfo * GetPakInfo() override
	{
		return nullptr;
	}
	virtual void FreePakInfo(PakInfo *) override
	{
	}
	virtual FILE * FOpen(const char * pName, const char * mode, unsigned nFlags = 0) override
	{
		return nullptr;
	}
	virtual FILE * FOpen(const char * pName, const char * mode, char * szFileGamePath, int nLen) override
	{
		return nullptr;
	}
	virtual FILE * FOpenRaw(const char * pName, const char * mode) override
	{
		return nullptr;
	}
	virtual size_t FReadRaw(void * data, size_t length, size_t elems, FILE * handle) override
	{
		return size_t();
	}
	virtual size_t FReadRawAll(void * data, size_t nFileSize, FILE * handle) override
	{
		return size_t();
	}
	virtual void * FGetCachedFileData(FILE * handle, size_t & nFileSize) override
	{
		return nullptr;
	}
	virtual size_t FWrite(const void * data, size_t length, size_t elems, FILE * handle) override
	{
		return size_t();
	}
	virtual char * FGets(char *, int, FILE *) override
	{
		return nullptr;
	}
	virtual int Getc(FILE *) override
	{
		return 0;
	}
	virtual size_t FGetSize(FILE * f) override
	{
		return size_t();
	}
	virtual size_t FGetSize(const char * pName, bool bAllowUseFileSystem = false) override
	{
		return size_t();
	}
	virtual int Ungetc(int c, FILE *) override
	{
		return 0;
	}
	virtual bool IsInPak(FILE * handle) override
	{
		return false;
	}
	virtual bool RemoveFile(const char * pName) override
	{
		return false;
	}
	virtual bool RemoveDir(const char * pName, bool bRecurse) override
	{
		return false;
	}
	virtual bool IsAbsPath(const char * pPath) override
	{
		return false;
	}
	virtual bool CopyFileOnDisk(const char * source, const char * dest, bool bFailIfExist) override
	{
		return false;
	}
	virtual size_t FSeek(FILE * handle, long seek, int mode) override
	{
		return size_t();
	}
	virtual long FTell(FILE * handle) override
	{
		return 0;
	}
	virtual int FClose(FILE * handle) override
	{
		return 0;
	}
	virtual int FEof(FILE * handle) override
	{
		return 0;
	}
	virtual int FError(FILE * handle) override
	{
		return 0;
	}
	virtual int FGetErrno() override
	{
		return 0;
	}
	virtual int FFlush(FILE * handle) override
	{
		return 0;
	}
	virtual void * PoolMalloc(size_t size) override
	{
		return nullptr;
	}
	virtual void PoolFree(void * p) override
	{
	}
	virtual IMemoryBlock * PoolAllocMemoryBlock(size_t nSize, const char * sUsage, size_t nAlign = 1) override
	{
		return nullptr;
	}
	virtual intptr_t FindFirst(const char * pDir, _finddata_t * fd, unsigned int nFlags = 0, bool bAllOwUseFileSystem = false) override
	{
		return -1;
	}
	virtual int FindNext(intptr_t handle, _finddata_t * fd) override
	{
		return -1;
	}
	virtual int FindClose(intptr_t handle) override
	{
		return 0;
	}
	virtual ICryPak::FileTime GetModificationTime(FILE * f) override
	{
		return ICryPak::FileTime();
	}
	virtual bool IsFileExist(const char * sFilename, EFileSearchLocation = eFileLocation_Any) override
	{
		return false;
	}
	virtual bool IsFolder(const char * sPath) override
	{
		return false;
	}
	virtual ICryPak::SignedFileSize GetFileSizeOnDisk(const char * filename) override
	{
		return ICryPak::SignedFileSize();
	}
	virtual bool IsFileCompressed(const char * filename) override
	{
		return false;
	}
	virtual bool MakeDir(const char * szPath, bool bGamePathMapping = false) override
	{
		return false;
	}
	virtual ICryArchive * OpenArchive(const char * szPath, unsigned int nFlags = 0, IMemoryBlock * pData = 0) override
	{
		return nullptr;
	}
	virtual const char * GetFileArchivePath(FILE * f) override
	{
		return nullptr;
	}
	virtual int RawCompress(const void * pUncompressed, unsigned long * pDestSize, void * pCompressed, unsigned long nSrcSize, int nLevel = -1) override
	{
		return 0;
	}
	virtual int RawUncompress(void * pUncompressed, unsigned long * pDestSize, const void * pCompressed, unsigned long nSrcSize) override
	{
		return 0;
	}
	virtual void RecordFileOpen(const ERecordFileOpenList eList) override
	{
	}
	virtual void RecordFile(FILE * in, const char * szFilename) override
	{
	}
	virtual IResourceList * GetResourceList(const ERecordFileOpenList eList) override
	{
		return nullptr;
	}
	virtual void SetResourceList(const ERecordFileOpenList eList, IResourceList * pResourceList) override
	{
	}
	virtual ICryPak::ERecordFileOpenList GetRecordFileOpenList() override
	{
		return ICryPak::ERecordFileOpenList();
	}
	virtual uint32 ComputeCRC(const char * szPath, uint32 nFileOpenFlags = 0) override
	{
		return uint32();
	}
	virtual bool ComputeMD5(const char * szPath, unsigned char * md5, uint32 nFileOpenFlags = 0) override
	{
		return false;
	}
	virtual int ComputeCachedPakCDR_CRC(const char * filename, bool useCryFile = true, IMemoryBlock * pData = NULL) override
	{
		return 0;
	}
	virtual void RegisterFileAccessSink(ICryPakFileAcesssSink * pSink) override
	{
	}
	virtual void UnregisterFileAccessSink(ICryPakFileAcesssSink * pSink) override
	{
	}
	virtual bool GetLvlResStatus() const override
	{
		return false;
	}
	virtual void DisableRuntimeFileAccess(bool status) override
	{
	}
	virtual bool DisableRuntimeFileAccess(bool status, threadID threadId) override
	{
		return false;
	}
	virtual bool CheckFileAccessDisabled(const char * name, const char * mode) override
	{
		return false;
	}
	virtual void SetRenderThreadId(threadID renderThreadId) override
	{
	}
	virtual int GetPakPriority() override
	{
		return 0;
	}
	virtual uint64 GetFileOffsetOnMedia(const char * szName) override
	{
		return uint64();
	}
	virtual EStreamSourceMediaType GetFileMediaType(const char * szName) override
	{
		return EStreamSourceMediaType();
	}
	virtual void CreatePerfHUDWidget() override
	{
	}
	virtual bool ForEachArchiveFolderEntry(const char * szArchivePath, const char * szFolderPath, const ArchiveEntrySinkFunction & callback) override
	{
		return false;
	}
	virtual int FPrintf(FILE *handle, const char *format, ...) override
	{
		return 0;
	}
};

class CEntityMockCryPak : public CMockCryPak
{
public:
	MOCK_METHOD2(IsFileExist, bool(const char* sFilename, EFileSearchLocation));
	MOCK_METHOD4(FindFirst, intptr_t(const char* pDir, _finddata_t* fd, unsigned int nFlags, bool bAllOwUseFileSystem));
	MOCK_METHOD2(FindNext, int(intptr_t handle, _finddata_t * fd));
	MOCK_METHOD1(FindClose, int(intptr_t handle));
};

class CMockTimer : public ITimer
{
public:
	MOCK_CONST_METHOD1(GetCurrTime, float(ETimer which));

	// Inherited via ITimer
	virtual void ResetTimer() override
	{
	}
	virtual void UpdateOnFrameStart() override
	{
	}
	CTimeValue dummy;
	virtual const CTimeValue & GetFrameStartTime(ETimer which = ETIMER_GAME) const override
	{
		return dummy;
	}
	virtual CTimeValue GetAsyncTime() const override
	{
		return CTimeValue();
	}
	virtual float GetAsyncCurTime() override
	{
		return 0.0f;
	}
	virtual float GetReplicationTime() const override
	{
		return 0.0f;
	}
	virtual float GetFrameTime(ETimer which = ETIMER_GAME) const override
	{
		return 0.0f;
	}
	virtual float GetRealFrameTime() const override
	{
		return 0.0f;
	}
	virtual float GetTimeScale() const override
	{
		return 0.0f;
	}
	virtual float GetTimeScale(uint32 channel) const override
	{
		return 0.0f;
	}
	virtual void ClearTimeScales() override
	{
	}
	virtual void SetTimeScale(float s, uint32 channel = 0) override
	{
	}
	virtual void EnableTimer(const bool bEnable) override
	{
	}
	virtual bool IsTimerEnabled() const override
	{
		return false;
	}
	virtual float GetFrameRate() override
	{
		return 0.0f;
	}
	virtual float GetProfileFrameBlending(float * pfBlendTime = 0, int * piBlendMode = 0) override
	{
		return 0.0f;
	}
	virtual void Serialize(TSerialize ser) override
	{
	}
	virtual bool PauseTimer(ETimer which, bool bPause) override
	{
		return false;
	}
	virtual bool IsTimerPaused(ETimer which) override
	{
		return false;
	}
	virtual bool SetTimer(ETimer which, float timeInSeconds) override
	{
		return false;
	}
	virtual void SecondsToDateUTC(time_t time, tm & outDateUTC) override
	{
	}
	virtual time_t DateToSecondsUTC(tm & timePtr) override
	{
		return time_t();
	}
	virtual float TicksToSeconds(int64 ticks) const override
	{
		return 0.0f;
	}
	virtual int64 GetTicksPerSecond() override
	{
		return int64();
	}
	virtual ITimer * CreateNewTimer() override
	{
		return nullptr;
	}
};

class CEntityTestBase : public ::testing::Test
{
protected:
	virtual void SetUp() override
	{
#if !defined(SYS_ENV_AS_STRUCT)
		memset(&m_env, 0, sizeof(SSystemGlobalEnvironment));
		gEnv = new (&m_env)SSystemGlobalEnvironment;
		SetCryEntityGlobalEnvironment(gEnv);
#endif
		gEnv->pSchematyc = nullptr;
		gEnv->pConsole = &m_mockConsole;
		gEnv->pCryPak = &m_mockCryPak;
		gEnv->pTimer = &m_mockTimer;
		
		gEnv->pCryPak = &m_mockCryPak;

		EXPECT_CALL(m_mockCryPak, IsFileExist(_, _))
			.WillRepeatedly(Return(false));

		EXPECT_CALL(m_mockCryPak, FindFirst(_, _, _, _))
			.WillRepeatedly(Return(-1));
	}

	virtual void TearDown() override
	{
	}

	CMockConsole m_mockConsole;
	CEntityMockCryPak m_mockCryPak;
	CMockTimer m_mockTimer;
	SSystemGlobalEnvironment m_env;
};


class CEntityClassRegistryUnitTest : public CEntityTestBase
{
protected:
	virtual void SetUp() override
	{
		CEntityTestBase::SetUp();
		CEntityClassRegistry* registry = new CEntityClassRegistry;
		registry->InitializeDefaultClasses();
		m_pRegistry = registry;
	}

	IEntityClassRegistry* m_pRegistry = nullptr;
};

TEST_F(CEntityClassRegistryUnitTest, Register)
{
	IEntityClassRegistry::SEntityClassDesc stdClass;
	stdClass.flags |= ECLF_INVISIBLE;
	stdClass.sName = "AreaBox";
	constexpr CryGUID guid = "{0398592E-2C80-408D-8ED7-8AB7C04A064B}"_cry_guid;
	stdClass.guid = guid;

	IEntityClass* entityClass = m_pRegistry->RegisterStdClass(stdClass);
	REQUIRE(entityClass != nullptr);
	REQUIRE(m_pRegistry->FindClass("AreaBox") == entityClass);
	REQUIRE(m_pRegistry->FindClassByGUID(guid) == entityClass);

	EXPECT_DEATH_IF_SUPPORTED(m_pRegistry->FindClass(nullptr), ".*");
	REQUIRE(m_pRegistry->FindClass("") == nullptr);
}

TEST_F(CEntityClassRegistryUnitTest, DefaultClass)
{
	IEntityClass* entityClass = m_pRegistry->GetDefaultClass();
	REQUIRE(entityClass != nullptr);
}

TEST(EntityId, RecoverElements)
{
	EntityIndex index = 0U;
	EntitySalt salt = 1U;
	SEntityIdentifier handle(salt, index);

	REQUIRE(handle.GetSalt() == salt);
	REQUIRE(handle.GetIndex() == index);

	index = 1337U;
	salt = 2U;
	handle = SEntityIdentifier(salt, index);

	REQUIRE(handle.GetSalt() == salt);
	REQUIRE(handle.GetIndex() == index);

	index = EntityArraySize - 1U;
	salt = 2U;
	handle = SEntityIdentifier(salt, index);

	REQUIRE(handle.GetSalt() == salt);
	REQUIRE(handle.GetIndex() == index);
}

TEST(SSaltBufferArray, RecoverUsedIndices)
{
	SSaltBufferArray saltBuffer;

	// Reserve entity id 1 and 2, ensure that GetMaxUsedEntityIndex is adjusted
	saltBuffer.InsertKnownHandle(1);
	REQUIRE(saltBuffer.GetMaxUsedEntityIndex() == 1);
	saltBuffer.InsertKnownHandle(2);
	REQUIRE(saltBuffer.GetMaxUsedEntityIndex() == 2);

	// Request the next available entity id
	const SEntityIdentifier firstIdentifier = saltBuffer.Insert();
	REQUIRE(firstIdentifier.GetIndex() == 3);
	// Use count must be 0
	REQUIRE(firstIdentifier.GetSalt() == 0);
	REQUIRE(saltBuffer.GetMaxUsedEntityIndex() == 3);

	// Request the next available entity id again
	const SEntityIdentifier secondIdentifier = saltBuffer.Insert();
	REQUIRE(secondIdentifier.GetIndex() == 4);
	REQUIRE(secondIdentifier.GetSalt() == 0);
	REQUIRE(saltBuffer.GetMaxUsedEntityIndex() == 4);

	// Remove the first id we requested, first checking if validity works
	REQUIRE(saltBuffer.IsValid(firstIdentifier));
	saltBuffer.Remove(firstIdentifier);
	REQUIRE(!saltBuffer.IsValid(firstIdentifier));

	// Now request the next available entity id, should be same as what was removed
	const SEntityIdentifier thirdIdentifier = saltBuffer.Insert();
	REQUIRE(thirdIdentifier.GetIndex() == firstIdentifier.GetIndex());
	REQUIRE(thirdIdentifier.GetSalt() == firstIdentifier.GetSalt() + 1);

	// Request a final identifier, ensuring that we resume from where we were before removal
	const SEntityIdentifier fourthIdentifier = saltBuffer.Insert();
	REQUIRE(fourthIdentifier.GetIndex() == 5);
	REQUIRE(fourthIdentifier.GetSalt() == 0);
	REQUIRE(saltBuffer.GetMaxUsedEntityIndex() == 5);
}

TEST(SSaltBufferArray, UseAllIndices)
{
	CEntitySystem::SEntityArray entityArray;
	REQUIRE(!entityArray.IsFull());
	REQUIRE(entityArray[0] == nullptr);

	for (EntityIndex i = 0; i < MaximumEntityCount; ++i)
	{
		const SEntityIdentifier identifier = entityArray.Insert();
		REQUIRE(entityArray[identifier.GetIndex()] == nullptr);
		REQUIRE(identifier.GetIndex() == (i + 1));
		REQUIRE(identifier.GetSalt() == 0);
		REQUIRE(entityArray.GetMaxUsedEntityIndex() == (i + 1));

		entityArray[identifier.GetIndex()] = reinterpret_cast<CEntity*>(identifier.GetIndex());
	}

	REQUIRE(entityArray[0] == nullptr);
	REQUIRE(entityArray.IsFull());
}

TEST(CEntityItMap, IterateEmpty)
{
	CEntitySystem::SEntityArray entityArray;
	REQUIRE(entityArray.GetMaxUsedEntityIndex() == 0);
	REQUIRE(entityArray.begin() == entityArray.end());

	_smart_ptr<CEntityItMap> pIterator = new CEntityItMap(entityArray);
	REQUIRE(pIterator->IsEnd());
}

TEST(CEntityItMap, SpawnAndIterate)
{
	CEntitySystem::SEntityArray entityArray;

	const uint8 numTestedEntities = 3;
	for (uint8 i = 0; i < numTestedEntities; ++i)
	{
		const SEntityIdentifier entityId = entityArray.Insert();
		entityArray[entityId.GetIndex()] = reinterpret_cast<CEntity*>(entityId.GetId());
	}

	_smart_ptr<CEntityItMap> pIterator = new CEntityItMap(entityArray);

	REQUIRE(!pIterator->IsEnd());

	for (uint8 i = 0; i < numTestedEntities; ++i)
	{
		REQUIRE(pIterator->Next() == entityArray[i + 1]);
	}

	REQUIRE(pIterator->IsEnd());
	REQUIRE(pIterator->GetReferenceCount() == 1);
}

TEST(CEntityItMap, SkipNullElements)
{
	CEntitySystem::SEntityArray entityArray;

	const uint8 numTestedEntities = 3;
	for (uint8 i = 0; i < numTestedEntities; ++i)
	{
		const SEntityIdentifier entityId = entityArray.Insert();
		entityArray[entityId.GetIndex()] = reinterpret_cast<CEntity*>(entityId.GetId());
	}

	// Remove the entity in the middle
	entityArray.Remove(SEntityIdentifier(2));
	entityArray[2] = nullptr;

	_smart_ptr<CEntityItMap> pIterator = new CEntityItMap(entityArray);

	REQUIRE(!pIterator->IsEnd());
	REQUIRE(pIterator->Next() == entityArray[1]);
	// Ensure that we skip 2, since it is now null
	REQUIRE(pIterator->Next() == entityArray[3]);

	REQUIRE(pIterator->IsEnd());
	REQUIRE(pIterator->GetReferenceCount() == 1);
}

#pragma warning(pop)

// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>
#include <CrySerialization/ClassFactory.h>
#include <CryString/CryString.h>
#include <CrySystem/ISystem.h>
#include <CryEntitySystem/IEntity.h>

#include <CryString/HashedString.h>

namespace DRS
{
struct IDialogLineDatabase;
struct IResponseManager;
struct IDataImportHelper;
struct IDialogLine;
struct IResponseActor;

struct IResponseAction;
typedef Serialization::ClassFactory<IResponseAction> ActionSerializationClassFactory;
typedef std::shared_ptr<IResponseAction>             IResponseActionSharedPtr;

struct IResponseCondition;
typedef Serialization::ClassFactory<IResponseCondition> ConditionSerializationClassFactory;
typedef std::shared_ptr<IResponseCondition>             IConditionSharedPtr;

struct IVariableCollection;
typedef std::shared_ptr<IVariableCollection> IVariableCollectionSharedPtr;

typedef int                                  SignalId;
static const SignalId s_InvalidSignalId = -1;

typedef int LipSyncID;
static const LipSyncID s_InvalidLipSyncId = -1;

//////////////////////////////////////////////////////////////////////////

struct ISpeakerManager
{
	struct ILipsyncProvider
	{
		virtual LipSyncID OnLineStarted(IResponseActor* pSpeaker, const IDialogLine* pLine) = 0;
		virtual void      OnLineEnded(LipSyncID lipsyncId, IResponseActor* pSpeaker, const IDialogLine* pLine) = 0;
		virtual bool      Update(LipSyncID lipsyncId, IResponseActor* pSpeaker, const IDialogLine* pLine) = 0;       //returns if the lip animation has finished
		//TODO: also pass in the current audio data for 'Update'
	};
	struct IListener
	{
		enum eLineEvent
		{
			eLineEvent_Started,                 //line started successfully
			eLineEvent_Queued,                  //line is waiting to start
			eLineEvent_Finished,                //line finished successfully
			eLineEvent_Canceling,               //waiting for stop trigger to finish
			eLineEvent_Canceled,                //was canceled while playing (by calling cancel or by destroying the actor)
			eLineEvent_CanceledWhileQueued,     //was canceled while queued (by calling cancel or by destroying the actor)
			eLineEvent_CouldNotBeStarted,       //was not started because of incorrect data.
			eLineEvent_SkippedBecauseOfPriority //another more important line was already playing (pLine will hold that line)
		};
		virtual void OnLineEvent(const IResponseActor* pSpeaker, const CHashedString& lineID, eLineEvent lineEvent, const IDialogLine* pLine) = 0;
	};

	virtual bool IsSpeaking(const IResponseActor* pActor, const CHashedString& lineID = CHashedString::GetEmpty()) const = 0;
	virtual bool StartSpeaking(IResponseActor* pActor, const CHashedString& lineID) = 0;
	virtual bool CancelSpeaking(const IResponseActor* pActor, int maxPrioToCancel = -1) = 0;
	virtual bool AddListener(IListener* pListener) = 0;
	virtual bool RemoveListener(IListener* pListener) = 0;
	virtual void SetCustomLipsyncProvider(ILipsyncProvider* pProvider) = 0;
};

//////////////////////////////////////////////////////////////////////////

struct IVariable
{
	virtual const CHashedString& GetName() const = 0;

	virtual void                 SetValue(int newValue) = 0;
	virtual void                 SetValue(float newValue) = 0;
	virtual void                 SetValue(const CHashedString& newValue) = 0;
	virtual void                 SetValue(bool newValue) = 0;

	//REMARK: Fetching values as wrong type, will log warnings in non-release builds. But you will still get the misinterpreted data.
	//So if you set the value to 1(int) and then call GetValueAsFloat you wont get 1.0f! So avoid changing the type of variables
	virtual int           GetValueAsInt() const = 0;
	virtual float         GetValueAsFloat() const = 0;
	virtual CHashedString GetValueAsHashedString() const = 0;
	virtual bool          GetValueAsBool() const = 0;
};

//////////////////////////////////////////////////////////////////////////

struct IVariableCollection
{
	virtual ~IVariableCollection() {}

	/**
	 * Creates a new variable in this VariableCollection and sets it to the specified initial value.
	 * REMARK: Creating variables with names that already exist will cause undefined behavior.
	 *
	 * Note: CreateVariable("VariableName", int/float/string InitialValue)
	 * @param name - unique name of the new variable to be generated. Must not exist in the collection before
	 * @param initial value - the initial value (of type int, float or const char*) of the newly generated variable. the string pointer can be released after calling this function.
	 * @return Returns a pointer to the newly generated variable. The actual type of the variable depends on the implementation of the DRS. Could be a class or just an identifier.
	 * @see SetVariableValue, GetVariable
	 */
	virtual IVariable* CreateVariable(const CHashedString& name, int initialValue) = 0;
	virtual IVariable* CreateVariable(const CHashedString& name, float initialValue) = 0;
	virtual IVariable* CreateVariable(const CHashedString& name, bool initialValue) = 0;
	virtual IVariable* CreateVariable(const CHashedString& name, const CHashedString& initialValue) = 0;

	/**
	 * Fetches the variable with the given name. Optionally (if the third parameter is set to true) will create the Variable if not existing. Will then change the value of the variable to the given value.
	 * The value-type of the variable and the value-type of the parameter needs to fit. Otherwise the behavior is undefined.
	 * HINT: If you going to change the value of this variable a lot, consider storing a pointer to the variable and use the SetVariableValue which takes a variable-pointer instead. It avoids the cost of the search.
	 *
	 * Note: SetVariableValue("Health", 0.5f, true, 0.0f)
	 * @param name - the name of the variable to set. If the variable is not existing and createIfNotExisting is true, it will be created.
	 * @param newValue - the value to set. The type of the newValue must match the previous type of the variable
	 * @param createIfNotExisting - should the variable be created if not existing. If set to "false" and if the variable is not existing, the set-operation will fail and return false.
	 * @param resetTime - defines the time, after which the setValue is changed backed to the original value. Specify 0.0f (or less) to set it permanently. Can be used as cooldowns to prevent spaming of responses.
	 * @return Returns if the Set operation was successful.
	 * @see CreateVariable, SetVariableValue (by Variable-Pointer), GetVariable
	 */
	virtual bool SetVariableValue(const CHashedString& name, int newValue, bool createIfNotExisting = true, float resetTime = -1.0f) = 0;
	virtual bool SetVariableValue(const CHashedString& name, float newValue, bool createIfNotExisting = true, float resetTime = -1.0f) = 0;
	virtual bool SetVariableValue(const CHashedString& name, bool newValue, bool createIfNotExisting = true, float resetTime = -1.0f) = 0;
	virtual bool SetVariableValue(const CHashedString& name, const CHashedString& newValue, bool createIfNotExisting = true, float resetTime = -1.0f) = 0;

	/**
	 * Will Fetch the variable from the collection with the specified name.
	 *
	 * Note: ResponseVariable* pOftenUsedVariable = GetVariable("Health");
	 * @return Returns a pointer the variable, or NULL if no variable with the given name exists in this collection.
	 * @see CreateVariable, SetVariableValue (by Variable Pointer), SetVariableValue (by Variablename)
	 */
	virtual IVariable* GetVariable(const CHashedString& name) const = 0;

	/**
	 * Will return the name of the collection.
	 *
	 * Note: string collectionNameAsString = m_pMyCollection->GetName().GetText();
	 * @return Returns the identifier that the DRS gave to this Variable collection.
	 */
	virtual const CHashedString& GetName() const = 0;

	//! Simple way to store some additional user data with the variable collection. Might be useful, if you need to pass a string along with a signal. The string will be copied.
	virtual void          SetUserString(const char* szUserString) = 0;
	//! Returns the additional user data string
	virtual const string& GetUserString() = 0;

	/**
	 * Serializes all the member variables of this class
	 *
	 * Note: Called by the serialization library to serialize to disk or to the UI
	 */
	virtual void Serialize(Serialization::IArchive& ar) = 0;

private:
	//to prevent implicit casts of char-ptr to bool, instead of hashedStrings
	bool       SetVariableValue(const CHashedString& name, const char* newValue, bool createIfNotExisting = true, float resetTime = -1.0f) { return false; }
	IVariable* CreateVariable(const CHashedString& name, const char* initialValue)                                                         { return nullptr; }
};

//////////////////////////////////////////////////////////////////////////

struct IResponseInstance
{
	virtual ~IResponseInstance() {}

	/**
	 * Will return the ResponseActor that is currently active in the ResponseInstance
	 *
	 * Note: pInstance->GetCurrentActor()
	 * @return Return the ResponseActor that is currently active in the ResponseInstance
	 * @see IResponseAction::Execute, IResponseActor
	 */
	virtual IResponseActor* GetCurrentActor() const = 0;

	/**
	 * With this method the currently active Actor of the response instance can be changed.
	 * all following actions and conditions will therefore use the new actor.
	 *
	 * Note: pInstance->SetCurrentResponder(pActorToChangeTo)
	 * @param pNewActor - the actor that should be the new active object for the response
	 * @see IDynamicResponseSystem::GetResponseActor
	 */
	virtual void SetCurrentActor(IResponseActor* pNewActor) = 0;

	/**
	 * Will return the EesponseActor that fired the signal in the first place
	 *
	 * Note: pInstance->GetOriginalSender()
	 * @return return the EesponseActor that fired the signal in the first place
	 * @see IResponseAction::Execute, IDynamicResponseSystem::QueueSignal
	 */
	virtual IResponseActor* const GetOriginalSender() const = 0;

	/**
	 * Will return the name of the signal that we currently respond to.
	 *
	 * Note: pInstance->GetSignalName()
	 * @return returns the name of the signal that we currently respond to.
	 * @see IResponseAction::Execute, IDynamicResponseSystem::QueueSignal
	 */
	virtual const CHashedString& GetSignalName() const = 0;

	/**
	 * Will return the context variable collection for this signal (if there was one specified when the signal was queued)
	 *
	 * Note: pInstance->GetContextVariables()
	 * @return return the context variable collection for this signal (if there is one)
	 * @see IResponseAction::Execute, IDynamicResponseSystem::QueueSignal
	 */
	virtual IVariableCollectionSharedPtr GetContextVariables() const = 0;
};

//////////////////////////////////////////////////////////////////////////

struct IDynamicResponseSystem
{
public:
	virtual ~IDynamicResponseSystem() {}

	/**
	 * Will load all response definitions from the specified folder. Will also create all needed subsystems
	 *
	 * Note: pDRS->Init(pPath)
	 * @param pFilesFolder - the folder where the response definition files are located
	 * @return return if responses were loaded
	 */
	virtual bool Init(const char* pFilesFolder) = 0;

	/**
	 * Will re-load all response definitions from the specified folder. Might be used for reloading-on-the-fly in the editor
	 *
	 * Note: pDRS->ReInit(pPath)
	 * @return return if responses were loaded
	 */
	virtual bool ReInit() = 0;

	/**
	 * Will update all systems that are part of DRS. (signal handling, updating of running response instances, variable reset timers...)
	 * Needs to be called continuously.
	 *
	 * Note: pDRS->Init(pPath)
	 */
	virtual void Update() = 0;

	/**
	 * Will create a new variable collection with the specified name. Will fail if a collection with that name already exists.
	 * the created Collection needs to be released with ReleaseVariableCollection when not needed anymore.
	 *
	 * Note: pDRS->CreateVariableCollection("myCollection")
	 * @param name - the name of the new collection. needs to be unique
	 * @return return the newly created Collection when successful.
	 * @see ReleaseVariableCollection
	 */
	virtual IVariableCollection* CreateVariableCollection(const CHashedString& name) = 0;

	/**
	 * Will release the given VariableCollection. The collection must have be generated with CreateVariableCollection.
	 *
	 * Note: pDRS->ReleaseVariableCollection(pMyCollection)
	 * @param pToBeReleased - Collection to be released
	 * @see CreateVariableCollection
	 */
	virtual void ReleaseVariableCollection(IVariableCollection* pToBeReleased) = 0;

	/**
	 * Will return the Variable collection with the specified name, if existing.
	 *
	 * Note: pDRS->GetCollection("myCollection")
	 * @param name - the name of the collection to fetch.
	 * @return returns the Variable collection with the specified name, if existing. Otherwise 0
	 * @see CreateVariableCollection
	 */
	virtual IVariableCollection* GetCollection(const CHashedString& name) = 0;

	/**
	 * Will return the Variable collection with the specified name, if existing. This one also supports the two 'special' collection 'local' and 'context'
	 * This function is mainly a helper to be used in CustomActions/CustomConditions
	 *
	 * Note: pDRS->GetCollection("myCollection", pResponseInstance)
	 * @param name - the name of the collection to fetch.
	 * @return returns the Variable collection with the specified name, if existing. Otherwise 0
	 * @see CreateVariableCollection
	 */
	virtual IVariableCollection* GetCollection(const CHashedString& name, IResponseInstance* pResponseInstance) = 0;

	/**
	 * Will return a new Variable collection that can be used as a context variable collection for sent signals.
	 * The DRS will not hold a reference to this created collection. So it will be released, when no one is referencing it anymore on the outside.
	 *
	 * Note: pDRS->CreateContextCollection()
	 * @return returns the empty variable collection.
	 * @see QueueSignal, CreateVariableCollection
	 */
	virtual IVariableCollectionSharedPtr CreateContextCollection() = 0;

	/**
	 * Will stop the execution of any response that was started by exactly this signal on the given object.
	 * REMARK: Its up to the actions, how to handle cancel requests, they might not stop immediately.
	 * REMARK: You will have to specify the object that has send the signal originally, remember that the active actor can change during the execution of the response
	 * REMARK: if you specify NULL for the pSender, the signal will be stopped on any actor currently executing it
	 *
	 * Note: pDRS->CancelSignalProcessing("si_EnemySpotted", pMyActor)
	 * @param signalName - Name of the signal for which we want to interrupt the execution of responses
	 * @param pSender - the Actor for which we want to stop the execution
	 */
	virtual void CancelSignalProcessing(const CHashedString& signalName, IResponseActor* pSender = nullptr) = 0;

	/**
	 * Will create a new Response Actor. This actor is registered in the DRS.
	 * All actors that will be used in the DRS needs to be created first, otherwise you wont be able to queue Signals for them or set them as a actor by name
	 *
	 * Note: pDRS->CreateResponseActor("Bob", myGameSpecificEntityGUID)
	 * @param pActorName - the unique name of the new actor. if a actor with that name already exist the behavior is undefined
	 * @param userData - this data will be attached to the actor. it can be obtained from the actor with the getUserData method. This is the link between your game-entity and the DRS-actor.
	 * @return return the newly created Actor when successful.
	 * @see ReleaseResponseActor, IResponseActor::GetUserData
	 */
	virtual IResponseActor* CreateResponseActor(const CHashedString& actorName, EntityId entityID = INVALID_ENTITYID) = 0;

	/**
	 * Will release the specified Actor. The actor cannot be used/found by the DRS afterwards.
	 *
	 * Note: pDRS->ReleaseResponseActor(pMyActor)
	 * @param pActorToFree - the actor that should be released
	 * @see CreateResponseActor
	 */
	virtual bool ReleaseResponseActor(IResponseActor* pActorToFree) = 0;

	/**
	 * Will return the actor with the specified name if existing. 0 otherwise
	 *
	 * Note: pDRS->GetResponseActor("human_01")
	 * @param pActorName - the name of the actor to look for
	 * @return return the found actor, or 0 if not found
	 * @see CreateResponseActor
	 */
	virtual IResponseActor* GetResponseActor(const CHashedString& actorName) = 0;

	/**
	 * Will return a helper class, which allows you to create DRS structures from your own data format. (e.g. used by the excel table importer)
	 *
	 * Note: pDRS->GetCustomDataformatHelper()
	 * @param resetHint - a hint what to reset. For example you could decide to only reset variables instead of destroy them.
	 */
	virtual IDataImportHelper* GetCustomDataformatHelper() = 0;

	enum eResetHints
	{
		eResetHint_Nothing              = 0,
		eResetHint_StopRunningResponses = BIT(0),
		eResetHint_ResetAllResponses    = BIT(1),
		eResetHint_Variables            = BIT(2),
		eResetHint_Speaker              = BIT(3),
		eResetHint_DebugInfos           = BIT(4),
		//... implementation specifics
		eResetHint_All                  = 0xFFFFFFFF
	};

	/**
	 * Will reset everything in the dynamic response system. So it will reset all variables and stop all signal responses. it wont remove registered actions or conditions or created actors.
	 *
	 * Note: pDRS->Reset()
	 * @param resetHint - a hint what to reset. For example you could decide to only reset variables instead of destroy them.
	 */
	virtual void Reset(uint32 resetHint = (uint32)eResetHint_All) = 0;

	/**
	 * Serializes all the member variables of this class
	 *
	 * Note: Called by the serialization library to serialize to disk or to the UI
	 */
	virtual void Serialize(Serialization::IArchive& ar) = 0;

	/**
	 * Gives access to a factory to construct actions
	 *
	 * Note: Used by the serialization library to get a factory class that constructs actions
	 * @return A factory to construct actions
	 */
	virtual ActionSerializationClassFactory& GetActionSerializationFactory() = 0;

	/**
	 * Gives access to a factory to construct conditions
	 *
	 * Note: Used by the serialization library to get a factory class that constructs conditions
	 * @return A factory to construct conditions
	 */
	virtual ConditionSerializationClassFactory& GetConditionSerializationFactory() = 0;

	//! Gets a pointer to the dialog line database
	//! \return A pointer to the dialog line database
	virtual IDialogLineDatabase* GetDialogLineDatabase() const = 0;

	//! Gets a pointer to the IResponseManager
	//! \return A pointer to the IResponseManager
	virtual IResponseManager* GetResponseManager() const = 0;

	//! Gets a pointer to the ISpeakerManager
	//! \return A pointer to the ISpeakerManager
	virtual ISpeakerManager* GetSpeakerManager() const = 0;

	//! Only needed for debug purposes, informs the system who is currently using it (so that debug outputs than create logs like "flowgraph 'human2' has changed variable X to value 0.3").
	//! \return the currently source of DRS actions
	virtual void        SetCurrentDrsUserName(const char* szNewDrsUserName) {}
	virtual const char* GetCurrentDrsUserName() const                       { return "unknown"; }
};

//////////////////////////////////////////////////////////////////////////
struct IResponseManager
{
	struct IListener
	{
		enum eProcessingResult
		{
			ProcessingResult_NoResponseDefined,
			ProcessingResult_ConditionsNotMet,
			ProcessingResult_Done,
			ProcessingResult_Canceled,
		};

		struct SSignalInfos
		{
			SSignalInfos(
			  const CHashedString& _name,
			  IResponseActor* _pSender,
			  const IVariableCollectionSharedPtr& _pContext,
			  SignalId _id) :
				name(_name),
				pSender(_pSender),
				pContext(_pContext),
				id(_id) {}

			const CHashedString&                name;
			IResponseActor*                     pSender;
			const IVariableCollectionSharedPtr& pContext;
			SignalId                            id;
		};

		virtual void OnSignalProcessingStarted(SSignalInfos& signal, IResponseInstance* pStartedResponse) {}
		virtual void OnSignalProcessingFinished(SSignalInfos& signal, IResponseInstance* pFinishedResponse, eProcessingResult outcome) = 0;
	};

	enum eSignalFilter
	{
		eSF_All,
		eSF_OnlyWithResponses,
		eSF_OnlyWithoutResponses
	};

	virtual ~IResponseManager() {}

	virtual bool AddListener(IListener* pNewListener, SignalId onlySignalWithID = s_InvalidSignalId) = 0;
	virtual bool RemoveListener(IListener* pListenerToRemove) = 0;

	//////////////////////////////////////////////////////////////////////////
	//mainly for the UI
	virtual DynArray<const char*> GetRecentSignals(eSignalFilter filter = eSF_All) = 0;
	virtual void                  SerializeResponse(Serialization::IArchive& ar, const DynArray<stack_string>& signalNames, IResponseActor* pActorForEvaluation = nullptr) = 0;
	virtual void                  SerializeRecentResponse(Serialization::IArchive& ar, const DynArray<stack_string>& signalNames, int maxElemets = -1) = 0;
	virtual void                  SerializeVariableChanges(Serialization::IArchive& ar, const stack_string& variableName = "ALL", int maxElemets = -1) = 0;
	virtual bool                  AddResponse(const stack_string& signalName) = 0;
	virtual bool                  RemoveResponse(const stack_string& signalName) = 0;
	//////////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////

struct IResponseActor
{
	virtual ~IResponseActor() {}

	/**
	 * Will return the name of the Actor in the DRS. This is the name, with which the actor can be found.
	 *
	 * Note: string actorNameAsString = pActor->GetName().GetText();
	 * @return Returns the identifier that the DRS gave to the Actor.
	 * @see IDynamicResponseSystem::CreateResponseActor
	 */
	virtual const CHashedString& GetName() const = 0;

	/**
	 * Will return the (local) variable collection for this actor.
	 * The variables in there can be modified.
	 *
	 * Note: pActor->GetLocalVariables()
	 * @return Returns the local variable collection for the actor
	 * @see IDynamicResponseSystem::CreateResponseActor
	 */
	virtual IVariableCollection* GetLocalVariables() = 0;

	/**
	 * Will return the (local) variable collection for this actor.
	 * The variables in there can NOT be modified in this const version.
	 *
	 * Note: pActor->GetLocalVariables()
	 * @return Returns the local variable collection for the actor
	 * @see IDynamicResponseSystem::CreateResponseActor
	 */
	virtual const IVariableCollection* GetLocalVariables() const = 0;

	/**
	 * Will return the entityID, that was attached to the actor on creation time
	 * Note: pActor->GetLinkedEnityID()
	 * @return returns the entityID, that was attached to the actor on creation time
	 * @see IDynamicResponseSystem::CreateResponseActor
	 */
	virtual EntityId GetLinkedEntityID() const = 0;

	/**
	 * Will return the Entity, that was attached to the actor on creation time
	 * Note: If no entityID was assigned the local client entity will be returned.
	 * @return returns the Entity, that was attached to the actor on creation time
	 * @see IDynamicResponseSystem::CreateResponseActor
	 */
	virtual IEntity* GetLinkedEntity() const = 0;

	/**
	 * Will queue a new signal. it will be handled in the next update of the DRS. The DRS will determine if there is a response for the signal.
	 *
	 * Note: pActor->QueueSignal("si_EnemySpotted", pContextVariableCollection);
	 * @param signalName - the name of the signal
	 * @param pSignalContext - if for the processing of the signal some additional input is required, these is the place to store it. The Signal will hold a reference as long as needed.
	 * @param pSignalListener - registers the given listener to track the processing of this signal, REMARK: you will be automatically removed as a listener, once you received the "OnSignalProcessingFinished" Callback
	 * @return returns an unique ID for this signal processing
	 * @see IDynamicResponseSystem::CreateContextCollection, IDynamicResponseSystem::CancelSignalProcessing
	 */
	virtual SignalId QueueSignal(const CHashedString& signalName, IVariableCollectionSharedPtr pSignalContext = nullptr, IResponseManager::IListener* pSignalListener = nullptr) = 0;
};

//////////////////////////////////////////////////////////////////////////
//! A base class for all objects (conditions, actions for now) that are displayed in Sandbox.
struct IEditorObject
{
	/**
	 * Just some debug information, that describe the action and its parameter. So that a history-log with executed actions can be generated.
	 *
	 * Note: Will be called from the DRS for debug output. No need to repeat the type in here again, just instance specific informations (e.g. the parameter values).
	 * @return a descriptive string containing the current parameter
	 */
	virtual string GetVerboseInfo() const { return ""; }

	/**
	 * Just some debug information, that describe the action and its parameter. So that a history-log with executed actions can be generated.
	 *
	 * Note: Will be called from the DRS for debug output
	 * @return the type if the object
	 */
	virtual const char* GetType() const = 0;

	string              GetVerboseInfoWithType() const { return string(GetType()) + ": " + GetVerboseInfo(); }
};

#if !defined(_RELEASE)
	#define SET_DRS_USER(x) gEnv->pDynamicResponseSystem->SetCurrentDrsUserName(x)
struct SCurrentDrsUserScopeHelper
{
	SCurrentDrsUserScopeHelper(const char* szCurrentSource) { SET_DRS_USER(szCurrentSource); }
	SCurrentDrsUserScopeHelper(const string& currentSource) { SET_DRS_USER(currentSource.c_str()); }
	~SCurrentDrsUserScopeHelper() { SET_DRS_USER(nullptr); }
};
	#define SET_DRS_USER_SCOPED(x) DRS::SCurrentDrsUserScopeHelper drsScopeHelper(x)
#else
	#define SET_DRS_USER_SCOPED(x)
	#define SET_DRS_USER(x)
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Interfaces needed only for the editor plugin.

struct IDialogLine
{
	virtual ~IDialogLine() {}
	virtual const string& GetText() const = 0;
	virtual const string& GetStartAudioTrigger() const = 0;
	virtual const string& GetEndAudioTrigger() const = 0;
	virtual const string& GetLipsyncAnimation() const = 0;
	virtual const string& GetStandaloneFile() const = 0;
	virtual void          SetText(const string& text) = 0;
	virtual void          SetStartAudioTrigger(const string& trigger) = 0;
	virtual void          SetEndAudioTrigger(const string& trigger) = 0;
	virtual void          Serialize(Serialization::IArchive& ar) = 0;
	virtual void          SetLipsyncAnimation(const string& lipsyncAnimation) = 0;
	virtual void          SetStandaloneFile(const string& standAlonefile) = 0;
};

//////////////////////////////////////////////////////////////////////////

struct IDialogLineSet
{
	enum class EPickModeFlags
	{
		None       = 0,
		Random     = 1 << 0,     //!< Pick one at random (NOT taking user defined PickFactors into consideration).
		Sequential = 1 << 1,     //!< Pick one after another.
		Any        = Random | Sequential,
	};

	virtual ~IDialogLineSet() {}
	virtual void          SetLineId(const CHashedString& lineId) = 0;
	virtual void          SetPriority(int priority) = 0;
	virtual void          SetFlags(uint32 flags) = 0;
	virtual CHashedString GetLineId() const = 0;
	virtual int           GetPriority() const = 0;
	virtual uint32        GetFlags() const = 0;
	virtual uint32        GetLineCount() const = 0;
	virtual IDialogLine*  GetLineByIndex(uint32 index) = 0;
	virtual IDialogLine*  InsertLine(uint32 index) = 0;
	virtual void          RemoveLine(uint32 index) = 0;
	virtual void          Serialize(Serialization::IArchive& ar) = 0;
};

//////////////////////////////////////////////////////////////////////////

struct IDialogLineDatabase
{
	virtual ~IDialogLineDatabase() {}
	virtual bool                        Save(const char* szFilePath) = 0;
	virtual uint32                      GetLineSetCount() = 0;
	virtual IDialogLineSet*             GetLineSetByIndex(uint32 index) = 0;
	virtual const IDialogLineSet* const GetLineSetById(const CHashedString& lineID) const = 0;
	virtual IDialogLineSet*             InsertLineSet(uint32 index) = 0;
	virtual void                        RemoveLineSet(uint32 index) = 0;
	virtual void                        Serialize(Serialization::IArchive& ar) = 0;
	virtual void                        SerializeLinesHistory(Serialization::IArchive& ar) = 0;
};

//! WIP
struct IDataImportHelper
{
	typedef IConditionSharedPtr (*      CondtionCreatorFct)(const string&, const char* szFormatName);
	typedef IResponseActionSharedPtr (* ActionCreatorFct)(const string&, const char* szFormatName);

	typedef uint32                      ResponseID;
	typedef uint32                      ResponseSegmentID;
	static const uint32              INVALID_ID = -1;

	virtual ResponseID               AddSignalResponse(const string& szName) = 0;
	virtual bool                     AddResponseCondition(ResponseID responseID, IConditionSharedPtr pCondition, bool bNegated) = 0;
	virtual bool                     AddResponseAction(ResponseID segmentID, IResponseActionSharedPtr pAction) = 0;
	virtual ResponseSegmentID        AddResponseSegment(ResponseID parentResponse, const string& szName) = 0;
	virtual bool                     AddResponseSegmentAction(ResponseID parentResponse, ResponseSegmentID segmentID, IResponseActionSharedPtr pAction) = 0;
	virtual bool                     AddResponseSegmentCondition(ResponseID parentResponse, ResponseSegmentID segmentID, IConditionSharedPtr pConditions, bool bNegated = false) = 0;

	virtual bool                     HasActionCreatorForType(const CHashedString& type) = 0;
	virtual bool                     HasConditionCreatorForType(const CHashedString& type) = 0;
	virtual IResponseActionSharedPtr CreateActionFromString(const CHashedString& type, const string& data, const char* szFormatName) = 0;
	virtual IConditionSharedPtr      CreateConditionFromString(const CHashedString& type, const string& data, const char* szFormatName) = 0;
	virtual void                     RegisterConditionCreator(const CHashedString& conditionType, CondtionCreatorFct pFunc) = 0;
	virtual void                     RegisterActionCreator(const CHashedString& actionTyp, ActionCreatorFct pFunc) = 0;

	virtual void                     Reset() = 0;
	virtual void                     Serialize(Serialization::IArchive& ar) = 0;
};
}  //namespace DRS

static const char* GetTypeHelperFct(const DRS::IEditorObject& pObject)
{
	return pObject.GetType();
}

#define DEFAULT_DRS_ACTION_COLOR "55BB55"
#define REGISTER_DRS_CUSTOM_ACTION_WITH_COLOR(_customAction, _color)                                                                                                                         \
  { using DRS::IResponseAction;                                                                                                                                                              \
    SERIALIZATION_CLASS_NAME_FOR_FACTORY(gEnv->pDynamicResponseSystem->GetActionSerializationFactory(), IResponseAction, _customAction, # _customAction, GetTypeHelperFct(_customAction())); \
    SERIALIZATION_CLASS_ANNOTATION_FOR_FACTORY(gEnv->pDynamicResponseSystem->GetActionSerializationFactory(), IResponseAction, _customAction, "color", _color); }
#define REGISTER_DRS_CUSTOM_ACTION(_customAction) REGISTER_DRS_CUSTOM_ACTION_WITH_COLOR(_customAction, DEFAULT_DRS_ACTION_COLOR)

#define DEFAULT_DRS_CONDITION_COLOR "4444EE"
#define REGISTER_DRS_CUSTOM_CONDITION_WITH_COLOR(_customCondition, _color)                                                                                                                                  \
  { using DRS::IResponseCondition;                                                                                                                                                                          \
    SERIALIZATION_CLASS_NAME_FOR_FACTORY(gEnv->pDynamicResponseSystem->GetConditionSerializationFactory(), IResponseCondition, _customCondition, # _customCondition, GetTypeHelperFct(_customCondition())); \
    SERIALIZATION_CLASS_ANNOTATION_FOR_FACTORY(gEnv->pDynamicResponseSystem->GetConditionSerializationFactory(), IResponseCondition, _customCondition, "color", _color); }
#define REGISTER_DRS_CUSTOM_CONDITION(_customCondition) REGISTER_DRS_CUSTOM_CONDITION_WITH_COLOR(_customCondition, DEFAULT_DRS_CONDITION_COLOR)

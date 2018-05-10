// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryInput/IInput.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryString/CryName.h>
#include <CryCore/smartptr.h>

typedef CCryName ActionId;
struct SInputEvent;

// Summary
//   Enumeration all all the possible activation mode used for the actions
enum EActionActivationMode // Value for onpress, onrelease, onhold must match EInputState
{
	eAAM_Invalid   = 0,
	eAAM_OnPress   = BIT(0),        // Used when the action key is pressed
	eAAM_OnRelease = BIT(1),        // Used when the action key is released
	eAAM_OnHold    = BIT(2),        // Used when the action key is held
	eAAM_Always    = BIT(3),

	// Special modifiers.
	eAAM_Retriggerable = BIT(4),
	eAAM_NoModifiers   = BIT(5),
	eAAM_ConsoleCmd    = BIT(6),
	eAAM_AnalogCompare = BIT(7),    // Used when analog compare op succeeds

};

// Summary
//  Input blocking action to perform when input is triggered
enum EActionInputBlockType
{
	eAIBT_None = 0,
	eAIBT_BlockInputs,
	eAIBT_Clear,
};

struct SActionInputBlocker
{
	// Summary:
	//	 Data used to block input symbol from firing event if matched

	SActionInputBlocker()
		: keyId(eKI_Unknown)
	{
	}

	SActionInputBlocker(const EKeyId keyId_)
		: keyId(keyId_)
	{
	}

	SActionInputBlocker& operator=(const SActionInputBlocker& rhs)
	{
		keyId = rhs.keyId;

		return *this;
	}

	EKeyId keyId;                       // External id for fast comparison.
};

typedef DynArray<SActionInputBlocker> TActionInputBlockers;

struct SActionInputBlockData
{
	SActionInputBlockData()
		: blockType(eAIBT_None)
		, fBlockDuration(0.0f)
		, activationMode(eAAM_Invalid)
		, deviceIndex(0)
		, bAllDeviceIndices(true)
	{
	}

	EActionInputBlockType blockType;
	TActionInputBlockers  inputs;
	float                 fBlockDuration;
	int                   activationMode;
	uint8                 deviceIndex;        // Device index - controller 1/2 etc
	bool                  bAllDeviceIndices;  // True to block all device indices of deviceID type, otherwise uses deviceIndex
};

enum EActionFilterType
{
	eAFT_ActionPass,
	eAFT_ActionFail,
};

enum EActionInputDevice
{
	eAID_Unknown       = 0,
	eAID_KeyboardMouse = BIT(0),
	eAID_XboxPad       = BIT(1),
	eAID_PS4Pad        = BIT(2),
	eAID_OculusTouch   = BIT(3),
};

struct SActionInputDeviceData
{
	SActionInputDeviceData(const EActionInputDevice type, const char* szTypeStr)
		: m_type(type)
		, m_typeStr(szTypeStr)
	{
	}
	EActionInputDevice  m_type;
	CryFixedStringT<16> m_typeStr;
};

//! Interface used to receive events when an action is triggered by the user
//! \par Example
//! \include CryAction/Examples/ActionListener.cpp
struct IActionListener
{
	virtual ~IActionListener(){}
	//! Fired when an action is triggered after a raw input was entered
	//! \param action identifier and name of the action that was triggered
	//! \param activationMode Activation flags for the input that triggered this event, see EInputState
	//! \param value Value of the input, in case of analog data
	virtual void OnAction(const ActionId& action, int activationMode, float value) = 0;
	virtual void AfterAction() {};
};
typedef std::vector<IActionListener*> TActionListeners;

// Blocking action listener (Improved, left old one to avoid huge refactor)
//------------------------------------------------------------------------
struct IBlockingActionListener
{
	virtual ~IBlockingActionListener(){}
	virtual bool OnAction(const ActionId& action, int activationMode, float value, const SInputEvent& inputEvent) = 0;
	virtual void AfterAction() {};
};

typedef std::shared_ptr<IBlockingActionListener> TBlockingActionListener;

enum EActionAnalogCompareOperation
{
	eAACO_None = 0,
	eAACO_Equals,
	eAACO_NotEquals,
	eAACO_GreaterThan,
	eAACO_LessThan,
};

typedef CryFixedStringT<32> TActionInputString;

//! Represents one input method that can be used to trigger an action
struct SActionInput
{
	SActionInput()
		: inputDevice(eAID_Unknown)
		, input("")
		, defaultInput("")
		, inputCRC(0)
		, fPressedTime(0.0f)
		, fPressTriggerDelay(0.0f)
		, fPressTriggerDelayRepeatOverride(-1.0f)
		, fLastRepeatTime(0.0f)
		, fAnalogCompareVal(0.0f)
		, fHoldTriggerDelay(0.0f)
		, fCurrentHoldValue(0.0f)
		, fReleaseTriggerThreshold(-1.0f)
		, fHoldRepeatDelay(0.0f)
		, fHoldTriggerDelayRepeatOverride(-1.0f)
		, activationMode(eAAM_Invalid)
		, iPressDelayPriority(0)
		, currentState(eIS_Unknown)
		, analogCompareOp(eAACO_None)
		, bHoldTriggerFired(false)
		, bAnalogConditionFulfilled(false)
	{
	}

	EActionInputDevice            inputDevice;
	TActionInputString            input;
	TActionInputString            defaultInput;
	SActionInputBlockData         inputBlockData;
	uint32                        inputCRC;
	float                         fPressedTime;
	float                         fPressTriggerDelay;
	float                         fPressTriggerDelayRepeatOverride;
	float                         fLastRepeatTime;
	float                         fAnalogCompareVal;
	float                         fHoldTriggerDelay;
	float                         fCurrentHoldValue;   // A normalized amount for the current hold before triggering at the hold delay. Is 1 when hold is hit, & it does not reset when repeating
	float                         fReleaseTriggerThreshold;
	float                         fHoldRepeatDelay;
	float                         fHoldTriggerDelayRepeatOverride;
	int                           activationMode;
	int                           iPressDelayPriority;   // If priority is higher than the current
	EInputState                   currentState;
	EActionAnalogCompareOperation analogCompareOp;
	bool                          bHoldTriggerFired;
	bool                          bAnalogConditionFulfilled;

	void                          GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(input);
	}
};

//------------------------------------------------------------------------

struct IActionMapPopulateCallBack
{
	virtual ~IActionMapPopulateCallBack(){}
	// Description:
	//			Callback function to retrieve all action names
	//			Use it in conjunction with IActionMapManager::EnumerateActions / IActionMapManager::GetActionsCount
	// Arguments:
	//     pName - Name of one of the effects retrieved
	virtual void AddActionName(const char* const pName) = 0;
};

//! Represents a singular action that remaps a raw input into an abstracted gameplay action
struct IActionMapAction
{
	virtual ~IActionMapAction(){}
	virtual void                GetMemoryUsage(ICrySizer*) const = 0;
	virtual void                Release() = 0;
	virtual int                 GetNumActionInputs() const = 0;
	virtual const SActionInput* FindActionInput(const char* szInput) const = 0;
	virtual const SActionInput* GetActionInput(const int iIndex) const = 0;
	virtual const SActionInput* GetActionInput(const EActionInputDevice device, const int iIndex) const = 0;
	virtual const ActionId&     GetActionId() const = 0;
	virtual const char*         GetTriggeredActionInput() const = 0;
};

//------------------------------------------------------------------------
struct IActionMapActionIterator
{
	virtual ~IActionMapActionIterator(){}
	virtual const IActionMapAction* Next() = 0; // Returns the same pointer every call, 0 if no more info available.
	virtual void                    AddRef() = 0;
	virtual void                    Release() = 0;
};
typedef _smart_ptr<IActionMapActionIterator> IActionMapActionIteratorPtr;

//! Represents an action map group that can be enabled / disabled independently
//! Can contain a larger number of actions that remap raw input either statically (defined by developer) or remapped at runtime by the player
struct IActionMap
{
	virtual ~IActionMap(){}
	virtual void                    GetMemoryUsage(ICrySizer*) const = 0;
	virtual void                    Release() = 0;
	virtual void                    Clear() = 0;
	virtual const IActionMapAction* GetAction(const ActionId& actionId) const = 0;
	virtual IActionMapAction*       GetAction(const ActionId& actionId) = 0;
	//! Creates an abstracted action that can be triggered by one or more input state changes
	//! \param actionId Identifier / name of the action we want to create
	//! \par Example
	//! \include CryAction/Examples/ActionListener.cpp
	virtual bool                    CreateAction(const ActionId& actionId) = 0;
	virtual bool                    RemoveAction(const ActionId& actionId) = 0;
	virtual int                     GetActionsCount() const = 0;
	virtual bool                    AddActionInput(const ActionId& actionId, const SActionInput& actionInput, const int iByDeviceIndex = -1) = 0;
	//! Binds a raw input to the action, ensuring that the action will be triggered when the raw input is detected
	//! \param actionId Identifier / name of the action we want to add the input for
	//! \param actionInput Definition of the raw input that we want to listen for
	//! \par Example
	//! \include CryAction/Examples/ActionListener.cpp
	virtual bool                    AddAndBindActionInput(const ActionId& actionId, const SActionInput& actionInput) = 0;
	virtual bool                    RemoveActionInput(const ActionId& actionId, const char* szInput) = 0;
	//! Rebinds the specified action from the current input to a new input of the same device type
	//! \param actionId Identifier / name of the action we want to rebind
	//! \param szCurrentInput The input that the action is currently bound to, note that this will traverse all inputs regardless of input device
	//! \param szNewInput The input that we want to bind to, has to remain the same device as szCurrentInput!
	//! \par Example
	//! \include CryAction/Examples/RebindActionInput.cpp
	virtual bool                    ReBindActionInput(const ActionId& actionId, const char* szCurrentInput, const char* szNewInput) = 0;
	//! Rebinds the specified action from the current input to a new input of a specific device type
	//! \param actionId Identifier / name of the action we want to rebind
	//! \param szNewInput New input we want to bind to
	//! \param device Device type that we want to remap
	//! \param iByDeviceIndex Index of the device that we want to remap, typically 0 but can be used in case of multiple devices of the same type
	//! \par Example
	//! \include CryAction/Examples/RebindActionInput.cpp
	virtual bool                    ReBindActionInput(const ActionId& actionId,
	                                                  const char* szNewInput,
	                                                  const EActionInputDevice device,
	                                                  const int iByDeviceIndex) = 0;
	virtual int                         GetNumRebindedInputs() = 0;
	virtual bool                        Reset() = 0;
	virtual bool                        LoadFromXML(const XmlNodeRef& actionMapNode) = 0;
	virtual bool                        LoadRebindingDataFromXML(const XmlNodeRef& actionMapNode) = 0;
	virtual bool                        SaveRebindingDataToXML(XmlNodeRef& actionMapNode) const = 0;
	virtual IActionMapActionIteratorPtr CreateActionIterator() = 0;
	virtual void                        SetActionListener(EntityId id) = 0;
	virtual EntityId                    GetActionListener() const = 0;
	virtual const char*                 GetName() = 0;
	virtual void                        Enable(bool enable) = 0;
	virtual bool                        Enabled() const = 0;
};

//------------------------------------------------------------------------
struct IActionMapIterator
{
	virtual ~IActionMapIterator(){}
	virtual IActionMap* Next() = 0;
	virtual void        AddRef() = 0;
	virtual void        Release() = 0;
};
typedef _smart_ptr<IActionMapIterator> IActionMapIteratorPtr;

//------------------------------------------------------------------------
struct IActionFilter
{
	virtual ~IActionFilter(){}
	virtual void        GetMemoryUsage(ICrySizer*) const = 0;
	virtual void        Release() = 0;
	virtual void        Filter(const ActionId& action) = 0;
	virtual bool        SerializeXML(const XmlNodeRef& root, bool bLoading) = 0;
	virtual const char* GetName() = 0;
	virtual void        Enable(bool enable) = 0;
	virtual bool        Enabled() = 0;
};

//------------------------------------------------------------------------
struct IActionFilterIterator
{
	virtual ~IActionFilterIterator(){}
	virtual IActionFilter* Next() = 0;
	virtual void           AddRef() = 0;
	virtual void           Release() = 0;
};
typedef _smart_ptr<IActionFilterIterator> IActionFilterIteratorPtr;

struct SActionMapEvent
{
	enum EActionMapManagerEvent
	{
		eActionMapManagerEvent_ActionMapsInitialized = 0,
		eActionMapManagerEvent_DefaultActionEntityChanged,
		eActionMapManagerEvent_FilterStatusChanged,
		eActionMapManagerEvent_ActionMapStatusChanged
	};

	SActionMapEvent(EActionMapManagerEvent event, UINT_PTR wparam, UINT_PTR lparam = 0) :
		event(event),
		wparam(wparam),
		lparam(lparam)
	{}

	EActionMapManagerEvent event;
	UINT_PTR               wparam;
	UINT_PTR               lparam;
};

struct IActionMapEventListener
{
	virtual ~IActionMapEventListener() {};
	virtual void OnActionMapEvent(const SActionMapEvent& event) = 0;
};

//------------------------------------------------------------------------

//! Main interface to the action map manager, responsible for catching raw input from IInput and mapping it to actions registered by the user.
struct IActionMapManager
{
	virtual ~IActionMapManager(){}
	virtual void                          Update() = 0;
	virtual void                          Reset() = 0;
	virtual void                          Clear() = 0;

	virtual bool                          InitActionMaps(const char* filename) = 0;
	virtual void                          SetLoadFromXMLPath(const char* szPath) = 0;
	virtual const char*                   GetLoadFromXMLPath() const = 0;
	virtual bool                          LoadFromXML(const XmlNodeRef& node) = 0;
	virtual bool                          LoadRebindDataFromXML(const XmlNodeRef& node) = 0;
	virtual bool                          SaveRebindDataToXML(XmlNodeRef& node) = 0;

	//! Adds a listener to receive events when actions are triggered
	//! \param pExtraActionListener The listener in which we want to receive events
	//! \param actionMap The action map we want to receive events for, or if unspecified sends events for all action maps
	//! \par Example
	//! \include CryAction/Examples/ActionListener.cpp
	virtual bool                          AddExtraActionListener(IActionListener* pExtraActionListener, const char* szActionMap = nullptr) = 0;
	//! Removes action map listener
	//! \param pExtraActionListener The listener that is currently registered to receive events
	//! \param actionMap The action map that we are receiving events for, or if unspecified removes from all action maps
	//! \par Example
	//! \include CryAction/Examples/ActionListener.cpp
	virtual bool                          RemoveExtraActionListener(IActionListener* pExtraActionListener, const char* szActionMap = nullptr) = 0;
	virtual const TActionListeners&       GetExtraActionListeners() const = 0;

	virtual bool                          AddFlowgraphNodeActionListener(IActionListener* pFlowgraphActionListener, const char* szActionMap = nullptr) = 0;
	virtual bool                          RemoveFlowgraphNodeActionListener(IActionListener* pFlowgraphActionListener, const char* szActionMap = nullptr) = 0;

	virtual void                          AddAlwaysActionListener(TBlockingActionListener pActionListener) = 0;
	virtual void                          RemoveAlwaysActionListener(TBlockingActionListener pActionListener) = 0;
	virtual void                          RemoveAllAlwaysActionListeners() = 0;

	//! Creates an action map that can contain multiple actions inside
	//! \param szName Name of the action map
	//! \par Example
	//! \include CryAction/Examples/ActionListener.cpp
	virtual IActionMap*                   CreateActionMap(const char* szName) = 0;
	virtual bool                          RemoveActionMap(const char* name) = 0;
	virtual void                          RemoveAllActionMaps() = 0;
	virtual IActionMap*                   GetActionMap(const char* name) = 0;
	virtual IActionFilter*                CreateActionFilter(const char* name, EActionFilterType type) = 0;
	virtual IActionFilter*                GetActionFilter(const char* name) = 0;
	virtual IActionMapIteratorPtr         CreateActionMapIterator() = 0;
	virtual IActionFilterIteratorPtr      CreateActionFilterIterator() = 0;

	virtual const SActionInput*           GetActionInput(const char* actionMapName, const ActionId& actionId, const EActionInputDevice device, const int iByDeviceIndex) const = 0;

	virtual void                          Enable(const bool enable, const bool resetStateOnDisable = false) = 0;
	virtual void                          EnableActionMap(const char* name, bool enable) = 0;
	virtual void                          EnableFilter(const char* name, bool enable) = 0;
	virtual bool                          IsFilterEnabled(const char* name) = 0;
	virtual void                          ReleaseFilteredActions() = 0;
	virtual void                          ClearStoredCurrentInputData() = 0;

	virtual bool                          ReBindActionInput(const char* actionMapName, const ActionId& actionId, const char* szCurrentInput, const char* szNewInput) = 0;

	virtual int                           GetVersion() const = 0;
	virtual void                          SetVersion(int version) = 0;

	virtual void                          EnumerateActions(IActionMapPopulateCallBack* pCallBack) const = 0;
	virtual int                           GetActionsCount() const = 0;
	virtual int                           GetActionMapsCount() const = 0;

	virtual bool                          AddInputDeviceMapping(const EActionInputDevice deviceType, const char* szDeviceTypeStr) = 0;
	virtual bool                          RemoveInputDeviceMapping(const EActionInputDevice deviceType) = 0;
	virtual void                          ClearInputDevicesMappings() = 0;
	virtual int                           GetNumInputDeviceData() const = 0;
	virtual const SActionInputDeviceData* GetInputDeviceDataByIndex(const int iIndex) = 0;
	virtual const SActionInputDeviceData* GetInputDeviceDataByType(const EActionInputDevice deviceType) = 0;
	virtual const SActionInputDeviceData* GetInputDeviceDataByType(const char* szDeviceType) = 0;

	virtual void                          RemoveAllRefireData() = 0;
	virtual bool                          LoadControllerLayoutFile(const char* szLayoutKeyName) = 0;

	virtual EntityId                      GetDefaultActionEntity() const = 0;
	virtual void                          SetDefaultActionEntity(EntityId id, bool bUpdateAll = true) = 0;
	virtual void                          RegisterActionMapEventListener(IActionMapEventListener* pActionMapEventListener) = 0;
	virtual void                          UnregisterActionMapEventListener(IActionMapEventListener* pActionMapEventListener) = 0;
};

template<class T>
class TActionHandler
{
public:
	// Returns true if the action should also be forwarded to scripts
	typedef bool (T::* TOnActionHandler)(EntityId entityId, const ActionId& actionId, int activationMode, float value);

	// setup action handlers
	void AddHandler(const ActionId& actionId, TOnActionHandler fnHandler)
	{
		m_actionHandlers.insert(std::make_pair(actionId, fnHandler));
	}

	size_t GetNumHandlers() const
	{
		return m_actionHandlers.size();
	}

	// call action handler
	bool Dispatch(T* pThis, EntityId entityId, const ActionId& actionId, int activationMode, float value)
	{
		bool rVal = false;
		return Dispatch(pThis, entityId, actionId, activationMode, value, rVal);
	}

	// call action handler
	bool Dispatch(T* pThis, EntityId entityId, const ActionId& actionId, int activationMode, float value, bool& rVal)
	{
		rVal = false;

		TOnActionHandler fnHandler = GetHandler(actionId);
		if (fnHandler && pThis)
		{
			rVal = (pThis->*fnHandler)(entityId, actionId, activationMode, value);
			return true;
		}
		else
			return false;
	}

	// get action handler
	TOnActionHandler GetHandler(const ActionId& actionId)
	{
		typename TActionHandlerMap::iterator handler = m_actionHandlers.find(actionId);

		if (handler != m_actionHandlers.end())
		{
			return handler->second;
		}
		return 0;
	}

	void Clear()
	{
		m_actionHandlers.clear();
	}

private:
	typedef std::multimap<ActionId, TOnActionHandler> TActionHandlerMap;

	TActionHandlerMap m_actionHandlers;
};
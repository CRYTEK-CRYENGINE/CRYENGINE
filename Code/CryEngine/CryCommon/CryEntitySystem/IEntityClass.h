// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>
#include <CryEntitySystem/IEntityComponent.h>

struct IEntity;
struct IEntityComponent;
struct SEntitySpawnParams;
struct IEntityScript;
struct IScriptTable;

class ITexture;

struct SEditorClassInfo
{
	SEditorClassInfo() :
		sIcon(""),
		sHelper(""),
		sCategory(""),
		bIconOnTop(false)
	{
	}

	string sIcon;
	string sHelper;
	string sCategory;
	bool   bIconOnTop;
};

enum EEntityClassFlags
{
	ECLF_INVISIBLE                         = BIT(0),  //!< If set this class will not be visible in editor,and entity of this class cannot be placed manually in editor.
	ECLF_BBOX_SELECTION                    = BIT(2),  //!< If set entity of this class can be selected by bounding box in the editor 3D view.
	ECLF_DO_NOT_SPAWN_AS_STATIC            = BIT(3),  //!< If set the entity of this class stored as part of the level won't be assigned a static id on creation.
	ECLF_MODIFY_EXISTING                   = BIT(4),  //!< If set modify an existing class with the same name.
	ECLF_SEND_SCRIPT_EVENTS_FROM_FLOWGRAPH = BIT(5),  //!< If set send script events to entity from Flowgraph.
	ECLF_ENTITY_ARCHETYPE                  = BIT(6),  //!< If set this indicate the entity class is actually entity archetype.
	ECLF_CREATE_PER_CLIENT                 = BIT(7)   //!< If set, an instance of this class will be created for each connecting client
};

struct IEntityClassRegistryListener;

//! Custom interface that can be used to reload an entity's script.
//! Only used by the editor, only.
struct IEntityScriptFileHandler
{
	virtual ~IEntityScriptFileHandler(){}

	//! Reloads the specified entity class' script.
	virtual void ReloadScriptFile() = 0;

	//! \return The class' script file name, if any.
	virtual const char* GetScriptFile() const = 0;

	virtual void        GetMemoryUsage(ICrySizer* pSizer) const { /*REPLACE LATER*/ }
};

struct IEntityEventHandler
{
	virtual ~IEntityEventHandler(){}
	virtual void GetMemoryUsage(ICrySizer* pSizer) const { /*REPLACE LATER*/ }

	enum EventValueType
	{
		Bool,
		Int,
		Float,
		Vector,
		Entity,
		String,
	};

	enum EventType
	{
		Input,
		Output,
	};

	//! Events info for this entity class.
	struct SEventInfo
	{
		const char*    name;        //!< Name of event.
		EventType      type;        //!< Input or Output event.
		EventValueType valueType;   //!< Type of event value.
		const char*    description; //!< Description of the event.
	};

	//! Refresh the class' event info.
	virtual void RefreshEvents() = 0;

	//! Load event info into the entity.
	virtual void LoadEntityXMLEvents(IEntity* entity, const XmlNodeRef& xml) = 0;

	//! \return Return Number of events for this entity.
	virtual int GetEventCount() const = 0;

	//! Retrieve information about events of the entity.
	//! \param nIndex Index of the event to retrieve, must be in 0 to GetEventCount()-1 range.
	//! \return Specified event description in SEventInfo structure.
	virtual bool GetEventInfo(int index, SEventInfo& info) const = 0;

	//! Send the specified event to the entity.
	//! \param entity The entity to send the event to.
	//! \param eventName Name of the event to send.
	virtual void SendEvent(IEntity* entity, const char* eventName) = 0;
};

//! Entity class defines what is this entity, what script it uses, what user proxy will be spawned with the entity, etc.
//! IEntityClass unique identify type of the entity, Multiple entities share the same entity class.
//! Two entities can be compared if they are of the same type by just comparing their IEntityClass pointers.
struct IEntityClass
{
	//! OnSpawnCallback is called when entity of that class is spawned.
	//! When registering new entity class this callback allow user to execute custom code on entity spawn,
	//! Like creation and initialization of some default components
	typedef std::function<bool (IEntity& entity, SEntitySpawnParams& params)> OnSpawnCallback;

	//! UserProxyCreateFunc is a function pointer type,.
	//! By calling this function EntitySystem can create user defined UserProxy class for an entity in SpawnEntity.
	//! For example:
	//! IEntityComponent* CreateUserProxy( IEntity *pEntity, SEntitySpawnParams &params )
	//! {
	//!     return new CUserProxy( pEntity,params );.
	//! }
	typedef IEntityComponent* (* UserProxyCreateFunc)(IEntity* pEntity, SEntitySpawnParams& params, void* pUserData);

	enum EventValueType
	{
		EVT_INT,
		EVT_FLOAT,
		EVT_BOOL,
		EVT_VECTOR,
		EVT_ENTITY,
		EVT_STRING
	};

	//! Events info for this entity class.
	struct SEventInfo
	{
		const char*    name;    //!< Name of event.
		EventValueType type;    //!< Type of event value.
		bool           bOutput; //!< Input or Output event.
	};

	// <interfuscator:shuffle>
	virtual ~IEntityClass(){}

	//! Destroy IEntityClass object, do not call directly, only EntityRegisty can destroy entity classes.
	virtual void Release() = 0;

	//! If this entity also uses a script, this is the name of the Lua table representing the entity behavior.
	//! \return Name of the entity class. Class name must be unique among all the entity classes.
	virtual const char* GetName() const = 0;

	//! Retrieve unique identifier for this entity class
	virtual CryGUID GetGUID() const = 0;

	//! \return Entity class flags.
	virtual uint32 GetFlags() const = 0;

	//! Set entity class flags.
	virtual void SetFlags(uint32 nFlags) = 0;

	//! Returns the Lua script file name.
	//! \return Lua Script filename, return empty string if entity does not use script.
	virtual const char* GetScriptFile() const = 0;

	//! Returns the IEntityScript interface assigned for this entity class.
	//! \return IEntityScript interface if this entity have script, or nullptr if no script defined for this entity class.
	virtual IEntityScript* GetIEntityScript() const = 0;

	//! Returns the IScriptTable interface assigned for this entity class.
	//! \return IScriptTable interface if this entity have script, or nullptr if no script defined for this entity class.
	virtual IScriptTable*             GetScriptTable() const = 0;

	virtual IEntityEventHandler*      GetEventHandler() const = 0;
	virtual IEntityScriptFileHandler* GetScriptFileHandler() const = 0;

	virtual const SEditorClassInfo&   GetEditorClassInfo() const = 0;
	virtual void                      SetEditorClassInfo(const SEditorClassInfo& editorClassInfo) = 0;

	//! Loads the script.
	//! \note It is safe to call LoadScript multiple times, the script will only be loaded on the first call (unless bForceReload is specified).
	virtual bool LoadScript(bool bForceReload) = 0;

	//! Returns pointer to the user defined function to create UserProxy.
	//! \return Return UserProxyCreateFunc function pointer.
	virtual IEntityClass::UserProxyCreateFunc GetUserProxyCreateFunc() const = 0;

	//! Returns pointer to the user defined data to be passed when creating UserProxy.
	//! \return Return pointer to custom user proxy data.
	virtual void* GetUserProxyData() const = 0;

	//! \return Return Number of input and output events defined in the entity script.
	virtual int GetEventCount() = 0;

	//! Retrieve information about input/output event of the entity.
	//! \param nIndex Index of the event to retrieve, must be in 0 to GetEventCount()-1 range.
	//! \return Specified event description in SEventInfo structure.
	virtual IEntityClass::SEventInfo GetEventInfo(int nIndex) = 0;

	//! Find event by name.
	//! \param sEvent Name of the event.
	//! \param event Output parameter for event.
	//! \return True if event found and event parameter is initialized.
	virtual bool FindEventInfo(const char* sEvent, SEventInfo& event) = 0;

	//! Return On spawn callback for the class
	virtual const OnSpawnCallback& GetOnSpawnCallback() const = 0;

	virtual void                   GetMemoryUsage(ICrySizer* pSizer) const = 0;
	// </interfuscator:shuffle>
};

//! This interface is the repository of the the various entity classes, it allows creation and modification of entities types.
//! There`s only one IEntityClassRegistry interface can exist per EntitySystem.
//! Every entity class that can be spawned must be registered in this interface.
//! \see IEntitySystem::GetClassRegistry
struct IEntityClassRegistry
{
	struct SEntityClassDesc
	{
		SEntityClassDesc()
			: flags(0)
			, sName("")
			, sScriptFile("")
			, pScriptTable(nullptr)
			, editorClassInfo()
			, pUserProxyCreateFunc(nullptr)
			, pUserProxyData(nullptr)
			, pEventHandler(nullptr)
			, pScriptFileHandler(nullptr)
			, pIFlowNodeFactory(nullptr)
		{
		};

		CryGUID                           guid;
		int                               flags;
		string                            sName;
		string                            sScriptFile;
		IScriptTable*                     pScriptTable;

		SEditorClassInfo                  editorClassInfo;

		IEntityClass::UserProxyCreateFunc pUserProxyCreateFunc;
		void*                             pUserProxyData;

		IEntityEventHandler*              pEventHandler;
		IEntityScriptFileHandler*         pScriptFileHandler;

		// Callback function when entity of that class spawns
		IEntityClass::OnSpawnCallback onSpawnCallback;

		//! GUID for the Schematyc runtime class
		CryGUID schematycRuntimeClassGuid;

		//! Optional FlowGraph factory.
		//! Entity class will store and reference count flow node factory.
		struct IFlowNodeFactory* pIFlowNodeFactory;
	};

	virtual ~IEntityClassRegistry(){}

	//! Retrieves pointer to the IEntityClass interface by entity class name.
	//! \return Pointer to the IEntityClass interface, or nullptr if class not found.
	virtual IEntityClass* FindClass(const char* sClassName) const = 0;

	//! Retrieves pointer to the IEntityClass interface by GUID.
	//! \return Pointer to the IEntityClass interface, or nullptr if class not found.
	virtual IEntityClass* FindClassByGUID(const CryGUID& guid) const = 0;

	//! Retrieves pointer to the IEntityClass interface for a default entity class.
	//! \return Pointer to the IEntityClass interface, It can never return nullptr.
	virtual IEntityClass* GetDefaultClass() const = 0;

	//! Load all entity class descriptions from the provided xml file.
	//! \param szFilename Path to XML file containing entity class descriptions.
	virtual void LoadClasses(const char* szFilename, bool bOnlyNewClasses = false) = 0;

	//! Register standard entity class, if class id not specified (is zero), generate a new class id.
	//! \return Pointer to the new created and registered IEntityClass interface, or nullptr if failed.
	virtual IEntityClass* RegisterStdClass(const SEntityClassDesc& entityClassDesc) = 0;

	//! Unregister an entity class.
	//! \return true if successfully unregistered.
	virtual bool UnregisterStdClass(const CryGUID& classGUID) = 0;

	//! Register a listener.
	virtual void RegisterListener(IEntityClassRegistryListener* pListener) = 0;

	//! Unregister a listener.
	virtual void UnregisterListener(IEntityClassRegistryListener* pListener) = 0;

	virtual void UnregisterSchematycEntityClass() = 0;

	// Registry iterator.

	//! Move the entity class iterator to the begin of the registry.
	//! To iterate over all entity classes, e.g.:
	//! IEntityClass *pClass = nullptr;
	//! for (pEntityRegistry->IteratorMoveFirst(); pClass = pEntityRegistry->IteratorNext();;)
	//! {
	//!     pClass
	//!     ...
	//! }
	virtual void IteratorMoveFirst() = 0;

	//! Get the next entity class in the registry.
	//! \return Return a pointer to the next IEntityClass interface, or nullptr if is the end
	virtual IEntityClass* IteratorNext() = 0;

	//! Return the number of entity classes in the registry.
	//! \return Return a pointer to the next IEntityClass interface, or nullptr if is the end
	virtual int GetClassCount() const = 0;
	// </interfuscator:shuffle>
};

enum EEntityClassRegistryEvent
{
	ECRE_CLASS_REGISTERED = 0,    //!< Sent when new entity class is registered.
	ECRE_CLASS_MODIFIED,          //!< Sent when new entity class is modified (see ECLF_MODIFY_EXISTING).
	ECRE_CLASS_UNREGISTERED       //!< Sent when new entity class is unregistered.
};

//! Use this interface to monitor changes within the entity class registry.
struct IEntityClassRegistryListener
{
	friend class CEntityClassRegistry;

public:

	inline IEntityClassRegistryListener()
		: m_pRegistry(nullptr)
	{}

	virtual ~IEntityClassRegistryListener()
	{
		if (m_pRegistry)
		{
			m_pRegistry->UnregisterListener(this);
		}
	}

	virtual void OnEntityClassRegistryEvent(EEntityClassRegistryEvent event, const IEntityClass* pEntityClass) = 0;

private:

	IEntityClassRegistry* m_pRegistry;
};

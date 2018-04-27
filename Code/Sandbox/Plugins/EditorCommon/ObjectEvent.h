// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//! Standard objects types.
//TODO : is there a reason for this to be a mask ? are they ever combined ? this is dangerous !!
enum ObjectType
{
	OBJTYPE_GROUP         = 1 << 0,
	OBJTYPE_TAGPOINT      = 1 << 1,
	OBJTYPE_AIPOINT       = 1 << 2,
	OBJTYPE_ENTITY        = 1 << 3,
	OBJTYPE_SHAPE         = 1 << 4,
	OBJTYPE_VOLUME        = 1 << 5,
	OBJTYPE_BRUSH         = 1 << 6,
	OBJTYPE_PREFAB        = 1 << 7,
	OBJTYPE_SOLID         = 1 << 8,
	OBJTYPE_CLOUD         = 1 << 9,
	OBJTYPE_CLOUDGROUP    = 1 << 10,
	OBJTYPE_VOXEL         = 1 << 11,
	OBJTYPE_ROAD          = 1 << 12,
	OBJTYPE_OTHER         = 1 << 13,
	OBJTYPE_DECAL         = 1 << 14,
	OBJTYPE_DISTANCECLOUD = 1 << 15,
	OBJTYPE_TELEMETRY     = 1 << 16,
	OBJTYPE_REFPICTURE    = 1 << 17,
	OBJTYPE_GEOMCACHE     = 1 << 18,
	OBJTYPE_VOLUMESOLID   = 1 << 19,
	OBJTYPE_ANY           = 0xFFFFFFFF,
};

//////////////////////////////////////////////////////////////////////////
//! Events that objects may want to handle.
//! Passed to OnEvent method of CBaseObject.
enum ObjectEvent
{
	EVENT_PREINGAME = 1,//!< Signals that editor is going to switch into the game mode.
	EVENT_INGAME,       //!< Signals that editor is switching into the game mode.
	EVENT_OUTOFGAME,    //!< Signals that editor is switching out of the game mode.
	EVENT_REFRESH,      //!< Signals that editor is refreshing level.
	EVENT_PLACE_STATIC, //!< Signals that editor needs to place all static objects on terrain.
	EVENT_REMOVE_STATIC,//!< Signals that editor needs to remove all static objects from terrain.
	EVENT_DBLCLICK,     //!< Signals that object have been double clicked.
	EVENT_RELOAD_ENTITY,//!< Signals that entities scripts must be reloaded.
	EVENT_RELOAD_TEXTURES,//!< Signals that all possible textures in objects should be reloaded.
	EVENT_RELOAD_GEOM,     //!< Signals that all possible geometries should be reloaded.
	EVENT_MISSION_CHANGE,  //!< Signals that mission have been changed.
	EVENT_PREFAB_REMAKE,//!< Recreate all objects in prefabs.
	EVENT_ALIGN_TOGRID, //!< Object should align itself to the grid.

	EVENT_PHYSICS_GETSTATE,//!< Signals that entities should accept their physical state from game.
	EVENT_PHYSICS_RESETSTATE,//!< Signals that physics state must be reseted on objects.
	EVENT_PHYSICS_APPLYSTATE,//!< Signals that the stored physics state must be applied to objects.

	EVENT_PRE_EXPORT, //!< Signals that the game is about to be exported, prepare any data if the object needs to

	EVENT_FREE_GAME_DATA,//!< Object should free game data that its holding.
	EVENT_CONFIG_SPEC_CHANGE, //!< Called when config spec changed.
	EVENT_HIDE_HELPER,        //!< Signals that happens when Helper mode switches to be hidden.
	EVENT_SHOW_HELPER,        //!< Signals that happens when Helper mode switches to be shown.
	EVENT_TRANSFORM_FINISHED  //!< Signal when the object was transformed
};

//! Events sent by object to listeners in EventCallback.
enum EObjectListenerEvent
{
	OBJECT_ON_DELETE = 0,// Sent after object was deleted from object manager.
	OBJECT_ON_ADD,                  // Sent after object was added to object manager.
	OBJECT_ON_TRANSFORM,            // Sent when object transformed.
	OBJECT_ON_VISIBILITY,           // Sent when object visibility changes.
	OBJECT_ON_RENAME,               // Sent when object changes name.
	OBJECT_ON_PREDELETE,            // Sent before an object is processed to be deleted from the object manager.
	OBJECT_ON_GROUP_OPEN,           // Sent when this group/prefab object is opened
	OBJECT_ON_GROUP_CLOSE,          // Sent when this group/prefab object is closed
	OBJECT_ON_PREFAB_CHANGED,       // Sent when prefab representation has been changed
	OBJECT_ON_UI_PROPERTY_CHANGED,  // Sent when any misc variable/property changes (Meant for UI/inspector).
	OBJECT_ON_LAYERCHANGE,          // Sent after object has changed layer
	OBJECT_ON_PREATTACHED,          // Sent before an object is attached to a group or prefab
	OBJECT_ON_ATTACHED,             // Sent after an object is attached to a group or prefab
	OBJECT_ON_PREDETACHED,          // Sent before object is detached from parent object
	OBJECT_ON_DETACHED,             // Sent after the object is detached from parent
	OBJECT_ON_PRELINKED,            // Sent before the object is linked
	OBJECT_ON_LINKED,               // Sent after the object is linked
	OBJECT_ON_PREUNLINKED,          // Sent before the object is linked
	OBJECT_ON_UNLINKED,             // Sent after the object is linked

	OBJECT_EVENT_LAST
};

struct IObjectLayer;
class CBaseObject;

//! Generic object event struct. To be used with EObjectListenerEvent
struct CObjectEvent
{
	CObjectEvent(EObjectListenerEvent type)
		: m_type(type)
	{
	};

	EObjectListenerEvent m_type;
};

struct CObjectPreDeleteEvent : public CObjectEvent
{
	CObjectPreDeleteEvent(const IObjectLayer* pLayer)
		: CObjectEvent(OBJECT_ON_PREDELETE)
		, m_pLayer(pLayer)
	{
	};

	const IObjectLayer* m_pLayer;
};

struct CObjectDeleteEvent : public CObjectEvent
{
	CObjectDeleteEvent(const IObjectLayer* pLayer)
		: CObjectEvent(OBJECT_ON_DELETE)
		, m_pLayer(pLayer)
	{
	};

	const IObjectLayer* m_pLayer;
};

struct CObjectPreAttachedEvent : public CObjectEvent
{
	CObjectPreAttachedEvent(const CBaseObject* pParent, bool shouldKeepPos)
		: CObjectEvent(OBJECT_ON_PREATTACHED)
		, m_pParent(pParent)
		, m_shouldKeepPos(shouldKeepPos) // TODO: This information should not be necessary, only track view seems to make use of it
	{
	};

	const CBaseObject* m_pParent;
	bool m_shouldKeepPos;
};

struct CObjectAttachedEvent : public CObjectEvent
{
	CObjectAttachedEvent(const CBaseObject* pParent)
		: CObjectEvent(OBJECT_ON_ATTACHED)
		, m_pParent(pParent)
	{
	};

	const CBaseObject* m_pParent;
};

struct CObjectPreDetachedEvent : public CObjectEvent
{
	CObjectPreDetachedEvent(const CBaseObject* pParent, bool shouldKeepPos)
		: CObjectEvent(OBJECT_ON_PREDETACHED)
		, m_pParent(pParent)
		, m_shouldKeepPos(shouldKeepPos) // TODO: This information should not be necessary, only track view seems to make use of it
	{
	};

	const CBaseObject* m_pParent;
	bool m_shouldKeepPos;
};

struct CObjectDetachedEvent : public CObjectEvent
{
	CObjectDetachedEvent(const CBaseObject* pParent)
		: CObjectEvent(OBJECT_ON_DETACHED)
		, m_pParent(pParent)
	{
	};

	const CBaseObject* m_pParent;
};

struct CObjectLayerChangeEvent : public CObjectEvent
{
	CObjectLayerChangeEvent(IObjectLayer* oldLayer)
		: CObjectEvent(OBJECT_ON_LAYERCHANGE)
		, m_poldLayer(oldLayer)
	{
	}

	IObjectLayer* m_poldLayer;
};

struct CObjectPreLinkEvent : public CObjectEvent
{
	CObjectPreLinkEvent(CBaseObject* pLinkedTo)
		: CObjectEvent(OBJECT_ON_PRELINKED)
		, m_pLinkedTo(pLinkedTo)
	{
	}

	CBaseObject* m_pLinkedTo;
};

struct CObjectUnLinkEvent : public CObjectEvent
{
	CObjectUnLinkEvent(CBaseObject* pLinkedTo)
		: CObjectEvent(OBJECT_ON_UNLINKED)
		, m_pLinkedTo(pLinkedTo)
	{
	}

	// !Object we are unlinking from. Necessary because pObj has already been detached and has no parent
	CBaseObject* m_pLinkedTo;
};

struct CObjectRenameEvent : public CObjectEvent
{
	CObjectRenameEvent(const string& prevName)
		: CObjectEvent(OBJECT_ON_RENAME)
		, m_name(prevName)
	{
	};

	string m_name;
};

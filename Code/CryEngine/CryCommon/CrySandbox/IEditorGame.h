// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include <CryEntitySystem/IEntityBasicTypes.h>
#include <CryGame/IGame.h>

struct IFlowSystem;
struct IGameTokenSystem;
namespace Telemetry {
struct ITelemetryRepository;
}

//! For game to access Editor functionality.
struct IGameToEditorInterface
{
	// <interfuscator:shuffle>
	virtual ~IGameToEditorInterface(){}
	virtual void SetUIEnums(const char* sEnumName, const char** sStringsArray, int nStringCount) = 0;
	// </interfuscator:shuffle>
};

struct IEquipmentSystemInterface
{
	struct IEquipmentItemIterator
	{
		struct SEquipmentItem
		{
			const char* name;
			const char* type;
		};
		// <interfuscator:shuffle>
		virtual ~IEquipmentItemIterator(){}
		virtual void AddRef() = 0;
		virtual void Release() = 0;
		virtual bool Next(SEquipmentItem& outItem) = 0;
		// </interfuscator:shuffle>
	};
	typedef _smart_ptr<IEquipmentItemIterator> IEquipmentItemIteratorPtr;

	// <interfuscator:shuffle>
	virtual ~IEquipmentSystemInterface(){}

	//! Return iterator with available equipment items of a certain type.
	//! Type can be empty to retrieve all items.
	virtual IEquipmentSystemInterface::IEquipmentItemIteratorPtr CreateEquipmentItemIterator(const char* type = "") = 0;
	virtual IEquipmentSystemInterface::IEquipmentItemIteratorPtr CreateEquipmentAccessoryIterator(const char* type) = 0;

	//! Delete all equipment packs.
	virtual void DeleteAllEquipmentPacks() = 0;

	//! Load a single equipment pack from an XmlNode.
	//! Equipment Pack is basically:
	//! <EquipPack name="BasicPack">.
	//!   <Items>.
	//!     <Scar type="Weapon" />.
	//!     <SOCOM type="Weapon" />.
	//!   </Items>.
	//!   <Ammo Scar="50" SOCOM="70" />.
	//! </EquipPack>.
	virtual bool LoadEquipmentPack(const XmlNodeRef& rootNode) = 0;

	// set the players equipment pack. maybe we enable this, but normally via FG only
	// virtual void SetPlayerEquipmentPackName(const char *packName) = 0;
	// </interfuscator:shuffle>
};

struct IGamePhysicsSettings
{
	virtual ~IGamePhysicsSettings() {}

	virtual const char* GetCollisionClassName(unsigned int bitIndex) = 0;
};

//! Interface used by the Editor to interact with the GameDLL.
struct IEditorGame
{
	typedef IEditorGame*(* TEntryFunction)();

	struct HelpersDrawMode
	{
		enum EType
		{
			Hide = 0,
			Show
		};
	};

	// <interfuscator:shuffle>
	CRY_DEPRECATED_GAME_DLL IEditorGame() = default;
	virtual ~IEditorGame(){}
	virtual bool                       Init(ISystem* pSystem, IGameToEditorInterface* pEditorInterface) = 0;
	virtual void                       Update(bool haveFocus, unsigned int updateFlags) = 0;
	virtual void                       Shutdown() = 0;
	virtual void                       SetGameMode(bool bGameMode) = 0;
	virtual bool                       SetPlayerPosAng(Vec3 pos, Vec3 viewDir) = 0;
	virtual void                       OnBeforeLevelLoad() = 0;
	virtual void                       OnAfterLevelInit(const char* levelName, const char* levelFolder) = 0;
	virtual void                       OnAfterLevelLoad(const char* levelName, const char* levelFolder) = 0;
	virtual void                       OnCloseLevel() = 0;
	virtual void                       OnSaveLevel() = 0;
	
	virtual bool                       BuildEntitySerializationList(XmlNodeRef output) = 0;
	virtual bool                       GetAdditionalMinimapData(XmlNodeRef output) = 0;

	virtual IEquipmentSystemInterface* GetIEquipmentSystemInterface() = 0;

	// telemetry functions: possibly should find a better place for these
	virtual void RegisterTelemetryTimelineRenderers(Telemetry::ITelemetryRepository* pRepository) = 0;

	//! Update (and render) all sorts of generic editor 'helpers'.
	//! This could be used, for example, to render certain metrics, boundaries, invalid links, etc.
	virtual void UpdateHelpers(const HelpersDrawMode::EType drawMode) {}

	virtual void OnDisplayRenderUpdated(bool displayHelpers) = 0;
	virtual void OnEntitySelectionChanged(EntityId entityId, bool isSelected) = 0;

	virtual IGamePhysicsSettings* GetIGamePhysicsSettings() = 0;
	// </interfuscator:shuffle>
};
//! \endcond
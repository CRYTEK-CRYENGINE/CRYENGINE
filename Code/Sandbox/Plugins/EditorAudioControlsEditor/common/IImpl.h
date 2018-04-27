// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SharedData.h"

#include <CrySystem/XML/IXml.h>

#ifdef PLUGIN_EXPORTS
	#define ACE_API DLL_EXPORT
#else
	#define ACE_API DLL_IMPORT
#endif

class CryIcon;
class QString;

namespace ACE
{
namespace Impl
{
struct IItem;
struct IItemModel;
struct ISettings;

struct IImpl
{
	//! \cond INTERNAL
	virtual ~IImpl() = default;
	//! \endcond

	//! Reloads all the middleware control data.
	//! \param preserveConnectionStatus - Keep the connection status of the controls. If true, when
	//! reloading the controls it will try to map the connected status they had previously
	//! (if they existed before the reload).
	virtual void Reload(bool const preserveConnectionStatus = true) = 0;

	//! Provides platform names to the middleware implementation.
	//! \param platforms - Platform names of the current project.
	//! Used for platform specific preload requests.
	virtual void SetPlatforms(Platforms const& platforms) = 0;

	//! This function returns the item model that is used to display data in the Middleware Data Panel's treeview.
	//! \return A pointer to the item model of the middleware.
	virtual IItemModel* GetItemModel() const = 0;

	//! Gets the middleware control given its unique id.
	//! \param id - Unique ID of the control.
	//! \return A pointer to the control that corresponds to the passed id. If none is found NULL is returned.
	virtual IItem* GetItem(ControlId const id) const = 0;

	//! Returns the icon corresponding to the middleware control passed as argument.
	//! \param pIItem - Middleware control
	//! \return An icon corresponding to the control type.
	virtual CryIcon const& GetItemIcon(IItem const* const pIItem) const = 0;

	//! Returns the icon corresponding to the middleware control passed as argument.
	//! \param pIItem - Middleware control
	//! \return A string with the type name corresponding to the control type.
	virtual QString const& GetItemTypeName(IItem const* const pIItem) const = 0;

	//! Gets the name of the implementation which might be used in the ACE UI.
	//! \return A string with the name of the implementation.
	virtual string const& GetName() const = 0;

	//! Gets the name of the implementation folder which might be used to construct paths to audio assets and ACE files.
	//! \return A string with the name of the implementation folder.
	virtual string const& GetFolderName() const = 0;

	//! Gets the settings for this implementation.
	virtual ISettings* GetSettings() = 0;

	//! Checks if the current middleware allows importing of files to the asset folder.
	virtual bool IsFileImportAllowed() const = 0;

	//! Checks if the given audio system control type is supported by the middleware implementation.
	//! \param assetType - An audio system control type.
	//! \return A bool if the type is supported or not.
	virtual bool IsSystemTypeSupported(EAssetType const assetType) const = 0;

	//! Checks if the given audio system control type and middleware control are compatible.
	//! \param assetType - An audio system control type.
	//! \param pIItem - A middleware control.
	//! \return A bool if the types are compatible or not.
	virtual bool IsTypeCompatible(EAssetType const assetType, IItem const* const pIItem) const = 0;

	//! The ACE lets the user create an audio system control out of a middleware control,
	//! for this it needs to convert a middleware control type to an audio system control type.
	//! \param pIItem - Middleware control
	//! \return An audio system control type that corresponds to the middleware control type passed as argument.
	virtual EAssetType ImplTypeToAssetType(IItem const* const pIItem) const = 0;

	//! Creates and returns a connection to a middleware control. The connection object is owned by this class.
	//! \param assetType - The type of the audio system control you are connecting to pMiddlewareControl.
	//! \param pIItem - Middleware control for which to make the connection.
	// \return A pointer to the newly created connection.
	virtual ConnectionPtr CreateConnectionToControl(EAssetType const assetType, IItem const* const pIItem) = 0;

	//! Creates and returns a connection defined in an XML node.
	//! The format of the XML node should be in sync with the CreateXMLNodeFromConnection function which is in charge of writing the node during serialization.
	//! If the XML node is unknown to the system NULL should be returned.
	//! If the middleware control referenced in the XML node does not exist it should be created and marked as "placeholder".
	//! \param pNode - XML node where the connection is defined.
	//! \param assetType - The type of the audio system control you are connecting to.
	//! \return A pointer to the newly created connection.
	virtual ConnectionPtr CreateConnectionFromXMLNode(XmlNodeRef pNode, EAssetType const assetType) = 0;

	//! When serializing connections between controls this function will be called once per connection to serialize its properties.
	//! This function should be in sync with CreateConnectionToControl as whatever it's written here will have to be read there.
	//! \param pConnection - Connection to serialize.
	//! \param assetType - Type of the audio system control that has this connection.
	//! \return An XML node with the connection serialized.
	virtual XmlNodeRef CreateXMLNodeFromConnection(ConnectionPtr const pConnection, EAssetType const assetType) = 0;

	//! Whenever a connection is added to an audio system control this function should be called to keep the system informed of which connections are being used.
	//! \param pConnection - Connection that has been enabled.
	virtual void EnableConnection(ConnectionPtr const pConnection) {}

	//! Whenever a connection is removed from an audio system control this function should be called to keep the system informed of which connections are being used.
	//! \param pConnection - Connection that has been disabled.
	virtual void DisableConnection(ConnectionPtr const pConnection) {}
};
} // namespace Impl
} // namespace ACE

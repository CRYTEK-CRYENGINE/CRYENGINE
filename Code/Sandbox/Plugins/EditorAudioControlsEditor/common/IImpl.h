// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ControlInfo.h"
#include "FileImportInfo.h"

#include <CrySandbox/CrySignal.h>
#include <FileDialogs/ExtensionFilter.h>

#ifdef ACE_PLUGIN_EXPORTS
	#define ACE_API DLL_EXPORT
#else
	#define ACE_API DLL_IMPORT
#endif

class QWidget;
class CryIcon;
class QString;
class QMimeData;
class XmlNodeRef;

namespace ACE
{
struct IConnection;

namespace Impl
{
struct IItem;

struct IImpl
{
	//! \cond INTERNAL
	virtual ~IImpl() = default;
	//! \endcond

	//! Initializes the middleware implementation.
	//! \param implInfo - Info struct to be filled by the middleware implementation.
	//! \param extensionFilters - File extension filters used for file import.
	//! \param supportedFileTypes - Supported file types used for file import.
	virtual void Initialize(SImplInfo& implInfo, ExtensionFilterVector& extensionFilters, QStringList& supportedFileTypes) = 0;

	//! Creates a new widget that is used for displaying middleware data in the middleware data panel.
	//! \return A pointer to the data panel.
	virtual QWidget* CreateDataPanel() = 0;

	//! Destroys the data panel.
	virtual void DestroyDataPanel() = 0;

	//! Reloads all the middleware control data.
	//! \param implInfo - Info struct to be filled by the middleware implementation.
	virtual void Reload(SImplInfo& implInfo) = 0;

	//! Gets the middleware control given its unique id.
	//! \param id - Unique ID of the control.
	//! \return A pointer to the control that corresponds to the passed id. If none is found NULL is returned.
	virtual IItem* GetItem(ControlId const id) const = 0;

	//! Returns the icon corresponding to the middleware control passed as argument.
	//! \param pIItem - Middleware control
	//! \return An icon corresponding to the control type.
	virtual CryIcon const& GetItemIcon(IItem const* const pIItem) const = 0;

	//! Returns the icon corresponding to the middleware control passed as argument.
	//! \param pIItem - Middleware control.
	//! \return A string with the type name corresponding to the control type.
	virtual QString const& GetItemTypeName(IItem const* const pIItem) const = 0;

	//! Sets the path to the middleware project.
	//! \param szPath - Folder path to the middleware project.
	virtual void SetProjectPath(char const* const szPath) = 0;

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
	//! \param assetType - The type of the audio system control you are connecting to the middleware control.
	//! \param pIItem - Middleware control for which to make the connection.
	// \return A pointer to the newly created connection.
	virtual IConnection* CreateConnectionToControl(EAssetType const assetType, IItem const* const pIItem) = 0;

	//! Creates and returns a connection defined in an XML node.
	//! The format of the XML node should be in sync with the CreateXMLNodeFromConnection function which is in charge of writing the node during serialization.
	//! If the XML node is unknown to the system NULL should be returned.
	//! If the middleware control referenced in the XML node does not exist it should be created and marked as "placeholder".
	//! \param node - XML node where the connection is defined.
	//! \param assetType - The type of the audio system control you are connecting to.
	//! \return A pointer to the newly created connection.
	virtual IConnection* CreateConnectionFromXMLNode(XmlNodeRef const& node, EAssetType const assetType) = 0;

	//! When serializing connections between controls this function will be called once per connection to serialize its properties.
	//! This function should be in sync with CreateConnectionToControl as whatever it's written here will have to be read there.
	//! \param pIConnection - Connection to serialize.
	//! \param assetType - Type of the audio system control that has this connection.
	//! \return An XML node with the connection serialized.
	virtual XmlNodeRef CreateXMLNodeFromConnection(IConnection const* const pIConnection, EAssetType const assetType, CryAudio::ContextId const contextId) = 0;

	//! The middleware implementation can provide an XML node that contains the amount of controls of the current library as attributes.
	//! \param szTag - Tag of the node.
	//! \param contextId - id of the context of the library that will contain the node.
	//! \return An XML node that contains the amount of controls. Return nullptr if the node should not get written to the file.
	virtual XmlNodeRef SetDataNode(char const* const szTag, CryAudio::ContextId const contextId) = 0;

	//! Signal the middleware implemementation that a library file will be written.
	virtual void OnBeforeWriteLibrary() = 0;

	//! Signal the middleware implemementation that a library file has been written.
	virtual void OnAfterWriteLibrary() = 0;

	//! Whenever a connection is added to an audio system control this function should be called to keep the system informed of which connections are being used.
	//! \param pIConnection - Connection that has been enabled.
	//! \param isLoading - Is data currently being loaded or not.
	virtual void EnableConnection(IConnection const* const pIConnection, bool const isLoading) = 0;

	//! Whenever a connection is removed from an audio system control this function should be called to keep the system informed of which connections are being used.
	//! \param pIConnection - Connection that has been disabled.
	//! \param isLoading - Is data currently being loaded or not.
	virtual void DisableConnection(IConnection const* const pIConnection, bool const isLoading) = 0;

	//! Free the memory and potentially other resources used by the supplied IConnection
	//! \param pIConnection - pointer to the connection to be discarded
	//! \return void
	virtual void DestructConnection(IConnection const* const pIConnection) = 0;

	//! Executed before data gets reloaded.
	virtual void OnBeforeReload() = 0;

	//! Executed after data has been reloaded.
	virtual void OnAfterReload() = 0;

	//! Request to select a middleware control which is connected to the selected audio system control.
	//! \param id - Id of the middleware control to select.
	virtual void OnSelectConnectedItem(ControlId const id) const = 0;

	//! Executed when file importer gets opened.
	virtual void OnFileImporterOpened() = 0;

	//! Executed when file importer gets closed.
	virtual void OnFileImporterClosed() = 0;

	//! Check if external data is allowed to drop. Used for file import outside the middleware data panel.
	virtual bool CanDropExternalData(QMimeData const* const pData) const = 0;

	//! Drops external data.  Used for file import outside the middleware data panel.
	virtual bool DropExternalData(QMimeData const* const pData, FileImportInfos& fileImportInfos) const = 0;

	//! Generates an item id based on the given parameters. Used for file import.
	//! \param name - name of the file.
	//! \param path - path of the file inside the audio/assets folder.
	//! \param isLocalized - bool if the item is localized or not.
	//! \return the id of the item.
	virtual ControlId GenerateItemId(QString const& name, QString const& path, bool const isLocalized) = 0;

	//! Signal to get infos of audio system controls that are connected to the selected middleware control.
	//! Used for context menu action to select a connected system control.
	//! \param ControlId - Id of the selected middleware control.
	//! \param SControlInfos - Container for infos of the connected audio system controls.
	CCrySignal<void(ControlId const, SControlInfos&)> SignalGetConnectedSystemControls;

	//! Signal to select an audio system control which is connected to the selected middleware control.
	//! \param ControlId - Id of the audio system control that should get selected.
	//! \param ControlId - Id of the selected middleware control.
	CCrySignal<void(ControlId const, ControlId const)> SignalSelectConnectedSystemControl;

	//! Signal to open a file selection dialog to import files.
	//! \param QString - Name of the target folder.
	//! \param bool - Is true when files to import are localized, otherwise false.
	CCrySignal<void(QString const&, bool const)> SignalImportFiles;

	//! Signal when files got dropped into the middleware data panel. Will open the file importer.
	//! \param FileImportInfos - Info struct of the files to import.
	//! \param QString - Name of the target folder.
	//! \param bool - Is true when files to import are localized, otherwise false.
	CCrySignal<void(FileImportInfos const&, QString const&, bool const)> SignalFilesDropped;
};
} // namespace Impl
} // namespace ACE

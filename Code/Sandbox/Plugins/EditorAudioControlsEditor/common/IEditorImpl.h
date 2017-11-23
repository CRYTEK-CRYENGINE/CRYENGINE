// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SystemTypes.h"

#include <CrySystem/XML/IXml.h>
#include <CryAudio/IAudioInterfacesCommonData.h>

#ifdef PLUGIN_EXPORTS
	#define ACE_API DLL_EXPORT
#else
	#define ACE_API DLL_IMPORT
#endif

namespace ACE
{
class CImplItem;

struct IImplSettings
{
	virtual char const* GetSoundBanksPath() const = 0;
	virtual char const* GetProjectPath() const = 0;
	virtual void        SetProjectPath(char const* szPath) = 0;
};

struct IEditorImpl
{
	virtual ~IEditorImpl() = default;

	// <title Reload>
	// Description:
	//      Reloads all the middleware control data
	// Arguments:
	//      bPreserveConnectionStatus - Keep the connection status of the controls. If true, when
	//			reloading the controls it will try to map the connected status they had previously
	//			(if they existed before the reload).
	virtual void Reload(bool const preserveConnectionStatus = true) = 0;

	// <title GetRoot>
	// Description:
	//      Middleware controls are organized in a tree structure.
	//      This function returns the root of the tree to allow for traversing the tree manually.
	// Returns:
	//      A pointer to the root of the control tree.
	// See Also:
	//      GetControl
	virtual CImplItem* GetRoot() = 0;

	// <title GetControl>
	// Description:
	//      Gets the middleware control given its unique id.
	// Arguments:
	//      id - Unique ID of the control.
	// Returns:
	//      A pointer to the control that corresponds to the passed id. If none is found NULL is returned.
	// See Also:
	//      GetRoot
	virtual CImplItem* GetControl(CID const id) const = 0;

	// <title GetTypeIcon>
	// Description:
	//      Returns the icon corresponding to the middleware control passed as argument.
	// Arguments:
	//      pItem - Middleware control
	// Returns:
	//      A string with the path to the icon corresponding to the control type
	// See Also:
	//
	virtual char const* GetTypeIcon(CImplItem const* const pItem) const = 0;

	// <title GetName>
	// Description:
	//      Gets the name of the implementation which might be used in the ACE UI.
	// Returns:
	//      String with the name of the implementation.
	virtual string GetName() const = 0;

	// <title GetSettings>
	// Description:
	//      Gets the settings for this implementation
	virtual IImplSettings* GetSettings() = 0;

	// <title IsTypeCompatible>
	// Description:
	//      Checks is the given audio system control type and middleware control are compatible.
	// Arguments:
	//      systemType - An audio system control type.
	//      pImplItem - A middleware control.
	// Returns:
	//      A bool if the types are compatible or not.
	// See Also:
	//      ImplTypeToSystemType
	virtual bool IsTypeCompatible(ESystemItemType const systemType, CImplItem const* const pImplItem) const = 0;

	// <title ImplTypeToSystemType>
	// Description:
	//      The ACE lets the user create an audio system control out of a middleware control, for this it needs to convert a middleware control type to an audio system control type.
	// Arguments:
	//      pItem - Middleware control
	// Returns:
	//      An audio system control type that corresponds to the middleware control type passed as argument.
	// See Also:
	//      GetCompatibleTypes
	virtual ESystemItemType ImplTypeToSystemType(CImplItem const* const pItem) const = 0;

	// <title CreateConnectionToControl>
	// Description:
	//      Creates and returns a connection to a middleware control. The connection object is owned by this class.
	// Arguments:
	//      controlType - The type of the audio system control you are connecting to pMiddlewareControl.
	//      pMiddlewareControl - Middleware control for which to make the connection.
	// Returns:
	//      A pointer to the newly created connection
	// See Also:
	//      CreateConnectionFromXMLNode

	virtual ConnectionPtr CreateConnectionToControl(ESystemItemType const controlType, CImplItem* const pMiddlewareControl) = 0;

	// <title CreateConnectionFromXMLNode>
	// Description:
	//      Creates and returns a connection defined in an XML node.
	//      The format of the XML node should be in sync with the CreateXMLNodeFromConnection function which is in charge of writing the node during serialization.
	//      If the XML node is unknown to the system NULL should be returned.
	//      If the middleware control referenced in the XML node does not exist it should be created and marked as "placeholder".
	// Arguments:
	//      pNode - XML node where the connection is defined.
	//			controlType - The type of the audio system control you are connecting to
	// Returns:
	//      A pointer to the newly created connection
	// See Also:
	//      CreateXMLNodeFromConnection
	virtual ConnectionPtr CreateConnectionFromXMLNode(XmlNodeRef pNode, ESystemItemType const controlType) = 0;

	// <title CreateXMLNodeFromConnection>
	// Description:
	//      When serializing connections between controls this function will be called once per connection to serialize its properties.
	//      This function should be in sync with CreateConnectionToControl as whatever it's written here will have to be read there.
	// Arguments:
	//      pConnection - Connection to serialize.
	//      controlType - Type of the audio system control that has this connection.
	// Returns:
	//      XML node with the connection serialized
	// See Also:
	//      CreateConnectionFromXMLNode
	virtual XmlNodeRef CreateXMLNodeFromConnection(ConnectionPtr const pConnection, ESystemItemType const controlType) = 0;

	// <title EnableConnection>
	// Description:
	//      Whenever a connection is added to an audio system control this function should be called to keep the system informed of which connections are being used.
	// Arguments:
	//      pConnection - Connection that has been enabled.
	// See Also:
	//
	virtual void EnableConnection(ConnectionPtr const pConnection) {}

	// <title DisableConnection>
	// Description:
	//      Whenever a connection is removed from an audio system control this function should be called to keep the system informed of which connections are being used.
	// Arguments:
	//      pConnection - Connection that has been disabled.
	// See Also:
	//
	virtual void DisableConnection(ConnectionPtr const pConnection) {}
};
} // namespace ACE

// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ACETypes.h"
#include <CrySystem/XML/IXml.h>
#include <CryAudio/IAudioInterfacesCommonData.h>

#ifdef PLUGIN_EXPORTS
	#define ACE_API DLL_EXPORT
#else
	#define ACE_API DLL_IMPORT
#endif

namespace ACE
{
class IAudioSystemItem;
class IAudioConnection;

typedef uint TImplControlTypeMask;

class IImplementationSettings
{
public:
	virtual const char* GetSoundBanksPath() const = 0;
	virtual const char* GetProjectPath() const = 0;
	virtual void        SetProjectPath(const char* szPath) = 0;
};

class IAudioSystemEditor
{
public:

	IAudioSystemEditor() = default;
	virtual ~IAudioSystemEditor() = default;

	// <title Reload>
	// Description:
	//      Reloads all the middleware control data
	// Arguments:
	//      bPreserveConnectionStatus - Keep the connection status of the controls. If true, when
	//			reloading the controls it will try to map the connected status they had previously
	//			(if they existed before the reload).
	virtual void Reload(bool bPreserveConnectionStatus = true) = 0;

	// <title GetRoot>
	// Description:
	//      Middleware controls are organized in a tree structure.
	//      This function returns the root of the tree to allow for traversing the tree manually.
	// Returns:
	//      A pointer to the root of the control tree.
	// See Also:
	//      GetControl
	virtual IAudioSystemItem* GetRoot() = 0;

	// <title GetControl>
	// Description:
	//      Gets the middleware control given its unique id.
	// Arguments:
	//      id - Unique ID of the control.
	// Returns:
	//      A pointer to the control that corresponds to the passed id. If none is found NULL is returned.
	// See Also:
	//      GetRoot
	virtual IAudioSystemItem* GetControl(CID id) const = 0;

	// <title ImplTypeToATLType>
	// Description:
	//      The ACE lets the user create an ATL control out of a middleware control, for this it needs to convert a middleware control type to an ATL control type.
	// Arguments:
	//      type - Middleware control type
	// Returns:
	//      An ATL control type that corresponds to the middleware control type passed as argument.
	// See Also:
	//      GetCompatibleTypes
	virtual EItemType ImplTypeToATLType(ItemType type) const = 0;

	// <title GetCompatibleTypes>
	// Description:
	//      Given an ATL control type this function returns all the middleware control types that can be connected to it.
	// Arguments:
	//      eATLControlType - An ATL control type.
	// Returns:
	//      A mask representing all the middleware control types that can be connected to the ATL control type passed as argument.
	// See Also:
	//      ImplTypeToATLType
	virtual TImplControlTypeMask GetCompatibleTypes(EItemType eATLControlType) const = 0;

	// <title CreateConnectionToControl>
	// Description:
	//      Creates and returns a connection to a middleware control. The connection object is owned by this class.
	// Arguments:
	//      eATLControlType - The type of the ATL control you are connecting to pMiddlewareControl.
	//      pMiddlewareControl - Middleware control for which to make the connection.
	// Returns:
	//      A pointer to the newly created connection
	// See Also:
	//      CreateConnectionFromXMLNode
	virtual ConnectionPtr CreateConnectionToControl(EItemType eATLControlType, IAudioSystemItem* pMiddlewareControl) = 0;

	// <title CreateConnectionFromXMLNode>
	// Description:
	//      Creates and returns a connection defined in an XML node.
	//      The format of the XML node should be in sync with the CreateXMLNodeFromConnection function which is in charge of writing the node during serialization.
	//      If the XML node is unknown to the system NULL should be returned.
	//      If the middleware control referenced in the XML node does not exist it should be created and marked as "placeholder".
	// Arguments:
	//      pNode - XML node where the connection is defined.
	//			eATLControlType - The type of the ATL control you are connecting to
	// Returns:
	//      A pointer to the newly created connection
	// See Also:
	//      CreateXMLNodeFromConnection
	virtual ConnectionPtr CreateConnectionFromXMLNode(XmlNodeRef pNode, EItemType eATLControlType) = 0;

	// <title CreateXMLNodeFromConnection>
	// Description:
	//      When serializing connections between controls this function will be called once per connection to serialize its properties.
	//      This function should be in sync with CreateConnectionToControl as whatever it's written here will have to be read there.
	// Arguments:
	//      pConnection - Connection to serialize.
	//      eATLControlType - Type of the ATL control that has this connection.
	// Returns:
	//      XML node with the connection serialized
	// See Also:
	//      CreateConnectionFromXMLNode
	virtual XmlNodeRef CreateXMLNodeFromConnection(const ConnectionPtr pConnection, const EItemType eATLControlType) = 0;

	// <title EnableConnection>
	// Description:
	//      Whenever a connection is added to an ATL control this function should be called to keep the system informed of which connections are being used.
	// Arguments:
	//      pConnection - Connection that has been enabled.
	// See Also:
	//
	virtual void EnableConnection(ConnectionPtr pConnection) {}

	// <title DisableConnection>
	// Description:
	//      Whenever a connection is removed from an ATL control this function should be called to keep the system informed of which connections are being used.
	// Arguments:
	//      pConnection - Connection that has been disabled.
	// See Also:
	//
	virtual void DisableConnection(ConnectionPtr pConnection) {}

	// <title GetTypeIcon>
	// Description:
	//      Returns the icon corresponding to the middleware control type passed as argument.
	// Arguments:
	//      type - Middleware control type
	// Returns:
	//      A string with the path to the icon corresponding to the control type
	// See Also:
	//
	virtual const char* GetTypeIcon(ItemType type) const = 0;

	// <title GetName>
	// Description:
	//      Gets the name of the implementation which might be used in the ACE UI.
	// Returns:
	//      String with the name of the implementation.
	virtual string GetName() const = 0;

	// <title GetSettings>
	// Description:
	//      Gets the settings for this implementation
	virtual IImplementationSettings* GetSettings() = 0;
};

}

#include <CrySystem/ISystem.h>

// Example of how an XML file can be parsed from a buffer
void ParseXml()
{
	const string xmlExample =
		"<XmlExample>"
		"	<Child firstAttribute=\"1\"/ stringAttribute=\"MyString\">"
		"</XmlExample>";

	// Parse the XML from the provided string, note that this can be replaced with ISystem::LoadXmlFromFile to load from disk.
	if (XmlNodeRef rootNode = gEnv->pSystem->LoadXmlFromBuffer(xmlExample.data(), xmlExample.length()))
	{
		// Iterate through all the children in the root node (in our case, "XmlExample")
		for (int i = 0, n = rootNode->getChildCount(); i < n; ++i)
		{
			// Get the child node at the specified index
			// Note that in our example this will only be triggered once, for "Child"
			XmlNodeRef childNode = rootNode->getChild(i);

			// Initialize 'value' to false by default, in case getAttr fails (the attribute did not exist on the node)
			bool value = false;
			// Get the attribute from the node, and assign to 'value'
			// Note that numerous overloads exist, this does not apply exclusively to booleans
			if (childNode->getAttr("firstAttribute", value))
			{
				// Child node had the attribute "firstAttribute", and the value is now assigned to 'value'
			}

			// Get the value of the string attribute
			// This is demonstrated as strings are wrapped by a separate class, unless primitives of other types
			// It is *not* possible to get attributes using normal CryString's or std::string.
			XmlString stringAttribute;
			if (childNode->getAttr("stringAttribute", stringAttribute))
			{
				// Child node had the attribute "stringAttribute", and the value is now assigned to 'stringAttribute'
				// Note that the data contained in stringAttribute will be invalid when the LoadXmlFromBuffer scope exits.
			}
		}
	}
}
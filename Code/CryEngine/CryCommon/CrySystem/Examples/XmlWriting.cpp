#include <CrySystem/ISystem.h>

// Example of how an XML structure can be created programmatically and saved to disk
void WriteXml()
{
	// Create an empty XML node with the name "MyRootNode"
	XmlNodeRef rootNode = gEnv->pSystem->CreateXmlNode("MyRootNode");

	// Create a node that we will later attach to the root as a child
	XmlNodeRef childNode = gEnv->pSystem->CreateXmlNode("MyChildNode");
	childNode->setAttr("firstAttribute", true);
	childNode->setAttr("stringAttribute", "MyString");

	// Add the child node to "MyRootNode"
	rootNode->addChild(childNode);

	// Save the XML to disk, relative to the asset directory
	rootNode->saveToFile("MyXmlFile.xml");
}
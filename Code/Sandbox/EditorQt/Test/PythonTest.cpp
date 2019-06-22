// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>

#include "Objects/ObjectManager.h"
#include <Objects/ObjectLoader.h>
#include <CrySystem/Testing/CryTest.h>
#include <CrySystem/Testing/CryTestCommands.h>
#include <PathUtils.h>

namespace Private_EditorTest
{
//! Call a function in a python module
void CallPythonFunction(PyObject* pModule, const string& functionName)
{
	//Get the function attribute from the function name
	PyObject* pFunction = PyObject_GetAttrString(pModule, functionName.c_str());
	//make sure it's valid and callable
	bool isCallableFunction = pFunction && PyCallable_Check(pFunction);
	//Debug break here if it's not valid
	CRY_TEST_ASSERT(isCallableFunction, "Unable to call %s function", functionName.c_str());
	//Call the function if ist's a valid callable
	if (isCallableFunction)
	{
		PyObject_CallFunction(pFunction, nullptr);
	}
	//we don't need the function object anymore, clean it up
	Py_XDECREF(pFunction);
}

//! Serialize a node into an xml archive and return a smart pointer reference of it
XmlNodeRef SerializeIntoArchive(CBaseObject* pObject, const std::string& tagName)
{
	XmlNodeRef node = XmlHelpers::CreateXmlNode(tagName.c_str());
	CObjectArchive archive(GetIEditor()->GetObjectManager(), node, false);
	pObject->Serialize(archive);
	return node;
}

//! Compare an attribute between two xml nodes
//! \param attributeIndex the index of an attribute in node
//! \param resultInfo the result of the comparison operation
bool CompareAttribute(XmlNodeRef node, XmlNodeRef nodeToCompare, int attributeIndex, string& resultInfo)
{
	bool result = true;

	const char* key;
	const char* nodeAttrValue;
	const char* nodeToCompareAttrValue;
	//Get the key of the attribute from the attributeIndex and make sure they both exist
	if (node->getAttributeByIndex(attributeIndex, &key, &nodeAttrValue) && nodeToCompare->getAttr(key, &nodeToCompareAttrValue))
	{
		//Compare the attribute data and make sure they are the same 
		if (strcmp(nodeAttrValue, nodeToCompareAttrValue) != 0)
		{
			resultInfo.Format("[Attribute Compare Failed]: Attribute \"%s\" does not have the same values: node tagged %s is \"%s\" node tagged \"%s\" is \"%s\"", key, node->getTag(), nodeToCompare->getTag(), nodeAttrValue, nodeToCompareAttrValue);
			result = false;
		}
	}
	else
	{
		resultInfo.Format("[Attribute Compare Failed]: Attribute \"%s\" is not found in node tagged \"%s\"", key, nodeToCompare->getTag());
		result = false;
	}

	return result;
}

//! Compare a node attributes and then all it's children recursively
bool CompareNode(XmlNodeRef preTestSerializedObject, XmlNodeRef postTestSerializedObject, string& resultInfo)
{
	bool result = true;

	if (preTestSerializedObject != postTestSerializedObject)
	{
		XmlNodeRef archiveToIterate = preTestSerializedObject;
		XmlNodeRef archiveToCompare = postTestSerializedObject;

		/*
		If the attribute count is different we will fail the comparison
		still we want to check all the attributes that will fail comparison and all the attributes that one node has and another one does not 
		so that they can be displayed to the user in an error message
		*/
		//Get the node with the biggest number of attributes and set it as the archiveToIterate
		if (preTestSerializedObject->getNumAttributes() != postTestSerializedObject->getNumAttributes())
		{
			if (preTestSerializedObject->getNumAttributes() > postTestSerializedObject->getNumAttributes())
			{
				archiveToIterate = preTestSerializedObject;
				archiveToCompare = postTestSerializedObject;
			}
			else
			{
				archiveToIterate = postTestSerializedObject;
				archiveToCompare = preTestSerializedObject;
			}

			//Test is already failed, prepare an error message
			resultInfo.Format("[Node tagged \"%s\" Compare Failed]: Nodes have a different number of attributes, \"%s\" node has: %d while \"%s\" node has: %d\n", archiveToIterate->getTag(), archiveToIterate->getTag(), archiveToIterate->getNumAttributes(), archiveToIterate->getTag(), archiveToCompare->getNumAttributes());
			result = false;
		}

		//Compare all the attributes and collect all the error messages (if any)
		std::vector<string> attributesComparisonErrors;
		for (int attributeIdx = 0; attributeIdx < archiveToIterate->getNumAttributes(); attributeIdx++)
		{
			//If a node has errors store the error message 
			string attributeCompareInfo;
			if (!CompareAttribute(archiveToIterate, archiveToCompare, attributeIdx, attributeCompareInfo))
			{
				attributesComparisonErrors.push_back(attributeCompareInfo);
			}
		}

		//If we have any error message it means the comparison failed, we should set the test result as failed and add all messages to the error string 
		if (attributesComparisonErrors.size())
		{
			result = false;
			resultInfo.append(string().Format("[Node tagged \"%s\" Compare Failed]: Node tagged \"%s\" and node tagged \"%s\" nodes have failed attributes compare with errors:\n", archiveToIterate->getTag(), archiveToIterate->getTag(), archiveToCompare->getTag()));
			for (string& errorInfo : attributesComparisonErrors)
			{
				resultInfo.append(string("->" + errorInfo + "\n"));
			}
		}

		if (result)
		{
			//Then if they have the same attributes recursively check the children
			if (preTestSerializedObject->getChildCount() == postTestSerializedObject->getChildCount())
			{
				for (int childIdx = 0; childIdx < preTestSerializedObject->getChildCount(); childIdx++)
				{
					if (!CompareNode(preTestSerializedObject->getChild(childIdx), postTestSerializedObject->getChild(childIdx), resultInfo))
					{
						result = false;
						break;
					}
				}
			}
			else
			{
				resultInfo.Format("[Node tagged \"%s\" Compare Failed]: Node tagged \"%s\" and node tagged \"%s\" nodes have a different child count", preTestSerializedObject->getTag(), archiveToIterate->getTag(), archiveToCompare->getTag());
				result = false;
			}
		}
	}

	return result;
}

//!Compare the two vectors of xml archives, comparison will be preTestSerializedObjects[i] == postTestSerializedObjects[i]
//! \param resultInfo stores an error log if the comparison failed
bool CompareXMLArchives(const std::vector<XmlNodeRef>& preTestSerializedObjects, const std::vector<XmlNodeRef>& postTestSerializedObjects, string& resultInfo)
{
	//make sure the vectors have the same sizes
	CRY_TEST_CHECK_EQUAL(preTestSerializedObjects.size(), postTestSerializedObjects.size());
	if (preTestSerializedObjects.size() != postTestSerializedObjects.size())
	{
		return false;
	}

	bool result = true;
	for (int i = 0; i < preTestSerializedObjects.size(); i++)
	{
		XmlNodeRef preTest = preTestSerializedObjects[i];
		XmlNodeRef postTest = postTestSerializedObjects[i];
		//Compare the nodes attributes and children to look for inconsistencies
		if (!CompareNode(preTest, postTest, resultInfo))
		{
			result = false;
			break;
		}
	}

	return result;
}
//! Find and serialize in xml archives all the objects in a python list, the objects are identified by name
//! \param serializedObjects this vector will contain the serialized objects xml archives
//! \param pObjectsList the python object containing a list of object names
void SerializePythonObjects(std::vector<XmlNodeRef>& serializedObjects, PyObject* pObjectsList, const std::string& archiveTag)
{
	//Iterate through all the string objects in the python list object
	for (int i = 0; i < PyList_Size(pObjectsList); i++)
	{
		//Get an item, get the underlying string and use it to find an object in the object manager
		PyObject* pListItem = PyList_GetItem(pObjectsList, i);
		bool isItemValid = pListItem && PyString_Check(pListItem);
		CRY_TEST_ASSERT(isItemValid, "Contents of the objectsToTest list must be of type string");

		if (isItemValid)
		{
			//Get the char string from the python object
			char* objectName = PyString_AsString(pListItem);
			CBaseObject* pObjectToTest = GetIEditor()->GetObjectManager()->FindObject(objectName);
			CRY_TEST_ASSERT(pObjectToTest, "Object named %s does not exist", objectName);

			if (pObjectToTest)
			{
				serializedObjects.push_back(SerializeIntoArchive(pObjectToTest, archiveTag));
			}
		}
	}
}

void RunPythonScript()
{
	//The tests are located in <EngineInstallation>/Editor/Scripts/Test
	string testScriptsPath;
	testScriptsPath.Format("%s/Test", PathUtil::ToUnixPath(PathUtil::GetEditorScriptsFolderPath()).c_str());

	/*
	   The following code adds the scripts test path to python sys path, it's the equivalent to this python code:

	   import sys
	   sys.path.append(\"<test scripts path>/\")\n

	 */

	//Get current system path
	PyObject* pSystemPath = PySys_GetObject("path");

	//Turn the system path string into a python object and append it to the python path
	PyObject* pTestScriptsPathString = PyString_FromString(testScriptsPath.c_str());

	//This will be the location of the new item in the list
	int newElementInListPosition = PyList_Size(pSystemPath);
	PyList_Append(pSystemPath, pTestScriptsPathString);
	Py_XDECREF(pTestScriptsPathString);

	std::vector<string> files;
	//Get all .py files in the Test directory so that we can import and execute them
	SDirectoryEnumeratorHelper helper;
	helper.ScanDirectoryRecursive("", testScriptsPath, "*.py", files);

	for (string& file : files)
	{
		PyObject* pStringObject = PyString_FromString(PathUtil::GetFileName(file.c_str()));

		PyObject* pModulesDictionary = PyImport_GetModuleDict();
		int moduleFound = PyDict_Contains(pModulesDictionary, pStringObject);

		PyObject* pImportedModule;
		if (moduleFound == 1)
		{
			PyObject* pModuleToReload = PyDict_GetItem(pModulesDictionary, pStringObject);
			//Module is already loaded, we need a reload in case module code was changed
			pImportedModule = PyImport_ReloadModule(pModuleToReload);
		}
		else
		{
			//Actually import the module using the file name
			pImportedModule = PyImport_Import(pStringObject);
		}

		if (pImportedModule)
		{
			//Find if the objectsToTest list containing the objects we want to serialize and test has been defined in the python script
			PyObject* pyObjectsToTestList = PyObject_GetAttrString(pImportedModule, "objectsToTest");

			bool isObjectsToTestValid = pyObjectsToTestList && PyList_Check(pyObjectsToTestList);
			CRY_TEST_ASSERT(isObjectsToTestValid, "Failed to find the objectsToTest list, this needs to be defined for the testing code to know which objects needs to be observed");

			//Call the test script "setup" function, this will setup the test and instantiate all necessary objects and add them to the objectsToTest list
			CallPythonFunction(pImportedModule, "setup");

			//The serialized objects archives
			std::vector<XmlNodeRef> preTestObjectsArchive;

			//Serialize the objects to test into archives
			SerializePythonObjects(preTestObjectsArchive, pyObjectsToTestList, "PreTest");

			CRY_TEST_ASSERT(!preTestObjectsArchive.empty(), "objectsToTest list is empty, no objects to test are available");

			//We have objects to test, proceed with the code
			if (preTestObjectsArchive.size())
			{
				//Call the test script "executeTest" function, this will execute the functions to test
				CallPythonFunction(pImportedModule, "executeTest");
			}

			std::vector<XmlNodeRef> postTestObjectsArchive;
			//Serialize the objects to test into archives
			SerializePythonObjects(postTestObjectsArchive, pyObjectsToTestList, "PostTest");

			string resultInfo;
			//Compare the archives serialized before and after the testing python script
			bool result = CompareXMLArchives(preTestObjectsArchive, postTestObjectsArchive, resultInfo);

			CRY_TEST_ASSERT(result, "XML Test Failed, error message is:\n%s\n", resultInfo.c_str());

			//We have objects to test, proceed with the code
			if (preTestObjectsArchive.size())
			{
				//Call the test script "executeTest" function, this should destroy every object created for this test so that are tests can subsequently be called on an empty level
				CallPythonFunction(pImportedModule, "cleanup");
			}
			//we don't need the list anymore, clean it up
			Py_XDECREF(pyObjectsToTestList);
		}

		//Delete the testing scripts path we added to the list
		PyList_SetSlice(pSystemPath, newElementInListPosition, newElementInListPosition, nullptr);

		//Clean up referenced objects
		Py_XDECREF(pImportedModule);
		Py_XDECREF(pStringObject);
	}
}
};

CRY_TEST(PythonTestUndoRedo, editor = true, game = false, timeout = 900000)
{
	commands =
	{
		Private_EditorTest::RunPythonScript };
}
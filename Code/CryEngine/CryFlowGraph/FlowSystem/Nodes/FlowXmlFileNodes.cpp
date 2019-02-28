// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlowXmlFileNodes.cpp
//  Description: Flowgraph nodes for opening/saving Xml documents
// -------------------------------------------------------------------------
//  History:
//    - 8/16/08 : File created - Kevin Kirst
//    - 09/06/2011: Added to SDK - Sascha Hoba
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FlowBaseXmlNode.h"

#define XML_FILELOC_ENUM ("enum_int:Map=0,Game=1,Root=2,User=3")
enum XML_FILELOC
{
	XML_FILELOC_MAP = 0,
	XML_FILELOC_GAME,
	XML_FILELOC_ROOT,
	XML_FILELOC_USER,
};

////////////////////////////////////////////////////
class CFlowXmlNode_NewDocument : public CFlowXmlNode_Base
{
	enum EInputs
	{
		EIP_Root = EIP_CustomStart,
	};

public:
	////////////////////////////////////////////////////
	CFlowXmlNode_NewDocument(SActivationInfo* pActInfo) : CFlowXmlNode_Base(pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowXmlNode_NewDocument(void)
	{

	}

	////////////////////////////////////////////////////
	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{

	}

	////////////////////////////////////////////////////
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		// Define input ports here, in same order as EInputPorts
		static const SInputPortConfig inputs[] =
		{
			ADD_BASE_INPUTS(),
			InputPortConfig<string>("Root","Root",  _HELP("Name of root element in new document"), 0, 0),
			{ 0 }
		};

		// Define output ports here, in same oreder as EOutputPorts
		static const SOutputPortConfig outputs[] =
		{
			ADD_BASE_OUTPUTS(),
			{ 0 }
		};

		// Fill in configuration
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Creates a blank document for writing new data into");
		config.SetCategory(EFLN_APPROVED);
	}

	////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowXmlNode_NewDocument(pActInfo);
	}

	////////////////////////////////////////////////////
	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	////////////////////////////////////////////////////
	virtual bool Execute(SActivationInfo* pActInfo)
	{
		bool bResult = false;

		SXmlDocument* doc;
		if (GDM->GetXmlDocument(pActInfo->pGraph, &doc))
		{
			doc->root = gEnv->pSystem->CreateXmlNode(GetPortString(pActInfo, EIP_Root));
			doc->active = doc->root;
			bResult = (NULL != doc->root);
		}

		return bResult;
	}
};

////////////////////////////////////////////////////
class CFlowXmlNode_SaveDocument : public CFlowXmlNode_Base
{
	enum EInputs
	{
		EIP_File = EIP_CustomStart,
		EIP_Location,
		EIP_Overwrite,
	};

public:
	////////////////////////////////////////////////////
	CFlowXmlNode_SaveDocument(SActivationInfo* pActInfo) : CFlowXmlNode_Base(pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowXmlNode_SaveDocument(void)
	{

	}

	////////////////////////////////////////////////////
	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{

	}

	////////////////////////////////////////////////////
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		// Define input ports here, in same order as EInputPorts
		static const SInputPortConfig inputs[] =
		{
			ADD_BASE_INPUTS(),
			InputPortConfig<string>("File",   "Default",  _HELP("File name of saved Xml document"),                             0, 0),
			InputPortConfig<int>("Location",  0,          _HELP("Local folder to place Xml document"),                          0, XML_FILELOC_ENUM),
			InputPortConfig<bool>("Overwrite",true,       _HELP("Specify if document should overwrite existing Xml document")),
			{ 0 }
		};

		// Define output ports here, in same oreder as EOutputPorts
		static const SOutputPortConfig outputs[] =
		{
			ADD_BASE_OUTPUTS(),
			{ 0 }
		};

		// Fill in configuration
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Saves active Xml data to disk");
		config.SetCategory(EFLN_APPROVED);
	}

	////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowXmlNode_SaveDocument(pActInfo);
	}

	////////////////////////////////////////////////////
	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	////////////////////////////////////////////////////
	virtual bool Execute(SActivationInfo* pActInfo)
	{
		bool bResult = false;

		SXmlDocument* doc;
		if (GDM->GetXmlDocument(pActInfo->pGraph, &doc) && doc->root)
		{
			const string file = GetPortString(pActInfo, EIP_File);
			const int loc = GetPortInt(pActInfo, EIP_Location);
			const bool bOverwrite = GetPortBool(pActInfo, EIP_Overwrite);

			ICryPak* pPak = gEnv->pCryPak;

			// Qualify path
			string szSavePath;
			switch (loc)
			{
			case XML_FILELOC_MAP:
				{
					if (gEnv->IsEditor())
					{
						char* levelName;
						char* levelPath;
						gEnv->pGameFramework->GetEditorLevel(&levelName, &levelPath);
						szSavePath = levelPath;
					}
					else
					{
						ILevelInfo* pLevel = gEnv->pGameFramework->GetILevelSystem()->GetCurrentLevel();
						if (pLevel)
						{
							szSavePath = pLevel->GetPath();
						}
					}
				}
				break;
			case XML_FILELOC_GAME:
				{
					szSavePath = PathUtil::GetGameFolder();
				}
				break;
			case XML_FILELOC_USER:
				{
					szSavePath = "%USER%/";
				}
				break;
			}
			const string szSaveFile = PathUtil::Make(szSavePath.c_str(), file, ".xml");

			// Check if file already exists
			if (bOverwrite || !pPak->IsFileExist(szSaveFile.c_str()))
			{
				bResult = doc->root->saveToFile(szSaveFile.c_str());
			}
		}

		return bResult;
	}
};

////////////////////////////////////////////////////
class CFlowXmlNode_OpenDocument : public CFlowXmlNode_Base
{
	enum EInputs
	{
		EIP_File = EIP_CustomStart,
		EIP_Location,
	};

public:
	////////////////////////////////////////////////////
	CFlowXmlNode_OpenDocument(SActivationInfo* pActInfo) : CFlowXmlNode_Base(pActInfo)
	{

	}

	////////////////////////////////////////////////////
	virtual ~CFlowXmlNode_OpenDocument(void)
	{

	}

	////////////////////////////////////////////////////
	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{

	}

	////////////////////////////////////////////////////
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		// Define input ports here, in same order as EInputPorts
		static const SInputPortConfig inputs[] =
		{
			ADD_BASE_INPUTS(),
			InputPortConfig<string>("File", "Default",  _HELP("File name of Xml document to open"),            0, 0),
			InputPortConfig<int>("Location",0,          _HELP("Local folder where Xml document can be found"), 0, XML_FILELOC_ENUM),
			{ 0 }
		};

		// Define output ports here, in same oreder as EOutputPorts
		static const SOutputPortConfig outputs[] =
		{
			ADD_BASE_OUTPUTS(),
			{ 0 }
		};

		// Fill in configuration
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Opens an Xml document from disk");
		config.SetCategory(EFLN_APPROVED);
	}

	////////////////////////////////////////////////////
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowXmlNode_OpenDocument(pActInfo);
	}

	////////////////////////////////////////////////////
	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	////////////////////////////////////////////////////
	virtual bool Execute(SActivationInfo* pActInfo)
	{
		bool bResult = false;

		SXmlDocument* doc;
		if (GDM->GetXmlDocument(pActInfo->pGraph, &doc))
		{
			const string file = GetPortString(pActInfo, EIP_File);
			const int loc = GetPortInt(pActInfo, EIP_Location);

			ICryPak* pPak = gEnv->pCryPak;

			// Qualify path
			string szLoadPath;
			switch (loc)
			{
			case XML_FILELOC_MAP:
				{
					if (gEnv->IsEditor())
					{
						char* levelName;
						char* levelPath;
						gEnv->pGameFramework->GetEditorLevel(&levelName, &levelPath);
						szLoadPath = levelPath;
					}
					else
					{
						ILevelInfo* pLevel = gEnv->pGameFramework->GetILevelSystem()->GetCurrentLevel();
						if (pLevel)
						{
							szLoadPath = pLevel->GetPath();
						}
					}
				}
				break;
			case XML_FILELOC_GAME:
				{
					szLoadPath = PathUtil::GetGameFolder();
				}
				break;
			case XML_FILELOC_USER:
				{
					szLoadPath = "%USER%/";
				}
				break;
			}
			const string szLoadFile = PathUtil::Make(szLoadPath.c_str(), file, ".xml");

			// Load file
			doc->root = gEnv->pSystem->LoadXmlFromFile(szLoadFile.c_str());
			doc->active = doc->root;
			bResult = (NULL != doc->root);
		}

		return bResult;
	}
};

////////////////////////////////////////////////////
////////////////////////////////////////////////////

REGISTER_FLOW_NODE("Xml:NewDocument", CFlowXmlNode_NewDocument);
REGISTER_FLOW_NODE("Xml:SaveDocument", CFlowXmlNode_SaveDocument);
REGISTER_FLOW_NODE("Xml:OpenDocument", CFlowXmlNode_OpenDocument);

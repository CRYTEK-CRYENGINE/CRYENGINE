// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a registry for vehicle damages templates

   -------------------------------------------------------------------------
   History:
   - 18:07:2006: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "CryAction.h"
#include "Serialization/XMLScriptLoader.h"
#include "IVehicleSystem.h"
#include "Vehicle.h"
#include "VehicleDamagesGroup.h"
#include "VehicleDamagesTemplateRegistry.h"

//------------------------------------------------------------------------
bool CVehicleDamagesTemplateRegistry::Init(const string& damagesTemplatesPath)
{
	if (damagesTemplatesPath.empty())
		return false;

	m_templates.clear();

	ICryPak* pCryPak = gEnv->pCryPak;

	_finddata_t fd;
	int ret;
	intptr_t handle;

	if ((handle = pCryPak->FindFirst(damagesTemplatesPath + string("*") + ".xml", &fd)) != -1)
	{
		do
		{
			string name(fd.name);

			if (name.substr(0, 4) != "def_")
			{
				string filename = damagesTemplatesPath + name;

				if (!RegisterTemplates(filename))
					CryLog("VehicleDamagesTemplateRegistry: error parsing template file <%s>.", filename.c_str());
			}

			ret = pCryPak->FindNext(handle, &fd);
		}
		while (ret >= 0);

		pCryPak->FindClose(handle);
	}

	return true;
}

//------------------------------------------------------------------------
bool CVehicleDamagesTemplateRegistry::RegisterTemplates(const string& filename)
{
	XmlNodeRef table = gEnv->pSystem->LoadXmlFromFile(filename);
	if (!table)
		return false;

	if (XmlNodeRef damagesGroupsTable = table->findChild("DamagesGroups"))
	{
		int i = 0;
		int c = damagesGroupsTable->getChildCount();

		for (; i < c; i++)
		{
			if (XmlNodeRef damagesGroupTable = damagesGroupsTable->getChild(i))
			{
				string name = damagesGroupTable->getAttr("name");
				if (!name.empty())
					m_templates.insert(TTemplateMap::value_type(name, damagesGroupTable));
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------
bool CVehicleDamagesTemplateRegistry::UseTemplate(const string& templateName, IVehicleDamagesGroup* pDamagesGroup)
{
	TTemplateMap::iterator ite = m_templates.find(templateName);
	if (ite != m_templates.end())
	{
		CVehicleModificationParams noModifications;
		CVehicleParams tmpVehicleParams(ite->second, noModifications);
		return pDamagesGroup->ParseDamagesGroup(tmpVehicleParams);
	}

	return false;
}

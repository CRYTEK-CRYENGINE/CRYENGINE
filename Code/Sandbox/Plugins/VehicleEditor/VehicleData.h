// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VehicleFactory_h__
#define __VehicleFactory_h__

#pragma once

#include <vector>
#include "Util/Variable.h"

#define DEBUGDRAW_VEED 10

const static string VEHICLE_XML_PATH = "scripts/entities/vehicles/implementations/xml/";
const static string VEHICLE_XML_DEF = "scripts/entities/vehicles/def_vehicle.xml";
const static string VEED_DEFAULTS = "scripts/entities/vehicles/veed_defaults.xml";

IVariablePtr LoadDefaultData();

/*!
 * Veed data structure holding a vehicle's data
 * This holds just a root variable and can be used to implement other logic.
 */
struct IVehicleData
{
	virtual IVariablePtr GetRoot() = 0;
};
class CVehicleData : public IVehicleData
{
public:
	CVehicleData()
	{
		m_pRoot = new CVariableArray;
	}
	~CVehicleData()
	{
		m_pRoot->Release();
	}
	IVariablePtr GetRoot() { return m_pRoot; }

	/**
	 * Get IVariable with default vehicle values
	 */
	static const IVariable* GetDefaultVar();

	/**
	 * Get Xml definition
	 */
	static const XmlNodeRef& GetXMLDef();

	/**
	 * Takes a IVariable, compares it to the specified default var,
	 * and fills missing children
	 */
	static void FillDefaults(IVariable* pVar, const char* defaultVar, const IVariable* pParent = GetDefaultVar());

protected:
	IVariablePtr m_pRoot;

	// static data
	static XmlNodeRef m_xmlDef;
	static IVariable* m_pDefaults;

};

/**
 * Get a IVariable in a CVariableArray by name
 */
IVariable* GetChildVar(const IVariable* array, const char* name, bool recursive = false);

IVariable* GetOrCreateChildVar(IVariable* array, const char* name, bool searchRec = false, bool createRec = false);

bool       HasChildVar(const IVariable* array, const IVariable* child, bool recursive = false);

IVariable* GetParentVar(IVariable* array, const IVariable* child);

void       ReplaceChildVars(IVariable* from, IVariable* to);

void       DumpVariable(IVariable* pVar, int level = 0);

/**
 * Create a Variable tree from an element in the xml definition
 */
IVariable* CreateVarFromDefNode(XmlNodeRef node);
IVariable* CreateDefaultVar(const char* name, bool rec = false);
IVariable* CreateDefaultChildOf(const char* name);

#endif


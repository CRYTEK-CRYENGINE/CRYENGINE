// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VEHICLEXMLLOADER_H__
#define __VEHICLEXMLLOADER_H__

#pragma once

struct IVehicleData;

IVehicleData* VehicleDataLoad(const char* definitionFile, const char* dataFile, bool bFillDefaults = true);
IVehicleData* VehicleDataLoad(const XmlNodeRef& definition, const char* dataFile, bool bFillDefaults = true);
IVehicleData* VehicleDataLoad(const XmlNodeRef& definition, const XmlNodeRef& data, bool bFillDefaults = true);

#endif


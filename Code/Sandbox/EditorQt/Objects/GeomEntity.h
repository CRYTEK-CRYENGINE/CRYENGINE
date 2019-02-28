// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EntityObject.h"

//////////////////////////////////////////////////////////////////////////
// Geometry entity.
//////////////////////////////////////////////////////////////////////////
class CGeomEntity : public CEntityObject
{
	typedef CEntityObject Super;

public:
	DECLARE_DYNCREATE(CGeomEntity)

	//////////////////////////////////////////////////////////////////////////
	// CEntity
	//////////////////////////////////////////////////////////////////////////
	virtual bool       Init(CBaseObject* prev, const string& file) override;
	virtual void       InitVariables() override;
	virtual bool       ConvertFromObject(CBaseObject* object) override;
	virtual bool       HasMeasurementAxis() const override { return true; }
	virtual bool       IsSimilarObject(CBaseObject* pObject) override;
	virtual void       GetScriptProperties(XmlNodeRef xmlProperties) override;
	virtual void       InvalidateGeometryFile(const string& file) override;
	virtual XmlNodeRef Export(const string& levelPath, XmlNodeRef& xmlNode) override;

	virtual void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;
	//////////////////////////////////////////////////////////////////////////

	const string& GetGeometryFile() const { return mv_geometry; }
	void          SetGeometryFile(const string& filename);

protected:
	void OnGeometryFileChange(IVariable* pVar);

	CVariable<string> mv_geometry;
};

/*!
 * Class Description of Entity
 */
class CGeomEntityClassDesc : public CObjectClassDesc
{
public:
	virtual ObjectType     GetObjectType() override { return OBJTYPE_ENTITY; };
	virtual const char*    ClassName() override { return "GeomEntity"; };
	virtual const char*    Category() override { return "Geom Entity"; };
	virtual CRuntimeClass* GetRuntimeClass() override { return RUNTIME_CLASS(CGeomEntity); };
	virtual const char*    GetFileSpec()  override { return "*.cgf;*.chr;*.cga;*.cdf"; };
	virtual const char*    GetDataFilesFilterString() override { return GetFileSpec(); }
	virtual bool           IsCreatable() const override { return CObjectClassDesc::IsEntityClassAvailable(); }
};


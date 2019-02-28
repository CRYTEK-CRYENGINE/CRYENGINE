// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Material/Material.h"

#define VEGETATION_ELEVATION_MIN "ElevationMin"
#define VEGETATION_ELEVATION_MAX "ElevationMax"
#define VEGETATION_SLOPE_MIN     "SlopeMin"
#define VEGETATION_SLOPE_MAX     "SlopeMax"

#include <CryMath/Random.h>

class CVegetationMap;
class CVegetationObject;
/** This is description of single static vegetation object instance.
 */
struct SANDBOX_API CVegetationInstance
{
	CVegetationInstance* next;  //! Next object instance
	CVegetationInstance* prev;  //! Prev object instance.

	Vec3                 pos;                    //!< Instance position.
	float                scale;                  //!< Instance scale factor.
	CVegetationObject*   object;                 //!< Object of this instance.
	IRenderNode*         pRenderNode;            // Render node of this object.
	IDecalRenderNode*    pRenderNodeGroundDecal; // Decal under vegetation.
	uint8                brightness;             //!< Brightness of this object.
	float                angle;                  //!< Instance rotation.
	float                angleX;                 //!< Angle X for rotation
	float                angleY;                 //!< Angle Y for rotation

	int                  m_refCount; //!< Number of references to this vegetation instance.

	bool                 m_boIsSelected;

	//! Add new refrence to this object.
	void AddRef() { m_refCount++; };

	//! Release refrence to this object.
	//! when reference count reaches zero, object is deleted.
	void Release()
	{
		if ((--m_refCount) <= 0)
			delete this;
	}
private:
	// Private destructor, to not be able to delete it explicitly.
	~CVegetationInstance() {};
};

//////////////////////////////////////////////////////////////////////////
/** Description of vegetation object which can be place on the map.
 */
class SANDBOX_API CVegetationObject : public CObject, public _i_reference_target_t
{
public:
	enum EGrowOn
	{
		eGrowOn_Terrain,
		eGrowOn_Brushes,
		eGrowOn_Both
	};

	CVegetationObject(int id);
	virtual ~CVegetationObject();

	void Serialize(XmlNodeRef& node, bool bLoading);

	// Member access
	const char* GetFileName() const { mv_fileName->Get(m_fileNameTmp); return m_fileNameTmp.GetString(); };
	void        SetFileName(const string& strFileName);

	//! Get this vegetation object GUID.
	CryGUID GetGUID() const { return m_guid; }

	//! Set current object group.
	void        SetGroup(const string& group);
	//! Get object's group.
	const char* GetGroup() const { return m_group; };

	//! Temporary unload IStatObj.
	void  UnloadObject();
	//! Load IStatObj again.
	void  LoadObject();

	float GetSize()               { return mv_size; };
	void  SetSize(float fSize)    { mv_size = fSize; };

	float GetSizeVar()            { return mv_sizevar; };
	void  SetSizeVar(float fSize) { mv_sizevar = fSize; };

	//! Get actual size of object.
	float GetObjectSize() const { return m_objectSize; }

	//////////////////////////////////////////////////////////////////////////
	// Operators.
	//////////////////////////////////////////////////////////////////////////
	void SetElevation(float min, float max) { mv_hmin = min; mv_hmax = max; };
	void SetSlope(float min, float max)     { mv_slope_min = min; mv_slope_max = max; };
	void SetDensity(float dens)             { mv_density = dens; };
	void SetSelected(bool bSelected);
	void SetHidden(bool bHidden);
	void SetNumInstances(int numInstances);
	void SetBending(float fBending) { mv_bending = fBending; }
	void SetFrozen(bool bFrozen)    { mv_layerFrozen = bFrozen; }
	void SetInUse(bool bNewState)   { m_bInUse = bNewState; };

	//////////////////////////////////////////////////////////////////////////
	// Accessors.
	//////////////////////////////////////////////////////////////////////////
	float GetElevationMin() const { return mv_hmin; };
	float GetElevationMax() const { return mv_hmax; };
	float GetSlopeMin() const     { return mv_slope_min; };
	float GetSlopeMax() const     { return mv_slope_max; };
	float GetDensity() const      { return mv_density; };
	bool  IsHidden() const;
	bool  IsSelected() const      { return m_bSelected; }
	int   GetNumInstances()       { return m_numInstances; };
	float GetBending()            { return mv_bending; };
	bool  GetFrozen() const       { return mv_layerFrozen; };
	bool  GetInUse()              { return m_bInUse; };
	bool  IsAutoMerged()          { return mv_autoMerged; }

	//////////////////////////////////////////////////////////////////////////
	void SetIndex(int i)  { m_index = i; };
	int  GetIndex() const { return m_index; };

	//! Set is object cast shadow.
	void SetCastShadow(bool on) { mv_castShadows = on; }
	//! Check if object casting shadow.
	bool IsCastShadow() const   { return mv_castShadows; };

	//! Set if object can be voxelized.
	void SetGIMode(bool on) { mv_giMode = on; }
	//! Check if object can be voxelized.
	bool GetGIMode() const   { return mv_giMode; };

	//////////////////////////////////////////////////////////////////////////
	void SetDynamicDistanceShadows(bool on) { mv_DynamicDistanceShadows = on; }
	bool HasDynamicDistanceShadows() const  { return mv_DynamicDistanceShadows; };

	//! Set to true if characters can hide behind this object.
	void SetHideable(bool on) { mv_hideable = on; }
	//! Check if characters can hide behind this object.
	bool IsHideable() const   { return mv_hideable; };

	//! if < 0 then AI calculates the navigation radius automatically
	//! if >= 0 then AI uses this radius
	float GetAIRadius() const                     { return mv_aiRadius; }
	void  SetAIRadius(float radius)               { mv_aiRadius = radius; }

	bool  IsRandomRotation() const                { return mv_rotation; }
	int   GetRotationRangeToTerrainNormal() const { return mv_rotationRangeToTerrainNormal; }
	bool  IsUseSprites() const                    { return mv_UseSprites; }
	bool  IsAlignToTerrain() const                { return mv_alignToTerrainCoefficient != 0.f; }
	bool  IsUseTerrainColor() const               { return mv_useTerrainColor; }
	bool  IsAffectedByBrushes() const             { return mv_growOn == eGrowOn_Brushes || mv_growOn == eGrowOn_Both; }
	bool  IsAffectedByTerrain() const             { return mv_growOn == eGrowOn_Terrain || mv_growOn == eGrowOn_Both; }

	//! Copy all parameters from specified vegetation object.
	void CopyFrom(const CVegetationObject& o);

	//! Return pointer to static object.
	IStatObj* GetObject() { return m_statObj; };

	//! Return true when the brush can paint on a location with the supplied parameters
	bool IsPlaceValid(float height, float slope) const;

	//! Calculate variable size for this object.
	float CalcVariableSize() const;

	//! Id of this object.
	int  GetId() const  { return m_id; };
	void SetId(int nId) { m_id = nId; SetEngineParams(); };

	void GetTerrainLayers(std::vector<string>& layers)       { layers = m_terrainLayers; };
	void SetTerrainLayers(const std::vector<string>& layers) { m_terrainLayers = layers; };
	bool IsUsedOnTerrainLayer(const string& layer);
	void UpdateTerrainLayerVariables();

	// Handle changing of the current configuration spec.
	void OnConfigSpecChange();

	// Return texture memory used by this object.
	int                GetTextureMemUsage(ICrySizer* pSizer = NULL);

	virtual CMaterial* GetMaterial() const { return m_pMaterial; }

	CVarObject* GetVarObject() const { return m_pVarObject.get(); }

protected:
	void SetEngineParams();
	void OnVarChange(IVariable* var);
	void OnTerrainLayerVarChange(IVariable* var);
	void OnMaterialChange(IVariable* var);
	void OnFileNameChange(IVariable* var);
	void UpdateMaterial();

	CVegetationObject(const CVegetationObject& o) {};

	CryGUID m_guid;

	//! Index of object in editor.
	int m_index;

	//! Objects group.
	string m_group;

	//! Used during generation ?
	bool m_bInUse;

	//! True if all instances of this object must be hidden.
	bool m_bHidden;

	//! True if Selected.
	bool m_bSelected;

	//! Real size of geometrical object.
	float m_objectSize;

	//! Number of instances of this vegetation object placed on the map.
	int m_numInstances;

	//! Current group Id of this object (Need not saving).
	int m_id;

	//! Pointer to object for this group.
	IStatObj* m_statObj;

	//! CGF custom material
	TSmartPtr<CMaterial> m_pMaterial;

	//! Ground decal material
	TSmartPtr<CMaterial> m_pMaterialGroundDecal;

	std::unique_ptr<CVarObject> m_pVarObject;

	// Place on terrain layers.
	std::vector<string> m_terrainLayers;

	bool                 m_bVarIgnoreChange;
	//////////////////////////////////////////////////////////////////////////
	// Variables.
	//////////////////////////////////////////////////////////////////////////
	CSmartVariable<float>   mv_density;
	CSmartVariable<float>   mv_hmin;
	CSmartVariable<float>   mv_hmax;
	CSmartVariable<float>   mv_slope_min;
	CSmartVariable<float>   mv_slope_max;
	CSmartVariable<float>   mv_size;
	CSmartVariable<float>   mv_sizevar;
	CSmartVariable<bool>    mv_castShadows; // Legacy, remains for backward compatibility
	CSmartVariableEnum<int> mv_castShadowMinSpec;
	CSmartVariable<bool>    mv_giMode;
	CSmartVariable<bool>    mv_instancing;
	CSmartVariable<bool>    mv_DynamicDistanceShadows;
	CSmartVariable<float>   mv_bending;
	CSmartVariableEnum<int> mv_hideable;
	CSmartVariableEnum<int> mv_playerHideable;
	CSmartVariable<bool>    mv_pickable;
	CSmartVariable<float>   mv_aiRadius;
	CSmartVariable<float>   mv_SpriteDistRatio;
	CSmartVariable<float>   mv_LodDistRatio;
	CSmartVariable<float>   mv_ShadowDistRatio;
	CSmartVariable<float>   mv_MaxViewDistRatio;
	CSmartVariable<string> mv_material;
	CSmartVariable<string> mv_materialGroundDecal;
	CSmartVariable<bool>    mv_UseSprites;
	CSmartVariable<bool>    mv_rotation;
	CSmartVariable<int>     mv_rotationRangeToTerrainNormal;
	CSmartVariable<bool>    mv_alignToTerrain; // Legacy, remains for backward compatibility
	CSmartVariable<float>   mv_alignToTerrainCoefficient;
	CSmartVariable<bool>    mv_useTerrainColor;
	CSmartVariable<bool>    mv_allowIndoor;
	CSmartVariable<bool>    mv_autoMerged;
	CSmartVariable<float>   mv_stiffness;
	CSmartVariable<float>   mv_damping;
	CSmartVariable<float>   mv_variance;
	CSmartVariable<float>   mv_airResistance;
	CSmartVariableEnum<int> mv_growOn;

	CSmartVariable<bool>    mv_layerFrozen;
	CSmartVariable<bool>    mv_layerWet;

	CSmartVariableEnum<int> mv_minSpec;

	CVariableArray          mv_layerVars;

	//! Filename of the associated CGF file
	CSmartVariable<string> mv_fileName;
	mutable string         m_fileNameTmp;

	friend class CVegetationPanel;
	friend class CVegetationMap;
};

//////////////////////////////////////////////////////////////////////////
inline bool CVegetationObject::IsPlaceValid(float height, float slope) const
{
	if (height < mv_hmin || height > mv_hmax)
		return false;
	if (slope < mv_slope_min || slope > mv_slope_max)
		return false;
	return true;
}

//////////////////////////////////////////////////////////////////////////
inline float CVegetationObject::CalcVariableSize() const
{
	if (mv_sizevar == 0)
	{
		return mv_size;
	}
	else
	{
		float fval = mv_sizevar * cry_random(-1.0f, 1.0f);
		if (fval >= 0)
		{
			return mv_size * (1.0f + fval);
		}
		else
		{
			return mv_size / (1.0f - fval);
		}
	}
}


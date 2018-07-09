// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __AICoverSurface_h__
#define __AICoverSurface_h__

#pragma once

#include "Objects/BaseObject.h"
#include "Objects/ObjectLoader.h"

#include <CryAISystem/ICoverSystem.h>

class CAICoverSurface
	: public CBaseObject
{
public:
	DECLARE_DYNCREATE(CAICoverSurface)

	enum EGeneratedResult
	{
		None = 0,
		Success,
		Error,
	};

	virtual bool          Init(CBaseObject* prev, const string& file);
	virtual bool          CreateGameObject();
	virtual void          Done();
	virtual void          InvalidateTM(int whyFlags);
	virtual void          SetSelected(bool bSelect);

	virtual void          Serialize(CObjectArchive& archive);
	virtual XmlNodeRef    Export(const string& levelPath, XmlNodeRef& xmlNode);

	virtual void          DeleteThis();
	virtual void          Display(CObjectRenderHelper& objRenderHelper);

	virtual float         GetCreationOffsetFromTerrain() const override { return 0.25f; }

	virtual bool          HitTest(HitContext& hitContext);
	virtual void          GetBoundBox(AABB& aabb);
	virtual void          GetLocalBounds(AABB& aabb);

	virtual void          SetHelperScale(float scale);
	virtual float         GetHelperScale();

	void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;

	float                 GetHelperSize() const;

	const CoverSurfaceID& GetSurfaceID() const;
	void                  SetSurfaceID(const CoverSurfaceID& coverSurfaceID);
	void                  Generate();
	void                  ValidateGenerated();

	template<typename T>
	void SerializeVar(CObjectArchive& archive, const char* name, CSmartVariable<T>& value)
	{
		if (archive.bLoading)
		{
			T saved;
			if (archive.node->getAttr(name, saved))
				value = saved;
		}
		else
			archive.node->setAttr(name, value);
	};

	template<typename T>
	void SerializeVarEnum(CObjectArchive& archive, const char* name, CSmartVariableEnum<T>& value)
	{
		if (archive.bLoading)
		{
			T saved;
			if (archive.node->getAttr(name, saved))
				value = saved;
		}
		else
			archive.node->setAttr(name, (T&)value);
	};

	template<>
	void SerializeVarEnum<string>(CObjectArchive& archive, const char* name, CSmartVariableEnum<string>& value)
	{
		if (archive.bLoading)
		{
			const char* saved;
			if (archive.node->getAttr(name, &saved))
				value = saved;
		}
		else
		{
			string current;
			value->Get(current);

			archive.node->setAttr(name, current.GetString());
		}
	};

	template<typename T>
	void SerializeValue(CObjectArchive& archive, const char* name, T& value)
	{
		if (archive.bLoading)
			archive.node->getAttr(name, value);
		else
			archive.node->setAttr(name, value);
	};

	template<>
	void SerializeValue(CObjectArchive& archive, const char* name, CoverSurfaceID& value)
	{
		uint32 id = value;
		if (archive.bLoading)
		{
			archive.node->getAttr(name, id);
			value = CoverSurfaceID(id);
		}
		else
			archive.node->setAttr(name, id);
	};

	template<>
	void SerializeValue<string>(CObjectArchive& archive, const char* name, string& value)
	{
		if (archive.bLoading)
		{
			const char* saved;
			if (archive.node->getAttr(name, &saved))
				value = saved;
		}
		else
			archive.node->setAttr(name, value.GetString());
	};

protected:
	CAICoverSurface();
	virtual ~CAICoverSurface();

	void                  CreateSampler();
	void                  ReleaseSampler();
	void                  StartSampling();

	void                  SetPropertyVarsFromParams(const ICoverSampler::Params& params);
	ICoverSampler::Params GetParamsFromPropertyVars();

	void                  CommitSurface();
	void                  ClearSurface();

	void                  DisplayBadCoverSurfaceObject(DisplayContext& disp);

	void                  OnPropertyVarChange(IVariable* var);

	void                  CreatePropertyVars();
	void                  ClonePropertyVars(CVarBlockPtr originalPropertyVars);
	_smart_ptr<CVarBlock> m_propertyVars;

	struct PropertyValues
	{
		CSmartVariableEnum<string> sampler;

		CSmartVariable<float>       limitLeft;
		CSmartVariable<float>       limitRight;
		CSmartVariable<float>       limitDepth;
		CSmartVariable<float>       limitHeight;

		CSmartVariable<float>       widthInterval;
		CSmartVariable<float>       heightInterval;

		CSmartVariable<float>       maxStartHeight;

		CSmartVariable<float>       simplifyThreshold;

		void                        Serialize(CAICoverSurface& object, CObjectArchive& archive)
		{
			object.SerializeVarEnum(archive, "Sampler", sampler);
			object.SerializeVar(archive, "LimitDepth", limitDepth);
			object.SerializeVar(archive, "LimitHeight", limitHeight);
			object.SerializeVar(archive, "LimitLeft", limitLeft);
			object.SerializeVar(archive, "LimitRight", limitRight);
			object.SerializeVar(archive, "SampleWidth", widthInterval);
			object.SerializeVar(archive, "SampleHeight", heightInterval);
			object.SerializeVar(archive, "MaxStartHeight", maxStartHeight);
			object.SerializeVar(archive, "SimplifyThreshold", simplifyThreshold);
		}

		// Needed because CSmartVariable copy constructor/assignment operator actually copies the pointer, and not the value
		PropertyValues& operator=(const PropertyValues& other)
		{
			sampler = *other.sampler;

			limitLeft = *other.limitLeft;
			limitRight = *other.limitRight;
			limitDepth = *other.limitDepth;
			limitHeight = *other.limitHeight;

			widthInterval = *other.widthInterval;
			heightInterval = *other.heightInterval;

			maxStartHeight = *other.maxStartHeight;

			simplifyThreshold = *other.simplifyThreshold;

			return *this;
		}
	}                m_propertyValues;

	AABB             m_aabb;
	AABB             m_aabbLocal;
	ICoverSampler*   m_sampler;
	CoverSurfaceID   m_surfaceID;
	float            m_helperScale;

	EGeneratedResult m_samplerResult;
};

class CAICoverSurfaceClassDesc : public CObjectClassDesc
{
public:
	ObjectType GetObjectType()
	{
		return OBJTYPE_AIPOINT;
	}

	const char* ClassName()
	{
		return "CoverSurface";
	}

	const char* Category()
	{
		return "AI";
	}

	CRuntimeClass* GetRuntimeClass()
	{
		return RUNTIME_CLASS(CAICoverSurface);
	}
};

#endif


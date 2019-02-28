// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryEntitySystem/IEntityComponent.h>

struct IAnimatedCharacter;

class CMannequinObject final : public IEntityComponent
{
private:

	class CProperties final : public IEntityPropertyGroup
	{
	public:

		CProperties(CMannequinObject& owner);

		string            modelFilePath;
		string            actionControllerFilePath;
		string            animationDatabaseFilePath;

		CMannequinObject& ownerBackReference;

	private:

		// IEntityPropertyGroup
		virtual const char* GetLabel() const override;
		virtual void        SerializeProperties(Serialization::IArchive& archive) override;
		// ~IEntityPropertyGroup
	};

public:

	CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS_GUID(CMannequinObject, "MannequinObjectComponent", "b8be725c-6065-4ec1-afdb-0be2819fbebb"_cry_guid);

	CMannequinObject();

private:

	// IEntityComponent
	virtual void                  Initialize() override;
	virtual void                  ProcessEvent(const SEntityEvent& event) override;
	virtual uint64                GetEventMask() const override;
	virtual IEntityPropertyGroup* GetPropertyGroup() override;
	virtual void                  GetMemoryUsage(ICrySizer* s) const override;
	// ~IEntityComponent

	void Reset();

private:

	CProperties         m_properties;
	IAnimatedCharacter* m_pAnimatedCharacter;
};

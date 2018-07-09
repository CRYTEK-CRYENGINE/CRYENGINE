// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CAIEntityComponent final : public IEntityComponent
{
public:
	CAIEntityComponent(const CWeakRef<CAIObject>& reference)
		: m_objectReference(reference) {}
	virtual ~CAIEntityComponent();

	// IEntityComponent
	virtual void Initialize() override;

	virtual void ProcessEvent(const SEntityEvent& event) override;
	virtual uint64 GetEventMask() const override;
	// ~IEntityComponent

	static void ReflectType(Schematyc::CTypeDesc<CAIEntityComponent>& desc)
	{
		desc.SetGUID("{435E4CAE-2A4D-453C-BAAB-F3006E329DA7}"_cry_guid);
		desc.SetComponentFlags({ IEntityComponent::EFlags::NoSave });
	}

	tAIObjectID GetAIObjectID() const { return m_objectReference.GetObjectID(); }

protected:
	CWeakRef<CAIObject> m_objectReference;
};
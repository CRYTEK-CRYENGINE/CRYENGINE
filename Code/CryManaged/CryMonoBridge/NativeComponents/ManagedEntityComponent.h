#pragma once

#include "EntityComponentFactory.h"

// Wrapper of an entity component to allow exposing managed equivalents to native code
class CManagedEntityComponent final : public IEntityComponent
{
public:
	CManagedEntityComponent(const CManagedEntityComponentFactory& factory);
	CManagedEntityComponent(CManagedEntityComponent&& other);

	virtual ~CManagedEntityComponent() {}

	// IEntityComponent
	virtual void PreInit(const SInitParams& params) override;
	virtual void Initialize() override;

	virtual	void ProcessEvent(const SEntityEvent &event) override;
	virtual uint64 GetEventMask() const override { return m_factory.m_eventMask; }
	// ~IEntityComponent

	std::shared_ptr<CMonoObject> GetObject() const { return m_pMonoObject; }
	const CManagedEntityComponentFactory& GetManagedFactory() const { return m_factory; }

	void SendSignal(int signalId, MonoInternals::MonoArray* pParams);
	uint16 GetPropertyCount() const { return m_numProperties; }

protected:
	const CManagedEntityComponentFactory& m_factory;
	std::shared_ptr<CMonoObject> m_pMonoObject;
	uint16 m_numProperties;
};

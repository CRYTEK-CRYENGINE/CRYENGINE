#pragma once

#include "EntityComponentFactory.h"

// Wrapper of an entity component to allow exposing managed equivalents to native code
class CManagedEntityComponent final : public IEntityComponent
{
public:
	CManagedEntityComponent(const CManagedEntityComponentFactory& factory);

	virtual ~CManagedEntityComponent() {}

	// IEntityComponent
	virtual void PreInit(const SInitParams& params) override;
	virtual void Initialize() override;

	virtual	void ProcessEvent(SEntityEvent &event) override;
	virtual uint64 GetEventMask() const override { return m_factory.m_eventMask; }
	// ~IEntityComponent

	CMonoObject* GetObject() const { return m_pMonoObject.get(); }
	const CManagedEntityComponentFactory& GetManagedFactory() const { return m_factory; }

	void SendSignal(int signalId, MonoInternals::MonoArray* pParams);

	struct SProperty
	{
		SProperty(uint16 i, size_t offset, MonoInternals::MonoTypeEnum type, EEntityPropertyType serType, std::shared_ptr<CMonoObject> pObject, bool bBelongsToComponent)
			: index(i)
			, offsetFromComponent(offset)
			, monoType(type)
			, serializationType(serType)
			, pTempObject(pObject)
			, bBelongsToComponentInstance(bBelongsToComponent) {}

		SProperty(const SProperty& other)
			: index(other.index)
			, offsetFromComponent(other.offsetFromComponent)
			, monoType(other.monoType)
			, serializationType(other.serializationType)
			, pTempObject(other.pTempObject != nullptr ? other.pTempObject->Clone() : nullptr)
			, bBelongsToComponentInstance(false) {}

		SProperty(SProperty&&) = delete;
		SProperty& operator=(SProperty&) = delete;
		SProperty& operator=(SProperty&&) = delete;

		uint16 index;
		size_t offsetFromComponent;
		MonoInternals::MonoTypeEnum monoType;
		EEntityPropertyType serializationType;

		// Temporary object containing the value of the property
		std::shared_ptr<CMonoObject> pTempObject;
		bool bBelongsToComponentInstance;
	};

protected:
	const CManagedEntityComponentFactory& m_factory;
	std::shared_ptr<CMonoObject> m_pMonoObject;
};

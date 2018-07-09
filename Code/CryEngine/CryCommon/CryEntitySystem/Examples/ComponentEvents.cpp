#include <CryEntitySystem/IEntitySystem.h>

class CMyEventComponent final : public IEntityComponent
{
public:
	// Provide a virtual destructor, ensuring correct destruction of IEntityComponent members
	virtual ~CMyEventComponent() = default;

	static void ReflectType(Schematyc::CTypeDesc<CMyEventComponent>& desc) { /* Reflect the component GUID in here. */}

	// Override the ProcessEvent function to receive the callback whenever an event specified in GetEventMask is triggered
	virtual void ProcessEvent(const SEntityEvent& event) override
	{
		// Check if this is the update event
		if (event.event == ENTITY_EVENT_UPDATE)
		{
			// The Update event provides delta time as the first floating-point parameter
			const float frameTime = event.fParam[0];

			/* Handle update logic here */
		}
	}

	// As an optimization, components have to specify a bitmask of the events that they want to handle
	// This is called once at component creation, and then only if IEntity::UpdateComponentEventMask is called, to support dynamic change of desired events (useful for disabling update when you don't need it)
	virtual uint64 GetEventMask() const override { return ENTITY_EVENT_BIT(ENTITY_EVENT_UPDATE); }
};
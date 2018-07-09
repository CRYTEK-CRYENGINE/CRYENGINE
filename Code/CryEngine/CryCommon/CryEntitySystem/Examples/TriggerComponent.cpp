#include <CryEntitySystem/IEntitySystem.h>

// Example of a component that receives enter and leave events from a virtual box positioned on the entity
class CMyTriggerComponent : public IEntityComponent
{
public:
	virtual ~CMyTriggerComponent() = default;
	static void ReflectType(Schematyc::CTypeDesc<CMyTriggerComponent>& desc) { /* Reflect the component GUID in here. */ }

	virtual void Initialize() override
	{
		// Create a new IEntityTriggerComponent instance, responsible for registering our entity in the proximity grid
		IEntityTriggerComponent* pTriggerComponent = m_pEntity->CreateComponent<IEntityTriggerComponent>();

		// Listen to area events in a 2m^3 box around the entity
		const Vec3 triggerBoxSize = Vec3(2, 2, 2);

		// Create an axis aligned bounding box, ensuring that we listen to events around the entity translation
		const AABB triggerBounds = AABB(triggerBoxSize * -0.5f, triggerBoxSize * 0.5f);
		// Now set the trigger bounds on the trigger component
		pTriggerComponent->SetTriggerBounds(triggerBounds);
	}

	virtual void ProcessEvent(const SEntityEvent& event) override
	{
		if (event.event == ENTITY_EVENT_ENTERAREA)
		{
			// Get the entity identifier of the entity that just entered our shape
			const EntityId enteredEntityId = static_cast<EntityId>(event.nParam[0]);
		}
		else if (event.event == ENTITY_EVENT_LEAVEAREA)
		{
			// Get the entity identifier of the entity that just left our shape
			const EntityId leftEntityId = static_cast<EntityId>(event.nParam[0]);
		}
	}

	virtual uint64 GetEventMask() const override
	{
		// Listen to the enter and leave events, in order to receive callbacks above when entities enter our trigger box
		return ENTITY_EVENT_BIT(ENTITY_EVENT_ENTERAREA) | ENTITY_EVENT_BIT(ENTITY_EVENT_LEAVEAREA);
	}
};
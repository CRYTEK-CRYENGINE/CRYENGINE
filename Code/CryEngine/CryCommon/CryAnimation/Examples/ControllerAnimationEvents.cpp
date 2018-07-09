#include <CryAnimation/ICryMannequin.h>
#include <CryEntitySystem/IEntitySystem.h>

#include <CryEntitySystem/IEntitySystem.h>

class CAnimationEventsComponent final : public IEntityComponent
{
public:
	static void ReflectType(Schematyc::CTypeDesc<CAnimationEventsComponent>& desc) { /* Reflect the component GUID in here. */ }

	// Override the ProcessEvent function to receive the callback whenever an animation event is triggered by any character
	virtual void ProcessEvent(const SEntityEvent& event) override
	{
		// Check if this is the animation event
		if (event.event == ENTITY_EVENT_ANIM_EVENT)
		{
			// Retrieve the animation event info from the first parameter
			const AnimEventInstance& animEvent = reinterpret_cast<const AnimEventInstance&>(event.nParam[0]);
			// Retrieve the character that this even occurred on
			ICharacterInstance* pCharacter = reinterpret_cast<ICharacterInstance*>(event.nParam[1]);
			// Now forward the event to the action controller
			m_pActionController->OnAnimationEvent(pCharacter, animEvent);
		}
	}

	// Subscribe to animation events
	virtual uint64 GetEventMask() const override { return ENTITY_EVENT_BIT(ENTITY_EVENT_ANIM_EVENT); }

protected:
	// The action controller we want to forward events to
	IActionController* m_pActionController;
};
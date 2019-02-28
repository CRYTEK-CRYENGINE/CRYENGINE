#pragma once

////////////////////////////////////////////////////////
// Base entity for native C++ entities that require the use of Editor properties
////////////////////////////////////////////////////////
template<typename T = IEntityComponent>
class CDesignerEntityComponent
	: public T
{
public:
	virtual ~CDesignerEntityComponent() {}

	// IEntityComponent
	virtual void Initialize() override { OnResetState(); }

	virtual void ProcessEvent(const SEntityEvent& event) override
	{
		switch (event.event)
		{
			// Physicalize on level start for Launcher
			case ENTITY_EVENT_START_LEVEL:
				// Editor specific, physicalize on reset, property change or transform change
			case ENTITY_EVENT_RESET:
			case ENTITY_EVENT_EDITOR_PROPERTY_CHANGED:
			case ENTITY_EVENT_XFORM_FINISHED_EDITOR:
				OnResetState();
				break;
		}
	}

	virtual uint64 GetEventMask() const override { return ENTITY_EVENT_BIT(ENTITY_EVENT_START_LEVEL) | ENTITY_EVENT_BIT(ENTITY_EVENT_RESET) | ENTITY_EVENT_BIT(ENTITY_EVENT_EDITOR_PROPERTY_CHANGED) | ENTITY_EVENT_BIT(ENTITY_EVENT_XFORM_FINISHED_EDITOR); }
	// ~IEntityComponent

	void ActivateFlowNodeOutput(const int portIndex, const TFlowInputData& inputData)
	{
		SEntityEvent evnt;
		evnt.event = ENTITY_EVENT_ACTIVATE_FLOW_NODE_OUTPUT;
		evnt.nParam[0] = portIndex;

		evnt.nParam[1] = (INT_PTR)&inputData;
		T::GetEntity()->SendEvent(evnt);
	}

	// Helper for loading geometry or characters
	void LoadMesh(int slot, const char *path)
	{
		if (IsCharacterFile(path))
		{
			T::GetEntity()->LoadCharacter(slot, path);
		}
		else
		{
			T::GetEntity()->LoadGeometry(slot, path);
		}
	}

	// Called when the entity needs to have its model, physics etc reset.
	// Usually on level load or when properties are changed.
	virtual void OnResetState() {}
};

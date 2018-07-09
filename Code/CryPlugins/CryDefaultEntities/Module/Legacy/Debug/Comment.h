#pragma once

#include "Legacy/Helpers/DesignerEntityComponent.h"

#include <CrySerialization/STL.h>

class CCommentEntity final 
	: public CDesignerEntityComponent<>
	, public IEntityPropertyGroup
{
	CRY_ENTITY_COMPONENT_INTERFACE_AND_CLASS_GUID(CCommentEntity, "Comment", "979e412f-68b9-4866-b61e-eb87cfa757cf"_cry_guid);

public:
	virtual ~CCommentEntity() {}

	// ISimpleExtension
	virtual void Initialize() override;

	virtual void ProcessEvent(const SEntityEvent& event) override;
	virtual uint64 GetEventMask() const override { return ENTITY_EVENT_BIT(ENTITY_EVENT_UPDATE); }

	virtual IEntityPropertyGroup* GetPropertyGroup() final { return this; }
	// ~ISimpleExtension

	// IEntityPropertyGroup
	virtual const char* GetLabel() const override { return "Comment Properties"; }

	virtual void SerializeProperties(Serialization::IArchive& archive) override
	{
		archive(m_text, "Text", "Text");
		archive.doc("Text to draw to screen");

		archive(m_maxDistance, "ViewDistance", "ViewDistance");
		archive.doc("Maximum entity distance from player");

		archive(m_fontSize, "FontSize", "FontSize");
		archive.doc("Maximum entity distance from player");

		if (archive.isInput())
		{
			OnResetState();
		}
	}
	// ~IEntityPropertyGroup

protected:
	ICVar* m_pEnableCommentsVar;

	float m_maxDistance = 100.f;
	float m_fontSize = 1.2f;

	string m_text;
};

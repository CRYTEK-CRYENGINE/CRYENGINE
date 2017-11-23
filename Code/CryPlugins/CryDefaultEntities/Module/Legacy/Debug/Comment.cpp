#include "StdAfx.h"
#include "Comment.h"

#include <CryRenderer/IRenderAuxGeom.h>

class CCommentEntityRegistrator
	: public IEntityRegistrator
{
	virtual void Register() override
	{
		if (gEnv->pEntitySystem->GetClassRegistry()->FindClass("Comment") != nullptr)
		{
			// Skip registration of default engine class if the game has overridden it
			CryLog("Skipping registration of default engine entity class Comment, overridden by game");
			return;
		}

		auto* pEntityClass = RegisterEntityWithDefaultComponent<CCommentEntity>("Comment", "Debug", "Comment.bmp");
		pEntityClass->SetFlags(pEntityClass->GetFlags() | ECLF_INVISIBLE);
	}
};

CCommentEntityRegistrator g_commentEntityRegistrator;

CRYREGISTER_CLASS(CCommentEntity);

void CCommentEntity::Initialize()
{
	CDesignerEntityComponent::Initialize();

	m_pEnableCommentsVar = gEnv->pConsole->GetCVar("cl_comment");
}

void CCommentEntity::ProcessEvent(const SEntityEvent& event)
{
	if (event.event == ENTITY_EVENT_UPDATE)
	{
		if (m_pEnableCommentsVar->GetIVal() == 0)
			return;

		float cameraDistance = (GetEntity()->GetWorldPos() - gEnv->pSystem->GetViewCamera().GetPosition()).GetLength();

		float alpha;

		if (m_maxDistance >= 255.f)
		{
			alpha = 1.f;
		}
		else if (cameraDistance < m_maxDistance)
		{
			alpha = cameraDistance / m_maxDistance;
			alpha = 1.f - alpha * alpha;
		}
		else
		{
			alpha = 0.f;
		}

		if (alpha == 0.f)
			return;

		float color[4] = { 1, 1, 1, alpha };

		IRenderAuxText::DrawLabelEx(GetEntity()->GetWorldPos(), m_fontSize, color, true, true, m_text);
	}
}

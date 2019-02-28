// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PersistantDebug.h"
#include "CryAction.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include <IUIDraw.h>

const char* CPersistantDebug::entityTagsContext = "PersistantDebugEntities";
const float CPersistantDebug::kUnlimitedTime = -1.0f;

void CPersistantDebug::AddEntityTag(const SEntityTagParams& params, const char* tagContext)
{
	// Create tag
	SEntityTag tag;
	tag.params = params;
	tag.params.column = max(1, tag.params.column);
	tag.params.fadeTime = max(0.f, tag.params.fadeTime);
	if (tagContext != NULL && *tagContext != '\0')
		tag.params.tagContext = tagContext;
	tag.totalTime = tag.params.visibleTime < 0 ? kUnlimitedTime : tag.params.visibleTime + tag.params.fadeTime;
	tag.totalFadeTime = tag.params.fadeTime;
	tag.vScreenPos.zero();

	SObj* obj = FindObj(params.entity);
	if (!obj)
	{
		// Create new object to push back
		SObj sobj;
		sobj.obj = eOT_EntityTag;
		sobj.entityId = params.entity;
		sobj.entityHeight = 0.f;
		sobj.timeRemaining = tag.totalTime;
		sobj.totalTime = tag.totalTime;
		sobj.columns.resize(params.column);
		AddToTagList(sobj.tags, tag);

		m_objects[entityTagsContext].push_back(sobj);
	}
	else
	{
		obj->timeRemaining = tag.totalTime < 0 ? kUnlimitedTime : max(obj->timeRemaining, tag.totalTime);
		obj->totalTime = obj->timeRemaining;
		int size = max(int(obj->columns.size()), params.column);
		if (obj->columns.size() < size)
			obj->columns.resize(size);

		AddToTagList(obj->tags, tag);
	}

	if (m_pETLog->GetIVal() > 0)
	{
		IEntity* ent = gEnv->pEntitySystem->GetEntity(params.entity);
		if (ent)
		{
			CryLog("[Entity Tag] %s added tag: %s", ent->GetName(), params.text.c_str());

			if (m_pETLog->GetIVal() > 1)
			{
				char text[256];
				cry_sprintf(text, "[Entity Tag] %s", params.text.c_str());
				gEnv->pAISystem->Record(ent->GetAI(), IAIRecordable::E_NONE, text);
			}
		}
	}
}

void CPersistantDebug::ClearEntityTags(EntityId entityId)
{
	SObj* obj = FindObj(entityId);
	if (obj)
		obj->tags.clear();
}

void CPersistantDebug::ClearStaticTag(EntityId entityId, const char* staticId)
{
	SObj* obj = FindObj(entityId);
	if (obj && staticId)
	{
		for (TListTag::iterator iterList = obj->tags.begin(); iterList != obj->tags.end(); ++iterList)
		{
			if (iterList->params.staticId.compare(staticId) == 0)
			{
				obj->tags.erase(iterList);
				break;
			}
		}
	}
}

void CPersistantDebug::ClearTagContext(const char* tagContext)
{
	if (!tagContext)
		return;

	MapListObj::iterator it = m_objects.find(entityTagsContext);
	if (it != m_objects.end())
	{
		for (ListObj::iterator iterList = it->second.begin(); iterList != it->second.end(); ++iterList)
			ClearTagContext(tagContext, iterList->entityId);
	}
}

void CPersistantDebug::ClearTagContext(const char* tagContext, EntityId entityId)
{
	if (!tagContext)
		return;

	SObj* obj = FindObj(entityId);
	if (obj)
	{
		for (TListTag::iterator iterList = obj->tags.begin(); iterList != obj->tags.end(); )
		{
			if (iterList->params.tagContext.compare(tagContext) != 0)
			{
				++iterList;
			}
			else
			{
				TListTag::iterator nextIter = obj->tags.erase(iterList);
				iterList = nextIter;
			}
		}
	}
}

void CPersistantDebug::UpdateTags(float frameTime, SObj& obj, bool doFirstPass)
{
	if (!doFirstPass)
	{
		// Every update calls itself recursively first to calculate the widths of each column
		// for multicolumn mode. Not the prettiest thing, but it's the best idea I've got so far!
		UpdateTags(frameTime, obj, true);
		frameTime = 0.f;

		for (int i = 0; i < obj.columns.size(); ++i)
			obj.columns[i].height = 0.f;
	}

	IFFont* pFont = gEnv->pCryFont->GetFont("default");
	assert(pFont);
	STextDrawContext ctx;
	ctx.SetSizeIn800x600(false);
	ctx.SetProportional(true);

	for (TListTag::iterator iterList = obj.tags.begin(); iterList != obj.tags.end(); ++iterList)
	{
		if (iterList->vScreenPos.IsZero())
			continue;

		float tagMaxDist = iterList->params.viewDistance;
		float fontSize = iterList->params.size * m_pETFontSizeMultiplier->GetFVal();

		// Calculate size of text on screen (so we can place it properly)
		ctx.SetSize(Vec2(12 * fontSize, 12 * fontSize));
		Vec2 textBoxSize = pFont->GetTextSize(iterList->params.text.c_str(), true, ctx);

		if (doFirstPass)
		{
			int pos(iterList->params.column - 1);
			obj.columns[pos].width = max(obj.columns[pos].width, textBoxSize.x + 15.f);
		}
		else
		{
			// Determine position
			SColumn& column = obj.columns[iterList->params.column - 1];
			Vec3 screenPos(iterList->vScreenPos);
			screenPos.x = screenPos.x * 0.01f * gEnv->pRenderer->GetOverlayWidth();
			screenPos.y = screenPos.y * 0.01f * gEnv->pRenderer->GetOverlayHeight() - textBoxSize.y - column.height;
			column.height += textBoxSize.y;

			// Adjust X value for multi-columns
			if (obj.columns.size() > 1)
			{
				int centerLine = obj.columns.size() / 2;
				if (iterList->params.column <= centerLine)
					for (int i = iterList->params.column - 1; i < centerLine; ++i)
						screenPos.x -= obj.columns[i].width;
				else
					for (int i = centerLine; i < iterList->params.column - 1; ++i)
						screenPos.x += obj.columns[i].width;

				if (obj.columns.size() % 2 != 0)
					screenPos.x -= textBoxSize.x * 0.5f;
			}
			else
			{
				screenPos.x -= textBoxSize.x * 0.5f;
			}

			// Determine color
			ColorF clr = iterList->params.color;
			if (1 == m_pETColorOverrideEnable->GetIVal())
			{
				clr.r = max(0.f, min(1.f, m_pETColorOverrideR->GetFVal()));
				clr.g = max(0.f, min(1.f, m_pETColorOverrideG->GetFVal()));
				clr.b = max(0.f, min(1.f, m_pETColorOverrideB->GetFVal()));
			}
			clr.a *= iterList->params.fadeTime / iterList->totalFadeTime; // Apply fade out time to alpha

			float clrAry[4] = { clr.r, clr.g, clr.b, clr.a };
			IRenderAuxText::Draw2dLabel(screenPos.x, screenPos.y, fontSize, clrAry, true, "%s", iterList->params.text.c_str());
		}
	}
}

void CPersistantDebug::PostUpdateTags(float frameTime, SObj& obj)
{
	Vec3 baseCenterPos;
	float heightAboveBase(0.f);
	if (!GetEntityParams(obj.entityId, baseCenterPos, heightAboveBase))
		return;

	// Check if entity is outside of global distance maximum or behind camera
	const CCamera& cam = GetISystem()->GetViewCamera();
	float distFromCam = (cam.GetPosition() - baseCenterPos).GetLength();
	float maxDist = m_pETMaxDisplayDistance->GetFVal();
	bool isOutOfRange(maxDist >= 0.f && distFromCam > maxDist);
	bool isBehindCamera(cam.GetViewdir().Dot(baseCenterPos - cam.GetPosition()) <= 0);

	heightAboveBase = max(obj.entityHeight, heightAboveBase); // never let stored entity height get smaller
	obj.entityHeight = heightAboveBase;                       // update stored position
	heightAboveBase += 0.2f;                                  // make tags start a little above the entity

	std::vector<TListTag::iterator> toClear;
	for (TListTag::iterator iterList = obj.tags.begin(); iterList != obj.tags.end(); ++iterList)
	{
		iterList->vScreenPos.zero();

		if (iterList->params.visibleTime != kUnlimitedTime)
		{
			if (iterList->params.visibleTime > 0.0f)
			{
				iterList->params.visibleTime -= frameTime;
				if (iterList->params.visibleTime < 0.0f)
				{
					// If visibleTime has hit 0, make sure any spillover gets applied to fade time
					iterList->params.fadeTime += iterList->params.visibleTime;
				}
			}
			else
			{
				iterList->params.fadeTime -= frameTime;
			}
		}

		if (iterList->params.fadeTime < 0.0f)
		{
			toClear.push_back(iterList);
		}
		else
		{
			if (isOutOfRange || isBehindCamera || 1 == m_pETHideAll->GetIVal() ||
			    (1 == m_pETHideBehaviour->GetIVal() && iterList->params.tagContext.compareNoCase("behaviour") == 0) ||
			    (1 == m_pETHideReadability->GetIVal() && iterList->params.tagContext.compareNoCase("readability") == 0) ||
			    (1 == m_pETHideAIDebug->GetIVal() && iterList->params.tagContext.compareNoCase("aidebug") == 0) ||
			    (1 == m_pETHideFlowgraph->GetIVal() && iterList->params.tagContext.compareNoCase("flowgraph") == 0) ||
			    (1 == m_pETHideScriptBind->GetIVal() && iterList->params.tagContext.compareNoCase("scriptbind") == 0))
				continue;

			// Check if entity is outside of max distance for this tag
			float tagMaxDist = iterList->params.viewDistance;
			if (tagMaxDist >= 0.f && distFromCam > tagMaxDist)
				continue;

			float distanceFix = distFromCam * 0.015f; // this constant found through trial and error
			float riseAmount(0.0f);
			if (iterList->params.staticId == "")
				riseAmount = 2.0f * distanceFix * (1 - (obj.timeRemaining / obj.totalTime));

			Vec3 tagPos(baseCenterPos.x, baseCenterPos.y, baseCenterPos.z + heightAboveBase + riseAmount);
			Vec3 screenPos(ZERO);
			gEnv->pRenderer->ProjectToScreen(tagPos.x, tagPos.y, tagPos.z, &screenPos.x, &screenPos.y, &screenPos.z);

			iterList->vScreenPos = screenPos;
		}
	}
	while (!toClear.empty())
	{
		obj.tags.erase(toClear.back());
		toClear.pop_back();
	}
}

CPersistantDebug::SObj* CPersistantDebug::FindObj(EntityId entity)
{
	MapListObj::iterator it = m_objects.find(entityTagsContext);
	if (it != m_objects.end())
	{
		ListObj::iterator it2 = std::find_if(it->second.begin(), it->second.end(), SObjFinder(entity));
		if (it2 != it->second.end())
			return &*it2;
	}

	return NULL;
}

void CPersistantDebug::AddToTagList(TListTag& tagList, SEntityTag& tag)
{
	if (tag.params.staticId != "")
	{
		// Special handling for static tags

		TListTag::iterator iter = tagList.begin();
		while (iter != tagList.end() &&
		       iter->params.staticId != "" &&
		       iter->params.staticId != tag.params.staticId)
			++iter;

		if (iter != tagList.end() && iter->params.staticId == tag.params.staticId)
		{
			// Replace this tag with new static tag
			*iter = tag;
		}
		else
		{
			// Insert as last static tag on list
			tagList.insert(iter, tag);
		}
	}
	else
	{
		TListTag::iterator iter = tagList.begin();
		while (iter != tagList.end() && iter->params.staticId != "")
			//(iter->totalTime > tag.totalTime || iter->params.staticId != ""))
			++iter;

		tagList.insert(iter, tag);
	}
}

bool CPersistantDebug::GetEntityParams(EntityId entityId, Vec3& baseCenterPos, float& height)
{
	IEntity* ent = gEnv->pEntitySystem->GetEntity(entityId);
	if (ent)
	{
		AABB bounds;
		ent->GetWorldBounds(bounds);
		baseCenterPos = bounds.GetCenter();
		baseCenterPos.z = bounds.min.z;
		height = bounds.GetSize().z;
		return true;
	}

	return false;
}

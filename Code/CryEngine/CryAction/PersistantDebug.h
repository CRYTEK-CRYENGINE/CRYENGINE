// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PERSISTANTDEBUG_H__
#define __PERSISTANTDEBUG_H__

#pragma once

#include <list>
#include <map>

#include <CryGame/IGameFramework.h>

class CPersistantDebug : public IPersistantDebug, public ILogCallback
{
public:
	void Begin(const char* name, bool clear);
	void AddSphere(const Vec3& pos, float radius, ColorF clr, float timeout);
	void AddDirection(const Vec3& pos, float radius, const Vec3& dir, ColorF clr, float timeout);
	void AddLine(const Vec3& pos1, const Vec3& pos2, ColorF clr, float timeout);
	void AddPlanarDisc(const Vec3& pos, float innerRadius, float outerRadius, ColorF clr, float timeout);
	void AddCone(const Vec3& pos, const Vec3& dir, float baseRadius, float height, ColorF clr, float timeout);
	void AddCylinder(const Vec3& pos, const Vec3& dir, float radius, float height, ColorF clr, float timeout);
	void Add2DText(const char* text, float size, ColorF clr, float timeout);
	void Add2DLine(float x1, float y1, float x2, float y2, ColorF clr, float timeout);
	void AddText(float x, float y, float size, ColorF clr, float timeout, const char* fmt, ...);
	void AddText3D(const Vec3& pos, float size, ColorF clr, float timeout, const char* fmt, ...);
	void AddQuat(const Vec3& pos, const Quat& q, float r, ColorF clr, float timeout);
	void AddAABB(const Vec3& min, const Vec3& max, ColorF clr, float timeout);

	void AddEntityTag(const SEntityTagParams& params, const char* tagContext = "");
	void ClearEntityTags(EntityId entityId);
	void ClearStaticTag(EntityId entityId, const char* staticId);
	void ClearTagContext(const char* tagContext);
	void ClearTagContext(const char* tagContext, EntityId entityId);

	bool Init();
	void Update(float frameTime);
	void PostUpdate(float frameTime);
	void Reset();

	CPersistantDebug();

	//ILogCallback
	virtual void OnWriteToConsole(const char* sText, bool bNewLine);
	virtual void OnWriteToFile(const char* sText, bool bNewLine) {}
	//~ILogCallback

private:

	IFFont* m_pDefaultFont;

	enum EObjType
	{
		eOT_Sphere,
		eOT_Arrow,
		eOT_Line,
		eOT_Disc,
		eOT_Text2D,
		eOT_Text3D,
		eOT_Line2D,
		eOT_Quat,
		eOT_EntityTag,
		eOT_Cone,
		eOT_Cylinder,
		eOT_AABB
	};

	struct SEntityTag
	{
		SEntityTagParams params;
		Vec3             vScreenPos;
		float            totalTime;
		float            totalFadeTime;
	};
	typedef std::list<SEntityTag> TListTag;

	struct SColumn
	{
		float width;
		float height;
	};
	typedef std::vector<SColumn> TColumns;

	struct SObj
	{
		EObjType obj;
		ColorF   clr;
		float    timeRemaining;
		float    totalTime;
		float    radius;
		float    radius2;
		Vec3     pos;
		Vec3     dir;
		Quat     q;
		string   text;

		EntityId entityId;
		float    entityHeight;
		TColumns columns;
		TListTag tags;
	};

	struct STextObj2D
	{
		string text;
		ColorF clr;
		float  size;
		float  timeRemaining;
		float  totalTime;
	};

	struct SObjFinder
	{
		SObjFinder(EntityId entityId) : entityId(entityId) {}
		bool operator()(const SObj& obj) { return obj.obj == eOT_EntityTag && obj.entityId == entityId; }
		EntityId entityId;
	};

	void  UpdateTags(float frameTime, SObj& obj, bool doFirstPass = false);
	void  PostUpdateTags(float frameTime, SObj& obj);
	void  AddToTagList(TListTag& tagList, SEntityTag& tag);
	SObj* FindObj(EntityId entityId);
	bool  GetEntityParams(EntityId entityId, Vec3& baseCenterPos, float& height);

	typedef std::list<SObj>           ListObj;
	typedef std::map<string, ListObj> MapListObj;
	typedef std::list<STextObj2D>     ListObjText2D; // 2D objects need a separate pass, so we put it into another list

	static const char*   entityTagsContext;
	static const float   kUnlimitedTime;

	MapListObj           m_objects;
	MapListObj::iterator m_current;
	ListObjText2D        m_2DTexts;

	// Pointers to Entity Tag console variables
	ICVar* m_pETLog;
	ICVar* m_pETHideAll;
	ICVar* m_pETHideBehaviour;
	ICVar* m_pETHideReadability;
	ICVar* m_pETHideAIDebug;
	ICVar* m_pETHideFlowgraph;
	ICVar* m_pETHideScriptBind;
	ICVar* m_pETFontSizeMultiplier;
	ICVar* m_pETMaxDisplayDistance;
	ICVar* m_pETColorOverrideEnable;
	ICVar* m_pETColorOverrideR;
	ICVar* m_pETColorOverrideG;
	ICVar* m_pETColorOverrideB;
};

#endif

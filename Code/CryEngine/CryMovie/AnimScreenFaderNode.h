// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ANIMSCREENFADERNODE_H__
#define __ANIMSCREENFADERNODE_H__

#pragma once

#include "AnimNode.h"

class CScreenFaderTrack;

class CAnimScreenFaderNode : public CAnimNode
{
public:
	CAnimScreenFaderNode(const int id);
	~CAnimScreenFaderNode();
	static void            Initialize();

	virtual EAnimNodeType  GetType() const override { return eAnimNodeType_ScreenFader; }
	virtual void           Animate(SAnimContext& animContext) override;
	virtual void           CreateDefaultTracks() override;
	virtual void           OnReset() override;
	virtual void           Activate(bool bActivate) override;
	virtual void           Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks) override;

	virtual unsigned int   GetParamCount() const override;
	virtual CAnimParamType GetParamType(unsigned int nIndex) const override;

	virtual void           Render() override;

	bool                   IsAnyTextureVisible() const;

protected:
	virtual bool GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const override;
	virtual bool NeedToRender() const override { return true; }

private:
	CAnimScreenFaderNode(const CAnimScreenFaderNode&);
	CAnimScreenFaderNode& operator=(const CAnimScreenFaderNode&);

	void Deactivate();

private:
	void PrecacheTexData();

	Vec4  m_startColor;
	bool  m_bActive;
	int   m_lastActivatedKey;
	bool  m_texPrecached;
};

#endif //__ANIMSCREENFADERNODE_H__

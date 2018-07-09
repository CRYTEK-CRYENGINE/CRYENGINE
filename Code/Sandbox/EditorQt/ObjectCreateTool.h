// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ObjectCreateTool_h__
#define __ObjectCreateTool_h__

#if _MSC_VER > 1000
	#pragma once
#endif

class CBaseObject;
class QMimeData;

//TODO : Replacement for CObjectCreateTool, work in progress
class CObjectCreateTool : public CEditTool
{
	DECLARE_DYNCREATE(CObjectCreateTool)

public:
	CObjectCreateTool();
	~CObjectCreateTool();

	virtual string GetDisplayName() const override { return "Create Object"; }
	virtual bool   MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags) override;
	virtual bool   OnDragEvent(CViewport* view, EDragEvent eventId, QEvent* event, uint32 flags) override;

	virtual bool   OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags) override;

	//Selects object type to create (and optional file to use)
	void              SelectObjectToCreate(const char* objectClassName, const char* filepath = nullptr);
	void              SelectObjectToCreate(CObjectClassDesc* objectType, const char* filepath = nullptr);

	CObjectClassDesc* GetSelectedObjectClass() { return m_objectClassDesc; }
	const char*       GetSelectedObjectFile()  { return m_objectFile; }

private:
	virtual void DeleteThis() override { delete this; };

	bool         CanStartCreation();
	void         StartCreation();
	void         FinishCreation(bool restart);
	void         CancelCreation(bool clear = true);
	void         RestartCreation();
	bool		 IsCreating() const { return m_createdObject != nullptr; }

	bool         IsValidDragData(const QMimeData* data, bool acceptValue);

	CObjectClassDesc* m_objectClassDesc;
	string            m_objectFile;

	CBaseObjectPtr    m_createdObject;

	bool              m_bStartedCreation;
	// set when creating composite objects where we don't want to cancel creation when the mouse leaves the viewport
	// after creation has started (nodes in a navigation area)
	bool m_bContinuousCreation;
};

#endif // __ObjectCreateTool_h__


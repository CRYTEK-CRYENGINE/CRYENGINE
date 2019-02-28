// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VehicleDialogComponent_h__
#define __VehicleDialogComponent_h__
#pragma once

struct IVariable;

// icon indexes
#define VEED_ASSET_HELPER_ICON 0
#define VEED_HELPER_ICON       2
#define VEED_PART_ICON         3
#define VEED_SEAT_ICON         4
#define VEED_VEHICLE_ICON      5
#define VEED_WHEEL_ICON        6
#define VEED_WEAPON_ICON       7
#define VEED_COMP_ICON         8
#define VEED_PARTICLE_ICON     9
#define VEED_OPEN_ICON         11
#define VEED_SAVE_ICON         12
#define VEED_NEW_ICON          13
#define VEED_MOD_ICON          14

struct STreeItem
{
	HTREEITEM item;
	HTREEITEM parent;
	STreeItem() : item(0), parent(0){}
	STreeItem(HTREEITEM hItem, HTREEITEM hParent){ item = hItem; parent = hParent; }
};

typedef std::map<CBaseObject*, STreeItem> TPartToTreeMap;

class CVehiclePrototype;

/*!
 * Interface for components the VehicleEditorDialog contains
 */
struct IVehicleDialogComponent
{
	virtual void UpdateVehiclePrototype(CVehiclePrototype* pProt) = 0;
	virtual void OnPaneClose() = 0;
	virtual void NotifyObjectsDeletion(CVehiclePrototype* pProt) {};
};

/** Logs to console if v_debugdraw set to DEBUGDRAW_VEED
 */
void VeedLog(const char* s, ...);

/** Interface for Veed objects
 */
struct IVeedObject
{
	virtual void        UpdateVarFromObject() = 0;
	virtual void        UpdateObjectFromVar() = 0;

	virtual const char* GetElementName() = 0;

	virtual IVariable*  GetVariable() = 0;
	virtual void        SetVariable(IVariable*) = 0;

	virtual bool        DeleteVar() = 0;
	virtual void        DeleteVar(bool del) = 0;

	virtual int         GetIconIndex() = 0;

	virtual void        UpdateScale(float scale) = 0;
	virtual void        OnTreeSelection() = 0;

	static IVeedObject* GetVeedObject(CBaseObject* pObj);
};

/** Base class for Veed objects
 */
class CVeedObject : public IVeedObject
{
public:
	CVeedObject()
		: m_pVar(0)
		, m_bDelVar(true)
		, m_ignoreOnTransformCallback(false)
		, m_ignoreObjectUpdateCallback(false)
	{}

	///////////////////////////////////////////////////////////////
	virtual void        UpdateVarFromObject()        {}
	virtual void        UpdateObjectFromVar()        {}

	virtual const char* GetElementName()             { return ""; }

	virtual IVariable*  GetVariable()                { return m_pVar; }
	virtual void        SetVariable(IVariable* pVar) { m_pVar = pVar; }

	virtual bool        DeleteVar()                  { return m_bDelVar; }
	virtual void        DeleteVar(bool del)          { m_bDelVar = del; }

	virtual int         GetIconIndex()               { return 0; }

	virtual void        UpdateScale(float scale)     {};
	virtual void        OnTreeSelection()            {}
	///////////////////////////////////////////////////////////////

protected:
	virtual void OnTransform() {}
	void         InitOnTransformCallback(CBaseObject* pObject);

	void         EnableUpdateObjectOnVarChange()  { m_ignoreObjectUpdateCallback = false; }
	void         DisableUpdateObjectOnVarChange() { m_ignoreObjectUpdateCallback = true; }

	void         UpdateObjectOnVarChangeCallback(IVariable* var);
	void         EnableUpdateObjectOnVarChange(const string& childVarName);
	void         DisableUpdateObjectOnVarChange(const string& childVarName);

private:
	void OnObjectEventCallback(CBaseObject* pObject, int eventId);

protected:
	IVariable* m_pVar;
	bool       m_bDelVar;

	bool       m_ignoreOnTransformCallback;
	bool       m_ignoreObjectUpdateCallback;
};

#endif // __VehicleDialogComponent_h__


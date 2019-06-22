#pragma once

#pragma include_alias("StdAfx.h", "DummyEditor.h")
#pragma include_alias("CryMath/Cry_Camera.h", "DummyEditor.h")
#pragma include_alias("LevelEditor/Tools/EditTool.h", "DummyEditor.h")
#pragma include_alias("Viewport.h", "DummyEditor.h")
#pragma include_alias("Objects/DisplayContext.h", "DummyEditor.h")
#pragma include_alias("Cry3DEngine/I3DEngine.h", "DummyEditor.h")
#pragma include_alias("CryEntitySystem/IEntity.h", "DummyEditor.h")
#pragma include_alias("CryAnimation/ICryAnimation.h", "DummyEditor.h")
#pragma include_alias("Objects/EntityObject.h", "DummyEditor.h")
#pragma include_alias("IEditorImpl.h", "DummyEditor.h")
#pragma include_alias("CrySystem/ConsoleRegistration.h", "DummyEditor.h")

#include "CryCore\Platform\platform.h"
#include "CryMath\Cry_Math.h"
struct IPhysicalWorld;
#include "CryPhysics\IPhysics.h"
#include "CrySystem\ISystem.h"
#include "Resource.h"

#define DECLARE_DYNAMIC(X)
#define IMPLEMENT_DYNAMIC(X,Y)
#define REGISTER_CVAR2(A,B,C,D,E)
#define _CRY_DEFAULT_MALLOC_ALIGNMENT 4
#define MK_ALT 0x20

inline struct {
	static HCURSOR LoadCursor(int id) { return ::LoadCursor(GetModuleHandle(0), MAKEINTRESOURCE(id)); }
} *AfxGetApp() { return nullptr; }

enum EMouseEvent { eMouseLDown, eMouseLUp, eMouseMove };

typedef Vec2i CPoint;
#undef ColorF
typedef Vec3 ColorF;

struct CCamera {
	float fovy;
	Matrix34 mtx;
	Vec2i size;

	const Matrix34& GetMatrix() const { return mtx; }
	Vec3 GetPosition() const { return mtx.GetTranslation(); }
	float GetFov() const { return fovy; }
	int GetViewSurfaceX() const { return size.x; }
	int GetViewSurfaceZ() const { return size.y; }
	const Matrix34& GetViewTM() const { return GetMatrix();	}
	float GetFOV() const { return GetFov(); }
	void GetDimensions(int *x,int *y) const { *x=size.x; *y=size.y; }
};

struct SDisplayContext {
	CCamera *camera;
	virtual void DrawLine(const Vec3& pt0, const Vec3& pt1, const ColorF& clr0, const ColorF& clr1) {}
	virtual void DrawBall(const Vec3& c, float r) {}
	virtual void DrawSolidBox(const Vec3& vmin, const Vec3& vmax) {}
	virtual void DrawArrow(const Vec3& start, const Vec3& end, float scale=1.0f) {}
	virtual void SetColor(COLORREF clr, float alpha=1) {}
	virtual void DrawTextLabel(const Vec3& pt, float fontSize, const char*, bool center) {}
};
struct CViewport : CCamera {};

class CEditTool {
public:
	virtual string GetDisplayName() const { return nullptr; }
	virtual void   Display(SDisplayContext& dc) {}
	virtual bool   OnSetCursor(CViewport* view) { return true; }
	virtual bool   MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags) { return true; }
};

struct ISkeletonPose {
	static void SynchronizeWithPhysicalEntity(IPhysicalEntity*) {}
	static IPhysicalEntity* GetCharacterPhysics() { return nullptr; }
};
struct ICharacterInstance {
	static ISkeletonPose* GetISkeletonPose() { return nullptr; }
};
struct IEntity {
	static ICharacterInstance *GetCharacter(int) { return nullptr; }
	static int GetSlotCount() { return 0; }
	static int GetId() { return 0; }
};
enum EEditorNotifyEvent { eNotify_OnBeginSimulationMode, eNotify_OnBeginGameMode, eNotify_OnEndUndoRedo };
struct IEditorNotifyListener {
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent ev) {}
};
struct QAction {
	static void setChecked(bool) {}
	static void setCheckable(bool) {}
	static bool isChecked() { return false; }
};
struct IEditorImpl {
	static void RegisterNotifyListener(IEditorNotifyListener*) {}
	static void UnregisterNotifyListener(IEditorNotifyListener*) {}
	static IEditorImpl *GetICommandManager() { return nullptr; }
	static QAction* GetAction(const char*) { return nullptr; }
};
inline IEditorImpl* GetIEditorImpl() { return nullptr; }
struct CEntityObject {
	static CEntityObject* FindFromEntityId(int) { return nullptr; }
	static void AcceptPhysicsState() {}
};
struct CUndo {
	CUndo(const char*) {}
};

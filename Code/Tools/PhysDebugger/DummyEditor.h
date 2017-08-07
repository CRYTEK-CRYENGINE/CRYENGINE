#pragma once

#pragma include_alias("StdAfx.h", "DummyEditor.h")
#pragma include_alias("EditTool.h", "DummyEditor.h")
#pragma include_alias("Viewport.h", "DummyEditor.h")
#pragma include_alias("Objects/DisplayContext.h", "DummyEditor.h")

#include "CryCore\Platform\platform.h"
#include "CryMath\Cry_Math.h"
struct IPhysicalWorld;
#include "CryPhysics\IPhysics.h"
#include "CrySystem\ISystem.h"
#include "Resource.h"

#define DECLARE_DYNAMIC(X)
#define IMPLEMENT_DYNAMIC(X,Y)
#define REGISTER_CVAR2(A,B,C,D,E)

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

struct DisplayContext {
	CCamera *camera;
	virtual void DrawLine(const Vec3& pt0, const Vec3& pt1, const ColorF& clr0, const ColorF& clr1) {}
	virtual void DrawBall(const Vec3& c, float r) {}
	virtual void SetColor(COLORREF clr, float alpha=1) {}
	virtual void DrawTextLabel(const Vec3& pt, float fontSize, const char*, bool center) {}
};
struct CViewport : CCamera {};

class CEditTool {
public:
	virtual string GetDisplayName() const { return nullptr; }
	virtual void   Display(DisplayContext& dc) {}
	virtual bool   OnSetCursor(CViewport* view) { return true; }
	virtual bool   MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags) { return true; }
};

struct ICharacterInstance {
	static ICharacterInstance* GetISkeletonPose() { return nullptr; }
	static IPhysicalEntity* GetCharacterPhysics() { return nullptr; }
};
struct IEntity {
	static ICharacterInstance *GetCharacter(int) { return nullptr; }
};
struct ISurfaceType {
	static int GetId() { return 0; }
};

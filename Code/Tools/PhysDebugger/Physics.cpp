#pragma warning (disable : 4244)
#include "CryCore\Platform\platform.h"
#include "CrySystem\ILog.h"
struct IPhysicalWorld;
#include "CryMath\Cry_Math.h"
#include "CrySystem\ISystem.h"
#define _LIB
#include "CryPhysics\IPhysics.h"
#include "DummyEditor.h"
#include "..\..\Sandbox\EditorQt\PhysTool.h"
#include "resource.h"

#include <GL\gl.h>
#include <GL\glu.h>

struct SRayRec {
	Vec3 origin;
	Vec3 dir;
	float time;
};
struct ColorB {
	ColorB() {}
	ColorB(unsigned char _r,unsigned char _g,unsigned char _b,unsigned char _a) { r=_r;g=_g;b=_b;a=_a; }
	unsigned char r,g,b,a;
};
struct STextRec {
	char *str;
	ColorB color;
	Vec3 pt;
	bool center;
};
char g_txtBuf[65536];
int g_txtBufPos = 0;

struct SEntStateHeader {
	SEntStateHeader *prev;
	const char *pname;
	int count;
	int size;
};

template<class T> struct Queue {
	T *buf = nullptr;
	int sz=0,first=0,last=-1,n=0;
	~Queue() { delete[] buf; }
	void init() { buf=new T[sz=64];	first=0; last=-1; n=0; }

	T& push_back() {
		if (n==sz) {
			T *prevbuf = buf; buf = new T[sz+=64];
			memcpy(buf+first, prevbuf+first, (n-first)*sizeof(T));
			if (first > last)	{
				int i = min(sz-n,last+1);
				memcpy(buf+n, prevbuf, i*sizeof(T));
				memcpy(buf, prevbuf+i, (last+1-i)*sizeof(T));
				last -= i - (n+i & (last-i)>>31);
			}
		}
		++n;
		return buf[last += 1-(sz & (sz-2-last)>>31)]; 
	}
	void iterate(std::function<bool(T&)> func) {
		if (n>0) {
			int i=first,iprev;
			do {
				bool done = func(buf[i]);
				iprev = i; i += 1-(sz & (sz-2-i)>>31);
				if (done)
					first=i,--n;
			} while(iprev!=last);
		}
	}
};

class CPhysRenderer : public IPhysRenderer {
public:
	CPhysRenderer();
	~CPhysRenderer();
	void Init();
	void DrawGeometry(IGeometry *pGeom, geom_world_data *pgwd, const ColorB &clr);
	void DrawText(const Vec3 &pt, const char *txt, ColorB color, bool center);
	void DrawGeomBuffers(float dt);
	void DrawTextBuffers(HDC hDC);

	virtual void DrawGeometry(IGeometry *pGeom, geom_world_data *pgwd, int idxColor=0, int bSlowFadein=0, const Vec3 &sweepDir=Vec3(0), const Vec4& clr=Vec4());
	virtual void DrawLine(const Vec3& pt0, const Vec3& pt1, int idxColor=0, int bSlowFadein=0);
	virtual void DrawText(const Vec3 &pt, const char *txt, int idxColor, float saturation=0) {
		uchar clr[4] = { min((int)(saturation*512),255), 0,0, 255u };
		clr[1] = (clr[idxColor+1] = min((int)((1-saturation)*512), 255))>>1;
		DrawText(pt,txt,*(ColorB*)clr,false);
	}
	virtual void DrawFrame(const Vec3& pnt, const Vec3* axes, const float scale, const Vec3* limits, const int axes_locked) {}
	virtual const char *GetForeignName(void *pForeignData,int iForeignData,int iForeignFlags) { 
		const char *name = "";
		switch (iForeignData) {
			case 100: name = (const char*)pForeignData; break;
			case 101: name = ((SEntStateHeader*)pForeignData)->pname;
		}
		return name;
	}
	virtual QuatT SetOffset(const Vec3& offs=Vec3(ZERO), const Quat& qrot=Quat(ZERO)) { return QuatT(IDENTITY);	}

	float m_wireframeDist;
	float m_timeRayFadein;
	int m_blockMask = 0;

protected:
	Queue<SRayRec> m_rayQ;
	Queue<STextRec> m_textQ;
	IGeometry *m_pRayGeom;
	primitives::ray *m_pRay;
	static ColorB g_colorTab[8];
};
CPhysRenderer g_PhysRenderer;

struct DisplayContextImp : DisplayContext {
	DisplayContextImp(CCamera *cam) { camera = cam; }
	virtual void DrawLine(const Vec3& pt0, const Vec3& pt1, const ColorF& clr0, const ColorF& clr1);
	virtual void DrawBall(const Vec3& c, float r);
	virtual void DrawTextLabel(const Vec3& pt, float fontSize, const char* txt, bool center) { g_PhysRenderer.DrawText(pt,txt,m_color,center); }
	virtual void SetColor(COLORREF clr, float alpha=1);
	ColorB m_color = { 255,255,255,255 };
};

class CPhysToolImp : public CPhysPullTool {
public:
	virtual ~CPhysToolImp() {}
};

IPhysicalWorld *g_pWorld;
ProfilerData *g_pPhysProfilerData;
Vec3 g_campos,g_camdir;
Vec3 g_camposLoc,g_camdirLoc;
CCamera g_Cam = { gf_PI/4 };
DisplayContextImp g_DC(&g_Cam);
CPhysToolImp g_Tool;
IPhysicalEntity *g_avatar = nullptr;
float g_viewdist = 100;
float g_movespeed = 0.2f;
HANDLE g_hThread, g_hThreadActive;
__int64 g_freq, g_profreq=1000000000;
inline float TicksToMilliseconds(__int64 ticks) { return ((ticks*1000000)/g_profreq)*0.001f; };
__int64 g_stepTime=0, g_moveTime=0, g_time0=0;
float g_dtPhys = 0.01f;
extern int g_bFast, g_bShowColl, g_bShowBBoxes, g_bShowProfiler, g_bAnimate, g_sync;
extern "C" CRYPHYSICS_API ProfilerData *GetProfileData(int iThread);

struct CSystemImp : ISystem {
	virtual IPhysRenderer *GetIPhysRenderer() const { return &g_PhysRenderer; }
	virtual float GetCurrTime() const { return time; }
	void UpdateTime() {
		__int64 curTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&curTime);
		time = (float)(((curTime-g_time0)*10000)/g_freq)*0.0001f;
	}
	float time = 0;
} g_System;
struct SEnvImp : SEnv {
	SEnvImp() { p3DEngine=pTimer=pSystem = &g_System; pThreadManager = nullptr; }
} env;
SEnv *gEnv = &env;

void ResetProfiler(ProfilerData *pd, int threads=-1)
{
	for(int i=0; i<=MAX_PHYS_THREADS; i++) if (threads & 1<<i) {
		pd[i].iLevel = 0;
		pd[i].sec0.m_iCurCode = 0;
		pd[i].sec0.m_iCurSlot = pd[i].iLastSampleSlot = pd[i].iLastTimeSample = 0;
		pd[i].pCurSection[0] = &pd[i].sec0;
		pd[i].TimeSamples[0].Reset();
	}
}

struct ProfilerVisInfo {
	unsigned __int64 iCode;
	float timeout;
	char bExpanded;
};
ProfilerVisInfo g_ProfVisInfos[1024];
int g_nProfVisInfos = 0;
unsigned __int64 g_iActiveCode = 0;

int getProfVisInfo(unsigned __int64 iCode, int bCreate) 
{
	for(int i=0; i<g_nProfVisInfos; i++) if (g_ProfVisInfos[i].iCode==iCode) {
		g_ProfVisInfos[i].timeout = 5.0f;
		return i;
	}
	if (!bCreate || g_nProfVisInfos==sizeof(g_ProfVisInfos)/sizeof(g_ProfVisInfos[0]))
		return 0;
	g_ProfVisInfos[g_nProfVisInfos].iCode = iCode;
	g_ProfVisInfos[g_nProfVisInfos].bExpanded = g_ProfVisInfos[0].bExpanded;
	g_ProfVisInfos[g_nProfVisInfos].timeout = 5.0f;
	return g_nProfVisInfos++;
}

int getProfParent(ProfilerData *pd, int iParent, int iSlot)
{
	int iChild;
	if (iParent<0)
		return -1;
	for(iChild=pd->TimeSamples[iParent].iChild; iChild>=0; iChild=pd->TimeSamples[iChild].iNext) if (iChild==iSlot)
		return iParent;
	for(iChild=pd->TimeSamples[iParent].iChild; iChild>=0; iChild=pd->TimeSamples[iChild].iNext) if ((iParent=getProfParent(pd,iChild,iSlot))>0)
		return iParent;
	return -1;
}

int getActiveSlot(ProfilerData *pd) 
{
	int iSlot;
	for(iSlot=pd->iLastTimeSample; iSlot>0 && pd->TimeSamples[iSlot].iCode!=g_iActiveCode; iSlot--);
	return iSlot;
}

void SelectNextProfVisInfo()
{
	ProfilerData *pd = g_pPhysProfilerData;
	int iSlot = getActiveSlot(g_pPhysProfilerData);
	if (!iSlot) { g_iActiveCode = pd->TimeSamples[1].iCode; return; }

	if (g_ProfVisInfos[getProfVisInfo(pd->TimeSamples[iSlot].iCode,0)].bExpanded && pd->TimeSamples[iSlot].iChild>=0)
			g_iActiveCode = pd->TimeSamples[pd->TimeSamples[iSlot].iChild].iCode;
	else {
		do {
			if (pd->TimeSamples[iSlot].iNext>=0) {
				g_iActiveCode = pd->TimeSamples[pd->TimeSamples[iSlot].iNext].iCode; return;
			} else
				iSlot = getProfParent(pd,0,iSlot);
		} while(iSlot);
		g_iActiveCode = 0;
	}
}

void SelectPrevProfVisInfo()
{
	ProfilerData *pd = g_pPhysProfilerData;
	int i,iSlot = getActiveSlot(g_pPhysProfilerData);
	if (!iSlot) { g_iActiveCode = pd->TimeSamples[1].iCode; return; }

	for(i=pd->iLastTimeSample; i>0 && pd->TimeSamples[i].iNext!=iSlot && pd->TimeSamples[i].iChild!=iSlot; i--);
	g_iActiveCode = pd->TimeSamples[i].iCode;

	if (pd->TimeSamples[i].iChild!=iSlot) 
		while (g_ProfVisInfos[getProfVisInfo(g_iActiveCode,0)].bExpanded && pd->TimeSamples[i].iChild>=0) {
			for(i=pd->TimeSamples[i].iChild; pd->TimeSamples[i].iNext>=0; i=pd->TimeSamples[i].iNext);
			g_iActiveCode = pd->TimeSamples[i].iCode;
		}
}

void ExpandProfVisInfo(int bExpand)
{
	g_ProfVisInfos[getProfVisInfo(g_iActiveCode,1)].bExpanded = bExpand;
}

DWORD WINAPI PhysProc(void *pParam)
{
	__int64 curTime; 

	while(1) {
		WaitForSingleObject(g_hThreadActive,INFINITE);
		QueryPerformanceCounter((LARGE_INTEGER*)&curTime);
		float dt = (float)(((curTime-g_stepTime)*10000)/g_freq)*0.0001f;
		if (dt<0 || dt>1)
			dt = 0.01f;
		if (dt<0.01f) {
			ReleaseMutex(g_hThreadActive);
			Sleep(0);	
			continue;
		}
		ResetProfiler(g_pPhysProfilerData, (1<<MAX_PHYS_THREADS)-1);
		g_pWorld->TimeStep(g_dtPhys = dt);
		g_stepTime = curTime;
		ReleaseMutex(g_hThreadActive);
	}

	return 0;
}

int OnCollision(const EventPhysCollision *epc)
{
	pe_params_part pp;
	pp.partid = epc->partid[1];
	if (epc->pEntity[0]->GetType()==PE_PARTICLE && epc->vloc[0].len2()>sqr(100.0f) && epc->pEntity[1]->GetParams(&pp) && pp.idmatBreakable>=0 && pp.flagsOR & geom_manually_breakable)
		g_pWorld->DeformPhysicalEntity(epc->pEntity[1], epc->pt, -epc->n, 1); 
	return 1;
}

void InitPhysics()
{
	gEnv->pPhysicalWorld = g_pWorld = CreatePhysicalWorld(&g_System);
	g_pPhysProfilerData = GetProfileData(0);
	QueryPerformanceFrequency((LARGE_INTEGER*)&g_freq);
	QueryPerformanceCounter((LARGE_INTEGER*)&g_time0);
	ResetProfiler(g_pPhysProfilerData);
	g_ProfVisInfos[0].iCode = 0;
	g_ProfVisInfos[0].timeout = 10.0f;
	g_ProfVisInfos[0].bExpanded = 0;

	HKEY hKey;
	DWORD dwSize = sizeof(g_profreq);
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0,KEY_QUERY_VALUE, &hKey)==ERROR_SUCCESS && 
			RegQueryValueEx(hKey, "~MHz", 0,0, (LPBYTE)&g_profreq, &dwSize)==ERROR_SUCCESS)
		g_profreq *= 1000000;

	g_pWorld->AddEventClient(EventPhysCollision::id, (int(*)(const EventPhys*))OnCollision, 1);

	g_pWorld->GetPhysVars()->bMultithreaded = 1;
	g_hThreadActive = CreateMutex(0,TRUE,0);
	g_hThread = CreateThread(0,0,PhysProc,0,0,0);
	//SetThreadPriority(g_hThread, THREAD_PRIORITY_LOWEST);
	g_PhysRenderer.Init();
}

void CleanupForeignData()
{
	auto iter = g_pWorld->GetEntitiesIterator();
	while (IPhysicalEntity *pent = iter->Next()) 
		for(SEntStateHeader *state=(SEntStateHeader*)pent->GetForeignData(101),*statePrev; state; state=statePrev) {
			statePrev=state->prev; delete[] (char*)state;
		}
	iter->Release();
}

void ShutdownPhysics()
{
	CleanupForeignData();
	g_Tool.~CPhysToolImp();
	WaitForSingleObject(g_hThreadActive,INFINITE);
	TerminateThread(g_hThread,0);
	CloseHandle(g_hThreadActive);
	g_pWorld->Shutdown();
}

void ReloadWorld(const char *fworld)
{
	CleanupForeignData();
	if(g_avatar) { g_avatar->Release(); g_avatar=nullptr; }
	g_Tool.~CPhysToolImp();
	WaitForSingleObject(g_hThreadActive,INFINITE);
	g_pWorld->ClearLoggedEvents();
	g_pWorld->Shutdown(0);
	g_pWorld->SerializeWorld(fworld,0);
	ReleaseMutex(g_hThreadActive);
}

void SaveWorld(const char *fworld)
{
	WaitForSingleObject(g_hThreadActive,INFINITE);
	g_pWorld->SerializeWorld(fworld,1);
	ReleaseMutex(g_hThreadActive);
}

void ReloadWorldAndGeometries(const char *fworld, const char *fgeoms)
{
	CleanupForeignData();
	if(g_avatar) { g_avatar->Release(); g_avatar=nullptr; }
	g_Tool.~CPhysToolImp();
	WaitForSingleObject(g_hThreadActive,INFINITE);
	g_pWorld->Shutdown();
	g_pWorld->Init();
	if (!_stricoll(fworld+strlen(fworld)-6,".phump"))
		g_pWorld->SerializeWorld(fworld,2);
	else if (!_stricoll(fgeoms+strlen(fgeoms)-6,".phump"))
		g_pWorld->SerializeWorld(fgeoms,2);
	else {
		g_pWorld->SerializeGeometries(fgeoms,0);
		g_pWorld->SerializeWorld(fworld,0);
	}

	pe_status_pos sp;
	IPhysicalEntity **ppEnts;
	Vec3 pos(ZERO), bbox[2]={ Vec3(-1E6f), Vec3(1E6f) };
	int i,nEnts;

	ResetProfiler(g_pPhysProfilerData);

	sp.pos.zero();
	sp.q.SetIdentity();
	IPhysicalEntity *player;
	if (!(player = g_pWorld->GetPhysicalEntityById(0x7777)) || !player->GetStatus(&sp) || sp.pos.len2()) { 
		pe_player_dimensions pd;
		for(i=g_pWorld->GetEntitiesInBox(bbox[0],bbox[1],ppEnts,ent_living)-1; i>=0 && !(ppEnts[i]->GetParams(&pd) && !pd.sizeCollider.x); i--);
		if (i>=0)
			ppEnts[i]->GetStatus(&sp);
		else {
			if (!(nEnts = g_pWorld->GetEntitiesInBox(bbox[0],bbox[1],ppEnts,ent_rigid)))
				nEnts = g_pWorld->GetEntitiesInBox(bbox[0],bbox[1],ppEnts,ent_independent);
			for(i=0,pos.z=-1e10f; i<nEnts; i++) {
				ppEnts[i]->GetStatus(&sp);
				pos += (sp.pos-pos)*(float)isneg(pos.z-sp.pos.z);
			}
			g_campos = pos-Vec3(0,1,0);
		}
	}
	g_campos = sp.pos;
	g_camdir = sp.q*Vec3(0,1,0);

	ReleaseMutex(g_hThreadActive);
	g_pWorld->GetPhysVars()->bSingleStepMode = 0;

	GLfloat zero[] = {0,0,0,0};
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, 0);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, zero);
}


void OnCameraMove(float dx, float dy, int trigger)
{
	__int64 curTime; QueryPerformanceCounter((LARGE_INTEGER*)&curTime);
	float dist = (float)(((curTime-g_moveTime)*10000)/g_freq)*0.0001f;
	g_moveTime = curTime;
	if (dist<0 || dist>0.1f)
		dist = 0.01f;
	if (!g_avatar) {
		dist *= g_movespeed*(1+g_bFast*10)*(1-((int)GetKeyState(VK_SHIFT)>>31)*2);
		g_campos += g_camdir*(dist*dy) + 
			(sqr(g_camdir.z)>g_camdir.len2()*0.99f ? g_camdir.GetOrthogonal() : g_camdir^Vec3(0,0,1)).normalized()*(dist*dx);
	} else switch (g_avatar->GetType()) {
		case PE_LIVING: {
			pe_status_pos sp; g_avatar->GetStatus(&sp);
			pe_action_move am;
			am.dir = sp.q*Vec3(dx,dy,0)*g_movespeed*30;
			am.iJump = trigger;
			am.dir.z += 3*trigger;
			g_avatar->Action(&am);
		} break;
		case PE_WHEELEDVEHICLE: {
			pe_action_drive ad;
			if (dy && !trigger)
				ad.dpedal = dy*dist;
			else {
				ad.pedal = 0;
				ad.iGear = 1;
			}
			if (dx)
				ad.dsteer = dx*dist;
			else 
				ad.steer = 0;
			ad.bHandBrake = trigger;
			g_avatar->Action(&ad);
		} break;
	}
}

void OnCameraRotate(float dx, float dy)
{
	Vec3 dirx,diry;
	dirx = (sqr(g_camdir.z)>g_camdir.len2()*0.99f ? g_camdir.GetOrthogonal() : g_camdir^Vec3(0,0,1)).normalized();
	diry = g_camdir^dirx;
	(g_camdir += dirx*dx+diry*dy).normalize();
	if (g_avatar)
		if (g_avatar->GetType()==PE_LIVING) {
			pe_params_pos pp;
			pp.q = Quat::CreateRotationV0V1(Vec3(0,1,0),Vec3(Vec2(g_camdir)).normalized());
			g_avatar->SetParams(&pp);
		}	else {
			pe_status_pos sp;	g_avatar->GetStatus(&sp);
			g_camdirLoc = g_camdir*sp.q;
		}
}

char *GetCamPosTxt(char *str)
{
	sprintf_s(str,64, "%.2f %.2f %.2f", g_campos.x,g_campos.y,g_campos.z);
	return str;
}
void SetCamPosTxt(char *str) 
{
	sscanf_s(str, "%f %f %f", &g_campos.x,&g_campos.y,&g_campos.z);
}
void ChangeViewDist(float dir)
{
	g_viewdist = dir>0 ? min(8000.0f,g_viewdist*1.1f) : max(1.0f,g_viewdist*0.9f);
}

void OnMouseEvent(uint evtWin, int x, int y, int flags)
{
	if (evtWin==WM_RBUTTONDOWN) {
		if (!g_avatar) {
			float k = 2*tan(g_Cam.fovy*0.5f)/g_Cam.size.y;
			ray_hit hit;
			if (g_pWorld->RayWorldIntersection(g_campos, g_Cam.mtx.TransformVector(Vec3((x-g_Cam.size.x*0.5f)*k, 1, (g_Cam.size.y*0.5f-y)*k)*1000), 
					ent_rigid|ent_sleeping_rigid|ent_living, rwi_colltype_any(geom_colltype0|geom_colltype_player)|rwi_pierceability(15), &hit,1)) 
			{
				(g_avatar = hit.pCollider)->AddRef();
				pe_status_pos sp;	g_avatar->GetStatus(&sp);
				g_camposLoc = (g_campos-sp.pos)*sp.q;
				g_camdirLoc = g_camdir*sp.q;
			}
		} else {
			g_avatar->Release(); g_avatar = nullptr;
		}
		return;
	}
	EMouseEvent evt;
	switch (evtWin) {
		case WM_LBUTTONDOWN: evt = eMouseLDown; break;
		case WM_LBUTTONUP  : evt = eMouseLUp; break;
		case WM_MOUSEMOVE  : evt = eMouseMove; break;
		default: return;
	}
	g_Tool.MouseCallback((CViewport*)&g_Cam, evt, CPoint(x,y), flags);
}

void OnSetCursor() 
{ 
	g_System.UpdateTime();
	g_Tool.Display(g_DC); 
}

#include <CryNetwork\ISerialize.h>
struct SMemSaver : ISerialize {
	CMemStream &stm;
	SMemSaver(CMemStream &stm) : stm(stm) {}
	virtual bool Write(const void *val, int sz) { stm.Write(val,sz); return true; }
	virtual bool OptionalGroup(bool present) { stm.Write(present); return present; }
};
struct SMemLoader : ISerialize {
	CMemStream &stm;
	SMemLoader(CMemStream &stm) : stm(stm) {}
	virtual bool Serialize(void *val, int sz) { stm.ReadRaw(val,sz); return true; }
	virtual bool OptionalGroup(bool present) { stm.Read(present); return present; }
};

void EvolveWorld(int dir)
{
	__int64 curTime; QueryPerformanceCounter((LARGE_INTEGER*)&curTime);
	ResetProfiler(g_pPhysProfilerData);
	float dt = (float)(((curTime-g_stepTime)*10000)/g_freq)*0.0001f;
	if (dt<0 || dt>0.05f)
		dt = 0.01f;
	pe_params_foreign_data pfd;
	pfd.iForeignData = 101;
	static CMemStream stmBuf(false);

	if (dir>0) {
		auto iter = g_pWorld->GetEntitiesIterator();
		while (IPhysicalEntity *pent = iter->Next()) if (pent->GetType()!=PE_STATIC) {
			SMemSaver saver(stmBuf); stmBuf.m_iPos=0;
			pent->GetStateSnapshot(TSerialize(&saver),0,16);
			SEntStateHeader *state = (SEntStateHeader*)pent->GetForeignData(101);
			if (state && state->size==stmBuf.m_iPos && !memcmp(stmBuf.GetBuf(),state+1,state->size))
				++state->count;
			else {
				SEntStateHeader *stateNew = (SEntStateHeader*)new char[sizeof(SEntStateHeader)+stmBuf.m_iPos];
				memcpy(stateNew+1, stmBuf.GetBuf(), stateNew->size = stmBuf.m_iPos);
				stateNew->pname = (stateNew->prev = state) ? state->pname : (const char*)pent->GetForeignData(100);
				stateNew->count = 1;
				pfd.pForeignData = stateNew;
				pent->SetParams(&pfd,1);
			}
		}

		g_pWorld->TimeStep(dt);
		g_pWorld->PumpLoggedEvents();
	} else if (dir<0) {
		auto iter = g_pWorld->GetEntitiesIterator();
		stmBuf.m_iPos = 0;
		while (IPhysicalEntity *pent = iter->Next()) if (pent->GetType()!=PE_STATIC) 
			stmBuf.Write(pent);
		iter->Release();

		for(int i=0; i*sizeof(void*) < stmBuf.m_iPos; i++) {
			IPhysicalEntity *pent = ((IPhysicalEntity**)stmBuf.m_pBuf)[i];
			if (SEntStateHeader *state = (SEntStateHeader*)pent->GetForeignData(101)) {
				CMemStream stm(state+1,state->size,false);
				SMemLoader loader(stm);
				pent->SetStateFromSnapshot(TSerialize(&loader),16);
				if (!--state->count) {
					if (!(pfd.pForeignData = state->prev)) {
						pfd.pForeignData = (void*)state->pname;
						pfd.iForeignData = 100;
					}
					pent->SetParams(&pfd,1);
					pfd.iForeignData = 101;
					delete[] (char*)state;
				}
			}
			switch (pent->GetType()) {
				case PE_SOFT: case PE_ROPE: 
				int awake = pent->GetStatus(&pe_status_awake());
				pe_action_awake aa;
				pent->Action(&aa,1);
				pent->StartStep(0.01f);
				pent->DoStep(0,0);
				aa.bAwake = awake;
				pent->Action(&aa,1);
			}
		}
	}
	g_stepTime = curTime;

	int i,j,bSelectionRemoved=0;
	for(i=j=1; i<g_nProfVisInfos; i++) {
		if (i!=j)
			g_ProfVisInfos[j] = g_ProfVisInfos[i];
		if (g_ProfVisInfos[i].iCode==g_iActiveCode && g_ProfVisInfos[i].timeout<5.0f)
			bSelectionRemoved = 1;		
		if ((g_ProfVisInfos[i].timeout-=dt)>0)
			j++;
	}
	g_nProfVisInfos = j;
	if (bSelectionRemoved)
		SelectPrevProfVisInfo();
}

void SetStep(float dt) { g_pWorld->GetPhysVars()->fixedTimestep = dt; }
float GetStep() { return g_pWorld->GetPhysVars()->fixedTimestep; }

void SetNumThreads(int n) { g_pWorld->GetPhysVars()->numThreads = n; }
int GetNumThreads(int& nmax) { nmax=MAX_PHYS_THREADS; return g_pWorld->GetPhysVars()->numThreads; }

int DrawProfileNode(int ix,int iy, int szy, ProfilerData *pd,int iNode)
{
	char str[1024];
	char bExpanded=0;
	float time;
	static float clr[2][3] = { {0.7f,0.88f,0.43f}, {1,1,1} };

	for(int iChild=pd->TimeSamples[iNode].iChild; iChild>=0 && pd->TimeSamples[iChild].iNext!=iChild; iChild=pd->TimeSamples[iChild].iNext) {
		bExpanded = g_ProfVisInfos[getProfVisInfo(pd->TimeSamples[iChild].iCode,0)].bExpanded;
		glColor3fv(clr[pd->TimeSamples[iChild].iCode==g_iActiveCode]);
		time = TicksToMilliseconds(bExpanded ? pd->TimeSamples[iChild].iSelfTime : pd->TimeSamples[iChild].iTotalTime);
		glRasterPos2i(ix,iy);
		glCallLists(sprintf(str, "%s %s %.3fms / %d", pd->TimeSamples[iChild].iChild>=0 ? (bExpanded ? "-":"+"):"", 
			pd->TimeSamples[iChild].pProfiler ? pd->TimeSamples[iChild].pProfiler->m_name:"???", time, pd->TimeSamples[iChild].iCount), GL_UNSIGNED_BYTE, str);
		iy += szy;
		if (bExpanded && pd->TimeSamples[iChild].iChild>=0)
			iy = DrawProfileNode(ix+10,iy,szy, pd,iChild);
	}
	return iy;
}

void ProcessPhysProfileNode(phys_profile_info &info)
{
	info.nTicksAvg = (int)(((int64)info.nTicksAvg * 15 + info.nTicksLast) >> 4);
	int mask = (info.nTicksPeak-info.nTicksLast) >> 31;
	mask |= (70-info.peakAge) >> 31;
	info.nTicksPeak += info.nTicksLast-info.nTicksPeak & mask;
	info.nCallsPeak += info.nCallsLast-info.nCallsPeak & mask;
	info.peakAge = info.peakAge+1 & ~mask;
}

void RenderWorld(HWND hWnd, HDC hDC)
{
	g_System.UpdateTime();

	RECT rect;
	GetClientRect(hWnd, &rect);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective((int)(g_Cam.GetFov()*(180/gf_PI)+0.5f), (double)rect.right/rect.bottom, 0.05f,g_viewdist*4);
	glViewport(0,0,rect.right,rect.bottom);
	g_Cam.size.set(rect.right+1,rect.bottom+1);

	glEnable(GL_DEPTH_TEST);
	glClearDepth(1.0f);
	glClearColor(0,0,0,1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (g_sync)
		WaitForSingleObject(g_hThreadActive,INFINITE);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	Vec3 posMarker;
	if (g_avatar) {
		pe_status_pos sp; g_avatar->GetStatus(&sp);
		g_campos = sp.q*g_camposLoc + sp.pos;
		if (g_avatar->GetType()!=PE_LIVING)
			g_camdir = sp.q*g_camdirLoc;
		(posMarker = (sp.BBox[0]+sp.BBox[1])*0.5f).z = sp.BBox[1].z+0.1f;
		posMarker += sp.pos;
	}
	gluLookAt(g_campos.x,g_campos.y,g_campos.z, g_campos.x+g_camdir.x,g_campos.y+g_camdir.y,g_campos.z+g_camdir.z, 0,0,1);
	Vec3 xaxis = (g_camdir^Vec3(0,0,1)).normalized();
	g_Cam.mtx.SetFromVectors(xaxis, g_camdir, xaxis^g_camdir, g_campos);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glShadeModel(GL_SMOOTH);
	glDisable(GL_LIGHTING);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	g_pWorld->GetPhysVars()->iDrawHelpers = 8114+g_bShowColl+g_bShowBBoxes*pe_helper_bbox;
	g_pWorld->DrawPhysicsHelperInformation(&g_PhysRenderer);
	g_PhysRenderer.m_blockMask |= 1;
	g_Tool.Display(g_DC);
	g_PhysRenderer.m_blockMask &= ~1;
	g_PhysRenderer.DrawGeomBuffers(0.01f);
	if (g_avatar) {
		float f = g_System.time-(int)g_System.time;
		g_DC.SetColor(RGB(128+(int)(sin(f*gf_PI)*127),0,0),0.7f);
		g_DC.DrawBall(posMarker,0.15f);	
	}

	glDisable(GL_DEPTH_TEST); 
	glShadeModel(GL_FLAT); 
	glMatrixMode(GL_PROJECTION); glLoadIdentity(); 
	glOrtho(rect.left,rect.right,rect.bottom,rect.top, -1,1); 
	glMatrixMode(GL_MODELVIEW); glLoadIdentity(); 
	glDisable(GL_BLEND); 

	SelectObject(hDC, GetStockObject(DEFAULT_GUI_FONT));
	static BOOL dummy = wglUseFontBitmaps(hDC,0,255,100);
	glListBase(100);
	long offsx = 0;
	TEXTMETRIC tm; GetTextMetrics(hDC, &tm);
	char str[256];
	SIZE sz;
	
	PhysicsVars *vars = g_pWorld->GetPhysVars();
	if (vars->bProfileGroups = vars->bProfileEntities = g_bShowProfiler>>1) {
		phys_profile_info* pInfos;
		int j,i=0,len,n;
		pe_status_pos sp;
		glColor4f(1,1,1,1);

		for (j=0,n=g_pWorld->GetGroupProfileInfo(pInfos); j<n; i++,j++)	{
			phys_profile_info &info = pInfos[j];
			ProcessPhysProfileNode(info);
			glRasterPos2i(20,20+i*tm.tmHeight);
			glCallLists(len=sprintf_s(str, "%s %.2fms/%d (peak %.2fms/%d)", info.pName, TicksToMilliseconds(info.nTicksAvg), info.nCallsLast,
				TicksToMilliseconds(info.nTicksPeak), info.nCallsPeak), GL_UNSIGNED_BYTE, str);
			if (g_bShowProfiler & 1) {
				GetTextExtentPoint(hDC, str,len, &sz);
				offsx = max(offsx, sz.cx);
			}
			i += j==n-3;
		}

		glColor3f(0.3f,0.6f,1);
		for(j=0,i++,n=g_pWorld->GetEntityProfileInfo(pInfos); j<n; j++,i++) {
			phys_profile_info &info = pInfos[j];
			ProcessPhysProfileNode(info);
			glRasterPos2i(20,20+i*tm.tmHeight);
			glCallLists(sprintf_s(str, "%.2fms/%d (peak %.2fms/%d) %s (id %d)", TicksToMilliseconds(info.nTicksAvg), info.nCallsLast,
				TicksToMilliseconds(info.nTicksPeak), info.nCallsPeak, info.pName, info.id), GL_UNSIGNED_BYTE, str);
			sprintf_s(str, "%s %.2fms", info.pName, TicksToMilliseconds(info.nTicks));
			info.pEntity->GetStatus(&sp);
			g_PhysRenderer.DrawText(sp.pos, str, ColorB(77,154,255,255), true);
		}
	}

	if (g_bShowProfiler & 1) {
		if (!g_sync)
			WaitForSingleObject(g_hThreadActive,INFINITE);
		DrawProfileNode(offsx+40,20,tm.tmHeight, g_pPhysProfilerData,0);
		ResetProfiler(g_pPhysProfilerData, 1<<MAX_PHYS_THREADS);
	}
	if (g_bShowProfiler & 1 || g_sync) 
		ReleaseMutex(g_hThreadActive);

	GetCamPosTxt(str);
	GetTextExtentPoint(hDC, str,strlen(str), &sz);
	glColor3f(1,1,1);
	glRasterPos2i(rect.right-20-sz.cx,20);
	glCallLists(strlen(str), GL_UNSIGNED_BYTE, str);
	int fps = min(100, (int)(1/g_dtPhys+0.9f)*g_bAnimate);
	if (fps>=99) fps = 100;
	sprintf_s(str, "PhysFPS %d %s", fps, fps==100 ? "(cap)":"");
	glRasterPos2i(rect.right-20-sz.cx,20+sz.cy);
	glCallLists(strlen(str), GL_UNSIGNED_BYTE, str);
	if (!g_bAnimate) {
		g_pWorld->GetPhysVars()->lastTimeStep = 0;
		glRasterPos2i(20,rect.bottom-(sz.cy*3>>1));
		strcpy(str, "Press F1 to start the simulation");
		glCallLists(strlen(str), GL_UNSIGNED_BYTE, str);
	}

	g_PhysRenderer.DrawTextBuffers(hDC);

	glFlush();
	glFinish();
	SwapBuffers(hDC);
	g_pWorld->PumpLoggedEvents();
}


ColorB CPhysRenderer::g_colorTab[8] = { 
	ColorB(136,141,162,255), ColorB(212,208,200,255), ColorB(255,255,255,255), ColorB(214,222,154,128), 
	ColorB(231,192,188,255), ColorB(164,0,0,80), ColorB(164,0,0,255), ColorB(168,224,251,255) 
};


CPhysRenderer::CPhysRenderer()
{
	m_pRayGeom = 0; m_pRay = 0;
	m_timeRayFadein = 0.2f;
	m_wireframeDist = 5.0f;
}

void CPhysRenderer::Init()
{
	primitives::ray aray; 
	aray.dir.Set(0,0,1); aray.origin.zero();
	m_pRayGeom = g_pWorld->GetGeomManager()->CreatePrimitive(primitives::ray::type, &aray);
	m_pRay = (primitives::ray*)m_pRayGeom->GetData();
	m_rayQ.init();
	m_textQ.init();
}

CPhysRenderer::~CPhysRenderer()
{
	if (m_pRayGeom) m_pRayGeom->Release();
}


void CPhysRenderer::DrawGeometry(IGeometry *pGeom, geom_world_data *pgwd, int idxColor, int bSlowFadein, const Vec3 &sweepDir, const Vec4&)
{
	if (m_blockMask & 1)
		return;
	if (!bSlowFadein)	{
		ColorB clr = g_colorTab[idxColor & 7];
		clr.a >>= idxColor>>8;
		DrawGeometry(pGeom,pgwd, clr);
	} else {
		if (pGeom->GetType()==GEOM_RAY) {
			primitives::ray *pray = (primitives::ray*)pGeom->GetData();
			SRayRec &r = m_rayQ.push_back();
			if (!pgwd) {
				r.origin = pray->origin;
				r.dir = pray->dir;
			} else {
				r.origin = pgwd->R*pray->origin*pgwd->scale + pgwd->offset;
				r.dir = pgwd->R*pray->dir;
			}
			r.time = m_timeRayFadein;
		}
	}
}

void CPhysRenderer::DrawLine(const Vec3& pt0, const Vec3& pt1, int idxColor, int bSlowFadein)
{
	m_pRay->origin = pt0;
	m_pRay->dir = pt1-pt0;
	DrawGeometry(m_pRayGeom,0, idxColor, bSlowFadein);
}

void CPhysRenderer::DrawGeomBuffers(float dt)
{
	float rtime=1/m_timeRayFadein;
	ColorB clr = g_colorTab[7]; 
	m_rayQ.iterate([&](SRayRec &r)->bool {
		clr.a = FtoI(g_colorTab[7].a*r.time*rtime);
		m_pRay->origin = r.origin;
		m_pRay->dir = r.dir;
		DrawGeometry(m_pRayGeom, 0, clr);
		return (r.time -= dt)<0;
	});
}

void CPhysRenderer::DrawText(const Vec3 &pt, const char *txt, ColorB color, bool center)
{
	STextRec& tx = m_textQ.push_back();
	tx = { g_txtBuf+g_txtBufPos, color, pt, center };
	strcpy(tx.str, txt);
	g_txtBufPos += strlen(txt)+1;
}

void CPhysRenderer::DrawTextBuffers(HDC hDC)
{
	float k = g_Cam.size.y*0.5f/tan(g_Cam.fovy*0.5f);
	m_textQ.iterate([&](STextRec& txt)->bool {
		Vec3 pt = (txt.pt-g_Cam.mtx.GetTranslation())*Matrix33(g_Cam.mtx);
		if (pt.y>0) {
			glColor4fv((float*)&(Vec3(*(Vec3_tpl<uchar>*)&txt.color)*(1.0f/256)));
			float ry = k/pt.y;
			Vec2i offs(g_Cam.size.x>>1, g_Cam.size.y>>1);
			SIZE sz;
			char *str=txt.str, *newline=strchr(str,'\n');
			if (txt.center || newline)
				GetTextExtentPoint(hDC,txt.str,newline ? newline-txt.str:strlen(str),&sz);
			if (txt.center)
				offs.x -= sz.cx>>1;
			offs += Vec2i(pt.x*ry,-pt.z*ry);
			int len; do {
				newline = strchr(str,'\n');
				len = newline ? newline+1-str : strlen(str);
				glRasterPos2i(offs.x, offs.y);
				glCallLists(len, GL_UNSIGNED_BYTE, str);
				str += len;	offs.y += sz.cy;
			} while(len);
		}
		return true;
	});
	g_txtBufPos = 0;
}


inline float getheight(primitives::heightfield *phf, int ix,int iy) { 
	return phf->fpGetHeightCallback ? 
		phf->getheight(ix,iy) : 
		((float*)phf->fpGetSurfTypeCallback)[Vec2i(ix,iy)*phf->stride]*phf->heightscale;
}

#define _clr(c) glColor4ubv((GLubyte*)&(c))
#define _vtx(v) glVertex3fv((const GLfloat*)&(v))

void CPhysRenderer::DrawGeometry(IGeometry *pGeom, geom_world_data *pgwd, const ColorB& clr)
{
	Matrix33 R = Matrix33::CreateIdentity();
	Vec3 pos(ZERO),center,sz,ldir0,ldir1(0,0,1),n,pt[5],campos;
	float scale=1.0f,t,l,sx,dist;
	primitives::box bbox;
	ColorB clrlit[4];
	int i,j,itype=pGeom->GetType();
#define FIAT_LUX(color) \
	t = (n*ldir0)*-0.5f; l = t+fabs_tpl(t); t = (n*ldir1)*-0.1f; l += t+fabs_tpl(t); l = min(1.0f,l+0.05f); \
	color = ColorB(FtoI(l*clr.r), FtoI(l*clr.g), FtoI(l*clr.b), clr.a)

	if (pgwd) {
		R = pgwd->R; pos = pgwd->offset; scale = pgwd->scale;
	}
	pGeom->GetBBox(&bbox);
	sz = (bbox.size*(bbox.Basis *= R.T()).Fabs())*scale;
	center = pos + R*bbox.center*scale;
	Vec3 dir(sgnnz(g_camdir.x)*sz.x,sgnnz(g_camdir.y)*sz.y,sgnnz(g_camdir.z)*sz.z);
	if (itype!=GEOM_HEIGHTFIELD && ((center-dir-g_campos)*g_camdir>g_viewdist || (center+dir-g_campos)*g_camdir<0))
		return;
	campos = g_campos*3;
	(ldir0 = g_camdir).z = 0; (ldir0 = ldir0.normalized()*0.5f).z = (float)sqrt3*-0.5f;
	glDepthMask(clr.a==255 ? GL_TRUE:GL_FALSE);

	switch (itype) 
	{
  	case GEOM_TRIMESH: case GEOM_VOXELGRID:	{
			mesh_data *pmesh = (mesh_data*)pGeom->GetData();
			glBegin(GL_TRIANGLES);
			for(i=0;i<pmesh->nTris;i++) {
				for(j=0;j<3;j++) pt[j] = R*pmesh->pVertices[pmesh->pIndices[i*3+j]]*scale+pos;
				n = R*pmesh->pNormals[i]; FIAT_LUX(clrlit[0]);
				 _clr(clrlit[0].r);	_vtx(pt[0]); _vtx(pt[1]); _vtx(pt[2]); 
			}	glEnd(); 
			glBegin(GL_LINES);
			for(i=0;i<pmesh->nTris;i++) {
				for(j=0;j<3;j++) pt[j] = R*pmesh->pVertices[pmesh->pIndices[i*3+j]]*scale+pos;
				if ((pt[0]+pt[1]+pt[2]-campos).len2()<sqr(m_wireframeDist)*9)	{
					 _clr(clr);	_vtx(pt[0]);_vtx(pt[1]); _vtx(pt[1]);_vtx(pt[2]); _vtx(pt[2]);_vtx(pt[0]);
				}
			}	glEnd(); 
			break; }

		case GEOM_HEIGHTFIELD: {
			primitives::heightfield *phf = (primitives::heightfield*)pGeom->GetData();
			center = phf->Basis*(((g_campos-pos)*R)/scale-phf->origin);
			Vec2 c(center.x*phf->stepr.x, center.y*phf->stepr.y);
			ColorB clrhf[2];
			dist = 100.0f/scale;
			clrhf[0] = ColorB(FtoI(clr.r*0.8f), FtoI(clr.g*0.8f), FtoI(clr.b*0.8f), clr.a);
			clrhf[1] = ColorB(FtoI(clr.r*0.4f), FtoI(clr.g*0.4f), FtoI(clr.b*0.4f), clr.a);
			R *= phf->Basis.T();
			pos += R*phf->origin*scale;
			glBegin(GL_TRIANGLES);
			for(j=max(0,FtoI(c.y-dist*phf->stepr.y-0.5f)); j<=min(phf->size.y-1,FtoI(c.y+dist*phf->stepr.y-0.5f)); j++) {
				sx = sqrt_tpl(max(0.0f, sqr(dist*phf->stepr.y)-sqr(j+0.5f-c.y)));
				i = max(0,FtoI(c.x-sx-0.5f));
				pt[0] = R*Vec3(i*phf->step.x, j*phf->step.y, getheight(phf,i,j))*scale + pos;
				pt[1] = R*Vec3(i*phf->step.x, (j+1)*phf->step.y, getheight(phf,i,j+1))*scale + pos;
				for(; i<=min(phf->size.x-1,FtoI(c.x+sx-0.5f)); i++)	{
					clrlit[0] = clrhf[(i^j)&1];
					pt[2] = R*Vec3((i+1)*phf->step.x, j*phf->step.y, getheight(phf,i+1,j))*scale + pos;
					pt[3] = R*Vec3((i+1)*phf->step.x, (j+1)*phf->step.y, getheight(phf,i+1,j+1))*scale + pos;
					if (!phf->fpGetSurfTypeCallback || phf->fpGetSurfTypeCallback(i,j)!=phf->typehole) {
						_clr(clrlit[0]);
						_vtx(pt[0]); _vtx(pt[2]); _vtx(pt[1]);
						_vtx(pt[1]); _vtx(pt[2]); _vtx(pt[3]);
					}
					pt[0] = pt[2]; pt[1] = pt[3];
				}
			}	glEnd();
			break; }

		case GEOM_BOX: {
			primitives::box *pbox = (primitives::box*)pGeom->GetData();
			bbox.Basis = pbox->Basis*R.T();
			bbox.center = pos+R*pbox->center*scale;
			bbox.size = pbox->size*scale;
			glBegin(GL_TRIANGLES);
			for(i=0;i<6;i++) {
				n = bbox.Basis.GetRow(i>>1)*float((i*2&2)-1); FIAT_LUX(clrlit[0]);
				pt[4] = bbox.center + n*bbox.size[i>>1];
				for(j=0;j<4;j++)
					pt[j] = pt[4] + bbox.Basis.GetRow(incm3(i>>1))*bbox.size[incm3(i>>1)]*float(((j^i)*2&2)-1) +
													bbox.Basis.GetRow(decm3(i>>1))*bbox.size[decm3(i>>1)]*float((j&2)-1);
				_clr(clrlit[0]);
				_vtx(pt[0]); _vtx(pt[2]); _vtx(pt[3]);
				_vtx(pt[0]); _vtx(pt[3]); _vtx(pt[1]);
			} glEnd();
			break; }

		case GEOM_CYLINDER: {
			primitives::cylinder *pcyl = (primitives::cylinder*)pGeom->GetData();
			const float cos15=0.96592582f,sin15=0.25881904f;
			float x,y;
			Vec3 axes[3],center;
			axes[2] = R*pcyl->axis;
			axes[0] = axes[2].GetOrthogonal().normalized();
			axes[1] = axes[2]^axes[0];
			center = R*pcyl->center*scale + pos;
			pt[0] = pt[2] = center+axes[0]*(pcyl->r*scale);
			n = axes[0]; FIAT_LUX(clrlit[0]);
			n = axes[2]; FIAT_LUX(clrlit[2]);
			n = -axes[2]; FIAT_LUX(clrlit[3]);
			glBegin(GL_TRIANGLES);
			axes[2] *= pcyl->hh*scale;
			for(i=0,x=cos15,y=sin15; i<24; i++,pt[0]=pt[1],clrlit[0]=clrlit[1]) {
				n = axes[0]*x + axes[1]*y; FIAT_LUX(clrlit[1]);
				pt[1] = center + n*(pcyl->r*scale);
				_clr(clrlit[0]);_vtx(pt[0]-axes[2]); _clr(clrlit[1]);_vtx(pt[1]-axes[2]); _clr(clrlit[0]);_vtx(pt[0]+axes[2]);
				_clr(clrlit[1]);_vtx(pt[1]+axes[2]); _clr(clrlit[0]);_vtx(pt[0]+axes[2]); _clr(clrlit[1]);_vtx(pt[1]-axes[2]);
				_clr(clrlit[2]); _vtx(pt[2]+axes[2]);_vtx(pt[0]+axes[2]);_vtx(pt[1]+axes[2]);
				_clr(clrlit[3]); _vtx(pt[2]-axes[2]);_vtx(pt[1]-axes[2]);_vtx(pt[0]-axes[2]);
				t = x; x = x*cos15-y*sin15; y = y*cos15+t*sin15;
			}	glEnd();
			break; }

		case GEOM_CAPSULE: case GEOM_SPHERE: {
			primitives::cylinder cyl,*pcyl;
			primitives::sphere *psph;
			if (itype==GEOM_CAPSULE)
				pcyl = (primitives::cylinder*)pGeom->GetData();
			else {
				psph = (primitives::sphere*)pGeom->GetData();	pcyl = &cyl;
				cyl.axis.Set(0,0,1); cyl.center=psph->center; cyl.hh=0; cyl.r=psph->r; 
			}
			const float cos15=0.96592582f,sin15=0.25881904f;
			float x,y,cost,sint,costup,sintup;
			Vec3 axes[3],center,haxis,nxy;
			int icap;
			axes[2] = R*pcyl->axis;
			axes[0] = axes[2].GetOrthogonal().normalized();
			axes[1] = axes[2]^axes[0];
			center = R*pcyl->center*scale + pos;
			pt[0] = pt[2] = center+axes[0]*(pcyl->r*scale);
			haxis = axes[2]*(pcyl->hh*scale);
			n = axes[0]; FIAT_LUX(clrlit[0]);
			glBegin(GL_TRIANGLES);
			for(i=0,x=cos15,y=sin15; i<24; i++,pt[0]=pt[1],clrlit[0]=clrlit[1]) {
				n = axes[0]*x + axes[1]*y; FIAT_LUX(clrlit[1]);
				pt[1] = center + n*(pcyl->r*scale);
				_clr(clrlit[0]);_vtx(pt[0]-haxis); _clr(clrlit[1]);_vtx(pt[1]-haxis); _clr(clrlit[0]);_vtx(pt[0]+haxis);
				_clr(clrlit[1]);_vtx(pt[1]+haxis); _clr(clrlit[0]);_vtx(pt[0]+haxis); _clr(clrlit[1]);_vtx(pt[1]-haxis);
				t = x; x = x*cos15-y*sin15; y = y*cos15+t*sin15;
			}
			for(icap=0;icap<2;icap++,haxis.Flip(),axes[2].Flip()) for(j=0,cost=1,sint=0,costup=cos15,sintup=sin15; j<6; j++) {
				n = axes[0]*cost+axes[2]*sint; FIAT_LUX(clrlit[0]); 
				pt[0] = center + haxis + n*(pcyl->r*scale);
				n = axes[0]*costup+axes[2]*sintup; FIAT_LUX(clrlit[2]);
				pt[2] = center + haxis + n*(pcyl->r*scale);
				for(i=0,x=cos15,y=sin15; i<24; i++,pt[0]=pt[1],pt[2]=pt[3],clrlit[0]=clrlit[1],clrlit[2]=clrlit[3]) {
					nxy = axes[0]*x + axes[1]*y; 
					n = nxy*costup+axes[2]*sintup; FIAT_LUX(clrlit[3]);
					pt[3] = center + haxis + n*(pcyl->r*scale);
					n = nxy*cost+axes[2]*sint; FIAT_LUX(clrlit[1]); 
					pt[1] = center + haxis + n*(pcyl->r*scale);
					_clr(clrlit[0]);_vtx(pt[0]); _clr(clrlit[1+icap]);_vtx(pt[1+icap]); _clr(clrlit[2-icap]);_vtx(pt[2-icap]);
					_clr(clrlit[1]);_vtx(pt[1]); _clr(clrlit[3-icap]);_vtx(pt[3-icap]); _clr(clrlit[2+icap]);_vtx(pt[2+icap]);
					t = x; x = x*cos15-y*sin15; y = y*cos15+t*sin15;
				}
				cost = costup; sint = sintup;
				costup = cost*cos15-sint*sin15; sintup = sint*cos15+cost*sin15;
			}
			glEnd();
			break; }

		case GEOM_RAY: {
			primitives::ray *pray = (primitives::ray*)pGeom->GetData();
			pt[0] = pos + R*pray->origin*scale;
			pt[1] = pt[0]+R*pray->dir*scale;
			glBegin(GL_LINES); _clr(clr);_vtx(pt[0]);_vtx(pt[1]); glEnd();
			break; }
	}
	glDepthMask(GL_TRUE);
}


#define _clrf(c) glColor3fv(&c.x)

void DisplayContextImp::DrawLine(const Vec3& pt0, const Vec3& pt1, const ColorF& clr0, const ColorF& clr1)
{
	glBegin(GL_LINES);
	_clrf(clr0); _vtx(pt0);
	_clrf(clr1); _vtx(pt1);
	glEnd();
}

void DisplayContextImp::DrawBall(const Vec3& c, float r)
{
	static GLUquadric *quad = gluNewQuadric();
	glPushMatrix();
	glTranslatef(c.x,c.y,c.z);
	gluSphere(quad,r,16,8);
	glPopMatrix();
}

void DisplayContextImp::SetColor(COLORREF clr, float alpha) 
{
	glColor4f((m_color.r=clr&255)*(1.0f/256), (m_color.g=clr>>8&255)*(1.0f/256), (m_color.b=clr>>16&255)*(1.0f/256), alpha);
}
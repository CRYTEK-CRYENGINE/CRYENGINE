#include <Windows.h>
#include <GL\gl.h>
#include <GL\glu.h>
#include <stdlib.h>
#include <stdio.h>
#include <Shlwapi.h>
#include <Shobjidl.h>

#include "resource.h"
#define MAX_LOADSTRING 100
#pragma warning (disable : 4244 4267 4996 4554)

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name
HDC g_hDC;
HWND g_hWnd=0, g_hEdit=0;
HGLRC g_glrc;
int g_bAnimate;
char g_arg[3][MAX_PATH];
int g_bFast=1,g_bShowColl=0,g_bShowBBoxes=0,g_bShowProfiler=0,g_moveMode=0,g_sync=0;
float g_stepCustom = 0.0333f;
extern HANDLE g_hThreadActive;

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

void InitPhysics();
void ShutdownPhysics();
void SaveWorld(const char *fworld);
void ReloadWorld(const char *fworld);
void ReloadWorldAndGeometries(const char *fworld,const char *fgeoms);
void EvolveWorld(int dir=1);
void RenderWorld(HWND hWnd, HDC hDC);
void OnCameraMove(float dx, float dy, int trigger=0);
void OnCameraRotate(float dx, float dy);
char *GetCamPosTxt(char *str);
void SetCamPosTxt(char *str);
void ChangeViewDist(float dir);
void OnMouseEvent(UINT evtWin, int x, int y, int flags);
void OnSetCursor();
void SelectNextProfVisInfo();
void SelectPrevProfVisInfo();
void ExpandProfVisInfo(int bExpand);
void SetStep(float dt);
float GetStep();
int GetNumThreads(int &numMax);
void SetNumThreads(int n);

BOOL SetupPixelFormat(HDC hDC)
{
  static PIXELFORMATDESCRIPTOR pfd = {
    sizeof(PIXELFORMATDESCRIPTOR),  // size of this pfd
    1,                              // version number
     PFD_DRAW_TO_WINDOW |            // support bitmap
     PFD_SUPPORT_OPENGL |             // support OpenGL
		 PFD_DOUBLEBUFFER,
    PFD_TYPE_RGBA,                  // RGBA type
    24,                             // 24-bit color depth
    0, 0, 0, 0, 0, 0,               // color bits ignored
    0,                              // no alpha buffer
    0,                              // shift bit ignored
    0,                              // no accumulation buffer
    0, 0, 0, 0,                     // accum bits ignored
    32,                             // 32-bit z-buffer
    0,                              // no stencil buffer
    0,                              // no auxiliary buffer
    PFD_MAIN_PLANE,                 // main layer
    0,                              // reserved
    0, 0, 0                         // layer masks ignored
  };
	int pixelformat;
	HBITMAP hBmp = (HBITMAP)GetCurrentObject(hDC, OBJ_BITMAP);
	BITMAP bmp;	GetObjectA(hBmp, sizeof(BITMAP), &bmp);
	pfd.cColorBits=bmp.bmPlanes*bmp.bmBitsPixel;
	if (!(pixelformat=ChoosePixelFormat(hDC, &pfd))) { 
		MessageBox(0,"ChoosePixelFormat failed",0,0); 
		return FALSE; 
	}
	if (!SetPixelFormat(hDC, pixelformat, &pfd)) { 
		MessageBox(0,"SetPixelFormat failed",0,0); 
		return FALSE; 
	}

  return TRUE;
}


int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_PHYSTESTBED, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	int i,bString,bMoving=0; char *ptr,*arg;
	for(i=0,ptr=lpCmdLine; i<3; i++) {
		for(arg=ptr,bString=0; *ptr && (bString || !isspace(*ptr)); ptr++)
			bString ^= (*ptr=='"');
		if (*ptr)
			*ptr++ = 0;
		strcpy(g_arg[i], arg);
	}

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow)) 
		return FALSE;

	hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_PHYSTESTBED);

	do {
		float dx=0,dy=0;
		int trigger = 0;
		if (GetForegroundWindow()==g_hWnd) {
			if (GetAsyncKeyState('A')<0) dx -= 1;
			if (GetAsyncKeyState('D')<0) dx += 1;
			if (GetAsyncKeyState('W')<0) dy += 1;
			if (GetAsyncKeyState('S')<0) dy -= 1;
			if (GetAsyncKeyState(VK_SPACE)<0 && g_bAnimate) trigger = 1;
		}
		int bMovingNew = dx*dx+dy*dy>0;
		if (bMovingNew | bMoving | trigger)
			OnCameraMove(dx,dy,trigger);
		bMoving = bMovingNew;

		if (PeekMessage(&msg,0,0,0,PM_REMOVE)) {
			if (g_hEdit || !TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			if (msg.message==WM_QUIT) 
				break;
		}	else if (g_bAnimate | bMoving || GetCapture()==g_hWnd && GetKeyState(VK_SHIFT)<0)
			InvalidateRect(g_hWnd,0,0);
			//RenderWorld(g_hWnd,g_hDC);
	} while (true);

	for(i=0;i<3;i++) if (strlen(g_arg[i])>=4 && *(int*)(g_arg[i]+strlen(g_arg[i])-4)=='pmt.')
		DeleteFile(g_arg[i]);

	return (int) msg.wParam;
}


ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX); 

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, (LPCTSTR)IDI_PHYSTESTBED);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= (LPCTSTR)IDC_PHYSTESTBED;
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= 0;//LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);

	return RegisterClassEx(&wcex);
}


void UpdateMenuItems(HWND hWnd)
{
	float dt = GetStep();
	if (dt>0 && dt!=0.01f && dt!=0.02f && dt!=g_stepCustom) {
		g_stepCustom = dt;
		MENUITEMINFO mii;	mii.cbSize = sizeof(mii);
		mii.fMask = MIIM_STRING;
		char str[20],*trail = str+sprintf_s(str,"Fixed Step %.5f",g_stepCustom)-1;
		for(; *trail=='0'; *trail--=0);
		mii.dwTypeData = str;
		SetMenuItemInfo(GetMenu(hWnd), ID_OPTIONS_FIXEDSTEP3, MF_BYCOMMAND, &mii);
	}
	HMENU hmenu = GetMenu(hWnd);
	EnableMenuItem(hmenu, ID_RELOADGEOMETRIES, MF_BYCOMMAND | (strlen(g_arg[0])*strlen(g_arg[1])>0 ? MF_ENABLED : MF_DISABLED));
	EnableMenuItem(hmenu, ID_SAVEWORLD, MF_BYCOMMAND | (strlen(g_arg[2])>0 ? MF_ENABLED : MF_DISABLED));
	EnableMenuItem(hmenu, ID_RELOADWORLD1, MF_BYCOMMAND | (strlen(g_arg[2])>0 ? MF_ENABLED : MF_DISABLED));
	CheckMenuItem(hmenu, ID_ANIMATE, MF_BYCOMMAND | (g_bAnimate ? MF_CHECKED:MF_UNCHECKED));
	CheckMenuItem(hmenu, ID_SYNC_RENDERING, MF_BYCOMMAND | (g_sync ? MF_CHECKED:MF_UNCHECKED));
	CheckMenuItem(hmenu, ID_OPTIONS_FIXEDSTEP0, MF_BYCOMMAND | (dt==0.0f ? MF_CHECKED:MF_UNCHECKED));
	CheckMenuItem(hmenu, ID_OPTIONS_FIXEDSTEP1, MF_BYCOMMAND | (dt==0.01f ? MF_CHECKED:MF_UNCHECKED));
	CheckMenuItem(hmenu, ID_OPTIONS_FIXEDSTEP2, MF_BYCOMMAND | (dt==0.02f ? MF_CHECKED:MF_UNCHECKED));
	CheckMenuItem(hmenu, ID_OPTIONS_FIXEDSTEP3, MF_BYCOMMAND | (dt==g_stepCustom ? MF_CHECKED:MF_UNCHECKED));
	int numThreadsMax,numThreads=GetNumThreads(numThreadsMax);
	EnableMenuItem(hmenu, ID_OPTIONS_THREAD2, MF_BYCOMMAND | (numThreadsMax>=2 ? MF_ENABLED : MF_DISABLED));
	EnableMenuItem(hmenu, ID_OPTIONS_THREAD3, MF_BYCOMMAND | (numThreadsMax>=3 ? MF_ENABLED : MF_DISABLED));
	EnableMenuItem(hmenu, ID_OPTIONS_THREAD4, MF_BYCOMMAND | (numThreadsMax>=4 ? MF_ENABLED : MF_DISABLED));
	CheckMenuItem(hmenu, ID_OPTIONS_THREAD1, MF_BYCOMMAND | (numThreads==1 ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(hmenu, ID_OPTIONS_THREAD2, MF_BYCOMMAND | (numThreads==2 ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(hmenu, ID_OPTIONS_THREAD3, MF_BYCOMMAND | (numThreads==3 ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(hmenu, ID_OPTIONS_THREAD4, MF_BYCOMMAND | (numThreads==4 ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(hmenu, ID_FASTMOVEMENT, MF_BYCOMMAND | (g_bFast ? MF_CHECKED:MF_UNCHECKED));
	CheckMenuItem(hmenu, ID_SLOWMOVEMENT, MF_BYCOMMAND | (g_bFast ? MF_UNCHECKED:MF_CHECKED));
	CheckMenuItem(hmenu, ID_DISPLAYCOLLISIONS, MF_BYCOMMAND | (g_bShowColl ? MF_CHECKED:MF_UNCHECKED));
	CheckMenuItem(hmenu, ID_OPTIONS_DISPLAYBBOXES, MF_BYCOMMAND | (g_bShowBBoxes ? MF_CHECKED:MF_UNCHECKED));
	CheckMenuItem(hmenu, ID_DISPLAYPROFILER, MF_BYCOMMAND | ((g_bShowProfiler&1) ? MF_CHECKED:MF_UNCHECKED));
	CheckMenuItem(hmenu, ID_DISPLAYPROFILERENT, MF_BYCOMMAND | ((g_bShowProfiler&2) ? MF_CHECKED:MF_UNCHECKED));
}


BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	InitPhysics();

	g_hWnd = CreateWindowEx(0, szWindowClass, szTitle, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);
	//SetLayeredWindowAttributes(g_hWnd, 0, 255, LWA_ALPHA);

	if (!g_hWnd)
		return FALSE;

	ShowWindow(g_hWnd, nCmdShow);
	UpdateWindow(g_hWnd);
	UpdateMenuItems(g_hWnd);

	return TRUE;
}


LRESULT (*g_DefEditProc)(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK EditWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg==WM_KILLFOCUS	|| msg==WM_KEYDOWN && (wParam==VK_ESCAPE || wParam==VK_RETURN)) {
		if (msg==WM_KEYDOWN && wParam==VK_RETURN) {
			char str[64]; GetWindowText(hWnd,str,sizeof(str));
			SetCamPosTxt(str);
			InvalidateRect(g_hWnd,0,0);
		}
		DestroyWindow(hWnd);
		return 0;
	}	else if (msg==WM_DESTROY)
		g_hEdit = 0;
	return g_DefEditProc(hWnd, msg, wParam, lParam);
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent, trg=0;
	PAINTSTRUCT ps;
	RECT rect;
	float dx,dy;

	switch (message) {
		case WM_COMMAND:
			wmId    = LOWORD(wParam); 
			wmEvent = HIWORD(wParam); 
			// Parse the menu selections:
			switch (wmId) {
				case ID_ANIMATE: g_bAnimate^=1; UpdateMenuItems(hWnd); RenderWorld(hWnd,g_hDC); 
						 if (g_bAnimate) 
							 ReleaseMutex(g_hThreadActive);
						 else
							 WaitForSingleObject(g_hThreadActive,INFINITE);
						 break;
				case ID_SYNC_RENDERING: g_sync^=1; UpdateMenuItems(hWnd); break;
				case ID_OPTIONS_FIXEDSTEP0: SetStep(0); UpdateMenuItems(hWnd); break;
				case ID_OPTIONS_FIXEDSTEP1: SetStep(0.01f); UpdateMenuItems(hWnd); break;
				case ID_OPTIONS_FIXEDSTEP2: SetStep(0.02f); UpdateMenuItems(hWnd); break;
				case ID_OPTIONS_FIXEDSTEP3: SetStep(g_stepCustom); UpdateMenuItems(hWnd); break;
				case ID_FASTMOVEMENT: g_bFast=1; UpdateMenuItems(hWnd); break;
				case ID_SLOWMOVEMENT: g_bFast=0; UpdateMenuItems(hWnd); break;
				case ID_OPTIONS_VIEWDIST_INC: ChangeViewDist(+1); InvalidateRect(hWnd,0,0); break;
				case ID_OPTIONS_VIEWDIST_DEC: ChangeViewDist(-1); InvalidateRect(hWnd,0,0); break;
				case ID_OPTIONS_CAMPOS: {
					GetClientRect(hWnd,&rect);
					char str[64];	GetCamPosTxt(str);
					SIZE sz; GetTextExtentPoint(g_hDC, str,strlen(str), &sz);
					g_hEdit = CreateWindowEx(0,"EDIT","",WS_CHILD|WS_VISIBLE|ES_LEFT,rect.right-20-sz.cx,10,sz.cx+20,sz.cy,hWnd,0,GetModuleHandle(0),0);
					g_DefEditProc = (LRESULT(*)(HWND,UINT,WPARAM,LPARAM))GetWindowLongPtr(g_hEdit, GWLP_WNDPROC);
					SetWindowLongPtr(g_hEdit, GWLP_WNDPROC, (LONG_PTR)EditWndProc);
					SendMessage(g_hEdit, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);
					SetWindowText(g_hEdit, str);
					SendMessage(g_hEdit, EM_SETSEL, 0,-1);
					SetFocus(g_hEdit);
					break;
				}
				case ID_DISPLAYCOLLISIONS: g_bShowColl^=1; UpdateMenuItems(hWnd); RenderWorld(hWnd,g_hDC); break;
				case ID_OPTIONS_DISPLAYBBOXES: g_bShowBBoxes^=1; UpdateMenuItems(hWnd); RenderWorld(hWnd,g_hDC); break;
				case ID_DISPLAYPROFILER: g_bShowProfiler^=1; UpdateMenuItems(hWnd); RenderWorld(hWnd,g_hDC); break;
				case ID_DISPLAYPROFILERENT: g_bShowProfiler^=2; UpdateMenuItems(hWnd); RenderWorld(hWnd,g_hDC); break;
				//case ID_RELOADWORLD: ReloadWorld(g_arg[0]); RenderWorld(hWnd,g_hDC); break;
				case ID_RELOADWORLD1: ReloadWorld(g_arg[2]); RenderWorld(hWnd,g_hDC); break;
				case ID_RELOADGEOMETRIES: 
					ReloadWorldAndGeometries(g_arg[0],g_arg[1]); 
					UpdateMenuItems(hWnd);
					RenderWorld(hWnd,g_hDC); 
					break;
				case ID_SAVEWORLD: SaveWorld(g_arg[2]); break;
				case IDM_ABOUT:
					DialogBox(hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
					break;
				{
					case ID_OPTIONS_THREAD4: ++trg;
					case ID_OPTIONS_THREAD3: ++trg;
					case ID_OPTIONS_THREAD2: ++trg;
					case ID_OPTIONS_THREAD1:
					SetNumThreads(trg+1);	UpdateMenuItems(hWnd);
				} break;
				{
				case ID_CHOOSE_ENTITIES_SAVE:	++trg;
				case ID_CHOOSE_GEOMETRIES:    ++trg;
				case ID_CHOOSE_ENTITIES: 
					IFileDialog *pfd;	CoCreateInstance(trg==2 ? CLSID_FileSaveDialog:CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
					static const WCHAR *title[3] = { L"Select entities file", L"Select geometries file", L"Select save file" };
					static const COMDLG_FILTERSPEC ext[2] = { { L"All files (assumes text format)", L"*.*" }, { L"Binary dumps", L"*.phump" } };
					pfd->SetTitle(title[trg]);
					pfd->SetFileTypes(1+(trg!=2), ext);
					pfd->SetDefaultExtension(L"txt");
					WCHAR path[MAX_PATH], *fname;
					if (strlen(g_arg[trg])) {
						path[MultiByteToWideChar(CP_UTF8,0,g_arg[trg],strlen(g_arg[trg]),path,sizeof(path)/sizeof(path[0])-1)] = 0;
						pfd->SetFileName(fname = PathFindFileNameW(path));
						*fname = 0;
						IShellItem *pfolder; SHCreateItemFromParsingName(path,0,IID_PPV_ARGS(&pfolder));
						pfd->SetFolder(pfolder);
						pfolder->Release();
					}
					if (SUCCEEDED(pfd->Show(NULL))) {
						IShellItem *fnew; pfd->GetResult(&fnew);
						fnew->GetDisplayName(SIGDN_FILESYSPATH, &fname);
						WideCharToMultiByte(CP_UTF8,0,fname,-1,g_arg[trg],sizeof(g_arg[trg])-1,0,0);
						fnew->Release();
					}
					pfd->Release();
					if (trg!=2 && !_stricoll(g_arg[trg]+strlen(g_arg[trg])-6,".phump"))	{
						if (!*g_arg[trg^1])
							strcpy(g_arg[trg^1], g_arg[trg]);
						SendMessage(hWnd, WM_COMMAND, ID_RELOADGEOMETRIES, 0);
					}
					UpdateMenuItems(hWnd);
				}	break;
				case IDM_EXIT:
					DestroyWindow(hWnd);
					break;
				default:
					return DefWindowProc(hWnd, message, wParam, lParam);
			}
			break;

		case WM_CREATE: {
			g_hDC = GetDC(hWnd);
			if (!SetupPixelFormat(g_hDC)) return -1;
			if (!(g_glrc=wglCreateContext(g_hDC)))
				MessageBox(0,"wglCreateContext failed",0,0);
			else if (!wglMakeCurrent(g_hDC,g_glrc))
				MessageBox(0,"wglMakeCurrent failed",0,0);
			else {
				SendMessage(hWnd,WM_COMMAND,ID_RELOADGEOMETRIES,0);
				return 0;
			}
			return -1;
		}

		case WM_PAINT:
			BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
			RenderWorld(hWnd,g_hDC);
			break;

		case WM_RBUTTONDOWN:
			if (GetCapture()!=hWnd) {
				if (GetKeyState(VK_SHIFT)<0)
					OnMouseEvent(message, lParam&0xffff, lParam>>16, wParam);
				else {
					GetClientRect(hWnd, &rect);
					rect.left = rect.left+rect.right>>1; rect.top = rect.top+rect.bottom>>1;
					ClientToScreen(hWnd,(LPPOINT)&rect);
					SetCursorPos(rect.left,rect.top);
					SetCapture(hWnd);	g_moveMode = 1;
				}
			}
			break;

		case WM_LBUTTONDOWN:
			if (GetCapture()!=hWnd) {
				SetCapture(hWnd);	g_moveMode = 0;
				OnMouseEvent(message, lParam&0xffff, lParam>>16, wParam);
			}
			break;

		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
			OnMouseEvent(message, lParam&0xffff, lParam>>16, wParam);
		case WM_CANCELMODE:
			if (GetCapture()==hWnd)	{
				ReleaseCapture();
				InvalidateRect(hWnd,0,0);
			}
			break;

		case WM_MOUSEMOVE:
			if (GetCapture()==hWnd) {
				GetClientRect(hWnd, &rect);
				dx = (float)((short)LOWORD(lParam)-(rect.left+rect.right>>1))/rect.bottom;
				dy = (float)((short)HIWORD(lParam)-(rect.top+rect.bottom>>1))/rect.bottom;
				if (g_moveMode==0)
					OnMouseEvent(message, lParam&0xffff, lParam>>16, wParam);
				else if (dx*dx+dy*dy>0) {
					OnCameraRotate(dx,dy);
					rect.left = rect.left+rect.right>>1; rect.top = rect.top+rect.bottom>>1;
					ClientToScreen(hWnd,(LPPOINT)&rect);
					SetCursorPos(rect.left,rect.top);
				}	else break;
				InvalidateRect(hWnd,0,0);
			}
			break;

		case WM_KEYDOWN:
			OnSetCursor();
			switch (wParam) {
				case VK_SPACE: if (!g_bAnimate) EvolveWorld(); break;
				case VK_BACK: if (!g_bAnimate) EvolveWorld(-1); break;
				case VK_DOWN: if (g_bShowProfiler) SelectNextProfVisInfo(); break;
				case VK_UP: if (g_bShowProfiler) SelectPrevProfVisInfo(); break;
				case VK_LEFT: if (g_bShowProfiler) ExpandProfVisInfo(0); break;
				case VK_RIGHT: if (g_bShowProfiler) ExpandProfVisInfo(1); break;
				default: return DefWindowProc(hWnd, message, wParam, lParam);
			}
			RenderWorld(hWnd,g_hDC);
			break;

		case WM_ERASEBKGND:
			return TRUE;

		case WM_SIZE:
			if (g_hEdit)
				DestroyWindow(g_hEdit);
			return DefWindowProc(hWnd, message, wParam, lParam);

		case WM_DESTROY:
			ShutdownPhysics();
			ReleaseDC(hWnd, g_hDC);
			wglDeleteContext(g_glrc);
			PostQuitMessage(0);
			break;

		case WM_SETCURSOR:
			if (LOWORD(lParam)==HTCLIENT)	{
				OnSetCursor();
				break;
		case WM_KEYUP:
			OnSetCursor();
			}
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
		{
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}

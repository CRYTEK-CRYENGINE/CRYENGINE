// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
// Description: EyeTracker for Windows (covering EyeX)
// - 20/04/2016 Created by Benjamin Peters

#include "StdAfx.h"

#ifdef CRY_PLATFORM_WINDOWS
#ifdef USE_EYETRACKER


#include "EyeTrackerInput.h"
#include "eyex/EyeX.h"

#pragma comment (lib, "Tobii.EyeX.Client.lib")

#define LOG(msg) CryLogAlways((std::string("[EYEX] ") + msg + "\n").c_str())

static CEyeTrackerInput* g_pEyeTrackerInstance = NULL;
static const TX_STRING InteractorId = "CRYENGINE";
static TX_CONTEXTHANDLE g_hContext = TX_EMPTY_HANDLE;
static TX_HANDLE g_hGlobalInteractorSnapshot = TX_EMPTY_HANDLE;
static SInputSymbol* g_pSymbolXAxis;
static SInputSymbol* g_pSymbolYAxis;

void InitializeEyeX();
void DeinitializeEyeX();


CEyeTrackerInput::CEyeTrackerInput() 
	: m_bProjectionChanged(false)
	, m_fEyeX(0)
	, m_fEyeY(0)
{
	g_pEyeTrackerInstance = this;

	g_pSymbolXAxis = new SInputSymbol(0, eKI_EyeTracker_X, "EyeTracker_X", SInputSymbol::Axis);
	g_pSymbolXAxis->deviceType = eIDT_EyeTracker;
	g_pSymbolXAxis->state = eIS_Unknown;
	g_pSymbolXAxis->user = 0;

	g_pSymbolYAxis = new SInputSymbol(1, eKI_EyeTracker_Y, "EyeTracker_Y", SInputSymbol::Axis);
	g_pSymbolYAxis->deviceType = eIDT_EyeTracker;
	g_pSymbolYAxis->state = eIS_Unknown;
	g_pSymbolYAxis->user = 0;
}


CEyeTrackerInput::~CEyeTrackerInput()
{
	DeinitializeEyeX();
	g_pEyeTrackerInstance = NULL;
}


bool CEyeTrackerInput::Init()	
{ 
	InitializeEyeX();
	return false; 
}


void CEyeTrackerInput::Update()
{
	if (m_bProjectionChanged)
	{
		SInputEvent event;
		event.deviceIndex = 0;
		event.deviceType = eIDT_EyeTracker;

		g_pSymbolXAxis->ChangeEvent(m_fEyeX);
		g_pSymbolXAxis->AssignTo(event);
		if (gEnv->pInput->OnFilterInputEvent(&event))
		{
			gEnv->pInput->PostInputEvent(event);
		}

		g_pSymbolYAxis->ChangeEvent(m_fEyeY);
		g_pSymbolYAxis->AssignTo(event);
		if (gEnv->pInput->OnFilterInputEvent(&event))
		{
			gEnv->pInput->PostInputEvent(event);
		}

		m_bProjectionChanged = false;
	}
}


void CEyeTrackerInput::SetEyeProjection(int iX, int iY)
{
	m_bProjectionChanged = true;

	if (!gEnv->IsEditor())
	{
		// Modify to match CE Window dimensions.
		HWND hWnd = (HWND)gEnv->pSystem->GetHWND();
		RECT windowRect;
		GetWindowRect(hWnd, &windowRect);
		m_fEyeX = (float)(iX - windowRect.left) / (float)(windowRect.right - windowRect.left);
		m_fEyeY = (float)(iY - windowRect.top) / (float)(windowRect.bottom - windowRect.top);
	}
	else
	{
		// Leave input raw if in editor mode. Editor will project to viewport itself.
		m_fEyeX = (float)iX;
		m_fEyeY = (float)iY;
	}
}


/*
* Initializes g_hGlobalInteractorSnapshot with an interactor that has the Gaze Point behavior.
*/
BOOL InitializeGlobalInteractorSnapshot(TX_CONTEXTHANDLE hContext)
{
	TX_HANDLE hInteractor = TX_EMPTY_HANDLE;
	TX_GAZEPOINTDATAPARAMS params = { TX_GAZEPOINTDATAMODE_LIGHTLYFILTERED };
	BOOL success;

	success = txCreateGlobalInteractorSnapshot(
		hContext,
		InteractorId,
		&g_hGlobalInteractorSnapshot,
		&hInteractor) == TX_RESULT_OK;
	success &= txCreateGazePointDataBehavior(hInteractor, &params) == TX_RESULT_OK;

	txReleaseObject(&hInteractor);

	return success;
}


/*
* Callback function invoked when a snapshot has been committed.
*/
void TX_CALLCONVENTION OnSnapshotCommitted(TX_CONSTHANDLE hAsyncData, TX_USERPARAM param)
{
	// check the result code using an assertion.
	// this will catch validation errors and runtime errors in debug builds. in release builds it won't do anything.

	TX_RESULT result = TX_RESULT_UNKNOWN;
	txGetAsyncDataResultCode(hAsyncData, &result);
	assert(result == TX_RESULT_OK || result == TX_RESULT_CANCELLED);
}


/*
* Callback function invoked when the status of the connection to the EyeX Engine has changed.
*/
void TX_CALLCONVENTION OnEngineConnectionStateChanged(TX_CONNECTIONSTATE connectionState, TX_USERPARAM userParam)
{
	switch (connectionState) 
	{
		case TX_CONNECTIONSTATE_CONNECTED: 
		{
			BOOL success;
			LOG("The connection state is now CONNECTED (We are connected to the EyeX Engine)");
			// commit the snapshot with the global interactor as soon as the connection to the engine is established.
			// (it cannot be done earlier because committing means "send to the engine".)
			success = txCommitSnapshotAsync(g_hGlobalInteractorSnapshot, OnSnapshotCommitted, NULL) == TX_RESULT_OK;
			if (!success)
				LOG("Failed to initialize the data stream.");
			else
				LOG("Waiting for gaze data to start streaming...");
		}
		break;

	case TX_CONNECTIONSTATE_DISCONNECTED:
		LOG("The connection state is now DISCONNECTED (We are disconnected from the EyeX Engine)");
		break;

	case TX_CONNECTIONSTATE_TRYINGTOCONNECT:
		LOG("The connection state is now TRYINGTOCONNECT (We are trying to connect to the EyeX Engine)");
		break;

	case TX_CONNECTIONSTATE_SERVERVERSIONTOOLOW:
		LOG("The connection state is now SERVER_VERSION_TOO_LOW: this application requires a more recent version of the EyeX Engine to run.");
		break;

	case TX_CONNECTIONSTATE_SERVERVERSIONTOOHIGH:
		LOG("The connection state is now SERVER_VERSION_TOO_HIGH: this application requires an older version of the EyeX Engine to run.");
		break;
	}
}


/*
* Handles an event from the Gaze Point data stream.
*/
void OnGazeDataEvent(TX_HANDLE hGazeDataBehavior)
{
	TX_GAZEPOINTDATAEVENTPARAMS eventParams;
	if (txGetGazePointDataEventParams(hGazeDataBehavior, &eventParams) == TX_RESULT_OK) 
	{
		g_pEyeTrackerInstance->SetEyeProjection((int)eventParams.X, (int)eventParams.Y);
	}
	else 
	{
		LOG("Failed to interpret gaze data event packet.");
	}
}


/*
* Callback function invoked when an event has been received from the EyeX Engine.
*/
void TX_CALLCONVENTION HandleEvent(TX_CONSTHANDLE hAsyncData, TX_USERPARAM userParam)
{
	TX_HANDLE hEvent = TX_EMPTY_HANDLE;
	TX_HANDLE hBehavior = TX_EMPTY_HANDLE;

	txGetAsyncDataContent(hAsyncData, &hEvent);

	// NOTE. Uncomment the following line of code to view the event object. The same function can be used with any interaction object.
	//OutputDebugStringA(txDebugObject(hEvent));

	if (txGetEventBehavior(hEvent, &hBehavior, TX_BEHAVIORTYPE_GAZEPOINTDATA) == TX_RESULT_OK) 
	{
		OnGazeDataEvent(hBehavior);
		txReleaseObject(&hBehavior);
	}

	// NOTE since this is a very simple application with a single interactor and a single data stream, 
	// our event handling code can be very simple too. A more complex application would typically have to 
	// check for multiple behaviors and route events based on interactor IDs.

	txReleaseObject(&hEvent);
}


void InitializeEyeX()
{
	TX_TICKET hConnectionStateChangedTicket = TX_INVALID_TICKET;
	TX_TICKET hEventHandlerTicket = TX_INVALID_TICKET;

	// initialize and enable the context that is our link to the EyeX Engine.
	BOOL success = txInitializeEyeX(TX_EYEXCOMPONENTOVERRIDEFLAG_NONE, NULL, NULL, NULL, NULL) == TX_RESULT_OK;
	success &= txCreateContext(&g_hContext, TX_FALSE) == TX_RESULT_OK;
	success &= InitializeGlobalInteractorSnapshot(g_hContext);
	success &= txRegisterConnectionStateChangedHandler(g_hContext, &hConnectionStateChangedTicket, OnEngineConnectionStateChanged, NULL) == TX_RESULT_OK;
	success &= txRegisterEventHandler(g_hContext, &hEventHandlerTicket, HandleEvent, NULL) == TX_RESULT_OK;
	success &= txEnableConnection(g_hContext) == TX_RESULT_OK;

	// let the events flow until a key is pressed.
	if (success)
	{
		LOG("Initialization successful.");
	}
	else
	{
		LOG("Initialization failed.");
	}
}


void DeinitializeEyeX()
{
	// disable and delete the context.
	txDisableConnection(g_hContext);
	txReleaseObject(&g_hGlobalInteractorSnapshot);
	BOOL success = txShutdownContext(g_hContext, TX_CLEANUPTIMEOUT_DEFAULT, TX_FALSE) == TX_RESULT_OK;
	success &= txReleaseContext(&g_hContext) == TX_RESULT_OK;
	success &= txUninitializeEyeX() == TX_RESULT_OK;
	if (!success)
	{
		LOG("Could not be shut down cleanly.");
	}
	else
	{
		LOG("Deitialization successful.");
	}
}

#endif //USE_EYETRACKER
#endif //CRY_PLATFORM_WINDOWS

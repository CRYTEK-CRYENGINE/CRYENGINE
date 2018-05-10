%include "CryEngine.swig"

%import "CryCommon.i"

%import <CryAction.i>
%import <CryAISystem.i>
%import <CryAnimation.i>
%import <CryAudio.i>
%import <Cry3DEngine.i>
%import <CryDynamicResponseSystem.i>
%import <CryEntitySystem.i>
%import <CryFont.i>
%import <CryInput.i>
%import <CryMonoBridge.i>
%import <CryLobby.i>
%import <CryMovie.i>
%import <CryNetwork.i>
%import <CryPhysics.i>
%import <CryRender.i>
%import <CryScriptSystem.i>

%{
#include <CrySystem/File/IFileChangeMonitor.h>
#include <CrySystem/Profilers/IStatoscope.h>
#include <CrySystem/Profilers/ProfileLog.h>
#include <CrySystem/Profilers/IPerfHud.h>
#include <CryMemory/ILocalMemoryUsage.h>
#include <CrySystem/ICodeCheckpointMgr.h>
#include <CrySystem/IOverloadSceneManager.h>
#include <CryNetwork/IServiceNetwork.h>
#include <CryNetwork/IRemoteCommand.h>
#include <CrySystem/File/IAVI_Reader.h>
#include <CrySystem/IBudgetingSystem.h>
#include <CrySystem/VR/IHMDManager.h>
#include <CrySystem/VR/IHMDDevice.h>
#include <CryCore/Platform/IPlatformOS.h>
#include <CrySystem/Profilers/IDiskProfiler.h>
#include <CrySystem/File/IResourceManager.h>
#include <CrySystem/IResourceCollector.h>
#include <CrySystem/ZLib/ILZ4Decompressor.h>
#include <CryNetwork/INotificationNetwork.h>
#include <CrySystem/XML/IReadWriteXMLSink.h>
#include <CrySystem/ITestSystem.h>
#include <CrySystem/ITextModeConsole.h>
#include <CrySystem/IValidator.h>
#include <CrySystem/ZLib/IZLibCompressor.h>
#include <CrySystem/ZLib/IZlibDecompressor.h>
#include <CrySystem/ICryMiniGUI.h>
#include <CrySystem/ICmdLine.h>
#include <CryThreading/IThreadManager.h>
#include <CryThreading/IJobManager.h>

namespace minigui { class CDrawContext{ public: virtual ~CDrawContext() {} }; }

using JobManager::SJobStateBase;
%}
%feature("director") ILogCallback;
%include "../../../../CryEngine/CryCommon/CrySystem/ILog.h"
%feature("director") ISystemEventListener;
%feature("director") ILoadingProgressListener;
%ignore CreateSystemInterface;
%include "../../../../CryEngine/CryCommon/CrySystem/ISystem.h"
%typemap(csbase) ICryPak::EPathResolutionRules "long"
%include "../../../../CryEngine/CryCommon/CrySystem/File/ICryPak.h"
%include "../../../../CryEngine/CryCommon/CrySystem/ICmdLine.h"
%typemap(csbase) EVarFlags "uint"
%feature("director") IRemoteConsoleListener;
%include "../../../../CryEngine/CryCommon/CryCore/SFunctor.h"
%typemap(cscode) ICVar %{
	public static System.IntPtr GetIntPtr(ICVar icvar)
	{
		return getCPtr(icvar).Handle;
	}
%}
// ensures ICVar can only be unregistered through CryEngine.ConsoleVariable.UnRegister in C#
%ignore ICVar::Release();

// hides methods that cannot be accessed in C#
%ignore ICVar::AddOnChange(const CallbackFunction &);
%ignore ICVar::SetOnChangeCallback(ConsoleVarFunc);
%ignore ICVar::GetOnChangeCallback() const;
%include "../../../../CryEngine/CryCommon/CrySystem/IConsole.h"

%include "../../../../CryEngine/CryCommon/CrySystem/ITimer.h"
%include "../../../../CryEngine/CryCommon/CrySystem/File/IFileChangeMonitor.h"
%include "../../../../CryEngine/CryCommon/CrySystem/Profilers/IStatoscope.h"
%include "../../../../CryEngine/CryCommon/CrySystem/Profilers/ProfileLog.h"
%include "../../../../CryEngine/CryCommon/CrySystem/Profilers/IPerfHud.h"
%include "../../../../CryEngine/CryCommon/CrySystem/Profilers/FrameProfiler/FrameProfiler.h"
%include "../../../../CryEngine/CryCommon/CryMemory/ILocalMemoryUsage.h"
%include "../../../../CryEngine/CryCommon/CrySystem/ICodeCheckpointMgr.h"
%ignore JobManager::SInfoBlock;
%ignore JobManager::SJobQueuePos;
%ignore InvokeOnLinkedStack;
%ignore GetJobManagerInterface;
%include "../../../../CryEngine/CryCommon/CryThreading/IJobManager.h"
%include "../../../../CryEngine/CryCommon/CrySystem/IOverloadSceneManager.h"
%include "../../../../CryEngine/CryCommon/CryNetwork/IServiceNetwork.h"
%include "../../../../CryEngine/CryCommon/CryNetwork/IRemoteCommand.h"
%include "../../../../CryEngine/CryCommon/CrySystem/File/IAVI_Reader.h"
%include "../../../../CryEngine/CryCommon/CrySystem/IBudgetingSystem.h"
%include "../../../../CryEngine/CryCommon/CrySystem/VR/IHMDManager.h"
%include "../../../../CryEngine/CryCommon/CrySystem/VR/IHMDDevice.h"
%ignore IPlatformOS::Create;
%include "../../../../CryEngine/CryCommon/CryCore/Platform/IPlatformOS.h"
%include "../../../../CryEngine/CryCommon/CrySystem/Profilers/IDiskProfiler.h"
%include "../../../../CryEngine/CryCommon/CrySystem/File/IResourceManager.h"
%include "../../../../CryEngine/CryCommon/CrySystem/IResourceCollector.h"
%include "../../../../CryEngine/CryCommon/CrySystem/ILocalizationManager.h"
%include "../../../../CryEngine/CryCommon/CrySystem/ZLib/ILZ4Decompressor.h"
%include "../../../../CryEngine/CryCommon/CryNetwork/INotificationNetwork.h"
%include "../../../../CryEngine/CryCommon/CrySystem/XML/IReadWriteXMLSink.h"
%include "../../../../CryEngine/CryCommon/CrySystem/ITestSystem.h"
%include "../../../../CryEngine/CryCommon/CrySystem/ITextModeConsole.h"
%include "../../../../CryEngine/CryCommon/CryThreading/IThreadManager.h"
%include "../../../../CryEngine/CryCommon/CrySystem/IValidator.h"
//%include "../../../CryEngine/CryCommon/IWindowMessageHandler.h"
%include "../../../../CryEngine/CryCommon/CrySystem/ZLib/IZLibCompressor.h"
%include "../../../../CryEngine/CryCommon/CrySystem/ZLib/IZlibDecompressor.h"
%include "../../../../CryEngine/CryCommon/CrySystem/ICryMiniGUI.h"

%extend ICryPak
{
	_finddata_t* FindAllocateData()
	{
		return new _finddata_t();
	}

	void FindFreeData(_finddata_t* fd)
	{
		delete fd;
	}

	bool FindIsResultValid(intptr_t result)
	{
		return result != -1;
	}

	char* FindDataGetPath(_finddata_t* fd)
	{
		if (fd == nullptr)
		{
			return nullptr;
		}

		return fd->name;
	}
}

namespace minigui { class CDrawContext{}; }
// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"


#include <time.h>

#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/ITimeOfDay.h>
#include <CryFlowGraph/IFlowBaseNode.h>

//////////////////////////////////////////////////////////////////////////
class CFlowTimeNode : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum EInputs
	{
		IN_PAUSED
	};
	enum EOutputs
	{
		OUT_SECONDS,
		OUT_TICK,
	};
	CFlowTimeNode(SActivationInfo* pActInfo)
	{
		//		pActInfo->pGraph->SetRegularlyUpdated( pActInfo->myID, true );
	}
	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>("paused", false, _HELP("When set to true will pause time output")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("seconds", _HELP("Outputs the current time in seconds.")),
			OutputPortConfig_Void("tick",      _HELP("Outputs event at this port every frame.")),
			{ 0 }
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Update:
			{
				bool bPaused = GetPortBool(pActInfo, IN_PAUSED);
				if (!bPaused)
				{
					float fSeconds = gEnv->pTimer->GetFrameStartTime().GetSeconds();
					ActivateOutput(pActInfo, OUT_SECONDS, fSeconds);
					ActivateOutput(pActInfo, OUT_TICK, 0);
				}
			}
			break;
		case eFE_Initialize:
			{
				bool bPaused = GetPortBool(pActInfo, IN_PAUSED);
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, !bPaused);
				break;
			}
		case eFE_Activate:
			if (IsPortActive(pActInfo, IN_PAUSED))
			{
				bool bPaused = GetPortBool(pActInfo, IN_PAUSED);
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, !bPaused);
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_TimeOfDay_LoadDefinitionFile : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum EInputs
	{
		EIP_Load = 0,
		EIP_FileName,
	};
	enum EOutputs
	{
		EOP_Success = 0,
		EOP_Fail,
	};

	CFlowNode_TimeOfDay_LoadDefinitionFile(SActivationInfo* pActInfo) {}

	~CFlowNode_TimeOfDay_LoadDefinitionFile() {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Load",        _HELP("Triggers the TOD file to be read")),
			InputPortConfig<string>("FileName", _HELP("Name of the xml file to be read (must be in level folder)"),_HELP("Filename")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Success", _HELP("Triggered when file has successfully loaded")),
			OutputPortConfig_Void("Fail",    _HELP("Triggered if file was not successfully loaded")),
			{ 0 }
		};

		config.sDescription = _HELP("Loads a TOD (Time of Day definition file)");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			break;
		case eFE_Activate:
			{
				if (!IsPortActive(pActInfo, EIP_Load))
					return;

				// get the file name
				string pathAndfileName;
				const string fileName = GetPortString(pActInfo, EIP_FileName);
				if (fileName.empty())
					return;

				ILevelInfo* pCurrentLevel = gEnv->pGameFramework->GetILevelSystem()->GetCurrentLevel();
				string path = pCurrentLevel ? pCurrentLevel->GetPath() : "";
				pathAndfileName.Format("%s/%s", path.c_str(), fileName.c_str());

				// try to load it
				XmlNodeRef root = GetISystem()->LoadXmlFromFile(pathAndfileName);
				if (root == NULL)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "FlowGraph: TimeOfDay Loading Node: Could not load tod file %s. Aborting.", pathAndfileName.c_str());
					ActivateOutput(pActInfo, EOP_Fail, true);
					return;
				}

				// get the TimeofDay interface
				ITimeOfDay* pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
				if (pTimeOfDay == NULL)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "FlowGraph: TimeOfDay Loading Node: Could not obtain ITimeOfDay interface from engine. Aborting.");
					ActivateOutput(pActInfo, EOP_Fail, true);
					return;
				}

				// try to serialize from that file
				pTimeOfDay->Serialize(root, true);
				pTimeOfDay->Update(true, true);

				ActivateOutput(pActInfo, EOP_Success, true);
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

};

//////////////////////////////////////////////////////////////////////////
// Time of day node.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_RealTime : public CFlowBaseNode<eNCT_Instanced>
{
public:
	enum Inputs
	{
		FORCE_UPDATE
	};
	enum Outputs
	{
		OUT_YEAR,
		OUT_MONTH,
		OUT_DAY,
		OUT_HOURS,
		OUT_MINUTES,
		OUT_SECONDS,
	};
	CFlowNode_RealTime(SActivationInfo* pActInfo)
	{
		memset(&m_lasttime, 0, sizeof(m_lasttime));
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_RealTime(pActInfo);
	}

	virtual void Serialize(SActivationInfo*, TSerialize ser)
	{
		if (ser.IsReading())
		{
			// forces output on loading
			m_lasttime.tm_hour = -1;
			m_lasttime.tm_min = -1;
			m_lasttime.tm_sec = -1;
		}
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("force_update"),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("year",  "Current Year"),
			OutputPortConfig<int>("month", "Current Month [1..12]"),
			OutputPortConfig<int>("day",   "Current Day of the Month [1..31]"),
			OutputPortConfig<int>("hours"),
			OutputPortConfig<int>("minutes"),
			OutputPortConfig<int>("seconds"),
			{ 0 }
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
			break;
		case eFE_Update:
			{
				Update(pActInfo, false);
			}
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, FORCE_UPDATE))
				{
					Update(pActInfo, true);
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
private:
	void Update(SActivationInfo* pActInfo, bool forceUpdate)
	{
		time_t long_time = time(NULL);
		tm* newtime = localtime(&long_time);

		if ((forceUpdate == true) || (newtime->tm_mday != m_lasttime.tm_mday))
			ActivateOutput(pActInfo, OUT_DAY, newtime->tm_mday);
		if ((forceUpdate == true) || (newtime->tm_mon != m_lasttime.tm_mon))
			ActivateOutput(pActInfo, OUT_MONTH, newtime->tm_mon + 1);
		if ((forceUpdate == true) || (newtime->tm_year != m_lasttime.tm_year))
			ActivateOutput(pActInfo, OUT_YEAR, newtime->tm_year + 1900);

		if ((forceUpdate == true) || (newtime->tm_hour != m_lasttime.tm_hour))
			ActivateOutput(pActInfo, OUT_HOURS, newtime->tm_hour);
		if ((forceUpdate == true) || (newtime->tm_min != m_lasttime.tm_min))
			ActivateOutput(pActInfo, OUT_MINUTES, newtime->tm_min);
		if ((forceUpdate == true) || (newtime->tm_sec != m_lasttime.tm_sec))
			ActivateOutput(pActInfo, OUT_SECONDS, newtime->tm_sec);

		m_lasttime = *newtime;
	}

	tm m_lasttime;
};

//////////////////////////////////////////////////////////////////////////
// Timer node.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_Timer : public CFlowBaseNode<eNCT_Instanced>
{
public:
	enum EInputs
	{
		IN_PERIOD,
		IN_MIN,
		IN_MAX,
		IN_PAUSED
	};
	enum EOutputs
	{
		OUT_OUT,
	};
	CFlowNode_Timer(SActivationInfo* pActInfo)
	{
		pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
		m_nCurrentCount = -100000;
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_Timer(pActInfo);
	}

	virtual void Serialize(SActivationInfo*, TSerialize ser)
	{
		ser.BeginGroup("Local");
		ser.Value("m_last", m_last);
		ser.Value("m_nCurrentCount", m_nCurrentCount);
		ser.EndGroup();
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("period", _HELP("Tick period in seconds")),
			InputPortConfig<int>("min",      _HELP("Minimal timer output value")),
			InputPortConfig<int>("max",      _HELP("Maximal timer output value")),
			InputPortConfig<bool>("paused",  _HELP("Timer will be paused when set to true")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("out"),
			{ 0 }
		};

		config.sDescription = _HELP("Timer node will output count from min to max, ticking at the specified period of time");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				bool bPaused = GetPortBool(pActInfo, IN_PAUSED);
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, !bPaused);
				m_last = gEnv->pTimer->GetFrameStartTime();
			}
			break;

		case eFE_Update:
			{
				if (GetPortBool(pActInfo, IN_PAUSED))
					return;

				float fPeriod = GetPortFloat(pActInfo, IN_PERIOD);
				CTimeValue time = gEnv->pTimer->GetFrameStartTime();
				CTimeValue dt = time - m_last;
				if (dt.GetSeconds() >= fPeriod)
				{
					m_last = time;
					int nMin = GetPortInt(pActInfo, IN_MIN);
					int nMax = GetPortInt(pActInfo, IN_MAX);
					m_nCurrentCount++;
					if (m_nCurrentCount < nMin)
						m_nCurrentCount = nMin;
					if (m_nCurrentCount > nMax)
					{
						m_nCurrentCount = nMin;
					}
					ActivateOutput(pActInfo, OUT_OUT, m_nCurrentCount);
				}
			}
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, IN_PAUSED))
				{
					bool bPaused = GetPortBool(pActInfo, IN_PAUSED);
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, !bPaused);
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
private:
	CTimeValue m_last;
	int        m_nCurrentCount;
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_TimedCounter : public CFlowBaseNode<eNCT_Instanced>
{
public:
	enum EInputs
	{
		IN_START = 0,
		IN_STOP,
		IN_CONTINUE,
		IN_PERIOD,
		IN_LIMIT
	};
	enum EOutputs
	{
		OUT_FINISHED = 0,
		OUT_COUNT
	};
	CFlowNode_TimedCounter(SActivationInfo* pActInfo) : m_count(0)
	{
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_TimedCounter(pActInfo);
	}

	virtual void Serialize(SActivationInfo*, TSerialize ser)
	{
		ser.BeginGroup("Local");
		ser.Value("lastTickTime", m_lastTickTime);
		ser.Value("count", m_count);
		ser.EndGroup();
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_AnyType("start",    _HELP("Triggers the start of the counter. If it is counting already, it resets it. Also, wathever was input here, will be output as 'finished' when the counting is done")),
			InputPortConfig_AnyType("stop",     _HELP("Stops counting")),
			InputPortConfig_AnyType("continue", _HELP("Continues counting")),
			InputPortConfig<float>("period",    1.f,                                                                                                                                                                        _HELP("Tick period in seconds")),
			InputPortConfig<int>("limit",       _HELP("How many ticks the counter will do before finish")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_AnyType("finished", _HELP("triggered when the counting is done. The value will be the same than the 'start' input")),
			OutputPortConfig<int>("count",       _HELP("outputs the tick counter value")),
			{ 0 }
		};

		config.sDescription = _HELP("counts a number of ticks specified by 'limit', and then outputs 'finished' with the value the input 'start' had when it was triggered");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			}
			break;

		case eFE_Update:
			{
				float fPeriod = GetPortFloat(pActInfo, IN_PERIOD);
				CTimeValue time = gEnv->pTimer->GetFrameStartTime();
				CTimeValue dt = time - m_lastTickTime;
				if (dt.GetSeconds() >= fPeriod)
				{
					m_lastTickTime = time;
					++m_count;
					ActivateOutput(pActInfo, OUT_COUNT, m_count);
					if (m_count >= GetPortInt(pActInfo, IN_LIMIT))
					{
						pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
						ActivateOutput(pActInfo, OUT_FINISHED, GetPortAny(pActInfo, IN_START));
					}
				}
			}
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, IN_START))
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
					m_count = 0;
					m_lastTickTime = gEnv->pTimer->GetFrameStartTime();
				}

				if (IsPortActive(pActInfo, IN_STOP))
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				}

				if (IsPortActive(pActInfo, IN_CONTINUE))
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
private:
	CTimeValue m_lastTickTime;
	int        m_count;
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_TimeOfDay : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum EInputs
	{
		IN_TIME = 0,
		IN_SET,
		IN_FORCEUPDATE,
		IN_GET,
		IN_SPEED,
		IN_SET_SPEED,
		IN_GET_SPEED
	};
	enum EOutputs
	{
		OUT_CURTIME = 0,
		OUT_CURSPEED
	};

	CFlowNode_TimeOfDay(SActivationInfo* /* pActInfo */) {}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("Time",       0.0f,                                                   _HELP("TimeOfDay to set (in hours 0-24)")),
			InputPortConfig_Void("Set",          _HELP("Trigger to Set TimeOfDay to [Time]"),            _HELP("SetTime")),
			InputPortConfig<bool>("ForceUpdate", false,                                                  _HELP("Force Immediate Update of Sky if true. Only in Special Cases!")),
			InputPortConfig_Void("Get",          _HELP("Trigger to Get TimeOfDay to [CurTime]"),         _HELP("GetTime")),
			InputPortConfig<float>("Speed",      1.0f,                                                   _HELP("Speed to bet set (via [SetSpeed]")),
			InputPortConfig_Void("SetSpeed",     _HELP("Trigger to set TimeOfDay Speed to [Speed]")),
			InputPortConfig_Void("GetSpeed",     _HELP("Trigger to get TimeOfDay Speed to [CurSpeed]")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>("CurTime",  _HELP("Current TimeOfDay (set when [GetTime] is triggered)")),
			OutputPortConfig<float>("CurSpeed", _HELP("Current TimeOfDay Speed (set when [GetSpeed] is triggered)")),
			{ 0 }
		};

		config.sDescription = _HELP("Set/Get TimeOfDay and Speed");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				ITimeOfDay* pTOD = gEnv->p3DEngine->GetTimeOfDay();
				if (pTOD == 0)
					return;

				if (IsPortActive(pActInfo, IN_SET))
				{
					const bool bForceUpdate = GetPortBool(pActInfo, IN_FORCEUPDATE);
					pTOD->SetTime(GetPortFloat(pActInfo, IN_TIME), bForceUpdate);
					ActivateOutput(pActInfo, OUT_CURTIME, pTOD->GetTime());
				}
				if (IsPortActive(pActInfo, IN_GET))
				{
					ActivateOutput(pActInfo, OUT_CURTIME, pTOD->GetTime());
				}
				if (IsPortActive(pActInfo, IN_SET_SPEED))
				{
					ITimeOfDay::SAdvancedInfo info;
					pTOD->GetAdvancedInfo(info);
					info.fAnimSpeed = GetPortFloat(pActInfo, IN_SPEED);
					pTOD->SetAdvancedInfo(info);
					ActivateOutput(pActInfo, OUT_CURSPEED, info.fAnimSpeed);
				}
				if (IsPortActive(pActInfo, IN_GET_SPEED))
				{
					ITimeOfDay::SAdvancedInfo info;
					pTOD->GetAdvancedInfo(info);
					ActivateOutput(pActInfo, OUT_CURSPEED, info.fAnimSpeed);
				}
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// Timer node.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_MeasureTime : public CFlowBaseNode<eNCT_Instanced>
{
public:
	enum EInputs
	{
		EIP_Start = 0,
		EIP_Stop
	};
	enum EOutputs
	{
		EOP_Started = 0,
		EOP_Stopped,
		EOP_Time,
	};

	CFlowNode_MeasureTime(SActivationInfo* pActInfo)
	{
		m_last = 0.0f;
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_MeasureTime(pActInfo);
	}

	virtual void Serialize(SActivationInfo*, TSerialize ser)
	{
		ser.Value("m_last", m_last);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Start", _HELP("Trigger to start measuring")),
			InputPortConfig_Void("Stop",  _HELP("Trigger to stop measuring")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Started",   _HELP("Triggered on Start")),
			OutputPortConfig_Void("Stopped",   _HELP("Triggered on Stop")),
			OutputPortConfig<float>("Elapsed", _HELP("Elapsed time in seconds")),
			{ 0 }
		};

		config.sDescription = _HELP("Node to measure elapsed time.");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_Start))
			{
				m_last = gEnv->pTimer->GetFrameStartTime();
				ActivateOutput(pActInfo, EOP_Started, true);
			}
			if (IsPortActive(pActInfo, EIP_Stop))
			{
				CTimeValue dt = gEnv->pTimer->GetFrameStartTime();
				dt -= m_last;
				m_last = 0.0f;
				ActivateOutput(pActInfo, EOP_Stopped, true);
				ActivateOutput(pActInfo, EOP_Time, dt.GetSeconds());
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
private:
	CTimeValue m_last;
};


//////////////////////////////////////////////////////////////////////////
// Time of day node.
//////////////////////////////////////////////////////////////////////////
class CFlowNode_ServerTime : public CFlowBaseNode<eNCT_Instanced>
{
public:
	enum Inputs
	{
		IN_BASETIME,
		IN_PERIOD,
	};
	enum Outputs
	{
		OUT_SECS,
		OUT_MSECS,
		OUT_PERIOD,
	};
	CFlowNode_ServerTime(SActivationInfo* pActInfo) : m_lasttime(0.0f), m_basetime(0.0f)
	{
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_ServerTime(pActInfo);
	}

	virtual void Serialize(SActivationInfo*, TSerialize ser)
	{
		if (ser.IsReading())
		{
			ser.Value("lasttime", m_lasttime);
			ser.Value("basetime", m_basetime);
			ser.Value("period", m_period);
		}
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<float>("basetime", 0.0f, _HELP("Set base time in seconds. All values output will be relative to this time.")),
			InputPortConfig<float>("period",   0.0f, _HELP("Set period of the timer in seconds. Timer will reset each time after reaching this value.")),
			{ 0 }
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("secs"),
			OutputPortConfig<int>("msecs"),
			OutputPortConfig<bool>("period"),
			{ 0 }
		};

		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
				if (IsPortActive(pActInfo, IN_BASETIME))
					m_basetime = GetPortFloat(pActInfo, IN_BASETIME);

				if (IsPortActive(pActInfo, IN_PERIOD))
					m_period = GetPortFloat(pActInfo, IN_PERIOD);

				m_msec = 0;
			}
			break;
		case eFE_Update:
			{
				CTimeValue now = gEnv->pGameFramework->GetServerTime();

				if (now != m_lasttime)
				{
					uint32 msec = (uint32)((now - m_basetime).GetMilliSeconds());
					uint32 period = (uint32)(m_period.GetMilliSeconds());

					if (period > 0)
					{
						msec = msec % period;
						if (msec < m_msec)
							ActivateOutput(pActInfo, OUT_PERIOD, 1);

						m_msec = msec;
					}
					ActivateOutput(pActInfo, OUT_MSECS, msec);
				}
				if (now.GetSeconds() != m_lasttime.GetSeconds())
				{
					float secs = (now - m_basetime).GetSeconds();
					float period = m_period.GetSeconds();

					if (period > 0.00001f)
						secs = fmod_tpl(secs, period);

					ActivateOutput(pActInfo, OUT_SECS, secs);
				}

				m_lasttime = now;
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
private:
	CTimeValue m_lasttime;
	CTimeValue m_basetime;
	CTimeValue m_period;
	uint32     m_msec;
};

REGISTER_FLOW_NODE("Time:Time", CFlowTimeNode)
REGISTER_FLOW_NODE("Time:RealTime", CFlowNode_RealTime)
REGISTER_FLOW_NODE("Time:Timer", CFlowNode_Timer)
REGISTER_FLOW_NODE("Time:TimeOfDayLoadDefinitionFile", CFlowNode_TimeOfDay_LoadDefinitionFile)
REGISTER_FLOW_NODE("Time:TimeOfDay", CFlowNode_TimeOfDay)
REGISTER_FLOW_NODE("Time:MeasureTime", CFlowNode_MeasureTime)
REGISTER_FLOW_NODE("Time:ServerTime", CFlowNode_ServerTime)
REGISTER_FLOW_NODE("Time:TimedCounter", CFlowNode_TimedCounter)

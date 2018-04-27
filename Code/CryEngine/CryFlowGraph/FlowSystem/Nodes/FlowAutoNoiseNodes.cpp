// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>
#include <CryMath/PNoise3.h>

//////////////////////////////////////////////////////////////////////////
class CFlowNode_AutoNoise1D : public CFlowBaseNode<eNCT_Instanced>
{
private:
	enum EInPorts
	{
		eInPorts_Active,
		eInPorts_Seed,
		eInPorts_Time,
		eInPorts_Frequency,
		eInPorts_Amplitude,
		eInPorts_OutputInterval,
		eInPorts_ResetInternalTimeOnDeactivate
	};

	enum EOutPorts
	{
		eOutPorts_Value,
		eOutPorts_CurrentTime
	};

public:
	CFlowNode_AutoNoise1D( SActivationInfo * pActInfo ) 
		: m_outputTimer(0.0f)
		, m_time(0.0f)
	{};

	virtual void GetConfiguration( SFlowNodeConfig &config ) override
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool> ( "active" , false, _HELP("if active is true the node will output values constantly in a certain interval.")),
			InputPortConfig<int>  ( "seed", false, _HELP("the seed for this noise output")),
			InputPortConfig<float>( "time", 0.0f, _HELP("time to sample noise at. If port activated AND the active port is FALSE it will output. This is also the initial sample time if the active port is FALSE") ),
			InputPortConfig<float>( "frequency", 1.0f, _HELP("scale factor for input value. out = amplitude * Noise1D(frequency * x)") ),
			InputPortConfig<float>( "amplitude", 1.0f, _HELP("scale factor for noise values. out = amplitude * Noise1D(frequency * x)") ),
			InputPortConfig<float>( "outputInterval", 0.0f, _HELP("how often the value outputs if active (in seconds)")),
			InputPortConfig<bool> ( "resetInternalTimeOnDeactivate", false, _HELP("if the node is deactivated (with the active port) internal time resets to 0.0f")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<float>( "value", _HELP("out = amplitude * Noise1D(frequency * x)") ),
			OutputPortConfig<float>( "currentTime", _HELP("the time the node was sampled at")),
			{0}
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo ) override
	{
		switch (event)
		{
		case eFE_Update:
			{
				const CTimeValue outputInterval = GetPortFloat(pActInfo, eInPorts_OutputInterval);
				const CTimeValue frameTime = gEnv->pTimer->GetFrameTime();
				
				m_time += frameTime;

				if (fabs(outputInterval.GetSeconds()) <= std::numeric_limits<float>::epsilon())
				{
					Output(pActInfo);
				}
				else
				{
					m_outputTimer -= frameTime;
					if (m_outputTimer.GetSeconds() <= 0.0f)
					{
						m_outputTimer += outputInterval;
						Output(pActInfo);
					}
				}
			}
			break;

		case eFE_Initialize:
			{
				m_outputTimer.SetSeconds( GetPortFloat(pActInfo, eInPorts_OutputInterval) );
				m_time.SetSeconds( GetPortFloat(pActInfo, eInPorts_Time) );
				const bool bActive = GetPortBool(pActInfo, eInPorts_Active);
				const int seed     = GetPortInt(pActInfo, eInPorts_Seed);

				m_noise.SetSeedAndReinitialize(seed);
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, bActive);
			}
			break;

		case eFE_Activate:
			{
				const bool bActive = GetPortBool(pActInfo, eInPorts_Active);

				if (IsPortActive(pActInfo, eInPorts_Active))
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, bActive);
					if (!bActive)
					{
						if (GetPortBool(pActInfo, eInPorts_ResetInternalTimeOnDeactivate))
						{
							m_time.SetSeconds(0.0f);
						}

						m_outputTimer.SetSeconds( 0.0f );
					}
				}

				if (IsPortActive(pActInfo, eInPorts_Seed))
				{
					const int seed = GetPortInt(pActInfo, eInPorts_Seed);
					m_noise.SetSeedAndReinitialize(seed);
				}

				if (IsPortActive(pActInfo, eInPorts_Time) && !bActive)
				{
					m_time.SetSeconds( GetPortFloat(pActInfo, eInPorts_Time) );
					Output(pActInfo);
				}
			}
			break;

		default: break;
		};
	};

	virtual void GetMemoryUsage(ICrySizer * s) const override
	{
		s->Add(*this);
	}

	virtual IFlowNodePtr Clone( SActivationInfo * pActInfo ) override
	{
		return new CFlowNode_AutoNoise1D(pActInfo);
	}

private:

	void Output( SActivationInfo * pActInfo )
	{
		const float freq      = GetPortFloat(pActInfo, eInPorts_Frequency);
		const float amplitude = GetPortFloat(pActInfo, eInPorts_Amplitude);
		const float time      = m_time.GetSeconds();
		const float out       = amplitude * m_noise.Noise1D(freq * time);
		ActivateOutput( pActInfo, eOutPorts_Value, out);
		ActivateOutput( pActInfo, eOutPorts_CurrentTime, time);
	}

private:
	CPNoise3   m_noise;
	CTimeValue m_outputTimer;
	CTimeValue m_time;
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_Auto3DNoise : public CFlowBaseNode<eNCT_Instanced>
{
private:
	enum EInPorts
	{
		eInPorts_Active,
		eInPorts_Seed,
		eInPorts_Time,
		eInPorts_Frequency,
		eInPorts_Amplitude,
		eInPorts_OutputInterval,
		eInPorts_ResetInternalTimeOnDeactivate
	};

	enum EOutPorts
	{
		eOutPorts_Value,
		eOutPorts_CurrentTime
	};

	enum EDim
	{
		eDim_First,
		eDim_Second,
		eDim_Third,
		eDim_Count
	};

public:
	CFlowNode_Auto3DNoise( SActivationInfo * pActInfo ) 
		: m_outputTimer(0.0f)
		, m_time(0.0f)
	{};

	virtual void GetConfiguration( SFlowNodeConfig &config ) override
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool> ( "active" , false, _HELP("if active is true the node will output values constantly in a certain interval.")),
			InputPortConfig<Vec3> ( "seed", Vec3(1.0f, 2.0f, 3.f), _HELP("the seed for this noise output")),
			InputPortConfig<float>( "time", 0.0f, _HELP("time to sample noise at. If port activated AND the active port is FALSE it will output. This is also the initial sample time if the active port is FALSE") ),
			InputPortConfig<Vec3> ( "frequency", Vec3(Vec3Constants<Vec3::value_type>::fVec3_One), _HELP("scale factor for input value. out = amplitude * Noise(frequency * x)") ),
			InputPortConfig<Vec3> ( "amplitude", Vec3(Vec3Constants<Vec3::value_type>::fVec3_One), _HELP("scale factor for noise values. out = amplitude * Noise(frequency * x)") ),
			InputPortConfig<float>( "outputInterval", 0.0f, _HELP("how often the value outputs if active (in seconds)")),
			InputPortConfig<bool> ( "resetInternalTimeOnDeactivate", false, _HELP("if the node is deactivated (with the active port) internal time resets to 0.0f")),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3> ( "value", _HELP("out = amplitude * Noise3D(frequency * x)") ),
			OutputPortConfig<float>( "currentTime", _HELP("the time the node was sampled at")),
			{0}
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo ) override
	{
		switch (event)
		{
		case eFE_Update:
			{
				const CTimeValue outputInterval = GetPortFloat(pActInfo, eInPorts_OutputInterval);
				const CTimeValue frameTime = gEnv->pTimer->GetFrameTime();

				m_time += frameTime;

				if (fabs(outputInterval.GetSeconds()) <= std::numeric_limits<float>::epsilon())
				{
					Output(pActInfo);
				}
				else
				{
					m_outputTimer -= frameTime;
					if (m_outputTimer.GetSeconds() <= 0.0f)
					{
						m_outputTimer += outputInterval;
						Output(pActInfo);
					}
				}
			}
			break;

		case eFE_Initialize:
			{
				m_outputTimer.SetSeconds( GetPortFloat(pActInfo, eInPorts_OutputInterval) );
				m_time.SetSeconds( GetPortFloat(pActInfo, eInPorts_Time) );
				const bool bActive = GetPortBool(pActInfo, eInPorts_Active);
				const Vec3 seed = GetPortVec3(pActInfo, eInPorts_Seed);

				m_noise[eDim_First].SetSeedAndReinitialize(uint(fabs_tpl(seed.x)));
				m_noise[eDim_Second].SetSeedAndReinitialize(uint(fabs_tpl(seed.y)));
				m_noise[eDim_Third].SetSeedAndReinitialize(uint(fabs_tpl(seed.z)));
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, bActive);
			}
			break;

		case eFE_Activate:
			{
				const bool bActive = GetPortBool(pActInfo, eInPorts_Active);

				if (IsPortActive(pActInfo, eInPorts_Active))
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, bActive);
					if (!bActive)
					{
						if (GetPortBool(pActInfo, eInPorts_ResetInternalTimeOnDeactivate))
						{
							m_time.SetSeconds( 0.0f );
						}
						m_outputTimer.SetSeconds(0.0f);
					}
				}

				if (IsPortActive(pActInfo, eInPorts_Seed))
				{
					const Vec3 seed = GetPortVec3(pActInfo, eInPorts_Seed);
					m_noise[eDim_First].SetSeedAndReinitialize(uint(fabs_tpl(seed.x)));
					m_noise[eDim_Second].SetSeedAndReinitialize(uint(fabs_tpl(seed.y)));
					m_noise[eDim_Third].SetSeedAndReinitialize(uint(fabs_tpl(seed.z)));
				}

				if (IsPortActive(pActInfo, eInPorts_Time) && !bActive)
				{
					m_time.SetSeconds( GetPortFloat(pActInfo, eInPorts_Time) );
					Output(pActInfo);
				}
			}
			break;

		default: break;
		};
	};

	virtual void GetMemoryUsage(ICrySizer * s) const override
	{
		s->Add(*this);
	}

	virtual IFlowNodePtr Clone( SActivationInfo * pActInfo ) override
	{
		return new CFlowNode_Auto3DNoise(pActInfo);
	}

private:

	void Output( SActivationInfo * pActInfo )
	{
		const Vec3 freq      = GetPortVec3(pActInfo, eInPorts_Frequency);
		const Vec3 amplitude = GetPortVec3(pActInfo, eInPorts_Amplitude);
		const float time     = m_time.GetSeconds();
		const Vec3 out( amplitude.x * m_noise[eDim_First].Noise1D(freq.x * time), amplitude.y * m_noise[eDim_Second].Noise1D(freq.y * time), amplitude.z * m_noise[eDim_Third].Noise1D(freq.z * time));
		ActivateOutput( pActInfo, eOutPorts_Value, out);
		ActivateOutput( pActInfo, eOutPorts_CurrentTime, time);
	}

private:
	CPNoise3   m_noise[eDim_Count];
	CTimeValue m_outputTimer;
	CTimeValue m_time;
};

// Node registration
REGISTER_FLOW_NODE( "Math:AutoNoise1D", CFlowNode_AutoNoise1D )
REGISTER_FLOW_NODE( "Math:Auto3DNoise", CFlowNode_Auto3DNoise )

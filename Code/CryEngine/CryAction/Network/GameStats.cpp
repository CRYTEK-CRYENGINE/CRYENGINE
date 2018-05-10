// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:

   -------------------------------------------------------------------------
   History:
   - 17:11:2006   15:38 : Created by Stas Spivakov

*************************************************************************/
#include "StdAfx.h"
#include "GameStats.h"
#include <CryNetwork/INetwork.h>
#include <CryNetwork/INetworkService.h>
#include <CryGame/IGameFramework.h>
#include "IGameRulesSystem.h"
#include "CryAction.h"
#include "GameContext.h"
#include "ILevelSystem.h"
#include "GameServerNub.h"
#include "GameStatsConfig.h"
#include "CryActionCVars.h"
#include <CryGame/IGameStatistics.h>

const float REPORT_INTERVAL = 1.0f;//each second send state to network engine
const float UPDATE_INTERVAL = 20.0f;

const bool DEBUG_VERBOSE = false;  // Default should be false

struct SGetTime
{
	SGetTime() : hasvalue(false), s(0.0f){}
	SGetTime(const CTimeValue& start) : hasvalue(false), s(start){}
	operator const CTimeValue&()
	{
		if (!hasvalue)
		{
			t = gEnv->pTimer->GetFrameStartTime() - s;
			hasvalue = true;
		}
		return t;
	}
	bool       hasvalue;
	CTimeValue t;
	CTimeValue s;
};

struct CGameStats::Listener
	: public IServerReportListener,
	  public IStatsTrackListener
{
	virtual void OnError(EServerReportError err)
	{
		switch (err)
		{
		case eSRE_socket:
			GameWarning("Server report error : socket.");
			break;
		case eSRE_connect:
			GameWarning("Server report error : could not connect to master server.");
			break;
		case eSRE_noreply:
			GameWarning("Server report error : no reply from master server, check internet connection or firewall settings.");
			break;
		}
	}

	virtual void OnPublicIP(uint32 ip, unsigned short port)
	{
		string ip_str;
		ip_str.Format("%u.%u.%u.%u", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
		CryLog("Server's public address is %s:%u", ip_str.c_str(), port);
		ip_str += ":";
		ip_str += gEnv->pConsole->GetCVar("sv_port")->GetString();
		CCryAction::GetCryAction()->OnActionEvent(SActionEvent(eAE_serverIp, port, ip_str));
	}

	virtual void OnError(EStatsTrackError)
	{
	}
	CGameStats* m_pStats;
};

// IHostMigrationEventListener
IHostMigrationEventListener::EHostMigrationReturn CGameStats::OnInitiate(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	return IHostMigrationEventListener::Listener_Done;
}

IHostMigrationEventListener::EHostMigrationReturn CGameStats::OnDisconnectClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	return IHostMigrationEventListener::Listener_Done;
}

IHostMigrationEventListener::EHostMigrationReturn CGameStats::OnDemoteToClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	if (!hostMigrationInfo.ShouldMigrateNub())
	{
		return IHostMigrationEventListener::Listener_Done;
	}

	if (m_serverReport)
	{
		m_serverReport->StopReporting();
	}

	return IHostMigrationEventListener::Listener_Done;
}

IHostMigrationEventListener::EHostMigrationReturn CGameStats::OnPromoteToServer(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	if (!hostMigrationInfo.ShouldMigrateNub())
	{
		return IHostMigrationEventListener::Listener_Done;
	}

	// Set up server side stats and start reporting the game on LAN browsers
	Init();
	ReportSession();

	return IHostMigrationEventListener::Listener_Done;
}

IHostMigrationEventListener::EHostMigrationReturn CGameStats::OnReconnectClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	return IHostMigrationEventListener::Listener_Done;
}

IHostMigrationEventListener::EHostMigrationReturn CGameStats::OnFinalise(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	return IHostMigrationEventListener::Listener_Done;
}

IHostMigrationEventListener::EHostMigrationReturn CGameStats::OnTerminate(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	return IHostMigrationEventListener::Listener_Done;
}

IHostMigrationEventListener::EHostMigrationReturn CGameStats::OnReset(SHostMigrationInfo& hostMigrationInfo, HMStateType& state)
{
	return IHostMigrationEventListener::Listener_Done;
}
// ~IHostMigrationEventListener

static const int MAX_INT = 0x7FFFFFFF;//maximum int (spelled by nonosuit voice)

struct CGameStats::SStatsTrackHelper
{
	SStatsTrackHelper(CGameStats* p) : m_parent(p), m_errors(0){}

	template<class T>
	void ServerValue(const char* key, T value)
	{
		int id = m_parent->m_config->GetKeyId(key);
		if (id != -1)
			m_parent->m_statsTrack->SetServerValue(id, value);
		else
			m_errors.push_back(key);
	}

	template<class T>
	void PlayerValue(int idx, const char* key, T value)
	{
		int id = m_parent->m_config->GetKeyId(key);
		if (id != -1)
			m_parent->m_statsTrack->SetPlayerValue(idx, id, value);
		else
			m_errors.push_back(key);
	}

	void PlayerCatValue(int idx, const char* cat, const char* key, int value)
	{
		int mod = m_parent->m_config->GetCategoryMod(cat);
		int code = m_parent->m_config->GetCodeByKeyName(cat, key);
		int id = m_parent->m_config->GetKeyId(key);
		if (id != -1 && code != -1 && mod > 1)
		{
			int64 val = (value * mod + code);
			if (val > MAX_INT)
			{
				//we need to clamp value so it will be biggest possible value with correct remainder from division by mod
				val = MAX_INT - (MAX_INT % mod) + code;
				if (val > MAX_INT)//still too big
					val -= mod;
			}
			m_parent->m_statsTrack->SetPlayerValue(idx, id, (int)val);
		}
		else
			m_errors.push_back(key);
	}

	template<class T>
	void TeamValue(int idx, const char* key, T value)
	{
		int id = m_parent->m_config->GetKeyId(key);
		if (id != -1)
			m_parent->m_statsTrack->SetTeamValue(idx, id, value);
		else
			m_errors.push_back(key);
	}

	CGameStats*         m_parent;
	std::vector<string> m_errors;
};

template<class T>
struct TTimeStampedData
{
	std::vector<std::pair<CTimeValue, T>> m_data;
	void Value(const CTimeValue& t, const T& data)
	{
		CRY_ASSERT(m_data.empty() || m_data.back().first <= t);//check that data is always ordered correctly
		m_data.push_back(std::make_pair(t, data));
	}
};

template<class T>
static void SaveToXML(const XmlNodeRef& n, const char* name, const T& a)
{
	n->setAttr(name, a);
}

static void SaveToXML(const XmlNodeRef& n, const char* name, const Vec3& a)
{
	XmlNodeRef v = gEnv->pSystem->CreateXmlNode("vec3");
	v->setAttr("name", name);
	v->setAttr("x", a.x);
	v->setAttr("y", a.y);
	v->setAttr("z", a.z);
	n->addChild(v);
}

static void SaveToXML(const XmlNodeRef& n, const char* name, const CTimeValue& t)
{
	n->setAttr(name, t.GetMilliSecondsAsInt64());
}

template<class T>
static void SaveToXML(const XmlNodeRef& n, const char* name, const TTimeStampedData<T>& data)
{
	XmlNodeRef v = gEnv->pSystem->CreateXmlNode("values");
	v->setAttr("name", name);
	for (int i = 0; i < data.m_data.size(); ++i)
	{
		XmlNodeRef c = gEnv->pSystem->CreateXmlNode("value");
		SaveToXML(c, "time", data.m_data[i].first);
		T::SaveToXML(c, "", data.m_data[i].second);
		v->addChild(c);
	}
	n->addChild(v);
}

struct CGameStats::SPlayerStats : public _reference_target_t
{
	void GetMemoryUsage(ICrySizer* pSizer) const { /*nothing*/ }
	struct SVehicles
	{
		struct SVehicle
		{
			SVehicle(const char* name) : m_name(name){}
			string     m_name; //class Name
			CTimeValue m_used;
		};
		struct SVehicleCompare
		{
			const bool operator()(const SVehicle& a, const string& b) { return a.m_name < b; }
			const bool operator()(const string& a, const SVehicle& b) { return a < b.m_name; }
		};
		typedef std::vector<SVehicle> TVehicles;
		SVehicles() : m_curVehicle(m_vehicles.end()), m_inVehicle(false)
		{}

		void Enter(const char* vehicle, const CTimeValue& t)
		{
			if (m_curVehicle != m_vehicles.end() && m_curVehicle->m_name == vehicle)//entered current vehicle
			{
				return;
			}
			//find it and add if needed
			TVehicles::iterator it = std::lower_bound(m_vehicles.begin(), m_vehicles.end(), CONST_TEMP_STRING(vehicle), SVehicleCompare());
			if (it == m_vehicles.end())
			{
				m_curVehicle = m_vehicles.insert(m_vehicles.end(), SVehicle(vehicle));
			}
			else
			{
				if (it->m_name == vehicle)
				{
					m_curVehicle = it;
				}
				else
				{
					m_curVehicle = m_vehicles.insert(it, SVehicle(vehicle));
				}
			}
			m_inVehicle = true;
			m_enteredVehicle = t;
		}

		void Leave(const char* vehicle, const CTimeValue& t)
		{
			if (m_curVehicle != m_vehicles.end())
			{
				CRY_ASSERT(!vehicle || m_curVehicle->m_name == vehicle);
				m_curVehicle->m_used += t - m_enteredVehicle;
				m_curVehicle = m_vehicles.end();
			}
			m_inVehicle = false;
		}

		bool                m_inVehicle;
		CTimeValue          m_enteredVehicle;
		TVehicles           m_vehicles;
		TVehicles::iterator m_curVehicle;

		void                GetMemoryUsage(ICrySizer* pSizer) const { /*nothing*/ }
	};

	struct SWeapons
	{
		struct SWeapon
		{
			SWeapon(const char* n) : m_name(n), m_kills(0), m_shots(0), m_hits(0), m_reloads(0), m_damage(0.0f){}
			string      m_name;//class Name
			CTimeValue  m_used;

			int         m_kills;
			int         m_shots;
			int         m_hits;
			int         m_reloads;
			float       m_damage;

			static void SaveToXML(const XmlNodeRef& n, const char* name, const SWeapon& a)
			{
				XmlNodeRef w = gEnv->pSystem->CreateXmlNode(name) ;
				::SaveToXML(w, "name", a.m_name) ;
				::SaveToXML(w, "used", a.m_used) ;
				::SaveToXML(w, "kills", a.m_kills) ;
				::SaveToXML(w, "shots", a.m_shots) ;
				::SaveToXML(w, "hits", a.m_hits) ;
				::SaveToXML(w, "reloads", a.m_reloads) ;
				::SaveToXML(w, "damage", a.m_damage);
				n->addChild(w);
			}
		};
		typedef std::vector<SWeapon> TWeapons;

		SWeapons() : m_curWeapon(m_weapons.end())
		{
		}

		struct SWeaponCompare
		{
			const bool operator()(const SWeapon& a, const string& b) { return a.m_name < b; }
			const bool operator()(const string& a, const SWeapon& b) { return a < b.m_name; }
		};

		void SelectWeapon(const char* name, const CTimeValue& time)
		{
			if (m_curWeapon != m_weapons.end() && m_curWeapon->m_name == name)//entered current vehicle
			{
				return;
			}
			DeselectWeapon(time);//deselect old one
			//find it and add if needed
			TWeapons::iterator it = std::lower_bound(m_weapons.begin(), m_weapons.end(), CONST_TEMP_STRING(name), SWeaponCompare());
			if (it == m_weapons.end())
			{
				m_curWeapon = m_weapons.insert(m_weapons.end(), SWeapon(name));
			}
			else
			{
				if (it->m_name == name)
				{
					m_curWeapon = it;
				}
				else
				{
					m_curWeapon = m_weapons.insert(it, SWeapon(name));
				}
			}
			m_selected = time;
		}

		void DeselectWeapon(const CTimeValue& t)
		{
			if (m_curWeapon != m_weapons.end())
			{
				m_curWeapon->m_used += t - m_selected;
				m_curWeapon = m_weapons.end();
			}
		}

		void Kill()
		{
			if (m_curWeapon != m_weapons.end())
				m_curWeapon->m_kills++;
		}

		void Kill(const char* name)
		{
			if (m_curWeapon != m_weapons.end() && m_curWeapon->m_name == name)
			{
				m_curWeapon->m_kills++;
			}
			else
			{
				TWeapons::iterator it = std::lower_bound(m_weapons.begin(), m_weapons.end(), CONST_TEMP_STRING(name), SWeaponCompare());
				if (it != m_weapons.end() && it->m_name == name)
					it->m_kills++;
			}
		}

		void Shot(int pel)
		{
			if (m_curWeapon != m_weapons.end())
			{
				m_curWeapon->m_shots += pel;
			}
		}

		void Damage(float val)
		{
			if (m_curWeapon != m_weapons.end())
				m_curWeapon->m_damage += val;
		}

		void Damage(const char* name, float val)
		{
			TWeapons::iterator it = std::lower_bound(m_weapons.begin(), m_weapons.end(), CONST_TEMP_STRING(name), SWeaponCompare());
			if (it != m_weapons.end() && it->m_name == name)
				it->m_damage += val;
		}

		void Hit()
		{
			if (m_curWeapon != m_weapons.end())
			{
				m_curWeapon->m_hits++;
			}
		}

		void Hit(const char* name)
		{
			TWeapons::iterator it = std::lower_bound(m_weapons.begin(), m_weapons.end(), CONST_TEMP_STRING(name), SWeaponCompare());
			if (it != m_weapons.end() && it->m_name == name)
				it->m_hits++;
		}

		void Reload()
		{
			if (m_curWeapon != m_weapons.end())
			{
				m_curWeapon->m_reloads++;
			}
		}

		const char* GetCurrent() const
		{
			if (m_curWeapon != m_weapons.end())
			{
				return m_curWeapon->m_name.c_str();
			}
			return "";
		}

		TWeapons           m_weapons;
		TWeapons::iterator m_curWeapon;
		CTimeValue         m_selected;

		static void        SaveToXML(const XmlNodeRef& n, const char* name, const SWeapons& a)
		{
			XmlNodeRef w = gEnv->pSystem->CreateXmlNode(name);
			for (int i = 0; i < a.m_weapons.size(); ++i)
			{
				SWeapon::SaveToXML(w, "weapon", a.m_weapons[i]);
			}
			n->addChild(w);
		}
	};

	struct SSuitMode
	{
		SSuitMode() :
			m_currentMode(-1)
		{
			std::fill(m_kills, m_kills + 4, 0);
		}
		void SwitchMode(int mode, const CTimeValue& time)
		{
			if (m_currentMode >= 0 && m_currentMode < 4)
			{
				m_used[m_currentMode] += time - m_lastTime;
			}
			m_currentMode = mode;
			m_lastTime = time;
		}
		void OnKill()
		{
			if (m_currentMode >= 0 && m_currentMode < 4)
				m_kills[m_currentMode]++;
		}
		CTimeValue m_lastTime;
		int        m_currentMode;
		int        m_kills[4];
		CTimeValue m_used[4];
	};

	struct SPlayerPos
	{
		Vec3        pos;
		Vec3        fwd;
		float       terrainHeight;
		float       camHeight;

		static void SaveToXML(const XmlNodeRef& n, const char* name, const SPlayerPos& a)
		{
			::SaveToXML(n, "pos", a.pos) ;
			::SaveToXML(n, "fwd", a.fwd) ;
			::SaveToXML(n, "ground", a.terrainHeight) ;
			::SaveToXML(n, "camera", a.camHeight);
		}
	};
	struct SPlayerEvent
	{
		string      event;
		string      str_param;
		int         int_param;
		Vec3        vec3_param;
		float       float_param;
		string      str_2_param;
		static void SaveToXML(const XmlNodeRef& n, const char* name, const SPlayerEvent& a)
		{
			::SaveToXML(n, "name", a.event) ;
			::SaveToXML(n, "str_param", a.str_param) ;
			::SaveToXML(n, "int_param", a.int_param) ;
			::SaveToXML(n, "vec3_param", a.vec3_param) ;
			::SaveToXML(n, "float_param", a.float_param) ;
			::SaveToXML(n, "str_2_param", a.str_2_param);
		}

		void GetMemoryUsage(ICrySizer* pSizer) const
		{
			pSizer->AddObject(event);
			pSizer->AddObject(str_param);
			pSizer->AddObject(str_2_param);
		}
	};
	SPlayerStats(const CTimeValue& t)
		: m_kills(0)
		, m_deaths(0)
		, m_shots(0)
		, m_hits(0)
		, m_melee(0)
		, m_revives(0)
		, m_inVehicle(false)
		, m_started(t)
		, m_dead(false)
	{

	}

	void EnterVehicle(const char* vehicle, const CTimeValue& t)
	{
		m_weapons.DeselectWeapon(t); //de-select weapon while in vehicle
		m_vehicles.Enter(vehicle, t);
	}

	void LeaveVehicle(const char* vehicle, const CTimeValue& t)
	{
		m_vehicles.Leave(vehicle, t);
	}

	void SelectSuitMode(int mode, const CTimeValue& t)
	{
		m_suit.SwitchMode(mode, t);
	}

	void ClientKill(const char* wep, const CTimeValue& time)
	{
		m_suit.OnKill();
		if (wep)
			m_weapons.Kill(wep);
	}

	void ClientDeath(const CTimeValue& time)
	{
		m_suit.SwitchMode(-1, time);//switch off
		m_vehicles.Leave(0, time);
	}

	void ClientRevive(const CTimeValue& time)
	{

	}

	void SelectWeapon(const char* name, const CTimeValue& time)
	{
		m_weapons.SelectWeapon(name, time);
	}

	void Spectator(bool spectator, const CTimeValue& time)
	{
		if (spectator)
			m_weapons.DeselectWeapon(time);
		PlayerEvent(time, "spectator", "", spectator ? 1 : 0, ZERO);
	}

	void WeaponShot(int pel, const CTimeValue& t, const Vec3& pos)
	{
		m_weapons.Shot(pel);
		PlayerEvent(t, "fire", m_weapons.GetCurrent(), pel, pos);
	}

	void Reload(const CTimeValue& t, const Vec3& pos)
	{
		m_weapons.Reload();
		PlayerEvent(t, "reload", m_weapons.GetCurrent(), 0, pos);
	}

	void FiremodeChanged(const CTimeValue& t, const Vec3& pos, const char* modeName)
	{
		PlayerEvent(t, "firemode_changed", modeName, 0, pos);
	}

	void ZoomIn(const CTimeValue& t, const Vec3& pos)
	{
		PlayerEvent(t, "zoom_in", 0, 0, pos);
	}

	void ZoomOut(const CTimeValue& t, const Vec3& pos)
	{
		PlayerEvent(t, "zoom_out", 0, 0, pos);
	}

	void WeaponDamage(const char* wep, float val)
	{
		if (wep)
			m_weapons.Damage(wep, val);
		else
			m_weapons.Damage(val);
	}

	void WeaponHit(const char* wep)
	{
		if (wep)
			m_weapons.Hit(wep);
		else
			m_weapons.Hit();
	}

	void WeaponKill(const char* wep)
	{
		if (wep)
			m_weapons.Kill(wep);
		else
			m_weapons.Kill();
	}

	void End(const CTimeValue& time)
	{
		m_weapons.DeselectWeapon(time);
		m_vehicles.Leave(0, time);
		m_suit.SwitchMode(-1, time);
		m_ended = time;
	}

	void Dump(const CTimeValue& time) const
	{
		CryLog("$3=====Dumping player stats====");
		CryLog("Kills: %d", m_kills);
		CryLog("Deaths: %d", m_deaths);
		CryLog("Shots: %d", m_shots);
		CryLog("Hits: %d", m_hits);
		CryLog("Melee: %d", m_melee);
		CryLog("Revive: %d", m_revives);
		CryLog("Playtime: %d sec", int((time - m_started).GetSeconds()));
		/*CryLog("$3====Suit modes :");
		   for(int i=0;i<4;++i)
		   {
		   int suit_time = 0;
		   if(m_suit.m_currentMode == i)
		   {
		    suit_time = int((time - m_suit.m_lastTime + m_suit.m_used[i]).GetSeconds());
		   }
		   else
		   {
		    suit_time = int(m_suit.m_used[i].GetSeconds());
		   }

		   CryLog("   Mode (%d) Time: %d sec Kills: %d", i, suit_time, m_suit.m_kills[i]);
		   }

		   CryLog("$3====Vehicles :");
		   for(int i=0;i<m_vehicles.m_vehicles.size();++i)
		   {
		   int vehicle_time = 0;
		   if(m_vehicles.m_inVehicle && m_vehicles.m_curVehicle == (m_vehicles.m_vehicles.begin()+i))
		   {
		    vehicle_time = int((time - m_vehicles.m_enteredVehicle + m_vehicles.m_vehicles[i].m_used).GetSeconds());
		   }
		   else
		   {
		    vehicle_time = int(m_vehicles.m_vehicles[i].m_used.GetSeconds());
		   }
		   CryLog("   Vehicle %s Time: %d sec", m_vehicles.m_vehicles[i].m_name.c_str(), vehicle_time);
		   }
		 */
		CryLog("$3====Weapons :");
		for (int i = 0; i < m_weapons.m_weapons.size(); ++i)
		{
			const SWeapons::SWeapon& wep = m_weapons.m_weapons[i];
			int weapon_time = 0;
			if (!m_vehicles.m_inVehicle && m_weapons.m_curWeapon == (m_weapons.m_weapons.begin() + i))
			{
				weapon_time = int((wep.m_used + time - m_weapons.m_selected).GetSeconds());
			}
			else
			{
				weapon_time = int(wep.m_used.GetSeconds());
			}
			CryLog("   Weapon %s Time: %d sec shots %d hits %d kills %d", wep.m_name.c_str(), weapon_time, wep.m_shots, wep.m_hits, wep.m_kills);
		}
		CryLog("$3=====End player stats dump===");
	}

	void PlayerPos(const CTimeValue& t, const Vec3& pos, const Vec3& fwd, float terrain, float camheight)
	{
		SPlayerPos p;
		p.pos = pos;
		p.fwd = fwd;
		p.terrainHeight = terrain;
		p.camHeight = camheight;
		m_positions.Value(t, p);
	}

	void PlayerEvent(const CTimeValue& t, const char* name, const char* param = 0, int iparam = 0, const Vec3& vparam = Vec3(0, 0, 0),
	                 float fparam = 0.0f, const char* sparam = 0)
	{
		SPlayerEvent e;
		e.event = name;
		e.str_param = param;
		e.int_param = iparam;
		e.vec3_param = vparam;
		e.float_param = fparam;
		e.str_2_param = sparam;
		m_events.Value(t, e);
	}

	static void SaveToXML(const XmlNodeRef& n, const char* name, const SPlayerStats& a)
	{
		::SaveToXML(n, "kills", a.m_kills);
		::SaveToXML(n, "deaths", a.m_deaths);
		::SaveToXML(n, "shots", a.m_shots);
		::SaveToXML(n, "hits", a.m_hits);
		::SaveToXML(n, "melee", a.m_melee);
		::SaveToXML(n, "revives", a.m_revives);
		::SaveToXML(n, "start", a.m_started);
		::SaveToXML(n, "end", a.m_ended);
		SWeapons::SaveToXML(n, "weapons", a.m_weapons);
		::SaveToXML(n, "positions", a.m_positions);
		::SaveToXML(n, "events", a.m_events);
	}

	int                            m_kills;
	int                            m_deaths;
	int                            m_shots;
	int                            m_hits;
	int                            m_melee;
	int                            m_revives;

	bool                           m_inVehicle;

	CTimeValue                     m_started;
	CTimeValue                     m_ended;
	SVehicles                      m_vehicles;
	SWeapons                       m_weapons;
	SSuitMode                      m_suit;
	bool                           m_dead;
	TTimeStampedData<SPlayerPos>   m_positions;
	TTimeStampedData<SPlayerEvent> m_events;
};

struct CGameStats::SRoundStats
{
	SRoundStats()
		: m_roundTime(0)
		, m_victoryCondition(0)
		, m_respawn(false)
		, m_ranked(0)
		, m_round(0)
		, m_winner(0)
	{}

	static void SaveToXML(const XmlNodeRef& n, const char* name, const SRoundStats& a)
	{
		::SaveToXML(n, "mapname", a.m_mapName);
		::SaveToXML(n, "roundTime", a.m_roundTime);
		::SaveToXML(n, "victoryCondition", a.m_victoryCondition);
		::SaveToXML(n, "respawn", a.m_respawn);
		::SaveToXML(n, "ranked", a.m_ranked);
		::SaveToXML(n, "winner", a.m_winner);
		::SaveToXML(n, "duration", a.m_duration);
	}

	void Start(const char* mapname, const CTimeValue& time)
	{
		m_mapName = mapname;
		if (ICVar* pV = gEnv->pConsole->GetCVar("g_roundtime"))
			m_roundTime = pV->GetIVal();
		if (ICVar* pV = gEnv->pConsole->GetCVar("g_victoryCondition"))
			m_victoryCondition = pV->GetIVal();
		if (ICVar* pV = gEnv->pConsole->GetCVar("g_respawn"))
			m_respawn = pV->GetIVal() != 0;
		if (ICVar* pV = gEnv->pConsole->GetCVar("g_ranked"))
			m_ranked = pV->GetIVal();
		m_round++;
		m_winner = 0;
		m_start = time;
	}

	void End(int winner, const CTimeValue& time)
	{
		m_winner = winner;
		m_duration = time - m_start;
	}

	string     m_mapName;
	int        m_roundTime;
	int        m_victoryCondition;
	bool       m_respawn;
	int        m_ranked;
	int        m_round;
	int        m_winner;
	CTimeValue m_start;
	CTimeValue m_duration;
};

CGameStats::CGameStats(CCryAction* pGameFramework)
	: m_pGameFramework(pGameFramework)
	, m_pActorSystem(pGameFramework->GetIActorSystem())
	, m_statsTrack(0)
	, m_serverReport(0)
	, m_pListener(new Listener())
	, m_lastUpdate(0.0f)
	, m_lastReport(0.0f)
	, m_playing(false)
	, m_stateChanged(false)
	, m_startReportNeeded(false)
	, m_lastPosUpdate(0.0f)
	, m_reportStarted(false)
{
	m_pListener->m_pStats = this;

	CCryAction::GetCryAction()->GetIGameplayRecorder()->RegisterListener(this);
	CCryAction::GetCryAction()->GetILevelSystem()->AddListener(this);
	CCryAction::GetCryAction()->RegisterListener(this, "GameStats", FRAMEWORKLISTENERPRIORITY_DEFAULT);
	m_config = CCryAction::GetCryAction()->GetGameStatsConfig();
	if (!gEnv->bServer)//we may not have a chance to do that
		Init();

	gEnv->pNetwork->AddHostMigrationEventListener(this, "CGameStats", ELPT_PostEngine);
}

CGameStats::~CGameStats()
{
	gEnv->pNetwork->RemoveHostMigrationEventListener(this);

	if (m_serverReport)
		m_serverReport->StopReporting();

	delete m_pListener;
	m_pListener = 0;

	CCryAction::GetCryAction()->GetIGameplayRecorder()->UnregisterListener(this);
	CCryAction::GetCryAction()->GetILevelSystem()->RemoveListener(this);
	CCryAction::GetCryAction()->UnregisterListener(this);
}

/*static const struct
   {
   EGameplayEvent key;
   const char*    name;
   }
   gEventNames[]={{eGE_GameReset,						"eGE_GameReset"},
              {eGE_GameStarted,						"eGE_GameStarted"},
              {eGE_GameEnd,								"eGE_GameEnd"},
              {eGE_SuddenDeath,						"eGE_SuddenDeath"},
              {eGE_RoundEnd,							"eGE_RoundEnd"},
              {eGE_Connected,							"eGE_Connected"},
              {eGE_Disconnected,					"eGE_Disconnected"},
              {eGE_Renamed,								"eGE_Renamed"},
              {eGE_ChangedTeam,						"eGE_ChangedTeam"},
              {eGE_Died,									"eGE_Died"},
              {eGE_Scored,								"eGE_Scored"},
              {eGE_Currency,							"eGE_Currency"},
              {eGE_Rank,									"eGE_Rank"},
              {eGE_Spectator,							"eGE_Spectator"},
              {eGE_ScoreReset,						"eGE_ScoreReset"},

              {eGE_AttachedAccessory,			"eGE_AttachedAccessory"},

              {eGE_ZoomedIn,							"eGE_ZoomedIn"},
              {eGE_ZoomedOut,							"eGE_ZoomedOut"},

              {eGE_Kill,									"eGE_Kill"},
              {eGE_Death,									"eGE_Death"},
              {eGE_Revive,								"eGE_Revive"},

              {eGE_SuitModeChanged,				"eGE_SuitModeChanged"},

              {eGE_Hit,										"eGE_Hit"},
              {eGE_Damage,								"eGE_Damage"},

              {eGE_WeaponHit,							"eGE_WeaponHit"},
              {eGE_WeaponReload,					"eGE_WeaponReload"},
              {eGE_WeaponShot,						"eGE_WeaponShot"},
              {eGE_WeaponMelee,						"eGE_WeaponMelee"},
              {eGE_WeaponFireModeChanged,	"eGE_WeaponFireModeChanged"},

              {eGE_ItemSelected,					"eGE_ItemSelected"},
              {eGE_ItemPickedUp,					"eGE_ItemPickedUp"},
              {eGE_ItemDropped,						"eGE_ItemDropped"},
              {eGE_ItemBought,						"eGE_ItemBought"},

              {eGE_EnteredVehicle,				"eGE_EnteredVehicle"},
              {eGE_LeftVehicle,						"eGE_LeftVehicle"}};

   static const unsigned int gEventNamesNum = CRY_ARRAY_COUNT(gEventNames);*/

void CGameStats::OnGameplayEvent(IEntity* pEntity, const GameplayEvent& event)
{
	/*	for(int i=0;i<gEventNamesNum;++i)
	   {
	    if(gEventNames[i].key == event.event)
	    {
	      CryLog("GameStats : Event %s",gEventNames[i].name);
	    }
	   }*/

	int e_id = pEntity ? (int) pEntity->GetId() : 0;
	switch (event.event)
	{
	case eGE_GameStarted:
		StartGame(event.value != 0);
		break;
	case eGE_GameEnd:
		EndGame(event.value != 0);
		break;
	case eGE_SuddenDeath:
		SuddenDeath(event.value != 0);
		break;
	case eGE_RoundEnd:
		EndRound(event.value != 0, atoi(event.description));
		break;
	case eGE_Renamed:
		SetName(e_id, event.description);
		break;
	case eGE_Scored:
		SetScore(e_id, event.description, (int)event.value);
		break;
	case eGE_Kill:
		OnKill(e_id, (EntityId*)event.extra);
		if (pEntity)
			ProcessPlayerStat(pEntity, event);
		break;
	case eGE_Death:
		OnDeath(e_id, (int)(TRUNCATE_PTR)event.extra);
		if (pEntity)
			ProcessPlayerStat(pEntity, event);
		break;
	case eGE_WeaponShot:
	case eGE_WeaponHit:
	case eGE_SuitModeChanged:
	case eGE_WeaponMelee:
	case eGE_LeftVehicle:
	case eGE_EnteredVehicle:
	case eGE_ItemSelected:
	case eGE_WeaponReload:
	case eGE_Damage:
	case eGE_Revive:
	case eGE_WeaponFireModeChanged:
	case eGE_ZoomedIn:
	case eGE_ZoomedOut:
		if (pEntity)
			ProcessPlayerStat(pEntity, event);
		break;
	case eGE_Connected:
		{
			bool restored = event.value != 0.0f;

			struct SExtraData
			{
				int status[2];
			};

			SExtraData* pExtra = static_cast<SExtraData*>(event.extra);
			NewPlayer(e_id, pExtra->status[0], pExtra->status[1] != 0, restored);
		}
		break;
	case eGE_ChangedTeam:
		SetTeam(e_id, (int)event.value);
		break;
	case eGE_Spectator:
		SetSpectator(e_id, (int)event.value);
		if (pEntity)
			ProcessPlayerStat(pEntity, event);
		break;
	case eGE_Disconnected:
		RemovePlayer(e_id, event.value != 0);
		break;
	case eGE_ScoreReset:
		ResetScore(e_id);
		break;
	case eGE_Rank:
		SetRank(e_id, (int)event.value);
		break;
	case eGE_GameReset:
		GameReset();
		break;
	default:
		break;
	}
}

void CGameStats::OnActionEvent(const SActionEvent& event)
{
	if (event.m_event == eAE_disconnected && !gEnv->bServer)//on disconnect submit our stats
	{
		if (m_statsTrack && m_playing)
		{
			/*for(PlayerStatsMap::iterator it = m_playerMap.begin(), eit = m_playerMap.end();it!=eit; ++it)
			   {
			   SubmitPlayerStats(it->second);
			   m_statsTrack->PlayerDisconnected(it->second.id);
			   it->second.stats = new SPlayerStats();
			   }*/
			//clients are not submitting any date if disconnected occasionally
			m_statsTrack->Reset();
		}
	}
}

void CGameStats::OnLevelNotFound(const char* levelName)
{
	m_startReportNeeded = false;
}

void CGameStats::OnLoadingStart(ILevelInfo* pLevel)
{
	if (gEnv->bServer)
	{
		ReportGame();

		if (pLevel && m_serverReport)
		{
			if (*(pLevel->GetDisplayName()))
				m_serverReport->SetServerValue("mapname", pLevel->GetDisplayName());
			else
				m_serverReport->SetServerValue("mapname", pLevel->GetName());
		}

		if (m_startReportNeeded)
		{
			if (m_serverReport && !m_reportStarted)
			{
				m_serverReport->StartReporting(gEnv->pGameFramework->GetServerNetNub(), m_pListener);
				m_reportStarted = true;
			}
			m_startReportNeeded = false;
		}

		if (m_serverReport)
		{
			m_serverReport->Update();
			m_stateChanged = false;
		}
	}
}

void CGameStats::OnLoadingLevelEntitiesStart(ILevelInfo* pLevel)
{
}

void CGameStats::OnLoadingComplete(ILevelInfo* pLevel)
{
}

void CGameStats::OnLoadingError(ILevelInfo* pLevel, const char* error)
{
	m_startReportNeeded = false;
}

void CGameStats::OnLoadingProgress(ILevelInfo* pLevel, int progressAmount)
{
}

void CGameStats::OnUnloadComplete(ILevelInfo* pLevel)
{
}

void CGameStats::OnKill(int plrId, EntityId* extra)
{

}

void CGameStats::OnDeath(int plrId, int shooterId)
{
}

void CGameStats::StartSession()
{
	m_roundStats.reset(new SRoundStats());

	m_startReportNeeded = true;
	if (!m_statsTrack || !m_serverReport)
		Init();
	if (!m_reportStarted && !m_startReportNeeded)//OnLoadingStart was not able to start report
		m_startReportNeeded = true;
	ReportSession();
}

void CGameStats::EndSession()
{
	m_startReportNeeded = false;
	m_reportStarted = false;
	if (m_serverReport)
	{
		m_serverReport->StopReporting();
	}
}

void CGameStats::StartGame(bool server)
{
	if (!server && gEnv->bServer)//we simply ignore client events on server
		return;

	if (IGameRulesSystem* pGR = gEnv->pGameFramework->GetIGameRulesSystem())
	{
		IGameRules* pR = pGR->GetCurrentGameRules();
		if (pR)
		{
			IEntityScriptComponent* pScriptProxy = static_cast<IEntityScriptComponent*>(pR->GetEntity()->GetProxy(ENTITY_PROXY_SCRIPT));
			if (pScriptProxy)
			{
				string gameState = pScriptProxy->GetState();
				if (gameState == "InGame")
				{
					m_playing = true;
					m_gameMode = pR->GetEntity()->GetClass()->GetName();
				}
			}
		}
	}

	if (!m_statsTrack || !m_serverReport)
		Init();

	if (m_serverReport)
	{
		ReportGame();
		m_serverReport->Update();
	}

	if (m_playing)
	{
		if (ILevelInfo* pLevelInfo = gEnv->pGameFramework->GetILevelSystem()->GetCurrentLevel())
		{
			m_mapName = pLevelInfo->GetName();//we only need pure level name here
			const char* p = strrchr(m_mapName.c_str(), '/');
			if (p != 0 && strlen(p) > 1)
			{
				m_mapName = p + 1;
				char* pStr = const_cast<char*>(m_mapName.data());
				pStr[0] = toupper(m_mapName[0]);
				for (int i = 1; i < m_mapName.size(); ++i)
					pStr[i] = tolower(m_mapName[i]);
			}
		}
	}

	if (m_statsTrack && m_playing && gEnv->bServer)
		m_statsTrack->StartGame();

	if (gEnv->bServer && m_playing)
	{
		m_roundStart = gEnv->pTimer->GetFrameStartTime();
		m_lastPosUpdate = 0.0f;
		ResetStats();
		m_roundStats->Start(m_mapName, m_roundStart);
	}
}

void CGameStats::ResetStats()
{
	SGetTime time(m_roundStart);
	for (PlayerStatsMap::iterator it = m_playerMap.begin(), eit = m_playerMap.end(); it != eit; ++it)
	{
		it->second.stats.clear();
		it->second.stats.push_back(new SPlayerStats(time));
		if (IActor* pActor = m_pGameFramework->GetIActorSystem()->GetActor(it->first))
		{
			if (IItem* pI = pActor->GetCurrentItem())
			{
				it->second.stats.back()->SelectWeapon(pI->GetEntity()->GetClass()->GetName(), time);
			}
		}
	}
}

void CGameStats::SaveStats()
{
	if (m_roundStats->m_round)
	{
		SGetTime time(m_roundStart);
		CryFixedStringT<128> timeStr;
		time_t ltime;
		::time(&ltime);
		tm* today = localtime(&ltime);
		strftime(timeStr.m_str, timeStr.MAX_SIZE, "%y-%m-%d_%H%M%S", today);

		XmlNodeRef root = gEnv->pSystem->CreateXmlNode("round");
		root->setAttr("round", m_roundStats->m_round);
		SRoundStats::SaveToXML(root, "", *m_roundStats.get());
		for (PlayerStatsMap::iterator it = m_playerMap.begin(), eit = m_playerMap.end(); it != eit; ++it)
		{
			for (SPlayerInfo::TStatsVct::iterator si = it->second.stats.begin(); si < it->second.stats.end(); ++si)
			{
				it->second.stats.back()->End(time);

				XmlNodeRef plr = gEnv->pSystem->CreateXmlNode("player");
				if (IEntity* p = gEnv->pEntitySystem->GetEntity(it->first))
				{
					plr->setAttr("id", p->GetId());
					if (IActor* pAct = m_pGameFramework->GetIActorSystem()->GetActor(p->GetId()))
					{
						int profile = m_pGameFramework->GetNetChannel(pAct->GetChannelId())->GetProfileId();
						plr->setAttr("profile", profile);
					}
				}
				plr->setAttr("name", it->second.name);
				plr->setAttr("team", it->second.team);
				plr->setAttr("spectator", it->second.spectator);

				SPlayerStats::SaveToXML(plr, "", *(si->get()));
				root->addChild(plr);
			}
		}
		root->saveToFile(string().Format("%s/StatsLogs/Round_%s.xml", gEnv->pCryPak->GetAlias("%USER%"), timeStr.c_str()));
	}
}

void CGameStats::EndGame(bool server)
{
	if (!server && gEnv->bServer) //we simply ignore client events on server
		return;

	if (m_statsTrack && m_playing)
	{
		SGetTime time(m_roundStart);

		if (gEnv->bServer)
			SubmitServerStats();
		for (PlayerStatsMap::iterator it = m_playerMap.begin(), eit = m_playerMap.end(); it != eit; ++it)
		{
			SubmitPlayerStats(it->second, gEnv->bServer, it->first == CCryAction::GetCryAction()->GetClientActorId());
			it->second.stats.push_back(new SPlayerStats(time));
		}
		m_statsTrack->EndGame();
	}

	m_playing = false;

	ReportGame();

	if (m_serverReport)
		m_serverReport->Update();
}

void CGameStats::NewPlayer(int plr, int team, bool spectator, bool restored)
{
	SGetTime time(m_roundStart);

	if (DEBUG_VERBOSE)
		CryLog("CGameStats::NewPlayer %08X %s", plr, restored ? "restored" : "");
	PlayerStatsMap::iterator it = m_playerMap.find(plr);
	if (it == m_playerMap.end())
	{
		it = m_playerMap.insert(std::make_pair(plr, SPlayerInfo())).first;
	}
	else
	{
		if (DEBUG_VERBOSE)
			CryLog("CGameStats::NewPlayer restored not found");
	}

	if (m_statsTrack)
	{
		if (restored)
			m_statsTrack->PlayerConnected(it->second.id);
		else
			it->second.id = m_statsTrack->AddPlayer(plr);
	}
	else
		it->second.id = -1;
	if (!it->second.stats.empty())
		it->second.stats.back()->End(time);
	it->second.stats.push_back(new SPlayerStats(time));

	if (IActor* pActor = m_pGameFramework->GetIActorSystem()->GetActor(plr))
	{
		if (IItem* pI = pActor->GetCurrentItem())
		{
			it->second.stats.back()->SelectWeapon(pI->GetEntity()->GetClass()->GetName(), time);
		}
	}

	if (!restored)
	{
		it->second.team = team;
		it->second.rank = 1;
		it->second.spectator = spectator;
		it->second.name = gEnv->pEntitySystem->GetEntity(plr)->GetName();
	}

	m_stateChanged = true;

	Report();

	if (m_serverReport && m_playerMap.size() == gEnv->pConsole->GetCVar("sv_maxplayers")->GetIVal())//if server is full, report now
	{
		m_serverReport->Update();
		m_stateChanged = false;
	}
}

void CGameStats::EndRound(bool server, int winner)
{
	if (!gEnv->bServer || !server)
		return;
	m_roundStats->End(winner, gEnv->pTimer->GetFrameStartTime());

	if (CCryActionCVars::Get().g_statisticsMode == 1)
		SaveStats();
}

void CGameStats::SuddenDeath(bool server)
{

}

void CGameStats::RemovePlayer(int plr, bool keep)
{
	SGetTime time(m_roundStart);

	if (DEBUG_VERBOSE)
		CryLog("CGameStats::RemovePlayer %08X %s", plr, keep ? "keep" : "");
	PlayerStatsMap::iterator it = m_playerMap.find(plr);

	if (it == m_playerMap.end())
		return;

	if (m_statsTrack && m_playing)
	{
		SubmitPlayerStats(it->second, gEnv->bServer, false);
		m_statsTrack->PlayerDisconnected(it->second.id);
	}
	it->second.stats.push_back(new SPlayerStats(time));

	if (!keep)
	{
		m_stateChanged = true;

		int num_players = m_playerMap.size();

		m_playerMap.erase(it);

		Report();

		if (m_serverReport && num_players == gEnv->pConsole->GetCVar("sv_maxplayers")->GetIVal())//if server was full, report now
		{
			m_serverReport->Update();
			m_stateChanged = false;
		}
	}
}

void CGameStats::CreateNewLifeStats(int plr)
{
	SGetTime time(m_roundStart);

	PlayerStatsMap::iterator it = m_playerMap.find(plr);
	if (it == m_playerMap.end())
		return;

	CRY_ASSERT(!it->second.stats.empty());

	if (it->second.stats.back()->m_deaths < 1)
		return;//initial resp
	it->second.stats.back()->End(time);

	it->second.stats.push_back(new SPlayerStats(time));
}

void CGameStats::ResetScore(int plr)
{
	int i = 0;
	for (PlayerStatsMap::iterator it = m_playerMap.begin(), eit = m_playerMap.end(); it != eit; ++it, ++i)
	{
		if (it->first != plr)
			continue;
		for (std::map<string, int>::iterator sit = it->second.scores.begin(); sit != it->second.scores.end(); ++sit)
		{
			sit->second = 0;
			if (m_serverReport)
				m_serverReport->SetPlayerValue(i, sit->first, "0");
		}
		break;
	}
	m_stateChanged = true;
}

void CGameStats::SetName(int plr, const char* name)
{
	PlayerStatsMap::iterator it = m_playerMap.find(plr);

	if (it == m_playerMap.end())
		return;

	it->second.name = name;

	m_stateChanged = true;
}

void CGameStats::SetScore(int plr, const char* desc, int value)
{
	PlayerStatsMap::iterator it = m_playerMap.find(plr);

	if (it != m_playerMap.end())
	{
		it->second.scores[desc] = value;
		m_stateChanged = true;
	}
}

void CGameStats::SetTeam(int plr, int value)
{
	PlayerStatsMap::iterator it = m_playerMap.find(plr);

	if (it != m_playerMap.end())
	{
		it->second.team = value;
		m_stateChanged = true;
	}

}

void CGameStats::SetRank(int plr, int value)
{
	PlayerStatsMap::iterator it = m_playerMap.find(plr);

	if (it != m_playerMap.end())
	{
		it->second.rank = value;
		m_stateChanged = true;
	}
}

void CGameStats::SetSpectator(int plr, int value)
{
	PlayerStatsMap::iterator it = m_playerMap.find(plr);

	if (it != m_playerMap.end())
	{
		it->second.spectator = value != 0;
	}
	m_stateChanged = true;
}

void CGameStats::GameReset()
{
	m_playing = false;
	m_playerMap.clear();
	m_teamMap.clear();

	if (m_statsTrack)
		m_statsTrack->Reset();
	m_stateChanged = true;
}

void CGameStats::Connected()
{
	//should not be called
	if (gEnv->IsClient() && !gEnv->bServer)//pure clients
	{
		if (IGameRulesSystem* pGR = gEnv->pGameFramework->GetIGameRulesSystem())
		{
			IGameRules* pR = pGR->GetCurrentGameRules();
			if (pR)
			{
				IEntityScriptComponent* pScriptProxy = static_cast<IEntityScriptComponent*>(pR->GetEntity()->GetProxy(ENTITY_PROXY_SCRIPT));
				if (pScriptProxy)
				{
					string gameState = pScriptProxy->GetState();
					if (gameState == "InGame")
					{
						m_playing = true;
						m_gameMode = pR->GetEntity()->GetClass()->GetName();
					}
				}
			}
		}
		if (m_statsTrack)
			m_statsTrack->Reset();
	}
}

void CGameStats::Update()
{
	//iterate players map and get positions
	SGetTime time(m_roundStart);
	CTimeValue gametime = time;

	if (gametime > m_lastUpdate + UPDATE_INTERVAL)
	{
		m_lastUpdate = gametime;
		m_lastReport = gametime;
		Report();

		if (m_stateChanged)
		{
			if (m_serverReport)
				m_serverReport->Update();
			m_stateChanged = false;
		}
	}
	else
	{
		if (gametime > m_lastReport + REPORT_INTERVAL)
		{
			Report();
			m_lastReport = gametime;
		}
	}
}

void CGameStats::Dump()
{
	SGetTime time(m_roundStart);
	//dump some info for debug purposes
	int lifeNo = 0;

	if (gEnv->bServer)
	{
		for (PlayerStatsMap::const_iterator it = m_playerMap.begin(), eit = m_playerMap.end(); it != eit; ++it)
		{
			CryLog("$2Stats for player %s", it->second.name.c_str());
			for (SPlayerInfo::TStatsVct::const_iterator si = it->second.stats.begin(); si < it->second.stats.end(); ++si)
			{
				CryLog("Life no: %d", lifeNo);
				si->get()->Dump(time);
				++lifeNo;
			}
			;
		}
	}
	else
	{
		IActor* pActor = CCryAction::GetCryAction()->GetClientActor();
		if (!pActor)
			return;

		PlayerStatsMap::iterator it = m_playerMap.find(pActor->GetEntityId());
		if (it == m_playerMap.end())
			return;

		for (SPlayerInfo::TStatsVct::iterator si = it->second.stats.begin(); si < it->second.stats.end(); ++si)
		{
			CryLog("Life no: %d", lifeNo);
			si->get()->Dump(time);
			++lifeNo;
		}
	}
}

void CGameStats::ReportSession()
{
	if (!m_serverReport)
		return;

	string name;
	ICVar* pName = gEnv->pConsole->GetCVar("sv_servername");
	if (pName)
		name = pName->GetString();
	if (name.empty())
		name = gEnv->pNetwork->GetHostName();

	CCryAction::GetCryAction()->OnActionEvent(SActionEvent(eAE_serverName, 0, name));

	if (gEnv->pConsole->GetCVar("sv_lanonly")->GetIVal())//we're on LAN so report our name
	{
		string ip = gEnv->pNetwork->GetHostName();
		ip += ":";
		ip += gEnv->pConsole->GetCVar("sv_port")->GetString();
		CCryAction::GetCryAction()->OnActionEvent(SActionEvent(eAE_serverIp, 0, ip));
	}

	if (m_serverReport)
	{
		m_serverReport->SetServerValue("hostname", name);
		m_serverReport->SetServerValue("hostport", gEnv->pConsole->GetCVar("sv_port")->GetString());

		char strProductVersion[256];
		gEnv->pSystem->GetProductVersion().ToString(strProductVersion);
		m_serverReport->SetServerValue("gamever", strProductVersion);
		m_serverReport->SetServerValue("maxplayers", gEnv->pConsole->GetCVar("sv_maxplayers")->GetString());
		ICVar* pFF = gEnv->pConsole->GetCVar("g_friendlyfireratio");
		m_serverReport->SetServerValue("friendlyfire", pFF ? ((pFF->GetFVal() != 0) ? "1" : "0") : 0);
		m_serverReport->SetServerValue("dx10", CCryAction::GetCryAction()->GetGameContext()->HasContextFlag(eGSF_ImmersiveMultiplayer) ? "1" : "0");
	}

	ReportGame();

	if ((CCryAction::GetCryAction()->GetILevelSystem()->IsLevelLoaded() && CCryAction::GetCryAction()->IsGameStarted()) || m_startReportNeeded)//otherwise, OnLoadingStart will report it
	{
		if (m_serverReport && !m_reportStarted)//report now
		{
			m_serverReport->StartReporting(gEnv->pGameFramework->GetServerNetNub(), m_pListener);
			m_reportStarted = true;
		}
		m_startReportNeeded = false;
	}

	m_startReportNeeded = !m_reportStarted;
}

void CGameStats::ReportGame()
{
	string mode;
	if (IEntity* pGRE = gEnv->pGameFramework->GetIGameRulesSystem()->GetCurrentGameRulesEntity())
	{
		mode = pGRE->GetClass()->GetName();
	}
	else
		mode = gEnv->pConsole->GetCVar("sv_gamerules")->GetString();

	string name;
	ICVar* pName = gEnv->pConsole->GetCVar("sv_servername");
	if (pName)
		name = pName->GetString();

	string timelimit;
	static ICVar* pVar = gEnv->pConsole->GetCVar("g_timelimit");
	timelimit = pVar ? pVar->GetString() : "0";

	if (m_serverReport)
	{
		string levelname;
		if (GetLevelName(levelname))
			m_serverReport->SetServerValue("mapname", levelname.c_str());
		m_serverReport->SetServerValue("gametype", mode);

		m_serverReport->SetServerValue("timelimit", pVar ? pVar->GetString() : "0");
	}

	Report();
}

void CGameStats::Report()
{
	if (!m_serverReport)
		return;

	int playerCount = m_playerMap.size();
	if (CGameServerNub* pServerNub = CCryAction::GetCryAction()->GetGameServerNub())
		playerCount = pServerNub->GetPlayerCount();

	//All server reporting is done here
	m_serverReport->SetReportParams(playerCount, m_teamMap.size());
	m_serverReport->SetServerValue("gamemode", m_playing ? "game" : "pre-game");

	CryFixedStringT<32> timeleft("-");
	if (IGameRulesSystem* pGR = gEnv->pGameFramework->GetIGameRulesSystem())
	{
		IGameRules* pR = pGR->GetCurrentGameRules();
		if (pR && pR->IsTimeLimited() && m_playing)
		{
			timeleft.Format("%.0f", pR->GetRemainingGameTime());
		}
	}

	m_serverReport->SetServerValue("timeleft", timeleft);

	CryFixedStringT<256> tempStr;

	m_serverReport->SetServerValue("numplayers", tempStr.Format("%d", playerCount));

	int i = 0;

	string mode;
	for (PlayerStatsMap::const_iterator it = m_playerMap.begin(); it != m_playerMap.end(); ++it)
	{
		static string value;
		m_serverReport->SetPlayerValue(i, "player", it->second.name);
		value.Format("%d", it->second.rank);
		m_serverReport->SetPlayerValue(i, "rank", value);
		value.Format("%d", it->second.team ? it->second.team : (it->second.spectator ? 0 : 1));
		m_serverReport->SetPlayerValue(i, "team", value);
		for (std::map<string, int>::const_iterator sit = it->second.scores.begin(); sit != it->second.scores.end(); ++sit)
			m_serverReport->SetPlayerValue(i, sit->first, tempStr.Format("%d", sit->second));
		++i;
	}
	while (i < playerCount)
	{
		m_serverReport->SetPlayerValue(i, "player", "<connecting>");
		++i;
	}
}

void CGameStats::ProcessPlayerStat(IEntity* pEntity, const GameplayEvent& event)
{
	if (CCryActionCVars::Get().g_statisticsMode != 1)
		return;

	SGetTime time(m_roundStart);

	const int e_id = pEntity->GetId();
	PlayerStatsMap::iterator it = m_playerMap.find(e_id);
	if (it == m_playerMap.end())
		return;
	SPlayerStats& plr = *(it->second.stats.back());
	if (gEnv->bServer)
	{
		switch (event.event)
		{
		case eGE_Kill:
			plr.m_kills++;
			{
				EntityId* params = (EntityId*)event.extra;
				IEntity* weapon = gEnv->pEntitySystem->GetEntity(params[0]);
				string wname = event.description ? event.description : "";
				if (weapon)
					wname = weapon->GetClass()->GetName();
				plr.PlayerEvent(time, "kill", wname, params[1], pEntity->GetWorldPos());
			}
			break;
		case eGE_Death:
			plr.m_deaths++;
			plr.m_inVehicle = false;
			plr.PlayerEvent(time, "death", "", 0, pEntity->GetWorldPos());
			break;
		case eGE_Revive:
			plr.m_revives++;
			plr.PlayerEvent(time, "spawn", "", int(event.value), pEntity->GetWorldPos());
			CreateNewLifeStats(e_id);
			break;
		case eGE_WeaponMelee:
			{
				plr.m_melee++;
				plr.PlayerEvent(time, "weapon_melee");
			}
			break;
		case eGE_EnteredVehicle:
			plr.m_inVehicle = true;
			break;
		case eGE_LeftVehicle:
			plr.m_inVehicle = false;
			break;
		}
	}

	static string vehicle_name;

	switch (event.event)
	{
	case eGE_WeaponShot:
		if (!plr.m_inVehicle)
		{
			if (event.description == NULL)
				break;

			if (strstr(event.description, "bullet") != 0 || strcmp(event.description, "shotgunshell") == 0 || strcmp(event.description, "alienmount_acmo") == 0)//only counting these
				plr.m_shots += int(event.value);
		}
		plr.WeaponShot(int(event.value), time, pEntity->GetWorldPos());
		break;
	/*case eGE_Damage:
	   {
	    float dmgVal = event.value;
	    const char* weaponName = event.description;
	    plr.WeaponDamage(weaponName, dmgVal);

	   }break;*/
	case eGE_WeaponHit:
		if (!plr.m_inVehicle)
		{
			float dmgVal = event.value;
			const char* weaponName = event.description;
			uint32 targetId = (uint32) reinterpret_cast<TRUNCATE_PTR>(event.extra);
			const Vec3 fakePos(0, 0, 0);
			const char* bodyPart = event.strData;

			plr.WeaponHit(weaponName);
			plr.WeaponDamage(weaponName, dmgVal);
			plr.PlayerEvent(time, "hit", weaponName, targetId, fakePos, dmgVal, bodyPart);

		}
		break;
	case eGE_Kill:
		{
			plr.ClientKill(event.description, time);

			EntityId* params = (EntityId*)event.extra;
			IEntity* pWeapon = 0;
			if (params)
				gEnv->pEntitySystem->GetEntity(params[0]);
			plr.WeaponKill(pWeapon ? pWeapon->GetClass()->GetName() : 0);
		}
		break;
	case eGE_Death:
		plr.ClientDeath(time);
		break;
	case eGE_Revive:
		plr.ClientRevive(time);
		break;
	case eGE_SuitModeChanged:
		plr.SelectSuitMode(int(event.value), time);
		break;
	case eGE_EnteredVehicle:
		{
			if (IVehicle* pV = CCryAction::GetCryAction()->GetIVehicleSystem()->GetVehicle((EntityId)(TRUNCATE_PTR)event.extra))
			{
				vehicle_name.Format("%s_%s", pV->GetEntity()->GetClass()->GetName(), pV->GetModification());
				plr.EnterVehicle(vehicle_name, time);
			}
		}
		break;
	case eGE_LeftVehicle:
		{
			if (IVehicle* pV = CCryAction::GetCryAction()->GetIVehicleSystem()->GetVehicle((EntityId)(TRUNCATE_PTR)event.extra))
			{
				vehicle_name.Format("%s_%s", pV->GetEntity()->GetClass()->GetName(), pV->GetModification());
				plr.LeaveVehicle(vehicle_name, time);
			}
		}
		break;
	case eGE_ItemSelected:
		{
			if (IEntity* pI = gEnv->pEntitySystem->GetEntity(EntityId((TRUNCATE_PTR)event.extra)))
				plr.SelectWeapon(pI->GetClass()->GetName(), time);
		}
		break;
	case eGE_WeaponReload:
		{
			plr.Reload(time, pEntity->GetWorldPos());
			break;
		}
	case eGE_Spectator:
		plr.Spectator(event.value != 0, time);
		break;
	case eGE_WeaponFireModeChanged:
		{
			plr.FiremodeChanged(time, pEntity->GetWorldPos(), event.description);
			break;
		}
	case eGE_ZoomedIn:
		{
			plr.ZoomIn(time, pEntity->GetWorldPos());
			break;
		}
	case eGE_ZoomedOut:
		{
			plr.ZoomOut(time, pEntity->GetWorldPos());
			break;
		}
	}
}

bool CGameStats::GetLevelName(string& mapname)
{
	string levelname;
	if (ILevelInfo* pLevelInfo = gEnv->pGameFramework->GetILevelSystem()->GetCurrentLevel())
	{
		levelname = pLevelInfo->GetDisplayName();
		if (levelname.empty())
			levelname = gEnv->pGameFramework->GetLevelName();
	}
	if (levelname.empty())
		return false;
	else
	{
		mapname = levelname;
		return true;
	}
}

void CGameStats::SubmitPlayerStats(const SPlayerInfo& plr, bool server, bool client)
{
	if (!m_statsTrack)
		return;
	SStatsTrackHelper hlp(this);
	//server stats

	const SPlayerStats& stats = *(plr.stats.back());
	if (server)
	{
		hlp.PlayerValue(plr.id, "name", plr.name.c_str());
		hlp.PlayerValue(plr.id, "rank", plr.rank);
		hlp.PlayerValue(plr.id, "kills", stats.m_kills);
		hlp.PlayerValue(plr.id, "deaths", stats.m_deaths);
		hlp.PlayerValue(plr.id, "shots", stats.m_shots);
		hlp.PlayerValue(plr.id, "hits", stats.m_hits);
		hlp.PlayerValue(plr.id, "melee", stats.m_melee);
	}

	//here are some client-only stats - mainly times
	if (client)
	{
		CTimeValue time = SGetTime(m_roundStart);
		plr.stats.back()->End(time);
		int played = int((time - plr.stats.back()->m_started).GetSeconds() + 0.5f);
		int played_minutes = played / 60;
		hlp.PlayerValue(plr.id, "played", played_minutes);
		static string keyName;

		keyName.Format("Played%s", m_gameMode.c_str());
		hlp.PlayerCatValue(plr.id, "mode", keyName, played);

		keyName.Format("PlayedMap%s", m_mapName.c_str());
		hlp.PlayerCatValue(plr.id, "map", keyName, played);

		for (int i = 0; i < 4; ++i)
		{
			int used = int(stats.m_suit.m_used[i].GetSeconds() + 0.5f);
			if (!used)
				continue;
			keyName.Format("UsedSuit%d", i);
			hlp.PlayerCatValue(plr.id, "suit_mode", keyName, used);
		}

		for (int i = 0; i < stats.m_vehicles.m_vehicles.size(); ++i)
		{
			const SPlayerStats::SVehicles::SVehicle& veh = stats.m_vehicles.m_vehicles[i];
			int used = int(veh.m_used.GetSeconds() + 0.5f);
			keyName.Format("UsedVehicle%s", veh.m_name.c_str());
			hlp.PlayerCatValue(plr.id, "vehicle", keyName, used);
		}

		for (int i = 0; i < stats.m_weapons.m_weapons.size(); ++i)
		{
			const SPlayerStats::SWeapons::SWeapon& wep = stats.m_weapons.m_weapons[i];
			if (!wep.m_kills)
				continue;

			keyName.Format("WeaponKills%s", wep.m_name.c_str());
			hlp.PlayerCatValue(plr.id, "weapon", keyName, wep.m_kills);
		}
	}

	//Debugging stuff
	if (!hlp.m_errors.empty())
	{
		GameWarning("Stats tracking : SubmitPlayerStats tried to use %d missing key(s)", (int)hlp.m_errors.size());
		for (int i = 0; i < hlp.m_errors.size(); ++i)
		{
			GameWarning("     Key %s not found", hlp.m_errors[i].c_str());
		}
	}
}

void CGameStats::SubmitServerStats()
{
	SStatsTrackHelper hlp(this);

	string mode;
	if (IEntity* pGRE = gEnv->pGameFramework->GetIGameRulesSystem()->GetCurrentGameRulesEntity())
	{
		mode = pGRE->GetClass()->GetName();
	}
	else
		mode = gEnv->pConsole->GetCVar("sv_gamerules")->GetString();

	char strProductVersion[256];
	gEnv->pSystem->GetProductVersion().ToString(strProductVersion);

	string name;
	ICVar* pName = gEnv->pConsole->GetCVar("sv_servername");
	if (pName)
		name = pName->GetString();
	if (name.empty())
		name = gEnv->pNetwork->GetHostName();

	string levelname;
	GetLevelName(levelname);

	hlp.ServerValue("mapname", levelname);
	hlp.ServerValue("gametype", mode);
	hlp.ServerValue("hostname", name);
	hlp.ServerValue("gamever", strProductVersion);
}

void CGameStats::Init()
{
	// michiel - GS
	return;
}

void CGameStats::GetMemoryStatistics(ICrySizer* s)
{
	s->AddObject(this, sizeof(*this));
	s->AddObject(m_playerMap);
}

void CGameStats::SPlayerInfo::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(name);
	pSizer->AddObject(scores);
	pSizer->AddObject(stats);
}

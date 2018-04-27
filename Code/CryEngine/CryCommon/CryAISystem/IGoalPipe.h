// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct GoalParameters
{
	Vec3       vPos;
	Vec3       vPosAux;

	IAIObject* pTarget;

	float      fValue;
	float      fValueAux;

	int        nValue;
	int        nValueAux;

	bool       bValue;

	string     str;
	string     strAux;

	string     scriptCode;

	GoalParameters() :
		vPos(ZERO),
		vPosAux(ZERO),
		pTarget(0),
		fValue(0.f),
		fValueAux(0.f),
		nValue(0),
		nValueAux(0),
		bValue(false)
	{
	}

	GoalParameters& operator=(const GoalParameters& params)
	{
		if (&params != this)
		{
			vPos = params.vPos;
			vPosAux = params.vPosAux;

			pTarget = params.pTarget;

			fValue = params.fValue;
			fValueAux = params.fValueAux;

			nValue = params.nValue;
			nValueAux = params.nValueAux;

			bValue = params.bValue;

			str = params.str;
			strAux = params.strAux;

			scriptCode = params.scriptCode;
		}

		return *this;
	}

#ifdef SERIALIZE_DYNAMIC_GOALPIPES
	void Serialize(TSerialize ser);
#endif
};

enum EGoalOperations
{
	eGO_FIRST,
	eGO_ACQUIRETARGET = eGO_FIRST,
	eGO_ADJUSTAIM,
	eGO_PEEK,
	eGO_ANIMATION,
	eGO_ANIMTARGET,
	eGO_APPROACH,
	eGO_BACKOFF,
	eGO_BODYPOS,
	eGO_BRANCH,
	eGO_CHARGE,
	eGO_CLEAR,
	eGO_COMMUNICATE,
	eGO_DEVALUE,
	eGO_FIRECMD,
	eGO_FOLLOWPATH,
	eGO_HIDE,
	eGO_IGNOREALL,
	eGO_LOCATE,
	eGO_LOOK,
	eGO_LOOKAROUND,
	eGO_LOOKAT,
	eGO_PATHFIND,
	eGO_RANDOM,
	eGO_RUN,
	eGO_SCRIPT,
	eGO_SEEKCOVER,
	eGO_SIGNAL,
	eGO_SPEED,
	eGO_STANCE,
	eGO_STICK,
	eGO_STICKMINIMUMDISTANCE,
	eGO_STICKPATH,
	eGO_MOVE,
	eGO_SHOOT,
	eGO_TELEPORT,
	eGO_STRAFE,
	eGO_TACTICALPOS,
	eGO_TIMEOUT,
	eGO_TRACE,
	eGO_USECOVER,
	eGO_WAIT,
	eGO_WAITSIGNAL,
	eGO_HOVER,
	eGO_FLY,
	eGO_CHASETARGET,
	eGO_FIREWEAPONS,
	eGO_ACQUIREPOSITION,
	eGO_SET_ANIMATION_TAG,
	eGO_CLEAR_ANIMATION_TAG,

	eGO_LAST
};

struct IGoalOp;

struct GoalParams
{
	enum Denominator
	{
		eD_INVALID = 0,
		eD_BOOL,
		eD_NUMBER,
		eD_FNUMBER,
		eD_STRING,
		eD_VEC3,
	};

	struct _data
	{
		Denominator d;

		union
		{
			bool        boolean;
			int32       number;
			float       fnumber;
			float       vec[3];
			const char* str;
		};
	};

	_data                m_Data;
	const char*          m_Name;
	DynArray<GoalParams> m_Childs;

	GoalParams() : m_Name(0) { m_Data.d = eD_INVALID; }
	GoalParams(bool value) : m_Name(0) { SetValue(value); }
	GoalParams(int32 value) : m_Name(0) { SetValue(value); }
	GoalParams(float value) : m_Name(0) { SetValue(value); }
	GoalParams(const char* value) : m_Name(0) { SetValue(value); }

	void              SetValue(bool b)                   { m_Data.boolean = b; m_Data.d = eD_BOOL; }
	bool              GetValue(bool& b) const            { b = m_Data.boolean; return m_Data.d == eD_BOOL; }

	void              SetValue(int number)               { m_Data.number = number; m_Data.d = eD_NUMBER; }
	bool              GetValue(int& number) const        { number = m_Data.number; return m_Data.d == eD_NUMBER; }

	void              SetValue(uint32 number)            { m_Data.number = number; m_Data.d = eD_NUMBER; }
	bool              GetValue(uint32& number) const     { number = m_Data.number; return m_Data.d == eD_NUMBER; }

	void              SetValue(float fnumber)            { m_Data.fnumber = fnumber; m_Data.d = eD_FNUMBER; }
	bool              GetValue(float& fnumber) const     { fnumber = m_Data.fnumber; return m_Data.d == eD_FNUMBER; }

	void              SetValue(const char* str)          { m_Data.str = str; m_Data.d = eD_STRING; }
	bool              GetValue(const char*& str) const   { str = m_Data.str; return m_Data.d == eD_STRING; }

	void              SetValue(const Vec3& vec)          { m_Data.vec[0] = vec.x; m_Data.vec[1] = vec.y; m_Data.vec[2] = vec.z; m_Data.d = eD_VEC3; }
	bool              GetValue(Vec3& vec) const          { vec.Set(m_Data.vec[0], m_Data.vec[1], m_Data.vec[2]); return m_Data.d == eD_VEC3; }

	void              SetName(const char* name)          { m_Name = name; }
	const char*       GetName() const                    { return m_Name; }

	void              AddChild(const GoalParams& params) { m_Childs.push_back(params); }
	size_t            GetChildCount() const              { return m_Childs.size(); }
	GoalParams&       GetChild(uint32 index)             { assert(static_cast<DynArray<GoalParams>::size_type>(index) < m_Childs.size()); return m_Childs[index]; }
	const GoalParams& GetChild(uint32 index) const       { assert(static_cast<DynArray<GoalParams>::size_type>(index) < m_Childs.size()); return m_Childs[index]; }

	operator bool() { return m_Name != 0; }
};

struct IGoalPipe
{
	enum EGroupType
	{
		eGT_NOGROUP,
		eGT_GROUPWITHPREV,
		eGT_GROUPED,
		eGT_LAST
	};

	// <interfuscator:shuffle>
	virtual ~IGoalPipe() {}

	// TODO evgeny Further clean-up from here

	virtual const char* GetName() const = 0;
	virtual void        HighPriority() = 0;

	virtual void        PushGoal(const XmlNodeRef& goalOpNode, EGroupType eGrouping) = 0;

	//! Push an existing goalop to the pipe with given parameters.
	//! \note This new PushGoal shouldn't need all these parameters but will do for a while.
	virtual void PushGoal(IGoalOp* pGoalOp, EGoalOperations op, bool bBlocking, EGroupType eGrouping, const GoalParameters& params) = 0;

	virtual void PushGoal(EGoalOperations name, bool bBlocking, EGroupType eGrouping, GoalParameters& params) = 0;
	virtual void PushLabel(const char* label) = 0;
	virtual void PushPipe(const char* szName, bool bBlocking, EGroupType eGrouping, GoalParameters& params) = 0;
	virtual void SetDebugName(const char* name) = 0;

	virtual void ParseParams(const GoalParams& node) = 0;
	virtual void ParseParam(const char* param, const GoalParams& node) = 0;
	// </interfuscator:shuffle>
};

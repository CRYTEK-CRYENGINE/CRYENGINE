CreateAIBehavior("GruntBase", "AIBaseBehavior",
{
	OnHighCover = function(behavior, entity, sender, data)
		AI.SetStance(entity.id, STANCE_HIGH_COVER)
	end,

	OnLowCover = function(behavior, entity, sender, data)
		AI.SetStance(entity.id, STANCE_LOW_COVER)
	end,

	OnEnemySeen = function(behavior, entity, distance)
		if (behavior.parent.OnEnemySeen) then behavior.parent:OnEnemySeen(entity, distance) end

		entity:SafeKillTimer(entity.AI.targetHiddenTimer)
		entity.AI.targetHiddenTimer = nil
		AI.SetBehaviorVariable(entity.id, "TargetHidden", false)
	end,

	OnLostSightOfTarget = function(behavior, entity)
		if (behavior.parent.OnLostSightOfTarget) then behavior.parent:OnLostSightOfTarget(entity) end

		entity:SafeKillTimer(entity.AI.targetHiddenTimer)
		entity.AI.targetHiddenTimer = SetTimerForBehaviorFunction(2000, behavior.OnTargetHidden, behavior, entity)

		AI.SetBehaviorVariable(entity.id, "SeenTargetsLastKnownPosition", false)
	end,

	-- This function will be called with a delay in order to suppress oscillations.
	OnTargetHidden = function(behavior, entity)
		AI.SetBehaviorVariable(entity.id, "TargetHidden", true)
		entity.targetHiddenTimer = nil
	end,


	-- Request drone(s) to seek for an enemy at its last known position (or
	-- otherwise nearby the caller).
	SendDroneSeekCommand = function(behavior, entity)

		local targetPosition = {}

		if (not AI.GetAttentionTargetPosition(entity.id, targetPosition)) then
			targetPosition = entity:GetPos()
		end

		entity.AI.DroneSeekCommand =
		{
			time = _time,
			pos = targetPosition,
		}

		entity:Log("SendDroneSeekCommand")
		AI.Signal(SIGNALFILTER_ANYONEINCOMM,1,"OnDroneSeekCommand", entity.id)
	end,

	OnDroneAlert = function(behavior, entity, sender)
		--entity:Log("OnDronAlert - Base")

		local targetEntity = AI.GetAttentionTargetEntity(sender.id)
		local pos = targetEntity:GetPos()

		entity.AI.DroneAlert =
		{
			droneId = sender.id,
			time = _time,
			targetPos = pos,
		}
	end,

	-- TODO : Send the position through the signal instead of storing it in the entity
	--        and fetching it later
	SendSuspiciousActivitySignal = function(behavior, entity, position)
		entity.AI.suspiciousActivityPosition = entity.AI.suspiciousActivityPosition or {x=0, y=0, z=0}
		CopyVector(entity.AI.suspiciousActivityPosition, position)
		AI.Signal(SIGNALFILTER_ANYONEINCOMM,1,"OnSuspiciousActivity", entity.id)
	end,

	-- TODO : Add the concept of suspicious activity to the perception manager
	--        and use it instead of OnNewAttentionTarget/OnAttentionTargetThreatChanged
	OnNewAttentionTarget = function(behavior, entity, sender, data)
		if (AI.GetAttentionTargetPosition(entity.id, g_Vectors.temp_v1)) then
			--entity:Log("OnNewAttentionTarget - SendSuspiciousActivitySignal")
			behavior:SendSuspiciousActivitySignal(entity, g_Vectors.temp_v1)

			local targetType = data.iValue

			if (targetType == AITARGET_VISUAL) then
				local target = AI.GetAttentionTargetEntity(entity.id)
				if (target and target.Properties.GunTurret) then
					AI.SetBehaviorVariable(entity.id, "TargetIsTurret", true)
				else
					AI.SetBehaviorVariable(entity.id, "TargetIsTurret", false)
				end
			end
		end
	end,

	OnAttentionTargetThreatChanged = function(behavior, entity)
		if (AI.GetAttentionTargetPosition(entity.id, g_Vectors.temp_v1)) then
			--entity:Log("OnAttentionTargetThreatChanged - SendSuspiciousActivitySignal")
			behavior:SendSuspiciousActivitySignal(entity, g_Vectors.temp_v1)
		end
	end,

	OnIncomingProjectile = function(behavior, entity, sender, data)
		AI.SetBehaviorVariable(entity.id, "AvoidIncomingProjectile", true)
		entity.lastIncomingProjectileThreatRayPos = data.point
		entity.lastIncomingProjectileThreatRayNormal = data.point2
	end,

	-- This function is called by Scorchers to warn friendlies of incoming fire.
	OnScorchDanger = function(behavior, entity, sender, data)
		AI.SetBehaviorVariable(entity.id, "ImmediateThreatScorch", true)
		entity.lastExplosiveThreatPos = data.point
	end,
})

---------------------------------------------------------------------
-- Used by the Sniper behavior
CreateAIBehavior("GruntIdle", "GruntBase",
{
	Alertness = 0,

	Constructor = function (behavior, entity)
		behavior:CheckStance(entity)
		entity:SelectPipe(0, "Empty")
	end,

	CheckStance = function(behavior, entity)
		if (not entity.idleStanceHasBeenSet) then
			entity.idleStanceHasBeenSet = true
			AI.SetStance(entity.id, STANCE_RELAXED)
		end
	end,
})
CreateAIBehavior("GruntSniperEngage", "GruntBase",
{
	Alertness = 2,

	Constructor = function(behavior, entity)
		entity:SelectPipe(0, "GruntSniperFindAndMoveToSniperSpot")
	end,

	SniperSpotReached = function(behavior, entity)
		entity:SelectPipe(0, "GruntSniperEngageFromSniperSpot")
	end,

	MoveSniperSpotFailed = function(behavior, entity)
		entity:OnError("GoToSniperSpotFailed")
	end,

	SniperSpotNotFound = function(behaviot, entity)
		entity:OnError("SniperSpotNotFound")
	end,

	OnBulletRain = function(behavior, entity)
		AI.SetBehaviorVariable(entity.id, "UseCover", true)
	end,

	OnEnemyDamage = function(behavior, entity)
		AI.SetBehaviorVariable(entity.id, "UseCover", true)
	end,
})

CreateAIBehavior("GruntSniperMoveToCover", "GruntBase",
{
	Alertness = 2,
	LeaveCoverOnStart = false,

	Constructor = function(behavior, entity)
		AI.SetFireMode(entity.id, FIREMODE_OFF)
		AI.SetStance(entity.id, STANCE_STAND)
		entity:SelectPipe(0, "GruntSniperFindAndMoveToCover")
	end,

	OnCoverCompromised = function(behavior, entity)
		entity:StartOrRestartPipe("GruntSniperFindAndMoveToCover")
	end,

	CoverNotFound = function(behavior, entity)
		--entity:Log("Cover not found!")
		AI.SetBehaviorVariable(entity.id, "UseCover", false)
	end,
})

CreateAIBehavior("GruntSniperHideInCover", "GruntBase",
{
	Alertness = 2,
	LeaveCoverOnStart = false,

	Constructor = function(behavior, entity)
		entity:SelectPipe(0, "GruntSniperHideInCover")
	end,

	OnCoverCompromised = function(behavior, entity)
		entity:LeaveCover(entity)
	end,

	OnBulletRain = function(behavior, entity)
		entity:StartOrRestartPipe("GruntSniperHideInCover")
	end,

	OnEnemyDamage = function(behavior, entity)
		entity:StartOrRestartPipe("GruntSniperHideInCover")
	end,

	DoneHiding = function(behavior, entity)
		AI.SetBehaviorVariable(entity.id, "UseCover", false)
	end,
})

CreateAIBehavior("GruntAssault", "GruntBase",
{
	Alertness = 2,

	Constructor = function(behavior, entity)
		AI.SetFireMode(entity.id, FIREMODE_BURST_WHILE_MOVING)
		AI.SetStance(entity.id, STANCE_STAND)
		entity:SelectPipe(0, "GruntAssaultEngageFromOpenArea")
	end,
})

---------------------------------------------------------------------

CreateAIBehavior("GruntDefendArea", "GruntBase",
{
	Constructor = function(behavior, entity)
		behavior:AnalyzeSituation(entity)
		behavior:SelectWeapon(entity)
	end,

	AnalyzeSituation = function(behavior, entity)
		if (DefendAreaAssignment:IsInsideDefendArea(entity)) then
			if (AI.GetAttentionTargetAIType(entity.id) ~= AIOBJECT_NONE) then
				AI.SetRefPointPosition(entity.id, entity.AI.defendArea.position)
				entity:StartOrRestartPipe("DefendAreaStandAndCheckForCloseCover")
			else
				entity:StartOrRestartPipe("Empty")
			end
		else
			AI.SetBehaviorVariable(entity.id, "InsideDefendArea", false)
		end
	end,

	OnEnemySeen = function(behavior, entity)
		if (behavior.parent.OnEnemySeen) then
			behavior.parent:OnEnemySeen(entity)
		end

		behavior:AnalyzeSituation(entity)
	end,

	OnCoverCompromised = function (behavior, entity, sender, data)
		behavior:AnalyzeSituation(entity)
	end,

	Reset = function(behavior, entity)
		behavior:AnalyzeSituation(entity)
	end,
})

CreateAIBehavior("GruntDefendAreaInCover", "GruntBase",
{
	LeaveCoverOnStart = false,

	Constructor = function(behavior, entity)
		entity:SelectPipe(0, "GruntFireInCover")
	end,

	OnCoverCompromised = function (behavior, entity, sender, data)
		entity:LeaveCover()
	end,

	Reset = function(behavior, entity)
		entity:LeaveCover()
	end,
})

CreateAIBehavior("GruntMoveToDefendArea", "GruntBase",
{
	Constructor = function(behavior, entity)
		behavior:AnalyzeSituation(entity)
		behavior:SelectWeapon(entity)
	end,

	Destructor = function(behavior, entity)
		entity:SafeKillTimer(entity.fallbackTimer)
	end,

	AnalyzeSituation = function(behavior, entity)
		if (DefendAreaAssignment:IsInsideDefendArea(entity)) then
			AI.SetBehaviorVariable(entity.id, "InsideDefendArea", true)
		else
			behavior:MoveToDefendArea(entity)
		end
	end,

	MoveToDefendArea = function(behavior, entity)
		AI.SetRefPointPosition(entity.id, entity.AI.defendArea.position)
		if (AI.GetAttentionTargetAIType(entity.id) ~= AIOBJECT_NONE) then
			entity:StartOrRestartPipe("GruntMoveToDefendAreaUsingCover")
		else
			entity:StartOrRestartPipe("GruntMoveToDefendArea")
		end
	end,

	OnNoPathFound = function(behavior, entity, sender, data)
		entity:OnError("Could not find a path to the defend area")
		entity:SelectPipe(0, "StandAndShoot");
		entity.fallbackTimer = Script.SetTimer(random(2500, 5000), function() behavior:AnalyzeSituation(entity); end)
	end,

	OnCoverCompromised = function (behavior, entity, sender, data)
		behavior:AnalyzeSituation(entity)
	end,

	Reset = function(behavior, entity)
		behavior:AnalyzeSituation(entity)
	end,
})

CreateAIBehavior("GruntHoldPosition", "GruntBase",
{
	Constructor = function(behavior, entity)
		behavior:AnalyzeSituation(entity)
		behavior:SelectWeapon(entity)
	end,

	AnalyzeSituation = function(behavior, entity)
		if (HoldPositionAssignment:IsAtHoldPosition(entity)) then
			if(AI.GetAttentionTargetAIType(entity.id) ~= AIOBJECT_NONE and not entity.AI.holdPosition.skipCover) then
				AI.SetRefPointPosition(entity.id, entity.AI.holdPosition.position)
				entity:SelectPipe(0, "HoldPositionStandAndCheckForCloseCover")
			else
				entity:SelectPipe(0, "Empty")
			end
		else
			AI.SetBehaviorVariable(entity.id, "AtHoldPosition", false)
		end
	end,

	OnEnemySeen = function(behavior, entity)
		if (behavior.parent.OnEnemySeen) then
			behavior.parent:OnEnemySeen(entity)
		end

		behavior:AnalyzeSituation(entity)
	end,

	OnCoverCompromised = function (behavior, entity, sender, data)
		behavior:AnalyzeSituation(entity)
	end,

	Reset = function(behavior, entity)
		behavior:AnalyzeSituation(entity)
	end,
})

CreateAIBehavior("GruntHoldPositionInCover", "GruntBase",
{
	LeaveCoverOnStart = false,

	Constructor = function(behavior, entity)
		entity:SelectPipe(0, "GruntFireInCover")
	end,

	OnCoverCompromised = function (behavior, entity, sender, data)
		entity:LeaveCover()
	end,

	Reset = function(behavior, entity)
		entity:LeaveCover()
	end,
})

CreateAIBehavior("GruntMoveToHoldPosition", "GruntBase",
{
	Constructor = function(behavior, entity)
		behavior:AnalyzeSituation(entity)
		behavior:SelectWeapon(entity)
	end,

	Destructor = function(behavior, entity)
		entity:SafeKillTimer(entity.fallbackTimer)
	end,

	AnalyzeSituation = function(behavior, entity)
		if (HoldPositionAssignment:IsAtHoldPosition(entity)) then
			AI.SetBehaviorVariable(entity.id, "AtHoldPosition", true)
		else
			behavior:MoveToHoldPosition(entity)
		end
	end,

	MoveToHoldPosition = function(behavior, entity)
		AI.SetRefPointPosition(entity.id, entity.AI.holdPosition.position)
		AI.SetRefPointDirection(entity.id, entity.AI.holdPosition.direction)
		if (AI.GetAttentionTargetAIType(entity.id) ~= AIOBJECT_NONE and entity.AI.holdPosition.useCover) then
			entity:StartOrRestartPipe("GruntMoveToHoldPositionUsingCover")
		else
			entity:StartOrRestartPipe("GruntMoveToHoldPosition")
		end
	end,

	OnNoPathFound = function(behavior, entity, sender, data)
		entity:OnError("Could not find a path to the hold position")
		entity:SelectPipe(0, "StandAndShoot");
		entity.fallbackTimer = Script.SetTimer(random(2500, 5000), function() behavior:AnalyzeSituation(entity); end)
	end,

	OnCoverCompromised = function (behavior, entity, sender, data)
		behavior:AnalyzeSituation(entity)
	end,

	Reset = function(behavior, entity)
		behavior:AnalyzeSituation(entity)
	end,
})

CreateAIBehavior("GruntCombatMove", "GruntBase",
{
	Constructor = function(behavior, entity)
		behavior:Move(entity)
		behavior:SelectWeapon(entity)
	end,

	Destructor = function(behavior, entity)
		SafeKillBehaviorTimer(entity, entity.checkCoverTimer)
		SafeKillBehaviorTimer(entity, entity.fallbackTimer)
	end,

	Move = function(behavior, entity)
		AI.SetRefPointPosition(entity.id, entity.AI.combatMove.position)
		if (AI.GetAttentionTargetEntity(entity.id)) then
			behavior:MoveWithCover(entity)
		else
			behavior:MoveDirectly(entity)
		end
	end,

	MoveDirectly = function(behavior, entity)
		entity:StartOrRestartPipe("GruntCombatMoveDirectly")
	end,

	MoveWithCover = function(behavior, entity)
		entity:StartOrRestartPipe("GruntCombatMoveWithCover")
	end,

	MoveDone = function(behavior, entity)
		if (DistanceSqVectors(entity:GetWorldPos(), entity.AI.combatMove.position) > 25) then
			behavior:Move(entity)
		else
			entity:ClearAssignment()
		end
	end,

	StartCoverCheckTimer = function(behavior, entity)
		SafeKillBehaviorTimer(entity, entity.checkCoverTimer)
		entity.checkCoverTimer = SetTimerForBehaviorFunction(2000, behavior.CheckCover, behavior, entity)
	end,

	StopCoverCheckTimer = function(behavior, entity)
		SafeKillBehaviorTimer(entity, entity.checkCoverTimer)
	end,

	CheckCover = function(behavior, entity)
		if (AI.GetTacticalPoints(entity.id, "CombatMoveSimple", g_Vectors.temp)) then
			behavior:MoveWithCover(entity)
		else
			entity.checkCoverTimer = SetTimerForBehaviorFunction(2000, behavior.CheckCover, behavior, entity)
		end
	end,

	OnEnemySeen = function(behavior, entity, distance)
		if (behavior.parent.OnEnemySeen) then behavior.parent:OnEnemySeen(entity, distance) end
		behavior:MoveWithCover(entity)
	end,

	OnNoPathFound = function(behavior, entity, sender, data)
		entity:OnError("Could not find a path to the hold position")
		entity:SelectPipe(0, "StandAndShoot");
		entity.fallbackTimer = SetTimerForBehaviorFunction(random(2500, 5000), behavior.Move, behavior, entity)
	end,

	OnCoverCompromised = function (behavior, entity, sender, data)
		behavior:MoveWithCover(entity)
	end,

	Reset = function(behavior, entity)
		behavior:Move(entity)
	end,
})

CreateAIBehavior("GruntCombatMoveInCover", "GruntBase",
{
	LeaveCoverOnStart = false,

	Constructor = function(behavior, entity)
		entity:SelectPipe(0, "GruntBriefShootFromCover")
	end,

	OnCoverCompromised = function (behavior, entity, sender, data)
		entity:LeaveCover()
	end,

	Reset = function(behavior, entity)
		entity:LeaveCover()
	end,
})
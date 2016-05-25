-- AI_Utils - Common AI utility functions.
-- Created by Mikko
--------------------------
Script.ReloadScript("Scripts/Entities/AI/AITerritory.lua");


AI_Utils = {
}

function AI_Utils:VerifyGroupBlackBoard(groupId)
	if (not AIBlackBoard[groupId]) then
		AIBlackBoard[groupId] = {};
	end
end

---------------------------------------------
function AI_Utils:SetupTerritory(entity)
	-- (evgeny 25 Sep 2009) Setup Wave as a bonus (but let's keep the original function name)

	local territory = entity.PropertiesInstance.AITerritoryAndWave.aiterritory_Territory;
	if (territory == "<None>") then
		AI.SetTerritoryShapeName(entity.id, "<None>");
		entity.AI.TerritoryShape = nil;
		entity.AI.Wave           = nil;
		return;
	end
	
	if ((not territory) or (territory == "") or (territory == "<Auto>")) then
		territory = AI.GetEnclosingGenericShapeOfType(entity:GetPos(), AIAnchorTable.COMBAT_TERRITORY, 1);
	end
	
	if (territory and (territory ~= "")) then
		AI.SetTerritoryShapeName(entity.id, territory);
	else
		AI.SetTerritoryShapeName(entity.id, "<None>");
		entity.AI.TerritoryShape = nil;
		entity.AI.Wave           = nil;
		return;
	end
	entity.AI.TerritoryShape = territory;

	local wave = entity.PropertiesInstance.AITerritoryAndWave.aiwave_Wave;
	if ((wave == "<None>") or (wave == "")) then
		wave = nil;
	end
	entity.AI.Wave = wave;

	AddToTerritoryAndWave(entity);
end

---------------------------------------------
function AI_Utils:SetupStandby(entity, force)
	-- Setup standby.
	entity.AI.StandbyShape = AI.GetEnclosingGenericShapeOfType(entity:GetPos(), AIAnchorTable.ALERT_STANDBY_IN_RANGE, 0);

	if(force and force == true and not entity.AI.StandbyShape) then
		entity.AI.StandbyShape = AI.CreateTempGenericShapeBox(entity:GetPos(), 15, 0, AIAnchorTable.ALERT_STANDBY_IN_RANGE);
	end

	if(entity.AI.StandbyShape) then
		AI.SetRefShapeName(entity.id, entity.AI.StandbyShape);
	else
		AI.SetRefShapeName(entity.id, "");
	end
end

---------------------------------------------
function AI_Utils:CanThrowGrenade(entity, smokeOnly)

	if (not AIBlackBoard.lastGrenadeTime) then
		AIBlackBoard.lastGrenadeTime = _time - 20;
	end

	local dt = _time - AIBlackBoard.lastGrenadeTime;
	if (dt < 9) then
--		System.Log(">>TIME! "..dt);
		return 0;
	end

	-- no need for distance/target checks if throwing smoke grenade (should be controlled by level setup)
	if(smokeOnly == nil)then
		local targetType = AI.GetTargetType(entity.id);
		if (targetType ~= AITARGET_MEMORY and targetType ~= AITARGET_ENEMY) then
	--		System.Log(">>BAD TARGET ");
			return 0;
		end
	
		local targetDist = AI.GetAttentionTargetDistance(entity.id);
		if (targetDist < 10.0 or targetDist > 45.0) then
	--		System.Log(">>BAD DISTANCE "..targetDist);
			return 0;
		end
	end
	local grenadesWeaponId = entity.inventory:GetGrenadeWeaponByClass("AIGrenades");
	
	if(smokeOnly) then
		grenadesWeaponId = entity.inventory:GetGrenadeWeaponByClass("AISmokeGrenades");
		if (grenadesWeaponId==nil) then
--			System.Log(">>NO GRENADES!");
			return 0;
		end
	else
		if (grenadesWeaponId==nil) then
			grenadesWeaponId = entity.inventory:GetGrenadeWeaponByClass("AIFlashbangs");
			if (grenadesWeaponId==nil) then
				return 0;
--				grenadesWeaponId = entity.inventory:GetGrenadeWeaponByClass("AISmokeGrenades");
--				if (grenadesWeaponId==nil) then
		--			System.Log(">>NO GRENADES!");
--					return 0;
--				end
			end
		end	
	end
		
	local grenadesWeapon = System.GetEntity(grenadesWeaponId);
	if (grenadesWeapon==nil) then
--		System.Log(">>NO WEAPON!");
		return 0;
	end
	if (grenadesWeapon.weapon:GetAmmoCount() <= 0) then
--		System.Log(">>NO AMMO!");
		return 0;
	end

	AIBlackBoard.lastGrenadeTime = _time;

	return 1;

end

---------------------------------------------
function AI_Utils:CanThrowSmokeGrenade(entity)

	if (not AIBlackBoard.lastSmokeGrenadeTime) then
		AIBlackBoard.lastSmokeGrenadeTime = _time - 40;
	end

	local dt = _time - AIBlackBoard.lastSmokeGrenadeTime;
	if (dt < 30) then
--		System.Log(">>TIME! "..dt);
		return 0;
	end

	-- no need for distance/target checks if throwing smoke grenade (should be controlled by level setup)
	local targetType = AI.GetTargetType(entity.id);
	if (targetType ~= AITARGET_MEMORY and targetType ~= AITARGET_ENEMY) then
--		System.Log(">>BAD TARGET ");
		return 0;
	end

	local targetDist = AI.GetAttentionTargetDistance(entity.id);
	if (targetDist < 20.0 or targetDist > 60.0) then
--		System.Log(">>BAD DISTANCE "..targetDist);
		return 0;
	end

	local grenadesWeaponId = entity.inventory:GetGrenadeWeaponByClass("AISmokeGrenades");
	if (grenadesWeaponId==nil) then
		return 0;
	end
	local grenadesWeapon = System.GetEntity(grenadesWeaponId);
	if (grenadesWeapon==nil) then
--		System.Log(">>NO WEAPON!");
		return 0;
	end
	if (grenadesWeapon.weapon:GetAmmoCount() <= 0) then
--		System.Log(">>NO AMMO!");
		return 0;
	end

	AIBlackBoard.lastSmokeGrenadeTime = _time;

	return 1;

end


function AI_Utils:HasPlayerAsAttentionTarget(entityId)
	local attentionTargetType = AI.GetAttentionTargetAIType(entityId);	
	if ( attentionTargetType == AIOBJECT_PLAYER ) then
		return true
	end	
	return false
end

function AI_Utils:AttentionTargetIsPlayerOrAssociatedWithPlayer(entityId)
	local attentionTargetType = AI.GetAttentionTargetAIType(entityId)
	if ( attentionTargetType == AIOBJECT_PLAYER ) then
		return true
	elseif ( attentionTargetType == AIOBJECT_DUMMY) then
		local attentionTargetEntity = AI.GetAttentionTargetEntity(entityId)
		if(attentionTargetEntity ~= nil) then
			local entityType = AI.GetTypeOf(attentionTargetEntity.id)
			if( entityType == AIOBJECT_PLAYER ) then
				return true
			end
		end
	end	
	return false
end

function AI_Utils:AttentionTargetIsTurretOrAssociatedWithTurret(entityId)
	local attentionTargetType = AI.GetAttentionTargetAIType(entityId)
	if ( attentionTargetType == AIOBJECT_TARGET or attentionTargetType == AIOBJECT_DUMMY) then
		local attentionTargetEntity = AI.GetAttentionTargetEntity(entityId)
		if(attentionTargetEntity ~= nil) then
			local entityClass = System.GetEntityClass(attentionTargetEntity.id)
			if( entityClass == "Turret" ) then
				return true
			end
		end
	end	
	return false
end

-- If the target is right above, you'll get +90 degrees
-- If the target is right below, you'll get -90 degrees
-- If the target is right ahead, you'll get 0 degrees
-- If anything goes wrong, you'll get 0 degrees
function GetTargetElevationAngleInDegrees(entity)
	if (entity == nil) then
		return 0.0
	end
	
	local targetPosition = g_Vectors.temp_v1
	if (not AI.GetAttentionTargetPosition(entity.id, targetPosition)) then
		return 0.0
	end
	
	local entityPosition = entity:GetWorldPos(g_Vectors.temp_v2)
	local entityToTargetDirection = GetDirection(entityPosition, targetPosition)
	local angle = GetAngleBetweenVectors(g_Vectors.down, entityToTargetDirection)	
	local targetElevationAngleInDegrees = math.deg(angle) - 90.0
	
	--Log("targetElevationAngleInDegrees = %f", targetElevationAngleInDegrees)
	
	return targetElevationAngleInDegrees
end

---------------------------------------------
function SetTimerForBehaviorFunction(timeMs, functionToCall, behavior, entity, extraData)
	local userData = {behavior = behavior, entity = entity, extraData = extraData}
	local functionTimer = Script.SetTimerForFunction(timeMs, "OnBehaviorFunctionTimerDone", userData)
	if (entity.AI.behaviorFuntions == nil) then
		entity.AI.behaviorFuntions = {}
	end
	entity.AI.behaviorFuntions[functionTimer] = functionToCall
	return functionTimer
end

function OnBehaviorFunctionTimerDone(userData, functionTimer)
	userData.entity.AI.behaviorFuntions[functionTimer](userData.behavior, userData.entity, userData.extraData)
	userData.entity.AI.behaviorFuntions[functionTimer] = nil
end

function SafeKillBehaviorTimer(entity, functionTimer)
	if (functionTimer and entity.AI.behaviorFuntions and entity.AI.behaviorFuntions[functionTimer]) then
		entity.AI.behaviorFuntions[functionTimer] = nil
		Script.KillTimer(functionTimer)
	end
end

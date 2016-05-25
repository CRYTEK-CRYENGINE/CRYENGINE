-- AICONFIG 
-- Used to load/reload everything related to ai

AI.LogEvent("[AISYSTEM] AIConfig starts  -------------------------------------");

-- DEBUG
Script.ReloadScript("Scripts/AI/Anchor.lua");
Script.ReloadScript("Scripts/AI/Group.lua");

Script.ReloadScript("Scripts/AI/pathfindProperties.lua");
Script.ReloadScript("Scripts/AI/GoalPipes/PipeManager.lua");
Script.ReloadScript("Scripts/AI/AIBehavior.lua");
Script.ReloadScript("Scripts/AI/AIGroupBehavior.lua");

Script.ReloadScript("Scripts/AI/Formations/FormationManager.lua");

Script.ReloadScript("Scripts/AI/Logic/AI_Utils.lua");
Script.ReloadScript("SCRIPTS/AI/Logic/BlackBoard.lua");

function ReloadTPS()
	AI.DestroyAllTPSQueries();
	Script.ReloadScript("Scripts/AI/TacticalPositionManager.lua");
	AI.TacticalPositionManager:OnInit();
end

function ReloadTPSExtensions()
	ReloadTPS();
	
	if (PipeManager) then
		PipeManager:OnInit();
	end
end

function AIReload()
	ReloadTPSExtensions();
	assignmentsHaveBeenCreated = nil
end

if (FormationManager) then
	FormationManager:OnInit();
end

-- SETTING UP THE PLAYER ASSESSMENT MULTIPLIER
-- should be in the range [0,1] BUT can be bigger - forcing something be very important (grenades)
-- if multiplayer is more than 1, object will not be ignored with "ignoreall" goal
AI.SetAssesmentMultiplier(AIOBJECT_PLAYER,1.0);	-- let's make squadmates preferred enemies
AI.SetAssesmentMultiplier(AIOBJECT_ATTRIBUTE,1.0);
AI.SetAssesmentMultiplier(AIOBJECT_RPG, 100); -- grenade is very important

--------------------------------------------------
--define combat classes
AICombatClasses = {};

--this will reset all existing combat classes
AI.AddCombatClass();	

--	ATTENTION!!! DON'T make the scale multiplier 0 unlees you really need this class to be ignored
AICombatClasses.Player = 0;
AICombatClasses.PlayerRPG = 1;
AICombatClasses.Infantry = 2;
AICombatClasses.InfantryRPG	= 3;
AICombatClasses.Tank = 4;
AICombatClasses.TankHi = 5;
AICombatClasses.Heli = 6;
AICombatClasses.VehicleGunner	= 7;
AICombatClasses.Hunter = 8;
AICombatClasses.Civilian = 9;
AICombatClasses.Car = 10;
AICombatClasses.Warrior = 11;
AICombatClasses.AAA = 12;
AICombatClasses.BOAT = 13;
AICombatClasses.APC = 14;
AICombatClasses.Squadmate = 15;
AICombatClasses.Scout = 16;
AICombatClasses.ascensionScout = 17;
AICombatClasses.ascensionVTOL = 18;
AICombatClasses.ascensionScout2 = 19;
																							--		0			1			2			3			4			5			6			7			8			9			10		11		12		13		14		15		16		17		18		19
AI.AddCombatClass(AICombatClasses.Player,					{	1.0,	1.0,	1.0,	1.0,	1.1,	1.1,	1.0,	1.0,	2.0,	0.0,	0.0,	0.0,	1.0,	1.0,	1.0,	0.1,	1.0,	1.0,	0.0,	0.0,   } );																								
AI.AddCombatClass(AICombatClasses.PlayerRPG,			{	1.0,	1.0,	1.0,	1.0,	1.5,	1.5,	1.0,	1.0,	2.0,	0.0,	1.2,	0.0,	1.2,	1.0,	1.2,	0.1,	1.0,	1.0,	0.0,	0.0,   } );	
AI.AddCombatClass(AICombatClasses.Infantry,				{	1.0,	1.0,	1.0,	1.0,	1.1,	1.1,	1.0,	1.0,	2.0,	0.0,	0.0,	0.0,	1.0,	1.0,	1.0,	0.1,	1.0,	1.0,	0.0,	0.0,   } );	
AI.AddCombatClass(AICombatClasses.InfantryRPG,		{	1.0,	1.0,	1.0,	1.0,	1.5,	1.5,	1.0,	1.0,	2.0,	0.0,	1.2,	0.0,	1.2,	1.0,	1.2,	0.1,	1.0,	1.0,	0.0,	0.0,   } );	
AI.AddCombatClass(AICombatClasses.Tank,						{	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	0.0,	1.0,	0.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	0.0,	1.0,   }, "OnTankSeen" );	
AI.AddCombatClass(AICombatClasses.TankHi,					{	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	0.0,	1.0,	0.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	0.0,	1.0,   }, "OnTankSeen" );	
AI.AddCombatClass(AICombatClasses.Heli,						{	1.0,	1.0,	1.0,	1.0,	0.9,	0.9,	1.0,	1.0,	1.0,	0.0,	1.0,	0.0,	0.9,	1.0,	0.9,	1.0,	1.0,	1.0,	0.0,	1.0,   }, "OnHeliSeen" );	
AI.AddCombatClass(AICombatClasses.VehicleGunner,	{	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	0.0,	1.0,	0.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	0.0,	1.0,   } );	-- for the vehicle gunner. 1.5 means, I want to set the same priority as the player.
AI.AddCombatClass(AICombatClasses.Hunter,					{	1.0,	1.2,	0.8,	1.0,	2.0,	2.0,	2.0,	1.0,	2.0,	0.0,	1.0,	0.0,	2.0,	2.0,	2.0,	0.8,	1.5,	0.0,	0.0,	0.0,   } );	
AI.AddCombatClass(AICombatClasses.Civilian,				{	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	0.0,	0.5,	0.0,	1.0,	1.0,	1.0,	0.1,	1.0,	0.0,	0.0,	0.0,   } );	
AI.AddCombatClass(AICombatClasses.Car,						{	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,   } );	
AI.AddCombatClass(AICombatClasses.Warrior,				{	0.05,	0.2,	0.05,	0.2,	0.2,	1.5,	2.0,	1.5,	0.05,	0.05,	1.5,	1.5,	1.5,	0.2,	1.5,	0.05,	0.05,	0.0,	0.0,	0.0,   } );	
AI.AddCombatClass(AICombatClasses.AAA,						{	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	0.0,	1.0,	0.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	0.0,	1.0,   }, "OnTankSeen" );	
AI.AddCombatClass(AICombatClasses.BOAT,						{	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	0.0,	1.0,	0.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	0.0,	1.0,   }, "OnBoatSeen" );	
AI.AddCombatClass(AICombatClasses.APC,						{	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	0.0,	1.0,	0.0,	1.0,	1.0,	1.0,	1.0,	1.0,	1.0,	0.0,	1.0,   }, "OnTankSeen" );	
AI.AddCombatClass(AICombatClasses.Squadmate,			{	1.0,	1.0,	1.0,	1.0,	1.1,	1.1,	1.0,	1.0,	2.0,	0.0,	0.0,	0.0,	1.0,	1.0,	1.0,	1.0,	1.0,	0.0,	0.0,	0.0,   } );	
AI.AddCombatClass(AICombatClasses.Scout,					{	1.0,  1.0,  0.7,	0.7,	0.9,	0.9,	1.0,	0.7,	1.0,	0.0,	1.0,	0.0,	0.9,	0.9,	0.9,	0.7,	1.0,	1.0,	0.0,	0.0,   } );	
AI.AddCombatClass(AICombatClasses.ascensionScout,	{	1.0,	1.0,	0.0,	0.5,	0.5,  0.0,	1.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,   } );	
AI.AddCombatClass(AICombatClasses.ascensionVTOL,	{	1.0,	1.0,	0.0,	0.0,	0.0, 	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,   } );	
AI.AddCombatClass(AICombatClasses.ascensionScout2,{	1.0,	1.0,	0.0,	0.7,	0.7,  0.0,	1.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,	0.0,   } );	


function AIReset()
	AIBlackBoard_Reset();
	spectacularKillInProgress = false
	lastWalkabilityCheckTime = false
end

function AI:OnSave(save)
	save.AIBlackBoard = AIBlackBoard;
end

function AI:OnLoad(saved)
	AIBlackBoard_Reset();
	merge(AIBlackBoard,saved.AIBlackBoard);
end

function AI:OnEditorMoveSimulation(entity, goalId, pos)

	AI.SetRefPointPosition(entity.id, pos)
	entity:InsertSubpipe(AIGOALPIPE_SAMEPRIORITY, "MoveToMiddleClickPositionInEditor", nil, goalId)

end

AI.LogEvent("[AISSYSTEM] CONFIG SCRIPT FILE LOADED. --------------------------")

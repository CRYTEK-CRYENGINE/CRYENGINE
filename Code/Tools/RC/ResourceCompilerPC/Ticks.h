// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _TICKS_
#define _TICKS_


#define TICKS_PER_SECOND (30)						//the maximum keyframe value is 4800Hz per second   
#define SECONDS_PER_TICK (1.0f/30.0f)		//the maximum keyframe value is 4800Hz per second   
#define TICKS_PER_FRAME (1)							//if we have 30 keyframes per second, then one tick is 1
#define TICKS_CONVERT (160)

//#define TICKS_PER_SECOND (120)					//the maximum keyframe value is 4800Hz per second   
//#define SECONDS_PER_TICK (1.0f/120.0f)	//the maximum keyframe value is 4800Hz per second   
//#define TICKS_PER_FRAME (4)							//if we have 30 keyframes per second, then one tick is 4
//#define TICKS_CONVERT (40)

//#define TICKS_PER_SECOND (4800)						//the maximum keyframe value is 4800Hz per second   
//#define SECONDS_PER_TICK (1.0f/4800.0f)		//the time for 1 tick    
//#define TICKS_PER_FRAME (160)							//if we have 30 keyframes per second, then one tick is 160
//#define TICKS_CONVERT (1)  

#endif
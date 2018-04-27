// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#define NUMMARKWORDS 10
#define WATERMARKDATA(name) uint name[] = { 0xDEBEFECA, 0xFABECEDA, 0xADABAFBE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

// (the name is such that you can have multiple watermarks in one exe, don't use
// names like "watermark" just incase you accidentally give out an exe with
// debug information).

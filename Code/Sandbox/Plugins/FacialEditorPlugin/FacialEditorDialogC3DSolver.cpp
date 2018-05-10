// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Util/Image.h"
#include "FacialEditorDialog.h"
#include "QtViewPane.h"
#include "SelectAnimationDialog.h"
#include <CrySystem/File/IAVI_Reader.h>
#include <CryMovie/IMovieSystem.h>
#include "JoystickUtils.h"

#include <Cry3DEngine/I3DEngine.h>
#include <CryAnimation/ICryAnimation.h>

#include "Controls/QuestionDialog.h"

// c3d stuff
//////////////////////////////////////////////////////////////////////////
void RPF(int* offset, char* type, char* dim, FILE* infile)
{
	char offset_low, offset_high;
	// Read Parameter Format for the variable following the
	// parameter name
	//		offset = number of bytes to start of next parameter (2 Bytes, reversed order: low, high)
	//		T = parameter type: -1 = char, 1 = boolean, 2 = int, 3 = float
	//		D = total size of array storage (incorrect, ignore)
	//		d = number of dimensions for an array parameter (d=0 for single value)
	//		dlen = length of data
	fread(&offset_low, sizeof(char), 1, infile);   // byte 1
	fread(&offset_high, sizeof(char), 1, infile);  // byte 2
	*offset = 256 * offset_high + offset_low;
	fread(type, sizeof(char), 1, infile);       // byte 3
	fread(dim, sizeof(char), 1, infile);        // byte 4
}

// markers definitions
//////////////////////////////////////////////////////////////////////////
#define FA_LT_EYEBROW_LT 0  // LBRMID:           left_eyebrow_left
#define FA_LT_EYEBROW_RT 1  // LBRIN:            left_eyebrow_right
#define FA_RT_EYEBROW_LT 2  // RBRIN:             right_eyebrow_left
#define FA_RT_EYEBROW_RT 3  // RBRMID:           right_eyebrow_right
#define FA_LT_CHEEK      4  // LCHEUP           left_cheek
#define FA_RT_CHEEK      5  // RCHEUP           right_cheek
#define FA_MOUTH_LT      6  // LIPLCR             left_mouthcorner
#define FA_MOUTH_RT      7  // LIPRCR             right_mouthcorner
#define FA_LT_LIP_TOP    8  // LIPUPL             left_lip_top
#define FA_RT_LIP_TOP    9  // LIPUPR             right_lip_top
#define FA_LT_LIP_BOTTOM 10 // LIPLOL             left_lip_bottom
#define FA_RT_LIP_BOTTOM 11 // LIPLOR             right_lip_bottom
#define FA_LT_NOSE       12 // LNOST              left_nose
#define FA_RT_NOSE       13 // RNOST             right_nose
#define FA_LT_EYELID_TOP 14 // LLLID               left_eyelid_upper
#define FA_LT_EYELID_LOW 15 // LULID               left_eyelid_lower
#define FA_RT_EYELID_TOP 16 // RLLID               right_eyelid_upper
#define FA_RT_EYELID_LOW 17 // RULID               righ_eyelid_lower

#define FA_CHIN          18  // C_CHIN                 chin
#define FA_CHIN_LT       19  // L_CHIN                 chin left
#define FA_CHIN_RT       20  // R_CHIN                 chin right

#define FA_LT_JAWMID     21        // L_JAWMID       left jawmid
#define FA_RT_JAWMID     22        // R_JAWMID       right jawmid
#define FA_LT_JAWOUT     23        // L_JAWOUT       left jawout
#define FA_RT_JAWOUT     24        // R_JAWOUT       right jawout

#define FA_LT_CHEBCK     25        // L_CHEBCK       left cheek back
#define FA_RT_CHEBCK     26        // R_CHEBCK       right cheek back

#define FA_LT_CREASE     27        // L_CREASE       left crease
#define FA_RT_CREASE     28        // R_CREASE       right crease

#define FA_LT_SNER       29      // L_SNER       left sner
#define FA_RT_SNER       30      // R_SNER       right sner

#define FA_CT_LIP_TOP    31  // C_LIPUP             center_lip_top
#define FA_CT_LIP_BOTTOM 32  // c_LIPLO             center_lip_bottom

#define FA_LT_PUF        33    // L_PUF       left puf
#define FA_RT_PUF        34    // R_PUF       right puf

#define FA_CT_NOSTIP     35     // FA_CT_NOSTIP nose tip

#define FA_RT_EAR        36   // R_EAR		right ear
#define FA_LT_EAR        37   // L_EAR		left ear

#define FA_RT_BROUT      38     // R_BROUT		right eyebrow out
#define FA_LT_BROUT      39     // L_BROUT		left eyebrow out

#define FA_LT_FHDLO      40  // L_FHDLO      left		forehead low
#define FA_RT_FHDLO      41  // R_FHDLO      right	forehead low

#define FA_LT_FHDUP      42  // L_FHDUP      left		forehead up
#define FA_RT_FHDUP      43  // R_FHDUP      right	forehead up

#define FA_LT_TEND       44  // L_TEND      left		tend
#define FA_RT_TEND       45  // R_TEND      right	tend
#define FA_CT_ADAM       46  // C_ADAM      adam

#define FA_LT_LIDLOIN    47  // L_LIDLOIN      left		lid low in
#define FA_RT_LIDLOIN    48  // R_LIDLOIN      right	lid low in

#define FA_LT_TMP        49 // L_TMP      left		temple
#define FA_RT_TMP        50 // R_TMP      right	temple

#define FA_LT_NOSTRIL    51  // L_NOST              left_nostril
#define FA_RT_NOSTRIL    52  // R_NOST             right_nostril

// this is used as reference center.
// The middle part of the Nose (directly on the cartilage)
// is the steadiest point in the middle face.
// define markers before nose, because it will go thru the list to center them
// subtracting NOSE to fix the center
#define FA_NOSE 53            // NOSBRI

// stabilization markers (not existing in the joysticks)
#define FA_R_HDB       54
#define FA_L_HDB       55
#define FA_C_HDB       56

#define FA_NUM_MARKERS 57

//////////////////////////////////////////////////////////////////////////
struct tMarker
{
	char  sMarker[64];
	char  sJoystick[64];
	char  sJoystick3D[64];
	float fScaleX, fScaleY, fScaleZ;
	float fROMScaleX, fROMScaleY;
	int   nPos;   // in vicon channels vector
	bool  bExport;
	bool  bExport3D;
	bool  b3D;
	bool  bUseForPlaneFitting;
	int   nJoystickIndex;
	int   nJoystickIndex3D;

	void  Set(const char* szMarker, const char* szJoystick, const char* szJoystick3D = NULL)
	{
		cry_strcpy(sMarker, szMarker);
		cry_strcpy(sJoystick, szJoystick);
		if (szJoystick3D)
		{
			cry_strcpy(sJoystick3D, szJoystick3D);
			b3D = true;
		}
		else
		{
			sJoystick3D[0] = 0;
			b3D = false;
		}
		nJoystickIndex = -1;
		nJoystickIndex3D = -1;
	}

	float fDefaultDistX, fDefaultDistY, fDefaultDistZ;
	float fMinX, fMaxX;
	float fMinY, fMaxY;
	Vec3  vDefaultPos;
	Vec3  vCurrPos;
};

//////////////////////////////////////////////////////////////////////////
void CFacialEditorDialog::LoadC3DFile(const char* filename)
{

	FILE* infile = fopen(filename, "rb");
	if (!infile)
		return;

	unsigned short max_gap;
	unsigned short nDummy, nMarkers, nFrameStart, nFrameEnd;

	gEnv->pLog->Log("Loading c3d file %s \n", filename);

	//////////////////////////////////////////////////////////////////////////
	// Read in Header
	// Key1, byte = 1,2; word = 1
	char cParaBlock, key1;
	fread(&cParaBlock, sizeof(cParaBlock), 1, infile);
	fread(&key1, sizeof(key1), 1, infile);
	gEnv->pLog->Log("parablock=%d, key1 = %d\n", cParaBlock, key1);
	if (key1 != 0x50)
	{
		CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Error - The file is not a c3d file"));
		fclose(infile);
		return;
	}

	// Number of 3D points per field, byte = 3,4; word = 2
	fread(&nMarkers, sizeof(nMarkers), 1, infile);
	gEnv->pLog->Log("num_markers = %d\n", nMarkers);

	// Number of analog channels per field byte = 5,6; word = 3
	fread(&nDummy, sizeof(nDummy), 1, infile);

	// Field number of first field of video data, byte = 7,8; word = 4
	// NOTE: 1 based
	fread(&nFrameStart, sizeof(nFrameStart), 1, infile);
	gEnv->pLog->Log("framestart = %d\n", nFrameStart);

	// Field number of last field of video data, byte = 9,10; word = 5
	fread(&nFrameEnd, sizeof(nFrameEnd), 1, infile);
	gEnv->pLog->Log("frameend= %d\n", nFrameEnd);

	int nFrames = nFrameEnd - nFrameStart;

	float fFreq, fDummy;

	// Maximum interpolation gap in fields, byte = 11,12; word = 6
	fread(&max_gap, sizeof(max_gap), 1, infile);

	// Scaling Factor, bytes = 13,14,15,16; word = 7,8
	fread(&fDummy, sizeof(fDummy), 1, infile);
	gEnv->pLog->Log("Scale = %f\n", fDummy);

	// Starting record number, byte = 17,18; word = 9
	unsigned short start_record_num;
	fread(&start_record_num, sizeof(start_record_num), 1, infile);
	gEnv->pLog->Log("start_record_num= %d\n", start_record_num);

	// Number of analog frames per video field, byte = 19,20; word = 10
	fread(&nDummy, sizeof(nDummy), 1, infile);

	// Video rate in Hz, bytes = 21,22; word = 11
	fread(&fFreq, sizeof(fFreq), 1, infile);
	gEnv->pLog->Log("freq= %f\n", fFreq);

	//////////////////////////////////////////////////////////////////////////
	// Read in parameters

	// Start at Byte 512 of infile
	// The first byte is a pointer to the first 512-byte block that starts the
	// parameter section – (the header record is block 1).
	fseek(infile, (cParaBlock - 1) * 512, SEEK_SET);

	// First four bytes specified

	// bytes 1 and 2 are part of ID key (1 and 80)
	fread(&nDummy, sizeof(nDummy), 1, infile);
	// byte 3 holds # of parameter records to follow
	// but is not set correctly by 370. Ignore.
	// byte 4 is processor type, Vicon uses DEC (type 2)
	char cDummy;
	//fread(&nDummy, sizeof(nDummy), 1, infile);
	fread(&cDummy, sizeof(cDummy), 1, infile);
	int nBlocks = cDummy;
	fread(&cDummy, sizeof(cDummy), 1, infile);

	// Because it is unknown how many groups and how many
	// paremeters/group are stored, must scan for each variable
	// of interest.
	// Note: Only data of interest passed back from this routine
	// is read. Program can be modified to read other data values.

	// 1st scan for group POINT
	// Parameters stored in POINT are:
	//		1. LABELS
	//		2. DESCRIPTIONS
	//		3. USED
	//		4. UNITS
	//		5. SCALE
	//		6. RATE
	//		7. DATA_START
	//		8. FAMES
	//		9. INITIAL_COMMAND
	//		10.X_SCREEN
	//		11.Y_SCREEN

	char gname[128], pname[128];
	gname[0] = 0;
	pname[0] = 0;

	gEnv->pLog->Log("searching for group POINT ... ");
	while (strncmp(gname, "POINT", 5) != 0)
	{
		fread(&cDummy, sizeof(cDummy), 1, infile);
		int nNameLen = abs(cDummy);
		// group ID number - not sequential etc. , ignore
		fread(&cDummy, sizeof(cDummy), 1, infile);

		fread(&gname, sizeof(char), nNameLen, infile);
		gname[nNameLen] = 0;
		gEnv->pLog->Log("current name=%s ", gname);

		short nOffset;
		fread(&nOffset, sizeof(nOffset), 1, infile);
		// group parameters
		//gEnv->pLog->Log("offset =%d ",nOffset);
		if (nOffset == 0)
			break;
		fseek(infile, nOffset - 2, SEEK_CUR);
	}

	gEnv->pLog->Log("searching for parameter LABELS ... ");
	while (1)
	{
		fread(&cDummy, sizeof(cDummy), 1, infile);
		int nNameLen = abs(cDummy);
		// group ID number - not sequential etc. , ignore
		fread(&cDummy, sizeof(cDummy), 1, infile);

		fread(&gname, sizeof(char), nNameLen, infile);
		gname[nNameLen] = 0;
		gEnv->pLog->Log("current name=%s ", gname);

		if (strncmp(gname, "LABELS", 5) == 0)
			break;

		short nOffset;
		fread(&nOffset, sizeof(nOffset), 1, infile);
		// group parameters
		//gEnv->pLog->Log("offset =%d ",nOffset);
		if (nOffset == 0)
			break;
		fseek(infile, nOffset - 2, SEEK_CUR);
	}

	unsigned char ncol, nrow;
	int offset;
	char type, dim;
	RPF(&offset, &type, &dim, infile);  // dim should be 2 for a 2D array of labels[np][4]
	// read in array dimensions: should be 4 x np
	fread(&ncol, sizeof(char), 1, infile);
	fread(&nrow, sizeof(char), 1, infile);
	gEnv->pLog->Log("\tfound parameter LABELS - (%d,%d)", ncol, nrow);

	tMarker m_lstMarkers[FA_NUM_MARKERS];

	m_lstMarkers[FA_LT_EYEBROW_LT].Set("L_BRMID", "left_eyebrow_left", "left_eyebrow_left_z");
	m_lstMarkers[FA_LT_EYEBROW_RT].Set("L_BRIN", "left_eyebrow_right", "left_eyebrow_right_z");
	m_lstMarkers[FA_RT_EYEBROW_LT].Set("R_BRIN", "right_eyebrow_left", "right_eyebrow_left_z");
	m_lstMarkers[FA_RT_EYEBROW_RT].Set("R_BRMID", "right_eyebrow_right", "right_eyebrow_right_z");
	m_lstMarkers[FA_LT_CHEEK].Set("L_CHEUP", "left_cheek", "left_cheek_z");
	m_lstMarkers[FA_RT_CHEEK].Set("R_CHEUP", "right_cheek", "right_cheek_z");
	m_lstMarkers[FA_MOUTH_LT].Set("L_LIPCR", "left_mouthcorner", "left_mouthcorner_z");
	m_lstMarkers[FA_MOUTH_RT].Set("R_LIPCR", "right_mouthcorner", "right_mouthcorner_z");
	m_lstMarkers[FA_LT_LIP_TOP].Set("L_LIPUP", "left_lip_top", "left_lip_top_z");
	m_lstMarkers[FA_RT_LIP_TOP].Set("R_LIPUP", "right_lip_top", "right_lip_top_z");
	m_lstMarkers[FA_LT_LIP_BOTTOM].Set("L_LIPLO", "left_lip_bottom", "left_lip_bottom_z");
	m_lstMarkers[FA_RT_LIP_BOTTOM].Set("R_LIPLO", "right_lip_bottom", "right_lip_bottom_z");
	m_lstMarkers[FA_LT_NOSE].Set("L_NOWI", "left_nose", "left_nose_z");
	m_lstMarkers[FA_RT_NOSE].Set("R_NOWI", "right_nose", "right_nose_z");
	m_lstMarkers[FA_LT_EYELID_TOP].Set("L_LIDUP", "left_eyelid_upper", "left_eyelid_upper_z");
	m_lstMarkers[FA_LT_EYELID_LOW].Set("L_LIDLOOUT", "left_eyelid_lower", "left_eyelid_lower_z");
	m_lstMarkers[FA_RT_EYELID_TOP].Set("R_LIDUP", "right_eyelid_upper", "right_eyelid_upper_z");
	m_lstMarkers[FA_RT_EYELID_LOW].Set("R_LIDLOOUT", "right_eyelid_lower", "right_eyelid_lower_z");

	m_lstMarkers[FA_CHIN].Set("C_CHIN", "chin", "chin_z");
	m_lstMarkers[FA_NOSE].Set("C_NOSBRI", "none");

	m_lstMarkers[FA_R_HDB].Set("R_HDB", "none");
	m_lstMarkers[FA_L_HDB].Set("L_HDB", "none");
	m_lstMarkers[FA_C_HDB].Set("C_HDB", "none");

	m_lstMarkers[FA_CT_LIP_TOP].Set("C_LIPUP", "center_lip_upper", "center_lip_upper_z");
	m_lstMarkers[FA_CT_LIP_BOTTOM].Set("C_LIPLO", "center_lip_lower", "center_lip_lower_z");

	m_lstMarkers[FA_CHIN_LT].Set("L_CHIN", "left_chin", "left_chin_z");
	m_lstMarkers[FA_CHIN_RT].Set("R_CHIN", "right_chin", "right_chin_z");

	m_lstMarkers[FA_LT_JAWMID].Set("L_JAWMID", "left_jawmid", "left_jawmid_z");
	m_lstMarkers[FA_RT_JAWMID].Set("R_JAWMID", "right_jawmid", "right_jawmid_z");
	m_lstMarkers[FA_LT_JAWOUT].Set("L_JAWOUT", "left_jawout", "left_jawout_z");
	m_lstMarkers[FA_RT_JAWOUT].Set("R_JAWOUT", "right_jawout", "right_jawout_z");

	m_lstMarkers[FA_LT_CHEBCK].Set("L_CHEBCK", "left_cheekback", "left_cheekback_z");
	m_lstMarkers[FA_RT_CHEBCK].Set("R_CHEBCK", "right_cheekback", "right_cheekback_z");

	m_lstMarkers[FA_LT_CREASE].Set("L_CREAS", "left_crease", "left_crease_z");
	m_lstMarkers[FA_RT_CREASE].Set("R_CREAS", "right_crease", "right_crease_z");

	m_lstMarkers[FA_LT_SNER].Set("L_SNER", "left_sner", "left_sner_z");
	m_lstMarkers[FA_RT_SNER].Set("R_SNER", "right_sner", "right_sner_z");

	m_lstMarkers[FA_LT_PUF].Set("L_PUF", "left_puf", "left_puf_z");
	m_lstMarkers[FA_RT_PUF].Set("R_PUF", "right_puf", "right_puf_z");

	m_lstMarkers[FA_CT_NOSTIP].Set("C_NOSTIP", "center_nose_tip", "center_nose_tip_z");

	m_lstMarkers[FA_RT_BROUT].Set("R_BROUT", "right_eyebrow_out", "right_eyebrow_out_z");
	m_lstMarkers[FA_LT_BROUT].Set("L_BROUT", "left_eyebrow_out", "left_eyebrow_out_z");

	m_lstMarkers[FA_RT_EAR].Set("R_EAR", "right_ear", "right_ear_z");
	m_lstMarkers[FA_LT_EAR].Set("L_EAR", "left_ear", "left_ear_z");

	m_lstMarkers[FA_LT_FHDLO].Set("L_FHDLO", "left_forehead_low", "left_forehead_low_z");
	m_lstMarkers[FA_RT_FHDLO].Set("R_FHDLO", "right_forehead_low", "right_forehead_low_z");

	m_lstMarkers[FA_LT_FHDUP].Set("L_FHDUP", "left_forehead_up", "left_forehead_up_z");
	m_lstMarkers[FA_RT_FHDUP].Set("R_FHDUP", "right_forehead_up", "right_forehead_up_z");

	m_lstMarkers[FA_LT_TEND].Set("L_TEND", "left_tend", "left_tend_z");
	m_lstMarkers[FA_RT_TEND].Set("R_TEND", "right_tend", "right_tend_z");
	m_lstMarkers[FA_CT_ADAM].Set("C_ADAM", "center_adam", "center_adam_z");

	m_lstMarkers[FA_LT_LIDLOIN].Set("L_LIDLOIN", "left_lid_low_in", "left_lid_low_in_z");
	m_lstMarkers[FA_RT_LIDLOIN].Set("R_LIDLOIN", "right_lid_low_in", "right_lid_low_in_z");

	m_lstMarkers[FA_LT_TMP].Set("L_TMP", "left_temple", "left_temple_z");
	m_lstMarkers[FA_RT_TMP].Set("R_TMP", "right_temple", "right_temple_z");

	m_lstMarkers[FA_LT_NOSTRIL].Set("L_NOST", "left_nostril", "left_nostril_z");
	m_lstMarkers[FA_RT_NOSTRIL].Set("R_NOST", "right_nostril", "right_nostril_z");

	IJoystick* m_pJoystickHead, * m_pJoystickLean;

	IJoystickSet* pJoySet = m_pContext->GetJoystickSet();
	int nCount = pJoySet->GetJoystickCount();
	for (int k1 = 0; k1 < nCount; k1++)
	{
		IJoystick* pJoy = pJoySet->GetJoystick(k1);
		if (!pJoy)
			continue;
		const char* szJoy = pJoy->GetName();
		for (int k = 0; k < FA_NUM_MARKERS; k++)
		{
			if (m_lstMarkers[k].b3D)
			{
				if (stricmp(m_lstMarkers[k].sJoystick3D, szJoy) == 0)
				{
					m_lstMarkers[k].nJoystickIndex3D = k1;
				}
			}

			if (stricmp(m_lstMarkers[k].sJoystick, szJoy) == 0)
			{
				m_lstMarkers[k].nJoystickIndex = k1;
				break;
			}
		} //k
		if (stricmp(szJoy, "Head") == 0)
		{
			m_pJoystickHead = pJoy;
		}
		else if (stricmp(szJoy, "Lean") == 0)
		{
			m_pJoystickLean = pJoy;
		}
	} //k1

	for (int k = 0; k < FA_NUM_MARKERS; k++)
	{
		m_lstMarkers[k].fScaleX = 1.0f;
		m_lstMarkers[k].fScaleY = 1.0f;
		m_lstMarkers[k].nPos = -1;
		if (m_lstMarkers[k].nJoystickIndex < 0)
			m_lstMarkers[k].bExport = false;
		else
			m_lstMarkers[k].bExport = true;

		if (m_lstMarkers[k].nJoystickIndex3D < 0)
			m_lstMarkers[k].bExport3D = false;
		else
			m_lstMarkers[k].bExport3D = true;

		m_lstMarkers[k].fMinX = 1.0f;
		m_lstMarkers[k].fMaxX = -1.0f;
		m_lstMarkers[k].fMinY = 1.0f;
		m_lstMarkers[k].fMaxY = -1.0f;
		m_lstMarkers[k].bUseForPlaneFitting = true;
	} //k

	m_lstMarkers[FA_LT_TEND].bUseForPlaneFitting = false;
	m_lstMarkers[FA_RT_TEND].bUseForPlaneFitting = false;
	m_lstMarkers[FA_CT_ADAM].bUseForPlaneFitting = false;

	m_lstMarkers[FA_RT_EAR].bUseForPlaneFitting = false;
	m_lstMarkers[FA_RT_EAR].bUseForPlaneFitting = false;
	m_lstMarkers[FA_LT_EAR].bUseForPlaneFitting = false;
	m_lstMarkers[FA_LT_EAR].bUseForPlaneFitting = false;

	m_lstMarkers[FA_LT_TMP].bUseForPlaneFitting = false;
	m_lstMarkers[FA_LT_TMP].bUseForPlaneFitting = false;
	m_lstMarkers[FA_RT_TMP].bUseForPlaneFitting = false;
	m_lstMarkers[FA_RT_TMP].bUseForPlaneFitting = false;

	const float fDefaultScale = 1.0f;
	for (int k = 0; k < FA_NUM_MARKERS; k++)
	{
		//m_lstMarkers[k].fScaleZ=(m_lstMarkers[k].fScaleX+m_lstMarkers[k].fScaleY)/2.0f;
		m_lstMarkers[k].fScaleX = fDefaultScale;
		m_lstMarkers[k].fScaleY = fDefaultScale;
		m_lstMarkers[k].fScaleZ = fDefaultScale;
	}

	std::vector<string> lstNames;
	lstNames.reserve(nMarkers);

	for (int i = 0; i < nrow; i++)
	{
		fread(&gname, sizeof(char), ncol, infile);
		gname[ncol] = 0;

		gEnv->pLog->Log("Label %d=%s ", i, gname);

		// gname has some other characters following the name
		int nLen = 0;
		for (int k = 0; k < ncol; k++)
		{
			if (gname[k] == 32)
			{
				gname[k] = 0;
				break;
			}
		} //k

		lstNames.push_back(string(gname));

		for (int k = 0; k < FA_NUM_MARKERS; k++)
		{
			if (stricmp(m_lstMarkers[k].sMarker, gname) == 0)
			{
				gEnv->pLog->Log("Match %s=%s ", m_lstMarkers[k].sMarker, gname);
				m_lstMarkers[k].nPos = i;
				break;
			}
		} //k
	}

	//////////////////////////////////////////////////////////////////////////
	// Read in markers

	// Data is stored in the following format
	// Each frame (or field) from first_field to last_field
	//		Each marker from 1 to num_markers
	//			Data: X,Y,Z,R,C (Total of 8 bytes)
	//		Next marker
	//		Each analog sample per frame from 1 to analog_frames_per_field
	//			Each analog channel from 1 to num_analog_channels
	//				Data: A (total of 2 bytes)
	//			Next channel
	//		Next sample
	// Next frame

	// Determine interval to next frame of this markers data
	int frame_length = (8 * 2 * nMarkers);

	// read the first frame
	for (int k = 0; k < FA_NUM_MARKERS; k++)
	{
		if (m_lstMarkers[k].nPos < 0)
			continue;

		// Determine data offset based upon starting channel number
		int offset = 8 * 2 * (m_lstMarkers[k].nPos);

		// Position cursor to first data point
		fseek(infile, (start_record_num - 1) * 512 + offset, SEEK_SET);

		//for (int j=0;j<nFrames;j++)
		{
			Vec3 vPos;
			// Read XYZ data for this frame
			fread(&vPos.x, sizeof(float), 1, infile);
			fread(&vPos.y, sizeof(float), 1, infile);
			fread(&vPos.z, sizeof(float), 1, infile);

			// Read Residual & # cameras
			fread(&fDummy, sizeof(float), 1, infile);

			// Skip to the next frame
			//fseek(infile,frame_length,SEEK_CUR);

			Vec3 vTPos(vPos.x, vPos.z, vPos.y);
			vTPos /= 1000.0f;

			m_lstMarkers[k].vDefaultPos = vTPos;

			//gEnv->pLog->Log("Marker %d(%s)=%f,%f,%f ",k,m_lstMarkers[k].sMarker,vTPos.x,vTPos.y,vTPos.z);
		} //j
	}   //k

	//////////////////////////////////////////////////////////////////////////
	// HDBR, HDBL
	// find main orientation using markers on the head

	Vec3 v0 = m_lstMarkers[FA_R_HDB].vDefaultPos;
	Vec3 v1 = m_lstMarkers[FA_L_HDB].vDefaultPos;
	Vec3 vC = (v0 + v1) / 2.0f;
	Vec3 v2 = m_lstMarkers[FA_C_HDB].vDefaultPos;
	Vec3 vec0 = v0 - vC;
	Vec3 vec1 = v2 - vC;
	Vec3 vec2 = vec0 % vec1;
	Matrix33 m_DefRot;
	m_DefRot.SetFromVectors(vec0, vec1, vec2);
	m_DefRot.OrthonormalizeFast();
	Ang3 m_DefAng = RAD2DEG(Ang3::GetAnglesXYZ(m_DefRot));
	gEnv->pLog->Log("Rot=%f,%f,%f", m_DefAng.x, m_DefAng.y, m_DefAng.z);

	m_DefRot.Transpose();

	/*
	   //////////////////////////////////////////////////////////////////////////
	   if (false)
	   {
	   FILE *fp=fopen("Markers.txt","wt");
	   std::vector<string> lstNamesSkip;
	   lstNamesSkip.push_back("R_EAR");
	   lstNamesSkip.push_back("L_EAR");
	   lstNamesSkip.push_back("R_HDB");
	   lstNamesSkip.push_back("R_HDBMID");
	   lstNamesSkip.push_back("C_HDB");
	   lstNamesSkip.push_back("L_HDBMID");
	   lstNamesSkip.push_back("L_HDB");
	   lstNamesSkip.push_back("R_TEND");
	   lstNamesSkip.push_back("L_TEND");
	   lstNamesSkip.push_back("C_ADAM");

	   const int nFramesExport=2;
	   float lstFramesExport[nFramesExport]={0,4.0f};
	   for (int j2=0;j2<nFramesExport;j2++)
	   {
	    std::vector<Vec3> pts;

	    for (int k=0;k<nMarkers;k++)
	    {
	      bool bSkip=false;
	      for (int j=0;j<lstNamesSkip.size();j++)
	      {
	        if (stricmp(lstNames[k].c_str(),lstNamesSkip[j].c_str())==0)
	        {
	          bSkip=true;
	          break;
	        }
	      } //j
	      if (bSkip)
	        continue;

	      // Determine data offset based upon starting channel number
	      int nFrame=(int)((lstFramesExport[j2]*fFreq)+0.5f);
	      int offset = 8*2*(k)+(nFrame*8*2*nMarkers);

	      // Position cursor to first data point
	      fseek(infile,(start_record_num-1)*512+offset,SEEK_SET);

	      //for (int j=0;j<nFrames;j++)
	      {
	        Vec3 vPos;
	        // Read XYZ data for this frame
	        fread(&vPos.x, sizeof(float), 1, infile);
	        fread(&vPos.y, sizeof(float), 1, infile);
	        fread(&vPos.z, sizeof(float), 1, infile);

	        // Read Residual & # cameras
	        fread(&fDummy, sizeof(float), 1, infile);

	        // Skip to the next frame
	        //fseek(infile,frame_length,SEEK_CUR);

	        //gEnv->pLog->Log("Marker %d(%s)=%f,%f,%f ",k,m_lstMarkers[k].sMarker,vPos.x,vPos.y,vPos.z);

	        Vec3 vTPos(vPos.x,vPos.z,vPos.y);
	        vTPos/=1000.0f;

	        pts.push_back(vTPos);
	      }
	    } //k

	    fprintf(fp,"%f \n",lstFramesExport[j2]);
	    fprintf(fp,"%d \n",(int)(pts.size()));
	    int nPts=pts.size();
	    for (int k=0;k<nPts;k++)
	    {
	      Vec3 vPos = pts[k];
	      fprintf(fp,"%f %f %f \n",vPos.x,vPos.y,vPos.z);
	    }
	    // next one is center again
	    Vec3 vPos = m_lstMarkers[FA_NOSE].vDefaultPos;
	    fprintf(fp,"%f %f %f \n",vPos.x,vPos.y,vPos.z);

	   } //j
	   fclose(fp);
	   }
	 */
	//////////////////////////////////////////////////////////////////////////
	// rbrin, lbrin

	// set nosbri as center and remove rotation
	for (int k = 0; k < FA_NUM_MARKERS; k++)
	{
		Vec3 vMarkerPos = m_lstMarkers[k].vDefaultPos;
		vMarkerPos -= m_lstMarkers[FA_NOSE].vDefaultPos;
		Vec3 vPos = vMarkerPos;
		vMarkerPos = m_DefRot.TransformVector(vPos);
		m_lstMarkers[k].vDefaultPos = vMarkerPos;
	} //k

	//////////////////////////////////////////////////////////////////////////
	// best fit plane looking at the face
	Plane tRes;

	Matrix33 C, tmp;
	Vec3 org, n;
	org.zero();
	C.SetZero();
	int nPt = 0;

	for (int k = 0; k < FA_NUM_MARKERS; k++)
	{
		if (!m_lstMarkers[k].bExport || !m_lstMarkers[k].bUseForPlaneFitting)
			continue;
		Vec3 vPos = m_lstMarkers[k].vDefaultPos;
		C += dotproduct_matrix(vPos, vPos, tmp);
		org += vPos;
		nPt++;
	}
	//nPt=FA_NUM_MARKERS;

	org /= nPt;
	C -= dotproduct_matrix(org, org, tmp) * (float)nPt;
	float det, maxdet;
	int i, j;

	for (i = 0, j = -1, maxdet = 0; i < 3; i++)
	{
		det = C(inc_mod3[i], inc_mod3[i]) * C(dec_mod3[i], dec_mod3[i]) - C(dec_mod3[i], inc_mod3[i]) * C(inc_mod3[i], dec_mod3[i]);
		if (fabs(det) > fabs(maxdet))
			maxdet = det, j = i;
	}
	if (j >= 0)
	{
		det = 1.0 / maxdet;
		n[j] = 1;
		n[inc_mod3[j]] = -(C(inc_mod3[j], j) * C(dec_mod3[j], dec_mod3[j]) - C(dec_mod3[j], j) * C(inc_mod3[j], dec_mod3[j])) * det;
		n[dec_mod3[j]] = -(C(inc_mod3[j], inc_mod3[j]) * C(dec_mod3[j], j) - C(dec_mod3[j], inc_mod3[j]) * C(inc_mod3[j], j)) * det;
		n = n.normalized();

		// nosbri is center now
		float fD = -(n * m_lstMarkers[FA_NOSE].vDefaultPos); //-(n*GetTransformedMarkerPos("NOSBRI"));
		tRes.Set(n, fD);
		//pbdst[nBuoys].waterPlane.origin = m_R*org*m_scale+m_offset;
		//pbdst[nBuoys].waterPlane.n = n*sgnnz(n*pbdst[nBuoys].waterPlane.n);
	}

	Plane m_ProjPlane = tRes;
	//////////////////////////////////////////////////////////////////////////

	Plane m_ProjPlaneX, m_ProjPlaneY, m_ProjPlaneZ;

	// | vertical plane (X left/right movements)
	v0 = m_lstMarkers[FA_NOSE].vDefaultPos;
	v1 = v0 + m_ProjPlane.n;
	v2 = m_lstMarkers[FA_CHIN].vDefaultPos;
	m_ProjPlaneY.SetPlane(v0, v1, v2);

	// - horizontal plane (Y up/down movements)
	vec0 = v1 - v0;
	vec1 = v2 - v0;
	vec2 = vec0 % vec1;
	v2 = vec2.GetNormalized();
	m_ProjPlaneX.SetPlane(v0, v1, v2);

	// - depth plane (Z in/out movements)
	m_ProjPlaneZ = m_ProjPlane;

	// find default distances
	for (int k = 0; k < FA_NUM_MARKERS; k++)
	{
		Vec3 vMarkerPos = m_lstMarkers[k].vDefaultPos;
		m_lstMarkers[k].fDefaultDistX = m_ProjPlaneY.DistFromPlane(vMarkerPos);
		m_lstMarkers[k].fDefaultDistY = m_ProjPlaneX.DistFromPlane(vMarkerPos);
		m_lstMarkers[k].fDefaultDistZ = m_ProjPlaneZ.DistFromPlane(vMarkerPos);
		//gEnv->pLog->Log("Distances: Marker (%f,%f,%f),%d(%s)=%f,%f ",vMarkerPos.x,vMarkerPos.y,vMarkerPos.z,k,m_lstMarkers[k].sMarker,m_lstMarkers[k].fDefaultDistX,m_lstMarkers[k].fDefaultDistY);
	} //k

	//////////////////////////////////////////////////////////////////////////
	// process all frames

	float fInvFPS = 1.0f / fFreq;
	float fTotalTime = (float)(nFrames) * fInvFPS;

	float fStartTime = m_pContext->GetSequenceTime();

	m_pContext->GetSequence()->SetTimeRange(Range(0, fStartTime + fTotalTime));
	m_pContext->SendEvent(EFD_EVENT_SEQUENCE_CHANGE);

	for (int j = 0; j < nFrames; j++)
	{
		float fCurrentTime = (float)(j) * fInvFPS;

		for (int k = 0; k < FA_NUM_MARKERS; k++)
		{
			if (m_lstMarkers[k].nPos < 0)
				continue;

			// Determine data offset based upon starting channel number
			int offset = 8 * 2 * (m_lstMarkers[k].nPos) + (j * 8 * 2 * nMarkers);

			// Position cursor to first data point
			fseek(infile, (start_record_num - 1) * 512 + offset, SEEK_SET);

			Vec3 vPos;
			// Read XYZ data for this frame
			fread(&vPos.x, sizeof(float), 1, infile);
			fread(&vPos.y, sizeof(float), 1, infile);
			fread(&vPos.z, sizeof(float), 1, infile);

			// Read Residual & # cameras
			fread(&fDummy, sizeof(float), 1, infile);

			// Skip to the next frame
			//fseek(infile,frame_length,SEEK_CUR);

			//gEnv->pLog->Log("Marker %d(%s)=%f,%f,%f ",k,m_lstMarkers[k].sMarker,vPos.x,vPos.y,vPos.z);

			Vec3 vTPos(vPos.x, vPos.z, vPos.y);
			vTPos /= 1000.0f;

			m_lstMarkers[k].vCurrPos = vTPos;
		} //k

		//////////////////////////////////////////////////////////////////////////
		// find inverse movement and solve
		// HDBR, HDBL
		v0 = m_lstMarkers[FA_R_HDB].vCurrPos;
		v1 = m_lstMarkers[FA_L_HDB].vCurrPos;
		vC = (v0 + v1) / 2.0f;
		v2 = m_lstMarkers[FA_C_HDB].vCurrPos;

		Matrix33 rMat;
		vec0 = v0 - vC;
		vec1 = v2 - vC;
		vec2 = vec0 % vec1;
		rMat.SetFromVectors(vec0, vec1, vec2);
		rMat.OrthonormalizeFast();
		Ang3 vNewAng = RAD2DEG(Ang3::GetAnglesXYZ(rMat));

		rMat.Transpose();

		Vec3 vCenter = m_lstMarkers[FA_NOSE].vCurrPos;

		IJoystickSet* pJoysticks = m_pContext->GetJoystickSet();

		for (int k = 0; k < FA_NUM_MARKERS; k++)
		{
			if (!m_lstMarkers[k].bExport)
				continue;

			Vec3 vMarkerPos = m_lstMarkers[k].vCurrPos;

			vMarkerPos -= vCenter;
			Vec3 vPos = vMarkerPos;
			Vec3 vMarkerPos2 = rMat.TransformVector(vPos);

			vMarkerPos = m_lstMarkers[k].vDefaultPos;

			float fDirs[2];

			fDirs[0] = m_ProjPlaneY.DistFromPlane(vMarkerPos2) - m_lstMarkers[k].fDefaultDistX;
			fDirs[1] = m_ProjPlaneX.DistFromPlane(vMarkerPos2) - m_lstMarkers[k].fDefaultDistY;

			if (fDirs[0] < m_lstMarkers[k].fMinX)
				m_lstMarkers[k].fMinX = (m_lstMarkers[k].fMinX + fDirs[0]) / 2.0f;
			if (fDirs[0] > m_lstMarkers[k].fMaxX)
				m_lstMarkers[k].fMaxX = (m_lstMarkers[k].fMaxX + fDirs[0]) / 2.0f;
			if (fDirs[1] < m_lstMarkers[k].fMinY)
				m_lstMarkers[k].fMinY = (m_lstMarkers[k].fMinY + fDirs[1]) / 2.0f;
			if (fDirs[1] > m_lstMarkers[k].fMaxY)
				m_lstMarkers[k].fMaxY = (m_lstMarkers[k].fMaxY + fDirs[1]) / 2.0f;

			m_lstMarkers[k].fROMScaleX = m_lstMarkers[k].fMaxX - m_lstMarkers[k].fMinX;
			if (m_lstMarkers[k].fROMScaleX != 0)
				m_lstMarkers[k].fROMScaleX = 1.0f / m_lstMarkers[k].fROMScaleX;
			m_lstMarkers[k].fROMScaleY = m_lstMarkers[k].fMaxY - m_lstMarkers[k].fMinY;
			if (m_lstMarkers[k].fROMScaleY != 0)
				m_lstMarkers[k].fROMScaleY = 1.0f / m_lstMarkers[k].fROMScaleY;

			fDirs[0] *= m_lstMarkers[k].fScaleX * m_pContext->m_fC3DScale;
			fDirs[1] *= m_lstMarkers[k].fScaleY * m_pContext->m_fC3DScale;

			IJoystick* pJoystick = (pJoysticks->GetJoystick(m_lstMarkers[k].nJoystickIndex));
			if (!pJoystick)
				continue;

			for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); axis < 2; axis = IJoystick::ChannelType(axis + 1))
			{
				IJoystickChannel* pChannel = pJoystick->GetChannel(axis);

				if (!pChannel)
					continue;

				//JoystickUtils::RemoveKeysInRange(pChannel, startTime + time, startTime + time + fVideoFPS);
				float value = (pChannel && pChannel->GetFlipped() ? -fDirs[axis] : fDirs[axis]);

				value *= pChannel->GetVideoMarkerScale();

				//if (axis==0 && (stricmp(pJoystick->GetName(),"center_lip_upper")==0) || (stricmp(pJoystick->GetName(),"left_lip_top")==0))
				//	gEnv->pLog->Log("Marker %d(%s)=%f,%f",axis,pJoystick->GetName(),fDirs[0],fDirs[1]);

				//JoystickUtils::SetKey(pChannel, startTime + time, value);

				//JoystickUtils::SetKey(pChannel, time, value);
				JoystickUtils::SetKey(pChannel, fStartTime + fCurrentTime, value);
			} //axis

			if (!m_lstMarkers[k].bExport3D)
				continue;

			pJoystick = (pJoysticks->GetJoystick(m_lstMarkers[k].nJoystickIndex3D));
			if (!pJoystick)
				continue;

			fDirs[0] = m_ProjPlaneZ.DistFromPlane(vMarkerPos2) - m_lstMarkers[k].fDefaultDistZ;
			fDirs[0] *= m_lstMarkers[k].fScaleZ * m_pContext->m_fC3DScale;

			fDirs[1] = fDirs[0];

			for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); axis < 2; axis = IJoystick::ChannelType(axis + 1))
			{
				IJoystickChannel* pChannel = pJoystick->GetChannel(axis);
				if (!pChannel)
					continue;
				float value = (pChannel && pChannel->GetFlipped() ? -fDirs[axis] : fDirs[axis]);
				value *= pChannel->GetVideoMarkerScale();
				JoystickUtils::SetKey(pChannel, fStartTime + fCurrentTime, value);
			} //axis

		} //k

		// head movements
		if (!m_pJoystickHead || !m_pJoystickLean)
			continue;

		float fDirs[2];

		Vec3 vAngles(vNewAng.x - m_DefAng.x, vNewAng.y - m_DefAng.y, vNewAng.z - m_DefAng.z);
		//gEnv->pLog->Log("Angles=%f,%f,%f(%f,%f,%f)-(%f,%f,%f)",vAngles.x,vAngles.y,vAngles.z,vNewAng.x,vNewAng.y,vNewAng.z,m_DefAng.x,m_DefAng.y,m_DefAng.z);

		IJoystick* pJoystick = m_pJoystickHead;
		fDirs[0] = sin(DEG2RAD(vAngles.y)) * m_pContext->m_fC3DScale;
		fDirs[1] = -sin(DEG2RAD(vAngles.x)) * m_pContext->m_fC3DScale;
		//fDirs[0]=vAngles.y*0.015f;
		//fDirs[1]=-vAngles.x*0.015f;
		for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); axis < 2; axis = IJoystick::ChannelType(axis + 1))
		{
			IJoystickChannel* pChannel = pJoystick->GetChannel(axis);
			float value = (pChannel && pChannel->GetFlipped() ? -fDirs[axis] : fDirs[axis]);
			if (pChannel)
				value *= pChannel->GetVideoMarkerScale();
			//JoystickUtils::SetKey(pChannel, time, value);
			JoystickUtils::SetKey(pChannel, fStartTime + fCurrentTime, value);
		} //axis

		pJoystick = m_pJoystickLean;
		//fDirs[0]=-vAngles.z*0.015f;
		fDirs[0] = -sin(DEG2RAD(vAngles.z)) * m_pContext->m_fC3DScale;
		fDirs[1] = 0;
		for (IJoystick::ChannelType axis = IJoystick::ChannelType(0); axis < 2; axis = IJoystick::ChannelType(axis + 1))
		{
			IJoystickChannel* pChannel = pJoystick->GetChannel(axis);
			float value = (pChannel && pChannel->GetFlipped() ? -fDirs[axis] : fDirs[axis]);
			if (pChannel)
				value *= pChannel->GetVideoMarkerScale();
			//JoystickUtils::SetKey(pChannel, time, value);
			JoystickUtils::SetKey(pChannel, fStartTime + fCurrentTime, value);
		} //axis

	} //j

	fclose(infile);

	m_pContext->bSequenceModfied = true;
	m_pContext->SendEvent(EFD_EVENT_SPLINE_CHANGE);
}


// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "rigidbody.h"

phys_job_info& GetJobProfileInst(int);

RigidBody::RigidBody()
{
	P.zero(); L.zero(); 
	v.zero(); w.zero(); 
	q.SetIdentity(); 
	pos.zero();
	Ibody.zero();
	memset(bProcessed,0,sizeof(bProcessed)); 
	M=Minv=V = 0;
	Iinv.SetZero(); 
	Ibody.zero(); 
	Ibody_inv.zero(); 
	qfb.SetIdentity(); 
	offsfb.zero();
	Fcollision.zero(); Tcollision.zero();
	Eunproj = 0;
	flags = 0;
}

void RigidBody::Create(const Vec3 &center,const Vec3 &Ibody0,const quaternionf &q0, float volume,float mass, 
											 const quaternionf &qframe,const Vec3 &posframe)
{
	float density=mass/volume;

	V = volume;M = mass; q = q0; pos = center;
	if (M>0) {
		(Ibody_inv = Ibody = Ibody0*density).invert();
		Minv = 1.0f/M;
	} else {
		Ibody_inv = Ibody.zero();
		Minv = 0;
	}
	qfb = !q*qframe;
	offsfb = (pos-posframe)*qframe;

	UpdateState();
}

void RigidBody::Add(const Vec3 &center,const Vec3 Ibodyop,const quaternionf &qop, float volume,float mass)
{
	if (mass==0.0f) return;
	if (fabs_tpl(M+mass)<M*0.0001f) {
		zero(); return;
	}
	float density=mass/volume; int i;
	Matrix33 Rop,Ibodyop_mtx,Iop,Ibody_mtx;
	Ibodyop_mtx.SetIdentity(); for(i=0;i<3;i++) Ibodyop_mtx(i,i)=Ibodyop[i]*density;

	Rop = Matrix33(!q*qop);
	Iop = Rop*Ibodyop_mtx*Rop.T();

	Vec3 posnew = (pos*M+center*mass)/(M+mass); 
	Ibody_mtx = Ibody;
	OffsetInertiaTensor(Ibody_mtx,(posnew-pos)*q,M);
	OffsetInertiaTensor(Iop,(posnew-center)*q,mass);
	M+=mass; V+=volume;	Ibody_mtx+=Iop; Minv=1.0f/M;

	quaternionf qframe = q*qfb;
	offsfb += (posnew-pos)*qframe;

	float Ibody_buf[9],Rbody2newbody[9];
	SetMtxStrided<float,3,1>(Ibody_mtx, Ibody_buf);
	matrixf eigenBasis(3,3,0,Rbody2newbody);
	matrixf(3,3,mtx_symmetric,Ibody_buf).jacobi_transformation(eigenBasis,&Ibody.x);
	(Ibody_inv = Ibody).invert();
	quaternionf qb2nb = (quaternionf)GetMtxStrided<float,3,1>(Rbody2newbody);
	q *= !qb2nb; qfb = qb2nb*qfb;	pos = posnew;

	UpdateState();
}

void RigidBody::zero()
{
	M = Minv = 0;
	Ibody.zero(); Ibody_inv.zero();
	P.zero(); L.zero(); v.zero(); w.zero();
}

void RigidBody::UpdateState()
{
	Matrix33 R = Matrix33(q);
	Iinv = R*Ibody_inv*R.T();
	if (Minv>0) {
		v = P*Minv;
		w = Iinv*L;
	}
}

inline Quat w_dt(const Vec3 &w, float dt) {
	float wlen = w.len();
	if (wlen < 0.001f) 
		return Quat(cos_tpl(wlen*dt*0.5f), w*(dt*0.5f));
	float sinha,cosha; sincos_tpl(wlen*dt*0.5f, &sinha,&cosha);
	return Quat(cosha, w*(sinha/wlen));
}

void RigidBody::Step(float dt)
{
	UpdateState();	

	pos += v*dt;

	float E0 = 0;
	Quat dq;
	if (!(flags & rb_RK4) && w.len2()*sqr(dt)<sqr(0.2f) || !(Ibody_inv.x+Ibody_inv.y+Ibody_inv.z))
		dq = w_dt(w,dt);
	else {
		E0 = L*w;
		Vec3 w1 = w;
		Quat q2 = w_dt(w1,dt*0.5f)*q; Vec3 w2 = q2*(Ibody_inv*(L*q2));
		Quat q3 = w_dt(w2,dt*0.5f)*q; Vec3 w3 = q3*(Ibody_inv*(L*q3));
		Quat q4 = w_dt(w3,dt     )*q; Vec3 w4 = q4*(Ibody_inv*(L*q4));
		dq = w_dt((w1+w4+(w2+w3)*2)*(1.0f/6),dt);
	}
	(q=dq*q).Normalize();

	if (Minv>0) {
		Matrix33 R = Matrix33(q);
		Iinv = R*Ibody_inv*R.T();
		w = Iinv*L;
	}
	float E1 = L*w;
	if (E1*(flags & rb_RK4) > 0.001f)
		w *= E0/E1;
}

void RigidBody::GetContactMatrix(const Vec3 &r, Matrix33 &K)
{
	if (!(flags & rb_articulated)) {
		Matrix33 rx; crossproduct_matrix(r,rx);
		K -= rx*Iinv*rx;
		K(0,0)+=Minv; K(1,1)+=Minv; K(2,2)+=Minv;
	} else 
		((ArticulatedBody*)this)->GetContactMatrix(r,K);
}


////////////////////////////////////////////////////////////////////////
///////////////////// Global Multibody Solver //////////////////////////
////////////////////////////////////////////////////////////////////////

struct contact_helper {
	Vec3 r0,r1;
	Matrix33 K;
	Vec3 n;
	Vec3 vreq;
	float Pspare;
	float friction;
	int flags;
	int iBody[2];
	short iCount;
	short iCountDst;
	float Pn;
};
struct contact_sandwich {
	int iMiddle;
	int iBread[2];
	contact_sandwich *next;
	int bProcessed;
};
struct buddy_info {
	int iBody;
	Vec3 vreq;
	int flags;
	buddy_info *next;
};
struct follower_thunk {
	int iBody;
	follower_thunk *next;
};
struct body_info {
	buddy_info *pbuddy;
	contact_sandwich *psandwich;
	follower_thunk *pfollower;
	float Minv;
	int iLevel;
	int idUpdate;
	int idx;
	Vec3 v_unproj,w_unproj;
	Vec3 Fcollision,Tcollision;
};
struct contact_helper_CG {
	unsigned int idx : 31;
	unsigned int bUseC;
	body_helper *pbody[2];
	Vec3 ptloc[2];
	Vec3 r0,r;
	Vec3 dP,P;
	float dPn;
	Vec3 vrel;
	Vec3 n;
};
struct contact_helper_constraint {
	Matrix33 C;
	Matrix33 Kinv;
};

struct RBdata {
	int nContacts,nBodies;
	entity_contact *pContacts[MAX_CONTACTS];
	RigidBody *pBodies[MAX_CONTACTS];
	contact_helper ContactsRB[MAX_CONTACTS];
	contact_helper_CG ContactsCG[MAX_CONTACTS];
	contact_helper_constraint ContactsC[MAX_CONTACTS];
	body_helper Bodies[MAX_CONTACTS];
	body_info infos[MAX_CONTACTS];
	contact_sandwich sandwichbuf[MAX_CONTACTS];
	buddy_info buddybuf[MAX_CONTACTS];
	follower_thunk followersbuf[MAX_CONTACTS];
	int nFollowers,idLastUpdate,nUnprojLoops;
	char SolverBuf[262144];
	int iSolverBufPos;
	char *pSolverBufAux;
	int iSolverBufAuxPos;
	int sizeSolverBufAux;
	bool bUsePreCG;
	
	RBdata()
		: nContacts(0)
		, nBodies(0)
		, nFollowers(0)
		, idLastUpdate(0)
		, nUnprojLoops(0)
		, iSolverBufPos(0)
		, pSolverBufAux(nullptr)
		, iSolverBufAuxPos(0)
		, sizeSolverBufAux(0)
		, bUsePreCG(false)
	{
		ZeroArray(pContacts);
		ZeroArray(pBodies);
		ZeroArray(ContactsRB);
		ZeroArray(ContactsCG);
		ZeroArray(Bodies);
		ZeroArray(infos);
		ZeroArray(sandwichbuf);
		ZeroArray(buddybuf);
		ZeroArray(followersbuf);
		ZeroArray(SolverBuf);
	}
};
RBdata *g_RBdata[MAX_PHYS_THREADS-1+FIRST_WORKER_THREAD] = { 0 };

void GetRBMemStats(ICrySizer *pSizer)
{
	SIZER_COMPONENT_NAME(pSizer, "RB thread data");	
	for(int i=0;i<MAX_PHYS_THREADS;i++) if (g_RBdata[i])
		pSizer->AddObject(g_RBdata[i], sizeof(*g_RBdata[i]));
}

#define g_nContacts     g_RBdata[iCaller]->nContacts
#define g_nBodies       g_RBdata[iCaller]->nBodies
#define g_pContacts     g_RBdata[iCaller]->pContacts
#define g_pBodies       g_RBdata[iCaller]->pBodies
#define g_ContactsRB    g_RBdata[iCaller]->ContactsRB
#define g_ContactsCG    g_RBdata[iCaller]->ContactsCG
#define g_ContactsC     g_RBdata[iCaller]->ContactsC
#define g_Bodies        g_RBdata[iCaller]->Bodies
#define g_infos         g_RBdata[iCaller]->infos
#define g_sandwichbuf   g_RBdata[iCaller]->sandwichbuf
#define g_followersbuf  g_RBdata[iCaller]->followersbuf
#define g_buddybuf      g_RBdata[iCaller]->buddybuf
#define g_nFollowers    g_RBdata[iCaller]->nFollowers
#define g_idLastUpdate  g_RBdata[iCaller]->idLastUpdate
#define g_nUnprojLoops  g_RBdata[iCaller]->nUnprojLoops
#define g_SolverBuf     g_RBdata[iCaller]->SolverBuf
#define g_iSolverBufPos g_RBdata[iCaller]->iSolverBufPos
#define g_pSolverBufAux g_RBdata[iCaller]->pSolverBufAux
#define g_iSolverBufAuxPos g_RBdata[iCaller]->iSolverBufAuxPos
#define g_sizeSolverBufAux g_RBdata[iCaller]->sizeSolverBufAux
#define g_bUsePreCG     g_RBdata[iCaller]->bUsePreCG

/*int g_nContacts,g_nBodies;
entity_contact *g_pContacts[MAX_CONTACTS];
static RigidBody *g_pBodies[MAX_CONTACTS];
static contact_helper g_ContactsRB[MAX_CONTACTS];
static contact_helper_CG g_ContactsCG[MAX_CONTACTS];
static contact_helper_constraint g_ContactsC[MAX_CONTACTS];
static body_helper g_Bodies[MAX_CONTACTS];
static body_info g_infos[MAX_CONTACTS];
static contact_sandwich g_sandwichbuf[MAX_CONTACTS];
static buddy_info g_buddybuf[MAX_CONTACTS];
static follower_thunk g_followersbuf[MAX_CONTACTS];
static int g_nFollowers,g_idLastUpdate,g_nUnprojLoops;
static char g_SolverBuf[262144];
static int g_iSolverBufPos;
bool g_bUsePreCG = true;*/
int __solver_step=0;


inline bool should_swap(entity_contact **pContacts,int i1,int i2,int &diff, int iCaller) { // sub-sorts contacts by pbody[1]
	diff |= pContacts[i1]->pbody[1]->bProcessed-pContacts[i2]->pbody[1]->bProcessed;
	return pContacts[i1]->pbody[1]->bProcessed < pContacts[i2]->pbody[1]->bProcessed;
}
inline bool should_swap(RigidBody **pBodies, int i1,int i2,int &diff, int iCaller) { // sorts bodies by level
	diff |= g_infos[pBodies[i1]->bProcessed[iCaller]].iLevel-g_infos[pBodies[i2]->bProcessed[iCaller]].iLevel;
	return g_infos[pBodies[i1]->bProcessed[iCaller]].iLevel < g_infos[pBodies[i2]->bProcessed[iCaller]].iLevel;
}
struct entity_contact_unproj : entity_contact {};
bool should_swap(entity_contact_unproj **pContacts,int i1,int i2,int &diff, int iCaller) { // sorts contacts by newly sorted bodies 
	int iop1 = isneg(g_infos[pContacts[i1]->pbody[0]->bProcessed[iCaller]].iLevel-g_infos[pContacts[i1]->pbody[1]->bProcessed[iCaller]].iLevel);
	int iop2 = isneg(g_infos[pContacts[i2]->pbody[0]->bProcessed[iCaller]].iLevel-g_infos[pContacts[i2]->pbody[1]->bProcessed[iCaller]].iLevel);
	diff |= g_infos[pContacts[i1]->pbody[iop1]->bProcessed[iCaller]].idx-g_infos[pContacts[i2]->pbody[iop2]->bProcessed[iCaller]].idx;
	return g_infos[pContacts[i1]->pbody[iop1]->bProcessed[iCaller]].idx < g_infos[pContacts[i2]->pbody[iop2]->bProcessed[iCaller]].idx;
}

template<class dtype> static void qsort(dtype *pArray,int left,int right,int iCaller) {
	if (left>=right) return;
	int i,last,diff=0; 
	swap(pArray, left,left+right>>1);
	for(last=left,i=left+1; i<=right; i++)
		if (should_swap(pArray,i,left,diff,iCaller))
			swap(pArray, ++last, i);
	swap(pArray, left,last);

	if (diff) {
		qsort(pArray, left,last-1, iCaller);
		qsort(pArray, last+1,right, iCaller);
	}
}
#define bidx0(i) (g_pContacts[i]->pbody[0]->bProcessed[iCaller])
#define bidx1(i) (g_pContacts[i]->pbody[1]->bProcessed[iCaller])

void update_followers(int iBody,int idUpdate, int iCaller) {
	if (g_infos[iBody].idUpdate==idUpdate) {
		g_nUnprojLoops++; return;
	}
	g_infos[iBody].idUpdate = idUpdate;
	for(follower_thunk *pfollower=g_infos[iBody].pfollower; pfollower; pfollower=pfollower->next)	
	if (g_infos[pfollower->iBody].iLevel<=g_infos[iBody].iLevel) {
		g_infos[pfollower->iBody].iLevel = g_infos[iBody].iLevel+1;
		update_followers(pfollower->iBody,idUpdate, iCaller);
	}
}

// trace_unproj_route recursive func: have 2 bodies (1 inside, 1 outside), find all possible 2nd outsides and call recurecively for each of them
//   (maintain a list of "followers" for each such body); assign level to each processed body
void trace_unproj_route(int iMiddle,int iBread, int iCaller);

void add_route_follower(int iBody,int iFollower, int iCaller) {
	follower_thunk* pfollower;
	for(pfollower=g_infos[iBody].pfollower; pfollower && pfollower->iBody!=iFollower; pfollower=pfollower->next);
	if (!pfollower && g_nFollowers<MAX_CONTACTS) {
		g_followersbuf[g_nFollowers].iBody = iFollower;
		g_followersbuf[g_nFollowers].next = g_infos[iBody].pfollower;
		g_infos[iBody].pfollower = g_followersbuf+g_nFollowers++;
		trace_unproj_route(iFollower, iBody, iCaller);
	}
}

void update_level(int iBody, int iNewLevel, int iCaller) {
	if ((unsigned int)g_infos[iBody].iLevel>=(unsigned int)iNewLevel)
		g_infos[iBody].iLevel = max(g_infos[iBody].iLevel, iNewLevel); // -1 or >=new level
	else {
		g_infos[iBody].iLevel = iNewLevel;
		update_followers(iBody,++g_idLastUpdate, iCaller);
	}
}

void trace_unproj_route(int iMiddle,int iBread, int iCaller) {
	int iop;
	for(contact_sandwich *psandwich=g_infos[iMiddle].psandwich; psandwich; psandwich=psandwich->next)
	if (iszero(psandwich->iMiddle-iMiddle) & ((iop=iszero(psandwich->iBread[0]-iBread)) | iszero(psandwich->iBread[1]-iBread))) {
		psandwich->bProcessed = 1;
		update_level(psandwich->iBread[iop], g_infos[iMiddle].iLevel+1, iCaller);
		add_route_follower(iMiddle,psandwich->iBread[iop], iCaller);
	}
}

real ComputeRc(RigidBody *body0, entity_contact **pContacts, int nAngContacts,int nContacts, int iCaller) // for MINRES unprojection solver
{
	int i;
	real rT_x_rc=0;
	Vec3 dP,r0;

	body0->Fcollision.zero(); body0->Tcollision.zero();
	for(i=0;i<nAngContacts;i++) // angular contacts
		body0->Tcollision += (g_ContactsCG[pContacts[i]->bProcessed].dP*g_ContactsCG[pContacts[i]->bProcessed].r.x)*
												 g_ContactsC[pContacts[i]->bProcessed].Kinv(0,0); 
	for(;i<nContacts;i++) { // positional contacts
		r0 = pContacts[i]->pt[pContacts[i]->flags>>contact_bidx_log2&1]-body0->pos;
		dP = (g_ContactsCG[pContacts[i]->bProcessed].dP*g_ContactsCG[pContacts[i]->bProcessed].r.x)*g_ContactsC[pContacts[i]->bProcessed].Kinv(0,0);
		body0->Fcollision += dP; body0->Tcollision += r0^dP;
	}
	for(i=0;i<nAngContacts;i++)	// angular contacts
		g_ContactsCG[pContacts[i]->bProcessed].vrel = body0->Iinv*body0->Tcollision;
	for(;i<nContacts;i++) { // positional contacts
		r0 = pContacts[i]->pt[pContacts[i]->flags>>contact_bidx_log2&1]-body0->pos;
		g_ContactsCG[pContacts[i]->bProcessed].vrel = body0->Fcollision*body0->Minv + (body0->Iinv*body0->Tcollision^r0);
	}
	for(i=0;i<nContacts;i++)
		rT_x_rc += (g_ContactsCG[pContacts[i]->bProcessed].dP*g_ContactsCG[pContacts[i]->bProcessed].vrel)*g_ContactsCG[pContacts[i]->bProcessed].r.x*
								g_ContactsC[pContacts[i]->bProcessed].Kinv(0,0);

	return rT_x_rc;
}


void InitContactSolver(float time_interval)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Physics, 0, "Physics Contact Solver");

	int iCaller = get_iCaller_int();
	if (!g_RBdata[iCaller])
		g_RBdata[iCaller] = new RBdata;
	g_nContacts = g_nBodies = 0;
	g_iSolverBufPos = 0;
	g_bUsePreCG = true;
	g_iSolverBufAuxPos = 0;
}

void CleanupContactSolvers()
{
	for (size_t i = 0; i < MAX_PHYS_THREADS-1+FIRST_WORKER_THREAD; ++ i)
	{
		delete g_RBdata[i];
		g_RBdata[i] = 0;
	}
}

char *AllocSolverTmpBuf(int size)
{
	int iCaller = get_iCaller_int();
	if (g_iSolverBufPos+size<sizeof(g_SolverBuf)) {
		g_iSolverBufPos += size;
		memset(g_SolverBuf+g_iSolverBufPos-size, 0, size);
		return g_SolverBuf+g_iSolverBufPos-size;
	}	else {
		if (g_iSolverBufAuxPos+size>g_sizeSolverBufAux)	{
			char *pBufAux = g_pSolverBufAux;
			ReallocateList(g_pSolverBufAux, g_iSolverBufAuxPos, g_sizeSolverBufAux+=sizeof(g_SolverBuf));
			for(int i=0; i<g_nContacts; i++) {
				if ((unsigned int)((char*)g_pContacts[i]-pBufAux) < (unsigned int)(g_sizeSolverBufAux-sizeof(g_SolverBuf)))
					(char*&)g_pContacts[i] += g_pSolverBufAux-pBufAux;
				if ((unsigned int)((char*)g_pContacts[i]->pBounceCount-pBufAux) < (unsigned int)(g_sizeSolverBufAux-sizeof(g_SolverBuf)))
					(char*&)g_pContacts[i]->pBounceCount += g_pSolverBufAux-pBufAux;
			}
		}
		memset(g_pSolverBufAux+g_iSolverBufAuxPos, 0, size);
		g_iSolverBufAuxPos += size;
		return g_pSolverBufAux+g_iSolverBufAuxPos-size;
	}
	return 0;
}

extern RigidBody g_StaticRigidBodies[];
void RegisterContact(entity_contact *pcontact)
{
	int iCaller = get_iCaller_int();
	if (!(pcontact->flags & (contact_maintain_count|contact_rope)))
		pcontact->pBounceCount = &pcontact->iCount;
	if ((UINT_PTR)pcontact->pbody[1]-(UINT_PTR)g_StaticRigidBodies<(UINT_PTR)(sizeof(RigidBody)*MAX_PHYS_THREADS))
		pcontact->pbody[1] = &g_StaticRigidBodies[iCaller];
	g_pContacts[g_nContacts++] = pcontact;
	g_nContacts = min(g_nContacts,(int)(CRY_ARRAY_COUNT(g_pContacts))-1);
}

void DisablePreCG()
{
	int iCaller = get_iCaller_int();
	g_bUsePreCG = false;
}

inline void ApplyImpulse(body_helper *pbody, const Vec3& dP, const Vec3& dL, int iCaller)
{
	if (pbody->M>=0.0f) {
		pbody->v += dP*pbody->Minv; pbody->w += pbody->Iinv*dL; pbody->L += dL;
	} else
		(*(ArticulatedBody**)&pbody->Iinv)->ApplyImpulse(dP,dL,g_RBdata[iCaller]->Bodies,iCaller);
}


#undef g_nContacts
#undef g_ContactsRB
#undef g_ContactsC
#undef g_Bodies
#define g_nContacts  nContacts
#define g_ContactsRB pContactsRB
#define g_ContactsC  pContactsC
#define g_Bodies     pBodies

int InvokeContactSolverMC(contact_helper *pContactsRB,contact_helper_constraint *pContactsC,body_helper *pBodies, 
													int nContacts,int nBodies, float Ebefore, int nMaxIters,float e,float minSeparationSpeed)
{
	CRY_PROFILE_REGION(PROFILE_PHYSICS, "LCPMC");
	int iCaller = get_iCaller_int();
	int i,j,bBounced,istart,iend,istep,nBounces=0,bContactBounced;
	float vrel,dPn,dPtang,Eafter;
	Vec3 r0,r1,dp,dP,n,Kdp;
	body_helper *hbody0,*hbody1;
	rope_solver_vtx *prope;

	do {
		bBounced = 0;
		//istep = ((iter&1)<<1)-1;
		//istart = g_nContacts-1 & -(iter&1^1);
		//iend = (g_nContacts+1 & -(iter&1))-1;
		istart=0; iend=g_nContacts; istep=1;

		for(i=istart; i!=iend; i+=istep) {
			if (g_ContactsRB[i].iCount >= (g_ContactsRB[i].flags & contact_count_mask))	{
				hbody0 = g_Bodies+g_ContactsRB[i].iBody[0]; hbody1 = g_Bodies+g_ContactsRB[i].iBody[1];
				r0=g_ContactsRB[i].r0; r1=g_ContactsRB[i].r1; n=g_ContactsRB[i].n;

				if (g_ContactsRB[i].flags & contact_rope) {
					vrel = g_ContactsRB[i].n*(hbody0->v + (hbody0->w^r0));
					vrel += g_ContactsRB[i].vreq*(hbody1->v + (hbody1->w^r1));
					prope = (rope_solver_vtx*)g_pContacts[i]->pBounceCount;
					for(j=0; j<g_pContacts[i]->iCount; j++)	{
						body_helper *hbody = g_Bodies+prope[j].iBody;
						vrel += prope[j].v*(hbody->v+(hbody->w^prope[j].r));
					}
					//if ((vrel-=g_ContactsRB[i].friction)<-e) {
					if (max((vrel-=g_ContactsRB[i].friction)+e, g_ContactsRB[i].Pn*g_ContactsRB[i].K(1,0)-g_ContactsRB[i].K(0,2)-1e-6f)<0) {
						dPn = -vrel*g_ContactsRB[i].K(0,1);
						dPn = min(dPn, dPn*(1.0f-g_ContactsRB[i].K(1,0)) + (g_ContactsRB[i].K(0,2)*1.001f-g_ContactsRB[i].Pn)*g_ContactsRB[i].K(1,0));
						g_ContactsRB[i].Pn += dPn;
						dP = g_ContactsRB[i].n*dPn; 
						hbody0->v += dP*hbody0->Minv; hbody0->w += hbody0->Iinv*(dp=r0^dP); hbody0->L += dp;
						dP = g_ContactsRB[i].vreq*(dPn*g_ContactsRB[i].K(0,0));
						hbody1->v += dP*hbody1->Minv; hbody1->w += hbody1->Iinv*(dp=r1^dP); hbody1->L += dp;
						for(j=0; j<g_pContacts[i]->iCount; j++)	{
							body_helper *hbody = g_Bodies+prope[j].iBody;
							dP = prope[j].P*dPn;
							hbody->v += dP*hbody->Minv; hbody->w += hbody->Iinv*(dp=prope[j].r^dP); hbody->L += dp;
						}
						bBounced++;	nBounces++;
					}
					continue;
				}

				if (!(g_ContactsRB[i].flags & contact_angular)) {
					r0 = g_ContactsRB[i].r0; r1 = g_ContactsRB[i].r1;
					dp = hbody0->v+(hbody0->w^r0) - hbody1->v-(hbody1->w^r1);
				} else
					dp = hbody0->w-hbody1->w;
				dp -= g_ContactsRB[i].vreq;
				if (g_ContactsRB[i].flags & contact_use_C)
					dp = g_ContactsC[i].C*dp;
				bContactBounced = 0;

				if (g_ContactsRB[i].flags & contact_constraint) {
					if ((g_ContactsC[i].C*dp).len2() > max(sqr(e),g_ContactsRB[i].vreq.len2()*sqr(0.05f))) {
						dP = g_ContactsC[i].Kinv*-dp;
						dPn = dP*g_ContactsRB[i].n;
						if (min(g_ContactsRB[i].Pspare, fabsf(g_ContactsRB[i].Pn+dPn)-g_ContactsRB[i].Pspare*1.01f)>1e-5f) {
							float t = (g_ContactsRB[i].Pspare*1.01f-fabsf(g_ContactsRB[i].Pn))/fabsf(dPn);
							dP*=t; dPn*=t;
							bContactBounced = isneg(0.001f-t);
						}	else
							bContactBounced = 1; 
						g_ContactsRB[i].Pn += dPn;
					}
				} else if (!(g_ContactsRB[i].flags & contact_wheel)) {
					if ((vrel=dp*n)<0 &&
							(isneg(e-fabs_tpl(vrel)) | isneg(0.0001f-g_ContactsRB[i].Pspare) & isneg(sqr(minSeparationSpeed)-(dp-n*vrel).len2()))) 
					//if ((vrel=dp*n)<0 && (vrel<-0.003 || g_pContacts[i]->Pspare>0.0001f && (dp-n*vrel).len2()>sqr(pss->minSeparationSpeed)))
					{
						if (g_ContactsRB[i].friction>0.01f) {
							dP = dp*(-dp.len2()/(dp*g_ContactsRB[i].K*dp));
							g_ContactsRB[i].Pn += dPn=dP*n;
							g_ContactsRB[i].Pspare += (dPn*=g_ContactsRB[i].friction);
							dPtang = sqrt_tpl(max(0.0f,dP.len2()-sqr(dP*n)));
							g_ContactsRB[i].Pspare -= dPtang;
							if (g_ContactsRB[i].Pspare<0) {	// friction cannot stop sliding
								dp += (dp-n*vrel)*((g_ContactsRB[i].Pspare)/dPtang); // remove part of dp that friction cannot stop
								Kdp = g_ContactsRB[i].K*dp;
								if (sqr(Kdp*n) < Kdp.len2()*0.001f)	// switch to frictionless contact in dangerous cases
									dP = n*-vrel/(n*g_ContactsRB[i].K*n);
								else
									dP = dp*-vrel/(n*Kdp); // apply impulse along dp so that it stops normal component
								g_ContactsRB[i].Pspare = 0;
							}
						} else {
							dP = n*(dPn=-vrel/(n*g_ContactsRB[i].K*n));
							g_ContactsRB[i].Pn += dPn;
						}
						bContactBounced = 1; 
					}
				} else if (dp.len2()>sqr(e) && g_ContactsRB[i].Pspare>hbody0->M*0.001f) {
					dP = dp*(-dp.len2()/(dp*g_ContactsRB[i].K*dp));
					g_ContactsRB[i].Pn += dPn=dP*n;
					dPn *= g_ContactsRB[i].friction;
					dPtang = sqrt_tpl(max(0.0f,dP.len2()-sqr(dP*n)));
					if (dPtang > dPn*1.01f)	{
						if (g_ContactsRB[i].Pspare*0.5f < dPtang-dPn)	{
							dP *= g_ContactsRB[i].Pspare*0.5f/(dPtang-dPn);
							g_ContactsRB[i].Pspare *= 0.5f;
						} else
							g_ContactsRB[i].Pspare -= dPtang-dPn;
						dP -= n*min(0.0f,dP*n);
					}
					bContactBounced = 1; 
				}

				if (bContactBounced) {
					if (g_ContactsRB[i].flags & contact_use_C)
						dP = g_ContactsC[i].C*dP;
					if (hbody0->M+hbody1->M>0) {
						if (!(g_ContactsRB[i].flags & contact_angular)) {
							hbody0->v += dP*hbody0->Minv; hbody0->w += hbody0->Iinv*(dp=r0^dP); hbody0->L += dp;
							hbody1->v -= dP*hbody1->Minv; hbody1->w -= hbody1->Iinv*(dp=r1^dP);	hbody1->L -= dp;
						}	else {
							hbody0->w += hbody0->Iinv*dP; hbody0->L += dP;
							hbody1->w -= hbody1->Iinv*dP; hbody1->L -= dP;
						}
					} else {
						if (!(g_ContactsRB[i].flags & contact_angular)) {
							ApplyImpulse(hbody0, dP,r0^dP, iCaller);
							ApplyImpulse(hbody1,-dP,dP^r1, iCaller);
						}	else {
							ApplyImpulse(hbody0, Vec3(ZERO),dP, iCaller);
							ApplyImpulse(hbody1, Vec3(ZERO),-dP, iCaller);
						}
					}
					g_ContactsRB[g_ContactsRB[i].iCountDst].iCount++;
					bBounced++;	nBounces++;
				}
			}
			g_ContactsRB[i].iCount = 0;
		} 

		for(i=0,Eafter=0.0f; i<nBodies; i++)
			Eafter += g_Bodies[i].v.len2()*g_Bodies[i].M + g_Bodies[i].L*g_Bodies[i].w;
		nBounces += g_nContacts-bBounced >> 4;

	} while (bBounced && nBounces<nMaxIters && Eafter<Ebefore*3.0f);
	return bBounced;
}

#undef g_nContacts
#undef g_ContactsRB
#undef g_ContactsC
#undef g_Bodies
#define g_nContacts  g_RBdata[iCaller]->nContacts
#define g_ContactsRB g_RBdata[iCaller]->ContactsRB
#define g_ContactsC  g_RBdata[iCaller]->ContactsC
#define g_Bodies     g_RBdata[iCaller]->Bodies


int ReadDelayedSolverResults(CMemStream &stm, entity_contact **&pContactsOut,RigidBody **&pBodiesOut)
{
	int iCaller = get_iCaller_int();
	int nBodies,flags,nMaxIters,bBounced;
	float dt,Ebefore,e,minSeparationSpeed;
#if defined(MEMSTREAM_DEBUG)
	{ int tag; stm.Read(tag); if (tag!=MEMSTREAM_DEBUG_TAG) {printf("begin %x!=%x\n",tag, MEMSTREAM_DEBUG_TAG); __debugbreak(); }}
#endif 
	stm.Read(g_nContacts); stm.Read(nBodies);	
	stm.Read(dt); stm.Read(Ebefore); 
	stm.Read(nMaxIters); stm.Read(e); stm.Read(minSeparationSpeed);
	stm.Read(flags); stm.Read(bBounced);
	stm.ReadRaw(g_ContactsRB, sizeof(contact_helper)*g_nContacts);
	if (flags & (contact_constraint|contact_use_C))
		stm.ReadRaw(g_ContactsC, sizeof(contact_helper_constraint)*g_nContacts);
	stm.ReadRaw(g_Bodies, sizeof(body_helper)*nBodies);
#if defined(MEMSTREAM_DEBUG)
	{ int tag; stm.Read(tag); if (tag!=MEMSTREAM_DEBUG_TAG+1) {printf("end %x!=%x\n",tag, MEMSTREAM_DEBUG_TAG+1); __debugbreak(); }}
#endif 
	pContactsOut=g_pContacts; pBodiesOut=g_pBodies;
	return nBodies<<16 | bBounced;
}

void InvokeDelayedContactSolver(CMemStream &stm)
{
	int nContacts,nBodies,nMaxIters,flags,dummy;
	float time_interval,Ebefore,e,minSeparationSpeed;
#if defined(MEMSTREAM_DEBUG)
  {int tag; stm.Read(tag); if (tag!=MEMSTREAM_DEBUG_TAG) {printf("beg %x!=%x\n",tag, MEMSTREAM_DEBUG_TAG); __debugbreak(); }}
#endif 
	stm.Read(nContacts); stm.Read(nBodies);	
	stm.Read(time_interval); stm.Read(Ebefore);	
	stm.Read(nMaxIters); stm.Read(e); stm.Read(minSeparationSpeed);
	stm.Read(flags);
	int *bBounced = (int*)(stm.m_pBuf+stm.m_iPos);
	stm.Read(dummy); // read over bBounced, but into a dummy var or else an overlapping memcpy happens(which is not allowed)
	contact_helper *pContactsRB = (contact_helper*)(stm.m_pBuf+stm.m_iPos); stm.m_iPos+=sizeof(contact_helper)*nContacts;
	contact_helper_constraint *pContactsC = (contact_helper_constraint*)(stm.m_pBuf+stm.m_iPos);
	if (flags & (contact_constraint|contact_use_C)) stm.m_iPos+=sizeof(contact_helper_constraint)*nContacts;
	body_helper *pBodies = (body_helper*)(stm.m_pBuf+stm.m_iPos); stm.m_iPos+=sizeof(body_helper)*nBodies;
#if defined(MEMSTREAM_DEBUG)
	{int tag; stm.Read(tag); if (tag!=MEMSTREAM_DEBUG_TAG+1) {printf("end %x!=%x\n",tag, MEMSTREAM_DEBUG_TAG+1); __debugbreak();}}
#endif 
	stm.bMeasureOnly = 0;
	*bBounced = InvokeContactSolverMC(pContactsRB,pContactsC,pBodies, nContacts,nBodies, Ebefore,nMaxIters,e,minSeparationSpeed);
}


struct SEntityGrid;
#ifdef MULTI_GRID
SEntityGrid *GetGridFromEntity(const CPhysicalEntity*);
QuatT GetGridTransform(const SEntityGrid *src, const SEntityGrid *dst);
Vec3 GetGridRelVel(const SEntityGrid *src, const SEntityGrid *dst);
#else
SEntityGrid *GetGridFromEntity(const CPhysicalEntity*) { return nullptr; }
QuatT GetGridTransform(const SEntityGrid *src, const SEntityGrid *dst) { return QuatT(IDENTITY); }
Vec3 GetGridRelVel(const SEntityGrid *src, const SEntityGrid *dst) { return Vec3(ZERO); }
#endif

struct noGridTrans {
	noGridTrans(const CPhysicalEntity *srcEnt, const SEntityGrid *dstGrid) {}
	noGridTrans(const SEntityGrid *srcGrid, const CPhysicalEntity *dstEnt) {}
	noGridTrans(const CPhysicalEntity *srcEnt, const CPhysicalEntity *dstEnt) {}
	template<typename T> const T& operator()(const T& op) const { return op; }
	template<typename T> const T& operator()(const T& op,bool) const { return op; }
};
struct GridTransFull {
	QuatT trans;
	GridTransFull(const CPhysicalEntity *srcEnt, const CPhysicalEntity *dstEnt) { trans = ::GetGridTransform(::GetGridFromEntity(srcEnt),::GetGridFromEntity(dstEnt)); }
	Vec3 operator()(const Vec3& pt) const { return trans*pt; }
};
struct GridTransRot {
	Quat q;
	GridTransRot(const CPhysicalEntity *srcEnt, const SEntityGrid *dstGrid) { q = ::GetGridTransform(::GetGridFromEntity(srcEnt),dstGrid).q; }
	Vec3 operator()(const Vec3& dir) const { return q*dir; }
	Matrix33 operator()(const Matrix33 &M) const { return Matrix33(q)*M*Matrix33(!q); }
};
struct GridTransDyn {
	Quat q;
	Vec3 dv;
	GridTransDyn(const SEntityGrid *srcGrid, const SEntityGrid *dstGrid) { 
		q = ::GetGridTransform(srcGrid,dstGrid).q; 
		dv = ::GetGridRelVel(srcGrid,dstGrid);
	}
	GridTransDyn(const CPhysicalEntity *srcEnt, const SEntityGrid *dstGrid) : GridTransDyn(::GetGridFromEntity(srcEnt),dstGrid) {} 
	GridTransDyn(const SEntityGrid *srcGrid, const CPhysicalEntity *dstEnt) : GridTransDyn(srcGrid,::GetGridFromEntity(dstEnt)) {} 
	Vec3 operator()(const Vec3& dir) const { return q*dir; }
	Vec3 operator()(const Vec3& dir, bool addVel) const { return q*dir+dv; }
	Matrix33 operator()(const Matrix33 &M) const { return Matrix33(q)*M*Matrix33(!q); }
};

template<typename TransFull, typename TransRot, int ipass>
int PrepContacts(int iCaller, int &nConstraintsRes, SEntityGrid* &pgridRes, int &bMultigridRes)
{
	int nBodies=0, nConstraints=0, bMultigrid=0;
	SEntityGrid *pgrid = ipass==0 ? GetGridFromEntity(g_pContacts[0]->pent[0]) : pgridRes;
	RigidBody *pbodyGrid = g_pContacts[0]->pbody[0];
	Matrix33 Ctmp;

	for(int i=0,j,iop; i<g_nContacts; i++) {
		entity_contact &cnt = *g_pContacts[i];
		if (ipass==0) {
			for(iop=0;iop<2;iop++) {
				if (!(j = cnt.pbody[iop]->bProcessed[iCaller])) {
					g_pBodies[nBodies++] = cnt.pbody[iop];
					cnt.pbody[iop]->bProcessed[iCaller] = j=nBodies;
				}
				g_ContactsRB[i].iBody[iop] = j-1;
				if (GetGridFromEntity(cnt.pent[iop]) != pgrid) {
					bMultigrid = 1;
					if (cnt.pbody[iop]->flags & rb_articulated || cnt.pbody[iop]->Minv < pbodyGrid->Minv) {
						pbodyGrid = cnt.pbody[iop]; pgrid = GetGridFromEntity(cnt.pent[iop]);
					}
				}
			}
			if (!(cnt.flags & contact_rope))
				cnt.iCount = 0;
			else for(rope_solver_vtx *prope=(rope_solver_vtx*)g_pContacts[iop=0,i]->pBounceCount; iop<cnt.iCount; iop++) {
				if (!(j = prope[iop].pbody->bProcessed[iCaller])) {
					*(CPhysicalEntity**)&g_Bodies[nBodies].Iinv = prope[iop].pent;
					g_pBodies[nBodies++] = prope[iop].pbody;
					prope[iop].pbody->bProcessed[iCaller] = j=nBodies;
					if (GetGridFromEntity(prope[iop].pent) != pgrid) {
						bMultigrid = 1;
						if (prope[iop].pbody->flags & rb_articulated || prope[iop].pbody->Minv < pbodyGrid->Minv) {
							pbodyGrid = prope[iop].pbody; pgrid = GetGridFromEntity(prope[iop].pent);
						}
					}
				}
				prope[iop].iBody = j-1;
			}

			if (!(cnt.flags & contact_preserve_Pspare))
				cnt.Pspare = 0;
			nConstraints -= -(cnt.flags & contact_constraint)>>31;
			g_ContactsCG[i].P.zero();
			cnt.bProcessed = i;
			g_ContactsC[i].C.SetIdentity();
			if (bMultigrid)
				continue;
		}	else for(iop=0;iop<2;iop++)
			*(CPhysicalEntity**)&g_Bodies[cnt.pbody[iop]->bProcessed[iCaller]-1].Iinv = cnt.pent[iop];

		TransRot AtoG(cnt.pent[0],pgrid), BtoG(cnt.pent[1],pgrid);
		TransFull AtoB(cnt.pent[0], cnt.pent[1]);

		if (cnt.flags & contact_use_C_1dof)	{
			Vec3 nloc = AtoG(cnt.nloc);
			dotproduct_matrix(nloc,nloc, g_ContactsC[i].C);
		} else if (cnt.flags & contact_use_C_2dof) {
			Vec3 nloc = AtoG(cnt.nloc);
			g_ContactsC[i].C -= dotproduct_matrix(nloc,nloc,Ctmp);
		}

		if (cnt.flags & contact_rope) {
			g_ContactsRB[i].K(0,0) = cnt.nloc.x;
			g_ContactsRB[i].K(0,1) = cnt.nloc.y;
			g_ContactsRB[i].K(0,2) = cnt.nloc.z;
			g_ContactsRB[i].K(1,0) = isneg(1e-8f-cnt.nloc.z);
		} else {
			Matrix33 K0(ZERO), K1(ZERO);
			if (!(cnt.flags & contact_angular)) {
				cnt.pbody[0]->GetContactMatrix(cnt.pt[0]-cnt.pbody[0]->pos, K0);
				cnt.pbody[1]->GetContactMatrix(AtoB(cnt.pt[1])-cnt.pbody[1]->pos, K1);
			} else {
				K0 = cnt.pbody[0]->Iinv;
				K1 = cnt.pbody[1]->Iinv;
			}
			g_ContactsRB[i].K = AtoG(K0) + BtoG(K1);
		}

		if (cnt.flags & contact_constraint_3dof)
			(g_ContactsC[i].Kinv=g_ContactsRB[i].K).Invert();
		else {
			Vec3 n = AtoG(cnt.n);
			if (cnt.flags & contact_constraint_1dof) {
				Vec3 axes[2]; int k; float mtx[2][2]; Matrix33 mtx1;
				axes[0] = n.GetOrthogonal().normalized();
				axes[1] = n ^ axes[0];
				for(j=0;j<2;j++) for(k=0;k<2;k++) mtx[j][k] = axes[j]*g_ContactsRB[i].K*axes[k];
				matrixf(2,2,0,mtx[0]).invert();
				g_ContactsC[i].Kinv.SetZero();
				for(j=0;j<2;j++) for(k=0;k<2;k++) 
					g_ContactsC[i].Kinv += dotproduct_matrix(axes[j],axes[k],mtx1)*mtx[j][k];
				dotproduct_matrix(n,n, g_ContactsC[i].C) *= -1.0f;
				g_ContactsC[i].C(0,0)+=1.0f; g_ContactsC[i].C(1,1)+=1.0f; g_ContactsC[i].C(2,2)+=1.0f; 
			}	else if (cnt.flags & contact_constraint_2dof) {
				float t = n*g_ContactsRB[i].K*n;
				dotproduct_matrix(n,n, g_ContactsC[i].C);
				(g_ContactsC[i].Kinv = g_ContactsC[i].C) /= t;
			} else if (cnt.friction<0.01f)
				dotproduct_matrix(n,n, g_ContactsC[i].C);
		}
	}

	if (ipass==0) {
		nConstraintsRes=nConstraints; pgridRes=pgrid; bMultigridRes=bMultigrid;
	}
	return nBodies;
}

template<typename TransFull,typename TransRot>
void PrepContactsMore(int iCaller, SEntityGrid *pgrid=nullptr)
{
	for(int i=0; i<g_nContacts; i++) {
		entity_contact &cnt = *g_pContacts[i];
		TransRot AtoG(cnt.pent[0], pgrid);
		TransFull BtoA(cnt.pent[1], cnt.pent[0]);
		Vec3 com0 = cnt.pbody[0]->pos;
		if (sizeof(TransFull)>sizeof(noGridTrans) && cnt.flags & contact_rope) {
			CPhysicalEntity *prope = (CPhysicalEntity*)cnt.nextAux;
			AtoG = TransRot(prope, pgrid);
			BtoA = TransFull(cnt.pent[1], prope);
			com0 = TransFull(cnt.pent[0],prope)(com0);
			rope_solver_vtx *pvtx = (rope_solver_vtx*)cnt.pBounceCount;
			for(int j=0; j<cnt.iCount; j++)	{
				pvtx[j].r = AtoG(pvtx[j].r);
				pvtx[j].P = AtoG(pvtx[j].P);
				pvtx[j].v = AtoG(pvtx[j].v);
			}
		}
		g_ContactsRB[i].r0 = AtoG(cnt.pt[0]-com0);
		g_ContactsRB[i].r1 = AtoG(cnt.pt[1]-BtoA(cnt.pbody[1]->pos));
		g_ContactsRB[i].n = AtoG(cnt.n);
		g_ContactsRB[i].vreq = AtoG(cnt.vreq);
		g_ContactsRB[i].Pspare = cnt.Pspare;
		g_ContactsRB[i].flags = cnt.flags;
		g_ContactsRB[i].friction = cnt.friction;
		g_ContactsRB[i].iCount = cnt.iCount;
		g_ContactsRB[i].iCountDst = ((entity_contact*)((char*)cnt.pBounceCount-
			((char*)&cnt.iCount-(char*)g_pContacts[i])))->bProcessed;
		g_ContactsRB[i].Pn = 0;
	}
}

template<typename TransDyn>
int PrepBodies(int iCaller, SEntityGrid *pgrid=nullptr)
{
	int nArtic=0, nBodies=g_nBodies;
	for(int i=0;i<nBodies;i++) {
		TransDyn toG(*(CPhysicalEntity**)&g_Bodies[i].Iinv, pgrid);
		g_pBodies[i]->Fcollision = g_pBodies[i]->P; g_pBodies[i]->Tcollision = g_pBodies[i]->L;
		g_Bodies[i].L = toG(g_pBodies[i]->L);
		g_Bodies[i].v = toG(g_pBodies[i]->v,true); g_Bodies[i].w = toG(g_pBodies[i]->w);
		if (!(g_pBodies[i]->flags & rb_articulated)) {
			g_Bodies[i].Minv = g_pBodies[i]->Minv; g_Bodies[i].Iinv = toG(g_pBodies[i]->Iinv); g_Bodies[i].M = g_pBodies[i]->M;
		} else {
			g_Bodies[i].M=-1e20f; g_Bodies[i].Minv=0.0f;
			*((ArticulatedBody**)&g_Bodies[i].Iinv) = (ArticulatedBody*)g_pBodies[i];
			nArtic++;
		}
	}
	return nArtic;
}

template<typename TransDyn>
void UpdateBodies(int iCaller, SEntityGrid *pgrid=nullptr)
{
	int nBodies = g_nBodies;
	if (sizeof(TransDyn) > sizeof(noGridTrans))
		for(int i=0;i<g_nContacts;i++) {
			for(int iop=0;iop<2;iop++)
				*(CPhysicalEntity**)&g_Bodies[g_pContacts[i]->pbody[iop]->bProcessed[iCaller]-1].Iinv = g_pContacts[i]->pent[iop];
			if (g_pContacts[i]->flags & contact_rope) {
				rope_solver_vtx *pvtx = (rope_solver_vtx*)g_pContacts[i]->pBounceCount;
				for(int j=0; j<g_pContacts[i]->iCount; j++)
					*(CPhysicalEntity**)&g_Bodies[pvtx[j].pbody->bProcessed[iCaller]-1].Iinv = pvtx[j].pent;
			}
		}
	for(int i=0; i<nBodies; i++) {
		if (sizeof(TransDyn)==sizeof(noGridTrans) || g_pBodies[i]->Minv>0) {
			TransDyn fromG(pgrid, *(CPhysicalEntity**)&g_Bodies[i].Iinv);
			g_pBodies[i]->P = (g_pBodies[i]->v=fromG(g_Bodies[i].v,true))*g_pBodies[i]->M; 
			g_pBodies[i]->L = g_pBodies[i]->q*(g_pBodies[i]->Ibody*(!g_pBodies[i]->q*(g_pBodies[i]->w = fromG(g_Bodies[i].w))));
		}
		g_pBodies[i]->bProcessed[iCaller] = 0; 
		g_pBodies[i]->Eunproj = 0;
	}
}


int InvokeContactSolver(float time_interval, SolverSettings *pss, float Ebefore, entity_contact **&pContactsOut,int &nContactsOut)
{
	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );

	int iCaller = get_iCaller()-1+FIRST_WORKER_THREAD;
	pContactsOut=g_pContacts;
	if (g_nContacts==0) return 0;
	int i,j,iop,nBodies,istart,iend,iter,bBounced,nConstraints,nMaxIters,nBuddies,bNeedLCPCG=0;
	RigidBody *body0,*body1;
	rope_solver_vtx *prope;
	Vec3 r0,r1,n,dp,dP;
	float vrel,Eafter,dPn,dPtang,rtime_interval=1/time_interval;
	float e = pss->accuracyMC;
	Matrix33 Ctmp;
	buddy_info *pbuddy0,*pbuddy1;
	SEntityGrid *pgrid;
	int bMultigrid = 0;
__solver_step++;
	nContactsOut = g_nContacts;

	nBodies = PrepContacts<noGridTrans,noGridTrans,0>(iCaller,nConstraints,pgrid,bMultigrid);
	if (bMultigrid)
		PrepContacts<GridTransFull,GridTransRot,1>(iCaller,nConstraints,pgrid,bMultigrid);

	if (pss->nMaxLCPCGiters>0 && !bMultigrid) {
		for(i=0;i<nBodies;i++) {
			g_infos[i].pbuddy=0; g_infos[i].iLevel=-1; 
			bNeedLCPCG |= isneg(sqr(pss->maxMCVel)-g_pBodies[i]->v.len2());
		}
		if (pss->maxMCMassRatio<=1.0f)
			bNeedLCPCG = 1;
		if (!bNeedLCPCG) {
			// require that the all contacts are grouped by pbody[0]
			for(istart=nBuddies=0; istart<g_nContacts; istart=iend) {
				// sub-group contacts relating to each pbody[0] by pbody[1] (using quick sort)
				for(iend=istart+1; iend<g_nContacts && g_pContacts[iend]->pbody[0]==g_pContacts[istart]->pbody[0]; iend++);
				//qsort(g_pContacts, istart,iend-1);
				// for each body: find all its contacting bodies and for each such body get integral vreq (but ignore separating contacts)
				for(i=istart; i<iend; i=j) {
					for(j=i,n.zero(); j<iend && g_pContacts[j]->pbody[1]==g_pContacts[i]->pbody[1]; j++)
						n += g_pContacts[j]->n;
					if (g_pContacts[i]->pbody[0]->Minv > g_pContacts[i]->pbody[1]->Minv*pss->maxMCMassRatio) { 
						g_buddybuf[nBuddies].next = g_infos[bidx0(i)].pbuddy;
						g_buddybuf[nBuddies].iBody = bidx1(i);
						g_buddybuf[nBuddies].vreq = n;
						g_infos[bidx0(i)].pbuddy = g_buddybuf+nBuddies++;
					}
					if (g_pContacts[i]->pbody[1]->Minv > g_pContacts[i]->pbody[0]->Minv*pss->maxMCMassRatio) { 
						g_buddybuf[nBuddies].next = g_infos[bidx1(i)].pbuddy;
						g_buddybuf[nBuddies].iBody = bidx0(i);
						g_buddybuf[nBuddies].vreq = -n;
						g_infos[bidx1(i)].pbuddy = g_buddybuf+nBuddies++;
					}
				}
			}
			// for every 2 contacting bodies of each body check if vreqs are conflicting, register sandwich triplet if so (1 inside, 2 outside)
			for(i=0; i<nBodies && !bNeedLCPCG; i++)
				for(pbuddy0=g_infos[i].pbuddy; pbuddy0; pbuddy0=pbuddy0->next)
					for(pbuddy1=pbuddy0->next; pbuddy1; pbuddy1=pbuddy1->next)
						bNeedLCPCG |= isneg(pbuddy0->vreq*pbuddy1->vreq);
		}
	}
	nMaxIters = max(pss->nMinMCiters*g_nContacts, pss->nMaxMCiters);
	g_nBodies = nBodies;

	if (g_bUsePreCG && g_nContacts<16 && !bMultigrid) {
		CRY_PROFILE_REGION(PROFILE_PHYSICS, "PreCG");

		real a,b,r2,r2new,pAp,vmax,vdiff;

		// try to drive all contact velocities to zero by using conjugate gradient
		for(i=0,r2=0,vmax=0;i<g_nContacts;i++) {
			body0 = g_pContacts[i]->pbody[0]; body1 = g_pContacts[i]->pbody[1];
			if (!(g_pContacts[i]->flags & contact_angular)) {
				r0 = g_pContacts[i]->pt[0]-body0->pos; r1 = g_pContacts[i]->pt[1]-body1->pos;
				dp = body0->v+(body0->w^r0) - body1->v-(body1->w^r1); 
			} else dp = body0->w-body1->w;
			g_ContactsCG[i].dP = g_ContactsCG[i].r = g_pContacts[i]->vreq - g_ContactsC[i].C*dp;
			g_ContactsCG[i].P.zero(); r2 += g_ContactsCG[i].r.len2(); vmax = max((float)vmax,g_ContactsCG[i].r.len2());
		}
		iter = g_nContacts*6;
		
		do {
			for(i=0;i<nBodies;i++) { g_pBodies[i]->Fcollision.zero(); g_pBodies[i]->Tcollision.zero(); }
			for(i=0;i<g_nContacts;i++) {
				body0 = g_pContacts[i]->pbody[0]; body1 = g_pContacts[i]->pbody[1]; 
				if (!(g_pContacts[i]->flags & contact_angular)) {
					r0 = g_pContacts[i]->pt[0]-body0->pos; r1 = g_pContacts[i]->pt[1]-body1->pos;
					body0->Fcollision += g_ContactsCG[i].dP; body0->Tcollision += r0^g_ContactsCG[i].dP;
					body1->Fcollision -= g_ContactsCG[i].dP; body1->Tcollision -= r1^g_ContactsCG[i].dP;
				} else {
					body0->Tcollision += g_ContactsCG[i].dP; body1->Tcollision -= g_ContactsCG[i].dP;
				}
			}
			for(i=0;i<g_nContacts;i++) {
				body0 = g_pContacts[i]->pbody[0]; body1 = g_pContacts[i]->pbody[1];
				if (!(g_pContacts[i]->flags & contact_angular)) {
					r0 = g_pContacts[i]->pt[0]-body0->pos; r1 = g_pContacts[i]->pt[1]-body1->pos;
					dp = body0->Fcollision*body0->Minv + (body0->Iinv*body0->Tcollision^r0);
					dp -= body1->Fcollision*body1->Minv + (body1->Iinv*body1->Tcollision^r1);
				} else
					dp = body0->Iinv*body0->Tcollision - body1->Iinv*body1->Tcollision;
				g_ContactsCG[i].vrel = g_ContactsC[i].C*dp;
			}
			
			for(i=0,pAp=0;i<g_nContacts;i++)
				pAp += g_ContactsCG[i].vrel*g_ContactsCG[i].dP;
			a = min((real)50.0, r2/max((real)1E-10,fabs_tpl(pAp)));
			for(i=0,r2new=0;i<g_nContacts;i++) {
				r2new += (g_ContactsCG[i].r -= g_ContactsCG[i].vrel*a).len2();
				g_ContactsCG[i].P += g_ContactsCG[i].dP*a;
			}
			r2new = max((real)1e-10, r2new);
			if (r2new>r2*500 || r2new>1e8f)
				break;
			b = r2new/max(r2new*(real)0.001,r2); r2=r2new;
			for(i=0,vmax=0;i<g_nContacts;i++) {
				(g_ContactsCG[i].dP*=b)+=g_ContactsCG[i].r;
				vmax = max((float)vmax,g_ContactsCG[i].r.len2());
			}
		} while (--iter && vmax>sqr(e));

		for(i=0;i<nBodies;i++) { g_pBodies[i]->Fcollision.zero(); g_pBodies[i]->Tcollision.zero(); }
		for(i=0;i<g_nContacts;i++) {
			body0 = g_pContacts[i]->pbody[0];	body1 = g_pContacts[i]->pbody[1];
			if (!(g_pContacts[i]->flags & contact_angular)) {
				body0->Fcollision += g_ContactsCG[i].P; body0->Tcollision += g_pContacts[i]->pt[0]-body0->pos ^ g_ContactsCG[i].P;
				body1->Fcollision -= g_ContactsCG[i].P; body1->Tcollision -= g_pContacts[i]->pt[1]-body1->pos ^ g_ContactsCG[i].P;
			}	else {
				body0->Tcollision += g_ContactsCG[i].P;	body1->Tcollision -= g_ContactsCG[i].P;
			}
		}
		for(i=0,Eafter=0;i<nBodies;i++)
			Eafter += (g_pBodies[i]->P+g_pBodies[i]->Fcollision)*(g_pBodies[i]->v+g_pBodies[i]->Fcollision*g_pBodies[i]->Minv) + 
				(g_pBodies[i]->L+g_pBodies[i]->Tcollision)*(g_pBodies[i]->w+g_pBodies[i]->Iinv*g_pBodies[i]->Tcollision);
		for(i=bBounced=0;i<g_nContacts;i++) {
			n = g_pContacts[i]->n; dPn = g_ContactsCG[i].P*n; dPtang = (g_ContactsCG[i].P-n*dPn).len2();
			float kmass = g_pContacts[i]->pbody[0]->Minv+g_pContacts[i]->pbody[1]->Minv;
			if (!(g_pContacts[i]->flags & contact_angular)) {
				dp = g_ContactsCG[i].P*kmass;	vdiff = -0.004f;
			} else {
				dp = g_pContacts[i]->pbody[0]->Iinv*g_ContactsCG[i].P + g_pContacts[i]->pbody[1]->Iinv*g_ContactsCG[i].P;
				vdiff = -0.015f;
			}
			bBounced += isneg(-(dp*n));
			if (!(g_pContacts[i]->flags & contact_constraint) &&
					(dp*n<-0.05f || // allow to pull bodies at contacts slightly
					dPtang*sqr(kmass) > sqr((dPn*g_pContacts[i]->friction+g_pContacts[i]->Pspare)*kmass+0.05f) || 
					(g_ContactsCG[i].r+g_pContacts[i]->vreq)*n<vdiff) || 
					g_pContacts[i]->flags & contact_constraint && g_pContacts[i]->flags & contact_preserve_Pspare &&
					dPn>g_pContacts[i]->Pspare*1.01f)
				break;
		}

		if (i==g_nContacts && Eafter<Ebefore*1.5f && bBounced && vmax<sqr(0.01)) { // conjugate gradient yielded acceptable results, apply these impulses and quit
			for(i=0;i<nBodies;i++) {
				if (g_pBodies[i]->M>0) {
					g_pBodies[i]->P += g_pBodies[i]->Fcollision; g_pBodies[i]->L += g_pBodies[i]->Tcollision;
					g_pBodies[i]->v = g_pBodies[i]->P*g_pBodies[i]->Minv; g_pBodies[i]->w = g_pBodies[i]->Iinv*g_pBodies[i]->L;
				}
				g_pBodies[i]->Fcollision *= rtime_interval; g_pBodies[i]->Tcollision *= rtime_interval;
				g_pBodies[i]->bProcessed[iCaller] = 0; 
				g_pBodies[i]->Eunproj = 0;
			}
			for(i=0;i<g_nContacts;i++) {
				//g_pContacts[i]->vrel = g_pContacts[i]->vreq-g_ContactsCG[i].r;
				g_pContacts[i]->Pspare = g_ContactsCG[i].dPn = g_ContactsCG[i].P*g_pContacts[i]->n;
			}
			return g_nBodies;
		}
	}

	int nArtic = 0;
	if (!bMultigrid) {
		PrepContactsMore<noGridTrans,noGridTrans>(iCaller);
		nArtic = PrepBodies<noGridTrans>(iCaller);
	} else {
		PrepContactsMore<GridTransFull,GridTransRot>(iCaller,pgrid);
		nArtic = PrepBodies<GridTransDyn>(iCaller,pgrid);
	}

	bBounced = InvokeContactSolverMC(g_ContactsRB,g_ContactsC,g_Bodies, g_nContacts,nBodies, Ebefore, nMaxIters,e,pss->minSeparationSpeed);

	if (!bMultigrid)
		UpdateBodies<noGridTrans>(iCaller);
	else
		UpdateBodies<GridTransDyn>(iCaller,pgrid);
	for(i=0; i<g_nContacts; i++)
		g_pContacts[i]->Pspare = g_ContactsRB[i].Pspare;

	/////////////////////////////////////////////////////////////////////////////////////////////////////////
	if (bBounced && pss->nMaxLCPCGiters>0 && bNeedLCPCG && g_nContacts<pss->maxLCPCGContacts && !nArtic) { 
	// use several iterations of CG solver, solving only for non-separating contacts
		unsigned int iClass;
		int cgiter,bStateChanged,n1dofContacts,n2dofContacts,nAngContacts,nFric0Contacts,nFricInfContacts,
			nContacts,nRopes,iSortedContacts[7],flags,bNoImprovement;
		real a,b,r2,r2new,pAp;
		body_helper *hbody0,*hbody1;
		float vmax=0;
		Vec3 vreq;
		e = pss->accuracyLCPCG;

		// prepare for CG solver: calculate target residuals (they will be used as starting residuals during each iteration) and vrel's
		for(i=bBounced=0; i<g_nContacts; i++) {
			body0 = g_pContacts[i]->pbody[0]; body1 = g_pContacts[i]->pbody[1];
			g_pContacts[i]->bProcessed = i;

			if (g_pContacts[i]->flags & contact_rope) {
				g_ContactsC[i].C.SetRow(0,g_pContacts[i]->n);
				g_ContactsC[i].C.SetRow(1,g_pContacts[i]->vreq);
				g_pContacts[i]->n.Set(1,0,0);
				for(iop=0,dp.zero(); iop<2; iop++) {
					body0 = g_pContacts[i]->pbody[iop]; r0 = g_pContacts[i]->pt[iop]-body0->pos;
					dp.x += g_ContactsC[i].C.GetRow(iop)*(body0->v + (body0->w^r0));
				} for(iop=0,prope=(rope_solver_vtx*)g_pContacts[i]->pBounceCount; iop<g_pContacts[i]->iCount; iop++)
					dp.x += prope[iop].v*(prope[iop].pbody->v + (prope[iop].pbody->w^prope[iop].r));
				g_ContactsRB[i].r0 = -dp;
				vreq.Set(g_pContacts[i]->friction,0,0);
				g_ContactsC[i].Kinv.SetZero();
				g_ContactsC[i].Kinv(0,0) = g_pContacts[i]->nloc.y;//K(0,1);
				if (g_ContactsRB[i].Pn*g_ContactsRB[i].K(1,0) > g_ContactsRB[i].K(2,0))
					g_pContacts[i]->flags &= ~contact_solve_for;
			} else {
				if (!(g_pContacts[i]->flags & contact_angular)) {
					r0 = g_pContacts[i]->pt[0]-body0->pos; r1 = g_pContacts[i]->pt[1]-body1->pos;
					dp = body0->v+(body0->w^r0) - body1->v-(body1->w^r1); 
				} else
					dp = body0->w-body1->w;
				vreq = g_pContacts[i]->vreq;
				if (g_pContacts[i]->flags & contact_use_C)
					dp = g_ContactsC[i].C*dp;

				if (g_pContacts[i]->flags & contact_wheel) {
					if (g_pContacts[i]->Pspare>g_pContacts[i]->pbody[0]->M*0.01f)	{
						g_pContacts[i]->flags = (g_pContacts[i]->flags & contact_use_C) | contact_constraint_3dof;
						(g_ContactsC[i].Kinv = g_ContactsRB[i].K).Invert();
					} else
						g_pContacts[i]->flags = contact_count_mask;	// don't solve for this contact
				}

				// remove constraint flags from contacts with counters, since constraints cannot to removed from solving process
				g_pContacts[i]->flags &= contact_count_mask | (g_pContacts[i]->flags&contact_count_mask)-1>>31;
				if (g_pContacts[i]->flags & contact_constraint_1dof)
					g_ContactsRB[i].r0 = (dp -= g_pContacts[i]->n*(dp*g_pContacts[i]->n));
				else if (g_pContacts[i]->flags&contact_constraint_2dof || !(g_pContacts[i]->flags&contact_constraint_3dof || g_pContacts[i]->Pspare>0)) {
					g_ContactsRB[i].r0.Set(g_pContacts[i]->n*dp,0,0); vreq.Set(g_pContacts[i]->n*vreq,0,0);
					dp = g_pContacts[i]->n*g_ContactsRB[i].r0.x;
					g_ContactsC[i].Kinv.SetZero();
					g_ContactsC[i].Kinv(0,0) = 1.0f/(g_pContacts[i]->n*g_ContactsRB[i].K*g_pContacts[i]->n);
				} else {
					g_ContactsRB[i].r0 = dp;
					if (!(g_pContacts[i]->flags & contact_constraint))
						(g_ContactsC[i].Kinv = g_ContactsRB[i].K).Invert(); // for constraints it's computed earlier
				}
				g_ContactsRB[i].r0.Flip();
			}
			
			if (g_pContacts[i]->flags & contact_constraint || (dp-vreq)*g_pContacts[i]->n<0)
				bBounced += isneg(sqr(e) - (dp-vreq).len2());
			g_pContacts[i]->vrel = (dp-vreq)*g_pContacts[i]->n;
			g_pContacts[i]->flags &= ~contact_solve_for;
		}

		if (bBounced) {
			{ CRY_PROFILE_REGION(PROFILE_PHYSICS,"LCPCG");

			cgiter = pss->nMaxLCPCGiters;
			for(i=0;i<nBodies;i++) {
				g_infos[i].Fcollision = g_pBodies[i]->Fcollision;
				g_infos[i].Tcollision = g_pBodies[i]->Tcollision;
			}
			vrel = min(pss->nMaxLCPCGmicroitersFinal,pss->nMaxLCPCGsubitersFinal)<=0 ? 1E10f : 0.0f;

			do {
				// sort contacts for the current iteration in the following way:
				// 0 | 1dof angular (always remove v-n*(v*n))					|	angular
				// 1 | 2dof angular constraints (always remove n*v)		|	angular
				// 2 | angular limits (remove n*v if <0)							|	angular
				// 3 | ropes																					| special
				// 4 | frictionless contacts (remove n*v if <0)				|	positional
				// 5 | infinite friction contacts (remove v if n*v<0)	|	positional
				// 6 | 3dof constraints (always remove v)							|	positional
				for(i=0;i<7;i++) iSortedContacts[i]=0;
				for(i=bStateChanged=0; i<g_nContacts; i++) {
					flags = g_pContacts[i]->flags;
					if (!(g_pContacts[i]->flags&contact_count_mask)) {
						g_pContacts[i]->flags |= contact_solve_for;
						if (g_pContacts[i]->flags & contact_constraint_1dof) iSortedContacts[0]++;
						else if (g_pContacts[i]->flags & contact_constraint_2dof) iSortedContacts[1]++;
						else if (g_pContacts[i]->flags & contact_constraint_3dof) iSortedContacts[6]++;
						else {
							if (g_pContacts[i]->vrel < vrel) {
								if (g_pContacts[i]->flags & contact_angular) iSortedContacts[2]++;
								else if (g_pContacts[i]->flags & contact_rope) iSortedContacts[3]++;
								else if (g_pContacts[i]->Pspare>0) iSortedContacts[5]++;
								else iSortedContacts[4]++;
							}	else
								g_pContacts[i]->flags &= ~contact_solve_for;
							bStateChanged += iszero((flags^g_pContacts[i]->flags) & contact_solve_for)^1;
						}
					}
				}
				if (!bStateChanged && vmax<sqr(e))
					break;
				iter = iSortedContacts[0]*2 + iSortedContacts[1]+iSortedContacts[2]+iSortedContacts[3]+iSortedContacts[4] + 
							 (iSortedContacts[5]+iSortedContacts[6])*3;
				for(i=1;i<7;i++) iSortedContacts[i]+=iSortedContacts[i-1];
				n1dofContacts=iSortedContacts[0]; n2dofContacts=iSortedContacts[1]; nAngContacts=iSortedContacts[2]; nRopes=iSortedContacts[3];
				nFric0Contacts=iSortedContacts[4];nFricInfContacts=iSortedContacts[5]; nContacts=iSortedContacts[6];
				for(i=0,r2=0; i<g_nContacts; i++) {
					if (g_pContacts[i]->flags & contact_constraint_1dof) iClass = 0;
					else if (g_pContacts[i]->flags & contact_constraint_2dof) iClass = 1;
					else if (g_pContacts[i]->flags & contact_constraint_3dof) iClass = 6;
					else if (g_pContacts[i]->flags & contact_solve_for) {
						if (g_pContacts[i]->flags & contact_angular) iClass = 2;
						else if (g_pContacts[i]->flags & contact_rope) iClass = 3;
						else if (g_pContacts[i]->Pspare>0) iClass = 5;
						else iClass = 4;
					} else
						continue;
					j = --iSortedContacts[iClass];

					g_ContactsCG[j].idx = i;
					body0 = g_pContacts[i]->pbody[0]; body1 = g_pContacts[i]->pbody[1];
					g_ContactsCG[j].pbody[0] = g_Bodies+g_ContactsRB[i].iBody[0]; g_ContactsCG[j].ptloc[0] = g_pContacts[i]->pt[0]-body0->pos;
					g_ContactsCG[j].pbody[1] = g_Bodies+g_ContactsRB[i].iBody[1]; g_ContactsCG[j].ptloc[1] = g_pContacts[i]->pt[1]-body1->pos;
					g_ContactsCG[j].n = g_pContacts[i]->n;
					g_ContactsCG[j].bUseC = iszero(g_pContacts[i]->flags & contact_use_C)^1;
					dp = g_ContactsC[i].Kinv*(g_ContactsCG[j].r = g_ContactsCG[j].r0 = g_ContactsRB[i].r0);
					if (g_pContacts[i]->flags & contact_use_C)
						dp = g_ContactsC[i].C*dp;
					r2 += dp*g_ContactsCG[j].r0;
					g_ContactsCG[j].dP = iClass-1u<4u ? g_pContacts[i]->n*(g_ContactsCG[j].dPn=dp.x) : dp;
					g_ContactsCG[j].P.zero();
				}
				if (!(cgiter==1 || !bStateChanged))	{
					iter = min(iter,pss->nMaxLCPCGsubiters);
					//if (iter*nContacts > pss->nMaxLCPCGmicroiters)
					//	iter = max(5,pss->nMaxLCPCGmicroiters/nContacts);
				} else {
					iter = min(iter*2,pss->nMaxLCPCGsubitersFinal);
					//if (iter*nContacts > pss->nMaxLCPCGmicroitersFinal)
					//	iter = max(10,pss->nMaxLCPCGmicroitersFinal/nContacts);
				}
				bNoImprovement = 0;
				
				if (r2>(real)1e-10 && iter>0) do {
					for(i=0;i<nBodies;i++) { g_Bodies[i].v.zero(); g_Bodies[i].w.zero(); }
					for(i=0;i<nAngContacts;i++) { // angular contacts
						g_ContactsCG[i].pbody[0]->w += g_ContactsCG[i].dP; 
						g_ContactsCG[i].pbody[1]->w -= g_ContactsCG[i].dP;
					} for(;i<nRopes;i++) { // ropes
						for(iop=0,dPn=g_ContactsCG[i].dPn; iop<2; iop++,dPn*=g_pContacts[g_ContactsCG[i].idx]->nloc.x) {
							hbody0 = g_ContactsCG[i].pbody[iop]; dP = g_ContactsC[g_ContactsCG[i].idx].C.GetRow(iop)*dPn;
							hbody0->v += dP; hbody0->w += g_ContactsCG[i].ptloc[iop]^dP;
						}	for(iop=0,prope=(rope_solver_vtx*)g_pContacts[g_ContactsCG[i].idx]->pBounceCount; iop<g_pContacts[g_ContactsCG[i].idx]->iCount; iop++) {
							g_Bodies[prope[iop].iBody].v += prope[iop].P*g_ContactsCG[i].dPn;
							g_Bodies[prope[iop].iBody].w += prope[iop].r^prope[iop].P*g_ContactsCG[i].dPn;
						}
					} for(;i<nContacts;i++) { // positional contacts
						hbody0 = g_ContactsCG[i].pbody[0]; hbody1 = g_ContactsCG[i].pbody[1]; 
						hbody0->v += g_ContactsCG[i].dP; hbody0->w += g_ContactsCG[i].ptloc[0]^g_ContactsCG[i].dP;
						hbody1->v -= g_ContactsCG[i].dP; hbody1->w -= g_ContactsCG[i].ptloc[1]^g_ContactsCG[i].dP;
					}

					for(i=0;i<nAngContacts;i++) {	// angular contacts
						hbody0 = g_ContactsCG[i].pbody[0]; hbody1 = g_ContactsCG[i].pbody[1];
						g_ContactsCG[i].vrel = hbody0->Iinv*hbody0->w - hbody1->Iinv*hbody1->w;
					} for(;i<nRopes;i++) {
						for(iop=0,g_ContactsCG[i].vrel.zero(); iop<2; iop++) {
							hbody0 = g_ContactsCG[i].pbody[iop]; 
							g_ContactsCG[i].vrel.x += g_ContactsC[g_ContactsCG[i].idx].C.GetRow(iop)*
								(hbody0->v*hbody0->Minv + (hbody0->Iinv*hbody0->w ^ g_ContactsCG[i].ptloc[iop]));
						} for(iop=0,prope=(rope_solver_vtx*)g_pContacts[g_ContactsCG[i].idx]->pBounceCount; iop<g_pContacts[g_ContactsCG[i].idx]->iCount; iop++) {
							hbody0 = g_Bodies+prope[iop].iBody;
							g_ContactsCG[i].vrel.x += prope[iop].v*(hbody0->v*hbody0->Minv + (hbody0->Iinv*hbody0->w^prope[iop].r));
						}
					} for(;i<nContacts;i++) { // positional contacts
						hbody0 = g_ContactsCG[i].pbody[0]; hbody1 = g_ContactsCG[i].pbody[1];
						g_ContactsCG[i].vrel  = hbody0->v*hbody0->Minv + (hbody0->Iinv*hbody0->w^g_ContactsCG[i].ptloc[0]);
						g_ContactsCG[i].vrel -= hbody1->v*hbody1->Minv + (hbody1->Iinv*hbody1->w^g_ContactsCG[i].ptloc[1]);
					}

					pAp = 0;
					for(i=0;i<n1dofContacts;i++) { // 1-dof contacts (remove 1 axis from vrel)
						g_ContactsCG[i].vrel -= g_ContactsCG[i].n*(g_ContactsCG[i].vrel*g_ContactsCG[i].n);
						pAp += g_ContactsCG[i].vrel*g_ContactsCG[i].dP;
					} for(;i<nFric0Contacts;i++) { // general 2-dof contacts (remove 2 axes from vrel - leave 1)
						g_ContactsCG[i].vrel.x = g_ContactsCG[i].n*g_ContactsCG[i].vrel;
						pAp += g_ContactsCG[i].vrel.x*g_ContactsCG[i].dPn;
					} for(;i<nContacts;i++)	// general 3-dof contacts (leave vrel untouched - all directions need to be 0ed)
						pAp += g_ContactsCG[i].vrel*g_ContactsCG[i].dP;

					a = min((real)50.0, r2/max((real)1E-10,fabs_tpl(pAp)));
					i = nFric0Contacts;	j = n1dofContacts + (nContacts & n1dofContacts-1>>31);
					if (nFric0Contacts-n1dofContacts < nContacts) do { // contacts with 3(and 2)-components vel
						i &= i-nContacts>>31;
						g_ContactsCG[i].vrel = g_ContactsC[g_ContactsCG[i].idx].Kinv*(g_ContactsCG[i].r -= g_ContactsCG[i].vrel*a);
						if (g_ContactsCG[i].bUseC)
							g_ContactsCG[i].vrel = g_ContactsC[g_ContactsCG[i].idx].C*g_ContactsCG[i].vrel;
						r2new += g_ContactsCG[i].vrel*g_ContactsCG[i].r;
						g_ContactsCG[i].P += g_ContactsCG[i].dP*a;
					} while(++i!=j);
					for(;i<nFric0Contacts;i++) { // contacts with 1-component vel
						g_ContactsCG[i].vrel.x = g_ContactsC[g_ContactsCG[i].idx].Kinv(0,0)*(g_ContactsCG[i].r.x -= g_ContactsCG[i].vrel.x*a);
						r2new += g_ContactsCG[i].vrel.x*g_ContactsCG[i].r.x;
						g_ContactsCG[i].P.x += g_ContactsCG[i].dPn*a;
					}
					r2new = max((real)1e-10, r2new);

					bNoImprovement = max(0,bNoImprovement-sgnnz(r2-r2new - r2*pss->minLCPCGimprovement));
					b = r2new/max(r2new*(real)0.001,r2); r2=r2new; vmax=0;
					i = nFric0Contacts; j = n1dofContacts + (nContacts & n1dofContacts-1>>31);
					if (nFric0Contacts-n1dofContacts < nContacts) do { // contacts with 3(and 2)-components vel
						i &= i-nContacts>>31;
						(g_ContactsCG[i].dP*=b) += g_ContactsCG[i].vrel;
						vmax = max(vmax,g_ContactsCG[i].r.len2());
					} while(++i!=j);
					for(;i<nFric0Contacts;i++) { // contacts with 1-component vel
						(g_ContactsCG[i].dPn*=b) += g_ContactsCG[i].vrel.x;
						g_ContactsCG[i].dP = g_ContactsCG[i].n*g_ContactsCG[i].dPn;
						vmax = max(vmax,sqr(g_ContactsCG[i].r.x));
					}

				} while (--iter && vmax>sqr(e) && bNoImprovement<pss->nMaxLCPCGFruitlessIters);
				for(i=0;i<nContacts;i++) g_pContacts[g_ContactsCG[i].idx]->vrel = g_ContactsCG[i].vrel*g_ContactsCG[i].n;

				for(i=n1dofContacts;i<nFric0Contacts;i++) g_ContactsCG[i].P = g_ContactsCG[i].n*g_ContactsCG[i].P.x;
				if (!bStateChanged)
					break;

				// calculate how the impulses affect the contacts excluded from this iteration
				for(i=0;i<nBodies;i++) { g_pBodies[i]->Fcollision.zero(); g_pBodies[i]->Tcollision.zero(); }
				for(i=0;i<nAngContacts;i++) { // angular contacts
					g_pContacts[g_ContactsCG[i].idx]->pbody[0]->Tcollision += g_ContactsCG[i].P; 
					g_pContacts[g_ContactsCG[i].idx]->pbody[1]->Tcollision -= g_ContactsCG[i].P;
				} for(;i<nRopes;i++) { // ropes
					for(iop=0,dPn=g_ContactsCG[i].P.x; iop<2; iop++,dPn*=g_pContacts[g_ContactsCG[i].idx]->nloc.x) {
						body0 = g_pContacts[g_ContactsCG[i].idx]->pbody[iop]; dP = g_ContactsC[g_ContactsCG[i].idx].C.GetRow(iop)*dPn;
						body0->Fcollision += dP; body0->Tcollision += g_ContactsCG[i].ptloc[iop]^dP;
					}	for(iop=0,prope=(rope_solver_vtx*)g_pContacts[g_ContactsCG[i].idx]->pBounceCount; iop<g_pContacts[g_ContactsCG[i].idx]->iCount; iop++) {
						prope[iop].pbody->Fcollision += prope[iop].P*g_ContactsCG[i].P.x;
						prope[iop].pbody->Tcollision += prope[iop].r^prope[iop].P*g_ContactsCG[i].P.x;
					}
				} for(;i<nContacts;i++) { // positional contacts
					body0 = g_pContacts[g_ContactsCG[i].idx]->pbody[0]; body1 = g_pContacts[g_ContactsCG[i].idx]->pbody[1]; 
					r0 = g_ContactsCG[i].ptloc[0]; r1 = g_ContactsCG[i].ptloc[1];
					body0->Fcollision += g_ContactsCG[i].P; body0->Tcollision += g_ContactsCG[i].ptloc[0]^g_ContactsCG[i].P;
					body1->Fcollision -= g_ContactsCG[i].P; body1->Tcollision -= g_ContactsCG[i].ptloc[1]^g_ContactsCG[i].P;
				}

				for(i=0;i<g_nContacts;i++) if (!(g_pContacts[i]->flags & (contact_solve_for|contact_count_mask))) {
					if (!(g_pContacts[i]->flags & contact_rope)) {
						body0 = g_pContacts[i]->pbody[0]; body1 = g_pContacts[i]->pbody[1];
						if (!(g_pContacts[i]->flags & contact_angular)) {
							r0 = g_pContacts[i]->pt[0]-body0->pos; r1 = g_pContacts[i]->pt[1]-body1->pos;
							dp  = body0->v + body0->Fcollision*body0->Minv + (body0->w+body0->Iinv*body0->Tcollision ^ r0);
							dp -= body1->v + body1->Fcollision*body1->Minv + (body1->w+body1->Iinv*body1->Tcollision ^ r1);
						} else
							dp = body0->w+body0->Iinv*body0->Tcollision - body1->w-body1->Iinv*body1->Tcollision;
						if (g_pContacts[i]->flags & contact_use_C)
							dp = g_ContactsC[i].C*dp;
					} else {
						for(iop=0,dp.zero(); iop<2; iop++) {
							body0 = g_pContacts[i]->pbody[iop]; r0 = g_pContacts[i]->pt[iop]-body0->pos;
							dp.x += g_ContactsC[i].C.GetRow(iop)*(body0->v+body0->Fcollision*body0->Minv + (body0->w+body0->Iinv*body0->Tcollision^r0));
						} for(iop=0,prope=(rope_solver_vtx*)g_pContacts[i]->pBounceCount; iop<g_pContacts[i]->iCount; iop++) {
							body0 = prope[iop].pbody;
							dp.x += prope[iop].v*(body0->v+body0->Fcollision*body0->Minv + (body0->w+body0->Iinv*body0->Tcollision^prope[iop].r));
						}
					}
					g_pContacts[i]->vrel = g_pContacts[i]->n*dp;
				}

				if (cgiter>2) for(i=n2dofContacts; i<nFricInfContacts; i++)
					g_pContacts[g_ContactsCG[i].idx]->vrel = g_pContacts[g_ContactsCG[i].idx]->n*(-g_ContactsCG[i].P*
							max(g_ContactsCG[i].pbody[0]->Minv,g_ContactsCG[i].pbody[1]->Minv) - g_ContactsCG[i].n*(e*3));
				else for(i=n2dofContacts; i<nFricInfContacts; i++) // if we reached the last iteration, keep the current 'solve for' contacts unconditionally
					g_pContacts[g_ContactsCG[i].idx]->vrel = -1.0f;
			} while(--cgiter>0);


			bBounced = 1;
			if (cgiter<pss->nMaxLCPCGiters) {
				for(i=0,vmax=0;i<nAngContacts;i++) {
					vmax = max(vmax,(g_pContacts[g_ContactsCG[i].idx]->pbody[0]->Ibody*g_ContactsCG[i].P).len2());
					vmax = max(vmax,(g_pContacts[g_ContactsCG[i].idx]->pbody[1]->Ibody*g_ContactsCG[i].P).len2());
				}	if (_isnan(vmax) || vmax>sqr(pss->maxwCG))
					bBounced = 0;
				for(vmax=0;i<nContacts;i++) {
					vmax = max(vmax,sqr(g_ContactsCG[i].pbody[0]->Minv)*g_ContactsCG[i].P.len2());
					vmax = max(vmax,sqr(g_ContactsCG[i].pbody[1]->Minv)*g_ContactsCG[i].P.len2());
				}	if (_isnan(vmax) || vmax>sqr(pss->maxvCG))
					bBounced = 0;

				if (bBounced) {
					// apply P from the last CG iteration
					for(i=0;i<nAngContacts;i++) { // angular contacts
						g_pContacts[g_ContactsCG[i].idx]->pbody[0]->L += g_ContactsCG[i].P; 
						g_pContacts[g_ContactsCG[i].idx]->pbody[1]->L -= g_ContactsCG[i].P;
					} for(;i<nRopes;i++) { // ropes
						for(iop=0,dPn=g_ContactsCG[i].P.x; iop<2; iop++,dPn*=g_pContacts[g_ContactsCG[i].idx]->nloc.x) {
							body0 = g_pContacts[g_ContactsCG[i].idx]->pbody[iop]; dP = g_ContactsC[g_ContactsCG[i].idx].C.GetRow(iop)*dPn;
							body0->P += dP; body0->L += g_ContactsCG[i].ptloc[iop]^dP;
						}	for(iop=0,prope=(rope_solver_vtx*)g_pContacts[g_ContactsCG[i].idx]->pBounceCount; iop<g_pContacts[g_ContactsCG[i].idx]->iCount; iop++) {
							prope[iop].pbody->P += prope[iop].P*g_ContactsCG[i].P.x;
							prope[iop].pbody->L += prope[iop].r^prope[iop].P*g_ContactsCG[i].P.x;
						}
					} for(;i<nContacts;i++) { // positional contacts
						body0 = g_pContacts[g_ContactsCG[i].idx]->pbody[0]; body1 = g_pContacts[g_ContactsCG[i].idx]->pbody[1]; 
						body0->P += g_ContactsCG[i].P; body0->L += g_ContactsCG[i].ptloc[0]^g_ContactsCG[i].P;
						body1->P -= g_ContactsCG[i].P; body1->L -= g_ContactsCG[i].ptloc[1]^g_ContactsCG[i].P;
					}
					for(i=0;i<nBodies;i++) if (g_pBodies[i]->M>0) {
						g_pBodies[i]->v = g_pBodies[i]->P*g_pBodies[i]->Minv; g_pBodies[i]->w = g_pBodies[i]->Iinv*g_pBodies[i]->L;
					}
				} else for(i=0; i<nBodies; i++) {
					g_pBodies[i]->Fcollision = g_infos[i].Fcollision;
					g_pBodies[i]->Tcollision = g_infos[i].Tcollision;
				}
			}
			for(i=0;i<g_nContacts;i++) if (g_pContacts[i]->flags & contact_rope) {
				g_pContacts[i]->n = g_ContactsC[i].C.GetRow(0);
				g_pContacts[i]->vreq = g_ContactsC[i].C.GetRow(1);
			}

			}//"LCPCG"

			if (bBounced) {
				CRY_PROFILE_REGION(PROFILE_PHYSICS, "LCPCG-unproj");

				///////////////////////////////////////////////////////////////////////////////////
				// now, use a separate solver for unprojections (unproject each body independently)
				entity_contact *pContacts[CRY_ARRAY_COUNT(g_pContacts)];
				int nSandwiches,iMinLevel,iMaxLevel,iLevel;
				float minMinv;

				for(i=0;i<nBodies;i++) {
					g_pBodies[i]->bProcessed[iCaller]=i; g_infos[i].pbuddy=0; g_infos[i].iLevel=-1; 
					g_infos[i].psandwich=0; g_infos[i].pfollower=0; g_infos[i].idUpdate=0;
				}
				for(i=0;i<g_nContacts;i++)
					g_pContacts[i]->flags &= ~contact_solve_for | ~(-(g_pContacts[i]->flags & contact_rope)>>31);
				// require that the all contacts are grouped by pbody[0]
				for(istart=nBuddies=0; istart<g_nContacts; istart=iend) {
					// sub-group contacts relating to each pbody[0] by pbody[1] (using quick sort)
					for(iend=istart+1; iend<g_nContacts && g_pContacts[iend]->pbody[0]==g_pContacts[istart]->pbody[0]; iend++);
					qsort(g_pContacts, istart,iend-1, iCaller);
					// for each body: find all its contacting bodies and for each such body get integral vreq (but ignore separating contacts)
					for(i=istart; i<iend; i=j) {
						for(j=i,vreq.zero(),flags=0; j<iend && g_pContacts[j]->pbody[1]==g_pContacts[i]->pbody[1]; j++)
							if (g_pContacts[j]->flags&contact_solve_for && g_pContacts[j]->vreq.len2()>0) {
								vreq += g_pContacts[j]->vreq; flags |= g_pContacts[j]->flags;
							}
						if (flags) { 
							g_buddybuf[nBuddies].next = g_infos[bidx0(i)].pbuddy;
							g_buddybuf[nBuddies].iBody = bidx1(i);
							g_buddybuf[nBuddies].vreq = vreq; g_buddybuf[nBuddies].flags = flags;
							g_infos[bidx0(i)].pbuddy = g_buddybuf+nBuddies++;
							g_buddybuf[nBuddies].next = g_infos[bidx1(i)].pbuddy;
							g_buddybuf[nBuddies].iBody = bidx0(i);
							g_buddybuf[nBuddies].vreq = -vreq; g_buddybuf[nBuddies].flags = flags;
							g_infos[bidx1(i)].pbuddy = g_buddybuf+nBuddies++;
						}
					}
				}

				// for every 2 contacting bodies of each body check if vreqs are conflicting, register sandwich triplet if so (1 inside, 2 outside)
				for(i=nSandwiches=0; i<nBodies; i++)
					for(pbuddy0=g_infos[i].pbuddy; pbuddy0; pbuddy0=pbuddy0->next)
						for(pbuddy1=pbuddy0->next; pbuddy1; pbuddy1=pbuddy1->next)
							if (pbuddy0->vreq*pbuddy1->vreq<0 && !((pbuddy0->flags^pbuddy1->flags)&contact_angular)) {
								g_sandwichbuf[nSandwiches].iMiddle = i;
								g_sandwichbuf[nSandwiches].iBread[0] = pbuddy0->iBody;
								g_sandwichbuf[nSandwiches].iBread[1] = pbuddy1->iBody;
								g_sandwichbuf[nSandwiches].next = g_infos[i].psandwich;
								g_sandwichbuf[nSandwiches].bProcessed = 0;
								g_infos[i].psandwich = g_sandwichbuf+nSandwiches++;
								if (nSandwiches==CRY_ARRAY_COUNT(g_sandwichbuf))
									goto endsandw;
							}
				endsandw:
				// call tracepath for each sandwich with static	as 'bread' (if no static - assign a 'pseudo-static')
				for(i=0;i<nBodies;i++) g_infos[i].Minv = g_pBodies[i]->Minv;
				// find the heaviest body participating as bread in any sandwich
				for(i=j=0,minMinv=1E10f; i<nSandwiches; i++) {
					if (g_infos[g_sandwichbuf[i].iBread[0]].Minv<minMinv) minMinv = g_infos[j=g_sandwichbuf[i].iBread[0]].Minv;
					if (g_infos[g_sandwichbuf[i].iBread[1]].Minv<minMinv) minMinv = g_infos[j=g_sandwichbuf[i].iBread[1]].Minv;
				}
				if (minMinv>0) { // if no static participates in sandwich, assign bodies contacting with statics Minv 0 and repeat the procedure
					for(i=0; i<g_nContacts; i++) if (g_pContacts[i]->pbody[0]->Minv*g_pContacts[i]->pbody[1]->Minv==0)
						g_infos[g_pContacts[i]->pbody[g_pContacts[i]->pbody[0]->Minv==0]->bProcessed[iCaller]].Minv = 0;
					for(i=0,minMinv=1E10f; i<nSandwiches; i++) {
						if (g_infos[g_sandwichbuf[i].iBread[0]].Minv<minMinv) minMinv = g_infos[j=g_sandwichbuf[i].iBread[0]].Minv;
						if (g_infos[g_sandwichbuf[i].iBread[1]].Minv<minMinv) minMinv = g_infos[j=g_sandwichbuf[i].iBread[1]].Minv;
					}
				}
				for(i=g_nFollowers=g_nUnprojLoops=0; i<nSandwiches; i++) {
					if (j==g_sandwichbuf[i].iBread[0] || g_infos[g_sandwichbuf[i].iBread[0]].Minv==0) iop=0;
					else if (j==g_sandwichbuf[i].iBread[1] || g_infos[g_sandwichbuf[i].iBread[1]].Minv==0) iop=1;
					else continue;
					g_infos[g_sandwichbuf[i].iBread[iop]].iLevel = 0;
					g_infos[g_sandwichbuf[i].iMiddle].iLevel = max(1,g_infos[g_sandwichbuf[i].iMiddle].iLevel);
					trace_unproj_route(g_sandwichbuf[i].iMiddle, g_sandwichbuf[i].iBread[iop], iCaller);
				}

				// option: since after this moment we are starting to get 'heuristic', maybe we should randomize the order of sandwiches?
				iter = 0;
				do {
					// iteratively select an unprocessed sandwich with the min level body (bread or middle), call trace_unproj_route for it; 
					// when the route reaches initialized branches, they will automatically update all previously traced followers
					do {
						for(i=0,j=-1,iMinLevel=0x10000; i<nSandwiches; i++) if (!g_sandwichbuf[i].bProcessed) {
							iLevel = min(g_infos[g_sandwichbuf[i].iMiddle].iLevel&0xFFFF,
								min(g_infos[g_sandwichbuf[i].iBread[0]].iLevel&0xFFFF,g_infos[g_sandwichbuf[i].iBread[1]].iLevel&0xFFFF));
							iop = iLevel-iMinLevel>>31;	j = i&iop | j&~iop;
							iMinLevel = min(iMinLevel,iLevel);
						}
						if (j<0) break;
						g_sandwichbuf[j].bProcessed = 1;
						i = (g_infos[g_sandwichbuf[j].iBread[0]].iLevel>>31&1 | g_infos[g_sandwichbuf[j].iBread[1]].iLevel>>31&2 |
							g_infos[g_sandwichbuf[j].iMiddle].iLevel>>31&4) ^ 7;
						if (i==7) {	// all participating bodies already have levels
							iop = isneg(g_infos[g_sandwichbuf[j].iBread[1]].iLevel-g_infos[g_sandwichbuf[j].iBread[0]].iLevel);	// min level bread
							if (g_infos[g_sandwichbuf[j].iMiddle].iLevel >= g_infos[g_sandwichbuf[j].iBread[iop]].iLevel)
								add_route_follower(g_sandwichbuf[j].iBread[iop], g_sandwichbuf[j].iMiddle, iCaller);	// breadmin -> middle -> breadmax(will be updated)
						} else if (i==3) { // both breads have levels
							g_infos[g_sandwichbuf[j].iMiddle].iLevel = 1;
							if (iMinLevel>1) { // bread0 <- middle=1 -> bread1
								add_route_follower(g_sandwichbuf[j].iMiddle, g_sandwichbuf[j].iBread[0], iCaller);
								add_route_follower(g_sandwichbuf[j].iMiddle, g_sandwichbuf[j].iBread[1], iCaller);
							} else { // breadmin -> middle=1 -> breadmax
								iop = isneg(g_infos[g_sandwichbuf[j].iBread[1]].iLevel-g_infos[g_sandwichbuf[j].iBread[0]].iLevel);	// min level bread
								add_route_follower(g_sandwichbuf[j].iBread[iop], g_sandwichbuf[j].iMiddle, iCaller);
							}
						} else if (i==4) { // only the middle has level
							g_infos[g_sandwichbuf[j].iBread[0]].iLevel = g_infos[g_sandwichbuf[j].iBread[1]].iLevel = 
								g_infos[g_sandwichbuf[j].iMiddle].iLevel+1;
							add_route_follower(g_sandwichbuf[j].iMiddle, g_sandwichbuf[j].iBread[0], iCaller);
							add_route_follower(g_sandwichbuf[j].iMiddle, g_sandwichbuf[j].iBread[1], iCaller);
						} else if (i!=0) { // either one bread or one bread and the middle have levels
							iop = i>>1&1; // the bread that has level
							g_infos[g_sandwichbuf[j].iMiddle].iLevel = min(g_infos[g_sandwichbuf[j].iMiddle].iLevel&0xFFFF, iMinLevel+1);
							if (i&2) // the middle had level
								add_route_follower(g_sandwichbuf[j].iMiddle, g_sandwichbuf[j].iBread[iop^1], iCaller);
							else
								add_route_follower(g_sandwichbuf[j].iBread[iop], g_sandwichbuf[j].iMiddle, iCaller);
						}
					} while(true);
					if (iter>0) break;

					// now we have only fully clear sandwiches in isolated branches; as we progress, however, some unprocessed sandwiches might
					// become partially initialized; ignore them - the previous loop will pick them up during the next iteration
					for(i=0;i<nSandwiches;i++) 
					if ((g_infos[g_sandwichbuf[i].iMiddle].iLevel & g_infos[g_sandwichbuf[i].iBread[0]].iLevel & g_infos[g_sandwichbuf[i].iBread[1]].iLevel)==-1) {
						g_sandwichbuf[i].bProcessed = 1;
						g_infos[g_sandwichbuf[i].iMiddle].iLevel = 0;
						g_infos[g_sandwichbuf[i].iBread[0]].iLevel = g_infos[g_sandwichbuf[i].iBread[1]].iLevel = 1;
						add_route_follower(g_sandwichbuf[i].iMiddle, g_sandwichbuf[i].iBread[0], iCaller);
						add_route_follower(g_sandwichbuf[i].iMiddle, g_sandwichbuf[i].iBread[1], iCaller);
					}
					++iter;
				} while(true);

				// set level to maxlevel+1 for all bodies that still don't have a level assigned
				// (they either didn't form sandwiches, or belonged to sandwiches that had only middles initialized, and didn't belong to routes
				for(i=iMaxLevel=0; i<nBodies; i++) iMaxLevel = max(iMaxLevel,g_infos[i].iLevel);
				for(i=0,iMaxLevel++; i<nBodies; i++) {
					g_infos[i].iLevel = iMaxLevel&g_infos[i].iLevel>>31 | max(g_infos[i].iLevel,0);
					if (g_pBodies[i]->Minv==0)
						g_infos[i].iLevel = 0; // force level 0 to statics (some can accidentally get non-0 level if they don't participate in sandwiches)
				}

	#ifdef _DEBUG
	for(i=j=0;i<nSandwiches;i++) 
	if (g_infos[g_sandwichbuf[i].iMiddle].iLevel>=max(g_infos[g_sandwichbuf[i].iBread[0]].iLevel,g_infos[g_sandwichbuf[i].iBread[1]].iLevel))
		j++;
	#endif

				// sort body list according to level
				qsort(g_pBodies,0,nBodies-1,iCaller);
				for(i=0;i<nBodies;i++) {
					g_infos[g_pBodies[i]->bProcessed[iCaller]].idx = i;
					g_infos[g_pBodies[i]->bProcessed[iCaller]].v_unproj.zero(); g_infos[g_pBodies[i]->bProcessed[iCaller]].w_unproj.zero();  
				}
				// sort contact list so that each contact gets assigned to the body with the higher level (among the two participating in the contact)
				union {entity_contact** pe; entity_contact_unproj **pu;} u; u.pe = g_pContacts;
				qsort(u.pu,0,g_nContacts-1,iCaller);

				for(istart=0; istart<g_nContacts; istart=j) {
					// among contacts assigned to a body, select only non-separating with non-zero vreq and use cg to enforce vreq (along its direction only)
					// don't enforce contacts with 0 vreq, since if we violate them, they'll be reported to have vreq during the next step
					body0 = g_pContacts[istart]->pbody[isneg(g_infos[bidx0(istart)].iLevel-g_infos[bidx1(istart)].iLevel)];
					iSortedContacts[0] = iSortedContacts[1] = 0;
					for(j=istart; j<g_nContacts && g_pContacts[j]->pbody[iop = isneg(g_infos[bidx0(j)].iLevel-g_infos[bidx1(j)].iLevel)]==body0; j++)
					if (g_pContacts[j]->flags & contact_solve_for && g_pContacts[j]->vreq.len2()>sqr(0.01f)) {
						g_pContacts[j]->flags = g_pContacts[j]->flags&~contact_bidx | iop<<contact_bidx_log2;
						g_pContacts[j]->bProcessed = j;
						g_ContactsCG[j].vrel = g_pContacts[j]->vreq*(iop*2-1);
						// from now on use dP to store normalized unprojection direction
						if (fabsf(sqr(g_pContacts[j]->vreq*g_pContacts[j]->n)-g_pContacts[j]->vreq.len2()) < g_pContacts[j]->vreq.len2()*0.01f)
							g_ContactsCG[j].dP = g_pContacts[j]->n;
						else 
							g_ContactsCG[j].dP = g_pContacts[j]->vreq.normalized();
						body1 = g_pContacts[j]->pbody[iop^1];
						if (!(g_pContacts[j]->flags & contact_angular)) {
							r0 = g_pContacts[j]->pt[iop]-body0->pos; r1 = g_pContacts[j]->pt[iop^1]-body1->pos;
							g_ContactsC[j].Kinv(0,0) = body0->Minv+g_ContactsCG[j].dP*(body0->Iinv*(r0^g_ContactsCG[j].dP)^r0);
							if (g_ContactsC[j].Kinv(0,0) < body0->Minv*0.1f) { // contact has near-degenerate Kinv, skip it
								g_pContacts[j]->flags &= ~contact_solve_for; continue;
							}
							g_ContactsCG[j].vrel += body0->v+g_infos[body0->bProcessed[iCaller]].v_unproj+(body0->w+g_infos[body0->bProcessed[iCaller]].w_unproj^r0);
							g_ContactsCG[j].vrel -= body1->v+g_infos[body1->bProcessed[iCaller]].v_unproj+(body1->w+g_infos[body1->bProcessed[iCaller]].w_unproj^r1);
							iSortedContacts[1]++;
						}	else {
							g_ContactsC[j].Kinv(0,0) = g_ContactsCG[j].dP*(body0->Iinv*g_ContactsCG[j].dP);
							if (sqr_signed(g_ContactsC[j].Kinv(0,0)) <= (body0->Ibody_inv*Vec3(1)).len2()*sqr(0.05f)) { // contact has near-degenerate Kinv, skip it
								g_pContacts[j]->flags &= ~contact_solve_for; continue;
							}
							g_ContactsCG[j].vrel += body0->w+g_infos[body0->bProcessed[iCaller]].w_unproj - body1->w-g_infos[body1->bProcessed[iCaller]].w_unproj;
							iSortedContacts[0]++;
						}
					}	else
						g_pContacts[j]->flags &= ~contact_solve_for;
					nContacts=iSortedContacts[0]+iSortedContacts[1]; nAngContacts=iSortedContacts[1]=iSortedContacts[0]; iSortedContacts[0]=0;
					if (body0->Minv==0)
						continue;

					for(j=istart; j<g_nContacts && g_pContacts[j]->pbody[iop = isneg(g_infos[bidx0(j)].iLevel-g_infos[bidx1(j)].iLevel)]==body0; j++)
					if (g_pContacts[j]->flags & contact_solve_for) {
						g_ContactsC[j].Kinv(0,0) = 1.0f/g_ContactsC[j].Kinv(0,0);
						g_ContactsCG[j].r0.x = g_ContactsCG[j].r.x = -(g_ContactsCG[j].vrel*g_ContactsCG[j].dP);
						g_ContactsCG[j].dPn = g_ContactsCG[j].r.x*g_ContactsC[j].Kinv(0,0);
						g_ContactsCG[j].P.x = 0;
						pContacts[iSortedContacts[g_pContacts[j]->flags>>contact_angular_log2&1^1]++] = g_pContacts[j];
					}
					r2 = ComputeRc(body0,pContacts,nAngContacts,nContacts, iCaller);

					// use MINRES to enforce vreq
					iter = nContacts;
					if (iter=nContacts) {
						if (r2>sqr(pss->minSeparationSpeed)) do {
							body0->Fcollision.zero(); body0->Tcollision.zero();
							for(i=0;i<nAngContacts;i++) // angular contacts
								body0->Tcollision += g_ContactsCG[pContacts[i]->bProcessed].dP*g_ContactsCG[pContacts[i]->bProcessed].dPn; 
							for(;i<nContacts;i++) { // positional contacts
								r0 = pContacts[i]->pt[pContacts[i]->flags>>contact_bidx_log2&1]-body0->pos;
								body0->Fcollision += g_ContactsCG[pContacts[i]->bProcessed].dP*g_ContactsCG[pContacts[i]->bProcessed].dPn; 
								body0->Tcollision += r0^g_ContactsCG[pContacts[i]->bProcessed].dP*g_ContactsCG[pContacts[i]->bProcessed].dPn;
							}
							for(i=0;i<nAngContacts;i++)	// angular contacts
								g_ContactsCG[pContacts[i]->bProcessed].vrel = body0->Iinv*body0->Tcollision;
							for(;i<nContacts;i++) { // positional contacts
								r0 = pContacts[i]->pt[pContacts[i]->flags>>contact_bidx_log2&1]-body0->pos;
								g_ContactsCG[pContacts[i]->bProcessed].vrel = body0->Fcollision*body0->Minv + (body0->Iinv*body0->Tcollision^r0);
							}
							for(i=0,pAp=0;i<nContacts;i++) { 
								g_ContactsCG[pContacts[i]->bProcessed].vrel.x = g_ContactsCG[pContacts[i]->bProcessed].dP*g_ContactsCG[pContacts[i]->bProcessed].vrel;
								pAp += sqr(g_ContactsCG[pContacts[i]->bProcessed].vrel.x)*g_ContactsC[pContacts[i]->bProcessed].Kinv(0,0);
							}
							a = min((real)20.0,r2/max((real)1E-10,pAp));
							for(i=0;i<nContacts;i++) {
								g_ContactsCG[pContacts[i]->bProcessed].r.x -= g_ContactsCG[pContacts[i]->bProcessed].vrel.x*a;
								g_ContactsCG[pContacts[i]->bProcessed].P.x += g_ContactsCG[pContacts[i]->bProcessed].dPn*a;
							}
							r2new = ComputeRc(body0,pContacts,nAngContacts,nContacts, iCaller);
							b = r2new/r2; r2=r2new;
							for(i=0;i<nContacts;i++)
								(g_ContactsCG[pContacts[i]->bProcessed].dPn*=b) += 
								g_ContactsCG[pContacts[i]->bProcessed].r.x*g_ContactsC[pContacts[i]->bProcessed].Kinv(0,0);
						} while(--iter && r2>sqr(pss->minSeparationSpeed));

						// store velocities in v_unproj,w_unproj
						body0->Fcollision.zero(); body0->Tcollision.zero();
						for(i=0;i<nAngContacts;i++) // angular contacts
							body0->Tcollision += g_ContactsCG[pContacts[i]->bProcessed].dP*g_ContactsCG[pContacts[i]->bProcessed].P.x; 
						for(;i<nContacts;i++) { // positional contacts
							r0 = pContacts[i]->pt[pContacts[i]->flags>>contact_bidx_log2&1]-body0->pos;
							body0->Fcollision += g_ContactsCG[pContacts[i]->bProcessed].dP*g_ContactsCG[pContacts[i]->bProcessed].P.x; 
							body0->Tcollision += r0^g_ContactsCG[pContacts[i]->bProcessed].dP*g_ContactsCG[pContacts[i]->bProcessed].P.x;
						}
						g_infos[body0->bProcessed[iCaller]].w_unproj = body0->Iinv*body0->Tcollision;
						g_infos[body0->bProcessed[iCaller]].v_unproj = body0->Fcollision*body0->Minv;

						float r2max,v2max,v2unproj;
						vmax = 2.0f;
						if (nAngContacts==nContacts)
							r2max = 1;
						else for(i=nAngContacts,r2max=0;i<nContacts;i++) {
							r2max = max(r2max,(pContacts[i]->pt[pContacts[i]->flags>>contact_bidx_log2&1]-body0->pos).len2());
							vmax = 1.0f;
						}
						for(i=0,v2max=0;i<nAngContacts;i++)
							v2max = max(v2max,sqr(g_ContactsCG[pContacts[i]->bProcessed].r0.x)*r2max);
						for(;i<nContacts;i++)
							v2max = max(v2max,sqr(g_ContactsCG[pContacts[i]->bProcessed].r0.x));
						v2max = min(v2max,sqr(pss->maxvUnproj));
						v2unproj = max(g_infos[body0->bProcessed[iCaller]].v_unproj.len2(), g_infos[body0->bProcessed[iCaller]].w_unproj.len2()*r2max);
						if (v2unproj > v2max*vmax) {
							v2unproj = sqrt_tpl(v2max/v2unproj);
							g_infos[body0->bProcessed[iCaller]].v_unproj *= v2unproj;
							g_infos[body0->bProcessed[iCaller]].w_unproj *= v2unproj;
						}
					}
				}

				// update bodies' positions with v_unproj*dt,w_unproj*dt
				for(i=0; i<nBodies; i++) {
					j = g_pBodies[i]->bProcessed[iCaller];
					g_pBodies[i]->bProcessed[iCaller] = 0;
					Vec3 L = g_pBodies[i]->q*(g_pBodies[i]->Ibody*(g_infos[j].w_unproj*g_pBodies[i]->q));
					g_pBodies[i]->Eunproj = (g_infos[j].v_unproj.len2()+(g_infos[j].w_unproj*L)*g_pBodies[i]->Minv)*0.5f;
					if (g_pBodies[i]->Eunproj>0) {
						if (!pss->bCGUnprojVel) {
							g_pBodies[i]->pos += g_infos[j].v_unproj*time_interval;
							if (g_infos[j].w_unproj.len2()*sqr(time_interval)<sqr(0.003f))
								g_pBodies[i]->q += quaternionf(0,g_infos[j].w_unproj*0.5f)*g_pBodies[i]->q*time_interval;
							else {
								float wlen = g_infos[j].w_unproj.len();
								//q = quaternionf(wlen*dt,w/wlen)*q;
								g_pBodies[i]->q = Quat::CreateRotationAA(wlen*time_interval,g_infos[j].w_unproj/wlen)*g_pBodies[i]->q;
							}
							g_pBodies[i]->q.Normalize();
							// don't unpdate Iinv and w, no1 will appreciate it after this point
						} else {
							g_pBodies[i]->v += g_infos[j].v_unproj;	g_infos[j].v_unproj.zero();
							g_pBodies[i]->w += g_infos[j].w_unproj;	g_infos[j].w_unproj.zero();
							g_pBodies[i]->P = g_pBodies[i]->v*g_pBodies[i]->M; 
							g_pBodies[i]->L = g_pBodies[i]->q*(g_pBodies[i]->Ibody*(!g_pBodies[i]->q*g_pBodies[i]->w));
						}
					}
					g_pBodies[i]->Fcollision = g_infos[j].Fcollision;
					g_pBodies[i]->Tcollision = g_infos[j].Tcollision;
				}
			}//"LCPCG-unproj"
		}
	}


	for(i=0;i<nBodies;i++) {
		g_pBodies[i]->Fcollision = (g_pBodies[i]->P-g_pBodies[i]->Fcollision)*rtime_interval; 
		g_pBodies[i]->Tcollision = (g_pBodies[i]->L-g_pBodies[i]->Tcollision)*rtime_interval;
	}
	for(i=0; i<g_nContacts; i++)
		g_pContacts[i]->Pspare = g_ContactsRB[i].Pn;
	return g_nBodies;
}

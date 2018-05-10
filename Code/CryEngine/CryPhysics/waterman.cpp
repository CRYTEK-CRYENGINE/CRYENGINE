// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "utils.h"
#include <CryPhysics/primitives.h>
#include "bvtree.h"
#include "aabbtree.h"
#include "obbtree.h"
#include "singleboxtree.h"
#include "geometry.h"
#include "trimesh.h"
#include "heightfieldbv.h"
#include "heightfieldgeom.h"
#include "waterman.h"
#include "rigidbody.h"
#include "physicalplaceholder.h"
#include "physicalentity.h"
#include "geoman.h"
#include "physicalworld.h"


CWaterMan::CWaterMan(CPhysicalWorld *pWorld, IPhysicalEntity *pArea)
{
	m_pWorld = pWorld; m_pArea = pArea;
	m_next = m_prev = 0;
	m_nTiles=-1; m_nCells=32; m_pTiles=0; m_pTilesTmp=0; m_pCellMask=0;	m_pCellNorm=0;
	m_dt = 0.02f; m_timeSurplus=0; 
	m_rtileSz=1.0f/(m_tileSz=10.0f); m_cellSz=m_tileSz/m_nCells;
	m_waveSpeed=100.f; m_minhSpread=0.05f; m_minVel=0.01f; m_dampingCenter=0.2f; m_dampingRim=1.5f;
	m_origin.zero(); m_waterOrigin.Set(1E10f,1E10f,1E10f);
	m_ix=m_iy=-10000; m_R.SetIdentity(); m_pCellMask=0;
	m_posViewer.zero(); m_bActive=0;
	m_pCellQueue = new int[m_szCellQueue=64];
	m_doffs=0; m_depth=8; m_kResistance=1; m_hlimit=7;
	m_lockUpdate = 0;
}

CWaterMan::~CWaterMan()
{
	if (m_pTiles) {
		for(int i=0;i<=sqr(m_nTiles*2+1);i++) delete m_pTiles[i]; delete[] m_pTiles; 
		delete[] m_pTilesTmp; delete[] m_pCellMask; delete[] m_pCellNorm;
	}
	if (m_pCellQueue) delete[] m_pCellQueue;
	if (m_prev) m_prev->m_next = m_next;
	if (m_next) m_next->m_prev = m_prev;
}


int CWaterMan::SetParams(pe_params *_params)
{
	if (_params->type==pe_params_waterman::type_id) {
		pe_params_waterman *params = (pe_params_waterman*)_params;
		int i,bRealloc=0;
		if (!is_unused(params->nExtraTiles)) { m_nTiles=params->nExtraTiles; bRealloc=1; }
		if (!is_unused(params->nCells)) { m_nCells=params->nCells; m_rcellSz=1.0f/(m_cellSz=m_tileSz/m_nCells); bRealloc=1; }
		if (!is_unused(params->tileSize)) { m_rtileSz=1.0f/(m_tileSz=params->tileSize);	m_rcellSz=1.0f/(m_cellSz=m_tileSz/m_nCells); }
		if (bRealloc) {
			if (m_pTiles) {
				for(i=0;i<sqr(m_nTiles*2+1);i++) delete m_pTiles[i];
				delete[] m_pTiles; delete[] m_pTilesTmp; delete[] m_pCellMask; delete[] m_pCellNorm;
				m_pTiles=0; m_pTilesTmp=0; m_pCellMask=0;
			}
			m_pTiles = new SWaterTile*[sqr(m_nTiles*2+1)+1]; 
			m_pTilesTmp = new SWaterTile*[sqr(m_nTiles*2+1)+1]; 
			m_pCellMask = new char[i=sqr((m_nTiles*2+1)*m_nCells+2)];
			memset(m_pCellNorm=new Vec2[i], 0, i*sizeof(m_pCellNorm[0]));
			for(i=0;i<=sqr(m_nTiles*2+1);i++)
				(m_pTiles[i] = new SWaterTile(m_nCells))->Activate()->Activate(0);
		}
		if (!is_unused(params->timeStep)) m_dt = params->timeStep;
		if (!is_unused(params->waveSpeed)) m_waveSpeed = params->waveSpeed;
		if (!is_unused(params->dampingCenter)) m_dampingCenter = params->dampingCenter;
		if (!is_unused(params->dampingRim)) m_dampingRim = params->dampingRim;
		if (!is_unused(params->minhSpread)) m_minhSpread = params->minhSpread;
		if (!is_unused(params->minVel)) m_minVel = params->minVel;
		if (!is_unused(params->simDepth)) m_depth = params->simDepth;
		if (!is_unused(params->heightLimit)) m_hlimit = params->heightLimit;
		if (!is_unused(params->resistance)) m_kResistance = params->resistance;
		if (!is_unused(params->posViewer)) SetViewerPos(params->posViewer,1);
		return 1;
	}
	return 0;
}

int CWaterMan::GetParams(pe_params *_params)
{
	if (_params->type==pe_params_waterman::type_id) {
		pe_params_waterman *params = (pe_params_waterman*)_params;
		params->posViewer = m_posViewer;
		params->nExtraTiles = m_nTiles;
		params->nCells = m_nCells;
		params->tileSize = m_tileSz;
		params->timeStep = m_dt;
		params->waveSpeed = m_waveSpeed;
		params->dampingCenter = m_dampingCenter;
		params->dampingRim = m_dampingRim;
		params->minhSpread = m_minhSpread;
		params->minVel = m_minVel;
		params->simDepth = m_depth;
		params->heightLimit = m_hlimit;
		params->resistance = m_kResistance;
		return 1;
	}
	return 0;
}

int CWaterMan::GetStatus(pe_status *_status)
{
	if (_status->type==pe_status_waterman::type_id) {
		pe_status_waterman *status = (pe_status_waterman*)_status;
		status->bActive = m_bActive;
		status->nTiles = m_nTiles*2+1;
		status->nCells = m_nCells;
		status->R = m_R;
		status->origin = m_origin;
		status->pTiles = (SWaterTileBase**)m_pTiles;
		return 1;
	}
	return 0;
}


void CWaterMan::OnWaterInteraction(CPhysicalEntity *pent)
{
	if (m_nTiles<0)
		return;
	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );
	PHYS_AREA_PROFILER((CPhysArea*)m_pArea)

	WriteLock lock(m_lockUpdate);
	int i,j,ipart,ipartMax,icont,npt,icell,ncont,nMeshes,nPrims;
	index_t idx[] = { 0,1,2, 0,3,1, 0,2,3, 1,3,2 };
	trinfo top[4] = { {{1,3,2}}, {{2,3,0}}, {{0,3,1}}, {{1,2,0}} };
	Vec3 vtx[4],normals[4],wloc,org,sz,pt,vloc,dir;
	Vec2i ibbox[2],ibbox1[2],ic,itile,strideTiles(1,m_nTiles*2+1);
	float w,h,depth;
	char *pcell;
	CTriMesh mesh;
	CSingleBoxTree sbtree;
	geom_world_data gwd;
	intersection_params ip;
	geom_contact *pcontacts;
	ptitem2d pts[256],*pvtx,*plist;
	edgeitem pcontour[256],*pedge; 
	grid wgrid;
	SWaterTile *ptile;
	jgrid_checker jgc;
	Matrix33 Rabs;
	RigidBody body,*pbody;
	box bbox;

	// project entity's bbox on the water grid, intersect with the viewer's vicinity rect
	org = ((pent->m_BBox[0]+pent->m_BBox[1])*0.5f-m_origin)*m_R;
	sz = (Rabs=m_R).Fabs()*((pent->m_BBox[1]-pent->m_BBox[0])*0.5f);
	ibbox[0].x = max(-m_nTiles, float2int((org.x-sz.x)*m_rtileSz));
	ibbox[0].y = max(-m_nTiles, float2int((org.y-sz.y)*m_rtileSz));
	ibbox[1].x = min( m_nTiles, float2int((org.x+sz.x)*m_rtileSz));
	ibbox[1].y = min( m_nTiles, float2int((org.y+sz.y)*m_rtileSz));
	if ((ibbox[1].x-ibbox[0].x|ibbox[1].y-ibbox[0].y)<0)
		return;
	wgrid.size.set((ibbox[1].x-ibbox[0].x+1)*m_nCells,(ibbox[1].y-ibbox[0].y+1)*m_nCells);
	wgrid.step.set(m_cellSz,m_cellSz);
	wgrid.stepr.set(m_rcellSz,m_rcellSz);
	wgrid.stride.set(1,wgrid.size.x+1);
	wgrid.origin.zero();
	jgc.pgrid=&wgrid; jgc.ppt=0; jgc.pCellMask=m_pCellMask+wgrid.size.x+2; jgc.pnorms=m_pCellNorm;
	jgc.bMarkCenters = 1;

	// build a tetrahedron around the intersection rect
	w = (ibbox[1].x-ibbox[0].x+1)*m_tileSz;
	h = (ibbox[1].y-ibbox[0].y+1)*m_tileSz;
	vtx[0].x=-(vtx[1].x=w*1.01f); vtx[0].y=vtx[1].y=h*-0.51f; 
	vtx[2].y=h*1.51f; vtx[3].z=(w+h)*-0.5f;
	vtx[0].z=vtx[1].z=vtx[2].z=vtx[2].x=vtx[3].x=vtx[3].y=0;

	mesh.m_nTris=mesh.m_nVertices = 4;
	mesh.m_pNormals = normals;
	mesh.m_pVertices = strided_pointer<Vec3>(vtx);
	mesh.m_pIndices = idx;
	mesh.m_flags = mesh_shared_topology|mesh_shared_idx|mesh_shared_vtx|mesh_shared_normals;
	mesh.m_nMaxVertexValency = 3;
	for(i=0;i<4;i++)
		normals[i] = (vtx[idx[i*3+1]]-vtx[idx[i*3]]^vtx[idx[i*3+2]]-vtx[idx[i*3]]).normalized();
	mesh.m_pTopology = top;
	sbtree.m_Box.Basis.SetIdentity(); sbtree.m_Box.bOriented=0;
	sbtree.m_Box.center.Set(0, h*0.5f, vtx[3].z*0.5f);
	sbtree.m_Box.size.Set(w*1.01f, h*1.01f, vtx[3].z*-0.5f);
	sbtree.m_pGeom=&mesh; sbtree.m_nPrims=mesh.m_nTris;
	mesh.m_pTree = &sbtree;
	memset(m_pCellMask+wgrid.size.x+1,15<<2,(wgrid.size.x+1)*wgrid.size.y*sizeof(m_pCellMask[0]));
	for(i=0;i<wgrid.size.x+2;i++) m_pCellMask[i]=m_pCellMask[wgrid.stride.y*(wgrid.size.y+1)+i] = 66;
	for(i=0;i<wgrid.size.y+2;i++) m_pCellMask[i*wgrid.stride.y] = 66;

	for(ipart=nMeshes=nPrims=ipartMax=0,depth=0;ipart<pent->m_nParts;ipart++) if (pent->m_parts[ipart].flags & geom_floats) {
		// intersect each entity part with the prism
		gwd.offset = (pent->m_pos+pent->m_qrot*pent->m_parts[ipart].pos-m_origin)*m_R - Vec3(ibbox[0].x+ibbox[1].x,ibbox[0].y+ibbox[1].y,0)*m_tileSz*0.5f;
		gwd.R = m_R.T()*Matrix33(pent->m_qrot*pent->m_parts[ipart].q);
		gwd.scale = pent->m_parts[ipart].scale;
		j = pent->m_parts[ipart].pPhysGeomProxy->pGeom->GetType();
		ipartMax += ipart-ipartMax & -isneg(pent->m_parts[ipartMax].pPhysGeomProxy->V*pent->m_parts[ipartMax].scale-pent->m_parts[ipart].pPhysGeomProxy->V*pent->m_parts[ipart].scale);
		pent->m_parts[ipart].pPhysGeomProxy->pGeom->GetBBox(&bbox);
		depth = max(depth,-(gwd.R*bbox.center*gwd.scale + gwd.offset - (gwd.R*bbox.Basis.T()).Fabs()*bbox.size*gwd.scale).z);
		if (j==GEOM_TRIMESH || j==GEOM_VOXELGRID || j==GEOM_HEIGHTFIELD) {
			if (ncont = mesh.Intersect(pent->m_parts[ipart].pPhysGeomProxy->pGeom,0,&gwd,&ip,pcontacts)) {
				for(icont=0;icont<ncont;icont++) {
					for(i=npt=0;i<pcontacts[icont].nborderpt;i++) if (fabs_tpl(pcontacts[icont].ptborder[i].z)<0.001f) {
						(pts[npt].pt = Vec2(pcontacts[icont].ptborder[i])) += Vec2(w,h)*0.5f; 
						pts[npt].next = pts+npt+1; pts[npt].prev = pts+npt-1;	
						if (++npt==CRY_ARRAY_COUNT(pts))
							break;
					}
					assert(npt>0);
					pts[0].prev=pts+npt-1; pts[npt-1].next=pts;	plist=pts;
					if (!pcontacts[icont].bBorderConsecutive && qhull2d(pts,npt,pcontour)) {
						plist=pvtx=(pedge=pcontour)->pvtx; npt=0; 
						do {
							pvtx->next = pedge->next->pvtx;
							pvtx->prev = pedge->prev->pvtx;
							pvtx = pvtx->next; npt++;
						}	while((pedge=pedge->next)!=pcontour);
					}

					jgc.iedge[0]=jgc.iedge[1]=jgc.iprevcell=jgc.iedgeExit0 = -1;
					for(pvtx=plist,i=0,j=2; i<npt; i++,pvtx=pvtx->next) {
						jgc.org = pvtx->pt; jgc.dirn = norm(jgc.dir = pvtx->next->pt-pvtx->pt);
						Vec3 gorg(jgc.org.x,jgc.org.y,0),gdir(jgc.dir.x,jgc.dir.y,0);
						DrawRayOnGrid(&wgrid, gorg,gdir, jgc);
					}
					if (jgc.iedge[0]!=jgc.iedgeExit0) for(i=jgc.iedgeExit0+1&3; i!=jgc.iedge[0]; i=i+1&3)
						j |= 4<<i;
					jgc.pCellMask[Vec2i(jgc.iprevcell&0xFFFF,jgc.iprevcell>>16)*wgrid.stride] &= j;
				}
				nMeshes++;
			}
		} else {
			// for non-mesh geoms, just project them on the grid and check each affected cell for point containment
			org = (pent->m_qrot*(pent->m_parts[ipart].q*bbox.center*pent->m_parts[ipart].scale+pent->m_parts[ipart].pos)+pent->m_pos-m_origin)*m_R;
			gwd.R = Matrix33(!(pent->m_qrot*pent->m_parts[ipart].q))*m_R;
			gwd.scale = pent->m_parts[ipart].scale==1.0f ? 1.0f:1.0f/pent->m_parts[ipart].scale;
			gwd.offset.Set((ibbox[0].x-0.5f)*m_tileSz+m_cellSz*0.5f, (ibbox[0].y-0.5f)*m_tileSz+m_cellSz*0.5f, 0);
			gwd.offset = ((m_R*gwd.offset+m_origin-pent->m_pos)*pent->m_qrot-pent->m_parts[ipart].pos)*pent->m_parts[ipart].q*gwd.scale;
			gwd.scale *= m_cellSz;
			sz = (bbox.size*(bbox.Basis*=gwd.R).Fabs())*pent->m_parts[ipart].scale;
			ibbox1[0].x = max(0, float2int((org.x-sz.x)*m_rcellSz-(ibbox[0].x-0.5f)*m_nCells-0.5f));
			ibbox1[0].y = max(0, float2int((org.y-sz.y)*m_rcellSz-(ibbox[0].y-0.5f)*m_nCells-0.5f));
			ibbox1[1].x = min((ibbox[1].x-ibbox[0].x+1)*m_nCells-1, float2int((org.x+sz.x)*m_rcellSz-(ibbox[0].x-0.5f)*m_nCells-0.5f));
			ibbox1[1].y = min((ibbox[1].y-ibbox[0].y+1)*m_nCells-1, float2int((org.y+sz.y)*m_rcellSz-(ibbox[0].y-0.5f)*m_nCells-0.5f));
			pcell = jgc.pCellMask+ibbox1[0]*wgrid.stride;
			for(ic.y=ibbox1[0].y; ic.y<=ibbox1[1].y; ic.y++,pcell+=wgrid.stride.y-(ibbox1[1].x-ibbox1[0].x+1)) 
				for(ic.x=ibbox1[0].x,pt=gwd.R*Vec3(ic.x,ic.y,0)*gwd.scale+gwd.offset; ic.x<=ibbox1[1].x; ic.x++,pcell++,pt+=gwd.R.GetColumn(0)*gwd.scale)
					*pcell |= pent->m_parts[ipart].pPhysGeomProxy->pGeom->PointInsideStatus(pt)*2;
			// find border cells by counting filled neighbours; set cell normal to cell_center-primitive_center
			org -= Vec3((ibbox[0].x-0.5f)*m_tileSz+0.5f*m_cellSz, (ibbox[0].y-0.5f)*m_tileSz+0.5f*m_cellSz, 0);
			for(ic.y=ibbox1[0].y,icell=ibbox1[0]*wgrid.stride; ic.y<=ibbox1[1].y; ic.y++,icell+=wgrid.stride.y-(ibbox1[1].x-ibbox1[0].x+1)) 
				for(ic.x=ibbox1[0].x,pt=gwd.R*Vec3(ic.x,ic.y,0)*gwd.scale+gwd.offset; ic.x<=ibbox1[1].x; ic.x++,icell++,pt+=gwd.R.GetColumn(0)*gwd.scale)
					if (((jgc.pCellMask[icell-1]^jgc.pCellMask[icell-1]>>5)&2) + ((jgc.pCellMask[icell-wgrid.stride.y]^jgc.pCellMask[icell-wgrid.stride.y]>>5)&2)	+
							((jgc.pCellMask[icell+1]^jgc.pCellMask[icell+1]>>5)&2) + ((jgc.pCellMask[icell+wgrid.stride.y]^jgc.pCellMask[icell+wgrid.stride.y]>>5)&2) < 
							(jgc.pCellMask[icell]&2)*4)
						jgc.pnorms[icell].set(ic.x*m_cellSz-org.x, ic.y*m_cellSz-org.y);	
			nPrims++;
		}
	}

	if (nMeshes+nPrims && depth>0) {
		float kr = m_kResistance*m_dt*min(1.0f,depth/max(1e-4f,m_depth*m_cellSz));
		if (nMeshes) {
			jgc.pqueue=m_pCellQueue; jgc.szQueue=m_szCellQueue;
			for(i=0;i<(wgrid.size.x+1)*wgrid.size.y;i++) if (jgc.pCellMask[i]&2 && jgc.pCellMask[i]&15<<2)
				for(j=0;j<4;j++) if (jgc.pCellMask[i] & 4<<j && !(jgc.pCellMask[icell = i+idx2offs(j)*wgrid.stride] & 2))
					jgc.MarkCellInteriorQueue(icell);
			m_pCellQueue=jgc.pqueue; m_szCellQueue=jgc.szQueue;
		}
		pbody = pent->GetRigidBodyData(&body,ipartMax);
		wloc = (pbody->w*m_R)*(1-iszero(pent->m_parts[ipartMax].pPhysGeomProxy->pGeom->GetType()-GEOM_SPHERE));
		for(itile.x=ibbox[0].x;itile.x<=ibbox[1].x;itile.x++) for(itile.y=ibbox[0].y;itile.y<=ibbox[1].y;itile.y++)	{
			(ptile=m_pTiles[(itile+Vec2i(1,1)*m_nTiles)*strideTiles])->Activate();
			icell = ((itile-ibbox[0])*wgrid.stride)*m_nCells;
			org = Vec3((itile.x-0.5f)*m_tileSz+0.5f*m_cellSz,(itile.y-0.5f)*m_tileSz+0.5f*m_cellSz,0)-(pbody->pos-m_origin)*m_R;
			vloc = pbody->v*m_R + (wloc^org);
			for(ic.y=i=0;ic.y<m_nCells;ic.y++,icell+=wgrid.stride.y-m_nCells) 
				for(ic.x=0;ic.x<m_nCells;ic.x++,icell++,i++) if ((jgc.pCellMask[icell] & 3 & -ptile->cell_used(i))==2)	{
					Vec3 vel = vloc+(wloc^Vec3(ic.x,ic.y,0)*m_cellSz);
					if (len2(jgc.pnorms[icell])>0)
						vel -= Vec3(jgc.pnorms[icell].GetNormalized())*vel.z;
					(*(Vec2*)(ptile->pvel+i) *= 1-kr) += Vec2(vel)*kr;
				}
		}
	}
	mesh.m_pTree = 0;
}


inline uchar pack_normal(const Vec2& n) {
	int bounds[] = { 0,SINCOSTABSZ>>1 };
	Vec2 nabs(fabs_tpl(n.x),fabs_tpl(n.y));
	float sin=min(nabs.x,nabs.y), cos=nabs.x+nabs.y-sin;
	if (!inrange(sin*sin+cos*cos,0.99f,1.01f))
		sin /= sqrt_tpl(sin*sin+cos*cos);
	for(int i=0;i<6;i++) {
		int isin = bounds[0]+bounds[1]>>1;
		bounds[isneg(sin-g_sintab[isin])] = isin;
	}
	int swap = isneg(nabs.x-nabs.y);
	return float2int(((SINCOSTABSZ&-swap)+bounds[0]*(1-swap*2))*(61.0f/SINCOSTABSZ))+1<<2 | isneg(n.x) | isneg(n.y)*2;
}
inline Vec2 unpack_normal(uchar n) {
	int i = float2int(((n>>2)-1)*(SINCOSTABSZ/61.0f));
	return Vec2(g_costab[i]*(1-(n<<1&2)), g_sintab[i]*(1-(n&2)));
}

void CWaterMan::GenerateSurface(const Vec2i* BBox, Vec3 *pvtx,Vec3_tpl<index_t> *pTris, const Vec2 *ptbrd,int nbrd, const Vec3& offs3d, Vec2 *pttmp, float tmax)
{
	int i,j,icell=m_nTiles*m_nCells+(m_nCells>>1);
	Vec2i BBoxFull[2] = { -Vec2i(icell,icell),Vec2i(icell,icell) };
	if (!BBox)
		BBox = BBoxFull;
	grid wgrid;
	jgrid_checker pgc; memset(&pgc,0,sizeof(pgc));
	wgrid.size = BBox[1]-BBox[0]+Vec2i(1,1);
	wgrid.step.set(m_cellSz,m_cellSz);
	wgrid.stepr.set(m_rcellSz,m_rcellSz);
	wgrid.stride.set(1,wgrid.size.x+1);
	wgrid.origin.zero();
	pgc.pgrid=&wgrid; pgc.pCellMask=m_pCellMask+wgrid.size.x+2; pgc.ppt=pttmp ? pttmp:(Vec2*)pvtx;
	memset(pgc.pnorms=m_pCellNorm, 0, (wgrid.size.x+2)*(wgrid.size.y+2)*sizeof(pgc.pnorms[0]));
	memset(m_pCellMask+wgrid.size.x+1,15<<2,(wgrid.size.x+1)*wgrid.size.y*sizeof(m_pCellMask[0]));
	for(i=0;i<wgrid.size.x+2;i++) m_pCellMask[i]=m_pCellMask[wgrid.stride.y*(wgrid.size.y+1)+i] = 66;
	for(i=0;i<wgrid.size.y+2;i++) m_pCellMask[i*wgrid.stride.y] = 66;
	Vec2 offs = (Vec2(BBox[0])-Vec2(0.5f,0.5f))*-m_cellSz;
	memset(pgc.ppt,0,(wgrid.size.x+1)*(wgrid.size.y+1)*sizeof(pgc.ppt[0]));

	pgc.iedge[0]=pgc.iedge[1]=pgc.iprevcell=pgc.iedgeExit0 = -1;
	for(i=0,j=2;i<nbrd;i++) {
		pgc.org = ptbrd[i]+offs-Vec2(offs3d); pgc.dirn = norm(pgc.dir = ptbrd[i+1&i+1-nbrd>>31]-ptbrd[i]);
		Vec3 org(pgc.org),dir(pgc.dir); DrawRayOnGrid(&wgrid,org,dir,pgc);
	}
	if (pgc.iedge[0]!=pgc.iedgeExit0) for(i=pgc.iedgeExit0+1&3; i!=pgc.iedge[0]; i=i+1&3)
		j |= 4<<i;
	pgc.pCellMask[Vec2i(pgc.iprevcell&0xFFFF,pgc.iprevcell>>16)*wgrid.stride] &= j;
	pgc.pqueue=m_pCellQueue; pgc.szQueue=m_szCellQueue;
	for(i=0;i<(wgrid.size.x+1)*wgrid.size.y;i++) if ((pgc.pCellMask[i]&64+8+2)==8+2 && !(pgc.pCellMask[i+1]&2))
		pgc.MarkCellInteriorQueue(i+1);
	m_pCellQueue=pgc.pqueue; m_szCellQueue=pgc.szQueue;

	for(int iy=0;iy<wgrid.size.y;iy++) for(int ix=0;ix<wgrid.size.x;ix++) {
		Vec3 vtx = Vec3(Vec2i(ix,iy)+BBox[0])*m_cellSz; 
		int itile,icell=GetCellIdx(Vec2i(ix,iy)+BBox[0],itile);
		SWaterTile *ptile = m_pTiles[itile];
		vtx.z = ptile->ph[icell];	
		j = pgc.pCellMask[i=ix+iy*wgrid.stride.y];
		if (j & 2 && (j & 60)!=60) {
			vtx = Vec3(pgc.ppt[i]-offs)+Vec3(0,0,GetHeight(pgc.ppt[i]-offs));
			ptile->norm[icell] = pack_normal(-pgc.pnorms[i]);
		}	else
			ptile->norm[icell] = (j>>1&1)-1;
		int ivtx=ix+iy*wgrid.size.x, itri=ix+iy*(wgrid.size.x-1)<<1;
		pvtx[ivtx] = vtx+offs3d;
		if ((iy+1-wgrid.size.y & ix+1-wgrid.size.x)<0) {
			int mask = -((j & pgc.pCellMask[i+1] & pgc.pCellMask[i+wgrid.stride.y] & pgc.pCellMask[i+wgrid.stride.y+1]&2)>>1);
			pTris[itri  ].Set(ivtx,ivtx+(1&mask),ivtx+(wgrid.size.x&mask));
			pTris[itri+1].Set(ivtx+1,ivtx+1+(wgrid.size.x&mask),ivtx+1+(wgrid.size.x-1&mask));
		}
	}
}


void CWaterMan::OnWaterHit(const Vec3 &pthit, const Vec3 &vel)
{
	Vec3 pt;
	Vec2i itile,icell;
	SWaterTile *ptile;
	pt = (pthit-m_origin)*m_R;
	itile.set(float2int(pt.x*m_rtileSz), float2int(pt.y*m_rtileSz));
	if (max(fabs_tpl(itile.x),fabs_tpl(itile.y))>m_nTiles)
		return;
	ptile = m_pTiles[(itile+Vec2i(1,1)*m_nTiles)*Vec2i(1,m_nTiles*2+1)];
	icell.set(float2int(pt.x*m_rcellSz-(itile.x-0.5f)*m_nCells-0.5f), (int)(float2int(pt.y*m_rcellSz)-(itile.y-0.5f)*m_nCells-0.5f));
	ptile->Activate();
	ptile->pvel[icell*Vec2i(1,m_nCells)].z += min(0.5f,max(-0.5f,m_R.GetRow(2)*vel));
}


void CWaterMan::SetViewerPos(const Vec3 &newpos, int iCaller)
{
	if (m_nTiles<0)
		return;

	int i,j,ix,iy,nbuoys;
	Vec3 axisx,axisy,g,newOrigin;
	Vec2i ic,ibbox[2],strideTile(1,m_nTiles*2+1);
	pe_params_buoyancy pb[4];
	m_bActive = 0;

	if (m_pArea) {
		m_pArea->GetParams(pb); i=0;
		pe_status_pos sp; sp.pMtx3x3=&m_R;
		m_pArea->GetStatus(&sp);
		axisx=m_R*Vec3(1,0,0); axisy=m_R*Vec3(0,1,0);
	} else {
		for(i=0,nbuoys=m_pWorld->CheckAreas(newpos,g,pb,4,-1,Vec3(ZERO),0,iCaller); i<nbuoys && pb[i].iMedium; i++);
		if (i>=nbuoys)
			return;
		axisx = pb[i].waterPlane.origin-pb[i].waterPlane.n*(pb[i].waterPlane.n*pb[i].waterPlane.origin);
		if (axisx.len2()==0)
			axisx = Vec3(1,0,0)-pb[i].waterPlane.n*pb[i].waterPlane.n.x;
		axisx.normalize();
		axisy = pb[i].waterPlane.n^axisx;
		m_R.SetFromVectors(axisx,axisy,pb[i].waterPlane.n);
		m_R.Transpose();
	}
	if (fabs_tpl((newpos-pb[i].waterPlane.origin)*pb[i].waterPlane.n)>m_tileSz*(m_nTiles+1))
		return;

	m_bActive = 1;
	
	ix = float2int((axisx*(newpos-pb[i].waterPlane.origin))*m_rtileSz-0.5f);
	iy = float2int((axisy*(newpos-pb[i].waterPlane.origin))*m_rtileSz-0.5f);
	newOrigin = pb[i].waterPlane.origin + axisx*((ix+0.5f)*m_tileSz) + axisy*((iy+0.5f)*m_tileSz);
	ibbox[0].set(max(0,ix-m_ix), max(0,iy-m_iy));
	ibbox[1].set(min(0,ix-m_ix)+m_nTiles*2, min(0,iy-m_iy)+m_nTiles*2);

	if (ibbox[1].x<ibbox[0].x || ibbox[1].y<ibbox[0].y)
		for(j=0;j<sqr(m_nTiles*2+1);j++) m_pTiles[j]->bActive = 0;
	else {
		memcpy(m_pTilesTmp, m_pTiles, sqr(m_nTiles*2+1)*sizeof(m_pTiles[0]));
		for(ic.x=ibbox[0].x;ic.x<=ibbox[1].x;ic.x++) for(ic.y=ibbox[0].y;ic.y<=ibbox[1].y;ic.y++)
			m_pTiles[(ic+Vec2i(m_ix-ix,m_iy-iy))*strideTile] = m_pTilesTmp[ic*strideTile];
		for(ic.y=j=0;ic.y<=m_nTiles*2;ic.y++) for(ic.x=0;ic.x<=m_nTiles*2;ic.x++)	{
			m_pTilesTmp[j] = m_pTilesTmp[ic*strideTile];
			j += (inrange(ic.x,ibbox[0].x-1,ibbox[1].x+1) & inrange(ic.y,ibbox[0].y-1,ibbox[1].y+1))^1;
		}
		ibbox[0] += Vec2i(m_ix-ix,m_iy-iy); ibbox[1] += Vec2i(m_ix-ix,m_iy-iy);
		for(ic.y=j=0;ic.y<=m_nTiles*2;ic.y++) for(ic.x=0;ic.x<=m_nTiles*2;ic.x++)
			if (!(inrange(ic.x,ibbox[0].x-1,ibbox[1].x+1) & inrange(ic.y,ibbox[0].y-1,ibbox[1].y+1)))
				(m_pTiles[ic*strideTile] = m_pTilesTmp[j++])->bActive = 0;
	}
	m_origin=newOrigin;	m_ix=ix; m_iy=iy;
	m_waterOrigin = pb[i].waterPlane.origin;
	m_posViewer = newpos;
}

void CWaterMan::UpdateWaterLevel(const plane& waterPlane, float dt)
{
	m_waterOrigin += waterPlane.n*(waterPlane.n*(waterPlane.origin-m_waterOrigin));
	float diff = waterPlane.n*(waterPlane.origin-m_origin);
	m_origin += waterPlane.n*diff;
	OffsetWaterLevel(-diff,dt);
}

void CWaterMan::OffsetWaterLevel(float diff, float dt)
{
	if (dt>0) {
		for(int i=0;i<=sqr(m_nTiles*2+1);i++) if (m_pTiles[i]->bActive)
			for(int j=0;j<sqr(m_nCells);j++) m_pTiles[i]->ph[j]+=diff;
		m_doffs	-= diff; 
	}
}


inline SWaterTile* CWaterMan::get_tile(const Vec2i& itile,const Vec2i& ic,int &i1)
{
	Vec2i itile1=itile, ic1=ic; int i;
	i = ic1.x>>31; itile1.x += i; ic1.x += m_nCells & i; 
	i = ic1.y>>31; itile1.y += i; ic1.y += m_nCells & i; 
	i = m_nCells-1-ic1.x>>31; itile1.x -= i; ic1.x -= m_nCells & i;
	i = m_nCells-1-ic1.y>>31; itile1.y -= i; ic1.y -= m_nCells & i;
	i1 = ic1.x + ic1.y*m_nCells;
	return (itile1.x|itile1.y|m_nTiles*2-itile1.x|m_nTiles*2-itile1.y)>=0 ? m_pTiles[itile1.x+itile1.y*(m_nTiles*2+1)] : 0;
}

struct reflector_tracer {
	uchar *norm;
	int i,stridey,bHit;
	int check_cell(const Vec2i& icell,int &ilastcell) {	return bHit=isneg(1-(int)norm[i=icell.x+icell.y*stridey]); }
};

inline void CWaterMan::advect_cell(const Vec2i& itile, const Vec2i& cell, const Vec2& vel, float m, grid* rgrid)
{
	Vec2 dir = vel*(m_rcellSz*m_dt), posNew = Vec2(cell)+dir;
	Vec2i ic(float2int(posNew.x+0.5f),float2int(posNew.y+0.5f));
	for(int i=0,i1; i<4; i++) if (SWaterTile *ptile1 = get_tile(itile,ic-Vec2i(i>>1,i&1),i1)) {
		float frac = fabs_tpl((posNew+Vec2i(i>>1^1,i&1^1)-ic).area());
		Vec2 velCell=vel, n;
		if (!ptile1->cell_used(i1)) {
			reflector_tracer rt;
			rt.norm=ptile1->norm; rt.stridey=m_nCells;
			Vec3 org3d=Vec3(cell)+Vec3(0.5f), dir3d=Vec3(dir-Vec2(i>>1,i&1)+ic-cell);
			DrawRayOnGrid(rgrid,org3d,dir3d,rt);
			if (!rt.bHit)
				continue;
			n = unpack_normal(ptile1->norm[i1=rt.i]);
			velCell -= n*min(0.0f,n*vel)*2;
		}	else if (ptile1->norm[i1])
			n=unpack_normal(ptile1->norm[i1]), velCell-=n*min(0.0f,n*vel)*2;
		ptile1->m[i1] += m*frac;
		ptile1->mv[i1] += velCell*(m*frac);
	}
}

void CWaterMan::TimeStep(float time_interval)
{
	if (m_nTiles<0 || !m_bActive || !m_pTiles)
		return;
	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );

	int i,j,ix,iy,i1,hasBorder=0, tileActive,volActive=0;
	float vmax,h,dt,vsum=0,depth=m_depth*m_cellSz,rnTiles=1.0f/max(1,m_nTiles),minh=max(depth*-0.95f,m_cellSz*-m_hlimit),maxh=m_cellSz*m_hlimit;
	Vec2i itile,ic,ic1,dc,dc1,strideTile(1,m_nTiles*2+1),stride(1,m_nCells);
	Diag33 damping; damping.x=damping.y=max(0.0f,1-m_dampingCenter*m_dt);
	SWaterTile *ptile,*ptile1;
	grid rgrid;
	rgrid.size.set(m_nCells,m_nCells);
	rgrid.step.set(1,1); rgrid.stepr.set(1,1);
	rgrid.stride.set(1,m_nCells);
	rgrid.origin.zero(); rgrid.bCyclic=1;
	if (m_pWorld->m_vars.fixedTimestep)
		m_dt = m_pWorld->m_vars.fixedTimestep;
	float g = m_waveSpeed;

	for(dt=m_timeSurplus; dt<time_interval; dt+=m_dt)	{
		float doffs = min(fabs_tpl(m_doffs),m_dt*3)*sgnnz(m_doffs);
		m_doffs -= doffs;
		volActive = 0;

		for(itile.x=0;itile.x<=m_nTiles*2;itile.x++) for(itile.y=0;itile.y<=m_nTiles*2;itile.y++) 
			if ((ptile=m_pTiles[itile*strideTile])->bActive) {
				float tdist = max(fabs_tpl(itile.x-m_nTiles),fabs_tpl(itile.y-m_nTiles))*rnTiles;
				damping.z = max(0.0f,1-m_dt*(m_dampingRim*tdist + m_dampingCenter*(1-tdist)));
				for(i=sqr(m_nCells)-1; i>=0; i--) 
					ptile->mv[i] = Vec2(ptile->pvel[i]=damping*ptile->pvel[i])*(ptile->m[i]=ptile->ph[i]+depth);
			}	else {
				memset(ptile->mv,0,sqr(m_nCells)*sizeof(ptile->mv[0]));
				for(i=sqr(m_nCells)-1;i>=0;i--) ptile->m[i]=depth;
			}

		static float rnused[] = { 1.0f,1.0f,0.5f,1.0f/3,0.25f };
		for(itile.x=0;itile.x<=m_nTiles*2;itile.x++) for(itile.y=0;itile.y<=m_nTiles*2;itile.y++) 
			if ((ptile = m_pTiles[itile*strideTile])->bActive) { 
				ic.set(0,0); dc1.set(1,0);
				for(int j1=0;j1<4;j1++,dc1=dc1.rot90ccw()) for(int i2=0;i2<m_nCells-1;i2++,ic+=dc1) if (ptile->cell_used(i=ic*stride)) {
					float pullm = isneg(-ptile->pvel[i].z); int nused=0,used;
					for(j=0,dc.set(0,1),h=0; j<4; j++,dc=dc.rot90cw()) if (ptile1=get_tile(itile,ic+dc,i1))	{
						nused += used=ptile1->cell_used(i1);
						h += ptile1->ph[i1]*used;	hasBorder|=1-used;
						ptile->mv[i] += Vec2(ptile1->pvel[i1])*max(0.0f,-ptile1->pvel[i1].z*m_dt*0.25f)*pullm;
					}
					vsum += (ptile->pvel[i].z += (h*rnused[nused]-ptile->ph[i])*g*m_dt);
				}
				for(iy=1,i=m_nCells+1;iy<m_nCells-1;iy++,i+=2) for(ix=1;ix<m_nCells-1;ix++,i++) if (ptile->cell_used(i)) {
					float pullm = isneg(-ptile->pvel[i].z); int nused=0,used;
					for(j=0,dc.set(0,1),h=0; j<4; j++,dc=dc.rot90cw()) {
						nused += used=ptile->cell_used(i1=(Vec2i(ix,iy)+dc)*stride);
						h += ptile->ph[i1]*used; hasBorder|=1-used;
						ptile->mv[i] += Vec2(ptile->pvel[i1])*max(0.0f,-ptile->pvel[i1].z*m_dt*0.25f)*pullm;
					}
					vsum += (ptile->pvel[i].z += (h*rnused[nused]-ptile->ph[i])*g*m_dt);
				}
			}

		for(itile.x=0;itile.x<=m_nTiles*2;itile.x++) for(itile.y=0;itile.y<=m_nTiles*2;itile.y++) 
			if ((ptile = m_pTiles[itile*strideTile])->bActive)
				for(iy=i=0;iy<m_nCells;iy++) for(ix=0;ix<m_nCells;ix++,i++) if (Vec2(ptile->pvel[i]).GetLength2()>sqr(m_minVel)) {	
					float m = min(ptile->m[i], ptile->ph[i]+depth);
					ptile->m[i] -= m; ptile->mv[i] -= Vec2(ptile->pvel[i])*m;
					advect_cell(itile,Vec2i(ix,iy),Vec2(ptile->pvel[i]),m,&rgrid);
				}

		vsum *= sqr(m_cellSz*m_rtileSz)*hasBorder;
		for(j=sqr(m_nTiles*2+1)-1; j>=0; j--) {
			if (!(ptile=m_pTiles[j])->bActive) {
				for(i=sqr(m_nCells)-1; i>=0 && ptile->mv[i].GetLength2()<sqr(m_minVel*ptile->m[i]); i--);
				if (i<0) {
					for(i=sqr(m_nCells)-1;i>=0;i--) ptile->ph[i]+=doffs;
					continue;
				}
			}
			for(i=sqr(m_nCells)-1,vmax=h=0,ptile=m_pTiles[j]; i>=0; i--) {
				int inside = ptile->cell_used(i);
				float hnew = ptile->m[i]-depth+doffs;
				ptile->pvel[i].z -= vsum;
				*(Vec2*)(ptile->pvel+i) = (Vec2(ptile->mv[i])/max(m_cellSz*0.01f,ptile->m[i]))*inside;
				int flip = isneg(ptile->ph[i]*(hnew += ptile->pvel[i].z*m_dt));
				hnew *= 1-m_dampingCenter*0.5f*m_dt;
				ptile->pvel[i].z *= (inrange(hnew,minh,maxh) | isneg(ptile->pvel[i].z*hnew));
				h = max(h, ptile->ph[i] = min(maxh,max(minh, hnew))*(1-flip&inside));
				vmax = max(vmax, ptile->pvel[i].len2());
			}
			volActive += (tileActive = isneg(sqr(m_minVel)-max(vmax,sqr(h*2*g*m_dt))));
			ptile->Activate(tileActive);
		}
	}
	if (m_pArea && volActive)
		m_pWorld->ActivateArea((CPhysArea*)m_pArea);
	m_timeSurplus = dt-time_interval;
}


void CWaterMan::DrawHelpers(IPhysRenderer *pRenderer)
{
	if (m_bActive) {
		int i;
		Vec2i ic,stride(1,m_nTiles*2+1);
		CHeightfield hf;
		geom_world_data gwd;
		hf.m_hf.Basis.SetIdentity();
		hf.m_hf.size.set(m_nCells-1,m_nCells-1);
		hf.m_hf.step.set(m_cellSz,m_cellSz);
		hf.m_hf.stride.set(1,m_nCells);
		hf.m_hf.stepr.x=hf.m_hf.stepr.y = 1.0f/m_cellSz;
		hf.m_hf.heightscale = 1.0f;
		hf.m_hf.heightmask = 0;
		hf.m_Tree.m_phf = &hf.m_hf;
		hf.m_Tree.m_pMesh = &hf;
		hf.m_pTree = &hf.m_Tree;
		hf.m_hf.fpGetHeightCallback = 0;
		gwd.R = m_R;
		gwd.offset = m_origin-m_R*(Vec3(m_nTiles+0.5f,m_nTiles+0.5f,0)*m_tileSz-Vec3(0.5f,0.5f,0)*m_cellSz);

		for(ic.x=0;ic.x<=m_nTiles*2;ic.x++) for(ic.y=0;ic.y<=m_nTiles*2;ic.y++) if (m_pTiles[i=ic*stride]->bActive) {
			hf.m_hf.origin.Set(ic.x*m_tileSz,ic.y*m_tileSz,0);
			hf.m_hf.fpGetSurfTypeCallback = (getSurfTypeCallback)m_pTiles[i]->ph;
			pRenderer->DrawGeometry(&hf,&gwd,7);
		}
	}
}

void CWaterMan::GetMemoryStatistics(ICrySizer *pSizer) const
{
	pSizer->AddObject((const char*)this, sizeof(*this));
	int n = sqr(m_nTiles*2+1)+1;
	pSizer->AddObject(m_pTiles, n*sizeof(m_pTiles[0]));
	for(int i=0;i<n;i++) {
		pSizer->AddObject(m_pTiles[i], sizeof(m_pTiles[i]));
		pSizer->AddObject(m_pTiles[i]->ph,  sqr(m_nCells)*sizeof(m_pTiles[i]->ph[0]));
		pSizer->AddObject(m_pTiles[i]->pvel,sqr(m_nCells)*sizeof(m_pTiles[i]->pvel[0]));
		pSizer->AddObject(m_pTiles[i]->mv,  sqr(m_nCells)*sizeof(m_pTiles[i]->mv[0]));
		pSizer->AddObject(m_pTiles[i]->m,   sqr(m_nCells)*sizeof(m_pTiles[i]->m[0]));
		pSizer->AddObject(m_pTiles[i]->norm,sqr(m_nCells)*sizeof(m_pTiles[i]->norm[0]));
	}
	pSizer->AddObject(m_pTilesTmp, n*sizeof(m_pTilesTmp[0])); 
	pSizer->AddObject(m_pCellMask, (n=sqr((m_nTiles*2+1)*m_nCells+2))*sizeof(m_pCellMask[0]));
	pSizer->AddObject(m_pCellNorm, n*sizeof(m_pCellNorm[0]));
	pSizer->AddObject(m_pCellQueue, m_szCellQueue*sizeof(m_pCellQueue[0]));
}
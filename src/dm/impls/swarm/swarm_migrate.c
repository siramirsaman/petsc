
#include <petsc/private/dmswarmimpl.h>    /*I   "petscdmswarm.h"   I*/
#include "data_bucket.h"
#include "data_ex.h"


#undef __FUNCT__
#define __FUNCT__ "DMSwarmMigrate_Push_Basic"
PetscErrorCode DMSwarmMigrate_Push_Basic(DM dm,PetscBool remove_sent_points)
{
  DM_Swarm *swarm = (DM_Swarm*)dm->data;
  PetscErrorCode ierr;
  DataEx de;
  PetscInt p,npoints,*rankval,n_points_recv;
  PetscMPIInt rank,nrank;
  void *point_buffer,*recv_points;
  size_t sizeof_dmswarm_point;
  
  ierr = MPI_Comm_rank(PetscObjectComm((PetscObject)dm),&rank);CHKERRQ(ierr);

  ierr = DataBucketGetSizes(swarm->db,&npoints,NULL,NULL);CHKERRQ(ierr);
  ierr = DMSwarmGetField(dm,"DMSwarm_rank",NULL,NULL,(void**)&rankval);CHKERRQ(ierr);
  
  de = DataExCreate(PetscObjectComm((PetscObject)dm),0);CHKERRQ(ierr);
  
  ierr = DataExTopologyInitialize(de);CHKERRQ(ierr);
  for (p=0; p<npoints; p++) {
    nrank = rankval[p];
    if (nrank != rank) {
      ierr = DataExTopologyAddNeighbour(de,nrank);CHKERRQ(ierr);
    }
  }
  ierr = DataExTopologyFinalize(de);CHKERRQ(ierr);
  
  ierr = DataExInitializeSendCount(de);CHKERRQ(ierr);
  for (p=0; p<npoints; p++) {
    nrank = rankval[p];
    if (nrank != rank) {
      ierr = DataExAddToSendCount(de,nrank,1);CHKERRQ(ierr);
    }
  }
  ierr = DataExFinalizeSendCount(de);CHKERRQ(ierr);
  
  ierr = DataBucketCreatePackedArray(swarm->db,&sizeof_dmswarm_point,&point_buffer);CHKERRQ(ierr);
  ierr = DataExPackInitialize(de,sizeof_dmswarm_point);CHKERRQ(ierr);
  for (p=0; p<npoints; p++) {
    nrank = rankval[p];
    if (nrank != rank) {
      /* copy point into buffer */
      ierr = DataBucketFillPackedArray(swarm->db,p,point_buffer);CHKERRQ(ierr);
      /* insert point buffer into DataExchanger */
      ierr = DataExPackData(de,nrank,1,point_buffer);CHKERRQ(ierr);
    }
  }
  ierr = DataExPackFinalize(de);CHKERRQ(ierr);

  
  if (remove_sent_points) {
    /* remove points which left processor */
    ierr = DataBucketGetSizes(swarm->db,&npoints,NULL,NULL);CHKERRQ(ierr);
    for (p=0; p<npoints; p++) {
      nrank = rankval[p];
      if (nrank != rank) {
        /* kill point */
        ierr = DataBucketRemovePointAtIndex(swarm->db,p);CHKERRQ(ierr);
        DataBucketGetSizes(swarm->db,&npoints,NULL,NULL);CHKERRQ(ierr); /* you need to update npoints as the list size decreases! */
        p--; /* check replacement point */
      }
    }		
  }
  ierr = DMSwarmRestoreField(dm,"DMSwarm_rank",NULL,NULL,(void**)&rankval);CHKERRQ(ierr);
  
  ierr = DataExBegin(de);CHKERRQ(ierr);
  ierr = DataExEnd(de);CHKERRQ(ierr);
  
  ierr = DataExGetRecvData(de,&n_points_recv,(void**)&recv_points);CHKERRQ(ierr);

	ierr = DataBucketGetSizes(swarm->db,&npoints,NULL,NULL);CHKERRQ(ierr);
	ierr = DataBucketSetSizes(swarm->db,npoints + n_points_recv,-1);CHKERRQ(ierr);
	for (p=0; p<n_points_recv; p++) {
    void *data_p = (void*)( (char*)recv_points + p*sizeof_dmswarm_point );
    
    ierr = DataBucketInsertPackedArray(swarm->db,npoints+p,data_p);CHKERRQ(ierr);
  }
  ierr = DataExView(de);CHKERRQ(ierr);
  ierr = DataBucketDestroyPackedArray(swarm->db,&point_buffer);CHKERRQ(ierr);
  ierr = DataExDestroy(de);CHKERRQ(ierr);
  
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMSwarmMigrate_GlobalToLocal_Basic"
PetscErrorCode DMSwarmMigrate_GlobalToLocal_Basic(DM dm,PetscInt *globalsize)
{
  DM_Swarm *swarm = (DM_Swarm*)dm->data;
  PetscErrorCode ierr;
  DataEx de;
  PetscInt p,npoints,*rankval,n_points_recv;
  PetscMPIInt rank,nrank,negrank;
  void *point_buffer,*recv_points;
  size_t sizeof_dmswarm_point;
  
  ierr = MPI_Comm_rank(PetscObjectComm((PetscObject)dm),&rank);CHKERRQ(ierr);
  
  ierr = DataBucketGetSizes(swarm->db,&npoints,NULL,NULL);CHKERRQ(ierr);
  *globalsize = npoints;
  ierr = DMSwarmGetField(dm,"DMSwarm_rank",NULL,NULL,(void**)&rankval);CHKERRQ(ierr);
  
  de = DataExCreate(PetscObjectComm((PetscObject)dm),0);CHKERRQ(ierr);
  
  ierr = DataExTopologyInitialize(de);CHKERRQ(ierr);
  for (p=0; p<npoints; p++) {
    negrank = rankval[p];
    if (negrank < 0) {
      nrank = -negrank - 1;
      ierr = DataExTopologyAddNeighbour(de,nrank);CHKERRQ(ierr);
    }
  }
  ierr = DataExTopologyFinalize(de);CHKERRQ(ierr);
  
  ierr = DataExInitializeSendCount(de);CHKERRQ(ierr);
  for (p=0; p<npoints; p++) {
    negrank = rankval[p];
    if (negrank < 0) {
      nrank = -negrank - 1;
      ierr = DataExAddToSendCount(de,nrank,1);CHKERRQ(ierr);
    }
  }
  ierr = DataExFinalizeSendCount(de);CHKERRQ(ierr);
  
  ierr = DataBucketCreatePackedArray(swarm->db,&sizeof_dmswarm_point,&point_buffer);CHKERRQ(ierr);
  ierr = DataExPackInitialize(de,sizeof_dmswarm_point);CHKERRQ(ierr);
  for (p=0; p<npoints; p++) {
    negrank = rankval[p];
    if (negrank < 0) {
      nrank = -negrank - 1;
      rankval[p] = nrank;
      /* copy point into buffer */
      ierr = DataBucketFillPackedArray(swarm->db,p,point_buffer);CHKERRQ(ierr);
      /* insert point buffer into DataExchanger */
      ierr = DataExPackData(de,nrank,1,point_buffer);CHKERRQ(ierr);
      rankval[p] = negrank;
    }
  }
  ierr = DataExPackFinalize(de);CHKERRQ(ierr);
  
  ierr = DMSwarmRestoreField(dm,"DMSwarm_rank",NULL,NULL,(void**)&rankval);CHKERRQ(ierr);
  
  ierr = DataExBegin(de);CHKERRQ(ierr);
  ierr = DataExEnd(de);CHKERRQ(ierr);
  
  ierr = DataExGetRecvData(de,&n_points_recv,(void**)&recv_points);CHKERRQ(ierr);
  
	ierr = DataBucketGetSizes(swarm->db,&npoints,NULL,NULL);CHKERRQ(ierr);
	ierr = DataBucketSetSizes(swarm->db,npoints + n_points_recv,-1);CHKERRQ(ierr);
	for (p=0; p<n_points_recv; p++) {
    void *data_p = (void*)( (char*)recv_points + p*sizeof_dmswarm_point );
    
    ierr = DataBucketInsertPackedArray(swarm->db,npoints+p,data_p);CHKERRQ(ierr);
  }
  ierr = DataExView(de);CHKERRQ(ierr);
  ierr = DataBucketDestroyPackedArray(swarm->db,&point_buffer);CHKERRQ(ierr);
  ierr = DataExDestroy(de);CHKERRQ(ierr);
  
  PetscFunctionReturn(0);
}

typedef struct {
  PetscMPIInt owner_rank;
  PetscReal min[3],max[3];
} CollectBBox;

#undef __FUNCT__
#define __FUNCT__ "DMSwarmMigrate_GlobalToLocal_BoundingBox"
PETSC_EXTERN PetscErrorCode DMSwarmMigrate_GlobalToLocal_BoundingBox(DM dm,PetscInt *globalsize)
{
  DM_Swarm *swarm = (DM_Swarm*)dm->data;
  PetscErrorCode ierr;
  DataEx de;
  PetscInt p,pk,npoints,*rankval,n_points_recv,n_bbox_recv,dim,neighbour_cells;
  PetscMPIInt rank,nrank;
  void *point_buffer,*recv_points;
  size_t sizeof_dmswarm_point,sizeof_bbox_ctx;
  PetscBool isdmda;
  CollectBBox *bbox,*recv_bbox;
  const PetscMPIInt *dmneighborranks;
  DM dmcell;
  
  ierr = MPI_Comm_rank(PetscObjectComm((PetscObject)dm),&rank);CHKERRQ(ierr);

  //ierr = DMSwarmGetDMCell(swarm,&dmcell);CHKERRQ(ierr);
  dmcell = swarm->dmcell;
  
  dim = 2;
  if (dim == 2) {
    neighbour_cells = 9;
  }
  
  isdmda = PETSC_FALSE;
  PetscObjectTypeCompare((PetscObject)dmcell,DMDA,&isdmda);
  if (!isdmda) SETERRQ(PetscObjectComm((PetscObject)dm),PETSC_ERR_SUP,"Only DMDA support for CollectBoundingBox");
  
  sizeof_bbox_ctx = sizeof(CollectBBox);
  PetscMalloc1(1,&bbox);
  bbox->owner_rank = rank;

  /* compute the bounding box based on the overlapping / stenctil size */
  //ierr = DMDAGetLocalBoundingBox(dmcell,bbox->min,bbox->max);CHKERRQ(ierr);
  {
    Vec lcoor;

    ierr = DMGetCoordinatesLocal(dmcell,&lcoor);CHKERRQ(ierr);
    
    ierr = VecStrideMin(lcoor,0,NULL,&bbox->min[0]);CHKERRQ(ierr);
    ierr = VecStrideMin(lcoor,1,NULL,&bbox->min[1]);CHKERRQ(ierr);
    ierr = VecStrideMax(lcoor,0,NULL,&bbox->max[0]);CHKERRQ(ierr);
    ierr = VecStrideMax(lcoor,1,NULL,&bbox->max[1]);CHKERRQ(ierr);
  }
  
  ierr = DataBucketGetSizes(swarm->db,&npoints,NULL,NULL);CHKERRQ(ierr);
  *globalsize = npoints;
  ierr = DMSwarmGetField(dm,"DMSwarm_rank",NULL,NULL,(void**)&rankval);CHKERRQ(ierr);
  
  de = DataExCreate(PetscObjectComm((PetscObject)dm),0);CHKERRQ(ierr);

  /* use DMDA neighbours */
	ierr = DMDAGetNeighbors(dmcell,&dmneighborranks);CHKERRQ(ierr);

  ierr = DataExTopologyInitialize(de);CHKERRQ(ierr);
  for (p=0; p<neighbour_cells; p++) {
		if ( (dmneighborranks[p] >= 0) && (dmneighborranks[p] != rank) ) {
      ierr = DataExTopologyAddNeighbour(de,dmneighborranks[p]);CHKERRQ(ierr);
    }
  }
  ierr = DataExTopologyFinalize(de);CHKERRQ(ierr);
  
  ierr = DataExInitializeSendCount(de);CHKERRQ(ierr);
  for (p=0; p<neighbour_cells; p++) {
		if ( (dmneighborranks[p] >= 0) && (dmneighborranks[p] != rank) ) {
      ierr = DataExAddToSendCount(de,dmneighborranks[p],1);CHKERRQ(ierr);
    }
  }
  ierr = DataExFinalizeSendCount(de);CHKERRQ(ierr);
  
  
  /* send bounding boxes */
  ierr = DataExPackInitialize(de,sizeof_bbox_ctx);CHKERRQ(ierr);
  for (p=0; p<neighbour_cells; p++) {
    nrank = dmneighborranks[p];
		if ( (nrank >= 0) && (nrank != rank) ) {
      /* insert bbox buffer into DataExchanger */
      ierr = DataExPackData(de,nrank,1,bbox);CHKERRQ(ierr);
    }
  }
  ierr = DataExPackFinalize(de);CHKERRQ(ierr);
  
  /* recv bounding boxes */
  ierr = DataExBegin(de);CHKERRQ(ierr);
  ierr = DataExEnd(de);CHKERRQ(ierr);

  ierr = DataExGetRecvData(de,&n_bbox_recv,(void**)&recv_bbox);CHKERRQ(ierr);

  
  for (p=0; p<n_bbox_recv; p++) {
    printf("[rank %d]: box from %d : range[%+1.4e,%+1.4e]x[%+1.4e,%+1.4e]\n",rank,recv_bbox[p].owner_rank,
           recv_bbox[p].min[0],recv_bbox[p].max[0],recv_bbox[p].min[1],recv_bbox[p].max[1]);
  }
  
  /* of course this is stupid as this "generic" function should have a better way to know what the coordinates are called */
  ierr = DataExInitializeSendCount(de);CHKERRQ(ierr);
  for (pk=0; pk<n_bbox_recv; pk++) {
    PetscReal *array_x,*array_y;
    
    ierr = DMSwarmGetField(dm,"coorx",NULL,NULL,(void**)&array_x);CHKERRQ(ierr);
    ierr = DMSwarmGetField(dm,"coory",NULL,NULL,(void**)&array_y);CHKERRQ(ierr);
    
    for (p=0; p<npoints; p++) {
      if ((array_x[p] >= recv_bbox[pk].min[0]) && (array_x[p] <= recv_bbox[pk].max[0]) ) {
        if ((array_y[p] >= recv_bbox[pk].min[1]) && (array_y[p] <= recv_bbox[pk].max[1]) ) {

          ierr = DataExAddToSendCount(de,recv_bbox[pk].owner_rank,1);CHKERRQ(ierr);
        
        }
      }
    }
    ierr = DMSwarmRestoreField(dm,"coory",NULL,NULL,(void**)&array_y);CHKERRQ(ierr);
    ierr = DMSwarmRestoreField(dm,"coorx",NULL,NULL,(void**)&array_x);CHKERRQ(ierr);
  }
  ierr = DataExFinalizeSendCount(de);CHKERRQ(ierr);


  
  ierr = DataBucketCreatePackedArray(swarm->db,&sizeof_dmswarm_point,&point_buffer);CHKERRQ(ierr);
  ierr = DataExPackInitialize(de,sizeof_dmswarm_point);CHKERRQ(ierr);
  for (pk=0; pk<n_bbox_recv; pk++) {
    PetscReal *array_x,*array_y;

    ierr = DMSwarmGetField(dm,"coorx",NULL,NULL,(void**)&array_x);CHKERRQ(ierr);
    ierr = DMSwarmGetField(dm,"coory",NULL,NULL,(void**)&array_y);CHKERRQ(ierr);

    for (p=0; p<npoints; p++) {
      if ((array_x[p] >= recv_bbox[pk].min[0]) && (array_x[p] <= recv_bbox[pk].max[0]) ) {
        if ((array_y[p] >= recv_bbox[pk].min[1]) && (array_y[p] <= recv_bbox[pk].max[1]) ) {
          // copy point into buffer //
          ierr = DataBucketFillPackedArray(swarm->db,p,point_buffer);CHKERRQ(ierr);
          // insert point buffer into DataExchanger //
          ierr = DataExPackData(de,recv_bbox[pk].owner_rank,1,point_buffer);CHKERRQ(ierr);
        }
      }
    }
  
    ierr = DMSwarmRestoreField(dm,"coory",NULL,NULL,(void**)&array_y);CHKERRQ(ierr);
    ierr = DMSwarmRestoreField(dm,"coorx",NULL,NULL,(void**)&array_x);CHKERRQ(ierr);
  }

  ierr = DataExPackFinalize(de);CHKERRQ(ierr);
  
  ierr = DMSwarmRestoreField(dm,"DMSwarm_rank",NULL,NULL,(void**)&rankval);CHKERRQ(ierr);
  
  ierr = DataExBegin(de);CHKERRQ(ierr);
  ierr = DataExEnd(de);CHKERRQ(ierr);
  
  ierr = DataExGetRecvData(de,&n_points_recv,(void**)&recv_points);CHKERRQ(ierr);
  
	ierr = DataBucketGetSizes(swarm->db,&npoints,NULL,NULL);CHKERRQ(ierr);
	ierr = DataBucketSetSizes(swarm->db,npoints + n_points_recv,-1);CHKERRQ(ierr);
	for (p=0; p<n_points_recv; p++) {
    void *data_p = (void*)( (char*)recv_points + p*sizeof_dmswarm_point );
    
    ierr = DataBucketInsertPackedArray(swarm->db,npoints+p,data_p);CHKERRQ(ierr);
  }

  ierr = DataBucketDestroyPackedArray(swarm->db,&point_buffer);CHKERRQ(ierr);
  PetscFree(bbox);
  ierr = DataExView(de);CHKERRQ(ierr);
  ierr = DataExDestroy(de);CHKERRQ(ierr);
  
  PetscFunctionReturn(0);
}

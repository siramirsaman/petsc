#ifndef lint
static char vcid[] = "$Id: regmpimat.c,v 1.3 1996/12/03 17:58:26 curfman Exp $";
#endif

/* 
   Implements parallel discrete function utilities for regular grids.

   This file contains code that is merely used while PETSc is
   in a state of transition.  Eventually, PETSc will directly 
   support this functionality and this code will be obselete.

   This code is NOT intended to be a model of appropriate
   application code!
*/

#include "src/dfvec/dfvimpl.h"
#include "src/mat/impls/aij/mpi/mpiaij.h"
#include "src/da/daimpl.h"
#include "draw.h"

/*
  MatViewDFVec_MPIAIJ - Views a parallel MATMPIAIJ matrix in the
  ordering that would be used if a single processor were used.
 */
int MatViewDFVec_MPIAIJ(Mat mat,DFVec dfv,Viewer viewer)
{
  Mat_MPIAIJ  *aij = (Mat_MPIAIJ *) mat->data;
  Mat_SeqAIJ* C = (Mat_SeqAIJ*)aij->A->data;
  int         ierr, format,shift = C->indexshift,rank, grow;
  FILE        *fd;
  ViewerType  vtype;

  ierr = ViewerGetType(viewer,&vtype); CHKERRQ(ierr);
  if (vtype  == ASCII_FILES_VIEWER || vtype == ASCII_FILE_VIEWER) { 
    ierr = ViewerGetFormat(viewer,&format);
    if (format == VIEWER_FORMAT_ASCII_INFO_LONG) {
      MatInfo info;
      int     flg;
      MPI_Comm_rank(mat->comm,&rank);
      ierr = ViewerASCIIGetPointer(viewer,&fd); CHKERRQ(ierr);
      ierr = MatGetInfo(mat,MAT_LOCAL,&info); 
      ierr = OptionsHasName(PETSC_NULL,"-mat_aij_no_inode",&flg); CHKERRQ(ierr);
      PetscSequentialPhaseBegin(mat->comm,1);
      if (flg) fprintf(fd,"[%d] Local rows %d nz %d nz alloced %d mem %d, not using I-node routines\n",
         rank,aij->m,(int)info.nz_used,(int)info.nz_allocated,(int)info.memory);
      else fprintf(fd,"[%d] Local rows %d nz %d nz alloced %d mem %d, using I-node routines\n",
         rank,aij->m,(int)info.nz_used,(int)info.nz_allocated,(int)info.memory);
      ierr = MatGetInfo(aij->A,MAT_LOCAL,&info);
      fprintf(fd,"[%d] on-diagonal part: nz %d \n",rank,(int)info.nz_used);
      ierr = MatGetInfo(aij->B,MAT_LOCAL,&info); 
      fprintf(fd,"[%d] off-diagonal part: nz %d \n",rank,(int)info.nz_used); 
      fflush(fd);
      PetscSequentialPhaseEnd(mat->comm,1);
      ierr = VecScatterView(aij->Mvctx,viewer); CHKERRQ(ierr);
      return 0; 
    }
    else if (format == VIEWER_FORMAT_ASCII_INFO) {
      return 0;
    }
  }

  if (vtype == DRAW_VIEWER) {
    Draw       draw;
    PetscTruth isnull;
    ierr = ViewerDrawGetDraw(viewer,&draw); CHKERRQ(ierr);
    ierr = DrawIsNull(draw,&isnull); CHKERRQ(ierr); if (isnull) return 0;
  }

  if (vtype == ASCII_FILE_VIEWER) {
    ierr = ViewerASCIIGetPointer(viewer,&fd); CHKERRQ(ierr);
    PetscSequentialPhaseBegin(mat->comm,1);
    fprintf(fd,"[%d] rows %d starts %d ends %d cols %d starts %d ends %d\n",
           aij->rank,aij->m,aij->rstart,aij->rend,aij->n,aij->cstart,
           aij->cend);
    ierr = MatView(aij->A,viewer); CHKERRQ(ierr);
    ierr = MatView(aij->B,viewer); CHKERRQ(ierr);
    fflush(fd);
    PetscSequentialPhaseEnd(mat->comm,1);
  }
  else {
    /* int size = aij->size; */
    rank = aij->rank;
    /*    if (size == 1) {
      ierr = MatView(aij->A,viewer); CHKERRQ(ierr);
    }
    else {
    */
    {
      /* assemble the entire matrix onto first processor. */
      Mat         A;
      Mat_SeqAIJ *Aloc;
      int         M = aij->M, N = aij->N,m,*ai,*aj,row,*cols,i,*ct,*gtog1;
      Scalar      *a;
      DF          df;

      ierr = DFVecGetDFShell(dfv,&df); CHKERRQ(ierr);
      ierr = DAGetGlobalToGlobal1_Private(df->da_user,&gtog1); CHKERRQ(ierr);

      if (!rank) {
	/* ierr = MatCreateMPIAIJ(mat->comm,M,N,M,N,0,PETSC_NULL,0,PETSC_NULL,&A);
               CHKERRQ(ierr); */
        ierr = MatCreateMPIAIJ(mat->comm,M,N,M,N,35,PETSC_NULL,35,PETSC_NULL,&A);
               CHKERRQ(ierr);
      }
      else {
        ierr = MatCreateMPIAIJ(mat->comm,0,0,M,N,0,PETSC_NULL,0,PETSC_NULL,&A);
               CHKERRQ(ierr);
      }
      PLogObjectParent(mat,A);

      /* copy over the A part */
      Aloc = (Mat_SeqAIJ*) aij->A->data;
      m = Aloc->m; ai = Aloc->i; aj = Aloc->j; a = Aloc->a;
      row = aij->rstart;
      ct = cols = (int *) PetscMalloc( (ai[m]+1)*sizeof(int) ); CHKPTRQ(cols);
      for ( i=0; i<ai[m]+shift; i++ ) {cols[i] = gtog1[aj[i] + aij->cstart + shift];}
      for ( i=0; i<m; i++ ) {
        grow = gtog1[row];
        ierr = MatSetValues(A,1,&grow,ai[i+1]-ai[i],cols,a,INSERT_VALUES); CHKERRQ(ierr);
        row++; a += ai[i+1]-ai[i]; cols += ai[i+1]-ai[i];
      } 
      aj = Aloc->j;

      /* copy over the B part */
      Aloc = (Mat_SeqAIJ*) aij->B->data;
      m = Aloc->m;  ai = Aloc->i; aj = Aloc->j; a = Aloc->a;
      row = aij->rstart;
      cols = ct;
      for ( i=0; i<ai[m]+shift; i++ ) {cols[i] = gtog1[aij->garray[aj[i]+shift]];}
      for ( i=0; i<m; i++ ) {
        grow = gtog1[row];
        ierr = MatSetValues(A,1,&grow,ai[i+1]-ai[i],cols,a,INSERT_VALUES);CHKERRQ(ierr);
        row++; a += ai[i+1]-ai[i]; cols += ai[i+1]-ai[i];
      } 
      /*      PetscFree(ct); */
      ierr = MatAssemblyBegin(A,MAT_FINAL_ASSEMBLY); CHKERRQ(ierr);
      ierr = MatAssemblyEnd(A,MAT_FINAL_ASSEMBLY); CHKERRQ(ierr);
      if (!rank) {
        ierr = MatView(((Mat_MPIAIJ*)(A->data))->A,viewer); CHKERRQ(ierr);
      }
      ierr = MatDestroy(A); CHKERRQ(ierr);
    }
  }
  return 0;
}





/*
    Scatters from a parallel vector to a sequential vector.
   Does case when we are merely selecting the local part of the 
   parallel vector.
*/
#include "petsc.h"
#include "comm.h"
#include "is.h"
#include "vec.h"
#include "sys.h"
#include "options.h"
#include "sysio.h"
#include <math.h>

int main(int argc,char **argv)
{
  int           n = 5, ierr;
  int           numtids,mytid,i;
  Scalar        one = 1.0, two = 2.0, three = 3.0, value;
  double        norm;
  Vec           x,y;
  IS            is1,is2;
  VecScatterCtx ctx = 0;

  MPI_Init(&argc,&argv); 
  MPI_Comm_size(MPI_COMM_WORLD,&numtids);
  MPI_Comm_rank(MPI_COMM_WORLD,&mytid);

  OptionsCreate(argc,argv,(char*)0,(char*)0);

  /* create two vectors */
  ierr = VecCreateMPI(MPI_COMM_WORLD,-1,numtids*n,&x); CHKERR(ierr);
  ierr = VecCreateSequential(n,&y); CHKERR(ierr);

  /* create two index sets */
  ierr = ISCreateStrideSequential(n,n*mytid,1,&is1); CHKERR(ierr);
  ierr = ISCreateStrideSequential(n,0,1,&is2); CHKERR(ierr);

  for ( i=n*mytid; i<n*(mytid+1); i++ ) {
    value = (Scalar) i;
    ierr = VecInsertValues(x,1,&i,&value); CHKERR(ierr);
  }
  ierr = VecBeginAssembly(x); CHKERR(ierr);
  ierr = VecEndAssembly(x); CHKERR(ierr);

  VecView(x,0); printf("----\n");

  ierr = VecScatterBegin(x,is1,y,is2,&ctx); CHKERR(ierr);
  ierr = VecScatterEnd(x,is1,y,is2,&ctx); CHKERR(ierr);
  VecScatterCtxDestroy(ctx);
  
  VecView(y,0);

  ierr = VecDestroy(x);CHKERR(ierr);
  ierr = VecDestroy(y);CHKERR(ierr);

  MPI_Finalize(); 
  return 0;
}
 

/*
 * <AUTO>
 *
 * FILE: utils.c
 *
 * DESCRIPTION:
 * This file contains lots of disparate functions that, in most cases,
 * have little to do with each other.  The idea is that one can find
 * utility functions of general interest collected together here.
 *
 * </AUTO>
 */

/*
 * For now, we'll put contributed code here
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <alloca.h>
#include <unistd.h>
#include <errno.h>

#include "dervish.h"
#include "phConsts.h"
//#include "phDervishUtils.h"		/* utilities which will one day be
//in dervish */
//#include "phChainDiff.h"
#include "phRandom.h"
#include "phExtract.h"
#include "phSkyUtils.h"
//#include "prvt/region_p.h"
#include "phVariablePsf.h"		/* for ACOEFF */
#include "phMeschach.h"
#include "phUtils.h"

/*****************************************************************************/
/*
 * Allocate some memory that can be made available in extremis.  This is
 * achieved via a shMemEmptyCB callback
 */
static void *strategic_memory_reserve = NULL; /* reserved memory, to be released when we run out */
static int allocated_reserve = 0;	/* did we allocate a reserve? */


/*
 * Do we have memory saved for later use?
 */
int
phStrategicMemoryReserveIsEmpty(void)
{
    return (allocated_reserve && strategic_memory_reserve == NULL) ? 1 : 0;
}



#if 0
   /* 
    * these are private, faster functions called by the general-purpose
    * (and global) phRegStatsSigmaClip function.
    */
static int
phRegStatsSigmaClipU16(REGION *reg, char mask_flag, int iter, float clipsig,
                      float *mean, float *stdev);

static int
phRegStatsSigmaClipFL32(REGION *reg, char mask_flag, int iter, float clipsig,
                      float *mean, float *stdev);

/*****************************************************************************/

const char *
phPhotoName(void)
{
   static char *name = "$Name$";

   return(name);
}

/***************************************************************************
 * <AUTO EXTRACT>
 *
 * ROUTINE: get_FtclOpts 
 * 
 * DESCRIPTION: 
 * This is a utility function for calling the ftcl_ParseArgv and doing most
 * of the book-keeping. It can be called in place of ftcl_ParseArgv, without
 * setting flags or processing error returns.
 *
 * return: TCL_OK              if all goes well
 *         TCL_ERROR           if not
 *
 * </AUTO>
 */

int
get_FtclOpts(
	     Tcl_Interp *interp,     /* I: TCL interpreter */
	     int *ac,                /* I: number of args in command line */
	     char **av,              /* I: one string per argument */
	     ftclArgvInfo *opts      /* I?: command option switches? */
	     )
{
   char *cmd = av[0];
   int flags = FTCL_ARGV_NO_LEFTOVERS;
   int status;


   Tcl_SetResult(interp,av[0], TCL_STATIC);
   Tcl_AppendResult(interp,": ",NULL);
   status = ftcl_ParseArgv(interp, ac, av, opts, flags);

   if(status == FTCL_ARGV_BADSYNTAX) {
      Tcl_AppendResult(interp,
		       ftcl_GetUsage(interp,opts,flags,cmd,0,"\nUsage:","\n"),
		       NULL);
      return(TCL_ERROR);
   } else if(status == FTCL_ARGV_GIVEHELP) {
      Tcl_SetResult(interp,
		    ftcl_GetUsage(interp,opts,flags,cmd,0,"\nUsage:","\n"),
		    TCL_VOLATILE);
      Tcl_AppendResult(interp,ftcl_GetArgInfo(interp,opts,flags,cmd,8),NULL);
      return(TCL_ERROR);
   } else {
      Tcl_ResetResult(interp);		/* remove the av[0] */
   }

   return(TCL_OK);
}

#if !(DERVISH_VERSION >= 6 && DERVISH_MINOR_VERSION >= 8)
/*
 * This code is lifted from shChain.c, where it is $ifdef 0. The concern
 * voiced below is somewhat valid, but not very. A user should call this
 * only when it's known to be safe, and if they don't call this they'll
 * simply code their own equivalent. RHL
 *
 * **** NOTE ****
 *
 *    This function should not be made availaible yet. It suffers from the
 *    fact that our objects themselves do not maintain referential integrity.
 *    Thus if an object is on two chains, and the user gets a pointer to an
 *    object on one chain, removes it from that chain, and subsequently
 *    deletes it by calling it's destructor, then the object is really and
 *    truly deleted. Thus making the pointer on the other chain invalid. Our
 *    object destructor routines should handle the job of making sure that 
 *    the object does not have any outstanding references before deleteing
 *    it. Until the object destructors have been modified to do so, this
 *    routine should not be used.
 *    
 *    - vijay
 *
 * **** END NOTE ****
 *
 * ROUTINE: shChainDestroy()
 * 
 * DESCRIPTION:
 *   shChainDestroy() deletes the specified chain and all objects on the chain
 *   as well. The memory for the CHAIN itself and all the CHAIN_ELEMs will be
 *   deleted. Also deleted is each element (pElement) on a CHAIN_ELEM.
 *
 * CALL:
 *   (int) shChainDestroy(CHAIN *pChain, void (*pDelFunc)(void *))
 *   pChain   - Pointer to the chain to be deleted
 *   pDelFunc - Pointer to a function which deletes each element on the chain.
 *              This function is passed the address of the element.
 *
 * RETURNS:
 *   0 : On success
 *   1 : Otherwise - for instance, if a chain's type is GENERIC, this function
 *       cannot delete objects on the chain.
 */
int
shChainDestroy(CHAIN *pChain, void (*pDelFunc)(void *))
{
   CHAIN_ELEM  *pElement,
               *pTmpElement;
   void        *pObject;     /* To be deleted */

   shAssert(pChain != NULL);

   if (pChain->type == shTypeGetFromName("GENERIC"))
       return 0;

   pElement = pChain->pFirst;
   while (pElement != NULL)  {
       pObject = pElement->pElement;
    
       if(p_shMemRefCntrGet(pObject) > 1) {
	  shMemRefCntrDecr(pObject);
       } else {
	  pDelFunc(pObject);
       }

       pTmpElement = pElement->pNext;
       shFree(pElement);
       pElement = pTmpElement;
   }

   shFree(pChain);

   return 0;
}
#endif

/***************************************************************************
 * <AUTO EXTRACT>
 *
 * ROUTINE: shMasksMerge  
 * 
 * DESCRIPTION: 
 * Given two MASKs, apply any necessary offset to match their origins
 * and then, for any pixel in which 'mask1' is not zero,
 * logically OR the 'mask2' element with 'val'.
 *
 * return: SH_SUCCESS          if all goes well
 *         SH_GENERIC_ERROR    if not
 *
 * </AUTO>
 */

RET_CODE
shMasksMerge(
           MASK *mask1,                 /* merge this mask */
           MASK *mask2,                 /* into this one */
           int val                      /* val to set mask to */
           )
{
   int col0,coln;                       /* first and last col of new region */
   int row0,rown;                       /* first and last row of new region */
   int c,r;
   unsigned char *ptr1,*ptr2;

   if(mask1 == NULL) {
      shError("mergeMasks: given a NULL mask to merge");
      return(SH_GENERIC_ERROR);
   }
   if(mask2 == NULL) {
      shError("mergeMasks: given a NULL mask to merge into");
      return(SH_GENERIC_ERROR);
   }

   col0 = (mask1->col0 > mask2->col0) ? mask1->col0 : mask2->col0;
   coln = (mask1->col0 + mask1->ncol < mask2->col0 + mask2->ncol) ?
                mask1->col0 + mask1->ncol - 1 : mask2->col0 + mask2->ncol - 1;
   row0 = (mask1->row0 > mask2->row0) ? mask1->row0 : mask2->row0;
   rown = (mask1->row0 + mask1->nrow < mask2->row0 + mask2->nrow) ?
                mask1->row0 + mask1->nrow - 1 : mask2->row0 + mask2->nrow - 1;


   if(col0 > coln || row0 > rown) {
      shError("mergeMasks: masks %s and %s don't overlap",
                 mask1->name,mask2->name);
      return(SH_GENERIC_ERROR);
   }

   for(r = row0;r < rown;r++) {
      ptr1 = &mask1->rows[r - mask1->row0][-mask1->col0];
      ptr2 = &mask2->rows[r - mask2->row0][-mask2->col0];
      for(c = col0;c < coln;c++) {
         if(ptr1[c]) ptr2[c] |= val;
      }
   }
   return(SH_SUCCESS);
}

/***************************************************************************
 * <AUTO EXTRACT>
 *
 * ROUTINE: shMasksMergeWithOffset 
 * 
 * DESCRIPTION: 
 * Merge masks, but with an integer offset. The (0,0) pixel of
 * mask1 gets merged to the (mask1->row0 + drow,mask1->col0 + dcol)
 * pixel of mask2
 *
 * return: SH_SUCCESS          if all goes well
 *         SH_GENERIC_ERROR    if not
 *
 * </AUTO>
 */

int
shMasksMergeWithOffset(
		       MASK *mask1,                 /* merge this mask */
		       MASK *mask2,                 /* into this one */
		       int val,                     /* val to set mask to */
		       int dcol,                    /* offsets of mask1;
						       column */
		       int drow                     /* and row */
           )
{
   int c,r;
   unsigned char *ptr1,*ptr2;

   if(mask1 == NULL) {
      shError("mergeMasks: given a NULL mask to merge");
      return(SH_GENERIC_ERROR);
   }
   if(mask2 == NULL) {
      shError("mergeMasks: given a NULL mask to merge into");
      return(SH_GENERIC_ERROR);
   }
   if((mask1->row0 + drow < 0) || (mask1->row0 + drow >= mask2->nrow) ||
      (mask1->col0 + dcol < 0) || (mask1->col0 + dcol >= mask2->ncol)) {
      shError("mergeMasks: merged mask's row/col is out of bounds");
      return(SH_GENERIC_ERROR);
   }
   if((mask1->row0 + drow + mask1->nrow > mask2->nrow) ||
      (mask1->col0 + dcol + mask1->ncol > mask2->ncol)) {
      shError("mergeMasks: merged mask's ending row/col is out of bounds");
      return(SH_GENERIC_ERROR);
   }

   for(r = 0;r < mask1->nrow;r++) {
      ptr1 = mask1->rows[r];
      ptr2 = &mask2->rows[r + mask1->row0 + drow][mask1->col0 + dcol];
      for(c = 0;c < mask1->ncol;c++) {
         if(ptr1[c]) ptr2[c] |= val;
      }
   }
   return(SH_SUCCESS);
}

/***************************************************************************
 * <AUTO EXTRACT>
 *
 * ROUTINE: shMasksOverlap 
 * 
 * DESCRIPTION: 
 * See if two masks overlap.
 * The (col0,row0) pixel of mask1 gets compared to the
 * (col0 + dcol,row0 + drow) pixel of mask2 
 *
 * return: 1                   if masks do overlap
 *         0                   if not
 *
 * </AUTO>
 */

int
shMasksOverlap(
	       MASK *mask1,		/* merge this mask */
	       MASK *mask2,		/* into this one */
	       int dcol,		/* offsets of mask1; column */
	       int drow			/* and row */
	       )
{
   int c,r;
   int col0,coln,row0,rown;
   int col20,row20;			/* col0 and row0 for mask2, after
					   allowing for dcol,drow */
   unsigned char *ptr1,*ptr2;

   col20 = mask2->col0 + dcol;
   row20 = mask2->row0 + drow;

   col0 = (mask1->col0 > col20) ? mask1->col0 : col20;
   coln = (mask1->col0 + mask1->ncol < col20 + mask2->ncol) ?
			     			mask1->col0 + mask1->ncol :
						col20 + mask2->ncol;
      
   row0 = (mask1->row0 > row20) ? mask1->row0 : row20;
   rown = (mask1->row0 + mask1->nrow < row20 + mask2->nrow) ?
						mask1->row0 + mask1->nrow :
						row20 + mask2->nrow;

   for(r = row0;r < rown;r++) {
      ptr1 = &mask1->rows[r - mask1->row0][-mask1->col0];
      ptr2 = &mask2->rows[r - row20][-col20];
      for(c = col0;c < coln;c++) {
	 if(ptr1[c] && ptr2[c]) {
	    return(1);
	 }
      }
   }
   return(0);
}

/************ BEGIN shRegGetEnlarged.c ***************************************/

/***************************************************************************
 * <AUTO EXTRACT>
 *
 * ROUTINE: shRegGetEnlarged 
 * 
 * DESCRIPTION: 
 * Return a new region that might be larger than some old one (thus we can't
 * call shSubRegNew), centered on the given row and col, with a total size
 * given by nrow and ncol args. There are several options for handling problem
 * cases when the enlarged area exceeds image boundaries.
 *     flags == ENLARGE_EXACT     return NULL if exact enlargement can't be
 *                                   created, due to boundary problems
 *     flags != ENLARGE_EXACT     return the "best" fit to the desired
 *                                   enlarged area, even if not exact
 * In all cases, a pointer to the new region is returned if possible, or
 * NULL if not.
 *
 * This implementation uses the function "parentRegion()" to find the 
 * parent of a given region; it needs to know how big the parent is.
 *
 * return: REGION * to enlarged REGION       if all goes well
 *         NULL                              if not
 *
 * </AUTO>
 */

REGION *
shRegGetEnlarged(
	   char *name,			/* name of new region */
	   REGION *region,		/* REGION to be copied */
	   int row,			/* center row in new region */
	   int col,			/* center col in new region */
	   int nrow,			/* number of rows in new region */
	   int ncol,			/* number of cols in new region */
	   REGION_FLAGS flags	/* controls error handling */
	   )
{
   int srow, scol, erow, ecol;
   REGION *reg, *new;
   
	shAssert(name != NULL);
	shAssert(region != NULL);

	if ((reg = parentRegion(region)) == NULL)
		reg = region;

	srow = row - ((nrow + 1)/2);
	erow = srow + nrow;
	scol = col - ((ncol + 1)/2);
	ecol = scol + ncol;

   shDebug(10, "shRegGetEnlarged: want sr %4d er %4d  sc %4d ec %4d", 
	 srow, erow, scol, ecol);
   shDebug(10, "shRegGetEnlarged: reg  sr %4d er %4d  sc %4d ec %4d", 
	 reg->row0, reg->row0 + reg->nrow, reg->col0, reg->col0 + reg->ncol);
	
	if ((srow >= reg->row0) && (erow < reg->row0 + reg->nrow) &&
	    (scol >= reg->col0) && (ecol < reg->col0 + reg->ncol)) {
		/* no problem at all */
	       srow -= reg->row0;
	       scol -= reg->col0;
	       if ((new = shSubRegNew(name, reg, nrow, ncol, srow, scol, 
		     COPY_MASK)) == NULL) {
		  shError("shRegGetEnlarged: shSubRegNew returns with error");
		  return(NULL);
	       }
	       new->row0 += reg->row0;
	       new->col0 += reg->col0;
	       return(new);
	}

	/* uh-oh, one of the boundary conditions failed.  If the user requested
	   an EXACT copy, the return NULL as an indication of failure. */
	if (flags & ENLARGE_EXACT)
		return(NULL);

   shDebug(10, "shRegGetEnlarged: cutting down");

	/* user is willing to accept our attempt at picking the "best" compromise
	   for the enlarged region.  For "best", let's use "region which would
	   begin or end where the desired enlarged area would", and simply cut
	   the region short wherever it exceeds the parent's boundary.  That is,
	   given a request for a 11x11 region centered on (2, 3), we'll return
	   a 8x9 region going (0-7, 0-8), simply ignoring the portion of the
	   11x11 region that falls off the image. */
	if (srow < reg->row0)
		srow = reg->row0;
	if (scol < reg->col0)
		scol = reg->col0;
	if (erow >= reg->row0 + reg->nrow)
		erow = reg->row0 + reg->nrow - 1;
	if (ecol >= reg->col0 + reg->ncol)
		ecol = reg->col0 + reg->ncol - 1;
	nrow = (erow - srow) + 1;
	ncol = (ecol - scol) + 1;

   shDebug(10, "shRegGetEnlarged: try  sr %4d er %4d  sc %4d ec %4d", 
	 srow, erow, scol, ecol);

	/* and now we can return the result of asking for the calculated, "best"
	   match to the desired region */
   srow -= reg->row0;
   scol -= reg->col0;
	if ((new = shSubRegNew(name, reg, nrow, ncol, srow, scol, 
	       COPY_MASK)) == NULL) {
	   shError("shRegGetEnlarged: shSubRegNew still returns with error");
	   return(NULL);
        }
        new->row0 += reg->row0;
        new->col0 += reg->col0;
	return(new);
}

/*****************************************************************************/

#ifndef DEBUG
#undef DEBUG		/* define for diagnostic output */
#endif

#ifndef DEBUG2
#undef DEBUG2		/* define for LOTS of output */
#endif 

/***************************************************************************
 * <AUTO EXTRACT>
 *
 * This routine turns a tcl array of handles into a pointer to an shMalloced
 * array of pointers to the objects represented by the tcl handles.
 *
 * return: SH_SUCCESS              if all goes well
 *         SH_GENERIC_ERROR        if not
 *
 */
int
phPtrArrayGetFromTclArrayOfHandles(
   Tcl_Interp *interp,
   char *arrayName,			/* name of tcl array */
   char *indices,			/* list of indices of tcl array */
   char *strtype,			/* type (e.g. REGION) of elements */
   void ***array,			/* C array returned - must be
					   freed with shFree */
   int *nele				/* O: number of array elements */
				   )
{
   int argc;
   char **argv;
   int i;
   void *address;
   char *strhandle;

   /* Make the ascii list accessible to C */
   if(Tcl_SplitList(interp, indices, &argc, &argv) == TCL_ERROR) {
      shErrStackPush("Error parsing input list of indices");
      return(SH_GENERIC_ERROR);
   }
   *nele = argc;

   /* Allocate the array of pointers */
   *array = (void **)shMalloc(argc*sizeof(void *));

   /* Get each array element and put it on the C array */
   for (i=0; i<argc; i++) {
      strhandle = Tcl_GetVar2(interp,arrayName,argv[i],0);
      if(strhandle == NULL ||
	 shTclAddrGetFromName (interp, strhandle,&address,strtype)
							 == SH_GENERIC_ERROR) {
         shFree(*array);
	 free(argv);
	 shErrStackPush("Error getting address for array element %s(%s)",
							    arrayName,argv[i]);
         return(SH_GENERIC_ERROR);
      }
      (*array)[i] = address;
   }

   free(argv);

   return(SH_SUCCESS);
}

/*****************************************************************************/
/*
 * <AUTO EXTRACT>
 *
 * Given the name of a TCL array, generate an array of floats corresponding
 * to its elements, and return the number of elements (or -1 in case of error)
 *
 * Do not forget to shFree *vals when you are done with it
 */
int
phFltArrayGetFromTclArray(Tcl_Interp *interp,
			  char *arrayName, /* name of tcl array */
			  char *indices, /* indices of tcl array */
			  float **vals)	/* array to allocate and return */
{
   int ac;
   char **av;
   double dbl;
   int i;
   char *ptr;
   
   /* Make the ascii list accessible to C */
   if(Tcl_SplitList(interp, indices, &ac, &av) == TCL_ERROR) {
      shErrStackPush("Error parsing input list of indices");
      return(-1);
   }

   *vals = shMalloc(ac*sizeof(float));
/*
 * read all those variables, and convert to float
 */
   for(i = 0;i < ac;i++) {
      if((ptr = Tcl_GetVar2(interp, arrayName, av[i], 0)) == NULL) {
	 shErrStackPush("phFltArrayGetFromTclArray: %s(%s) doesn't exist",
							    arrayName,av[i]);
	 free(av);
	 shFree(*vals); *vals = NULL;
	 return(-1);
      }

      if(Tcl_GetDouble(interp, ptr, &dbl) == TCL_OK) {
	 (*vals)[i] = dbl;
      } else {
	 free(av);
	 shFree(*vals); *vals = NULL;

	 return(-1);
      }
   }

   free(av);
   
   return(ac);
}

/*****************************************************************************/
/*
 * <AUTO EXTRACT>
 *
 * Given a TCL list, generate an array of floats corresponding to its
 * elements, and return the number of elements (or -1 in case of error)
 *
 * Do not forget to shFree *vals when you are done with it
 */
int
phFltArrayGetFromTclList(Tcl_Interp *interp,
			 char *list,	/* tcl list */
			 float **vals,	/* array to allocate and return */
			 int nel)	/* number of elements, or -1 */
{
   int ac;
   char **av;
   double dbl;				/* used for parsing values */
   int i;
/*
 * split up the ascii list
 */
   if(Tcl_SplitList(interp, list, &ac, &av) == TCL_ERROR) {
      shErrStackPush("Error parsing input list");
      return(-1);
   }
/*
 * check number of elements, if specified
 */
   if(nel >= 0 && ac != nel) {
      char buff[10];
      sprintf(buff, "%d", nel);
      Tcl_AppendResult(interp, "Expected ", buff, " elements,", (char *)NULL);
      sprintf(buff, "%d", ac);
      Tcl_AppendResult(interp, " saw ", buff, ".", (char *)NULL);
   }
/*
 * read all those variables, and convert to float
 */
   *vals = shMalloc(ac*sizeof(float));
   for(i = 0;i < ac;i++) {
      if(Tcl_GetDouble(interp, av[1], &dbl) == TCL_OK) {
	 (*vals)[i] = dbl;
      } else {
	 free(av);
	 shFree(*vals); *vals = NULL;

	 return(-1);
      }
   }

   free(av);
   
   return(ac);
}

/*****************************************************************************/
/*
 * <AUTO EXTRACT>
 *
 * Given a C array of pointers, define a TCL array of handles
 * to those objects
 *
 * return: SH_SUCCESS              if all goes well
 *         SH_GENERIC_ERROR        if not. In this case, some allocated handles
 *				   memory may not be freed.
 */
int
phTclArrayOfHandlesGetFromCArray(Tcl_Interp *interp,
				 void **Carray, /* C array to export */
				 int nele, /* dimen of Carray */
				 const char *type, /* type (e.g. REGION) of
						      elements */
				 const char *TCLarray, /* name of tcl array */
				 const char **indices) /*indices for TCLarray*/
{
   HANDLE hand;
   int i;
   char hname[HANDLE_NAMELEN];
   char varname[100];			/* name of variable to set */

   for(i = 0; i < nele; i++) {
      if(p_shTclHandleNew(interp,hname) != TCL_OK) {
	 shTclInterpAppendWithErrStack(interp);
	 return(SH_GENERIC_ERROR);
      }
      hand.ptr = Carray[i];
      hand.type = shTypeGetFromName((char *)type); /* cast away const */
   
      if(p_shTclHandleAddrBind(interp,hand,hname) != TCL_OK) {
	 Tcl_SetResult(interp,"can't bind to new TYPE handle",TCL_STATIC);
	 return(SH_GENERIC_ERROR);
      }

      sprintf(varname,"%s(%s)", TCLarray, indices[i]);
      if(Tcl_SetVar(interp, varname, hname, 0) == NULL) {
	 Tcl_SetResult(interp,"can't set new TYPE handle in a TCL array",
								   TCL_STATIC);
	 return(SH_GENERIC_ERROR);
      }
   }

   return(SH_SUCCESS);
}

/*****************************************************************************/

static void free_script(EDIT *membase);
static int chain_to_array(CHAIN *l, void **arr, int size);
static EDIT *chain_prepare(EDIT *script);

/*
 * these variables have to be extern as they are calculated by shChainDiff
 * but the results are extracted by over functions. They are all freed
 * by calling shChainDiffFree when you are done.
 */
static void **chain1 = NULL, **chain2 = NULL; /* elements of lists */
static EDIT *membase = NULL;		/* base of list of all EDIT nodes;
					   This is used to manage memory */
/***************************************************************************
 * <AUTO EXTRACT>
 *
 * ROUTINE: shChainDiff 
 * 
 * DESCRIPTION: 
 * Compare two chains. The algorithm used is due to Eugene Meyers,
 * Algorithmica V1 p251 (1986); the code is modified from that presented
 * by Miller and Myers in Software -- Practice and Experience V15 p1025 (1085)
 *
 * The latter paper gives a very clear explanation of the elegant algorithm
 *
 * return: EDIT * to differences            if all goes well
 *         NULL                             if not
 *
 * </AUTO>
 */

#define MAXLINES 2000			/* max number of members in a list */
#define ORIGIN MAXLINES			/* subscript for diagonal 0 */

EDIT *
shChainDiff(
   CHAIN *l1,                       /* I: first chain to compare */
   CHAIN *l2,                       /* I: second chain to compare */
   int (*func)(void *a, void *b),   /* I: the comparison function */
   int max_d			            /* I: bound on size of edit script */
   )
{
   int d;				/* current edit distance */
   int k;				/* current diagonal */
   int last_d[2*MAXLINES + 1];		/* the row containing the last d */
   int lower,upper;			/* left- and right-most diagonals */
   EDIT *new_node;			/* a new EDIT element */
   int n1,n2;				/* number of lines in files 1 and 2 */
   int row,col;				/* row and column numbers */
   EDIT *script[2*MAXLINES + 1];	/* the editing script */

/*
 * no-one cleaned up after the last call to shListFree; let us do it
 * for them. There's no need to free chain[12]
 */
   if(membase != NULL) {
      free_script(membase); membase = NULL;
   }
/*
 * convert chains to arrays
 */
   if(l1 == NULL || l2 == NULL) {
      shErrStackPush("A chain to be diffed is NULL");
      return(NULL);
   }
   if(shChainTypeGet(l1) != shChainTypeGet(l2)) {
      shErrStackPush("The two chains being diffed must be of the same type");
      return(NULL);
   }
   
   if(chain1 == NULL) {
      chain1 = (void **)shMalloc(MAXLINES*sizeof(void *));
   }
   if(chain2 == NULL) {
      chain2 = (void **)shMalloc(MAXLINES*sizeof(void *));
   }
   
   n1 = chain_to_array(l1,chain1,MAXLINES);
   n2 = chain_to_array(l2,chain2,MAXLINES);
   if(n1 < 0 || n2 < 0) {
      return(NULL);
   }
   if(max_d < 0) {
      max_d = n1 + n2;			/* it can't be more than this */
   }
/*
 * initialise; we can think of this as inserting zeros into the edit
 * difference matrix where appropriate
 */
   for(row = 0;row < n1 && row < n2;row++) {
      if((*func)(chain1[row],chain2[row]) != 0) {
         break;
      }
   }
   last_d[ORIGIN] = row;
   script[ORIGIN] = NULL;
   lower = (row == n1) ? ORIGIN + 1 : ORIGIN - 1;
   upper = (row == n2) ? ORIGIN - 1 : ORIGIN + 1;

   if(lower > upper) {
      membase = (EDIT *)shMalloc(sizeof(EDIT));
      membase->op = IDENTICAL;
      membase->link = NULL;
      return(membase);
   }
/*
 * Loop through values of the edit distance, d
 */
   for(d = 1;d <= max_d;d++) {
      for(k = lower;k <= upper;k += 2) { /* loop over each diagonal */
         new_node = (EDIT *)shMalloc(sizeof(EDIT));
/*
 * find an entry with edit distance d on diagonal k. We already have found
 * ones with (d - 1) on neighbouring diagonals, and now we have to decide
 * whether moving down from the last d-1 on diagonal k+1 (i.e. DELETE) puts
 * us farther along diagonal k towards the bottom right corner than moving 
 * right from the last d-1 on diagonal k-1 (i.e. INSERT) would
 */
         if(k == ORIGIN - d || 
         	(k != ORIGIN + d && last_d[k + 1] >= last_d[k - 1])) {
            row = last_d[k + 1] + 1;
            new_node->link = script[k + 1];
            new_node->op = DELETE;
         } else {
            row = last_d[k - 1];
            new_node->link = script[k - 1];
            new_node->op = INSERT;
         }
         col = row + k - ORIGIN;
         new_node->line1.n = row;
         new_node->line2.n = col;
         script[k] = new_node;

	 new_node->memchain = membase; membase = new_node;
/*
 * now see how far we can slide down the diagonal
 */
	 while(row < n1 && col < n2 && (*func)(chain1[row],chain2[col]) == 0) {
	    row++; col++;
	 }
	 last_d[k] = row;
/*
 * See where we are
 */	 
	 if(row == n1 && col == n2) {	/* bottom right corner */
	    return(chain_prepare(script[k]));
	 }
	 if(row == n1) {		/* last row; don't look left */
	    lower = k + 2;		/* it'll be --'d */
	 }
	 if(col == n2) {		/* last col; don't look up */
	    upper = k - 2;		/* it'll be ++'d */
	 }
      }
      lower--;
      upper++;
   }
/*
 * if we got here we must have tried to generate too long a script
 */
   shErrStackPush("The files differ in at least %d lines\n",d);
   shChainDiffFree();   

   return(NULL);
}

/*****************************************************************************/
/***************************************************************************
 * <AUTO EXTRACT>
 *
 * ROUTINE: shChainDiffFree 
 * 
 * DESCRIPTION: 
 * free all the state associated with a call to shChainDiff
 *
 * return: nothing
 *
 * </AUTO>
 */

void
shChainDiffFree(void)
{
   shFree((char *)chain1); chain1 = NULL;
   shFree((char *)chain2); chain2 = NULL;
   free_script(membase); membase = NULL;
}

/*****************************************************************************/
/*
 * convert a chain to an array, returning the number of elements
 */
static int
chain_to_array(CHAIN *l, void **arr, int size)
{
   int i;

   if (shChainSize(l) >= size) {
      shErrStackPush("list is too long in list_to_array");
      return(-1);
   }

   for (i = 0; i < shChainSize(l); i++) {
      arr[i + 1] = (void *) shChainElementGetByPos(l, i);
   }

   return(i);
}

/*****************************************************************************/
/*
 * reverse the order of the editing script, and change line numbers to pointers
 */
static EDIT *
chain_prepare(EDIT *script)
{
   EDIT *a,*b;
   EDIT *tmp = NULL;
   
   a = script;
   while(a != NULL) {
      b = tmp;
      tmp = a;
      a = a->link;
      tmp->link = b;
      tmp->line1.ptr = chain1[tmp->line1.n - 1];
      tmp->line2.ptr = chain2[tmp->line2.n - 1];
   }

   return(tmp);
}

/*****************************************************************************/
/*
 * free up the memory storing the edit scripts
 */
static void
free_script(EDIT *ptr)
{
   EDIT *tmp;

   while(ptr != NULL) {
      tmp = ptr->memchain;
      shFree(ptr);
      ptr = tmp;
   }
}

/****************************************************************************/
/***************************************************************************
 * <AUTO EXTRACT>
 *
 * ROUTINE: shChainDiffPrint 
 * 
 * DESCRIPTION: 
 * Return the answer in various more-or-less helpful ways
 *
 * This function prints out the editing commands required to
 * convert from chain1 to chain2.
 *
 * It is probably not very useful in the context of a chain comparator
 *
 * return: nothing
 *
 * </AUTO>
 */

void
shChainDiffPrint(
   EDIT *script            /* I: EDIT structure to be explicated */
   )
{
   EDIT *a = NULL,*b;
   int change;

   while(script != NULL) {
      b = script;
      switch(script->op) {
       case INSERT:
         printf("Inserted after line %d:\n",script->line1.n);
         break;
       case DELETE:
         do {				/* look for block of lines */
            a = b;
            b = b->link;
         } while(b != NULL && b->op == DELETE && b->line1.n == a->line1.n + 1);
/*
 * b now points to the command after the last deletion
 */
         change = (b != NULL && b->op == INSERT &&
					     b->line1.n == a->line1.n) ? 1 : 0;
         if(change) {
            printf("Changed ");
         } else {
            printf("Deleted ");
         }

         if(a == script) {
            printf("line %d:\n",script->line1.n);
         } else {
            printf("lines %d--%d:\n",script->line1.n,a->line1.n);
         }
/*
 * print deleted lines
 */         
         do {
	    printf("  0x%p\n",chain1[script->line1.n - 1]);
            script = script->link;
         } while(script != b);
         if(!change) {			/* just a deletion */
            continue;
         }
         printf("To:\n");
         break;
       default:
	 break;
      }
/*
 * and then the inserted ones 
 */
      do {
	 printf("  0x%p\n",chain2[script->line2.n - 1]);
         script = script->link;
      } while(script != NULL && script->op == INSERT &&
						script->line1.n == b->line1.n);
   }
} 

/*****************************************************************************/
/***************************************************************************
 * <AUTO EXTRACT>
 *
 * ROUTINE: shChainDiffAsList 
 * 
 * DESCRIPTION: 
 * This function sets the return value of a TCL interpreter to be a list
 * of values (# addr) where # is a chain number (1 or 2) and addr is the
 * address of an element of that chain that has no matching element on the
 * other chain
 *
 * return: SH_SUCCESS          if all goes well
 *         SH_GENERIC_ERROR    if not
 *
 * </AUTO>
 */

RET_CODE
shChainDiffAsList(
   Tcl_Interp *interp, 
   EDIT *script             /* I: differences already found */
   )
{
   char buff[SIZE];
   
   if(script == NULL) {
      Tcl_SetResult(interp,"No chain diff is specified",TCL_STATIC);
      return(SH_GENERIC_ERROR);
   } else if(script->op == IDENTICAL) {
      Tcl_SetResult(interp,"",TCL_STATIC);
      return(SH_SUCCESS);
   }

   Tcl_SetResult(interp,"",TCL_STATIC);
   
   while(script != NULL) {
      if(script->op == DELETE) {
	 sprintf(buff,"1 0x%p",script->line1.ptr);
      } else {
	 sprintf(buff,"2 0x%p",script->line2.ptr);
      }
      Tcl_AppendElement(interp,buff);
      script = script->link;
   }
   return(SH_SUCCESS);
} 

/***************************************************************************
 * <AUTO EXTRACT>
 *
 * ROUTINE: phRegStatsSigmaClip 
 * 
 * DESCRIPTION: 
 * This routine is a driver for the routines which calculate 
 * the mean and stdev of pixel values; it checks the REGION's
 * type and calls the appropriate subroutine to carry out the
 * arithmetic.
 *
 * return: SH_SUCCESS          if all goes well
 *         SH_GENERIC_ERROR    if not
 *
 * </AUTO>
 */

int
phRegStatsSigmaClip(
   REGION *reg,             /* I: region in which to calc statistics */
   char mask_flag,          /* I: ignore pixels if MASK values OR with this */
   int iter,                /* I: number of iterations of sigma clipping */
   float clipsig,           /* I: ignore points > clipsig*stdev from mean */
   float *mean,             /* O: calculated clipped mean */
   float *stdev             /* O: calculated clipped standard deviation */
   )
{
   int ret;

   switch (reg->type) {
   case TYPE_U16: 
      ret = phRegStatsSigmaClipU16(reg, mask_flag, iter, clipsig, 
	                                  mean, stdev);
      break;
   case TYPE_FL32: 
      ret = phRegStatsSigmaClipFL32(reg, mask_flag, iter, clipsig, 
	                                  mean, stdev);
      break;
   default:
      shDebug(0, "phRegStatsSigmaClip: given REGION of type not U16 or FL32");
      ret = SH_GENERIC_ERROR;
   }
   return(ret);
}


   /*
    * this routine is slow, but probably more robust than the quick
    * histogram functions.  It finds the mean and standard deviation
    * of all pixels in the given region which do not have MASK bits
    * set to any of the given "mask_flag" values.
    *
    * the routine performs sigma-clipping, iterating "iter" times
    * and ignoring any points more than "clipsig" standard deviations
    * from the mean value of the previous iteration.
    *
    * the resulting mean and stdev are returned in the "mean" and
    * "stdev" arguments.  If all goes well, SH_SUCCESS is returned.
    * If there is a problem, SH_GENERIC_ERROR is returned.
    * If fewer than 2 pixels are valid, the mean and standard deviation
    * are set to 1.0 and 1.0, and a warning message is printed.
    */

static int
phRegStatsSigmaClipU16(REGION *reg, char mask_flag, int iter, float clipsig,
                      float *mean, float *stdev)
{
   int it, i, j;
   float minval, maxval, val, sum, sumsq, num, mn, sig;
   U16 *ppix;

   shAssert(reg->type == TYPE_U16);

   it = 0;
   minval = 0;
   maxval = 65536;

   shAssert(mask_flag == 0);

   while (1) {

      sum = sumsq = num = 0;
      for (i = 0; i < reg->nrow; i++) {
         ppix = &(reg->rows_u16[i][0]);
         for (j = 0; j < reg->ncol; j++, ppix++) {
            val = *ppix;
            if ((val < minval) || (val > maxval)) {
               continue;
            }
            sum += val;
            sumsq += val*val;
            num++;
         }
      }
      if (num > 1) {
         mn = sum/num;
         sig = sqrt( fabs(sumsq/num) - (mn*mn) );
      }
      else {
         shDebug(2, "shRegStatsSigmaClipU16: only %f pixels; return mean, sig = 1.0",
               num);
         mn = 1.0;
         sig = 1.0;
         break;
      }

      /* now check to see if we can quit */
      if (++it >= iter) {
         break;
      }

      /* set limits for accepting pixels on the next iteration */
      minval = mn - clipsig*sig;
      maxval = mn + clipsig*sig;

   }

   *mean = mn;
   *stdev = sig;
   return(SH_SUCCESS);
}


   /*
    * this routine is analogous to phRegStatsSigmaClipU16, but works
    * on FL32 regions. 
    * 
    * See comments above.
    */

static int
phRegStatsSigmaClipFL32(REGION *reg, char mask_flag, int iter, float clipsig,
                      float *mean, float *stdev)
{
   int it, i, j;
   float minval, maxval, val, sum, sumsq, num, mn, sig;
   float *ppix;

   shAssert(reg->type == TYPE_FL32);

   it = 0;
   minval = -1.0e10;
   maxval = 1.0e10;
   shAssert(mask_flag == 0);

   while (1) {

      sum = sumsq = num = 0;
      for (i = 0; i < reg->nrow; i++) {
         ppix = &(reg->rows_fl32[i][0]);
         for (j = 0; j < reg->ncol; j++, ppix++) {
            val = *ppix;
            if ((val < minval) || (val > maxval)) {
               continue;
            }
            sum += val;
            sumsq += val*val;
            num++;
         }
      }
      if (num > 1) {
         mn = sum/num;
         sig = sqrt( fabs(sumsq/num) - (mn*mn) );
      }
      else {
         shDebug(2, "shRegStatsSigmaClipFL32: only %f pixels; return mean, sig = 1.0",
               num);
         mn = 1.0;
         sig = 1.0;
         break;
      }

      /* now check to see if we can quit */
      if (++it >= iter) {
         break;
      }

      /* set limits for accepting pixels on the next iteration */
      minval = mn - clipsig*sig;
      maxval = mn + clipsig*sig;

   }

   *mean = mn;
   *stdev = sig;
   return(SH_SUCCESS);
}


/***************************************************************************
 * <AUTO EXTRACT>
 *
 * ROUTINE: shRegIntSetVal 
 * 
 * DESCRIPTION: 
 * Set all the values of a integral (8, 16, or 32 bit) region to a given value.
 * This is MUCH faster than regSetWithDbl, and useful in writing tests
 *
 * return: SH_SUCCESS		If region type is supported
 *         SH_GENERIC_ERROR	otherwise
 */

int
shRegIntSetVal(REGION *reg,             /* set all pixels in this region ... */
	       const float val)		/* to this value */
{
   int dsize;				/* sizeof one pixel */
   int i;
   int ncol;				/* == reg->ncol */
   void **rptr;				/* pointer to rows */

   shAssert(reg != NULL);
   ncol = reg->ncol;

   if(reg->type == TYPE_U8) {
      const int pval = val + 0.5;
      for(i = 0;i < ncol;i++) {
	 reg->rows_u8[0][i] = pval;
      }
      rptr = (void **)reg->rows_u8;
      dsize = sizeof(U8);
   } else if(reg->type == TYPE_S8) {
      const int pval = val < 0 ? -(-val + 0.5) : val + 0.5;
      for(i = 0;i < ncol;i++) {
	 reg->rows_s8[0][i] = pval;
      }
      rptr = (void **)reg->rows_s8;
      dsize = sizeof(S8);
   } else if(reg->type == TYPE_U16) {
      const int pval = val + 0.5;
      U16 *row = reg->rows_u16[0];	/* unaliased for speed */
      
      for(i = 0;i < ncol;i++) {
	 row[i] = pval;
      }
      rptr = (void **)reg->rows_u16;
      dsize = sizeof(U16);
   } else if(reg->type == TYPE_S16) {
      const int pval = val < 0 ? -(-val + 0.5) : val + 0.5;
      for(i = 0;i < ncol;i++) {
	 reg->rows_s16[0][i] = pval;
      }
      rptr = (void **)reg->rows_s16;
      dsize = sizeof(S16);
   } else if(reg->type == TYPE_U32) {
      const int pval = val + 0.5;
      for(i = 0;i < ncol;i++) {
	 reg->rows_u32[0][i] = pval;
      }
      rptr = (void **)reg->rows_u32;
      dsize = sizeof(U32);
   } else if(reg->type == TYPE_S32) {
      const int pval = val < 0 ? -(-val + 0.5) : val + 0.5;
      for(i = 0;i < ncol;i++) {
	 reg->rows_s32[0][i] = pval;
      }
      rptr = (void **)reg->rows_s32;
      dsize = sizeof(S32);
   } else if(reg->type == TYPE_FL32) {
      for(i = 0;i < ncol;i++) {
	 reg->rows_fl32[0][i] = val;
      }
      rptr = (void **)reg->rows_fl32;
      dsize = sizeof(FL32);
   } else {
      shError("shRegIntSetVal doesn't handle regions of type %d\n",reg->type);
      return(SH_GENERIC_ERROR);
   }

   for(i = 1;i < reg->nrow;i++) {
      memcpy(rptr[i],(const void *)rptr[0],ncol*dsize);
   }
   return(SH_SUCCESS);
}

/***************************************************************************
 * <AUTO EXTRACT>
 *
 * Set all the values of a region (currently only PIX) within a mask
 * to a specified value
 *
 * The mask must be wholly enclosed within the region
 */
void
phRegIntSetValInObjmask(REGION *reg,	/* set all pixels in this region ... */
			const OBJMASK *om, /* within this mask... */
			const float val) /* to this value */
{
   int i;
   int nspan;				/* == om->nspan */
   int om_width;			/* maximum width of om */
   const PIX pval = FLT2PIX(val);
   int row0, col0;			/* == reg->{row,col}0 */
   PIX **rows;				/* == reg->ROWS */
   SPAN *s;				/* a span of om */
   PIX *vals;				/* an array with each element == val */
   int y, x1, x2;			/* unpacked from s */

   shAssert(reg != NULL && reg->type == TYPE_PIX);
   rows = reg->ROWS; row0 = reg->row0; col0 = reg->col0;
   shAssert(om != NULL);
   shAssert(om->rmin >= row0 && om->rmax < row0 + reg->nrow);
   shAssert(om->cmin >= col0 && om->cmax < col0 + reg->ncol);
   nspan = om->nspan;
   om_width = om->rmax - om->rmin + 1;

   vals = alloca(om_width*sizeof(PIX));
   for(i = 0; i < om_width; i++) {
      vals[i] = pval;
   }

   for(i = 0;i < nspan;i++) {
      s = &om->s[i];
      y = s->y; x1 = s->x1; x2 = s->x2;
      memcpy(&rows[y - row0][x1 - col0], vals, (x2 - x1 + 1)*sizeof(PIX));
   }
}

/***************************************************************************
 * <AUTO EXTRACT>
 *
 * Clip all the values of a region (currently only PIX) within a mask
 * to lie at or below a specified value
 *
 * The mask must be wholly enclosed within the region
 */
void
phRegIntClipValInObjmask(REGION *reg,	/* clip all pixels in this region ...*/
			 const OBJMASK *om, /* within this mask... */
			 const int drow, const int dcol, /* offset om by (drow, dcol) */
			 const int val)	/* to lie at or below this value */
{
   int i, j;
   int j0, j1;				/* range of j values in a row */
   int nspan;				/* == om->nspan */
   int row0, col0;			/* == reg->{row,col}0 */
   PIX **rows, *row;			/* == reg->ROWS, reg->ROWS[] */
   SPAN *s;				/* a span of om */
   int y, x1, x2;			/* unpacked from s */

   shAssert(reg != NULL && reg->type == TYPE_PIX);
   rows = reg->ROWS; row0 = reg->row0 - drow; col0 = reg->col0 - dcol;
   shAssert(om != NULL);
   nspan = om->nspan;

   for(i = 0; i < nspan; i++) {
      s = &om->s[i];
      y = s->y; x1 = s->x1; x2 = s->x2;
      if(y - row0 < 0 || y - row0 >= reg->nrow) {
	  continue;
      }
      row = rows[y - row0];
      j0 = x1 - col0; j1 = x2 - row0 + 1;
      if(j0 < 0) j0 = 0;
      if(j1 > reg->ncol) j1 = reg->ncol;
      for(j = j0; j < j1; j++) {
	 if(row[j] > val) {
	    row[j] = val;
	 }
      }
   }
}

/***************************************************************************
 * <AUTO EXTRACT>
 *
 * Copy one region to another. It is also able to convert U16 to FL32,
 * removing the SOFT_BIAS
 *
 * return: SH_SUCCESS		If region type is supported
 *         SH_GENERIC_ERROR	otherwise
 */
int
shRegIntCopy(REGION *out,		/* output region */
	     const REGION *in)		/* input region */
{
   int i, j;
   int ncol,nrow;			/* unpacked from out */

   shAssert(out != NULL && in != NULL);
   shAssert(in->nrow == out->nrow && in->ncol == out->ncol);

   ncol = out->ncol;
   nrow = out->nrow;

   if(out->type == TYPE_U8) {
      shAssert(in->type == out->type);
      for(i = 0;i < nrow;i++) {
	 memcpy(out->rows_u8[i],in->rows_u8[i],ncol);
      }
   } else if(out->type == TYPE_S8) {
      shAssert(in->type == out->type);
      for(i = 0;i < nrow;i++) {
	 memcpy(out->rows_s8[i],in->rows_s8[i],ncol);
      }
   } else if(out->type == TYPE_U16) {
      if(in->type == TYPE_U8) {
	 U8 *iptr;
	 U16 *optr;

	 for(i = 0;i < nrow;i++) {
	    iptr = in->rows_u8[i];
	    optr = out->rows_u16[i];
	    for(j = 0;j < ncol; j++) {
	       optr[j] = iptr[j];
	    }
	 }
      } else if(in->type == TYPE_U16) {
	 for(i = 0;i < nrow;i++) {
	    memcpy(out->rows_u16[i],in->rows_u16[i],ncol*sizeof(U16));
	 }
      } else if(in->type == TYPE_S32) {
	 S32 *iptr;
	 U16 *optr;
	 unsigned int oval;

	 for(i = 0;i < nrow;i++) {
	    iptr = in->rows_s32[i];
	    optr = out->rows_u16[i];
	    for(j = 0;j < ncol; j++) {
	       oval = iptr[j] + SOFT_BIAS;
	       optr[j] = (oval > MAX_U16) ? MAX_U16 : oval;
	    }
	 }
      } else if(in->type == TYPE_FL32) {
	 FL32 *iptr;
	 U16 *optr;
	 unsigned int oval;

	 for(i = 0;i < nrow;i++) {
	    iptr = in->rows_fl32[i];
	    optr = out->rows_u16[i];
	    for(j = 0;j < ncol; j++) {
	       oval = iptr[j] + SOFT_BIAS + 0.5;
	       optr[j] = (oval > MAX_U16) ? MAX_U16 : oval;
	    }
	 }
      } else {
	 shError("shRegIntCopy doesn't convert regions of type %d to U16\n",
		 in->type);
	 return(SH_GENERIC_ERROR);
      }
   } else if(out->type == TYPE_S16) {
      shAssert(in->type == out->type);
      for(i = 0;i < nrow;i++) {
	 memcpy(out->rows_s16[i],in->rows_s16[i],ncol*sizeof(S16));
      }
   } else if(out->type == TYPE_U32) {
      shAssert(in->type == out->type);
      for(i = 0;i < nrow;i++) {
	 memcpy(out->rows_u32[i],in->rows_u32[i],ncol*sizeof(U32));
      }
   } else if(out->type == TYPE_S32) {
      shAssert(in->type == out->type);
      for(i = 0;i < nrow;i++) {
	 memcpy(out->rows_s32[i],in->rows_s32[i],ncol*sizeof(S32));
      }
   } else if(out->type == TYPE_FL32) {
      if(in->type == TYPE_U16) {
	 U16 *iptr;
	 FL32 *optr;

	 for(i = 0;i < nrow;i++) {
	    iptr = in->rows_u16[i];
	    optr = out->rows_fl32[i];
	    for(j = 0;j < ncol; j++) {
	       optr[j] = (int)(iptr[j] + 0.499) - SOFT_BIAS; /* If we added 0.5
								then
								U16--FL32--U16
								would add 1 */
	    }
	 }
      } else if(in->type == TYPE_FL32) {
	 for(i = 0;i < nrow;i++) {
	    memcpy(out->rows_fl32[i],in->rows_fl32[i],ncol*sizeof(FL32));
	 }
      } else {
	 shError("shRegIntCopy doesn't convert regions of type %d to FL32\n",
		 in->type);
	 return(SH_GENERIC_ERROR);
      }
   } else {
      shError("shRegIntCopy doesn't handle regions of type %d\n",
	      out->type);
      return(SH_GENERIC_ERROR);
   }

   return(SH_SUCCESS);
}

/***************************************************************************
 * <AUTO EXTRACT>
 *
 * Add one images to another: reg1 += reg2
 *
 * return: SH_SUCCESS		If both regions are of the same supported type
 *         SH_GENERIC_ERROR	otherwise
 */
int
shRegIntAdd(REGION *reg1,		/* region 1 */
	    REGION *reg2)		/* region 2 */
{
   int i;
   int ncol,nrow;			/* unpacked from reg1 */

   shAssert(reg1 != NULL && reg2 != NULL);

   if(reg1->type != reg2->type) {
      shError("shRegIntAdd: region types differ\n");
      return(SH_GENERIC_ERROR);
   }

   ncol = reg1->ncol;
   nrow = reg1->nrow;

   if(reg2->ncol != ncol || reg2->nrow != nrow) {
      shError("shRegIntAdd: region sizes differ\n");
      return(SH_GENERIC_ERROR);
   }

   if(reg1->type == TYPE_U8) {
      U8 **rptr1 = reg1->rows_u8;
      U8 **rptr2 = reg2->rows_u8;
      U8 *ptr1, *ptr2, *end;
      
      for(i = 0;i < nrow;i++) {
	 ptr1 = rptr1[i];
	 ptr2 = rptr2[i];
	 end = ptr1 + ncol;
	 while(ptr1 < end) {
	    *ptr1++ += *ptr2++;
	 }
      }
   } else if(reg1->type == TYPE_S8) {
      S8 **rptr1 = reg1->rows_s8;
      S8 **rptr2 = reg2->rows_s8;
      S8 *ptr1, *ptr2, *end;
      
      for(i = 0;i < nrow;i++) {
	 ptr1 = rptr1[i];
	 ptr2 = rptr2[i];
	 end = ptr1 + ncol;
	 while(ptr1 < end) {
	    *ptr1++ += *ptr2++;
	 }
      }
   } else if(reg1->type == TYPE_U16) {
      U16 **rptr1 = reg1->rows_u16;
      U16 **rptr2 = reg2->rows_u16;
      U16 *ptr1, *ptr2, *end;
      
      for(i = 0;i < nrow;i++) {
	 ptr1 = rptr1[i];
	 ptr2 = rptr2[i];
	 end = ptr1 + ncol;
	 while(ptr1 < end) {
	    *ptr1++ += *ptr2++;
	 }
      }
   } else if(reg1->type == TYPE_S16) {
      S16 **rptr1 = reg1->rows_s16;
      S16 **rptr2 = reg2->rows_s16;
      S16 *ptr1, *ptr2, *end;
      
      for(i = 0;i < nrow;i++) {
	 ptr1 = rptr1[i];
	 ptr2 = rptr2[i];
	 end = ptr1 + ncol;
	 while(ptr1 < end) {
	    *ptr1++ += *ptr2++;
	 }
      }
   } else if(reg1->type == TYPE_U32) {
      U32 **rptr1 = reg1->rows_u32;
      U32 **rptr2 = reg2->rows_u32;
      U32 *ptr1, *ptr2, *end;
      
      for(i = 0;i < nrow;i++) {
	 ptr1 = rptr1[i];
	 ptr2 = rptr2[i];
	 end = ptr1 + ncol;
	 while(ptr1 < end) {
	    *ptr1++ += *ptr2++;
	 }
      }
   } else if(reg1->type == TYPE_S32) {
      S32 **rptr1 = reg1->rows_s32;
      S32 **rptr2 = reg2->rows_s32;
      S32 *ptr1, *ptr2, *end;
      
      for(i = 0;i < nrow;i++) {
	 ptr1 = rptr1[i];
	 ptr2 = rptr2[i];
	 end = ptr1 + ncol;
	 while(ptr1 < end) {
	    *ptr1++ += *ptr2++;
	 }
      }
   } else if(reg1->type == TYPE_FL32) {
      FL32 **rptr1 = reg1->rows_fl32;
      FL32 **rptr2 = reg2->rows_fl32;
      FL32 *ptr1, *ptr2, *end;
      
      for(i = 0;i < nrow;i++) {
	 ptr1 = rptr1[i];
	 ptr2 = rptr2[i];
	 end = ptr1 + ncol;
	 while(ptr1 < end) {
	    *ptr1++ += *ptr2++;
	 }
      }
   } else {
      shError("shRegIntAdd doesn't handle regions of type %d\n", reg1->type);
      return(SH_GENERIC_ERROR);
   }

   return(SH_SUCCESS);
}

/***************************************************************************
 * <AUTO EXTRACT>
 *
 * Add a constant to each pixel of an image (not all types are supported)
 *
 * This is MUCH faster than using regAdd
 *
 * return: SH_SUCCESS		If region type is supported
 *         SH_GENERIC_ERROR	otherwise
 */

int
shRegIntConstAdd(REGION *reg,		/* The region ... */
		 const float val,	/* the constant to add */
		 const int dither)		/* dither, not round */
{
   int i;
   int ncol,nrow;			/* unpacked from reg */
   float tmp;

   shAssert(reg != NULL);
   shAssert(!dither || reg->type == TYPE_U16);

   ncol = reg->ncol;
   nrow = reg->nrow;

   if(reg->type == TYPE_U8) {
      U8 **rptr = reg->rows_u8;
      U8 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    tmp = *ptr + val + 0.5;
	    *ptr++ = (tmp >= 0) ? (tmp <= MAX_U8 ? tmp : MAX_U8) : 0;
	 }
      }
   } else if(reg->type == TYPE_S8) {
      S8 **rptr = reg->rows_s8;
      S8 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    tmp = *ptr + val + 0.5;
	    *ptr++ = (tmp >= MIN_S8) ? (tmp <= MAX_S8 ? tmp : MAX_S8) : MIN_S8;
	 }
      }
   } else if(reg->type == TYPE_U16) {
      U16 **rptr = reg->rows_u16;
      U16 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 if(dither) {
	    while(ptr < end) {
	       tmp = *ptr + val + phRandomUniformdev();
	       *ptr++ = (tmp >= 0) ? (tmp <= MAX_U16 ? tmp : MAX_U16) : 0;
	    }
	 } else {
	    while(ptr < end) {
	       tmp = *ptr + val + 0.5;
	       *ptr++ = (tmp >= 0) ? (tmp <= MAX_U16 ? tmp : MAX_U16) : 0;
	    }
	 }
      }
   } else if(reg->type == TYPE_S16) {
      S16 **rptr = reg->rows_s16;
      S16 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    tmp = *ptr + val + 0.5;
	    *ptr++ = (tmp >= MIN_S16) ?
				     (tmp <= MAX_S16 ? tmp : MAX_S16) : MIN_S16;
	 }
      }
   } else if(reg->type == TYPE_U32) {
      U32 **rptr = reg->rows_u32;
      U32 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    tmp = *ptr + val + 0.5;
	    *ptr++ = (tmp >= 0) ? (tmp <= MAX_U32 ? tmp : MAX_U32) : 0;
	 }
      }
   } else if(reg->type == TYPE_S32) {
      S32 **rptr = reg->rows_s32;
      S32 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    tmp = *ptr + val + 0.5;
	    *ptr++ = (tmp >= MIN_S32) ?
				    (tmp <= MAX_S32 ? tmp : MAX_S32) : MIN_S32;
	 }
      }
   } else if(reg->type == TYPE_FL32) {
      FL32 **rptr = reg->rows_fl32;
      FL32 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    *ptr++ += val;
	 }
      }
   } else {
      shError("shRegIntConstAdd doesn't handle regions of type %d\n",
	      reg->type);
      return(SH_GENERIC_ERROR);
   }

   return(SH_SUCCESS);
}

/***************************************************************************
 * <AUTO EXTRACT>
 *
 * Multiply each pixel of an image by a floating constant
 * (not all types are supported)
 *
 * Note that shRegIntConstMultAndShift multiplies by an int and down-shifts
 * the result; this is faster yet.
 *
 * This is MUCH faster than using shRegMultWithDbl
 *
 * return: SH_SUCCESS		If region type is supported (currently only
 *				unsigned ints so as not to worry about
 *                              rounding negative numbers)
 *         SH_GENERIC_ERROR	otherwise
 */

int
shRegIntConstMult(
		 REGION *reg,		/* The region ... */
		 const float val	/* the constant to multiply by */
		 )
{
   int i;
   int ncol,nrow;			/* unpacked from reg */

   shAssert(reg != NULL);

   ncol = reg->ncol;
   nrow = reg->nrow;

   if(reg->type == TYPE_U8) {
      U8 **rptr = reg->rows_u8;
      U8 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    *ptr = val*(*ptr) + 0.5;
	    ptr++;
	 }
      }
   } else if(reg->type == TYPE_U16) {
      U16 **rptr = reg->rows_u16;
      U16 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    *ptr = val*(*ptr) + 0.5;
	    ptr++;
	 }
      }
   } else if(reg->type == TYPE_U32) {
      U32 **rptr = reg->rows_u32;
      U32 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    *ptr = val*(*ptr) + 0.5;
	    ptr++;
	 }
      }
   } else if(reg->type == TYPE_S32) {
      S32 **rptr = reg->rows_s32;
      S32 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    *ptr = val*(*ptr) + 0.5;
	    ptr++;
	 }
      }
   } else if(reg->type == TYPE_FL32) {
      FL32 **rptr = reg->rows_fl32;
      FL32 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    *ptr++ *= val;
	 }
      }
   } else {
      shError("shRegIntConstMult doesn't handle regions of type %d\n",
	      reg->type);
      return(SH_GENERIC_ERROR);
   }

   return(SH_SUCCESS);
}

/***************************************************************************
 * <AUTO EXTRACT>
 *
 * Multiply each pixel of an image by a integer constant, and down-shift
 * the result (not all types are supported). See also shRegIntConstMult
 *
 * This is MUCH faster than using shRegMultWithDbl
 *
 * return: SH_SUCCESS		If region type is supported (currently only
 *				unsigned chars and shorts, so as not to worry
 *				about rounding negative numbers and overflows)
 *         SH_GENERIC_ERROR	otherwise
 */

int
shRegIntConstMultAndShift(
			  REGION *reg,	/* The region ... */
			  const int val, /* the constant to multiply by */
			  const int shift /* how much to down-shift */
		 )
{
   int i;
   int ncol,nrow;			/* unpacked from reg */
   const int half = (1 << shift)/2;	/* (half >> shift) == 0.5 */

   shAssert(reg != NULL);

   ncol = reg->ncol;
   nrow = reg->nrow;

   if(reg->type == TYPE_U8) {
      U8 **rptr = reg->rows_u8;
      U8 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    *ptr = (val*(*ptr) + half) >> shift;
	    ptr++;
	 }
      }
   } else if(reg->type == TYPE_U16) {
      U16 **rptr = reg->rows_u16;
      U16 *ptr,*end;
      int pval;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    pval = (val*(*ptr) + half) >> shift;
	    *ptr++ = (pval > MAX_U16 ? MAX_U16 : pval);
	 }
      }
   } else if(reg->type == TYPE_S32) {
      S32 **rptr = reg->rows_s32;
      S32 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    *ptr = (val*(*ptr) + half) >> shift;
	    ptr++;
	 }
      }
   } else if(reg->type == TYPE_FL32) {
      FL32 **rptr = reg->rows_fl32;
      FL32 *ptr,*end;
      const float fval = (float)val/(1 << shift);

      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    *ptr++ *= fval;
	 }
      }
   } else {
      shError("shRegIntConstMultAndShift doesn't handle regions of type %d\n",
	      reg->type);
      return(SH_GENERIC_ERROR);
   }

   return(SH_SUCCESS);
}

/*****************************************************************************/
/*
 * Calculate the linear combination of two images, specifically given
 * regions reg1 and reg2, and three constants a, b, and c, carry out
 *   reg1 = a + b*reg1 + c*reg2
 * If c is 0, reg2 may be NULL
 *
 * return: reg1 (or a new region if how == 2)
 *         NULL	otherwise
 */
REGION *
shRegIntLincom(REGION *reg1,
	       const REGION *reg2,
	       float a,
	       float b,
	       float c,
	       LINCOM_FLAGS flag)	/* How to handle non-identical REGIONS:
					   LINCOM_EXACT:     fail
					   LINCOM_INTERSECT: add the intersection of regions
					   LINCOM_UNION:     grow reg1 to contain union of regions */
{
   int i, j;
   int ncol,nrow;			/* unpacked from reg1 */
   int val;

   shAssert(reg1 != NULL && (c == 0.0 || reg2 != NULL));
   /*
    * Do we need to find just the overlapping parts of the REGIONs?
    */
   if(flag != LINCOM_EXACT && reg2 != NULL) {	/* flag is irrelevant if !reg2 */
      const int row0_1 = reg1->row0;
      const int col0_1 = reg1->col0;
      const int row1_1 = reg1->row0 + reg1->nrow - 1;
      const int col1_1 = reg1->col0 + reg1->ncol - 1;
      const int row0_2 = reg2->row0;
      const int col0_2 = reg2->col0;
      const int row1_2 = reg2->row0 + reg2->nrow - 1;
      const int col1_2 = reg2->col0 + reg2->ncol - 1;

      if(flag == LINCOM_INTERSECT) {	/* intersection */
	  /*
	   * Figure out boundary of intersect in absolute coordinates
	   */
	  const int row0 = (row0_1 > row0_2) ? row0_1 : row0_2;
	  const int col0 = (col0_1 > col0_2) ? col0_1 : col0_2;
	  const int row1 = (row1_1 < row1_2) ? row1_1 : row1_2;
	  const int col1 = (col1_1 < col1_2) ? col1_1 : col1_2;
	  
	  if(row1 >= row0 && col1 >= col0) {
	      const int nrow = row1 - row0 + 1;
	      const int ncol = col1 - col0 + 1;
	      REGION *sreg1 = shSubRegNew("", reg1, nrow, ncol,
					  row0 - row0_1, col0 - col0_1, 0);
	      REGION *sreg2 = shSubRegNew("", reg2, nrow, ncol,
					  row0 - row0_2, col0 - col0_2, 0);
	      
	      REGION *ret = shRegIntLincom(sreg1, sreg2, a, b, c, LINCOM_EXACT);
	      
	      shRegDel(sreg1);
	      shRegDel(sreg2);

	      return (ret == NULL) ? NULL : reg1;
	  } else {
	      ;				/* no intersection */
	  }
      } else if(flag == LINCOM_UNION) {		/* calculate Union */
	  /*
	   * Figure out bounding box of reg1 & reg2 in absolute coordinates
	   */
	  const int row0 = (row0_1 < row0_2) ? row0_1 : row0_2;
	  const int col0 = (col0_1 < col0_2) ? col0_1 : col0_2;
	  const int row1 = (row1_1 > row1_2) ? row1_1 : row1_2;
	  const int col1 = (col1_1 > col1_2) ? col1_1 : col1_2;
	  
	  const int nrow = row1 - row0 + 1;
	  const int ncol = col1 - col0 + 1;
	  if (nrow == reg1->nrow && ncol == reg1->ncol) { /* a perfect fit */
	      return(shRegIntLincom(reg1, reg2, a, b, c, LINCOM_INTERSECT));
	  } else {
	      REGION *out = shRegNew("union", nrow, ncol, reg1->type);
	      shRegIntSetVal(out, a);
	      out->row0 = row0; out->col0 = col0;
	      
	      if(shRegIntLincom(out, reg1, a, b, c, LINCOM_INTERSECT) == NULL ||
		 shRegIntLincom(out, reg2, a, b, c, LINCOM_INTERSECT) == NULL) {
		  shRegDel(out);

		  return(NULL);
	      }

	      out->mask = NULL;		/* XXX should be union of input masks */
	      phSpanmaskDel((SPANMASK *)reg1->mask); reg1->mask = NULL;
	      shRegDel(reg1);

	      return(out);
	  }
      } else {
	  shFatal("shRegIntLincom: Illegal value of flag: %d\n", flag);
      }
       
      return(reg1);
   }

   ncol = reg1->ncol;
   nrow = reg1->nrow;

   if(reg2 != NULL && (reg2->ncol != ncol || reg2->nrow != nrow)) {
      shError("shRegIntLincom: region sizes differ\n");
      return(NULL);
   }

   if(reg1->type == TYPE_U16) {
      U16 **rrow1 = reg1->rows_u16;
      U16 **rrow2 = (reg2 == NULL) ? NULL : reg2->rows_u16;
      U16 *row1, *row2;

      if(reg2 != NULL && reg1->type != reg2->type) {
	 shError("shRegIntLincom: region types differ\n");
	 return(NULL);
      }
      
      for(i = 0;i < nrow;i++) {
	 row1 = rrow1[i];
	 if(reg2 == NULL) {
	    for(j = 0;j < ncol;j++) {
	       val = a + b*row1[j] + 0.5;
	       row1[j] = val < 0 ? 0 : (val > MAX_U16 ? MAX_U16 : val);
	    }
	 } else {
	    row2 = rrow2[i];
	    for(j = 0;j < ncol;j++) {
	       val = a + b*row1[j] + c*row2[j] + 0.5;
	       row1[j] = val < 0 ? 0 : (val > MAX_U16 ? MAX_U16 : val);
	    }
	 }
      }
   } else if(reg1->type == TYPE_S32) {
      S32 **rrow1 = reg1->rows_s32;
      S32 **rrow2 = (reg2 == NULL) ? NULL : reg2->rows_s32;
      S32 *row1, *row2;
      
      if(reg2 != NULL && reg1->type != reg2->type) {
	 shError("shRegIntLincom: region types differ\n");
	 return(NULL);
      }
      
      for(i = 0;i < nrow;i++) {
	 row1 = rrow1[i];
	 if(reg2 == NULL) {
	    for(j = 0;j < ncol;j++) {
	       row1[j] = a + b*row1[j] + 0.5;
	    }
	 } else {
	    row2 = rrow2[i];
	    for(j = 0;j < ncol;j++) {
	       row1[j] = a + b*row1[j] + c*row2[j] + 0.5;
	    }
	 }
      }
   } else if(reg1->type == TYPE_FL32) {
      FL32 **rrow1 = reg1->rows_fl32, *row1;
      
      if(reg2 == NULL) {
	 for(i = 0;i < nrow;i++) {
	    row1 = rrow1[i];
	    for(j = 0;j < ncol;j++) {
	       row1[j] = a + b*row1[j];
	    }
	 }
      } else if(reg2->type == TYPE_U16) {
	 U16 **rrow2 = reg2->rows_u16, *row2;

	 for(i = 0;i < nrow;i++) {
	    row1 = rrow1[i];
	    row2 = rrow2[i];
	    for(j = 0;j < ncol;j++) {
	       row1[j] = a + b*row1[j] + c*(row2[j] - SOFT_BIAS);
	    }
	 }
      } else if(reg2->type == TYPE_FL32) {
	 FL32 **rrow2 = reg2->rows_fl32, *row2;

	 for(i = 0;i < nrow;i++) {
	    row1 = rrow1[i];
	    row2 = rrow2[i];
	    for(j = 0;j < ncol;j++) {
	       row1[j] = a + b*row1[j] + c*row2[j];
	    }
	 }
      } else {
	 shError("shRegIntLincom: region types are incompatible\n");
	 return(NULL);
      }
   } else {
      shError("shRegIntLincom "
	      "doesn't handle regions of type %d\n", reg1->type);
      return(NULL);
   }

   return(reg1);
}

/***************************************************************************
 * <AUTO EXTRACT>
 *
 * AND each pixel of an image with a mask; not all types are supported
 *
 * return: SH_SUCCESS		If region type is supported
 *         SH_GENERIC_ERROR	otherwise
 */
int
shRegIntLogand(REGION *reg,		/* The region ... */
	       const unsigned int mask)	/* the mask to be ANDed */
{
   int i;
   int ncol,nrow;			/* unpacked from reg */

   shAssert(reg != NULL);

   ncol = reg->ncol;
   nrow = reg->nrow;

   if(reg->type == TYPE_U8) {
      U8 **rptr = reg->rows_u8;
      U8 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    *ptr++ &= mask;
	 }
      }
   } else if(reg->type == TYPE_S8) {
      S8 **rptr = reg->rows_s8;
      S8 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    *ptr++ &= mask;
	 }
      }
   } else if(reg->type == TYPE_U16) {
      U16 **rptr = reg->rows_u16;
      U16 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    *ptr++ &= mask;
	 }
      }
   } else if(reg->type == TYPE_S16) {
      S16 **rptr = reg->rows_s16;
      S16 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    *ptr++ &= mask;
	 }
      }
   } else if(reg->type == TYPE_U32) {
      U32 **rptr = reg->rows_u32;
      U32 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    *ptr++ &= mask;
	 }
      }
   } else if(reg->type == TYPE_S32) {
      S32 **rptr = reg->rows_s32;
      S32 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    *ptr++ &= mask;
	 }
      }
   } else {
      shErrStackPush("shRegIntLogand doesn't handle regions of type %d\n",
		       reg->type);
      return(SH_GENERIC_ERROR);
   }

   return(SH_SUCCESS);
}

/*****************************************************************************/
/*
 * <AUTO EXTRACT>
 *
 * Convert a U16 region to S32, which is returned.
 *
 * If reg_s32 is non-NULL, it must be the same size  as reg_u16, and will
 * be the return value of the function. If it's NULL, a new region will
 * be allocated and returned.
 */
REGION *
phRegS32ToU16Convert(REGION *reg_s32,	/* output region; may be NULL */
		     const REGION *reg_u16) /* input region */
{
   int i, j;
   int nrow, ncol;			/* == reg_u16->n{row,col} */
   U16 *row_u16;			/* == reg_u16->rows_u16[?] */
   S32 *row_s32;			/* == reg_s32->rows_s32[?] */
   
   shAssert(reg_u16 != NULL && reg_u16->type == TYPE_U16);
   nrow = reg_u16->nrow; ncol = reg_u16->ncol;

   if(reg_s32 == NULL) {
      reg_s32 = shRegNew(reg_u16->name, nrow, ncol, TYPE_S32);
   } else {
      shAssert(reg_s32->type == TYPE_S32);
      shAssert(reg_s32->nrow == nrow && reg_s32->ncol == ncol);
   }

   for(i = 0;i < nrow;i++) {
      row_u16 = reg_u16->rows_u16[i];
      row_s32 = reg_s32->rows_s32[i];
      for(j = 0; j < ncol; j++) {
	 row_s32[j] = row_u16[j];
      }
   }

   return(reg_s32);
}

/*****************************************************************************/
/*
 * <AUTO EXTRACT>
 *
 * Return a shifted copy of a PIX region
 *
 * If out is non-NULL, it must be the same size as in, and will
 * be the return value of the function. If it's NULL, a new region will
 * be allocated and returned.
 *
 * If the scratch region, scr, is NULL one will be allocated and freed for
 * you.
 */
#define FILTSIZE (2*15 + 1)		/* max size of smoothing filters */

REGION *
phRegIntShift(REGION *out,		/* output region; may be NULL */
	      const REGION *in,		/* input region */
	      REGION *scr,		/* scratch space; may be NULL */
	      int filtsize,		/* size of sinc filter (must be odd) */
	      float dr,			/* shift by this far in row... */
	      float dc)			/*      and this far in column */
{
   int c;
   float filtr[FILTSIZE], filtc[FILTSIZE]; /* sinc filters */
   int own_scr;				/* do we own the scr region? */
   int i;
   int idc, idr;			/* integral part of shift */
   int nrow, ncol;			/* == in->n{row,col} */
   PIX **rows;				/* == out->ROWS */
   
   shAssert(in != NULL && in->type == TYPE_PIX);

   if(filtsize%2 == 0) filtsize++;
   if(filtsize > FILTSIZE) {
      shError("phRegIntShift: filter too large (max %d)", FILTSIZE);
      filtsize = FILTSIZE;
   }
   
   nrow = in->nrow; ncol = in->ncol;
/*
 * reduce desired shifts to the range [-0.5, 0.5]
 */
   idr = reduce_shift(dr, &dr);
   idc = reduce_shift(dc, &dc);
/*
 * Did we shift clean off the region?
 */
   if(idr <= -nrow || idr >= nrow || idc <= -ncol || idc >= ncol) {
      shErrStackPush("phRegIntShift: Attempting to shift by (%d,%d): "
		     " greater than region size (%dx%d)",
		     idr, idc, nrow, ncol);

      return(NULL);
   }
/*
 * Check/create regions
 */
   if(out == NULL) {
      out = shRegNew(in->name, nrow, ncol, in->type);
   }
   shAssert(out->type == in->type);
   shAssert(out->nrow == nrow && out->ncol == ncol);

   rows = out->ROWS;

   own_scr = (scr == NULL) ? 1 : 0;
   if(own_scr) {
      scr = shRegNew("shift scratch", nrow, ncol, in->type);
   }
   shAssert(scr->type == in->type);
   shAssert(scr->nrow == nrow && scr->ncol == ncol);
/*
 * prepare sinc filters, then do the fractional shift. Note the convention
 * in naming filters passed to phConvolve(), the first extends across columns
 */
   get_sync_with_cosbell(filtr, (filtsize + 1)/2, -dr);
   get_sync_with_cosbell(filtc, (filtsize + 1)/2, -dc);

   phConvolve(out, in, scr, filtsize, filtsize, filtc, filtr, 0,CONVOLVE_MULT);
/*
 * do the non-fractional shifts. Rows first
 */
   if(idr < 0) {
      idr = -idr;
      for(i = 0;i < nrow - idr;i++) {
	 memcpy(rows[i], rows[i + idr], ncol*sizeof(PIX));
      }
      for(;i < nrow;i++) {
	  for(c = 0; c < ncol; c++) {
	      rows[i][c] = SOFT_BIAS;
	  }
      }
   } else if(idr == 0) {
      ;				/* nothing to do */
   } else {
      for(i = nrow - 1; i >= idr;i--) {
	 memcpy(rows[i], rows[i - idr], ncol*sizeof(PIX));
      }
      for(;i >= 0;i--) {
	  for(c = 0; c < ncol; c++) {
	      rows[i][c] = SOFT_BIAS;
	  }
      }
   }
/*
 * and now columns
 */
   if(idc < 0) {
      idc = -idc;
      for(i = 0; i < nrow; i++) {
	 memmove(&rows[i][0], &rows[i][idc], (ncol - idc)*sizeof(PIX));
	 for(c = ncol - idc; c < ncol; c++) {
	     rows[i][c] = SOFT_BIAS;
	 }
      }
   } else if(idc == 0) {
      ;				/* nothing to do */
   } else {
      for(i = 0; i < nrow; i++) {
	 memmove(&rows[i][idc], &rows[i][0], (ncol - idc)*sizeof(PIX));
	 for(c = 0; c < idc; c++) {
	     rows[i][c] = SOFT_BIAS;
	 }
      }
   }
/*
 * cleanup
 */
   if(own_scr) {
      shRegDel(scr);
   }
   
   return(out);
}

/*****************************************************************************/
/*
 * <AUTO EXTRACT>
 *
 * Add an N(mean,sigma^2) Gaussian variate to each pixel.
 *
 *
 * We treat the U16 case specially; not only is the Gaussian variate
 * inlined, but the result is properly dithered (note: this means that
 * the routine will not add noise if the requested variance is less than
 * 1/12, the variance that would be added by dithering)
 *
 * return: SH_SUCCESS		If region type is supported
 *         SH_GENERIC_ERROR	otherwise
 */
int
phRegIntGaussianAdd(REGION *reg,	/* The region ... */
		    RANDOM *rand,	/* random numbers */
		    const float mean,	/* add an N(mean,sigma^2) Gaussian */
		    const float sigma)
{
   const float inorm = 1.0/(float)((1U<<(8*sizeof(int)-1)) - 1);
   float fac,r = 0,v1,v2;		/* initialise r to appease
					   the IRIX 5.3 cc */
   int i;
   int ncol,nrow;			/* unpacked from reg */

   shAssert(reg != NULL);

   ncol = reg->ncol;
   nrow = reg->nrow;

   (void)phRandomSeedSet(NULL, 0);	/* clear saved value */

   if(reg->type == TYPE_U8) {
      U8 **rptr = reg->rows_u8;
      U8 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    *ptr++ += mean + sigma*phGaussdev() + 0.5;
	 }
      }
   } else if(reg->type == TYPE_S8) {
      S8 **rptr = reg->rows_s8;
      S8 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    *ptr++ += mean + sigma*phGaussdev() + 0.5;
	 }
      }
   } else if(reg->type == TYPE_U16) {	/* inline gaussian for this case */
      U16 **rptr = reg->rows_u16;
      U16 *ptr,*end;
      float var2;			/* twice the variance corrected
					   for dither noise */

      var2 = 2*(sigma*sigma - 1/12.0);
      if(var2 <= 0) {
	 return(SH_SUCCESS);		/* desired sigma's less than dither
					   noise */
      }

      DECLARE_PHRANDOM(rand);
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 if(ncol&01) {			/* odd number of columns in loop */
	    float gdev = phGaussdev();	/* in different line from PHRANDOM */
	    *ptr++ += mean + sqrt(var2/2)*gdev + (PHRANDOM & 0x1);
	 }
	 while(ptr < end) {
	    do {
	       v1 = PHRANDOM*inorm;
	       v2 = PHRANDOM*inorm;
	       r = v1*v1+v2*v2;
	    } while (r >= 1.0);
	    fac = sqrt(-var2*log(r)/r);
	    
	    *ptr++ += mean + fac*v1 + (PHRANDOM & 0x1);
	    *ptr++ += mean + fac*v2 + (PHRANDOM & 0x1);
	 }
      }
      END_PHRANDOM(rand);
   } else if(reg->type == TYPE_S16) {
      S16 **rptr = reg->rows_s16;
      S16 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    *ptr++ += mean + sigma*phGaussdev() + 0.5;
	 }
      }
   } else if(reg->type == TYPE_U32) {
      U32 **rptr = reg->rows_u32;
      U32 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    *ptr++ += mean + sigma*phGaussdev() + 0.5;
	 }
      }
   } else if(reg->type == TYPE_S32) {
      S32 **rptr = reg->rows_s32;
      S32 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    *ptr++ += mean + sigma*phGaussdev() + 0.5;
	 }
      }
   } else {
      shError("phRegIntGaussianAdd doesn't handle regions of type %d\n",
	      reg->type);
      return(SH_GENERIC_ERROR);
   }

   return(SH_SUCCESS);
}

/*****************************************************************************/
/*
 * <AUTO EXTRACT>
 *
 * Add an N(0,sigma^2) Gaussian variate to each pixel, where the
 * variance is the (intensity - bkgd)/gain.
 *
 * We treat the U16 case specially; not only is the Gaussian variate
 * inlined, but the result is properly dithered (note: this means that
 * the routine will not add noise to pixels whose variance is less than
 * 1/12, the variance that would be added by dithering)
 *
 * If poisson is true, rather than adding Gaussian noise we sample each
 * pixel value assuming the given gain (that is, we assume that the
 * number of electrons is (intensity - bkgd)*gain, draw a sample from
 * a Poisson process with this mean, divide by gain, and reinstate the bkgd)
 *
 * return: SH_SUCCESS		If region type is supported
 *         SH_GENERIC_ERROR	otherwise
 */
int
phRegIntNoiseAdd(REGION *reg,		/* The region ... */
		 RANDOM *rand,		/* random numbers */
		 const int bkgd,	/* background to subtract */
		 const float gain,	/* add an N(0,(I - bkgd)/gain)
					   Gaussian */
		 const int poisson)	/* don't use a Gaussian; rather
					   Poisson sample (I - bkgd) */
{
   const float inorm = 1.0/(float)((1U<<(8*sizeof(int)-1)) - 1);
   float fac,r = 0,v1,v2;		/* initialise r to appease
					   the IRIX 5.3 cc */
   int i;
   int ncol,nrow;			/* unpacked from reg */
   const float igain = 1/gain;		/* inverse gain */
   float sigma;				/* s.d. of a pixel */

   shAssert(reg != NULL);

   ncol = reg->ncol;
   nrow = reg->nrow;

   if(poisson) {			/* Poisson not Gaussian */
      float mu;
      if(reg->type == TYPE_FL32) {
	 FL32 **rptr = reg->rows_fl32;
	 FL32 *ptr,*end;
	 
	 for(i = 0;i < nrow;i++) {
	    ptr = rptr[i];
	    end = ptr + ncol;
	    while(ptr < end) {
	       mu = (*ptr - bkgd)*gain;
	       *ptr++ = bkgd + phPoissondev(mu)*igain;
	    }
	 }
      } else {
	 shError("shRegIntNoiseAdd doesn't handle regions of type %d when poisson is true\n",
		 reg->type);
	 return(SH_GENERIC_ERROR);
      }

      return(SH_SUCCESS);
   }

   if(reg->type == TYPE_U8) {
      U8 **rptr = reg->rows_u8;
      U8 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    sigma = sqrt((*ptr - bkgd)*igain);
	    *ptr++ += sigma*phGaussdev() + 0.5;
	 }
      }
   } else if(reg->type == TYPE_S8) {
      S8 **rptr = reg->rows_s8;
      S8 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    sigma = sqrt((*ptr - bkgd)*igain);
	    *ptr++ += sigma*phGaussdev() + 0.5;
	 }
      }
   } else if(reg->type == TYPE_U16) {
      U16 **rptr = reg->rows_u16;
      U16 *ptr,*end;
      float tmp;
      
      DECLARE_PHRANDOM(rand);
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 if(ncol&01) {			/* even number of columns in loop */
	    fac = (*ptr - bkgd)*igain - 1/12.0;
	    if(fac > 0) {
	       *ptr += sqrt(fac)*phGaussdev() + (PHRANDOM & 0x1);
	    }
	    ptr++;
	 }
	 while(ptr < end) {
	    do {
	       v1 = PHRANDOM*inorm;
	       v2 = PHRANDOM*inorm;
	       r = v1*v1+v2*v2;
	    } while (r >= 1.0);
	    tmp = -2.0*log(r)/(r*gain);
	    
	    fac = tmp*(*ptr - bkgd) - 1/12.0;
	    if(fac > 0) {
	       *ptr += sqrt(fac)*v1 + (PHRANDOM & 0x1);
	    }
	    ptr++;
	    
	    fac = tmp*(*ptr - bkgd) - 1/12.0;
	    if(fac > 0) {
	       *ptr += sqrt(fac)*v2 + (PHRANDOM & 0x1);
	    }
	    ptr++;
	 }
      }
      END_PHRANDOM(rand);
   } else if(reg->type == TYPE_S16) {
      S16 **rptr = reg->rows_s16;
      S16 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    sigma = sqrt((*ptr - bkgd)*igain);
	    *ptr++ += sigma*phGaussdev() + 0.5;
	 }
      }
   } else if(reg->type == TYPE_U32) {
      U32 **rptr = reg->rows_u32;
      U32 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    sigma = sqrt((*ptr - bkgd)*igain);
	    *ptr++ += sigma*phGaussdev() + 0.5;
	 }
      }
   } else if(reg->type == TYPE_S32) {
      S32 **rptr = reg->rows_s32;
      S32 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    sigma = sqrt((*ptr - bkgd)*igain);
	    *ptr++ += sigma*phGaussdev() + 0.5;
	 }
      }
   } else if(reg->type == TYPE_FL32) {
      FL32 **rptr = reg->rows_fl32;
      FL32 *ptr,*end;
      
      for(i = 0;i < nrow;i++) {
	 ptr = rptr[i];
	 end = ptr + ncol;
	 while(ptr < end) {
	    sigma = sqrt((*ptr - bkgd)*igain);
	    *ptr++ += sigma*phGaussdev();
	 }
      }
   } else {
      shError("shRegIntNoiseAdd doesn't handle regions of type %d\n",
	      reg->type);
      return(SH_GENERIC_ERROR);
   }

   return(SH_SUCCESS);
}

/*****************************************************************************/
/*
 * <AUTO EXTRACT>
 *
 * Given a set of values at a set of positions, return the coefficients
 * of the LSQ best fit polynomial
 */
#define RC_SCALE 1e-3			/* scale factor for rowc/colc coeffs */

ACOEFF *
phPolynomialFit(float *val,		/* values to be fit */
		float *valErr,		/* errors in values; or NULL */
		float *row,		/* row positions of val[] */
		float *col,		/* column positions of val[] */
		int n,			/* number of points in {val,row,col} */
		int nterm_row,		/* number of terms; in row and  */
		int nterm_col,		/*     column direction. linear == 2 */
		float *pmean,		/* return mean of fit (or NULL) */
		float *psig)		/* return s.d. of fit (or NULL) */
{
   ACOEFF *acoeff = phAcoeffNew(nterm_row, nterm_col);
   MAT *A;				/* normal equations for LSQ fit are */
   VEC *b;				/*      A*w = b */
   int i, j, k;
   VEC *lambda;				/* eigen values */
   const int nparam = nterm_row*nterm_col; /* number of parameters to fit */
   MAT *Q;				/* eigen vectors */
   double sum;				/* as it says, a sum of something */
   float *variance;			/* variance of val[] */
   VEC *w;				/* desired weights */
   float *zcoeffs_i, *zcoeffs_j;	/* row[]^n*col[]^m */

   shAssert(n >= 1);
/*
 * if valErr is NULL, use equal weights for all objects
 */
   variance = alloca(n*sizeof(float));
   if(valErr == NULL) {
      for(i = 0; i < n; i++) {
	 variance[i] = 1;
      }
   } else {
      for(i = 0; i < n; i++) {
	 shAssert(valErr[i] > 0.0);
	 variance[i] = 1/(valErr[i]*valErr[i]);
      }
   }

   shAssert(nparam > 0);
/*
 * allocate needed matrices.  The zcoeffs_[ij] are used to make the
 * code easier to write (and read); they are the appropriate powers
 * of position for each element of val[] for each elements of A and b
 */
   zcoeffs_i = alloca(2*n*sizeof(float));
   zcoeffs_j = zcoeffs_i + n;

   A = phMatNew(nparam, nparam);
   b = phVecNew(nparam);
   lambda = NULL;
   Q = phMatNew(nparam, nparam);
/*
 * and fill out the matrices for the normal equations, Aw = b.
 */
   for(i = 0; i < nparam; i++) {
      for(k = 0; k < n; k++) {
	 zcoeffs_i[k] =
	   pow(RC_SCALE*row[k], i%nterm_row)*pow(RC_SCALE*col[k], i/nterm_row);
      }
      
      sum = 0.0;
      for(k = 0; k < n; k++) {
	 sum += zcoeffs_i[k]*val[k]*variance[k];
      }
      b->ve[i] = sum;
      
      for(j = i; j < nparam; j++) {
	 for(k = 0; k < n; k++) {
	    zcoeffs_j[k] =
	   pow(RC_SCALE*row[k], j%nterm_row)*pow(RC_SCALE*col[k], j/nterm_row);
	 }

	 sum = 0.0;
	 for(k = 0; k < n; k++) {
	    sum += zcoeffs_i[k]*zcoeffs_j[k]*variance[k];
	 }
	 A->me[j][i] = A->me[i][j] = sum;
      }
   }
/*
 * solve the system, replacing any eigenvalues that are too small with 0,
 * which phEigenBackSub() will interpret as infinity.
 */
   lambda = phEigen(A, Q, lambda);

   for(i = 0; i < nparam; i++) {
      if(fabs(lambda->ve[i]) < 1e-6) {
	 lambda->ve[i] = 0.0;
      }
   }

   w = phEigenBackSub(Q, lambda, b);
/*
 * pack the results into the ACOEFF
 */
   for(k = 0; k < nparam; k++) {
      acoeff->c[k%nterm_row][k/nterm_row] = w->ve[k];
   }
/*
 * clean up fitting code
 */
   phMatDel(A);
   phVecDel(b);
   phVecDel(lambda);
   phMatDel(Q);
   phVecDel(w);
/*
 * Calculate statistics of fit if so desired
 */
   if(pmean == NULL && psig == NULL) {
      return(acoeff);
   }

   {
      float mean;			/* desired mean */
      float *tmp = variance;		/* just an alias for clarity */

      sum = 0;
      for(k = 0; k < n; k++) {
	 tmp[k] = phPolynomialEval(acoeff, row[k], col[k]);
	 sum += tmp[k];
      }
      mean = sum/n;
      if(pmean != NULL) *pmean = mean;

      if(psig != NULL) {
	 sum = 0.0;
	 for(k = 0; k < n; k++) {
	    sum += (tmp[k] - mean)*(tmp[k] - mean);
	 }

	 *psig = (n == 1) ? 0 : sqrt(sum/(n - 1));
      }
   }

   return(acoeff);
}

/*****************************************************************************/
/*
 * <AUTO EXTRACT>
 *
 * Evaluate an ACOEFF at a given location
 */
float
phPolynomialEval(const ACOEFF *acoeff,	/* ACOEFF to evaluate */
		 float row, float col)	/* desired position */
{
   int i, j;
   double sum = 0.0;

   for(i = 0; i < acoeff->nrow; i++) {
      for(j = 0; j < acoeff->ncol; j++) {
	 sum += acoeff->c[i][j]*pow(RC_SCALE*row, i)*pow(RC_SCALE*col, j);
      }
   }

   return(sum);
}

/*****************************************************************************/
/*
 * <AUTO EXTRACT>
 * Set the bits in a MASK to val within a circle of radius r, centred
 * at (rowc, colc)
 */
void
phMaskSetFromCircle(MASK *mask,		/* the mask to set */
		    char val,		/* to this value */
		    float rowc, float colc, /* desired centre */
		    float rad)		/* desired radius */
{
   int r, c;				/* row and column counters */
   int cstart, cend;			/* range of pixels in a row */
   float hlen;				/* length of a span */
   int irow;				/* index of row containg centre */
   int irad;				/* (int)rad + a little bit */
   int nrow, ncol;			/* == mask->n{row,col} */
   unsigned char *row;			/* == mask->rows[.] */
   
   shAssert(mask != NULL && rad >= 0);

   nrow = mask->nrow; ncol = mask->ncol;
   rowc -= mask->row0;
   colc -= mask->col0;

   irow = (int)rowc;
   irad = rad + 2;			/* "+ 2" allows for rounding problems*/

   for(r = irow - irad; r <= irow + irad;r++) {
      if(r < 0 || r >= nrow) continue;
      hlen = rad*rad - (r - rowc)*(r - rowc);
      if(hlen < 0) continue;

      hlen = sqrt(hlen);
      cstart = ((int)(colc - hlen + 0.5) < 0) ? 0 : (int)(colc - hlen + 0.5);
      cend = ((int)(colc + hlen + 0.5) >= ncol) ?
					   ncol - 1 : (int)(colc + hlen + 0.5);

      row = mask->rows[r];
      for(c = cstart;c < cend; c++) {
	 row[c] |= val;
      }
   }
}

/*****************************************************************************/
/*
 * <AUTO EXTRACT>
 *
 * Use the system qsort to sort a chain
 *
 *
 * N.b. this routine belongs in shChain.c; it assumes knowledge of
 * CHAIN internals
 *
 * Note also that any cursors that the chain might have may be invalid
 * when this routine returns. This routine doesn't bother to go through
 * all of the possible cursors checking if any exist.
 */
void
shChainQsort(CHAIN *chain,
	     int (*compar)(const void *, const void *))
{
   void **arr;				/* unpack chain into this array */
   CHAIN_ELEM *elem;			/* a link of a chain */
   int i;
   int n;				/* length of chain */

   shAssert(chain != NULL && chain->type != shTypeGetFromName("GENERIC"));

   if((n = chain->nElements) <= 1) {		/* nothing to do */
      return;
   }
/*
 * extract chain into arr[]
 */
   arr = alloca(n*sizeof(void *));

   for(elem = chain->pFirst, i = 0; elem != NULL; elem = elem->pNext, i++) {
      arr[i] = elem->pElement;
   }
/*
 * call the system qsort to do the work
 */
   qsort(arr, n, sizeof(void *), compar);
/*
 * and rebuild the chain
 */
   for(elem = chain->pFirst, i = 0; elem != NULL; elem = elem->pNext, i++) {
      elem->pElement = arr[i];
   }
}

/*****************************************************************************/
/*
 * a qsort comparison function
 */
static int
compar_s32(const S32 *a, const S32 *b)
{
   return(*a - *b);
}

/*****************************************************************************/
/*
 * Find the median of a set of PIX data
 */
/*
 * sort a PIX array in place using Shell's method
 */
static void
shshsort(PIX *arr, int n)
{
    unsigned int i, j, inc;
    PIX t;
    
    inc=1;
    do{
        inc *= 3;
        inc++;
    }while(inc <= n);
    do{
        inc /= 3;
        for(i=inc;i<n;i++){
            t = arr[i];
            j=i;
            while(arr[j-inc] > t){
                arr[j] = arr[j-inc];
                j -= inc;
                if(j<inc) break;
            }
            arr[j] = t;
        }
    } while(inc > 1);
}

/*
 * sorts an array using a Shell sort, and find the mean and quartiles.
 *
 * If clip is false, use all the data; otherwise find the quartiles for
 * the main body of the histogram, and reevaluate the mean for the main body.
 *
 * The quartile algorithm assumes that the data are distributed uniformly
 * in the histogram cells, and the quartiles are thus linearly interpolated
 * in the cumulative histogram. This is about as good as one can do with
 * dense histograms, and for sparse ones is as good as anything.
 *
 * The returned value is the median, and equals qt[1]
 *
 * This code is taken from photo/src/extract_utils.c
 */
float
phQuartilesGetFromArray(const void *arr, /* the data values */
			int type,	/* type of data */
			int n,		/* number of points */
			int clip,	/* should we clip histogram? */
			float *qt,	/* quartiles (may be NULL) */
			float *mean,	/* mean (may be NULL) */
			float *sig)	/* s.d. (may be NULL) */
{
    int i;
    int sum;
    PIX *data;				/* a modifiable copy of arr */
    register int np;
    const PIX *p;
    int ldex,udex;
    float fdex;
    float fldex;
    int npass;				/* how many passes? 2 => trim */
    int pass;				/* which pass through array? */
    int cdex;    
    int dcell;
    int dlim = 0;
    float qt_s[3];			/* storage for qt is required */

    if(type == TYPE_S32) {		/* handle S32 specially */
       S32 *sdata;			/* modifiable copy of data */
       shAssert(!clip);			/* not (yet?) supported for S32 */
       shAssert(n > 0);
       shAssert(n < 10000);		/* this routine is not optimised */

       sdata = alloca((n + 1)*sizeof(S32)); memcpy(sdata, arr, n*sizeof(S32));
       qsort(sdata, n, sizeof(S32),
	     (int (*)(const void *, const void *))compar_s32);

       if(n%2 == 1) {
	  return(sdata[n/2]);
       } else {
	  return(0.5*(sdata[n/2] + sdata[n/2 + 1]));
       }
    }

    shAssert(type == TYPE_PIX && n > 0);

    if(qt == NULL) {
       qt = qt_s;
    }
    
    npass = clip ? 2 : 1;

    data = alloca((n + 1)*sizeof(PIX)); memcpy(data, arr, n*sizeof(PIX));
    shshsort(data, n);

    for(pass=0;pass < npass;pass++){
       for(i = 0;i < 3;i++) {
	  fdex = 0.25*(float)((i+1)*n);	/*float index*/
	  cdex = fdex;
	  dcell = data[cdex];
	  ldex = cdex;
	  if(ldex > 0) {
	     while(data[--ldex] == dcell && ldex > 0) continue;
	     /* ldex is now the last index for which data<cdex */
	     
	     if(ldex > 0 || data[ldex] != dcell) {
		/* we stopped before the end or we stopped at the
		 * end but would have stopped anyway, so bump it up; 
		 */
		ldex++;
	     }
	  }
	  /* The value of the cumulative histogram at the left edge of the
	   * dcell cell is ldex; ie exactly ldex values lie strictly below
	   * dcell, and data=dcell BEGINS at ldex.
	   */
	  udex = cdex;
	  while(udex < n && data[++udex] == dcell) continue;
	  /* first index for which data>cdex or the end of the array, 
	   * whichever comes first. This can run off the end of
	   * the array, but it does not matter; if data[n] is accidentally
	   * equal to dcell, udex == n on the next go and it falls out 
	   * before udex is incremented again. */
	  
	  /* now the cumulative histogram at the right edge of the dcell
	   * cell is udex-1, and the number of instances for which the data
	   * are equal to dcell exactly is udex-ldex. Thus if we assume
	   * that the data are distributed uniformly within a histogram
	   * cell, the quartile can be computed:
	   */
	  fldex = ldex; 
	  
	  shAssert(udex != ldex);
	  qt[i] = dcell - 1 + 0.5 + (fdex - fldex)/(float)(udex-ldex);
	  
	  /* The above is all OK except for one singular case: if the
	   * quartile is EXACTLY at a histogram cell boundary (a half-integer) 
	   * as computed above AND the previous histogram cell is empty, the
	   * result is not intuitively correct, though the 'real' answer 
	   * is formally indeterminate even with the unform-population-in-
	   * cells ansatz. The cumulative histogram has a segment of zero
	   * derivative in this cell, and intuitively one would place the
	   * quartile in the center of this segment; the algorithm above
	   * places it always at the right end. This code, which can be
	   * omitted, fixes this case.
	   *
	   * We only have to do something if the quartile is exactly at a cell
	   * boundary; in this case ldex cannot be at either end of the array,
	   * so we do not need to worry about the array boundaries .
	   */
	  if(4*ldex == (i+1)*n) {
	     int zext = dcell - data[ldex-1] - 1;
	     
	     if(zext > 0) {
		/* there is at least one empty cell in the histogram
		 * prior to the first data==dcell one
		 */
		qt[i] -= 0.5*zext;
	     }
	  }
       }

       if(npass == 1) {			/* no trimming to be done */
	  if(sig != NULL) {
	     *sig = IQR_TO_SIGMA*(qt[2] - qt[0]);
	  }
       } else {
	  /*
	   * trim the histogram if possible to the first percentile below
	   * and the +2.3 sigma point above
	   */
	  if(pass==0){
	     /* terminate data array--array must be of size (n+1) (JEG code) */
	     data[n] = 0x7fff;
	     /* trim histogram */
	     ldex = .01*n;		/* index in sorted data array at first
					   percentile */
	     dlim = qt[1] + 2.3*IQR_TO_SIGMA*(qt[2] - qt[0]) + 0.5;
	     if(dlim >= data[n-1] || udex >= n) {  /* off top of data or
						      already at end */
		if(ldex == 0) {
		   if(sig != NULL) {
		      *sig = IQR_TO_SIGMA*(qt[2] - qt[0]);
		   }
		   break;		/* histogram is too small; we're done*/
		}
	     } else {
		/* find the index corresponding to 2.3 sigma; this should be
		 * done by a binary search */
		udex--; 
		while(data[++udex] <= dlim){;}
		n = udex - ldex;
		data = data + ldex;
	     }
	  }else{   /* have trimmed hist and recomputed quartiles */
	     if(sig != NULL) {
		*sig = 1.025*IQR_TO_SIGMA*(qt[2]-qt[0]);
	     }
	  }
       }
    }

    if(mean != NULL) {
       sum = 0;
       np = n;
       p = data;
       while(np--){
	  sum += *p++;
       }
       *mean = (float)sum/(float)n;
    }

    return(qt[1]);
}    

/*****************************************************************************/
/*
 * <AUTO EXTRACT>
 *
 * Calculate a CRC for a string of characters. You can call this routine
 * repeatedly to build up a CRC. Only the last 16bits are significant.
 *
 * e.g.
 *   crc = phCrcCalc(0, buff, n) & 0xFFFF;
 * or
 *   crc = 0;
 *   crc = phCrcCalc(crc, buff0, n);
 *   crc = phCrcCalc(crc, buff1, n);
 *   crc = phCrcCalc(crc, buff2, n);
 *   crc &= 0xFFFF;
 */
long
phCrcCalc(long crc,			/* initial value of CRC (e.g. 0) */
	  const char *buff,		/* buffer to be CRCed */
	  int n)			/* number of chars in buff */
{
   register long c;
   static long crcta[16] = {
      0L, 010201L, 020402L, 030603L, 041004L,
      051205L, 061406L, 071607L, 0102010L,
      0112211L, 0122412L, 0132613L, 0143014L,
      0153215L, 0163416L, 0173617L
   };					/* CRC generation tables */
   static long crctb[16] = {
      0L, 010611L, 021422L, 031233L, 043044L,
      053655L, 062466L, 072277L, 0106110L,
      0116701L, 0127532L, 0137323L, 0145154L,
      0155745L, 0164576L, 0174367L
   };
   const char *ptr = buff;		/* pointers to buff */
   const char *const end = buff + n;	/*        and to end of desired data */

   for(;ptr != end;ptr++) {
      c = crc ^ (long)(*ptr);
      crc = (crc >> 8) ^ (crcta[(c & 0xF0) >> 4] ^ crctb[c & 0x0F]);
   }

   return(crc);
}

#define BUFSIZE 4096

long
phCrcCalcFromFile(const char *file,	/* file to be CRCed */
		  int nbyte)		/* number of chars to process */
{
   char buff[BUFSIZE];			/* input buffer */
   long crc = 0;			/* desired CRC */
   FILE *fil;				/* FILE pointer for file in question */
   int nread;				/* number of bytes read */

   if(nbyte <= 0) {
      nbyte = -1;
   }

   if((fil = fopen(file,"r")) == NULL) {
      return(0);
   }

   crc = 0;
   while(nbyte != 0 && (nread = fread(buff,1,BUFSIZE,fil)) > 0) {
      if(nbyte > 0 && nbyte < nread) {
	 nread = nbyte;
      }

      crc = phCrcCalc(crc, buff, nread);

      nbyte -= nread;
   }
   fclose(fil);

   crc &= 0xFFFF;

   return(crc);
}

/*****************************************************************************/
/*
 * This is like tmpfile, but some versions of tmpfile only return 26 unique
 * names; mktemp() and tmpnam() are no better. This code tries up to 26**3
 * unique names for a given PID
 *
 * What is more, this version really creates the file in /tmp not /
 * as tmpfile is reputed to do on OSF/1 (see PR 323)
 */
#define NCHAR 62                        /* number of chars used in names */
#define NTMP (26*26*26)                 /* number of temp names tried */

static char
get09azAZ(int c)
{
   c %= NCHAR;
   if(c < 10) {
      return(c + '0');
   }
   c -= 10;
   if(c < 26) {
      return(c + 'a');
   }
   c -= 26;
   return(c + 'A');
}

FILE *
phTmpfile(void)
{
   char name[101];
   static int id = 0;
   int i;
   int id0;
   int len;
   FILE *fil;
   const char *path = "/tmp/photo_XXXXXX"; /* desired filename template */
   int pid = getpid();
   int tmp;

   strncpy(name, path, 100);
   len = strlen(name);
  
   shAssert(len > 6 && strncmp(&name[len - 6],"XXXXXX",6) == 0);
/*
 * convert the pid to a 3-character string
 */
   for(i = 4;i <= 6;i++) {
      name[len - i] = get09azAZ(pid%NCHAR); pid /= NCHAR;
   }

   for(id0 = (id + NTMP - 1)%NTMP;id != id0;id = (id + 1)%NTMP) {
      tmp = id;
      for(i = 1;i <= 3;i++) {
         name[len - i] = get09azAZ(tmp%NCHAR); tmp /= NCHAR;
      }
      if((fil = fopen(name, "r")) == NULL) { /* doesn't exist. Good */
	 fil = fopen(name,"wb+");	/* open for read/write */
	 unlink(name);			/* delete file */
         return(fil);
      } else {
         fclose(fil);
      }
   }

   errno = ENFILE;			/* ~ posix specification */
   shError("Failed to find unique name of the form %s\n",path);
   return(NULL);
}

/*****************************************************************************/
/*
 * Set a floor to a region's values
 */
void
phRegFloor(REGION *reg,			/* the region */
	   float min)			/* desired minimum value */
{
   int i, j;
   const PIX minpix = FLT2PIX(min);	/* minimum value */
   int nrow, ncol;			/* == reg->n{row,col} */
   PIX *row;				/* == reg->ROWS[] */

   shAssert(reg != NULL && reg->type == TYPE_PIX);
   nrow = reg->nrow; ncol = reg->ncol;

   for(i = 0; i < nrow; i++) {
      row = reg->ROWS[i];
      for(j = 0; j < ncol; j++) {
	 row[j] = (row[j] > minpix) ? row[j] : minpix;
      }
   }
}




/********************** ORPOL() *****************************************/
/* package for orthogonal polynomial construction and fitting  */
/* JEG  version 22:25 2003 21 Feb */
/* 
 * from alpha version, normalized argument in tchebyfit() to avoid
 * problems with dynamic range with large arguments and high order
 */
#define MAXPTS 2048   

#define MAXORD 50
#define MAXORP (MAXORD + 1)

static VECTOR_TYPE aorp[ MAXORP*MAXORP ] ;
static VECTOR_TYPE *ap[ MAXORP] ;
static VECTOR_TYPE q[ MAXORP ] ;	/* normalizations */
static VECTOR_TYPE p[MAXORP*MAXPTS] ;	/* MAXORP*MAXPTS */
static VECTOR_TYPE *pv[ MAXORP ] ;
static int npt ;
static VECTOR_TYPE *wts ;
/* 
 * "version" number to allow checking whether polynomials need
 * regenerating; incremented by mkorpol each time it is called
 */
static int ver_orpol = 0;     

static VECTOR_TYPE tchwt[ MAXPTS ];
static VECTOR_TYPE tchx[ MAXPTS ];

/* `dot' product */
static double 
dp(const VECTOR_TYPE *a1,
   VECTOR_TYPE *a2,
   VECTOR_TYPE *a3)
{
    double dot ;
    int i ;
    dot = 0. ;
    for ( i=0 ; i < npt ; i++ ) dot += a1[i] * a2[i] * a3[i] * (*(wts+i)) ;
    return dot ;
}

/************************ MKORPOL()*************************************/ 
/*
 * mkorpol makes a set of polynomials which are orthogonal with respect
 * to the weights w; ie SUM w[i]*Pj(x[i])*Pk(x[i]) = q[j]delta(j,k).  If
 * you wish to construct a set of orthogonal functions which are f(x)Pj(x),
 * just use a set of weights which are w' = w*f(x)^2; the routine will
 * construct the poly part.
 */

static void 
mkorpol(VECTOR_TYPE *w, VECTOR_TYPE *x, int n, int m)  
/* w are the weights */  
/* x is the abscissa array */
/* n is the number of points */
/* m is the maximum order */
{
	int i,j ;
	double alpha, beta ;
	int maxo2 = MAXORP*MAXORP ;
	int maxop = MAXORP*MAXPTS ;
	int maxorp = MAXORP;
	
        ver_orpol++;
	wts = w ;
	npt = n ;

	shAssert(npt <= MAXPTS);
	shAssert(m <= MAXORD);

	for ( i=0 ; i< maxo2 ; i++ )
		aorp[i] = 0. ;
	for ( i=0 ; i < maxorp ; i++ )  /* pointers to polys */
		ap[i] = aorp + i*maxorp ;
	for ( i=0 ; i < maxop ; i++ )    /* poly value matrix */
		p[i] = 0. ;
	for ( i=0 ; i < n ; i++ )        
		*(p+i) = 1. ;     /* p0 is identically 1 */
	for ( i=0 ; i < maxorp ; i++ ) 
		pv[i] = p + i * n ;
	q[0] = dp( p,p,p ) ;
	aorp[0] = 1. ;
	for ( j=1 ; j <= m ; j++ ) {
		if ( j > 1 ){
			beta = -q[j-1]/q[j-2] ;
		}else{
			beta = 0. ;
		}
		alpha = -dp(x, pv[j-1] , pv[j-1] )/ q[j-1] ;
		for (i=0 ; i<n ; i++ )
			*(*(pv+j)+i) = (x[i] + alpha) * (*(*(pv+j-1)+i))
				+ beta * ( j > 1 ? (*(*(pv+j-2)+i)) : 0. )  ;
		for (i=1 ; i <= j ; i++ )
			*(*(ap+j)+i) = *(*(ap+j-1) + i-1 );
		for (i=0 ; i<j ; i++ )
			*(*(ap+j)+i) += alpha * (*(*(ap+j-1)+i))
				+ beta * ( j>1 ? (*(*(ap+j-2)+i)) : 0. ) ;
		q[j] = dp( pv[j], pv[j], p ) ;
	}
}


/************************** ORPFIT() *****************************************/
/* 
 * orpfit() fits an orthopoly expansion to an array of ordinates.
 * y is the ordinate array, m the max order, ay the polynomial
 * coefficients. orpfit expects VECTOR_TYPE coef, note. If you are
 * fitting to a set f(x)Pj(x) as described in mkorpoly(), the
 * ordinates presented to this routine need to be the ratio 
 * of the real ordinates to f(x)   
 */


static void
orpfit(const VECTOR_TYPE *y,
       VECTOR_TYPE *ay, 
       int m)   
{
    double bc ;
    int i ;
    int j ;

    for (i=0 ; i <= m ; i++ ) ay[i] = 0. ;
    for ( j=0 ; j <= m ; j++ ) {
        bc = dp(y,pv[j],p)/q[j] ;
        for (i=0 ; i <= j ; i++) ay[i] += bc * (*(*(ap+j)+i)) ;
    }
}



/****************************** FPOLY() **************************************/
/* fpoly evaluates a polynomial defined by VECTOR_TYPE coefficients a, order m, 
 * at double argument z.
 */

static double 
fpoly(double z, VECTOR_TYPE *a, int m) 
{
    int i ;
    VECTOR_TYPE p ;
    p = 0. ;
    for ( i=m ; i >0 ; i-- ) p = z * (p + a[i]) ;
    p += a[0] ;
    return p ;
}

/************************ TCHEBYFIT ****************************************/
/*
 * This routine constructs tchebychev-like polynomials on a grid and fits
 * a proferred set of y values. The weights used are the input weights
 * times the tchebychev weights. The abscissa array is ASSUMED to be 
 * uniform and the `continuous' fitting interval from xb = -0.5*(x[1]-x[0]) to
 * xe= x[npt-1] + 0.5*(x[npt-1] - x[npt-2]), which is to say that the 
 * tchebychev weights are proportional to 1 / sqrt(1 - X^2) where
 * X = (2*x - (xb+xe))/(xe-xb) ranges from not quite -1 to not quite +1
 * over the abscissa net.
 *
 * x is the abscissa array, VECTOR_TYPE
 * y is the ordinate array to be fitted, VECTOR_TYPE
 * w is the input weight array, VECTOR_TYPE
 * n is the number of points in the net on which x,y,w are defined
 * m is the order of the fit desired.
 * fitx is the net of abscissae upon which fity is evaluated. If this
 *      argument is zero (NULL), the output net is assumed to be the
 *      same array as x and number as n.
 * fity is the table of fitted output values, which need not be evaluated 
 *      on the same net as the input values.
 * fitn is the number of points in the output array; if fitx = NULL, it is
 *      ignored.
 *
 * Note that the tchebychev-like basis set and the fitting polynomial are never
 * seen or felt--only the input and output tables; the routine is a very
 * good interpolator/smoother.
 */
 
void
phTchebyfit(const VECTOR_TYPE *x,
	    const VECTOR_TYPE *y,
	    const VECTOR_TYPE *w,
	    int n, int m,
	    const VECTOR_TYPE *fitx,
	    VECTOR_TYPE *fity, 
            int fitn)
{
    int i;
    double xe = x[n-1] + 0.5*(x[n-1] - x[n-2]);
    double xb = x[0] - 0.5*(x[1] - x[0]);
    double xx, X;
    VECTOR_TYPE ay[MAXORP];
        
    shAssert(n <= MAXPTS);
    shAssert(m <= MAXORP);
    shAssert((xb - xe) != 0.);
    
    for(i = 0; i < n; i++){
        xx = x[i];
        X = (2. * xx - (xb + xe))/(xe - xb);
        tchwt[i] = w[i] /sqrt(1. - X*X) ; 
        tchx[i] = X;   /* X's range is approx -1 to 1 */
    }
    mkorpol(tchwt, tchx, n, m);
    orpfit(y,ay,m);        
    if(fitx == (VECTOR_TYPE *)NULL){
        fitx = x;
        fitn = n;
    }
    for(i = 0; i < fitn; i++){
        X = (2. * fitx[i] - (xb + xe))/(xe-xb);
        fity[i] = fpoly(X, ay, m);
    }
}

/************************************************************************************************************/
/*
 * <AUTO EXTRACT>
 *
 * Given a region, extrapolate to add another row and column
 *
 * This is useful because we only save the real values of sky BINREGIONs
 * but we need the extrapolated values to call phSkySubtract
 */
REGION *
phRegionExtrapolate(REGION *out,	     /* output region, or NULL */
		    const REGION *in)	     /* input region */
{
    int r, c;
    REGION *tmp;
    BINREGION *breg = phBinregionNew();	/* allows us to call phBinregionInterpolate */

    shAssert(in != NULL && in->type == TYPE_S32);

    if(out == NULL) {
	out = shRegNew("", in->nrow + 1, in->ncol + 1, TYPE_S32);
    }
    shAssert(out->nrow == in->nrow + 1 && out->ncol == in->ncol + 1);
    /*
     * Copy in into the bottom left of out
     */
    tmp = shSubRegNew("", out, in->nrow, in->ncol, 0, 0, NO_FLAGS);
    shRegIntCopy(tmp, in);
    shRegDel(tmp);
    /*
     * And now use phBinregionInterpolate() to fill out the top and right
     */
    breg->reg = (REGION *)in;		/* we promise not to change it, and will free it in a moment */
    breg->bin_row = breg->bin_col = 1;

    c = out->ncol - 1;
    for(r = 0; r < out->nrow; r++) {
	out->rows_s32[r][c] = phBinregionInterpolate(breg, r, c);
    }

    r = out->nrow - 1;
    for(c = 0; c < out->ncol - 1; c++) {
	out->rows_s32[r][c] = phBinregionInterpolate(breg, r, c);
    }
    /*
     * Clean up
     */
    breg->reg = NULL;
    phBinregionDel(breg);

    return(out);
}

/*****************************************************************************/

PIXDATATYPE
phTypenameToType(const char *typeStr)
{
   PIXDATATYPE type;

   if (typeStr == NULL) {
       return 0;
   }

   if(strcmp(typeStr, "U8") == 0) {
      type = TYPE_U8;
   } else if(strcmp(typeStr, "S8") == 0) {
      type = TYPE_S8;
   } else if(strcmp(typeStr, "U16") == 0) {
      type = TYPE_U16;
   } else if(strcmp(typeStr, "S16") == 0) {
      type = TYPE_S16;
   } else if(strcmp(typeStr, "S32") == 0) {
      type = TYPE_S32;
   } else if(strcmp(typeStr, "FL32") == 0) {
      type = TYPE_FL32;
   } else {
      type = 0;
   }

   return type;
}

static void *
photoMemEmptyFunc(size_t nbyte)
{
    shAssert (nbyte > 0);			/* make compilers happy */

   if (strategic_memory_reserve == NULL) {
       shFatal("PHOTO internal error(1): out of system memory");
       return(NULL);
   } else {
       fprintf(stderr, "Releasing strategic memory reserve\n");
       free(strategic_memory_reserve);
       strategic_memory_reserve = NULL;
       
       shMemDefragment(1);

       return NULL;
   }
}

int
phStrategicMemoryReserveSet(const size_t size)
{
    free(strategic_memory_reserve); strategic_memory_reserve = NULL;

    if (size == 0) {
	allocated_reserve = 0;
	shMemEmptyCB(NULL);

	return 1;
    }

    strategic_memory_reserve = malloc(size);
    allocated_reserve = 1;
    
    shMemEmptyCB(photoMemEmptyFunc);

    return (strategic_memory_reserve == NULL) ? 0 : 1;
}

#endif
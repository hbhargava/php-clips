   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  01/06/16             */
   /*                                                     */
   /*                    MEMORY MODULE                    */
   /*******************************************************/

/*************************************************************/
/* Purpose: Memory allocation routines.                      */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Removed HaltExecution check from the           */
/*            EnvReleaseMem function. DR0863                 */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Corrected code to remove compiler warnings.    */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems.                   */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Removed genlongalloc/genlongfree functions.    */
/*                                                           */
/*            Added get_mem and rtn_mem macros.              */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Removed deallocating message parameter from    */
/*            EnvReleaseMem.                                 */
/*                                                           */
/*            Removed support for BLOCK_MEMORY.              */
/*                                                           */
/*************************************************************/

#include <stdio.h>

#include "setup.h"

#include "constant.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "router.h"
#include "utility.h"

#include <stdlib.h>

#if WIN_MVC
#include <malloc.h>
#endif

#define STRICT_ALIGN_SIZE sizeof(double)

#define SpecialMalloc(sz) malloc((STD_SIZE) sz)
#define SpecialFree(ptr) free(ptr)

/********************************************/
/* InitializeMemory: Sets up memory tables. */
/********************************************/
void InitializeMemory(
  void *theEnv)
  {
   AllocateEnvironmentData(theEnv,MEMORY_DATA,sizeof(struct memoryData),NULL);

   MemoryData(theEnv)->OutOfMemoryFunction = DefaultOutOfMemoryFunction;

#if (MEM_TABLE_SIZE > 0)
   MemoryData(theEnv)->MemoryTable = (struct memoryPtr **)
                 malloc((STD_SIZE) (sizeof(struct memoryPtr *) * MEM_TABLE_SIZE));

   if (MemoryData(theEnv)->MemoryTable == NULL)
     {
      PrintErrorID(theEnv,"MEMORY",1,true);
      EnvPrintRouter(theEnv,WERROR,"Out of memory.\n");
      EnvExitRouter(theEnv,EXIT_FAILURE);
     }
   else
     {
      int i;
      
      for (i = 0; i < MEM_TABLE_SIZE; i++) MemoryData(theEnv)->MemoryTable[i] = NULL;
     }
#else // MEM_TABLE_SIZE == 0
      MemoryData(theEnv)->MemoryTable = NULL;
#endif 
  }

/***************************************************/
/* genalloc: A generic memory allocation function. */
/***************************************************/
void *genalloc(
  void *theEnv,
  size_t size)
  {
   char *memPtr;
      
   memPtr = (char *) malloc(size);
        
   if (memPtr == NULL)
     {
      EnvReleaseMem(theEnv,(long) ((size * 5 > 4096) ? size * 5 : 4096));
      memPtr = (char *) malloc(size);
      if (memPtr == NULL)
        {
         EnvReleaseMem(theEnv,-1L);
         memPtr = (char *) malloc(size);
         while (memPtr == NULL)
           {
            if ((*MemoryData(theEnv)->OutOfMemoryFunction)(theEnv,size))
              return(NULL);
            memPtr = (char *) malloc(size);
           }
        }
     }

   MemoryData(theEnv)->MemoryAmount += (long) size;
   MemoryData(theEnv)->MemoryCalls++;

   return((void *) memPtr);
  }

/***********************************************/
/* DefaultOutOfMemoryFunction: Function called */
/*   when the KB runs out of memory.           */
/***********************************************/
bool DefaultOutOfMemoryFunction(
  void *theEnv,
  size_t size)
  {
#if MAC_XCD
#pragma unused(size)
#endif

   PrintErrorID(theEnv,"MEMORY",1,true);
   EnvPrintRouter(theEnv,WERROR,"Out of memory.\n");
   EnvExitRouter(theEnv,EXIT_FAILURE);
   return(true);
  }

/***********************************************************/
/* EnvSetOutOfMemoryFunction: Allows the function which is */
/*   called when the KB runs out of memory to be changed.  */
/***********************************************************/
bool (*EnvSetOutOfMemoryFunction(void *theEnv,bool (*functionPtr)(void *,size_t)))(void *,size_t)
  {
   bool (*tmpPtr)(void *,size_t);

   tmpPtr = MemoryData(theEnv)->OutOfMemoryFunction;
   MemoryData(theEnv)->OutOfMemoryFunction = functionPtr;
   return(tmpPtr);
  }

/****************************************************/
/* genfree: A generic memory deallocation function. */
/****************************************************/
int genfree(
  void *theEnv,
  void *waste,
  size_t size)
  {   
   free(waste);

   MemoryData(theEnv)->MemoryAmount -= (long) size;
   MemoryData(theEnv)->MemoryCalls--;

   return(0);
  }

/******************************************************/
/* genrealloc: Simple (i.e. dumb) version of realloc. */
/******************************************************/
void *genrealloc(
  void *theEnv,
  void *oldaddr,
  size_t oldsz,
  size_t newsz)
  {
   char *newaddr;
   unsigned i;
   size_t limit;

   newaddr = ((newsz != 0) ? (char *) gm2(theEnv,newsz) : NULL);

   if (oldaddr != NULL)
     {
      limit = (oldsz < newsz) ? oldsz : newsz;
      for (i = 0 ; i < limit ; i++)
        { newaddr[i] = ((char *) oldaddr)[i]; }
      for ( ; i < newsz; i++)
        { newaddr[i] = '\0'; }
      rm(theEnv,(void *) oldaddr,oldsz);
     }

   return((void *) newaddr);
  }

/********************************/
/* EnvMemUsed: C access routine */
/*   for the mem-used command.  */
/********************************/
long int EnvMemUsed(
  void *theEnv)
  {
   return(MemoryData(theEnv)->MemoryAmount);
  }

/************************************/
/* EnvMemRequests: C access routine */
/*   for the mem-requests command.  */
/************************************/
long int EnvMemRequests(
  void *theEnv)
  {
   return(MemoryData(theEnv)->MemoryCalls);
  }

/***************************************/
/* UpdateMemoryUsed: Allows the amount */
/*   of memory used to be updated.     */
/***************************************/
long int UpdateMemoryUsed(
  void *theEnv,
  long int value)
  {
   MemoryData(theEnv)->MemoryAmount += value;
   return(MemoryData(theEnv)->MemoryAmount);
  }

/*******************************************/
/* UpdateMemoryRequests: Allows the number */
/*   of memory requests to be updated.     */
/*******************************************/
long int UpdateMemoryRequests(
  void *theEnv,
  long int value)
  {
   MemoryData(theEnv)->MemoryCalls += value;
   return(MemoryData(theEnv)->MemoryCalls);
  }

/***********************************/
/* EnvReleaseMem: C access routine */
/*   for the release-mem command.  */
/***********************************/
long int EnvReleaseMem(
  void *theEnv,
  long int maximum)
  {
   struct memoryPtr *tmpPtr, *memPtr;
   int i;
   long int returns = 0;
   long int amount = 0;

   for (i = (MEM_TABLE_SIZE - 1) ; i >= (int) sizeof(char *) ; i--)
     {
      YieldTime(theEnv);
      memPtr = MemoryData(theEnv)->MemoryTable[i];
      while (memPtr != NULL)
        {
         tmpPtr = memPtr->next;
         genfree(theEnv,(void *) memPtr,(unsigned) i);
         memPtr = tmpPtr;
         amount += i;
         returns++;
         if ((returns % 100) == 0)
           { YieldTime(theEnv); }
        }
      MemoryData(theEnv)->MemoryTable[i] = NULL;
      if ((amount > maximum) && (maximum > 0))
        { return(amount); }
     }

   return(amount);
  }

/*****************************************************/
/* gm1: Allocates memory and sets all bytes to zero. */
/*****************************************************/
void *gm1(
  void *theEnv,
  size_t size)
  {
   struct memoryPtr *memPtr;
   char *tmpPtr;
   size_t i;

   if (size < (long) sizeof(char *)) size = sizeof(char *);

   if (size >= MEM_TABLE_SIZE)
     {
      tmpPtr = (char *) genalloc(theEnv,(unsigned) size);
      for (i = 0 ; i < size ; i++)
        { tmpPtr[i] = '\0'; }
      return((void *) tmpPtr);
     }

   memPtr = (struct memoryPtr *) MemoryData(theEnv)->MemoryTable[size];
   if (memPtr == NULL)
     {
      tmpPtr = (char *) genalloc(theEnv,(unsigned) size);
      for (i = 0 ; i < size ; i++)
        { tmpPtr[i] = '\0'; }
      return((void *) tmpPtr);
     }

   MemoryData(theEnv)->MemoryTable[size] = memPtr->next;

   tmpPtr = (char *) memPtr;
   for (i = 0 ; i < size ; i++)
     { tmpPtr[i] = '\0'; }

   return ((void *) tmpPtr);
  }

/*****************************************************/
/* gm2: Allocates memory and does not initialize it. */
/*****************************************************/
void *gm2(
  void *theEnv,
  size_t size)
  {
#if (MEM_TABLE_SIZE > 0)
   struct memoryPtr *memPtr;
#endif

   if (size < sizeof(char *)) size = sizeof(char *);

#if (MEM_TABLE_SIZE > 0)
   if (size >= MEM_TABLE_SIZE) return(genalloc(theEnv,(unsigned) size));

   memPtr = (struct memoryPtr *) MemoryData(theEnv)->MemoryTable[size];
   if (memPtr == NULL)
     {
      return(genalloc(theEnv,size));
     }

   MemoryData(theEnv)->MemoryTable[size] = memPtr->next;

   return ((void *) memPtr);
#else
   return(genalloc(theEnv,size));
#endif
  }

/*****************************************************/
/* gm3: Allocates memory and does not initialize it. */
/*****************************************************/
void *gm3(
  void *theEnv,
  size_t size)
  {
#if (MEM_TABLE_SIZE > 0)
   struct memoryPtr *memPtr;
#endif

   if (size < (long) sizeof(char *)) size = sizeof(char *);

#if (MEM_TABLE_SIZE > 0)
   if (size >= MEM_TABLE_SIZE) return(genalloc(theEnv,size));

   memPtr = (struct memoryPtr *) MemoryData(theEnv)->MemoryTable[(int) size];
   if (memPtr == NULL)
     { return(genalloc(theEnv,size)); }

   MemoryData(theEnv)->MemoryTable[(int) size] = memPtr->next;

   return ((void *) memPtr);
#else
   return(genalloc(theEnv,size));
#endif
  }

/****************************************/
/* rm: Returns a block of memory to the */
/*   maintained pool of free memory.    */
/****************************************/
int rm(
  void *theEnv,
  void *str,
  size_t size)
  {
#if (MEM_TABLE_SIZE > 0)
   struct memoryPtr *memPtr;
#endif

   if (size == 0)
     {
      SystemError(theEnv,"MEMORY",1);
      EnvExitRouter(theEnv,EXIT_FAILURE);
     }

   if (size < sizeof(char *)) size = sizeof(char *);

#if (MEM_TABLE_SIZE > 0)
   if (size >= MEM_TABLE_SIZE) return(genfree(theEnv,(void *) str,size));

   memPtr = (struct memoryPtr *) str;
   memPtr->next = MemoryData(theEnv)->MemoryTable[size];
   MemoryData(theEnv)->MemoryTable[size] = memPtr;
#else
   return(genfree(theEnv,(void *) str,size));
#endif

   return(1);
  }

/********************************************/
/* rm3: Returns a block of memory to the    */
/*   maintained pool of free memory that's  */
/*   size is indicated with a long integer. */
/********************************************/
int rm3(
  void *theEnv,
  void *str,
  size_t size)
  {
#if (MEM_TABLE_SIZE > 0)
   struct memoryPtr *memPtr;
#endif

   if (size == 0)
     {
      SystemError(theEnv,"MEMORY",1);
      EnvExitRouter(theEnv,EXIT_FAILURE);
     }

   if (size < (long) sizeof(char *)) size = sizeof(char *);
   
#if (MEM_TABLE_SIZE > 0)
   if (size >= MEM_TABLE_SIZE) return(genfree(theEnv,(void *) str,size));

   memPtr = (struct memoryPtr *) str;
   memPtr->next = MemoryData(theEnv)->MemoryTable[(int) size];
   MemoryData(theEnv)->MemoryTable[(int) size] = memPtr;
#else
   return(genfree(theEnv,(void *) str,size));
#endif

   return(1);
  }

/***************************************************/
/* PoolSize: Returns number of bytes in free pool. */
/***************************************************/
unsigned long PoolSize(
  void *theEnv)
  {
   unsigned long cnt = 0;

#if (MEM_TABLE_SIZE > 0)
   register int i;
   struct memoryPtr *memPtr;

   for (i = sizeof(char *) ; i < MEM_TABLE_SIZE ; i++)
     {
      memPtr = MemoryData(theEnv)->MemoryTable[i];
      while (memPtr != NULL)
        {
         cnt += (unsigned long) i;
         memPtr = memPtr->next;
        }
     }
#endif

   return(cnt);
  }

/***************************************************************/
/* ActualPoolSize : Returns number of bytes DOS requires to    */
/*   store the free pool.  This routine is functionally        */
/*   equivalent to pool_size on anything other than the IBM-PC */
/***************************************************************/
unsigned long ActualPoolSize(
  void *theEnv)
  {
   return(PoolSize(theEnv));
  }

/********************************************/
/* EnvSetConserveMemory: Allows the setting */
/*    of the memory conservation flag.      */
/********************************************/
bool EnvSetConserveMemory(
  void *theEnv,
  bool value)
  {
   bool ov;

   ov = MemoryData(theEnv)->ConserveMemory;
   MemoryData(theEnv)->ConserveMemory = value;
   return(ov);
  }

/*******************************************/
/* EnvGetConserveMemory: Returns the value */
/*    of the memory conservation flag.     */
/*******************************************/
bool EnvGetConserveMemory(
  void *theEnv)
  {
   return(MemoryData(theEnv)->ConserveMemory);
  }

/**************************/
/* genmemcpy:             */
/**************************/
void genmemcpy(
  char *dst,
  char *src,
  unsigned long size)
  {
   unsigned long i;

   for (i = 0L ; i < size ; i++)
     dst[i] = src[i];
  }

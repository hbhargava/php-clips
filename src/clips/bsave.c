   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  01/06/16             */
   /*                                                     */
   /*                     BSAVE MODULE                    */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides core routines for saving constructs to  */
/*   a binary file.                                          */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Added environment parameter to GenClose.       */
/*            Added environment parameter to GenOpen.        */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Used genstrncpy instead of strncpy.            */
/*                                                           */
/*            Borland C (IBM_TBC) and Metrowerks CodeWarrior */
/*            (MAC_MCW, IBM_MCW) are no longer supported.    */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*************************************************************/

#include "setup.h"

#include "argacces.h"
#include "bload.h"
#include "cstrnbin.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "memalloc.h"
#include "moduldef.h"
#include "router.h"
#include "symblbin.h"

#include "bsave.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if BLOAD_AND_BSAVE
   static void                        FindNeededItems(void *);
   static void                        InitializeFunctionNeededFlags(void *);
   static void                        WriteNeededFunctions(void *,FILE *);
   static size_t                      FunctionBinarySize(void *);
   static void                        WriteBinaryHeader(void *,FILE *);
   static void                        WriteBinaryFooter(void *,FILE *);
#endif
   static void                        DeallocateBsaveData(void *);

/**********************************************/
/* InitializeBsaveData: Allocates environment */
/*    data for the bsave command.             */
/**********************************************/
void InitializeBsaveData(
  void *theEnv)
  {
   AllocateEnvironmentData(theEnv,BSAVE_DATA,sizeof(struct bsaveData),DeallocateBsaveData);
  }
  
/************************************************/
/* DeallocateBsaveData: Deallocates environment */
/*    data for the bsave command.               */
/************************************************/
static void DeallocateBsaveData(
  void *theEnv)
  {
   struct BinaryItem *tmpPtr, *nextPtr;
   
   tmpPtr = BsaveData(theEnv)->ListOfBinaryItems;
   while (tmpPtr != NULL)
     {
      nextPtr = tmpPtr->next;
      rtn_struct(theEnv,BinaryItem,tmpPtr);
      tmpPtr = nextPtr;
     }
  }

/**************************************/
/* BsaveCommand: H/L access routine   */
/*   for the bsave command.           */
/**************************************/
void BsaveCommand(
  UDFContext *context,
  CLIPSValue *returnValue)
  {
#if (! RUN_TIME) && BLOAD_AND_BSAVE
   const char *fileName;

   fileName = GetFileName(context);
   if (fileName != NULL)
     {
      if (EnvBsave(UDFContextEnvironment(context),fileName))
        {
         mCVSetBoolean(returnValue,true);
         return;
        }
     }
#else
#if MAC_XCD
#pragma unused(context)
#endif
#endif
   mCVSetBoolean(returnValue,false);
  }

#if BLOAD_AND_BSAVE

/******************************/
/* EnvBsave: C access routine */
/*   for the bsave command.   */
/******************************/
bool EnvBsave(
  void *theEnv,
  const char *fileName)
  {
   FILE *fp;
   struct BinaryItem *biPtr;
   char constructBuffer[CONSTRUCT_HEADER_SIZE];
   long saveExpressionCount;

   /*===================================*/
   /* A bsave can't occur when a binary */
   /* image is already loaded.          */
   /*===================================*/

   if (Bloaded(theEnv))
     {
      PrintErrorID(theEnv,"BSAVE",1,false);
      EnvPrintRouter(theEnv,WERROR,
          "Cannot perform a binary save while a binary load is in effect.\n");
      return(0);
     }

   /*================*/
   /* Open the file. */
   /*================*/

   if ((fp = GenOpen(theEnv,fileName,"wb")) == NULL)
     {
      OpenErrorMessage(theEnv,"bsave",fileName);
      return(0);
     }

   /*==============================*/
   /* Remember the current module. */
   /*==============================*/

   SaveCurrentModule(theEnv);

   /*==================================*/
   /* Write binary header to the file. */
   /*==================================*/

   WriteBinaryHeader(theEnv,fp);

   /*===========================================*/
   /* Initialize count variables, index values, */
   /* and determine some of the data structures */
   /* which need to be saved.                   */
   /*===========================================*/

   ExpressionData(theEnv)->ExpressionCount = 0;
   InitializeFunctionNeededFlags(theEnv);
   InitAtomicValueNeededFlags(theEnv);
   FindHashedExpressions(theEnv);
   FindNeededItems(theEnv);
   SetAtomicValueIndices(theEnv,false);

   /*===============================*/
   /* Save the functions and atoms. */
   /*===============================*/

   WriteNeededFunctions(theEnv,fp);
   WriteNeededAtomicValues(theEnv,fp);

   /*=========================================*/
   /* Write out the number of expression data */
   /* structures in the binary image.         */
   /*=========================================*/

   GenWrite((void *) &ExpressionData(theEnv)->ExpressionCount,(unsigned long) sizeof(unsigned long),fp);

   /*===========================================*/
   /* Save the numbers indicating the amount of */
   /* memory needed to bload the constructs.    */
   /*===========================================*/

   for (biPtr = BsaveData(theEnv)->ListOfBinaryItems;
        biPtr != NULL;
        biPtr = biPtr->next)
     {
      if (biPtr->bsaveStorageFunction != NULL)
        {
         genstrncpy(constructBuffer,biPtr->name,CONSTRUCT_HEADER_SIZE);
         GenWrite(constructBuffer,(unsigned long) CONSTRUCT_HEADER_SIZE,fp);
         (*biPtr->bsaveStorageFunction)(theEnv,fp);
        }
     }

   /*====================================*/
   /* Write a binary footer to the file. */
   /*====================================*/

   WriteBinaryFooter(theEnv,fp);

   /*===================*/
   /* Save expressions. */
   /*===================*/

   ExpressionData(theEnv)->ExpressionCount = 0;
   BsaveHashedExpressions(theEnv,fp);
   saveExpressionCount = ExpressionData(theEnv)->ExpressionCount;
   BsaveConstructExpressions(theEnv,fp);
   ExpressionData(theEnv)->ExpressionCount = saveExpressionCount;

   /*===================*/
   /* Save constraints. */
   /*===================*/

   WriteNeededConstraints(theEnv,fp);

   /*==================*/
   /* Save constructs. */
   /*==================*/

   for (biPtr = BsaveData(theEnv)->ListOfBinaryItems;
        biPtr != NULL;
        biPtr = biPtr->next)
     {
      if (biPtr->bsaveFunction != NULL)
        {
         genstrncpy(constructBuffer,biPtr->name,CONSTRUCT_HEADER_SIZE);
         GenWrite(constructBuffer,(unsigned long) CONSTRUCT_HEADER_SIZE,fp);
         (*biPtr->bsaveFunction)(theEnv,fp);
        }
     }

   /*===================================*/
   /* Save a binary footer to the file. */
   /*===================================*/

   WriteBinaryFooter(theEnv,fp);

   /*===========*/
   /* Clean up. */
   /*===========*/

   RestoreAtomicValueBuckets(theEnv);

   /*=================*/
   /* Close the file. */
   /*=================*/

   GenClose(theEnv,fp);

   /*=============================*/
   /* Restore the current module. */
   /*=============================*/

   RestoreCurrentModule(theEnv);

   /*==================================*/
   /* Return true to indicate success. */
   /*==================================*/

   return(true);
  }

/*********************************************/
/* InitializeFunctionNeededFlags: Marks each */
/*   function in the list of functions as    */
/*   being unneeded by this binary image.    */
/*********************************************/
static void InitializeFunctionNeededFlags(
  void *theEnv)
  {
   struct FunctionDefinition *functionList;

   for (functionList = GetFunctionList(theEnv);
        functionList != NULL;
        functionList = functionList->next)
     { functionList->bsaveIndex = 0; }
  }

/**********************************************************/
/* FindNeededItems: Searches through the constructs for   */
/*   the functions, constraints, or atoms that are needed */
/*   by that construct. This routine also counts the      */
/*   number of expressions in use (through a global).     */
/**********************************************************/
static void FindNeededItems(
  void *theEnv)
  {
   struct BinaryItem *biPtr;

   for (biPtr = BsaveData(theEnv)->ListOfBinaryItems;
        biPtr != NULL;
        biPtr = biPtr->next)
     { if (biPtr->findFunction != NULL) (*biPtr->findFunction)(theEnv); }
  }

/****************************************************/
/* WriteNeededFunctions: Writes the names of needed */
/*   functions to the binary save file.             */
/****************************************************/
static void WriteNeededFunctions(
  void *theEnv,
  FILE *fp)
  {
   unsigned long int count = 0;
   size_t space, length;
   struct FunctionDefinition *functionList;

   /*================================================*/
   /* Assign each function an index if it is needed. */
   /*================================================*/

   for (functionList = GetFunctionList(theEnv);
        functionList != NULL;
        functionList = functionList->next)
     {
      if (functionList->bsaveIndex)
        { functionList->bsaveIndex = (short int) count++; }
      else
        { functionList->bsaveIndex = -1; }
     }

   /*===================================================*/
   /* Write the number of function names to be written. */
   /*===================================================*/

   GenWrite(&count,(unsigned long) sizeof(unsigned long int),fp);
   if (count == 0)
     {
      GenWrite(&count,(unsigned long) sizeof(unsigned long int),fp);
      return;
     }

   /*================================*/
   /* Determine the amount of space  */
   /* needed for the function names. */
   /*================================*/

   space = FunctionBinarySize(theEnv);
   GenWrite(&space,(unsigned long) sizeof(unsigned long int),fp);

   /*===============================*/
   /* Write out the function names. */
   /*===============================*/

   for (functionList = GetFunctionList(theEnv);
        functionList != NULL;
        functionList = functionList->next)
     {
      if (functionList->bsaveIndex >= 0)
        {
         length = strlen(ValueToString(functionList->callFunctionName)) + 1;
         GenWrite((void *) ValueToString(functionList->callFunctionName),(unsigned long) length,fp);
        }
     }
  }

/*********************************************/
/* FunctionBinarySize: Determines the number */
/*   of bytes needed to save all of the      */
/*   function names in the binary save file. */
/*********************************************/
static size_t FunctionBinarySize(
  void *theEnv)
  {
   size_t size = 0;
   struct FunctionDefinition *functionList;

   for (functionList = GetFunctionList(theEnv);
        functionList != NULL;
        functionList = functionList->next)
     {
      if (functionList->bsaveIndex >= 0)
        { size += strlen(ValueToString(functionList->callFunctionName)) + 1; }
     }

   return(size);
  }

/***************************************************/
/* SaveBloadCount: Used to save the data structure */
/*   count values when a binary save command is    */
/*   issued when a binary image is loaded.         */
/***************************************************/
void SaveBloadCount(
  void *theEnv,
  long cnt)
  {
   BLOADCNTSV *tmp, *prv;

   tmp = get_struct(theEnv,bloadcntsv);
   tmp->val = cnt;
   tmp->nxt = NULL;

   if (BsaveData(theEnv)->BloadCountSaveTop == NULL)
     { BsaveData(theEnv)->BloadCountSaveTop = tmp; }
   else
     {
      prv = BsaveData(theEnv)->BloadCountSaveTop;
      while (prv->nxt != NULL)
        { prv = prv->nxt; }
      prv->nxt = tmp;
     }
  }

/**************************************************/
/* RestoreBloadCount: Restores the data structure */
/*   count values after a binary save command is  */
/*   completed when a binary image is loaded.     */
/**************************************************/
void RestoreBloadCount(
  void *theEnv,
  long *cnt)
  {
   BLOADCNTSV *tmp;

   *cnt = BsaveData(theEnv)->BloadCountSaveTop->val;
   tmp = BsaveData(theEnv)->BloadCountSaveTop;
   BsaveData(theEnv)->BloadCountSaveTop = BsaveData(theEnv)->BloadCountSaveTop->nxt;
   rtn_struct(theEnv,bloadcntsv,tmp);
  }

/**********************************************/
/* MarkNeededItems: Examines an expression to */
/*   determine which items are needed to save */
/*   an expression as part of a binary image. */
/**********************************************/
void MarkNeededItems(
  void *theEnv,
  struct expr *testPtr)
  {
   while (testPtr != NULL)
     {
      switch (testPtr->type)
        {
         case SYMBOL:
         case STRING:
         case GBL_VARIABLE:
         case INSTANCE_NAME:
            ((SYMBOL_HN *) testPtr->value)->neededSymbol = true;
            break;

         case FLOAT:
            ((FLOAT_HN *) testPtr->value)->neededFloat = true;
            break;

         case INTEGER:
            ((INTEGER_HN *) testPtr->value)->neededInteger = true;
            break;

         case FCALL:
            ((struct FunctionDefinition *) testPtr->value)->bsaveIndex = 1;
            break;

         case RVOID:
           break;

         default:
           if (EvaluationData(theEnv)->PrimitivesArray[testPtr->type] == NULL) break;
           if (EvaluationData(theEnv)->PrimitivesArray[testPtr->type]->bitMap)
             { ((BITMAP_HN *) testPtr->value)->neededBitMap = true; }
           break;

        }

      if (testPtr->argList != NULL)
        { MarkNeededItems(theEnv,testPtr->argList); }

      testPtr = testPtr->nextArg;
     }
  }

/******************************************************/
/* WriteBinaryHeader: Writes a binary header used for */
/*   verification when a binary image is loaded.      */
/******************************************************/
static void WriteBinaryHeader(
  void *theEnv,
  FILE *fp)
  {
   GenWrite((void *) BloadData(theEnv)->BinaryPrefixID,(unsigned long) strlen(BloadData(theEnv)->BinaryPrefixID) + 1,fp);
   GenWrite((void *) BloadData(theEnv)->BinaryVersionID,(unsigned long) strlen(BloadData(theEnv)->BinaryVersionID) + 1,fp);
  }

/******************************************************/
/* WriteBinaryFooter: Writes a binary footer used for */
/*   verification when a binary image is loaded.      */
/******************************************************/
static void WriteBinaryFooter(
  void *theEnv,
  FILE *fp)
  {
   char footerBuffer[CONSTRUCT_HEADER_SIZE];

   genstrncpy(footerBuffer,BloadData(theEnv)->BinaryPrefixID,CONSTRUCT_HEADER_SIZE);
   GenWrite(footerBuffer,(unsigned long) CONSTRUCT_HEADER_SIZE,fp);
  }

#endif /* BLOAD_AND_BSAVE */

#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE

/**********************************************************/
/* AddBinaryItem: Informs the bload/bsave commands of the */
/*   appropriate access functions needed to save/load the */
/*   data structures of a construct or other "item" to a  */
/*   binary file.                                         */
/**********************************************************/
bool AddBinaryItem(
  void *theEnv,
  const char *name,
  int priority,
  void (*findFunction)(void *),
  void (*expressionFunction)(void *,FILE *),
  void (*bsaveStorageFunction)(void *,FILE *),
  void (*bsaveFunction)(void *,FILE *),
  void (*bloadStorageFunction)(void *),
  void (*bloadFunction)(void *),
  void (*clearFunction)(void *))
  {
   struct BinaryItem *newPtr, *currentPtr, *lastPtr = NULL;

   /*========================================*/
   /* Create the binary item data structure. */
   /*========================================*/

   newPtr = get_struct(theEnv,BinaryItem);

   newPtr->name = name;
   newPtr->findFunction = findFunction;
   newPtr->expressionFunction = expressionFunction;
   newPtr->bsaveStorageFunction = bsaveStorageFunction;
   newPtr->bsaveFunction = bsaveFunction;
   newPtr->bloadStorageFunction = bloadStorageFunction;
   newPtr->bloadFunction = bloadFunction;
   newPtr->clearFunction = clearFunction;
   newPtr->priority = priority;

   /*=================================*/
   /* If no binary items are defined, */
   /* just put the item on the list.  */
   /*=================================*/

   if (BsaveData(theEnv)->ListOfBinaryItems == NULL)
     {
      newPtr->next = NULL;
      BsaveData(theEnv)->ListOfBinaryItems = newPtr;
      return(true);
     }

   /*=========================================*/
   /* Otherwise, place the binary item at the */
   /* appropriate place in the list of binary */
   /* items based on its priority.            */
   /*=========================================*/

   currentPtr = BsaveData(theEnv)->ListOfBinaryItems;
   while ((currentPtr != NULL) ? (priority < currentPtr->priority) : false)
     {
      lastPtr = currentPtr;
      currentPtr = currentPtr->next;
     }

   if (lastPtr == NULL)
     {
      newPtr->next = BsaveData(theEnv)->ListOfBinaryItems;
      BsaveData(theEnv)->ListOfBinaryItems = newPtr;
     }
   else
     {
      newPtr->next = currentPtr;
      lastPtr->next = newPtr;
     }

   /*==================================*/
   /* Return true to indicate the item */
   /* was successfully added.          */
   /*==================================*/

   return(true);
  }

#endif /* BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE */





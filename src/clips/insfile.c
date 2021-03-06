   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  01/06/16             */
   /*                                                     */
   /*         INSTANCE LOAD/SAVE (ASCII/BINARY) MODULE    */
   /*******************************************************/

/*************************************************************/
/* Purpose:  File load/save routines for instances           */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Added environment parameter to GenClose.       */
/*            Added environment parameter to GenOpen.        */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Corrected code to remove compiler warnings.    */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            For save-instances, bsave-instances, and       */
/*            bload-instances, the class name does not       */
/*            have to be in scope if the module name is      */
/*            specified.                                     */
/*                                                           */
/*      6.40: Added Env prefix to GetEvaluationError and     */
/*            SetEvaluationError functions.                  */
/*                                                           */
/*************************************************************/

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */

#include <stdlib.h>

#include "setup.h"

#if OBJECT_SYSTEM

#include "argacces.h"
#include "classcom.h"
#include "classfun.h"
#include "memalloc.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#if DEFTEMPLATE_CONSTRUCT && DEFRULE_CONSTRUCT
#include "factmngr.h"
#endif
#include "inscom.h"
#include "insfun.h"
#include "insmngr.h"
#include "inspsr.h"
#include "object.h"
#include "router.h"
#include "strngrtr.h"
#include "symblbin.h"
#include "sysdep.h"

#include "insfile.h"

/* =========================================
   *****************************************
                   CONSTANTS
   =========================================
   ***************************************** */
#define MAX_BLOCK_SIZE 10240

/* =========================================
   *****************************************
               MACROS AND TYPES
   =========================================
   ***************************************** */
struct bsaveSlotValue
  {
   long slotName;
   unsigned valueCount;
  };

struct bsaveSlotValueAtom
  {
   unsigned short type;
   long value;
  };

/* =========================================
   *****************************************
      INTERNALLY VISIBLE FUNCTION HEADERS
   =========================================
   ***************************************** */

static long InstancesSaveCommandParser(UDFContext *,const char *,long (*)(void *,const char *,int,
                                                   EXPRESSION *,bool));
static DATA_OBJECT *ProcessSaveClassList(void *,const char *,EXPRESSION *,int,bool);
static void ReturnSaveClassList(void *,DATA_OBJECT *);
static long SaveOrMarkInstances(void *,void *,int,DATA_OBJECT *,bool,bool,
                                         void (*)(void *,void *,INSTANCE_TYPE *));
static long SaveOrMarkInstancesOfClass(void *,void *,struct defmodule *,int,DEFCLASS *,
                                                bool,int,void (*)(void *,void *,INSTANCE_TYPE *));
static void SaveSingleInstanceText(void *,void *,INSTANCE_TYPE *);
static void ProcessFileErrorMessage(void *,const char *,const char *);
#if BSAVE_INSTANCES
static void WriteBinaryHeader(void *,FILE *);
static void MarkSingleInstance(void *,void *,INSTANCE_TYPE *);
static void MarkNeededAtom(void *,int,void *);
static void SaveSingleInstanceBinary(void *,void *,INSTANCE_TYPE *);
static void SaveAtomBinary(void *,unsigned short,void *,FILE *);
#endif

static long LoadOrRestoreInstances(void *,const char *,bool,bool);

#if BLOAD_INSTANCES
static bool VerifyBinaryHeader(void *,const char *);
static bool LoadSingleBinaryInstance(void *);
static void BinaryLoadInstanceError(void *,SYMBOL_HN *,DEFCLASS *);
static void CreateSlotValue(void *,DATA_OBJECT *,struct bsaveSlotValueAtom *,unsigned long); 
static void *GetBinaryAtomValue(void *,struct bsaveSlotValueAtom *);
static void BufferedRead(void *,void *,unsigned long);
static void FreeReadBuffer(void *);
#endif

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***************************************************
  NAME         : SetupInstanceFileCommands
  DESCRIPTION  : Defines function interfaces for
                 saving instances to files
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Functions defined to KB
  NOTES        : None
 ***************************************************/
void SetupInstanceFileCommands(
  void *theEnv)
  {
#if BLOAD_INSTANCES || BSAVE_INSTANCES
   AllocateEnvironmentData(theEnv,INSTANCE_FILE_DATA,sizeof(struct instanceFileData),NULL);

   InstanceFileData(theEnv)->InstanceBinaryPrefixID = "\5\6\7CLIPS";
   InstanceFileData(theEnv)->InstanceBinaryVersionID = "V6.00";
#endif

#if (! RUN_TIME)
   EnvAddUDF(theEnv,"save-instances","l", SaveInstancesCommand,
                    "SaveInstancesCommand",1,UNBOUNDED,"y;sy",NULL);
   EnvAddUDF(theEnv,"load-instances","l", LoadInstancesCommand,
                    "LoadInstancesCommand",1,1,"sy",NULL);
   EnvAddUDF(theEnv,"restore-instances","l", RestoreInstancesCommand,
                    "RestoreInstancesCommand",1,1,"sy",NULL);

#if BSAVE_INSTANCES
   EnvAddUDF(theEnv,"bsave-instances","l", BinarySaveInstancesCommand,
                    "BinarySaveInstancesCommand",1,UNBOUNDED,"y;sy",NULL);
#endif
#if BLOAD_INSTANCES
   EnvAddUDF(theEnv,"bload-instances","l", BinaryLoadInstancesCommand,
                    "BinaryLoadInstancesCommand",1,1,"sy",NULL);
#endif

#endif
  }


/****************************************************************************
  NAME         : SaveInstancesCommand
  DESCRIPTION  : H/L interface for saving
                   current instances to a file
  INPUTS       : None
  RETURNS      : The number of instances saved
  SIDE EFFECTS : Instances saved to named file
  NOTES        : H/L Syntax :
                 (save-instances <file> [local|visible [[inherit] <class>+]])
 ****************************************************************************/
void SaveInstancesCommand(
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   mCVSetInteger(returnValue,InstancesSaveCommandParser(context,"save-instances",EnvSaveInstancesDriver));
  }

/******************************************************
  NAME         : LoadInstancesCommand
  DESCRIPTION  : H/L interface for loading
                   instances from a file
  INPUTS       : None
  RETURNS      : The number of instances loaded
  SIDE EFFECTS : Instances loaded from named file
  NOTES        : H/L Syntax : (load-instances <file>)
 ******************************************************/
void LoadInstancesCommand(
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   const char *fileFound;
   CLIPSValue theArg;
   long instanceCount;
   Environment *theEnv = UDFContextEnvironment(context);

   if (! UDFFirstArgument(context,LEXEME_TYPES,&theArg))
     { return; }

   fileFound = mCVToString(&theArg);

   instanceCount = EnvLoadInstances(theEnv,fileFound);
   if (EvaluationData(theEnv)->EvaluationError)
     { ProcessFileErrorMessage(theEnv,"load-instances",fileFound); }
     
   mCVSetInteger(returnValue,instanceCount);
  }

/***************************************************
  NAME         : EnvLoadInstances
  DESCRIPTION  : Loads instances from named file
  INPUTS       : The name of the input file
  RETURNS      : The number of instances loaded
  SIDE EFFECTS : Instances loaded from file
  NOTES        : None
 ***************************************************/
long EnvLoadInstances(
  void *theEnv,
  const char *file)
  {
   return(LoadOrRestoreInstances(theEnv,file,true,true));
  }

/***************************************************
  NAME         : EnvLoadInstancesFromString
  DESCRIPTION  : Loads instances from given string
  INPUTS       : 1) The input string
                 2) Index of char in string after
                    last valid char (-1 for all chars)
  RETURNS      : The number of instances loaded
  SIDE EFFECTS : Instances loaded from string
  NOTES        : Uses string routers
 ***************************************************/
long EnvLoadInstancesFromString(
  void *theEnv,
  const char *theString,
  size_t theMax)
  {
   long theCount;
   const char * theStrRouter = "*** load-instances-from-string ***";

   if ((theMax == -1) ? (!OpenStringSource(theEnv,theStrRouter,theString,0)) :
                        (!OpenTextSource(theEnv,theStrRouter,theString,0,(unsigned) theMax)))
     return(-1L);
   theCount = LoadOrRestoreInstances(theEnv,theStrRouter,true,false);
   CloseStringSource(theEnv,theStrRouter);
   return(theCount);
  }

/*********************************************************
  NAME         : RestoreInstancesCommand
  DESCRIPTION  : H/L interface for loading
                   instances from a file w/o messages
  INPUTS       : None
  RETURNS      : The number of instances restored
  SIDE EFFECTS : Instances loaded from named file
  NOTES        : H/L Syntax : (restore-instances <file>)
 *********************************************************/
void RestoreInstancesCommand(
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   const char *fileFound;
   CLIPSValue theArg;
   long instanceCount;
   Environment *theEnv = UDFContextEnvironment(context);

   if (! UDFFirstArgument(context,LEXEME_TYPES,&theArg))
     { return; }
     
   fileFound = mCVToString(&theArg);

   instanceCount = EnvRestoreInstances(theEnv,fileFound);
   
   if (EvaluationData(theEnv)->EvaluationError)
     { ProcessFileErrorMessage(theEnv,"restore-instances",fileFound); }
   
   mCVSetInteger(returnValue,instanceCount);
  }

/***************************************************
  NAME         : EnvRestoreInstances
  DESCRIPTION  : Restores instances from named file
  INPUTS       : The name of the input file
  RETURNS      : The number of instances restored
  SIDE EFFECTS : Instances restored from file
  NOTES        : None
 ***************************************************/
long EnvRestoreInstances(
  void *theEnv,
  const char *file)
  {
   return(LoadOrRestoreInstances(theEnv,file,false,true));
  }

/***************************************************
  NAME         : EnvRestoreInstancesFromString
  DESCRIPTION  : Restores instances from given string
  INPUTS       : 1) The input string
                 2) Index of char in string after
                    last valid char (-1 for all chars)
  RETURNS      : The number of instances loaded
  SIDE EFFECTS : Instances loaded from string
  NOTES        : Uses string routers
 ***************************************************/
long EnvRestoreInstancesFromString(
  void *theEnv,
  const char *theString,
  size_t theMax)
  {
   long theCount;
   const char *theStrRouter = "*** load-instances-from-string ***";

   if ((theMax == -1) ? (!OpenStringSource(theEnv,theStrRouter,theString,0)) :
                        (!OpenTextSource(theEnv,theStrRouter,theString,0,(unsigned) theMax)))
     return(-1L);
   theCount = LoadOrRestoreInstances(theEnv,theStrRouter,false,false);
   CloseStringSource(theEnv,theStrRouter);
   return(theCount);
  }

#if BLOAD_INSTANCES

/*******************************************************
  NAME         : BinaryLoadInstancesCommand
  DESCRIPTION  : H/L interface for loading
                   instances from a binary file
  INPUTS       : None
  RETURNS      : The number of instances loaded
  SIDE EFFECTS : Instances loaded from named binary file
  NOTES        : H/L Syntax : (bload-instances <file>)
 *******************************************************/
void BinaryLoadInstancesCommand(
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   const char *fileFound;
   CLIPSValue theArg;
   long instanceCount;
   Environment *theEnv = UDFContextEnvironment(context);

   if (! UDFFirstArgument(context,LEXEME_TYPES,&theArg))
     { return; }
     
   fileFound = mCVToString(&theArg);

   instanceCount = EnvBinaryLoadInstances(theEnv,fileFound);
   if (EvaluationData(theEnv)->EvaluationError)
     { ProcessFileErrorMessage(theEnv,"bload-instances",fileFound); }
   mCVSetInteger(returnValue,instanceCount);
  }

/****************************************************
  NAME         : EnvBinaryLoadInstances
  DESCRIPTION  : Loads instances quickly from a
                 binary file
  INPUTS       : The file name
  RETURNS      : The number of instances loaded
  SIDE EFFECTS : Instances loaded w/o message-passing
  NOTES        : None
 ****************************************************/
long EnvBinaryLoadInstances(
  void *theEnv,
  const char *theFile)
  {
   long i,instanceCount;

   if (GenOpenReadBinary(theEnv,"bload-instances",theFile) == 0)
     {
      OpenErrorMessage(theEnv,"bload-instances",theFile);
      EnvSetEvaluationError(theEnv,true);
      return(-1L);
     }
   if (VerifyBinaryHeader(theEnv,theFile) == false)
     {
      GenCloseBinary(theEnv);
      EnvSetEvaluationError(theEnv,true);
      return(-1L);
     }
   
   EnvIncrementGCLocks(theEnv);
   ReadNeededAtomicValues(theEnv);

   InstanceFileData(theEnv)->BinaryInstanceFileOffset = 0L;

   GenReadBinary(theEnv,(void *) &InstanceFileData(theEnv)->BinaryInstanceFileSize,sizeof(unsigned long));
   GenReadBinary(theEnv,(void *) &instanceCount,sizeof(long));

   for (i = 0L ; i < instanceCount ; i++)
     {
      if (LoadSingleBinaryInstance(theEnv) == false)
        {
         FreeReadBuffer(theEnv);
         FreeAtomicValueStorage(theEnv);
         GenCloseBinary(theEnv);
         EnvSetEvaluationError(theEnv,true);
         EnvDecrementGCLocks(theEnv);
         return(i);
        }
     }

   FreeReadBuffer(theEnv);
   FreeAtomicValueStorage(theEnv);
   GenCloseBinary(theEnv);

   EnvDecrementGCLocks(theEnv);
   return(instanceCount);
  }

#endif

/*******************************************************
  NAME         : EnvSaveInstances
  DESCRIPTION  : Saves current instances to named file
  INPUTS       : 1) The name of the output file
                 2) A flag indicating whether to
                    save local (current module only)
                    or visible instances
                    LOCAL_SAVE or VISIBLE_SAVE
                 3) A list of expressions containing
                    the names of classes for which
                    instances are to be saved
                 4) A flag indicating if the subclasses
                    of specified classes shoudl also
                    be processed
  RETURNS      : The number of instances saved
  SIDE EFFECTS : Instances saved to file
  NOTES        : None
 *******************************************************/
long EnvSaveInstances(
  void *theEnv,
  const char *file,
  int saveCode)
  {
   return EnvSaveInstancesDriver(theEnv,file,saveCode,NULL,true);
  }

/*******************************************************
  NAME         : EnvSaveInstancesDriver
  DESCRIPTION  : Saves current instances to named file
  INPUTS       : 1) The name of the output file
                 2) A flag indicating whether to
                    save local (current module only)
                    or visible instances
                    LOCAL_SAVE or VISIBLE_SAVE
                 3) A list of expressions containing
                    the names of classes for which
                    instances are to be saved
                 4) A flag indicating if the subclasses
                    of specified classes shoudl also
                    be processed
  RETURNS      : The number of instances saved
  SIDE EFFECTS : Instances saved to file
  NOTES        : None
 *******************************************************/
long EnvSaveInstancesDriver(
  void *theEnv,
  const char *file,
  int saveCode,
  EXPRESSION *classExpressionList,
  bool inheritFlag)
  {
   FILE *sfile = NULL;
   int oldPEC,oldATS,oldIAN;
   DATA_OBJECT *classList;
   long instanceCount;

   classList = ProcessSaveClassList(theEnv,"save-instances",classExpressionList,
                                    saveCode,inheritFlag);
   if ((classList == NULL) && (classExpressionList != NULL))
     return(0L);

   SaveOrMarkInstances(theEnv,(void *) sfile,saveCode,classList,
                             inheritFlag,true,NULL);

   if ((sfile = GenOpen(theEnv,file,"w")) == NULL)
     {
      OpenErrorMessage(theEnv,"save-instances",file);
      ReturnSaveClassList(theEnv,classList);
      EnvSetEvaluationError(theEnv,true);
      return(0L);
     }

   oldPEC = PrintUtilityData(theEnv)->PreserveEscapedCharacters;
   PrintUtilityData(theEnv)->PreserveEscapedCharacters = true;
   oldATS = PrintUtilityData(theEnv)->AddressesToStrings;
   PrintUtilityData(theEnv)->AddressesToStrings = true;
   oldIAN = PrintUtilityData(theEnv)->InstanceAddressesToNames;
   PrintUtilityData(theEnv)->InstanceAddressesToNames = true;

   SetFastSave(theEnv,sfile);
   instanceCount = SaveOrMarkInstances(theEnv,(void *) sfile,saveCode,classList,
                                       inheritFlag,true,SaveSingleInstanceText);
   GenClose(theEnv,sfile);
   SetFastSave(theEnv,NULL);

   PrintUtilityData(theEnv)->PreserveEscapedCharacters = oldPEC;
   PrintUtilityData(theEnv)->AddressesToStrings = oldATS;
   PrintUtilityData(theEnv)->InstanceAddressesToNames = oldIAN;
   ReturnSaveClassList(theEnv,classList);
   return(instanceCount);
  }

#if BSAVE_INSTANCES

/****************************************************************************
  NAME         : BinarySaveInstancesCommand
  DESCRIPTION  : H/L interface for saving
                   current instances to a binary file
  INPUTS       : None
  RETURNS      : The number of instances saved
  SIDE EFFECTS : Instances saved (in binary format) to named file
  NOTES        : H/L Syntax :
                 (bsave-instances <file> [local|visible [[inherit] <class>+]])
 *****************************************************************************/
void BinarySaveInstancesCommand(
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   mCVSetInteger(returnValue,InstancesSaveCommandParser(context,"bsave-instances",EnvBinarySaveInstancesDriver));
  }

/*******************************************************
  NAME         : EnvBinarySaveInstances
  DESCRIPTION  : Saves current instances to binary file
  INPUTS       : 1) The name of the output file
                 2) A flag indicating whether to
                    save local (current module only)
                    or visible instances
                    LOCAL_SAVE or VISIBLE_SAVE
  RETURNS      : The number of instances saved
  SIDE EFFECTS : Instances saved to file
  NOTES        : None
 *******************************************************/
long EnvBinarySaveInstances(
  void *theEnv,
  const char *file,
  int saveCode)
  {
   return EnvBinarySaveInstancesDriver(theEnv,file,saveCode,NULL,true);
  }
  
/*******************************************************
  NAME         : EnvBinarySaveInstancesDriver
  DESCRIPTION  : Saves current instances to binary file
  INPUTS       : 1) The name of the output file
                 2) A flag indicating whether to
                    save local (current module only)
                    or visible instances
                    LOCAL_SAVE or VISIBLE_SAVE
                 3) A list of expressions containing
                    the names of classes for which
                    instances are to be saved
                 4) A flag indicating if the subclasses
                    of specified classes shoudl also
                    be processed
  RETURNS      : The number of instances saved
  SIDE EFFECTS : Instances saved to file
  NOTES        : None
 *******************************************************/
long EnvBinarySaveInstancesDriver(
  void *theEnv,
  const char *file,
  int saveCode,
  EXPRESSION *classExpressionList,
  bool inheritFlag)
  {
   DATA_OBJECT *classList;
   FILE *bsaveFP;
   long instanceCount;

   classList = ProcessSaveClassList(theEnv,"bsave-instances",classExpressionList,
                                    saveCode,inheritFlag);
   if ((classList == NULL) && (classExpressionList != NULL))
     return(0L);

   InstanceFileData(theEnv)->BinaryInstanceFileSize = 0L;
   InitAtomicValueNeededFlags(theEnv);
   instanceCount = SaveOrMarkInstances(theEnv,NULL,saveCode,classList,inheritFlag,
                                       false,MarkSingleInstance);

   if ((bsaveFP = GenOpen(theEnv,file,"wb")) == NULL)
     {
      OpenErrorMessage(theEnv,"bsave-instances",file);
      ReturnSaveClassList(theEnv,classList);
      EnvSetEvaluationError(theEnv,true);
      return(0L);
     }
   WriteBinaryHeader(theEnv,bsaveFP);
   WriteNeededAtomicValues(theEnv,bsaveFP);

   fwrite((void *) &InstanceFileData(theEnv)->BinaryInstanceFileSize,sizeof(unsigned long),1,bsaveFP);
   fwrite((void *) &instanceCount,sizeof(long),1,bsaveFP);

   SetAtomicValueIndices(theEnv,false);
   SaveOrMarkInstances(theEnv,(void *) bsaveFP,saveCode,classList,
                       inheritFlag,false,SaveSingleInstanceBinary);
   RestoreAtomicValueBuckets(theEnv);
   GenClose(theEnv,bsaveFP);
   ReturnSaveClassList(theEnv,classList);
   return(instanceCount);
  }

#endif

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/******************************************************
  NAME         : InstancesSaveCommandParser
  DESCRIPTION  : Argument parser for save-instances
                 and bsave-instances
  INPUTS       : 1) The name of the calling function
                 2) A pointer to the support
                    function to call for the save/bsave
  RETURNS      : The number of instances saved
  SIDE EFFECTS : Instances saved/bsaved
  NOTES        : None
 ******************************************************/
static long InstancesSaveCommandParser(
  UDFContext *context,
  const char *functionName,
  long (*saveFunction)(void *,const char *,int,EXPRESSION *,bool))
  {
   const char *fileFound;
   DATA_OBJECT temp;
   int argCount,saveCode = LOCAL_SAVE;
   EXPRESSION *classList = NULL;
   bool inheritFlag = false;
   Environment *theEnv = UDFContextEnvironment(context);

   if (EnvArgTypeCheck(theEnv,functionName,1,SYMBOL_OR_STRING,&temp) == false)
     return(0L);
     
   fileFound = DOToString(temp);

   argCount = EnvRtnArgCount(theEnv);
   if (argCount > 1)
     {
      if (EnvArgTypeCheck(theEnv,functionName,2,SYMBOL,&temp) == false)
        {
         ExpectedTypeError1(theEnv,functionName,2,"symbol \"local\" or \"visible\"");
         EnvSetEvaluationError(theEnv,true);
         return(0L);
        }
      if (strcmp(DOToString(temp),"local") == 0)
        saveCode = LOCAL_SAVE;
      else if (strcmp(DOToString(temp),"visible") == 0)
        saveCode = VISIBLE_SAVE;
      else
        {
         ExpectedTypeError1(theEnv,functionName,2,"symbol \"local\" or \"visible\"");
         EnvSetEvaluationError(theEnv,true);
         return(0L);
        }
      classList = GetFirstArgument()->nextArg->nextArg;

      /* ===========================
         Check for "inherit" keyword
         Must be at least one class
         name following
         =========================== */
      if ((classList != NULL) ? (classList->nextArg != NULL) : false)
        {
         if ((classList->type != SYMBOL) ? false :
             (strcmp(ValueToString(classList->value),"inherit") == 0))
           {
            inheritFlag = true;
            classList = classList->nextArg;
           }
        }
     }

   return((*saveFunction)(theEnv,fileFound,saveCode,classList,inheritFlag));
  }

/****************************************************
  NAME         : ProcessSaveClassList
  DESCRIPTION  : Evaluates a list of class name
                 expressions and stores them in a
                 data object list
  INPUTS       : 1) The name of the calling function
                 2) The class expression list
                 3) A flag indicating if only local
                    or all visible instances are
                    being saved
                 4) A flag indicating if inheritance
                    relationships should be checked
                    between classes
  RETURNS      : The evaluated class pointer data
                 objects - NULL on errors
  SIDE EFFECTS : Data objects allocated and
                 classes validated
  NOTES        : None
 ****************************************************/
static DATA_OBJECT *ProcessSaveClassList(
  void *theEnv,
  const char *functionName,
  EXPRESSION *classExps,
  int saveCode,
  bool inheritFlag)
  {
   DATA_OBJECT *head = NULL,*prv,*newItem,tmp;
   DEFCLASS *theDefclass;
   struct defmodule *currentModule;
   int argIndex = inheritFlag ? 4 : 3;

   currentModule = ((struct defmodule *) EnvGetCurrentModule(theEnv));
   while (classExps != NULL)
     {
      if (EvaluateExpression(theEnv,classExps,&tmp))
        goto ProcessClassListError;
      if (tmp.type != SYMBOL)
        goto ProcessClassListError;
      if (saveCode == LOCAL_SAVE)
        theDefclass = LookupDefclassAnywhere(theEnv,currentModule,DOToString(tmp));
      else
        //theDefclass = LookupDefclassInScope(theEnv,DOToString(tmp));
        { theDefclass = LookupDefclassByMdlOrScope(theEnv,DOToString(tmp)); }

      if (theDefclass == NULL)
        goto ProcessClassListError;
      else if (theDefclass->abstract && (inheritFlag == false))
        goto ProcessClassListError;
      prv = newItem = head;
      while (newItem != NULL)
        {
         if (newItem->value == (void *) theDefclass)
           goto ProcessClassListError;
         else if (inheritFlag)
           {
            if (HasSuperclass((DEFCLASS *) newItem->value,theDefclass) ||
                HasSuperclass(theDefclass,(DEFCLASS *) newItem->value))
             goto ProcessClassListError;
           }
         prv = newItem;
         newItem = newItem->next;
        }
      newItem = get_struct(theEnv,dataObject);
      newItem->type = DEFCLASS_PTR;
      newItem->value = (void *) theDefclass;
      newItem->next = NULL;
      if (prv == NULL)
        head = newItem;
      else
        prv->next = newItem;
      argIndex++;
      classExps = classExps->nextArg;
     }
   return(head);

ProcessClassListError:
   if (inheritFlag)
     ExpectedTypeError1(theEnv,functionName,argIndex,"valid class name");
   else
     ExpectedTypeError1(theEnv,functionName,argIndex,"valid concrete class name");
   ReturnSaveClassList(theEnv,head);
   EnvSetEvaluationError(theEnv,true);
   return(NULL);
  }

/****************************************************
  NAME         : ReturnSaveClassList
  DESCRIPTION  : Deallocates the class data object
                 list created by ProcessSaveClassList
  INPUTS       : The class data object list
  RETURNS      : Nothing useful
  SIDE EFFECTS : Class data object returned
  NOTES        : None
 ****************************************************/
static void ReturnSaveClassList(
  void *theEnv,
  DATA_OBJECT *classList)
  {
   DATA_OBJECT *tmp;

   while (classList != NULL)
     {
      tmp = classList;
      classList = classList->next;
      rtn_struct(theEnv,dataObject,tmp);
     }
  }

/***************************************************
  NAME         : SaveOrMarkInstances
  DESCRIPTION  : Iterates through all specified
                 instances either marking needed
                 atoms or writing instances in
                 binary/text format
  INPUTS       : 1) NULL (for marking),
                    logical name (for text saves)
                    file pointer (for binary saves)
                 2) A cope flag indicating LOCAL
                    or VISIBLE saves only
                 3) A list of data objects
                    containing the names of classes
                    of instances to be saved
                 4) A flag indicating whether to
                    include subclasses of arg #3
                 5) A flag indicating if the
                    iteration can be interrupted
                    or not
                 6) The access function to mark
                    or save an instance (can be NULL
                    if only counting instances)
  RETURNS      : The number of instances saved
  SIDE EFFECTS : Instances amrked or saved
  NOTES        : None
 ***************************************************/
static long SaveOrMarkInstances(
  void *theEnv,
  void *theOutput,
  int saveCode,
  DATA_OBJECT *classList,
  bool inheritFlag,
  bool interruptOK,
  void (*saveInstanceFunc)(void *,void *,INSTANCE_TYPE *))
  {
   struct defmodule *currentModule;
   int traversalID;
   DATA_OBJECT *tmp;
   INSTANCE_TYPE *ins;
   long instanceCount = 0L;

   currentModule = ((struct defmodule *) EnvGetCurrentModule(theEnv));
   if (classList != NULL)
     {
      traversalID = GetTraversalID(theEnv);
      if (traversalID != -1)
        {
         for (tmp = classList ;
              (! ((tmp == NULL) || (EvaluationData(theEnv)->HaltExecution && interruptOK))) ;
              tmp = tmp->next)
           instanceCount += SaveOrMarkInstancesOfClass(theEnv,theOutput,currentModule,saveCode,
                                                       (DEFCLASS *) tmp->value,inheritFlag,
                                                       traversalID,saveInstanceFunc);
         ReleaseTraversalID(theEnv);
        }
     }
   else
     {
      for (ins = (INSTANCE_TYPE *) GetNextInstanceInScope(theEnv,NULL) ;
           (ins != NULL) && (EvaluationData(theEnv)->HaltExecution != true) ;
           ins = (INSTANCE_TYPE *) GetNextInstanceInScope(theEnv,(void *) ins))
        {
         if ((saveCode == VISIBLE_SAVE) ? true :
             (ins->cls->header.whichModule->theModule == currentModule))
           {
            if (saveInstanceFunc != NULL)
              (*saveInstanceFunc)(theEnv,theOutput,ins);
            instanceCount++;
           }
        }
     }
   return(instanceCount);
  }

/***************************************************
  NAME         : SaveOrMarkInstancesOfClass
  DESCRIPTION  : Saves off the direct (and indirect)
                 instance of the specified class
  INPUTS       : 1) The logical name of the output
                    (or file pointer for binary
                     output)
                 2) The current module
                 3) A flag indicating local
                    or visible saves
                 4) The defclass
                 5) A flag indicating whether to
                    save subclass instances or not
                 6) A traversal id for marking
                    visited classes
                 7) A pointer to the instance
                    manipulation function to call
                    (can be NULL for only counting
                     instances)
  RETURNS      : The number of instances saved
  SIDE EFFECTS : Appropriate instances saved
  NOTES        : None
 ***************************************************/
static long SaveOrMarkInstancesOfClass(
  void *theEnv,
  void *theOutput,
  struct defmodule *currentModule,
  int saveCode,
  DEFCLASS *theDefclass,
  bool inheritFlag,
  int traversalID,
  void (*saveInstanceFunc)(void *,void *,INSTANCE_TYPE *))
  {
   INSTANCE_TYPE *theInstance;
   DEFCLASS *subclass;
   long i;
   long instanceCount = 0L;

   if (TestTraversalID(theDefclass->traversalRecord,traversalID))
     return(instanceCount);
   SetTraversalID(theDefclass->traversalRecord,traversalID);
   if (((saveCode == LOCAL_SAVE) &&
        (theDefclass->header.whichModule->theModule == currentModule)) ||
       ((saveCode == VISIBLE_SAVE) &&
        DefclassInScope(theEnv,theDefclass,currentModule)))
     {
      for (theInstance = (INSTANCE_TYPE *)
             EnvGetNextInstanceInClass(theEnv,(void *) theDefclass,NULL) ;
           theInstance != NULL ;
           theInstance = (INSTANCE_TYPE *)
           EnvGetNextInstanceInClass(theEnv,(void *) theDefclass,(void *) theInstance))
        {
         if (saveInstanceFunc != NULL)
           (*saveInstanceFunc)(theEnv,theOutput,theInstance);
         instanceCount++;
        }
     }
   if (inheritFlag)
     {
      for (i = 0 ; i < theDefclass->directSubclasses.classCount ; i++)
        {
         subclass = theDefclass->directSubclasses.classArray[i];
           instanceCount += SaveOrMarkInstancesOfClass(theEnv,theOutput,currentModule,saveCode,
                                                       subclass,true,traversalID,
                                                       saveInstanceFunc);
        }
     }
   return(instanceCount);
  }

/***************************************************
  NAME         : SaveSingleInstanceText
  DESCRIPTION  : Writes given instance to file
  INPUTS       : 1) The logical name of the output
                 2) The instance to save
  RETURNS      : Nothing useful
  SIDE EFFECTS : Instance written
  NOTES        : None
 ***************************************************/
static void SaveSingleInstanceText(
  void *theEnv,
  void *vLogicalName,
  INSTANCE_TYPE *theInstance)
  {
   long i;
   INSTANCE_SLOT *sp;
   const char *logicalName = (const char *) vLogicalName;

   EnvPrintRouter(theEnv,logicalName,"([");
   EnvPrintRouter(theEnv,logicalName,ValueToString(theInstance->name));
   EnvPrintRouter(theEnv,logicalName,"] of ");
   EnvPrintRouter(theEnv,logicalName,ValueToString(theInstance->cls->header.name));
   for (i = 0 ; i < theInstance->cls->instanceSlotCount ; i++)
     {
      sp = theInstance->slotAddresses[i];
      EnvPrintRouter(theEnv,logicalName,"\n   (");
      EnvPrintRouter(theEnv,logicalName,ValueToString(sp->desc->slotName->name));
      if (sp->type != MULTIFIELD)
        {
         EnvPrintRouter(theEnv,logicalName," ");
         PrintAtom(theEnv,logicalName,(int) sp->type,sp->value);
        }
      else if (GetInstanceSlotLength(sp) != 0)
        {
         EnvPrintRouter(theEnv,logicalName," ");
         PrintMultifield(theEnv,logicalName,(MULTIFIELD_PTR) sp->value,0,
                         (long) (GetInstanceSlotLength(sp) - 1),false);
        }
      EnvPrintRouter(theEnv,logicalName,")");
     }
   EnvPrintRouter(theEnv,logicalName,")\n\n");
  }

#if BSAVE_INSTANCES

/***************************************************
  NAME         : WriteBinaryHeader
  DESCRIPTION  : Writes identifying string to
                 instance binary file to assist in
                 later verification
  INPUTS       : The binary file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Binary prefix headers written
  NOTES        : None
 ***************************************************/
static void WriteBinaryHeader(
  void *theEnv,
  FILE *bsaveFP)
  {   
   fwrite((void *) InstanceFileData(theEnv)->InstanceBinaryPrefixID,
          (STD_SIZE) (strlen(InstanceFileData(theEnv)->InstanceBinaryPrefixID) + 1),1,bsaveFP);
   fwrite((void *) InstanceFileData(theEnv)->InstanceBinaryVersionID,
          (STD_SIZE) (strlen(InstanceFileData(theEnv)->InstanceBinaryVersionID) + 1),1,bsaveFP);
  }

/***************************************************
  NAME         : MarkSingleInstance
  DESCRIPTION  : Marks all the atoms needed in
                 the slot values of an instance
  INPUTS       : 1) The output (ignored)
                 2) The instance
  RETURNS      : Nothing useful
  SIDE EFFECTS : Instance slot value atoms marked
  NOTES        : None
 ***************************************************/
static void MarkSingleInstance(
  void *theEnv,
  void *theOutput,
  INSTANCE_TYPE *theInstance)
  {
#if MAC_XCD
#pragma unused(theOutput)
#endif
   INSTANCE_SLOT *sp;
   long i, j;

   InstanceFileData(theEnv)->BinaryInstanceFileSize += (unsigned long) (sizeof(long) * 2);
   theInstance->name->neededSymbol = true;
   theInstance->cls->header.name->neededSymbol = true;
   InstanceFileData(theEnv)->BinaryInstanceFileSize +=
       (unsigned long) ((sizeof(long) * 2) +
                        (sizeof(struct bsaveSlotValue) *
                         theInstance->cls->instanceSlotCount) +
                        sizeof(unsigned long) +
                        sizeof(unsigned));
   for (i = 0 ; i < theInstance->cls->instanceSlotCount ; i++)
     {
      sp = theInstance->slotAddresses[i];
      sp->desc->slotName->name->neededSymbol = true;
      if (sp->desc->multiple)
        {
         for (j = 1 ; j <= GetInstanceSlotLength(sp) ; j++)
           MarkNeededAtom(theEnv,GetMFType(sp->value,j),GetMFValue(sp->value,j));
        }
      else
        MarkNeededAtom(theEnv,(int) sp->type,sp->value);
     }
  }

/***************************************************
  NAME         : MarkNeededAtom
  DESCRIPTION  : Marks an integer/float/symbol as
                 being need by a set of instances
  INPUTS       : 1) The type of atom
                 2) The value of the atom
  RETURNS      : Nothing useful
  SIDE EFFECTS : Atom marked for saving
  NOTES        : None
 ***************************************************/
static void MarkNeededAtom(
  void *theEnv,
  int type,
  void *value)
  {
   InstanceFileData(theEnv)->BinaryInstanceFileSize += (unsigned long) sizeof(struct bsaveSlotValueAtom);

   /* =====================================
      Assumes slot value atoms  can only be
      floats, integers, symbols, strings,
      instance-names, instance-addresses,
      fact-addresses or external-addresses
      ===================================== */
   switch (type)
     {
      case SYMBOL:
      case STRING:
      case INSTANCE_NAME:
         ((SYMBOL_HN *) value)->neededSymbol = true;
         break;
      case FLOAT:
         ((FLOAT_HN *) value)->neededFloat = true;
         break;
      case INTEGER:
         ((INTEGER_HN *) value)->neededInteger = true;
         break;
      case INSTANCE_ADDRESS:
         GetFullInstanceName(theEnv,(INSTANCE_TYPE *) value)->neededSymbol = true;
         break;
     }
  }

/****************************************************
  NAME         : SaveSingleInstanceBinary
  DESCRIPTION  : Writes given instance to binary file
  INPUTS       : 1) Binary file pointer
                 2) The instance to save
  RETURNS      : Nothing useful
  SIDE EFFECTS : Instance written
  NOTES        : None
 ****************************************************/
static void SaveSingleInstanceBinary(
  void *theEnv,
  void *vBsaveFP,
  INSTANCE_TYPE *theInstance)
  {
   long nameIndex;
   long i,j;
   INSTANCE_SLOT *sp;
   FILE *bsaveFP = (FILE *) vBsaveFP;
   struct bsaveSlotValue bs;
   long totalValueCount = 0L;
   long slotLen;

   /* ===========================
      Write out the instance name
      =========================== */
   nameIndex = (long) theInstance->name->bucket;
   fwrite((void *) &nameIndex,(int) sizeof(long),1,bsaveFP);

   /* ========================
      Write out the class name
      ======================== */
   nameIndex = (long) theInstance->cls->header.name->bucket;
   fwrite((void *) &nameIndex,(int) sizeof(long),1,bsaveFP);

   /* ======================================
      Write out the number of slot-overrides
      ====================================== */
   fwrite((void *) &theInstance->cls->instanceSlotCount,
          (int) sizeof(short),1,bsaveFP);

   /* =========================================
      Write out the slot names and value counts
      ========================================= */
   for (i = 0 ; i < theInstance->cls->instanceSlotCount ; i++)
     {
      sp = theInstance->slotAddresses[i];

      /* ===============================================
         Write out the number of atoms in the slot value
         =============================================== */
      bs.slotName = (long) sp->desc->slotName->name->bucket;
      bs.valueCount = sp->desc->multiple ? GetInstanceSlotLength(sp) : 1;
      fwrite((void *) &bs,(int) sizeof(struct bsaveSlotValue),1,bsaveFP);
      totalValueCount += (unsigned long) bs.valueCount;
     }

   /* ==================================
      Write out the number of slot value
      atoms for the whole instance
      ================================== */
   if (theInstance->cls->instanceSlotCount != 0) // (totalValueCount != 0L) : Bug fix if any slots, write out count 
     fwrite((void *) &totalValueCount,(int) sizeof(unsigned long),1,bsaveFP);

   /* ==============================
      Write out the slot value atoms
      ============================== */
   for (i = 0 ; i < theInstance->cls->instanceSlotCount ; i++)
     {
      sp = theInstance->slotAddresses[i];
      slotLen = sp->desc->multiple ? GetInstanceSlotLength(sp) : 1;

      /* =========================================
         Write out the type and index of each atom
         ========================================= */
      if (sp->desc->multiple)
        {
         for (j = 1 ; j <= slotLen ; j++)
           SaveAtomBinary(theEnv,GetMFType(sp->value,j),GetMFValue(sp->value,j),bsaveFP);
        }
      else
        SaveAtomBinary(theEnv,(unsigned short) sp->type,sp->value,bsaveFP);
     }
  }

/***************************************************
  NAME         : SaveAtomBinary
  DESCRIPTION  : Writes out an instance slot value
                 atom to the binary file
  INPUTS       : 1) The atom type
                 2) The atom value
                 3) The binary file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : atom written
  NOTES        :
 ***************************************************/
static void SaveAtomBinary(
  void *theEnv,
  unsigned short type,
  void *value,
  FILE *bsaveFP)
  {
   struct bsaveSlotValueAtom bsa;

   /* =====================================
      Assumes slot value atoms  can only be
      floats, integers, symbols, strings,
      instance-names, instance-addresses,
      fact-addresses or external-addresses
      ===================================== */
   bsa.type = type;
   switch (type)
     {
      case SYMBOL:
      case STRING:
      case INSTANCE_NAME:
         bsa.value = (long) ((SYMBOL_HN *) value)->bucket;
         break;
      case FLOAT:
         bsa.value = (long) ((FLOAT_HN *) value)->bucket;
         break;
      case INTEGER:
         bsa.value = (long) ((INTEGER_HN *) value)->bucket;
         break;
      case INSTANCE_ADDRESS:
         bsa.type = INSTANCE_NAME;
         bsa.value = (long) GetFullInstanceName(theEnv,(INSTANCE_TYPE *) value)->bucket;
         break;
      default:
         bsa.value = -1L;
     }
   fwrite((void *) &bsa,(int) sizeof(struct bsaveSlotValueAtom),1,bsaveFP);
  }

#endif

/**********************************************************************
  NAME         : LoadOrRestoreInstances
  DESCRIPTION  : Loads instances from named file
  INPUTS       : 1) The name of the input file
                 2) An integer flag indicating whether or
                    not to use message-passing to create
                    the new instances and delete old versions
                 3) An integer flag indicating if arg #1
                    is a file name or the name of a string router
  RETURNS      : The number of instances loaded/restored
  SIDE EFFECTS : Instances loaded from file
  NOTES        : None
 **********************************************************************/
static long LoadOrRestoreInstances(
  void *theEnv,
  const char *file,
  bool usemsgs,
  bool isFileName)
  {
   DATA_OBJECT temp;
   FILE *sfile = NULL,*svload = NULL;
   const char *ilog;
   EXPRESSION *top;
   int svoverride;
   long instanceCount = 0L;

   if (isFileName) {
     if ((sfile = GenOpen(theEnv,file,"r")) == NULL)
       {
        EnvSetEvaluationError(theEnv,true);
        return(-1L);
       }
     svload = GetFastLoad(theEnv);
     ilog = (char *) sfile;
     SetFastLoad(theEnv,sfile);
   } else {
     ilog = file;
   }
   top = GenConstant(theEnv,FCALL,(void *) FindFunction(theEnv,"make-instance"));
   GetToken(theEnv,ilog,&DefclassData(theEnv)->ObjectParseToken);
   svoverride = InstanceData(theEnv)->MkInsMsgPass;
   InstanceData(theEnv)->MkInsMsgPass = usemsgs;
   while ((GetType(DefclassData(theEnv)->ObjectParseToken) != STOP) && (EvaluationData(theEnv)->HaltExecution != true))
     {
      if (GetType(DefclassData(theEnv)->ObjectParseToken) != LPAREN)
        {
         SyntaxErrorMessage(theEnv,"instance definition");
         rtn_struct(theEnv,expr,top);
         if (isFileName) {
           GenClose(theEnv,sfile);
           SetFastLoad(theEnv,svload);
         }
         EnvSetEvaluationError(theEnv,true);
         InstanceData(theEnv)->MkInsMsgPass = svoverride;
         return(instanceCount);
        }
      if (ParseSimpleInstance(theEnv,top,ilog) == NULL)
        {
         if (isFileName) {
           GenClose(theEnv,sfile);
           SetFastLoad(theEnv,svload);
         }
         InstanceData(theEnv)->MkInsMsgPass = svoverride;
         EnvSetEvaluationError(theEnv,true);
         return(instanceCount);
        }
      ExpressionInstall(theEnv,top);
      EvaluateExpression(theEnv,top,&temp);
      ExpressionDeinstall(theEnv,top);
      if (! EvaluationData(theEnv)->EvaluationError)
        instanceCount++;
      ReturnExpression(theEnv,top->argList);
      top->argList = NULL;
      GetToken(theEnv,ilog,&DefclassData(theEnv)->ObjectParseToken);
     }
   rtn_struct(theEnv,expr,top);
   if (isFileName) {
     GenClose(theEnv,sfile);
     SetFastLoad(theEnv,svload);
   }
   InstanceData(theEnv)->MkInsMsgPass = svoverride;
   return(instanceCount);
  }

/***************************************************
  NAME         : ProcessFileErrorMessage
  DESCRIPTION  : Prints an error message when a
                 file containing text or binary
                 instances cannot be processed.
  INPUTS       : The name of the input file and the
                 function which opened it.
  RETURNS      : No value
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
static void ProcessFileErrorMessage(
  void *theEnv,
  const char *functionName,
  const char *fileName)
  {
   PrintErrorID(theEnv,"INSFILE",1,false);
   EnvPrintRouter(theEnv,WERROR,"Function ");
   EnvPrintRouter(theEnv,WERROR,functionName);
   EnvPrintRouter(theEnv,WERROR," could not completely process file ");
   EnvPrintRouter(theEnv,WERROR,fileName);
   EnvPrintRouter(theEnv,WERROR,".\n");
  }

#if BLOAD_INSTANCES

/*******************************************************
  NAME         : VerifyBinaryHeader
  DESCRIPTION  : Reads the prefix and version headers
                 from a file to verify that the
                 input is a valid binary instances file
  INPUTS       : The name of the file
  RETURNS      : true if OK, false otherwise
  SIDE EFFECTS : Input prefix and version read
  NOTES        : Assumes file already open with 
                 GenOpenReadBinary
 *******************************************************/
static bool VerifyBinaryHeader(
  void *theEnv,
  const char *theFile)
  {
   char buf[20];

   GenReadBinary(theEnv,(void *) buf,(unsigned long) (strlen(InstanceFileData(theEnv)->InstanceBinaryPrefixID) + 1));
   if (strcmp(buf,InstanceFileData(theEnv)->InstanceBinaryPrefixID) != 0)
     {
      PrintErrorID(theEnv,"INSFILE",2,false);
      EnvPrintRouter(theEnv,WERROR,theFile);
      EnvPrintRouter(theEnv,WERROR," file is not a binary instances file.\n");
      return(false);
     }
   GenReadBinary(theEnv,(void *) buf,(unsigned long) (strlen(InstanceFileData(theEnv)->InstanceBinaryVersionID) + 1));
   if (strcmp(buf,InstanceFileData(theEnv)->InstanceBinaryVersionID) != 0)
     {
      PrintErrorID(theEnv,"INSFILE",3,false);
      EnvPrintRouter(theEnv,WERROR,theFile);
      EnvPrintRouter(theEnv,WERROR," file is not a compatible binary instances file.\n");
      return(false);
     }
   return(true);
  }

/***************************************************
  NAME         : LoadSingleBinaryInstance
  DESCRIPTION  : Reads the binary data for a new
                 instance and its slot values and
                 creates/initializes the instance
  INPUTS       : None
  RETURNS      : true if all OK,
                 false otherwise
  SIDE EFFECTS : Binary data read and instance
                 created
  NOTES        : Uses global GenReadBinary(theEnv,)
 ***************************************************/
static bool LoadSingleBinaryInstance(
  void *theEnv)
  {
   SYMBOL_HN *instanceName,
             *className;
   short slotCount;
   DEFCLASS *theDefclass;
   INSTANCE_TYPE *newInstance;
   struct bsaveSlotValue *bsArray;
   struct bsaveSlotValueAtom *bsaArray = NULL;
   long nameIndex;
   unsigned long totalValueCount;
   long i, j;
   INSTANCE_SLOT *sp;
   DATA_OBJECT slotValue,junkValue;

   /* =====================
      Get the instance name
      ===================== */
   BufferedRead(theEnv,(void *) &nameIndex,(unsigned long) sizeof(long));
   instanceName = SymbolPointer(nameIndex);

   /* ==================
      Get the class name
      ================== */
   BufferedRead(theEnv,(void *) &nameIndex,(unsigned long) sizeof(long));
   className = SymbolPointer(nameIndex);

   /* ==================
      Get the slot count
      ================== */
   BufferedRead(theEnv,(void *) &slotCount,(unsigned long) sizeof(short));

   /* =============================
      Make sure the defclass exists
      and check the slot count
      ============================= */
   //theDefclass = LookupDefclassInScope(theEnv,ValueToString(className));
   theDefclass = LookupDefclassByMdlOrScope(theEnv,ValueToString(className));
   if (theDefclass == NULL)
     {
      ClassExistError(theEnv,"bload-instances",ValueToString(className));
      return(false);
     }
   if (theDefclass->instanceSlotCount != slotCount)
     {
      BinaryLoadInstanceError(theEnv,instanceName,theDefclass);
      return(false);
     }

   /* ===================================
      Create the new unitialized instance
      =================================== */
   newInstance = BuildInstance(theEnv,instanceName,theDefclass,false);
   if (newInstance == NULL)
     {
      BinaryLoadInstanceError(theEnv,instanceName,theDefclass);
      return(false);
     }
   if (slotCount == 0)
     return(true);

   /* ====================================
      Read all slot override info and slot
      value atoms into big arrays
      ==================================== */
   bsArray = (struct bsaveSlotValue *) gm2(theEnv,(sizeof(struct bsaveSlotValue) * slotCount));
   BufferedRead(theEnv,(void *) bsArray,(unsigned long) (sizeof(struct bsaveSlotValue) * slotCount));

   BufferedRead(theEnv,(void *) &totalValueCount,(unsigned long) sizeof(unsigned long));

   if (totalValueCount != 0L)
     {
      bsaArray = (struct bsaveSlotValueAtom *)
                  gm3(theEnv,(long) (totalValueCount * sizeof(struct bsaveSlotValueAtom)));
      BufferedRead(theEnv,(void *) bsaArray,
                   (unsigned long) (totalValueCount * sizeof(struct bsaveSlotValueAtom)));
     }

   /* =========================
      Insert the values for the
      slot overrides
      ========================= */
   for (i = 0 , j = 0L ; i < slotCount ; i++)
     {
      /* ===========================================================
         Here is another check for the validity of the binary file -
         the order of the slots in the file should match the
         order in the class definition
         =========================================================== */
      sp = newInstance->slotAddresses[i];
      if (sp->desc->slotName->name != SymbolPointer(bsArray[i].slotName))
        goto LoadError;
      CreateSlotValue(theEnv,&slotValue,(struct bsaveSlotValueAtom *) &bsaArray[j],
                      bsArray[i].valueCount);

      if (PutSlotValue(theEnv,newInstance,sp,&slotValue,&junkValue,"bload-instances") == false)
        goto LoadError;

      j += (unsigned long) bsArray[i].valueCount;
     }

   rm(theEnv,(void *) bsArray,(sizeof(struct bsaveSlotValue) * slotCount));

   if (totalValueCount != 0L)
     rm3(theEnv,(void *) bsaArray,
         (long) (totalValueCount * sizeof(struct bsaveSlotValueAtom)));

   return(true);

LoadError:
   BinaryLoadInstanceError(theEnv,instanceName,theDefclass);
   QuashInstance(theEnv,newInstance);
   rm(theEnv,(void *) bsArray,(sizeof(struct bsaveSlotValue) * slotCount));
   rm3(theEnv,(void *) bsaArray,
       (long) (totalValueCount * sizeof(struct bsaveSlotValueAtom)));
   return(false);
  }

/***************************************************
  NAME         : BinaryLoadInstanceError
  DESCRIPTION  : Prints out an error message when
                 an instance could not be
                 successfully loaded from a
                 binary file
  INPUTS       : 1) The instance name
                 2) The defclass
  RETURNS      : Nothing useful
  SIDE EFFECTS : Error message printed
  NOTES        : None
 ***************************************************/
static void BinaryLoadInstanceError(
  void *theEnv,
  SYMBOL_HN *instanceName,
  DEFCLASS *theDefclass)
  {
   PrintErrorID(theEnv,"INSFILE",4,false);
   EnvPrintRouter(theEnv,WERROR,"Function bload-instances unable to load instance [");
   EnvPrintRouter(theEnv,WERROR,ValueToString(instanceName));
   EnvPrintRouter(theEnv,WERROR,"] of class ");
   PrintClassName(theEnv,WERROR,theDefclass,true);
  }

/***************************************************
  NAME         : CreateSlotValue
  DESCRIPTION  : Creates a data object value from
                 the binary slot value atom data
  INPUTS       : 1) A data object buffer
                 2) The slot value atoms array
                 3) The number of values to put
                    in the data object
  RETURNS      : Nothing useful
  SIDE EFFECTS : Data object initialized
                 (if more than one value, a
                 multifield is created)
  NOTES        : None
 ***************************************************/
static void CreateSlotValue(
  void *theEnv,
  DATA_OBJECT *result,
  struct bsaveSlotValueAtom *bsaValues,
  unsigned long valueCount)
  {
   register unsigned i;

   if (valueCount == 0)
     {
      result->type = MULTIFIELD;
      result->value = EnvCreateMultifield(theEnv,0L);
      result->begin = 0;
      result->end = -1;
     }
   else if (valueCount == 1)
     {
      result->type = bsaValues[0].type;
      result->value = GetBinaryAtomValue(theEnv,&bsaValues[0]);
     }
   else
     {
      result->type = MULTIFIELD;
      result->value = EnvCreateMultifield(theEnv,valueCount);
      result->begin = 0;
      SetpDOEnd(result,valueCount);
      for (i = 1 ; i <= valueCount ; i++)
        {
         SetMFType(result->value,i,(short) bsaValues[i-1].type);
         SetMFValue(result->value,i,GetBinaryAtomValue(theEnv,&bsaValues[i-1]));
        }
     }
  }

/***************************************************
  NAME         : GetBinaryAtomValue
  DESCRIPTION  : Uses the binary index of an atom
                 to find the ephemeris value
  INPUTS       : The binary type and index
  RETURNS      : The symbol/etc. pointer
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
static void *GetBinaryAtomValue(
  void *theEnv,
  struct bsaveSlotValueAtom *ba)
  {
   switch (ba->type)
     {
      case SYMBOL:
      case STRING:
      case INSTANCE_NAME:
         return((void *) SymbolPointer(ba->value));
      case FLOAT:
         return((void *) FloatPointer(ba->value));
      case INTEGER:
         return((void *) IntegerPointer(ba->value));
      case FACT_ADDRESS:
#if DEFTEMPLATE_CONSTRUCT && DEFRULE_CONSTRUCT
         return((void *) &FactData(theEnv)->DummyFact);
#else
         return(NULL);
#endif
      case EXTERNAL_ADDRESS:
        return(NULL);

      default:
        {
         SystemError(theEnv,"INSFILE",1);
         EnvExitRouter(theEnv,EXIT_FAILURE);
        }
     }
   return(NULL);
  }

/***************************************************
  NAME         : BufferedRead
  DESCRIPTION  : Reads data from binary file
                 (Larger blocks than requested size
                  may be read and buffered)
  INPUTS       : 1) The buffer
                 2) The buffer size
  RETURNS      : Nothing useful
  SIDE EFFECTS : Data stored in buffer
  NOTES        : None
 ***************************************************/
static void BufferedRead(
  void *theEnv,
  void *buf,
  unsigned long bufsz)
  {
   unsigned long i,amountLeftToRead;

   if (InstanceFileData(theEnv)->CurrentReadBuffer != NULL)
     {
      amountLeftToRead = InstanceFileData(theEnv)->CurrentReadBufferSize - InstanceFileData(theEnv)->CurrentReadBufferOffset;
      if (bufsz <= amountLeftToRead)
        {
         for (i = 0L ; i < bufsz ; i++)
           ((char *) buf)[i] = InstanceFileData(theEnv)->CurrentReadBuffer[i + InstanceFileData(theEnv)->CurrentReadBufferOffset];
         InstanceFileData(theEnv)->CurrentReadBufferOffset += bufsz;
         if (InstanceFileData(theEnv)->CurrentReadBufferOffset == InstanceFileData(theEnv)->CurrentReadBufferSize)
           FreeReadBuffer(theEnv);
        }
      else
        {
         if (InstanceFileData(theEnv)->CurrentReadBufferOffset < InstanceFileData(theEnv)->CurrentReadBufferSize)
           {
            for (i = 0L ; i < amountLeftToRead ; i++)
              ((char *) buf)[i] = InstanceFileData(theEnv)->CurrentReadBuffer[i + InstanceFileData(theEnv)->CurrentReadBufferOffset];
            bufsz -= amountLeftToRead;
            buf = (void *) (((char *) buf) + amountLeftToRead);
           }
         FreeReadBuffer(theEnv);
         BufferedRead(theEnv,buf,bufsz);
        }
     }
   else
     {
      if (bufsz > MAX_BLOCK_SIZE)
        {
         InstanceFileData(theEnv)->CurrentReadBufferSize = bufsz;
         if (bufsz > (InstanceFileData(theEnv)->BinaryInstanceFileSize - InstanceFileData(theEnv)->BinaryInstanceFileOffset))
           {
            SystemError(theEnv,"INSFILE",2);
            EnvExitRouter(theEnv,EXIT_FAILURE);
           }
        }
      else if (MAX_BLOCK_SIZE >
              (InstanceFileData(theEnv)->BinaryInstanceFileSize - InstanceFileData(theEnv)->BinaryInstanceFileOffset))
        InstanceFileData(theEnv)->CurrentReadBufferSize = InstanceFileData(theEnv)->BinaryInstanceFileSize - InstanceFileData(theEnv)->BinaryInstanceFileOffset;
      else
        InstanceFileData(theEnv)->CurrentReadBufferSize = (unsigned long) MAX_BLOCK_SIZE;
      InstanceFileData(theEnv)->CurrentReadBuffer = (char *) genalloc(theEnv,InstanceFileData(theEnv)->CurrentReadBufferSize);
      GenReadBinary(theEnv,(void *) InstanceFileData(theEnv)->CurrentReadBuffer,InstanceFileData(theEnv)->CurrentReadBufferSize);
      for (i = 0L ; i < bufsz ; i++)
        ((char *) buf)[i] = InstanceFileData(theEnv)->CurrentReadBuffer[i];
      InstanceFileData(theEnv)->CurrentReadBufferOffset = bufsz;
      InstanceFileData(theEnv)->BinaryInstanceFileOffset += InstanceFileData(theEnv)->CurrentReadBufferSize;
     }
  }

/*****************************************************
  NAME         : FreeReadBuffer
  DESCRIPTION  : Deallocates buffer for binary reads
  INPUTS       : None
  RETURNS      : Nothing usefu
  SIDE EFFECTS : Binary global read buffer deallocated
  NOTES        : None
 *****************************************************/
static void FreeReadBuffer(
  void *theEnv)
  {
   if (InstanceFileData(theEnv)->CurrentReadBufferSize != 0L)
     {
      genfree(theEnv,(void *) InstanceFileData(theEnv)->CurrentReadBuffer,InstanceFileData(theEnv)->CurrentReadBufferSize);
      InstanceFileData(theEnv)->CurrentReadBuffer = NULL;
      InstanceFileData(theEnv)->CurrentReadBufferSize = 0L;
     }
  }

#endif /* BLOAD_INSTANCES */

#endif /* OBJECT_SYSTEM */



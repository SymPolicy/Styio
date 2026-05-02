#pragma once
#ifndef STYIO_IR_DECLARATION_H_
#define STYIO_IR_DECLARATION_H_

/* Styio IR Base */
class StyioIR;

/* SG = Styio General. Default/general IR nodes. */
class SGResId;
class SGType;

class SGConstBool;

class SGConstInt;
class SGConstFloat;

class SGConstChar;
class SGConstString;
class SGFormatString;

class SGStruct;

class SGCast;

class SGBinOp;
class SGCond; /* Conditional: An expression that can be evaluated to a boolean type. */

class SGVar;
class SGFlexBind;
class SGFinalBind;
class SGDynLoad;

class SGFuncArg;
class SGFunc;
class SGCall;
class SGExportDecl;
class SGExternBlock;

class SGReturn;

class SGLoop;
class SGForEach;
class SGRangeFor;
class SGIf;

class SGStateSnapLoad;
class SGStateHistLoad;
class SGSeriesAvgStep;
class SGSeriesMaxStep;
class SGMatch;
class SGBreak;
class SGContinue;

class SGUndef;
class SGFallback;
class SGWaveMerge;
class SGWaveDispatch;
class SGGuardSelect;
class SGEqProbe;

class SGSnapshotDecl;
class SGSnapshotShadowLoad;

/* SIO = Styio Input/Output. Files, standard streams, stdin/stdout/stderr,
   future network, and filesystem IO nodes. */
class SIOHandleAcquire;
class SIOFileLineIter;
class SIOStreamZip;
class SIOInstantPull;
class SIOListReadStdin;
class SIOResourceWriteToFile;
class SIOStdStreamWrite;
class SIOStdStreamLineIter;
class SIOStdStreamPull;

/* SC = Styio Collection. List, dictionary, and future data structures
   such as matrix. */
class SCListLiteral;
class SCDictLiteral;
class SCListClone;
class SCListLen;
class SCListGet;
class SCListSet;
class SCListToString;
class SCDictClone;
class SCDictLen;
class SCDictGet;
class SCDictSet;
class SCDictKeys;
class SCDictValues;
class SCDictToString;

// class SGIfElse;
// class SGForLoop;
// class SGWhileLoop;

class SGBlock;
class SGEntry;
class SGMainEntry;

/* IOIR */
class SIOPath;
class SIOPrint;
class SIORead;

#endif  // STYIO_IR_DECLARATION_H_

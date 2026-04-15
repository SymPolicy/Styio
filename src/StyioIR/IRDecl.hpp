#pragma once
#ifndef STYIO_IR_DECLARATION_H_
#define STYIO_IR_DECLARATION_H_

/* Styio IR Base */
class StyioIR;

/* General IRs */
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

class SGReturn;

class SGLoop;
class SGForEach;
class SGListLiteral;
class SGDictLiteral;
class SGRangeFor;
class SGIf;
class SGListReadStdin;
class SGListClone;
class SGListLen;
class SGListGet;
class SGListSet;
class SGListToString;
class SGDictClone;
class SGDictLen;
class SGDictGet;
class SGDictSet;
class SGDictKeys;
class SGDictValues;
class SGDictToString;

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

class SGHandleAcquire;
class SGFileLineIter;
class SGStreamZip;
class SGSnapshotDecl;
class SGSnapshotShadowLoad;
class SGInstantPull;
class SGResourceWriteToFile;
class SIOStdStreamWrite;
class SIOStdStreamLineIter;
class SIOStdStreamPull;

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

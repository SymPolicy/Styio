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

class SGFuncArg;
class SGFunc;
class SGCall;

class SGReturn;

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
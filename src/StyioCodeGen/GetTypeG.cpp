// [C++ STL]
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// [Styio]
#include "../StyioException/Exception.hpp"
#include "../StyioIR/GenIR/GenIR.hpp"
#include "../StyioToken/Token.hpp"
#include "../StyioUtil/BoundedType.hpp"
#include "../StyioUtil/Util.hpp"
#include "CodeGenVisitor.hpp"

#include "llvm/IR/DerivedTypes.h"

llvm::Type*
StyioToLLVM::toLLVMType(SGResId* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGType* node) {
  if (auto cap = styio_bounded_ring_capacity(node->data_type)) {
    return llvm::ArrayType::get(theBuilder->getInt64Ty(), *cap);
  }
  switch (node->data_type.option) {
    case StyioDataTypeOption::Bool:
      return theBuilder->getInt1Ty();
    case StyioDataTypeOption::Integer:
      return theBuilder->getInt64Ty();
    case StyioDataTypeOption::Float:
      return theBuilder->getDoubleTy();
    case StyioDataTypeOption::String:
      return llvm::PointerType::get(*theContext, 0);
    default:
      return theBuilder->getInt64Ty();
  }
};

llvm::Type*
StyioToLLVM::toLLVMType(SGConstBool* node) {
  return theBuilder->getInt1Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGConstInt* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGConstFloat* node) {
  return theBuilder->getDoubleTy();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGConstChar* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGConstString* node) {
  return llvm::PointerType::get(*theContext, 0);
};

llvm::Type*
StyioToLLVM::toLLVMType(SGFormatString* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGStruct* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGCast* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGBinOp* node) {
  return node->data_type->toLLVMType(this);
};

llvm::Type*
StyioToLLVM::toLLVMType(SGCond* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGVar* node) {
  /* Logical (pulse) value is scalar i64; storage may be [|n|] array (see SGFinalBind alloca). */
  if (styio_bounded_ring_capacity(node->var_type->data_type)) {
    return theBuilder->getInt64Ty();
  }
  return node->var_type->toLLVMType(this);
};

llvm::Type*
StyioToLLVM::toLLVMType(SGFlexBind* node) {
  if (auto* m = dynamic_cast<SGMatch*>(node->value)) {
    if (m->repr_kind == SGMatchReprKind::ExprMixed) {
      return llvm::PointerType::get(*theContext, 0);
    }
  }
  if (auto* fb = dynamic_cast<SGFallback*>(node->value)) {
    llvm::Type* pt = fb->primary->toLLVMType(this);
    llvm::Type* at = fb->alternate->toLLVMType(this);
    if (pt->isIntegerTy(64) && at->isPointerTy()) {
      return at;
    }
  }
  return node->var->toLLVMType(this);
};

llvm::Type*
StyioToLLVM::toLLVMType(SGFinalBind* node) {
  return node->var->var_type->toLLVMType(this);
};

llvm::Type*
StyioToLLVM::toLLVMType(SGFuncArg* node) {
  return node->arg_type->toLLVMType(this);
};

llvm::Type*
StyioToLLVM::toLLVMType(SGFunc* node) {
  return node->ret_type->toLLVMType(this);
};

llvm::Type*
StyioToLLVM::toLLVMType(SGCall* node) {
  if (llvm::Function* f = theModule->getFunction(node->func_name->as_str())) {
    return f->getReturnType();
  }
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGReturn* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGBlock* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGEntry* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGMainEntry* node) {
  return theBuilder->getInt64Ty();
};

llvm::Type*
StyioToLLVM::toLLVMType(SGLoop* node) {
  (void)node;
  return theBuilder->getVoidTy();
}

llvm::Type*
StyioToLLVM::toLLVMType(SGForEach* node) {
  (void)node;
  return theBuilder->getVoidTy();
}

llvm::Type*
StyioToLLVM::toLLVMType(SGListLiteral* node) {
  (void)node;
  return theBuilder->getInt64Ty();
}

llvm::Type*
StyioToLLVM::toLLVMType(SGStateSnapLoad* node) {
  (void)node;
  return theBuilder->getInt64Ty();
}

llvm::Type*
StyioToLLVM::toLLVMType(SGStateHistLoad* node) {
  (void)node;
  return theBuilder->getInt64Ty();
}

llvm::Type*
StyioToLLVM::toLLVMType(SGSeriesAvgStep* node) {
  (void)node;
  return theBuilder->getInt64Ty();
}

llvm::Type*
StyioToLLVM::toLLVMType(SGSeriesMaxStep* node) {
  (void)node;
  return theBuilder->getInt64Ty();
}

llvm::Type*
StyioToLLVM::toLLVMType(SGMatch* node) {
  if (node->repr_kind == SGMatchReprKind::ExprMixed) {
    return llvm::PointerType::get(*theContext, 0);
  }
  return theBuilder->getInt64Ty();
}

llvm::Type*
StyioToLLVM::toLLVMType(SGBreak* node) {
  (void)node;
  return theBuilder->getVoidTy();
}

llvm::Type*
StyioToLLVM::toLLVMType(SGContinue* node) {
  (void)node;
  return theBuilder->getVoidTy();
}

llvm::Type*
StyioToLLVM::toLLVMType(SGUndef* node) {
  (void)node;
  return theBuilder->getInt64Ty();
}

llvm::Type*
StyioToLLVM::toLLVMType(SGFallback* node) {
  llvm::Type* pt = node->primary->toLLVMType(this);
  llvm::Type* at = node->alternate->toLLVMType(this);
  if (pt->isIntegerTy(64) && at->isPointerTy()) {
    return at;
  }
  return theBuilder->getInt64Ty();
}

llvm::Type*
StyioToLLVM::toLLVMType(SGWaveMerge* node) {
  (void)node;
  return theBuilder->getInt64Ty();
}

llvm::Type*
StyioToLLVM::toLLVMType(SGWaveDispatch* node) {
  (void)node;
  return theBuilder->getVoidTy();
}

llvm::Type*
StyioToLLVM::toLLVMType(SGGuardSelect* node) {
  (void)node;
  return theBuilder->getInt64Ty();
}

llvm::Type*
StyioToLLVM::toLLVMType(SGEqProbe* node) {
  (void)node;
  return theBuilder->getInt64Ty();
}

llvm::Type*
StyioToLLVM::toLLVMType(SGHandleAcquire* node) {
  (void)node;
  return theBuilder->getVoidTy();
}

llvm::Type*
StyioToLLVM::toLLVMType(SGFileLineIter* node) {
  (void)node;
  return theBuilder->getVoidTy();
}

llvm::Type*
StyioToLLVM::toLLVMType(SGStreamZip* node) {
  (void)node;
  return theBuilder->getVoidTy();
}

llvm::Type*
StyioToLLVM::toLLVMType(SGSnapshotDecl* node) {
  (void)node;
  return theBuilder->getVoidTy();
}

llvm::Type*
StyioToLLVM::toLLVMType(SGSnapshotShadowLoad* node) {
  (void)node;
  return theBuilder->getInt64Ty();
}

llvm::Type*
StyioToLLVM::toLLVMType(SGInstantPull* node) {
  (void)node;
  return theBuilder->getInt64Ty();
}

llvm::Type*
StyioToLLVM::toLLVMType(SGResourceWriteToFile* node) {
  (void)node;
  return theBuilder->getVoidTy();
}

llvm::Type*
StyioToLLVM::toLLVMType(SIOStdStreamWrite* node) {
  (void)node;
  return theBuilder->getVoidTy();
}

llvm::Type*
StyioToLLVM::toLLVMType(SIOStdStreamLineIter* node) {
  (void)node;
  return theBuilder->getVoidTy();
}

llvm::Type*
StyioToLLVM::toLLVMType(SIOStdStreamPull* node) {
  (void)node;
  return llvm::PointerType::get(*theContext, 0);
}
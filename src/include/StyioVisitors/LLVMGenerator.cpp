// [C++ STL]
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// [Styio]
#include "../StyioAST/AST.hpp"
#include "../StyioException/Exception.hpp"
#include "../StyioToken/Token.hpp"
#include "./Util.hpp"

// [LLVM]
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"
#include "llvm/LinkAllIR.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/Utils.h"

// template <typename T>
// llvm::Type
// get_llvm_type(T ast) {
//   switch (ast->hint()) {
//     case StyioNodeHint::Bool:

//       break;

//     case StyioNodeHint::Int:
//       switch (ast->DType) {
//         case StyioDataType::i32:
//           return llvm_ir_builder->getInt32Ty();
//           // break;

//         case StyioDataType::i64:
//           return llvm_ir_builder->getInt64Ty();
//           // break;

//         case StyioDataType::undefined:
//           return llvm_ir_builder->getInt32Ty();
//           // break;

//         default:
//           return llvm_ir_builder->getInt32Ty();
//           // break;
//       }
//       break;

//     case StyioNodeHint::Float:

//       break;

//     case StyioNodeHint::Char:

//       break;

//     default:
//       break;
//   }
// }

void
im_here() {
  std::cout << "I am here!" << std::endl;
}

/*
  CreateEntryBlockAlloca: Create an alloca() instruction in the entry block of
  the function. This is used for mutable variables etc.
*/
llvm::AllocaInst*
StyioToLLVM::createAllocaFuncEntry(llvm::Function* TheFunction, llvm::StringRef VarName) {
  llvm::IRBuilder<> tmp_block(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
  return tmp_block.CreateAlloca(
    llvm::Type::getDoubleTy(*llvm_context), /* Type should be modified in the future. */
    nullptr,
    VarName
  );
}

llvm::Type*
StyioToLLVM::matchType(std::string type) {
  if (type == "i32") {
    return llvm_ir_builder->getInt32Ty();
  }
  else if (type == "i64") {
    return llvm_ir_builder->getInt64Ty();
  }
  else if (type == "f32") {
    return llvm_ir_builder->getFloatTy();
  }
  else if (type == "f64") {
    return llvm_ir_builder->getDoubleTy();
  }

  else if (type == "i1") {
    return llvm_ir_builder->getInt1Ty();
  }
  else if (type == "i8") {
    return llvm_ir_builder->getInt8Ty();
  }
  else if (type == "i16") {
    return llvm_ir_builder->getInt16Ty();
  }
  else if (type == "i128") {
    return llvm_ir_builder->getInt128Ty();
  }

  return llvm_ir_builder->getInt1Ty();
}

void
StyioToLLVM::print_type_checking(shared_ptr<StyioAST> program) {
  std::cout << "\033[1;32mAST\033[0m \033[1;33m--After-Type-Checking\033[0m"
            << "\n"
            << std::endl;
  std::cout << program->toString() << std::endl;
  std::cout << "\n"
            << std::endl;
}

int
StyioToLLVM::run_llvm_ir(shared_ptr<MainBlockAST> program) {
  this->toLLVM(program.get());

  std::error_code EC;
  llvm::raw_fd_ostream output_stream(
    "output.ll",
    EC,
    llvm::sys::fs::OpenFlags::OF_None
  );
  if (EC) {
    std::cerr << "Can't open file output.ll; " << EC.message() << std::endl;
  }
  /* write to current_work_directory/output.ll */
  llvm_module->print(output_stream, nullptr);

  return std::system("lli output.ll");
}

void
StyioToLLVM::print_llvm_ir(int lli_result) {
  string verifyModule_msg;

  if (llvm::verifyModule(*llvm_module)) {
    verifyModule_msg = "[\033[1;31mFAILED \033[0m] llvm::verifyModule()";
  }
  else {
    verifyModule_msg = "[\033[1;32mSUCCESS\033[0m] llvm::verifyModule()";
  }

  std::string lli_msg;
  if (lli_result) {
    lli_msg = "[\033[1;31mFAILED \033[0m] lli output.ll";
  }
  else {
    lli_msg = "[\033[1;32mSUCCESS\033[0m] lli output.ll";
  }

  std::cout << "\n";

  /* llvm ir -> stdout */
  llvm_module->print(llvm::outs(), nullptr);
  /* llvm ir -> stderr */
  // llvm_module -> print(llvm::errs(), nullptr);

  /* print to stdout */
  std::cout << "\n"
            << "\033[1;32mLLVM IR\033[0m"
            << "\n"
            << verifyModule_msg
            << "\n"
            << lli_msg
            << std::endl;
}

/*
  StyioNaive
  [:] BoolAST
*/

llvm::Value*
StyioToLLVM::toLLVM(BoolAST* ast) {
  if (ast->getValue()) {
    return llvm::ConstantInt::getTrue(*llvm_context);
  }
  else {
    return llvm::ConstantInt::getFalse(*llvm_context);
  }
}

llvm::Value*
StyioToLLVM::toLLVM(NoneAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(EOFAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(EmptyAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(EmptyBlockAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(PassAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(BreakAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(ReturnAST* ast) {
  return llvm_ir_builder->CreateRet(ast->getExpr()->toLLVM(this));
}

llvm::Value*
StyioToLLVM::toLLVM(CommentAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(IdAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);

  const string& varname = ast->getAsStr();
  
  if (mutable_variables.contains(varname)) {
    llvm::AllocaInst* variable = mutable_variables[varname];
    
    return llvm_ir_builder->CreateLoad(variable->getAllocatedType(), variable);
    // return llvm_ir_builder->CreateLoad(variable->getAllocatedType(), variable, varname.c_str());
    // return variable; /* This line is WRONG! I keep it for debug. */
  }

  if (named_values.contains(varname)) {
    return named_values[varname];
    // return variable; /* This line is WRONG! I keep it for debug. */
  }

  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(VarAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(ArgAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(OptArgAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(OptKwArgAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(VarTupleAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);

  auto& vars = ast->getArgs();
  for (auto const& s : vars) {
    s->toLLVM(this);
  }

  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(DTypeAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(IntAST* ast) {
  switch (ast->getType()) {
    case StyioDataType::i1: {
      return llvm_ir_builder->getInt1(std::stoi(ast->getValue()));
    } break;

    case StyioDataType::i8: {
      return llvm_ir_builder->getInt8(std::stoi(ast->getValue()));
    } break;

    case StyioDataType::i16: {
      return llvm_ir_builder->getInt16(std::stoi(ast->getValue()));
    } break;

    case StyioDataType::i32: {
      return llvm_ir_builder->getInt32(std::stoi(ast->getValue()));
    } break;

    case StyioDataType::i64: {
      return llvm_ir_builder->getInt64(std::stoi(ast->getValue()));
    } break;

    case StyioDataType::i128: {
      return llvm::ConstantInt::get(llvm::Type::getInt128Ty(*llvm_context), std::stoi(ast->getValue()));
    } break;

    default: {
      return llvm_ir_builder->getInt32(std::stoi(ast->getValue()));
    } break;
  }
}

llvm::Value*
StyioToLLVM::toLLVM(FloatAST* ast) {
  switch (ast->getType()) {
    case StyioDataType::f64:
      return llvm::ConstantFP::get(*llvm_context, llvm::APFloat(std::stod(ast->getValue())));

    default:
      return llvm::ConstantFP::get(*llvm_context, llvm::APFloat(std::stod(ast->getValue())));
  }
}

llvm::Value*
StyioToLLVM::toLLVM(CharAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(StringAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(TypeConvertAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(FmtStrAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(ResourceAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(LocalPathAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(RemotePathAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(WebUrlAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(DBUrlAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(ExtPackAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(ListAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(TupleAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(SetAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(RangeAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(SizeOfAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(BinOpAST* ast) {
  llvm::Value* output = llvm_ir_builder->getInt32(0);

  llvm::Value* l_val = ast->getLhs()->toLLVM(this);
  llvm::Value* r_val = ast->getRhs()->toLLVM(this);

  switch (ast->getOperand()) {
    case StyioNodeHint::Bin_Add: {
      return llvm_ir_builder->CreateAdd(l_val, r_val);
    }

    break;

    case StyioNodeHint::Bin_Sub: {
      return llvm_ir_builder->CreateSub(l_val, r_val);
    }

    break;

    case StyioNodeHint::Bin_Mul: {
      return llvm_ir_builder->CreateMul(l_val, r_val);
    }

    break;

    case StyioNodeHint::Bin_Div: {
      // llvm_ir_builder -> CreateFDiv(l_val, r_val, "add");
    }

    break;

    case StyioNodeHint::Bin_Pow:
      // llvm_ir_builder -> CreateFAdd(l_val, r_val, "add");

      break;

    case StyioNodeHint::Bin_Mod:
      // llvm_ir_builder -> CreateFAdd(l_val, r_val, "add");

      break;

    case StyioNodeHint::Inc_Add:
      /* code */
      break;

    case StyioNodeHint::Inc_Sub:
      /* code */
      break;

    case StyioNodeHint::Inc_Mul:
      /* code */
      break;

    case StyioNodeHint::Inc_Div:
      /* code */
      break;

    default:
      break;
  }

  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(BinCompAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(CondAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(CallAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(ListOpAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

/*
  FlexBind

  Other Names For Search:
  - Flexible Binding
  - Mutable Variable
  - Mutable Assignment
*/
llvm::Value*
StyioToLLVM::toLLVM(FlexBindAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);

  // // Look this variable up in the function.
  // llvm::AllocaInst* A = named_values[ast->getName()];
  // if (!A) {
  //   string errmsg = string("varname not found");
  //   throw StyioSyntaxError(errmsg);
  //   return nullptr;
  // }

  // // Load the value.
  // return llvm_ir_builder->CreateLoad(A->getAllocatedType(), A, Name.c_str());

  switch (ast->getValue()->hint()) {
    case StyioNodeHint::Int: {
      const string& varname = ast->getName();
      if (mutable_variables.contains(varname)) {
        llvm::AllocaInst* variable = mutable_variables[varname];
        llvm_ir_builder->CreateStore(ast->getValue()->toLLVM(this), variable);
      }
      else {
        llvm::AllocaInst* variable = llvm_ir_builder->CreateAlloca(
          llvm_ir_builder->getInt32Ty(),
          nullptr,
          varname.c_str()
        );
        mutable_variables[varname] = variable;

        llvm_ir_builder->CreateStore(ast->getValue()->toLLVM(this), variable);
      }
    }

    break;

    case StyioNodeHint::Float: {
      const string& varname = ast->getName();
      if (mutable_variables.contains(varname)) {
        llvm::AllocaInst* variable = mutable_variables[varname];
        llvm_ir_builder->CreateStore(ast->getValue()->toLLVM(this), variable);
      }
      else {
        llvm::AllocaInst* variable = llvm_ir_builder->CreateAlloca(
          llvm_ir_builder->getDoubleTy(),
          nullptr,
          varname.c_str()
        );
        mutable_variables[varname] = variable;

        llvm_ir_builder->CreateStore(ast->getValue()->toLLVM(this), variable);
      }
    }

    break;

    case StyioNodeHint::Id: {
      const string& varname = ast->getName();
      if (mutable_variables.contains(varname)) {
        llvm::AllocaInst* variable = mutable_variables[varname];
        llvm_ir_builder->CreateStore(ast->getValue()->toLLVM(this), variable);
      }
      else {
        llvm::AllocaInst* variable = llvm_ir_builder->CreateAlloca(llvm_ir_builder->getInt32Ty(), nullptr);
        mutable_variables[varname] = variable;

        llvm_ir_builder->CreateStore(ast->getValue()->toLLVM(this), variable);
      }
    }

    break;

    case StyioNodeHint::BinOp: {
      const string& varname = ast->getName();
      if (mutable_variables.contains(varname)) {
        llvm::AllocaInst* variable = mutable_variables[varname];
        // llvm_ir_builder->CreateLoad(variable->getAllocatedType(), variable, varname.c_str());
        llvm_ir_builder->CreateStore(ast->getValue()->toLLVM(this), variable);
      }
      else {
        // llvm::AllocaInst* variable = llvm_ir_builder->CreateAlloca(llvm_ir_builder->getInt32Ty(), nullptr, varname);
        llvm::AllocaInst* variable = llvm_ir_builder->CreateAlloca(llvm_ir_builder->getInt32Ty(), nullptr);
        mutable_variables[varname] = variable;

        llvm_ir_builder->CreateStore(ast->getValue()->toLLVM(this), variable);
      }
    }

    break;

    default:

      break;
  }

  // llvm_ir_builder->CreateAlloca(llvm::Type::getDoubleTy(*llvm_context), nullptr, ast->getName());

  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(FinalBindAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);

  switch (ast->getValue()->hint()) {
    case StyioNodeHint::Int: {
      const string& varname = ast->getName();
      if (named_values.contains(varname)) {
        /* ERROR */
      }
      else {
        named_values[varname] = ast->getValue()->toLLVM(this);
      }
    } break;

    case StyioNodeHint::BinOp: {
      const string& varname = ast->getName();
      if (named_values.contains(varname)) {
        /* ERROR */
      }
      else {
        named_values[varname] = ast->getValue()->toLLVM(this);
      }
    }

    default: {
    } break;
  }

  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(StructAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(ReadFileAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(PrintAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(ForwardAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);

  switch (ast->hint()) {
    case StyioNodeHint::Fill_Forward: {
      // ast -> getArgs() -> toLLVM(this);
      ast->getThen()->toLLVM(this);
    } break;

    default:
      break;
  }

  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(CheckEqAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(CheckIsInAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(FromToAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(InfiniteAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(AnonyFuncAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(FuncAST* ast) {
  if (ast->hasName() && ast->hasArgs() && ast->hasRetType()) {
    /* FuncAST -> Forward -> VarTuple -> [<VarAST>..] */
    auto& sf_args = ast->getForward()->getArgs()->getArgs();

    std::vector<llvm::Type*> lf_args;
    for (auto& arg : sf_args) {
      lf_args.push_back(matchType(arg->getTypeStr()));
    }

    llvm::FunctionType* lf_type = llvm::FunctionType::get(
      /* Result (Type) */ matchType(ast->getRetTypeStr()),
      /* Params (Type) */ lf_args,
      /* isVarArg */ false
    );

    llvm::Function* lfunc =
      llvm::Function::Create(lf_type, llvm::GlobalValue::ExternalLinkage, ast->getName(), *llvm_module);

    for (size_t i = 0; i < lf_args.size(); i++) {
      lfunc->getArg(i)->setName(sf_args.at(i)->getName());
    }

    llvm::BasicBlock* block =
      llvm::BasicBlock::Create(*llvm_context, "entry", lfunc);

    llvm_ir_builder->SetInsertPoint(block);

    ast->getForward()->toLLVM(this);

    llvm::verifyFunction(*lfunc);
  }

  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(IterAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(LoopAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(CondFlowAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(CasesAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(MatchCasesAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);
  return output;
}

llvm::Value*
StyioToLLVM::toLLVM(SideBlockAST* ast) {
  auto output = llvm::ConstantInt::getFalse(*llvm_context);

  auto& stmts = ast->getStmts();
  for (auto const& s : stmts) {
    s->toLLVM(this);
  }

  return output;
}

llvm::Function*
StyioToLLVM::toLLVM(MainBlockAST* ast) {
  /*
    Get Void Type: llvm::Type::getVoidTy(*llvm_context)
    Use Void Type: nullptr
  */
  llvm::FunctionType* func_type = llvm::FunctionType::get(llvm_ir_builder->getInt32Ty(), false);
  llvm::Function* main_func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage, "main", *llvm_module);
  llvm::BasicBlock* entry = llvm::BasicBlock::Create(*llvm_context, "entrypoint", main_func);
  llvm_ir_builder->SetInsertPoint(entry);

  auto& stmts = ast->getStmts();
  for (auto const& s : stmts) {
    s->toLLVM(this);
  }

  llvm_ir_builder->CreateRet(llvm_ir_builder->getInt32(0));

  return main_func;
}
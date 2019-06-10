//===-- Execution.cpp - Implement code to simulate the program ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file contains the actual instruction interpreter.
//
//===----------------------------------------------------------------------===//

#include "WhiteBoxInterpreter.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/IntrinsicLowering.h"
#include "llvm/ExecutionEngine/Action.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cmath>
using namespace llvm;

#define DEBUG_TYPE "WhiteBoxInterpreter"

STATISTIC(NumDynamicInsts, "Number of dynamic instructions executed");

WhiteBoxInterpreter::WhiteBoxInterpreter(std::unique_ptr<Module> M, Action *action)
  : Interpreter(std::move(M)) {
  this->action = action;
  this->action->setInterpreter(this);
}



/// run - Start execution with the specified function and arguments.
///
GenericValue WhiteBoxInterpreter::runFunction(Function *F,
					 ArrayRef<GenericValue> ArgValues) {
  assert (F && "Function *F was null at entry to run()");

  // Try extra hard not to pass extra args to a function that isn't
  // expecting them.  C programmers frequently bend the rules and
  // declare main() with fewer parameters than it actually gets
  // passed, and the interpreter barfs if you pass a function more
  // parameters than it is declared to take. This does not attempt to
  // take into account gratuitous differences in declared types,
  // though.
  const size_t ArgCount = F->getFunctionType()->getNumParams();
  ArrayRef<GenericValue> ActualArgs =
      ArgValues.slice(0, std::min(ArgValues.size(), ArgCount));

  // Set up the function call.
  callFunction(F, ActualArgs);

  // Start executing the function.
  run();

  return this->ExitValue;
}


void WhiteBoxInterpreter::run() {
  action->setECStack(&ECStack);
  while (!ECStack.empty()) {
    // Interpret a single instruction & increment the "PC".
    ExecutionContext &SF = ECStack.back();  // Current stack frame
    Instruction &I = *SF.CurInst++;         // Increment before execute

    // Track the number of dynamic instructions executed.
    ++NumDynamicInsts;

    LLVM_DEBUG(dbgs() << "About to interpret: " << I);
    action->beforeVisitInst(I, SF);
    if (!action->skipExecuteInst(I)) {
      // outs() << I << "\n";
      visit(I);   // Dispatch to one of the visit* methods...
    }
    action->afterVisitInst(I, SF);
  }
}


//===----------------------------------------------------------------------===//
// control flow instruction
//===----------------------------------------------------------------------===//

// RETURN instruction
// ex: ret i32 0
// visitReturnInst(ReturnInst &I);

// branch instruction
// ex: br label %14   (unconditional branch)
// ex: br i1 %cond, label %IfEqual, label %IfUnequal (conditional branch)
// TraceAction: neither type is to be traced
// FaultAction: FIXME
// visitBranchInst(BranchInst &I);

// switch instruction
// ex: switch i32 %10, label %14 [
//       i32 10, label %11
//     ]
// TraceAction: not act
// FaultAction: TODO
// visitSwitchInst(SwitchInst &I);

// FIXME: to finish the implementation
// visitIndirectBrInst(IndirectBrInst &I);

//===----------------------------------------------------------------------===//
// binary operators and comparisons
//===----------------------------------------------------------------------===//

// Binary Operators
// ex: %16 = sub nsw i32 %15, 19
// visitBinaryOperator(BinaryOperator &I);

// icmp instruction
// ex: %5 = icmp sgt i32 %4, 10
// visitICmpInst(ICmpInst &I)

// fcmp instruction
// ex: %21 = fcmp ogt double %20, 1.000000e+0
// visitFCmpInst(FCmpInst &I)

//===----------------------------------------------------------------------===//
// memory operators
//===----------------------------------------------------------------------===//

// LOAD instruction
// ex: %4 = load i32, i32* %2, align 4
// visitLoadInst(LoadInst &I);

// STORE instruction
// ex: store i32 0, i32* %1, align 4
// visitStoreInst(StoreInst &I);

// GetElementPtr instruction
// ex: %32 = getelementptr inbounds [3 x i32], [3 x i32]* %4, i64 0, i64 %31
// visitGetElementPtrInst(GetElementPtrInst &I);

//===----------------------------------------------------------------------===//
// ternary operator
//===----------------------------------------------------------------------===//

// SELECT instruction: ternary operator
// ex: %X = select i1 true, i8 17, i8 42
// visitSelectInst(SelectInst &I)

//===----------------------------------------------------------------------===//
// call
//===----------------------------------------------------------------------===//

// visitCallInst(CallInst &I)
// visitInvokeInst(InvokeInst &I)

//===----------------------------------------------------------------------===//
// shift instructions
//===----------------------------------------------------------------------===//

// visitShl(BinaryOperator &I)
// visitLShr(BinaryOperator &I)
// visitAShr(BinaryOperator &I)

//===----------------------------------------------------------------------===//
//                 Miscellaneous Instruction Implementations
//===----------------------------------------------------------------------===//

// visitVAArgInst(VAArgInst &I)
// visitExtractElementInst(ExtractElementInst &I)
// visitInsertElementInst(InsertElementInst &I)
// visitShuffleVectorInst(ShuffleVectorInst &I)
// visitExtractValueInst(ExtractValueInst &I)
// visitInsertValueInst(InsertValueInst &I)

//

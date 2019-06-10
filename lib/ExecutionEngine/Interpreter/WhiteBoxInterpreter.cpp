//===- Interpreter.cpp - Top-Level LLVM Interpreter Implementation --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the top-level functionality for the LLVM interpreter.
// This interpreter is designed to be a very simple, portable, inefficient
// interpreter.
//
//===----------------------------------------------------------------------===//

#include "Interpreter.h"
#include "WhiteBoxInterpreter.h"
#include <cstring>
using namespace llvm;

namespace {

static struct RegisterWBInterp {
  RegisterWBInterp() { WhiteBoxInterpreter::Register(); }
} WBInterpRegistrator;

}

extern "C" void LLVMLinkInWhiteBoxInterpreter() { }

/// Create a new white-box interpreter object.
///
ExecutionEngine *WhiteBoxInterpreter::create(std::unique_ptr<Module> M,
					Action *action,
					std::string *ErrStr) {
  // Tell this Module to materialize everything and release the GVMaterializer.
  if (Error Err = M->materializeAll()) {
    std::string Msg;
    handleAllErrors(std::move(Err), [&](ErrorInfoBase &EIB) {
      Msg = EIB.message();
    });
    if (ErrStr)
      *ErrStr = Msg;
    // We got an error, just return 0
    return nullptr;
  }

  return new WhiteBoxInterpreter(std::move(M), action);
}

#ifndef LLVM_LIB_EXECUTIONENGINE_INTERPRETER_WHITEBOXINTERPRETER_H
#define LLVM_LIB_EXECUTIONENGINE_INTERPRETER_WHITEBOXINTERPRETER_H

#include "llvm/ExecutionEngine/Action.h"
#include "Interpreter.h"

namespace llvm {

class WhiteBoxInterpreter : public Interpreter {
  using base_type = Interpreter;
  Action * action;

public:
  explicit WhiteBoxInterpreter(std::unique_ptr<Module> M, Action *action);

  static void Register() {
    WBInterpCtor = create;
  }

  /// Create an white-box interpreter ExecutionEngine.
  ///
  static ExecutionEngine *create(std::unique_ptr<Module> M,
				 Action *action,
                                 std::string *ErrorStr = nullptr);

  GenericValue runFunction(Function *F,
                           ArrayRef<GenericValue> ArgValues) override;
  void run() ;

};

} // End llvm namespace

#endif

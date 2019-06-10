#ifndef LLVM_EXECUTIONENGINE_ACTION_H
#define LLVM_EXECUTIONENGINE_ACTION_H

#include "llvm/ADT/APInt.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/IR/InstVisitor.h"
#include <list>

namespace llvm {

struct ExecutionContext;
class Interpreter;


class ECStackAccessor {
private:
  std::vector<ExecutionContext> * ECStack;

public:
  virtual ~ECStackAccessor() {}
  virtual void setECStack(std::vector<ExecutionContext> * ECStack) {
    this->ECStack = ECStack;
  }
  ExecutionContext *currentEC();
};

class Action: public ECStackAccessor {
private:
  Interpreter * interpreter;
public:
  Action() {}
  virtual ~Action() {}

  virtual void setInterpreter(Interpreter * interpreter);
  Interpreter * getInterpreter();
  virtual void beforeVisitInst(Instruction &I, ExecutionContext &SF) {}
  virtual bool skipExecuteInst(Instruction &I) {return false;}
  virtual void afterVisitInst(Instruction &I, ExecutionContext &SF) {}
  virtual void print(raw_ostream &ROS) {}
};


class ActionFactory {
public:
  Action * createAction(const char *);
};

class ChainedAction : public Action {
  std::list<Action *> actionList;

public:
  void addAction(Action *action) {
    actionList.push_back(action);
  }

  void setInterpreter(Interpreter * interpreter) override {
    for (Action *action : actionList) {
      action->setInterpreter(interpreter);
    }
  }

  void setECStack(std::vector<ExecutionContext> * ECStack) override {
    for (Action *action : actionList) {
      action->setECStack(ECStack);
    }
  }

  void beforeVisitInst(Instruction &I, ExecutionContext &SF) override {
    for (Action *action : actionList) {
      action->beforeVisitInst(I, SF);
    }
  };

  bool skipExecuteInst(Instruction &I) override {
    bool ret = false;
    for (Action *action : actionList) {
      ret = ret || action->skipExecuteInst(I);
    }

    return ret;
  }

  void afterVisitInst(Instruction &I, ExecutionContext &SF) override {
    for (Action *action : actionList) {
      action->afterVisitInst(I, SF);
    }
  };

  void print(raw_ostream &ROS) override {
    for (Action *action : actionList) {
      action->print(ROS);
    }
  }

};

class HelloWorldAction : public Action {
public:
  void beforeVisitInst(Instruction &I, ExecutionContext &SF) override {
    outs() << "(helloworld) Before visit: " << I << "\n";
  };

  bool skipExecuteInst(Instruction &I) override {
    return false;
  }

  void afterVisitInst(Instruction &I, ExecutionContext &SF) override {
    outs() << "(helloworld) After visit: " << I << "\n";
  };

  void print(raw_ostream &ROS) override {
    ROS << "HelloWorld => An example implementation of Action.\n";
  }

};

class TraceProcessor: public InstVisitor<TraceProcessor>,
		      public ECStackAccessor {
private:
  Action * action;
  void traceAPInt(APInt Val);

  // default visitor for most of instructions
  void defaultVisitor(Value &I);
  void visitNotImplementedInst(Value &I) {
    errs() << "Instruction not interpretable yet >>" << I << "\n";
    llvm_unreachable(nullptr);
  }
  GenericValue getOperandValue(Value *I, ExecutionContext &SF);

public:
  TraceProcessor(Action * action) { this->action = action; }

  void trace(GenericValue GV, Type *Ty);

  // =========== instruction visitors ============

  // ----- Terminator Instructions ----
  // `ret`: taking care the return values to the caller
  void visitReturnInst(ReturnInst &I);
  // `br`, `switch`, `indirectbr`: no need to trace, the conditions are
  //                               contained in registers
  void visitBranchInst(BranchInst &I) {}
  void visitSwitchInst(SwitchInst &I) {}

  // ---- Binary Instructions ----
  void visitBinaryOperator(BinaryOperator &I) { defaultVisitor(I); }
  void visitICmpInst(ICmpInst &I) { defaultVisitor(I); }
  void visitFCmpInst(FCmpInst &I) { defaultVisitor(I); }

  // ---- Memory Access Instructions ----
  // `alloca`: no need to trace
  void visitAllocaInst(AllocaInst &I) {}
  void visitLoadInst(LoadInst &I) { defaultVisitor(I); }
  void visitStoreInst(StoreInst &I);
  void visitGetElementPtrInst(GetElementPtrInst &I) { defaultVisitor(I); }

  // ---- Value Trunc and Extend Instructions ----
  // The following instructions are ignored by TraceAction
  // `trunc .. to`, `fptrunc .. to`,
  // `zext .. to`, `sext .. to`, `fpext .. to`,
  // `uitofp .. to`, `sitofp .. to`,
  // `fptoui .. to`, `fptosi .. to`,
  // `ptrtoint .. to`, `inttoptr .. to`,
  // `bitcast .. to`
  void visitTruncInst(TruncInst &I) {}
  void visitZExtInst(ZExtInst &I) {}
  void visitSExtInst(SExtInst &I) {}
  void visitBitCastInst(BitCastInst &I) {}


  // ---- Conditional (ternary) operator ----
  void visitSelectInst(SelectInst &I) { defaultVisitor(I); }

  // ---- Function Calls ----
  // `call`, `invoke`: returned values will be taken care by `ret` instruction
  void visitCallInst(CallInst &I) {}


  // ---- Shift Instructions ----
  // `shl`, `lshr`, `ashr`: no need to trace

  // ---- misc ----
  void visitVAArgInst(VAArgInst &I) { visitNotImplementedInst(I); }
  // `extractelement`, `insertelement`, `shufflevector`: no need to trace
  // `extractvalue`, `insertvalue`: no need to trace

  void visitInstruction(Instruction &I) { visitNotImplementedInst(I); }
};

class TraceAction : public Action, public InstVisitor<TraceAction> {
  TraceProcessor postProcessor = TraceProcessor(this);

public:
  void setECStack(std::vector<ExecutionContext> * ECStack) override {
    postProcessor.setECStack(ECStack);
  }

  void afterVisitInst(Instruction &I, ExecutionContext &SF) override {
    postProcessor.visit(I);
  }

  void print(raw_ostream &ROS) override;
};

// operators
inline raw_ostream &operator<<(raw_ostream &OS, Action& action)
{
  action.print(OS);
  return OS;
}


}

#endif

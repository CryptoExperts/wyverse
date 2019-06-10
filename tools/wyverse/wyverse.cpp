//===- wyverse.cpp - LLVM Interpreter / Dynamic compiler ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This utility provides a simple wrapper around the LLVM Execution Engines,
// which allow the direct execution of LLVM programs through an interpreter.
//
//===----------------------------------------------------------------------===//

#include "llvm/Config/llvm-config.h"
#include "llvm/ExecutionEngine/Action.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/WithColor.h"
#include "logo-ascii.inc"
#include <cerrno>

#ifdef __CYGWIN__
#include <cygwin/version.h>
#if defined(CYGWIN_VERSION_DLL_MAJOR) && CYGWIN_VERSION_DLL_MAJOR<1007
#define DO_NOTHING_ATEXIT 1
#endif
#endif

using namespace llvm;

#define DEBUG_TYPE "wyverse"

namespace {

  cl::opt<std::string>
  InputFile(cl::desc("<input bitcode>"), cl::Positional, cl::init("-"));

  cl::list<std::string>
  InputArgv(cl::ConsumeAfter, cl::desc("<program arguments>..."));


  cl::opt<std::string>
  EntryFunc("entry-function",
            cl::desc("Specify the entry function (default = 'main') "
                     "of the executable"),
            cl::value_desc("function"),
            cl::init("main"));

  cl::list<std::string>
  ExtraModules("extra-module",
         cl::desc("Extra modules to be loaded"),
         cl::value_desc("input bitcode"));

  cl::opt<std::string>
  FakeArgv0("fake-argv0",
            cl::desc("Override the 'argv[0]' value passed into the executing"
                     " program"), cl::value_desc("executable"));

  enum ActionType {
		helloworld, trace
  };
  cl::list<ActionType>
  ActionList(cl::desc("Available Actions:"),
	     cl::values(clEnumVal(helloworld, "An example of action"),
			clEnumVal(trace,      "Tracing memory / register")));

  cl::opt<int>
  MemoryRead("memory-read",
	     cl::desc("Choose the memory read width to be traced "
		      "(trace action only): -1 disable, 0 all "),
	     cl::value_desc("bytes"),
	     cl::init(0));
  cl::opt<int>
  MemoryWrite("memory-write",
	     cl::desc("Choose the memory write width to be traced "
		      "(trace action only): -1 disable, 0 all "),
	     cl::value_desc("bytes"),
	     cl::init(0));
  cl::opt<int>
  StackAccess("stack",
	     cl::desc("Choose the stack access width to be traced "
		      "(trace action only): -1 disable, 0 all "),
	     cl::value_desc("bytes"),
	     cl::init(0));
  cl::opt<int>
  RegisterAccess("register",
	     cl::desc("Choose the register width to be traced "
		      "(trace action only): -1 disable, 0 all "),
	     cl::value_desc("bytes"),
	     cl::init(0));


  ExitOnError ExitOnErr;
}

LLVM_ATTRIBUTE_NORETURN
static void reportError(SMDiagnostic Err, const char *ProgName) {
  Err.print(ProgName, errs());
  exit(1);
}

inline const char* ActionTypeToString(ActionType at)
{
  switch (at) {
  case helloworld:   return "helloworld";
  case trace:        return "trace";
  // default:           return "[Unknown ActionType]";
  }
}


//===----------------------------------------------------------------------===//
// main Driver function
//
int main(int argc, char **argv, char * const *envp) {
  WithColor(outs(), raw_ostream::BLUE, true) << AsciiLogo;

  InitLLVM X(argc, argv);

  if (argc > 1)
    ExitOnErr.setBanner(std::string(argv[0]) + ": ");
  cl::ParseCommandLineOptions(argc, argv, "Wyverse interpreter\n");


  // Create a chain of actions
  ChainedAction *actionList = new ChainedAction(); // to free up
  ActionFactory actionFactory;
  for (unsigned i = 0; i != ActionList.size(); ++i) {
    const char * actionType = ActionTypeToString(ActionList[i]);
    Action *action = actionFactory.createAction(actionType);
    if (action)
      actionList->addAction(action);
  }

  WithColor stringOuts = WithColor(outs(), raw_ostream::GREEN);
  stringOuts << "====== Enabled actions ======\n\n";
  stringOuts.resetColor() << *actionList << "\n";
  stringOuts.changeColor(raw_ostream::GREEN) << "*****************************\n";
  stringOuts.resetColor();

  outs() << RegisterAccess << "  register access\n";

  LLVMContext Context;

  // Load the bitcode...
  SMDiagnostic Err;
  std::unique_ptr<Module> Owner = parseIRFile(InputFile, Err, Context);
  Module *Mod = Owner.get();
  if (!Mod)
    reportError(Err, argv[0]);

  std::string ErrorMsg;
  EngineBuilder builder(std::move(Owner));
  builder.setErrorStr(&ErrorMsg);
  builder.setEngineKind(EngineKind::WhiteBoxInterpreter);
  builder.setAction(actionList);

  // Create execution engine
  std::unique_ptr<ExecutionEngine> EE(builder.create());
  if (!EE) {
    if (!ErrorMsg.empty())
      WithColor::error(errs(), argv[0])
          << "error creating EE: " << ErrorMsg << "\n";
    else
      WithColor::error(errs(), argv[0]) << "unknown error creating EE!\n";
    exit(1);
  }

  // Load any additional modules specified on the command line.
  for (unsigned i = 0, e = ExtraModules.size(); i != e; ++i) {
    std::unique_ptr<Module> XMod = parseIRFile(ExtraModules[i], Err, Context);
    if (!XMod)
      reportError(Err, argv[0]);
    EE->addModule(std::move(XMod));
  }

  // If the user specifically requested an argv[0] to pass into the program,
  // do it now.
  if (!FakeArgv0.empty()) {
    InputFile = static_cast<std::string>(FakeArgv0);
  } else {
    // Otherwise, if there is a .bc suffix on the executable strip it off, it
    // might confuse the program.
    if (StringRef(InputFile).endswith(".bc"))
      InputFile.erase(InputFile.length() - 3);
  }

  // Add the module's name to the start of the vector of arguments to main().
  InputArgv.insert(InputArgv.begin(), InputFile);

  // Call the main function from M as if its signature were:
  //   int main (int argc, char **argv, const char **envp)
  // using the contents of Args to determine argc & argv, and the contents of
  // EnvVars to determine envp.
  //
  Function *EntryFn = Mod->getFunction(EntryFunc);
  if (!EntryFn) {
    WithColor::error(errs(), argv[0])
        << '\'' << EntryFunc << "\' function not found in module.\n";
    return -1;
  }

  // Reset errno to zero on entry to main.
  errno = 0;

  int Result = -1;


  // If the program doesn't explicitly call exit, we will need the Exit
  // function later on to make an explicit call, so get the function now.
  Constant *Exit = Mod->getOrInsertFunction("exit", Type::getVoidTy(Context),
					    Type::getInt32Ty(Context));
  EE->runStaticConstructorsDestructors(false);

  // Trigger compilation separately so code regions that need to be
  // invalidated will be known.
  (void)EE->getPointerToFunction(EntryFn);

  // Run main.
  Result = EE->runFunctionAsMain(EntryFn, InputArgv, envp);

  // Run static destructors.
  EE->runStaticConstructorsDestructors(true);

  // If the program didn't call exit explicitly, we should call it now.
  // This ensures that any atexit handlers get called correctly.
  if (Function *ExitF = dyn_cast<Function>(Exit)) {
    std::vector<GenericValue> Args;
    GenericValue ResultGV;
    ResultGV.IntVal = APInt(32, Result);
    Args.push_back(ResultGV);
    EE->runFunction(ExitF, Args);
    WithColor::error(errs(), argv[0]) << "exit(" << Result << ") returned!\n";
    abort();
  } else {
    WithColor::error(errs(), argv[0])
      << "exit defined with wrong prototype!\n";
    abort();
  }
  return Result;
}

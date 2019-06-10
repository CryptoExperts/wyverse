if ( LLVM_INCLUDE_UTILS )
  add_subdirectory(ChildTarget)
endif()

set(LLVM_LINK_COMPONENTS
  Core
  ExecutionEngine
  IRReader
  Interpreter
  Support
  TransformUtils
  )

add_llvm_tool(wyverse
  wyverse.cpp

  DEPENDS
  intrinsics_gen
  )
export_executable_symbols(wyverse)

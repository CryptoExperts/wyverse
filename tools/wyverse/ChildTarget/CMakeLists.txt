set(LLVM_LINK_COMPONENTS
  OrcJIT
  RuntimeDyld
  Support
  )

add_llvm_utility(wyverse-child-target
  ChildTarget.cpp

  DEPENDS
  intrinsics_gen
)

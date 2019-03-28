set(LLVM_LINK_COMPONENTS
  Option
  Support
  )

add_clang_tool(clang-ifso
  clang-ifso.cpp
  )

target_link_libraries(clang-ifso
  PRIVATE
  clangAST
  clangBasic
  clangDriver
  clangFrontend
  clangRewriteFrontend
  clangSerialization
  clangStaticAnalyzerFrontend
  clangTooling
  LLVMSupport
  )

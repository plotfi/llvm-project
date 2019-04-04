set(LLVM_LINK_COMPONENTS
  Option
  Support
  ObjectYAML
  )

add_clang_tool(clang-ifso
  clang-ifso.cpp
  yaml2elf.cpp
  Symbols2Yaml.cpp
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

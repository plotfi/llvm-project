; Check TODO
; REQUIRES: asserts
; RUN: opt -module-summary -module-hash %s -o %t1.bc
; RUN: opt -module-summary -module-hash %p/Inputs/two-codegen-rounds.ll -o %t2.bc

; RUN: llvm-lto -thinlto-action=run %t1.bc %t2.bc -thinlto-two-codegen-rounds \
; RUN:    -exported-symbol=a -exported-symbol=b -exported-symbol=c -exported-symbol=d \
; RUN:    -mcpu=aarch64-linux-gnu -enable-machine-outliner -debug-only=machine-outliner
; RUN: llvm-objdump --disassemble %t1.bc.thinlto.o | FileCheck %s

; CHECK: _OUTLINED_FUNCTION_0_

target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-ios"

define void @d(i32 %a, i32 %b, i32 %c, i32 %d) {
entry:
  call void @z(i32 1, i32 2, i32 3, i32 4)
  ret void
}

declare void @z(i32, i32, i32, i32)

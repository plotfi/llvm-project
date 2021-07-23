; REQUIRES: asserts
; RUN: opt %s -o %t.bc
; RUN: llvm-lto -enable-lto-internalization=false -enable-machine-outliner -use-linkonceodr-linkage-outlining=true -debug-only=order-file-state -order-outlined-functions=true -order-file-symbol=%p/Inputs/machine-outliner-order-file-symbol.txt %t.bc 2>&1 | FileCheck  %s --check-prefix=LTO-ORDER
; RUN: llvm-lto -thinlto-action=run -enable-machine-outliner -use-linkonceodr-linkage-outlining=true -debug-only=order-file-state -order-outlined-functions=true -order-file-symbol=%p/Inputs/machine-outliner-order-file-symbol.txt %t.bc 2>&1 | FileCheck  %s --check-prefix=THINLTO-ORDER
; RUN: llvm-objdump -d --no-leading-addr --no-leading-headers --no-show-raw-insn --print-imm-hex %t.bc.thinlto.o | FileCheck %s

target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-ios"

declare dso_local void @z1(i32 %a, i32 %b, i32 %c)
declare dso_local void @z2(i32 %a, i32 %b, i32 %c, i32 %d)
declare dso_local void @z3(i32 %a, i32 %b, i32 %c, i32 %d, i32 %e)

; LTO-ORDER-LABEL: Final Order File Symbols (8):
; LTO-ORDER: _c
; LTO-ORDER-NEXT: _OUTLINED_FUNCTION_0
; LTO-ORDER-NEXT: _OUTLINED_FUNCTION_2
; LTO-ORDER-NEXT: _OUTLINED_FUNCTION_3
; LTO-ORDER-NEXT: _a
; LTO-ORDER-NEXT: _OUTLINED_FUNCTION_4
; LTO-ORDER-NEXT: _OUTLINED_FUNCTION_1
; LTO-ORDER-NEXT: _b

; THINLTO-ORDER-LABEL: Final Order File Symbols (8):
; THINLTO-ORDER: _c
; THINLTO-ORDER-NEXT: _OUTLINED_FUNCTION_0_0
; THINLTO-ORDER-NEXT: _OUTLINED_FUNCTION_0_2
; THINLTO-ORDER-NEXT: _OUTLINED_FUNCTION_0_3
; THINLTO-ORDER-NEXT: _a
; THINLTO-ORDER-NEXT: _OUTLINED_FUNCTION_0_4
; THINLTO-ORDER-NEXT: _OUTLINED_FUNCTION_0_1
; THINLTO-ORDER-NEXT: _b

; CHECK-LABEL: <_OUTLINED_FUNCTION_0_0>:
; CHECK: #0x4

; CHECK-LABEL: <_OUTLINED_FUNCTION_0_1>:
; CHECK: #0x7

; CHECK-LABEL: <_OUTLINED_FUNCTION_0_2>:
; CHECK: #0x3

; CHECK-LABEL: <_OUTLINED_FUNCTION_0_3>:
; CHECK: #0x3

; CHECK-LABEL: <_OUTLINED_FUNCTION_0_4>:
; CHECK: #0xa

define i32 @main() {
entry:
  tail call void @a()
  tail call void @b()
  tail call void @c()
  ret i32 1
}

define void @a() noinline {
entry:
  tail call void @z1(i32 1, i32 2, i32 3)
  tail call void @z1(i32 8, i32 9, i32 10)
  tail call void @z2(i32 1, i32 2, i32 3, i32 4)
  tail call void @z2(i32 4, i32 5, i32 6, i32 7)
  tail call void @z3(i32 1, i32 2, i32 3, i32 4, i32 5)
  ret void
}

define void @b() noinline {
entry:
  tail call void @z2(i32 4, i32 5, i32 6, i32 7)
  tail call void @z1(i32 8, i32 9, i32 10)
  tail call void @z1(i32 1, i32 2, i32 3)
  ret void
}

define void @c() noinline {
entry:
  tail call void @z2(i32 1, i32 2, i32 3, i32 4)
  tail call void @z1(i32 1, i32 2, i32 3)
  tail call void @z3(i32 1, i32 2, i32 3, i32 4, i32 5)
  ret void
}

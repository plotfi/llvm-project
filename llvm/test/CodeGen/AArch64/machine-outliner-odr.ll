; RUN: opt %s -o %t.bc
; RUN: opt %p/Inputs/machine-outliner-odr-input-1.ll -o %t1.bc
; RUN: opt %p/Inputs/machine-outliner-odr-input-2.ll -o %t2.bc

; RUN: llvm-lto -thinlto-action=run --enable-machine-outliner -enable-linkonceodr-outlining=false %t.bc %t1.bc %t2.bc
; RUN: llvm-readelf --symbols %t1.bc.thinlto.o | FileCheck %s --check-prefix=CHECK1-NOODR
; RUN: llvm-readelf --symbols %t2.bc.thinlto.o | FileCheck %s --check-prefix=CHECK2-NOODR

; CHECK1-NOODR: Name: _OUTLINED_FUNCTION_0
; NOT-CHECK1-NOODR: WeakDef (0x80)
; NOT-CHECK1-NOODR: WeakRef (0x40)

; CHECK2-NOODR: Name: _OUTLINED_FUNCTION_0
; NOT-CHECK2-NOODR: WeakDef (0x80)
; NOT-CHECK2-NOODR: WeakRef (0x40)

; RUN: llvm-lto -thinlto-action=run --enable-machine-outliner -enable-linkonceodr-outlining=true %t.bc %t1.bc %t2.bc
; RUN: llvm-readelf --symbols %t1.bc.thinlto.o | FileCheck %s --check-prefix=CHECK1-ODR
; RUN: llvm-readelf --symbols %t2.bc.thinlto.o | FileCheck %s --check-prefix=CHECK2-ODR

; CHECK1-ODR: Name: _OUTLINED_FUNCTION_1_0
; CHECK1-ODR: WeakDef (0x80)
; CHECK1-ODR: WeakRef (0x40)

; CHECK2-ODR: Name: _OUTLINED_FUNCTION_2_0
; CHECK2-ODR: WeakDef (0x80)
; CHECK2-ODR: WeakRef (0x40)

target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-ios"

@g = common dso_local local_unnamed_addr global i32 0, align 4


define dso_local void @z(i32 %a, i32 %b, i32 %c, i32 %d) noinline {
entry:
  store i32 %a, i32* @g, align 4
  ret void
}

define dso_local i32 @main() {
entry:
  tail call void bitcast (void (...)* @a to void ()*)()
  tail call void @b(i32 1) #3
  tail call void bitcast (void (...)* @c to void ()*)()
  tail call void @d(i32 2) #3
  %0 = load i32, i32* @g, align 4
  ret i32 %0
}

declare dso_local void @a(...) local_unnamed_addr

declare dso_local void @b(i32) local_unnamed_addr

declare dso_local void @c(...) local_unnamed_addr

declare dso_local void @d(i32) local_unnamed_addr

; RUN: llc -verify-machineinstrs -enable-machine-outliner -outlining-iterations=0 -mtriple=aarch64-linux-gnu -debug-only=machine-outliner  %s -o /dev/null 2>&1 | FileCheck %s -check-prefix=ITER0
; RUN: llc -verify-machineinstrs -enable-machine-outliner -outlining-iterations=1 -mtriple=aarch64-linux-gnu < %s | FileCheck %s -check-prefix=ITER1
; RUN: llc -verify-machineinstrs -enable-machine-outliner -outlining-iterations=2 -mtriple=aarch64-linux-gnu < %s | FileCheck %s -check-prefix=ITER2
; RUN: llc -verify-machineinstrs -enable-machine-outliner -outlining-iterations=3 -mtriple=aarch64-linux-gnu -debug-only=machine-outliner  %s -o /dev/null 2>&1 | FileCheck %s -check-prefix=ITER3

declare void @z1(i32, i32)
declare void @z2(i32, i32, i32)

define void @a(i32 %p1) {
entry:
; ITER1-LABEL: a:
; ITER1:         bl      OUTLINED_FUNCTION_0
; ITER1-NEXT:    orr     w1, wzr, #0x3
; ITER1-NEXT:    bl      OUTLINED_FUNCTION_1
; ITER1-NEXT:    orr     w1, wzr, #0x2
; ITER1-NEXT:    bl      OUTLINED_FUNCTION_1
; ITER1-NEXT:    bl      OUTLINED_FUNCTION_0

; ITER2-LABEL: a:
; ITER2:         bl      OUTLINED_FUNCTION_0
; ITER2-NEXT:    orr     w1, wzr, #0x3
; ITER2-NEXT:    bl      OUTLINED_FUNCTION_1
; ITER2-NEXT:    bl      OUTLINED_FUNCTION_2
; ITER2-NEXT:    bl      OUTLINED_FUNCTION_0
  call void @z1(i32 1,i32 3)
  call void @z2(i32 %p1, i32 3, i32 4)
  call void @z2(i32 %p1, i32 2, i32 4)
  call void @z1(i32 1,i32 3)
  ret void
}

define void @b(i32 %p1) {
entry:
; ITER1-LABEL: b:
; ITER1:         bl      OUTLINED_FUNCTION_0
; ITER1-NEXT:    orr     w1, wzr, #0x2
; ITER1-NEXT:    bl      OUTLINED_FUNCTION_1
; ITER1-NEXT:    orr     w1, wzr, #0x2
; ITER1-NEXT:    bl      OUTLINED_FUNCTION_1
; ITER1-NEXT:    bl      OUTLINED_FUNCTION_0

; ITER2-LABEL: b:
; ITER2:         bl      OUTLINED_FUNCTION_0
; ITER2-NEXT:    bl      OUTLINED_FUNCTION_2
; ITER2-NEXT:    bl      OUTLINED_FUNCTION_2
; ITER2-NEXT:    bl      OUTLINED_FUNCTION_0
  call void @z1(i32 1,i32 3)
  call void @z2(i32 %p1, i32 2, i32 4)
  call void @z2(i32 %p1, i32 2, i32 4)
  call void @z1(i32 1,i32 3)
  ret void
}

; ITER0: No outlining is performed

; ITER1-LABEL: OUTLINED_FUNCTION_0:
; ITER1:        orr     w0, wzr, #0x1
; ITER1-NEXT:   orr     w1, wzr, #0x3
; ITER1-NEXT:   b       z1

; ITER1-LABEL: OUTLINED_FUNCTION_1:
; ITER1:        orr     w2, wzr, #0x4
; ITER1-NEXT:   mov     w0, w19
; ITER1-NEXT:   b       z2

; ITER2-LABEL: OUTLINED_FUNCTION_2:
; ITER2:        orr     w1, wzr, #0x2
; ITER2-NEXT:   b       OUTLINED_FUNCTION_1

; ITER3: Did not outline on iteration 3 out of 3

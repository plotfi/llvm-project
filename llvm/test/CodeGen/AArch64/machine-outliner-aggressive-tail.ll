; RUN: llc -verify-machineinstrs -enable-machine-outliner -aggressive-tail-call-outlining -mtriple=aarch64-linux-gnu < %s | tee /tmp/log | FileCheck %s

; CHECK: OUTLINED_FUNCTION_AGGRESSIVE_TAIL_CALL_0:
; CHECK:      ldp     x19, x30, [sp, #32]
; CHECK-NEXT: ldp     x21, x20, [sp, #16]
; CHECK-NEXT: ldr     x22, [sp], #48
; CHECK-NEXT: ret

define void @a(i32 %a, i32 %b, i32 %c, i32 %d) {
entry:
  call void @z(i32 %a, i32 %b, i32 %c, i32 %d)
  call void @z(i32 1, i32 2, i32 3, i32 4)
  call void @z(i32 %a, i32 %b, i32 %c, i32 %d)
  ret void
}

define void @b(i32 %a, i32 %b, i32 %c, i32 %d) {
entry:
  call void @z(i32 %a, i32 %b, i32 %c, i32 %d)
  call void @z(i32 4, i32 3, i32 2, i32 1)
  call void @z(i32 %a, i32 %b, i32 %c, i32 %d)
  ret void
}

declare void @z(i32, i32, i32, i32)


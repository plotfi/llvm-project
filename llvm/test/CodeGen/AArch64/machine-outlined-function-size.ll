
; RUN: llc -stats -stats-json -enable-machine-outliner=always < %s 2>&1 | FileCheck %s
; REQUIRES: asserts

; CHECK: "machine-outliner.SizeOutlinedFunctions"
; CHECK: "machine-outliner.SizeTotalFunctions"

target triple = "arm64-unknown-ios12.0.0"

define void @f1() minsize {
entry:
  tail call void @f2(i32 1, i32 2, i32 3, i32 4)
  tail call void @f2(i32 1, i32 2, i32 3, i32 5)
  tail call void @f2(i32 1, i32 2, i32 3, i32 6)
  ret void
}

declare void @f2(i32, i32, i32, i32)

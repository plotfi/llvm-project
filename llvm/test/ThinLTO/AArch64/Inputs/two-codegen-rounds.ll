target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-ios"

define void @a(i32 %a, i32 %b, i32 %c, i32 %d) {
entry:
  call void @z(i32 1, i32 2, i32 3, i32 4)
  ret void
}

define void @b(i32 %a, i32 %b, i32 %c, i32 %d) {
entry:
  call void @z(i32 1, i32 2, i32 3, i32 4)
  ret void
}

define void @c(i32 %a, i32 %b, i32 %c, i32 %d) {
entry:
  call void @z(i32 1, i32 2, i32 3, i32 4)
  ret void
}

declare void @z(i32, i32, i32, i32)


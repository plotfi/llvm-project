
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-ios"

define void @a() noinline {
entry:
  tail call void @z(i32 1, i32 2, i32 3, i32 4)
  ret void
}

declare void @z(i32, i32, i32, i32)

define dso_local void @b(i32* nocapture readnone %p) noinline {
entry:
  tail call void @z(i32 1, i32 2, i32 3, i32 4)
  ret void
}

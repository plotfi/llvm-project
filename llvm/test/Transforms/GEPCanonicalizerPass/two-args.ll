; RUN: opt %s -gepcanon -S -verify

target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-ios12.0.0"

%T1 = type <{ [1 x i8] }>
%T2 = type <{ i64 }>
%T3 = type { i64 }

define i8 @f2(i64 %arg) {
entry:
  ret i8 3
}

define void @f1(%T1* noalias nocapture %arg,
                %T2* noalias nocapture dereferenceable(8) %arg1,
                %T3* swiftself %arg2,
                %T3* %Self, i8** %SelfWitnessTable) {
entry:
  %a = getelementptr inbounds %T2, %T2* %arg1, i64 0, i32 0
  %b = load i64, i64* %a, align 8
  %c = tail call swiftcc i8 @f2(i64 %b)
  %d = getelementptr inbounds %T1, %T1* %arg, i64 0, i32 0, i64 0
  store i8 %c, i8* %d, align 1
  ret void
}

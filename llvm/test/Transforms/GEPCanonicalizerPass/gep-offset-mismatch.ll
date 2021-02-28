; RUN: opt %s -gepcanon -S -verify -gep-canon-group-min-size=1 | FileCheck %s

target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-ios12.0.0"

%T1 = type opaque
%T2 = type <{ [1 x i8] }>
%T3 = type <{ %T4 }>
%T4 = type <{ %T5 }>
%T5 = type <{ %T6, %T1* }>
%T6 = type <{ i64 }>
%T7 = type { i64 }


; CHECK: f1_GEPCANONED

declare i8 @f2(i64, %T1*)

define void @f1(%T2* noalias nocapture %arg,
                %T3* noalias nocapture dereferenceable(16) %arg1,
                %T7* swiftself %arg2, %T7* %Self, i8** %SelfWitnessTable) {
entry:
  %a = getelementptr inbounds %T3, %T3* %arg1, i64 0, i32 0, i32 0, i32 0, i32 0
  %b = load i64, i64* %a, align 8
  %c = getelementptr inbounds %T3, %T3* %arg1, i64 0, i32 0, i32 0, i32 1
  %d = load %T1*, %T1** %c, align 8
  %e = tail call swiftcc i8 @f2(i64 %b, %T1* %d) #1
  %f = getelementptr inbounds %T2, %T2* %arg, i64 0, i32 0, i64 0
  store i8 %e, i8* %f, align 1
  ret void
}

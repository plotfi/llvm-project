; RUN: opt %s -gepcanon -S -verify -o - | FileCheck %s

; CHECK: f1_GEPCANONED

target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-ios12.0.0"

%swift.bridge = type opaque
%T14Main7MyClassC = type <{ %swift.refcounted, %TSS, %TSS, %TSSSg,
  %TSSSg, %TSi, %Ts5Int64V, %TSiSg, [7 x i8], %TSayypGSg,
  %T14Main14MyStructAsPropV, %T14Main15MyStruct2AsPropV,
  %T14Main14MyStructAsPropVSg,
  %T14Main20MyStructNestedAsPropVSg, [7 x i8],
  %T14Main13MyClassAsPropCSg, %T14Main14MyClassPAsPropCSg,
  %T14Main14MyClass2AsPropCSg,
  %T14Main16MyProtocolAsProp_pSg,
  %T14Main17MyProtocol2AsProp_pSg, %TSb,
  %T14Main15MyEnumIntAsPropO,
  %T14Main17MyEnumInt64AsPropOSg,
  %T14Main18MyEnumStringAsPropO,
  %T14Main17MyEnumErrorAsPropO, [3 x i8], %swift.function,
  %swift.function, %TS2iIegyd_Sg,
  %TSS14Main13MyClassAsPropCIeggo_Sg, %TSS, %TSSSg, %TSS, %TSS }>
%swift.refcounted = type { %swift.type*, i64 }
%swift.type = type { i64 }
%TSi = type <{ i64 }>
%Ts5Int64V = type <{ i64 }>
%TSiSg = type <{ [8 x i8], [1 x i8] }>
%TSayypGSg = type <{ [8 x i8] }>
%T14Main14MyStructAsPropV = type <{ %TSi, %TSS, %TSSSg }>
%T14Main15MyStruct2AsPropV = type <{ %TSi, %TSi }>
%T14Main14MyStructAsPropVSg = type <{ [40 x i8] }>
%T14Main20MyStructNestedAsPropVSg = type <{ [56 x i8], [1 x i8] }>
%T14Main13MyClassAsPropCSg = type <{ [8 x i8] }>
%T14Main14MyClassPAsPropCSg = type <{ [8 x i8] }>
%T14Main14MyClass2AsPropCSg = type <{ [8 x i8] }>
%T14Main16MyProtocolAsProp_pSg = type <{ [40 x i8] }>
%T14Main17MyProtocol2AsProp_pSg = type <{ [40 x i8] }>
%TSb = type <{ i1 }>
%T14Main15MyEnumIntAsPropO = type <{ i8 }>
%T14Main17MyEnumInt64AsPropOSg = type <{ [1 x i8] }>
%T14Main18MyEnumStringAsPropO = type <{ i8 }>
%T14Main17MyEnumErrorAsPropO = type <{ i8 }>
%swift.function = type { i8*, %swift.refcounted* }
%TS2iIegyd_Sg = type <{ [16 x i8] }>
%TSS14Main13MyClassAsPropCIeggo_Sg = type <{ [16 x i8] }>
%TSSSg = type <{ [16 x i8] }>
%TSS = type <{ %Ts11_StringGutsV }>
%Ts11_StringGutsV = type <{ %Ts13_StringObjectV }>
%Ts13_StringObjectV = type <{ %Ts6UInt64V, %swift.bridge* }>
%Ts6UInt64V = type <{ i64 }>

declare void @swift_beginAccess(i8*, [24 x i8]*, i64, i8*)
declare %swift.bridge* @swift_bridgeObjectRetain(%swift.bridge* returned)

define { i64, %swift.bridge* } @f1(%T14Main7MyClassC* swiftself %arg) {
entry:
  %a  = alloca [24 x i8], align 8
  %i  = getelementptr inbounds %T14Main7MyClassC, %T14Main7MyClassC* %arg, i64 0, i32 1
  %i1 = getelementptr inbounds [24 x i8], [24 x i8]* %a, i64 0, i64 0
  %i2 = bitcast %TSS* %i to i8*
  call void @swift_beginAccess(i8* nonnull %i2, [24 x i8]* nonnull %a, i64 0, i8* null) #2
  %b  = getelementptr inbounds %TSS, %TSS* %i, i64 0, i32 0, i32 0, i32 0, i32 0
  %i3 = load i64, i64* %b, align 8
  %c  = getelementptr inbounds %T14Main7MyClassC, %T14Main7MyClassC* %arg, i64 0, i32 1, i32 0, i32 0, i32 1
  %i4 = load %swift.bridge*, %swift.bridge** %c, align 8
  %i5 = insertvalue { i64, %swift.bridge* } undef, i64 %i3, 0
  %i6 = insertvalue { i64, %swift.bridge* } %i5, %swift.bridge* %i4, 1
  %i7 = call %swift.bridge* @swift_bridgeObjectRetain(%swift.bridge* returned %i4) #2
  ret { i64, %swift.bridge* } %i6
}


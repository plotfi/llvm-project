; RUN: opt %s -gepcanon -S | llc -o - - --asm-verbose=0 | FileCheck %s


; CHECK: _A:
; CHECK-NEXT:   add  x0, x20, #16
; CHECK-NEXT:   b  l_A_GEPCANONED

; CHECK: _B:
; CHECK-NEXT:   add  x0, x20, #64
; CHECK-NEXT:   b  l_B_GEPCANONED

; CHECK: _C:
; CHECK-NEXT:   add  x0, x20, #112
; CHECK-NEXT:   b  l_C_GEPCANONED

; CHECK: l_A_GEPCANONED:
; CHECK-NEXT:   b  _OUTLINED_FUNCTION_

; CHECK: l_B_GEPCANONED:
; CHECK-NEXT:   b  _OUTLINED_FUNCTION_

; CHECK: l_C_GEPCANONED:
; CHECK-NEXT:   b  _OUTLINED_FUNCTION_

target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "aarch64--ios"

%swift.bridge = type opaque
%TSS = type <{  <{ <{ i64, %swift.bridge* }> }> }>

%T1 = type <{ [8 x i8] }>
%T2 = type <{ [8 x i8] }>
%T3 = type <{ [8 x i8] }>
%T4 = type <{ [8 x i8] }>
%T5 = type <{ %T1, %T2, %T3, %T4, %TSS }>
%T6 = type <{ %TSS, %T5, %T5, %T5 }>

define swiftcc void @A(%T5* noalias nocapture sret %arg, %T6* noalias nocapture readonly swiftself dereferenceable(112) %arg1) #0 {
entry:
  %.title = getelementptr inbounds %T6, %T6* %arg1, i64 0, i32 1
  %i = bitcast %T5* %.title to i64*
  %i2 = load i64, i64* %i, align 8
  %.title.inlineStyleRanges = getelementptr inbounds %T6, %T6* %arg1, i64 0, i32 1, i32 1
  %i3 = bitcast %T2* %.title.inlineStyleRanges to i64*
  %i4 = load i64, i64* %i3, align 8
  %.title.linkRanges = getelementptr inbounds %T6, %T6* %arg1, i64 0, i32 1, i32 2
  %i5 = bitcast %T3* %.title.linkRanges to i64*
  %i6 = load i64, i64* %i5, align 8
  %.title.scaleSizeRanges = getelementptr inbounds %T6, %T6* %arg1, i64 0, i32 1, i32 3
  %i7 = bitcast %T4* %.title.scaleSizeRanges to <2 x i64>*
  %i8 = load <2 x i64>, <2 x i64>* %i7, align 8
  %.title.text._guts._object._object = getelementptr inbounds %T6, %T6* %arg1, i64 0, i32 1, i32 4, i32 0, i32 0, i32 1
  %i9 = load %swift.bridge*, %swift.bridge** %.title.text._guts._object._object, align 8
  %i10 = inttoptr i64 %i2 to %swift.bridge*
  %i11 = inttoptr i64 %i4 to %swift.bridge*
  %i12 = inttoptr i64 %i6 to %swift.bridge*
  %i13 = extractelement <2 x i64> %i8, i32 0
  %i14 = inttoptr i64 %i13 to %swift.bridge*
  %i15 = bitcast %T5* %arg to i64*
  store i64 %i2, i64* %i15, align 8
  %.inlineStyleRanges = getelementptr inbounds %T5, %T5* %arg, i64 0, i32 1
  %i16 = bitcast %T2* %.inlineStyleRanges to i64*
  store i64 %i4, i64* %i16, align 8
  %.linkRanges = getelementptr inbounds %T5, %T5* %arg, i64 0, i32 2
  %i17 = bitcast %T3* %.linkRanges to i64*
  store i64 %i6, i64* %i17, align 8
  %.scaleSizeRanges = getelementptr inbounds %T5, %T5* %arg, i64 0, i32 3
  %i18 = bitcast %T4* %.scaleSizeRanges to <2 x i64>*
  store <2 x i64> %i8, <2 x i64>* %i18, align 8
  %.text._guts._object._object = getelementptr inbounds %T5, %T5* %arg, i64 0, i32 4, i32 0, i32 0, i32 1
  store %swift.bridge* %i9, %swift.bridge** %.text._guts._object._object, align 8
  ret void
}

define swiftcc void @B(%T5* noalias nocapture sret %arg, %T6* noalias nocapture readonly swiftself dereferenceable(112) %arg1) #0 {
entry:
  %.content = getelementptr inbounds %T6, %T6* %arg1, i64 0, i32 2
  %i = bitcast %T5* %.content to i64*
  %i2 = load i64, i64* %i, align 8
  %.content.inlineStyleRanges = getelementptr inbounds %T6, %T6* %arg1, i64 0, i32 2, i32 1
  %i3 = bitcast %T2* %.content.inlineStyleRanges to i64*
  %i4 = load i64, i64* %i3, align 8
  %.content.linkRanges = getelementptr inbounds %T6, %T6* %arg1, i64 0, i32 2, i32 2
  %i5 = bitcast %T3* %.content.linkRanges to i64*
  %i6 = load i64, i64* %i5, align 8
  %.content.scaleSizeRanges = getelementptr inbounds %T6, %T6* %arg1, i64 0, i32 2, i32 3
  %i7 = bitcast %T4* %.content.scaleSizeRanges to <2 x i64>*
  %i8 = load <2 x i64>, <2 x i64>* %i7, align 8
  %.content.text._guts._object._object = getelementptr inbounds %T6, %T6* %arg1, i64 0, i32 2, i32 4, i32 0, i32 0, i32 1
  %i9 = load %swift.bridge*, %swift.bridge** %.content.text._guts._object._object, align 8
  %i10 = inttoptr i64 %i2 to %swift.bridge*
  %i11 = inttoptr i64 %i4 to %swift.bridge*
  %i12 = inttoptr i64 %i6 to %swift.bridge*
  %i13 = extractelement <2 x i64> %i8, i32 0
  %i14 = inttoptr i64 %i13 to %swift.bridge*
  %i15 = bitcast %T5* %arg to i64*
  store i64 %i2, i64* %i15, align 8
  %.inlineStyleRanges = getelementptr inbounds %T5, %T5* %arg, i64 0, i32 1
  %i16 = bitcast %T2* %.inlineStyleRanges to i64*
  store i64 %i4, i64* %i16, align 8
  %.linkRanges = getelementptr inbounds %T5, %T5* %arg, i64 0, i32 2
  %i17 = bitcast %T3* %.linkRanges to i64*
  store i64 %i6, i64* %i17, align 8
  %.scaleSizeRanges = getelementptr inbounds %T5, %T5* %arg, i64 0, i32 3
  %i18 = bitcast %T4* %.scaleSizeRanges to <2 x i64>*
  store <2 x i64> %i8, <2 x i64>* %i18, align 8
  %.text._guts._object._object = getelementptr inbounds %T5, %T5* %arg, i64 0, i32 4, i32 0, i32 0, i32 1
  store %swift.bridge* %i9, %swift.bridge** %.text._guts._object._object, align 8
  ret void
}

define swiftcc void @C(%T5* noalias nocapture sret %arg, %T6* noalias nocapture readonly swiftself dereferenceable(112) %arg1) #0 {
entry:
  %.body = getelementptr inbounds %T6, %T6* %arg1, i64 0, i32 3
  %i = bitcast %T5* %.body to i64*
  %i2 = load i64, i64* %i, align 8
  %.body.inlineStyleRanges = getelementptr inbounds %T6, %T6* %arg1, i64 0, i32 3, i32 1
  %i3 = bitcast %T2* %.body.inlineStyleRanges to i64*
  %i4 = load i64, i64* %i3, align 8
  %.body.linkRanges = getelementptr inbounds %T6, %T6* %arg1, i64 0, i32 3, i32 2
  %i5 = bitcast %T3* %.body.linkRanges to i64*
  %i6 = load i64, i64* %i5, align 8
  %.body.scaleSizeRanges = getelementptr inbounds %T6, %T6* %arg1, i64 0, i32 3, i32 3
  %i7 = bitcast %T4* %.body.scaleSizeRanges to <2 x i64>*
  %i8 = load <2 x i64>, <2 x i64>* %i7, align 8
  %.body.text._guts._object._object = getelementptr inbounds %T6, %T6* %arg1, i64 0, i32 3, i32 4, i32 0, i32 0, i32 1
  %i9 = load %swift.bridge*, %swift.bridge** %.body.text._guts._object._object, align 8
  %i10 = inttoptr i64 %i2 to %swift.bridge*
  %i11 = inttoptr i64 %i4 to %swift.bridge*
  %i12 = inttoptr i64 %i6 to %swift.bridge*
  %i13 = extractelement <2 x i64> %i8, i32 0
  %i14 = inttoptr i64 %i13 to %swift.bridge*
  %i15 = bitcast %T5* %arg to i64*
  store i64 %i2, i64* %i15, align 8
  %.inlineStyleRanges = getelementptr inbounds %T5, %T5* %arg, i64 0, i32 1
  %i16 = bitcast %T2* %.inlineStyleRanges to i64*
  store i64 %i4, i64* %i16, align 8
  %.linkRanges = getelementptr inbounds %T5, %T5* %arg, i64 0, i32 2
  %i17 = bitcast %T3* %.linkRanges to i64*
  store i64 %i6, i64* %i17, align 8
  %.scaleSizeRanges = getelementptr inbounds %T5, %T5* %arg, i64 0, i32 3
  %i18 = bitcast %T4* %.scaleSizeRanges to <2 x i64>*
  store <2 x i64> %i8, <2 x i64>* %i18, align 8
  %.text._guts._object._object = getelementptr inbounds %T5, %T5* %arg, i64 0, i32 4, i32 0, i32 0, i32 1
  store %swift.bridge* %i9, %swift.bridge** %.text._guts._object._object, align 8
  ret void
}

attributes #0 = { minsize nounwind optsize }

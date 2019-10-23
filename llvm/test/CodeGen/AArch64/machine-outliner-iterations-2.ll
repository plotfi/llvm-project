; RUN: llc -verify-machineinstrs -enable-machine-outliner -outlining-iterations=1 -mtriple=aarch64-linux-gnu < %s | FileCheck %s -check-prefix=ITER1
; RUN: llc -verify-machineinstrs -enable-machine-outliner -outlining-iterations=2 -mtriple=aarch64-linux-gnu < %s | FileCheck %s -check-prefix=ITER2

%"struct.NArchive::NZip::CExtraSubBlock" = type { i16, %class.CBuffer }
%class.CBuffer = type { i32 (...)**, i64, i8* }
%struct._FILETIME = type { i32, i32 }
%"class.NArchive::NZip::CLocalItem" = type { %"struct.NArchive::NZip::CVersion", i16, i16, i32, i32, i64, i64, %class.CStringBase, %"struct.NArchive::NZip::CExtraBlock" }
%"struct.NArchive::NZip::CVersion" = type { i8, i8 }
%class.CStringBase = type { i8*, i32, i32 }
%"struct.NArchive::NZip::CExtraBlock" = type { %class.CObjectVector }
%class.CObjectVector = type { %class.CRecordVector }
%class.CRecordVector = type { %class.CBaseRecordVector }
%class.CBaseRecordVector = type { i32 (...)**, i32, i32, i8*, i64 }
%"class.NArchive::NZip::CItem" = type <{ %"class.NArchive::NZip::CLocalItem", %"struct.NArchive::NZip::CVersion", i16, i32, i64, %struct._FILETIME, %struct._FILETIME, %struct._FILETIME, %"struct.NArchive::NZip::CExtraBlock", %class.CBuffer, i8, i8, i8, [5 x i8] }>

; Function Attrs: minsize norecurse nounwind optsize ssp uwtable
define zeroext i1 @_ZNK8NArchive4NZip14CExtraSubBlock15ExtractNtfsTimeEiR9_FILETIME(%"struct.NArchive::NZip::CExtraSubBlock"* nocapture readonly %this, i32 %index, %struct._FILETIME* nocapture dereferenceable(8) %ft) local_unnamed_addr #0 align 2 {
entry:
  %dwLowDateTime = getelementptr inbounds %struct._FILETIME, %struct._FILETIME* %ft, i64 0, i32 0
  %dwHighDateTime = getelementptr inbounds %struct._FILETIME, %struct._FILETIME* %ft, i64 0, i32 1
  %0 = bitcast %struct._FILETIME* %ft to <2 x i32>*
  store <2 x i32> zeroinitializer, <2 x i32>* %0, align 4, !tbaa !3
  %_capacity.i = getelementptr inbounds %"struct.NArchive::NZip::CExtraSubBlock", %"struct.NArchive::NZip::CExtraSubBlock"* %this, i64 0, i32 1, i32 1
  %1 = load i64, i64* %_capacity.i, align 8, !tbaa !7
  %conv = trunc i64 %1 to i32
  %ID = getelementptr inbounds %"struct.NArchive::NZip::CExtraSubBlock", %"struct.NArchive::NZip::CExtraSubBlock"* %this, i64 0, i32 0
  %2 = load i16, i16* %ID, align 8, !tbaa !11
  %cmp = icmp ne i16 %2, 10
  %cmp3 = icmp ult i32 %conv, 32
  %or.cond = or i1 %cmp, %cmp3
  br i1 %or.cond, label %cleanup71, label %if.end

if.end:                                           ; preds = %entry
  %_items.i = getelementptr inbounds %"struct.NArchive::NZip::CExtraSubBlock", %"struct.NArchive::NZip::CExtraSubBlock"* %this, i64 0, i32 1, i32 2
  %3 = load i8*, i8** %_items.i, align 8, !tbaa !14
  %add.ptr = getelementptr inbounds i8, i8* %3, i64 4
  %sub = add i32 %conv, -4
  br label %while.cond

while.cond:                                       ; preds = %cleanup, %if.end
  %size.0 = phi i32 [ %sub, %if.end ], [ %sub68, %cleanup ]
  %p.0 = phi i8* [ %add.ptr, %if.end ], [ %add.ptr67, %cleanup ]
  %cmp6 = icmp ugt i32 %size.0, 4
  br i1 %cmp6, label %while.body, label %cleanup71

while.body:                                       ; preds = %while.cond
  %4 = load i8, i8* %p.0, align 1, !tbaa !15
  %conv7 = zext i8 %4 to i32
  %arrayidx8 = getelementptr inbounds i8, i8* %p.0, i64 1
  %5 = load i8, i8* %arrayidx8, align 1, !tbaa !15
  %conv10 = zext i8 %5 to i32
  %shl = shl nuw nsw i32 %conv10, 8
  %or = or i32 %shl, %conv7
  %add.ptr12 = getelementptr inbounds i8, i8* %p.0, i64 2
  %6 = load i8, i8* %add.ptr12, align 1, !tbaa !15
  %conv14 = zext i8 %6 to i32
  %arrayidx16 = getelementptr inbounds i8, i8* %p.0, i64 3
  %7 = load i8, i8* %arrayidx16, align 1, !tbaa !15
  %conv18 = zext i8 %7 to i32
  %shl19 = shl nuw nsw i32 %conv18, 8
  %or20 = or i32 %shl19, %conv14
  %add.ptr21 = getelementptr inbounds i8, i8* %p.0, i64 4
  %sub22 = add i32 %size.0, -4
  %cmp23 = icmp ugt i32 %or20, %sub22
  %spec.select = select i1 %cmp23, i32 %sub22, i32 %or20
  %cmp27 = icmp eq i32 %or, 1
  %cmp28 = icmp ugt i32 %spec.select, 23
  %or.cond72 = and i1 %cmp27, %cmp28
  br i1 %or.cond72, label %cleanup.thread, label %cleanup

cleanup.thread:                                   ; preds = %while.body
  %mul = shl nsw i32 %index, 3
  %idx.ext = sext i32 %mul to i64
  %add.ptr30 = getelementptr inbounds i8, i8* %add.ptr21, i64 %idx.ext
  %8 = load i8, i8* %add.ptr30, align 1, !tbaa !15
  %conv32 = zext i8 %8 to i32
  %arrayidx33 = getelementptr inbounds i8, i8* %add.ptr30, i64 1
  %9 = load i8, i8* %arrayidx33, align 1, !tbaa !15
  %conv34 = zext i8 %9 to i32
  %shl35 = shl nuw nsw i32 %conv34, 8
  %or36 = or i32 %shl35, %conv32
  %arrayidx37 = getelementptr inbounds i8, i8* %add.ptr30, i64 2
  %10 = load i8, i8* %arrayidx37, align 1, !tbaa !15
  %conv38 = zext i8 %10 to i32
  %shl39 = shl nuw nsw i32 %conv38, 16
  %or40 = or i32 %or36, %shl39
  %arrayidx41 = getelementptr inbounds i8, i8* %add.ptr30, i64 3
  %11 = load i8, i8* %arrayidx41, align 1, !tbaa !15
  %conv42 = zext i8 %11 to i32
  %shl43 = shl nuw i32 %conv42, 24
  %or44 = or i32 %or40, %shl43
  store i32 %or44, i32* %dwLowDateTime, align 4, !tbaa !16
  %add.ptr46 = getelementptr inbounds i8, i8* %add.ptr30, i64 4
  %12 = load i8, i8* %add.ptr46, align 1, !tbaa !15
  %conv48 = zext i8 %12 to i32
  %arrayidx50 = getelementptr inbounds i8, i8* %add.ptr46, i64 1
  %13 = load i8, i8* %arrayidx50, align 1, !tbaa !15
  %conv51 = zext i8 %13 to i32
  %shl52 = shl nuw nsw i32 %conv51, 8
  %or53 = or i32 %shl52, %conv48
  %arrayidx55 = getelementptr inbounds i8, i8* %add.ptr46, i64 2
  %14 = load i8, i8* %arrayidx55, align 1, !tbaa !15
  %conv56 = zext i8 %14 to i32
  %shl57 = shl nuw nsw i32 %conv56, 16
  %or58 = or i32 %or53, %shl57
  %arrayidx60 = getelementptr inbounds i8, i8* %add.ptr46, i64 3
  %15 = load i8, i8* %arrayidx60, align 1, !tbaa !15
  %conv61 = zext i8 %15 to i32
  %shl62 = shl nuw i32 %conv61, 24
  %or63 = or i32 %or58, %shl62
  store i32 %or63, i32* %dwHighDateTime, align 4, !tbaa !18
  br label %cleanup71

cleanup:                                          ; preds = %while.body
  %idx.ext66 = zext i32 %spec.select to i64
  %add.ptr67 = getelementptr inbounds i8, i8* %add.ptr21, i64 %idx.ext66
  %sub68 = sub i32 %sub22, %spec.select
  br label %while.cond

cleanup71:                                        ; preds = %while.cond, %cleanup.thread, %entry
  %retval.3 = phi i1 [ false, %entry ], [ true, %cleanup.thread ], [ false, %while.cond ]
  ret i1 %retval.3
}

; Function Attrs: minsize norecurse nounwind optsize ssp uwtable
define zeroext i1 @_ZNK8NArchive4NZip14CExtraSubBlock15ExtractUnixTimeEiRj(%"struct.NArchive::NZip::CExtraSubBlock"* nocapture readonly %this, i32 %index, i32* nocapture dereferenceable(4) %res) local_unnamed_addr #0 align 2 {
entry:
  store i32 0, i32* %res, align 4, !tbaa !3
  %_capacity.i = getelementptr inbounds %"struct.NArchive::NZip::CExtraSubBlock", %"struct.NArchive::NZip::CExtraSubBlock"* %this, i64 0, i32 1, i32 1
  %0 = load i64, i64* %_capacity.i, align 8, !tbaa !7
  %conv = trunc i64 %0 to i32
  %ID = getelementptr inbounds %"struct.NArchive::NZip::CExtraSubBlock", %"struct.NArchive::NZip::CExtraSubBlock"* %this, i64 0, i32 0
  %1 = load i16, i16* %ID, align 8, !tbaa !11
  %cmp = icmp ne i16 %1, 21589
  %cmp3 = icmp ult i32 %conv, 5
  %or.cond = or i1 %cmp, %cmp3
  br i1 %or.cond, label %cleanup31, label %if.end

if.end:                                           ; preds = %entry
  %_items.i = getelementptr inbounds %"struct.NArchive::NZip::CExtraSubBlock", %"struct.NArchive::NZip::CExtraSubBlock"* %this, i64 0, i32 1, i32 2
  %2 = load i8*, i8** %_items.i, align 8, !tbaa !14
  %incdec.ptr = getelementptr inbounds i8, i8* %2, i64 1
  %3 = load i8, i8* %2, align 1, !tbaa !15
  %dec = add i32 %conv, -1
  %conv7 = zext i8 %3 to i32
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %if.end
  %size.0 = phi i32 [ %dec, %if.end ], [ %size.1, %for.inc ]
  %p.0 = phi i8* [ %incdec.ptr, %if.end ], [ %p.1, %for.inc ]
  %i.0 = phi i32 [ 0, %if.end ], [ %inc, %for.inc ]
  %cmp6 = icmp ult i32 %i.0, 3
  br i1 %cmp6, label %for.body, label %cleanup31

for.body:                                         ; preds = %for.cond
  %shl = shl i32 1, %i.0
  %and = and i32 %shl, %conv7
  %cmp8 = icmp eq i32 %and, 0
  br i1 %cmp8, label %for.inc, label %if.then9

if.then9:                                         ; preds = %for.body
  %cmp10 = icmp ult i32 %size.0, 4
  br i1 %cmp10, label %cleanup31, label %if.end12

if.end12:                                         ; preds = %if.then9
  %cmp13 = icmp eq i32 %i.0, %index
  br i1 %cmp13, label %if.then14, label %if.end27

if.then14:                                        ; preds = %if.end12
  %4 = load i8, i8* %p.0, align 1, !tbaa !15
  %conv15 = zext i8 %4 to i32
  %arrayidx16 = getelementptr inbounds i8, i8* %p.0, i64 1
  %5 = load i8, i8* %arrayidx16, align 1, !tbaa !15
  %conv17 = zext i8 %5 to i32
  %shl18 = shl nuw nsw i32 %conv17, 8
  %or = or i32 %shl18, %conv15
  %arrayidx19 = getelementptr inbounds i8, i8* %p.0, i64 2
  %6 = load i8, i8* %arrayidx19, align 1, !tbaa !15
  %conv20 = zext i8 %6 to i32
  %shl21 = shl nuw nsw i32 %conv20, 16
  %or22 = or i32 %or, %shl21
  %arrayidx23 = getelementptr inbounds i8, i8* %p.0, i64 3
  %7 = load i8, i8* %arrayidx23, align 1, !tbaa !15
  %conv24 = zext i8 %7 to i32
  %shl25 = shl nuw i32 %conv24, 24
  %or26 = or i32 %or22, %shl25
  store i32 %or26, i32* %res, align 4, !tbaa !3
  br label %cleanup31

if.end27:                                         ; preds = %if.end12
  %add.ptr = getelementptr inbounds i8, i8* %p.0, i64 4
  %sub = add i32 %size.0, -4
  br label %for.inc

for.inc:                                          ; preds = %for.body, %if.end27
  %size.1 = phi i32 [ %sub, %if.end27 ], [ %size.0, %for.body ]
  %p.1 = phi i8* [ %add.ptr, %if.end27 ], [ %p.0, %for.body ]
  %inc = add nuw nsw i32 %i.0, 1
  br label %for.cond

cleanup31:                                        ; preds = %if.then9, %for.cond, %if.then14, %entry
  %retval.2 = phi i1 [ false, %entry ], [ true, %if.then14 ], [ false, %for.cond ], [ false, %if.then9 ]
  ret i1 %retval.2
}

; Function Attrs: minsize optsize ssp uwtable
define zeroext i1 @_ZNK8NArchive4NZip10CLocalItem5IsDirEv(%"class.NArchive::NZip::CLocalItem"* %this) local_unnamed_addr #1 align 2 {
entry:
  %Name = getelementptr inbounds %"class.NArchive::NZip::CLocalItem", %"class.NArchive::NZip::CLocalItem"* %this, i64 0, i32 7
  %call2 = tail call zeroext i1 @_ZN8NArchive9NItemName12HasTailSlashERK11CStringBaseIcEj(%class.CStringBase* nonnull dereferenceable(16) %Name, i32 1) #3
  ret i1 %call2
}

; Function Attrs: minsize optsize
declare zeroext i1 @_ZN8NArchive9NItemName12HasTailSlashERK11CStringBaseIcEj(%class.CStringBase* dereferenceable(16), i32) local_unnamed_addr #2

; Function Attrs: minsize optsize ssp uwtable
define i32 @_ZNK8NArchive4NZip5CItem16GetWinAttributesEv(%"class.NArchive::NZip::CItem"* %this) local_unnamed_addr #1 align 2 {
entry:
  %HostOS = getelementptr inbounds %"class.NArchive::NZip::CItem", %"class.NArchive::NZip::CItem"* %this, i64 0, i32 1, i32 1
  %0 = load i8, i8* %HostOS, align 1, !tbaa !19
  switch i8 %0, label %sw.epilog [
    i8 0, label %sw.bb
    i8 11, label %sw.bb
    i8 3, label %sw.bb2
  ]

sw.bb:                                            ; preds = %entry, %entry
  %FromCentral = getelementptr inbounds %"class.NArchive::NZip::CItem", %"class.NArchive::NZip::CItem"* %this, i64 0, i32 11
  %1 = load i8, i8* %FromCentral, align 1, !tbaa !26, !range !27
  %tobool = icmp eq i8 %1, 0
  br i1 %tobool, label %sw.epilog, label %if.then

if.then:                                          ; preds = %sw.bb
  %ExternalAttributes = getelementptr inbounds %"class.NArchive::NZip::CItem", %"class.NArchive::NZip::CItem"* %this, i64 0, i32 3
  %2 = load i32, i32* %ExternalAttributes, align 4, !tbaa !28
  br label %sw.epilog

sw.bb2:                                           ; preds = %entry
  %ExternalAttributes3 = getelementptr inbounds %"class.NArchive::NZip::CItem", %"class.NArchive::NZip::CItem"* %this, i64 0, i32 3
  %3 = load i32, i32* %ExternalAttributes3, align 4, !tbaa !28
  %and = and i32 %3, -65536
  %and4 = and i32 %3, 1073741824
  %tobool5 = icmp eq i32 %and4, 0
  %spec.select.v = select i1 %tobool5, i32 32768, i32 32784
  %spec.select = or i32 %spec.select.v, %and
  br label %cleanup

sw.epilog:                                        ; preds = %entry, %sw.bb, %if.then
  %winAttributes.1 = phi i32 [ %2, %if.then ], [ 0, %sw.bb ], [ 0, %entry ]
  %call = tail call zeroext i1 @_ZNK8NArchive4NZip5CItem5IsDirEv(%"class.NArchive::NZip::CItem"* nonnull %this) #3
  %or10 = or i32 %winAttributes.1, 16
  %spec.select17 = select i1 %call, i32 %or10, i32 %winAttributes.1
  br label %cleanup

cleanup:                                          ; preds = %sw.epilog, %sw.bb2
  %retval.0 = phi i32 [ %spec.select17, %sw.epilog ], [ %spec.select, %sw.bb2 ]
  ret i32 %retval.0
}

; Function Attrs: minsize optsize
declare zeroext i1 @_ZNK8NArchive4NZip5CItem5IsDirEv(%"class.NArchive::NZip::CItem"*) local_unnamed_addr #2

; Function Attrs: minsize norecurse nounwind optsize ssp uwtable
define void @_ZN8NArchive4NZip10CLocalItem11SetFlagBitsEiii(%"class.NArchive::NZip::CLocalItem"* nocapture %this, i32 %startBitNumber, i32 %numBits, i32 %value) local_unnamed_addr #0 align 2 {
entry:
  %notmask = shl nsw i32 -1, %numBits
  %sub = xor i32 %notmask, -1
  %shl2 = shl i32 %sub, %startBitNumber
  %Flags = getelementptr inbounds %"class.NArchive::NZip::CLocalItem", %"class.NArchive::NZip::CLocalItem"* %this, i64 0, i32 1
  %0 = load i16, i16* %Flags, align 2, !tbaa !29
  %1 = trunc i32 %shl2 to i16
  %2 = xor i16 %1, -1
  %conv5 = and i16 %0, %2
  %shl6 = shl i32 %value, %startBitNumber
  %3 = trunc i32 %shl6 to i16
  %conv9 = or i16 %conv5, %3
  store i16 %conv9, i16* %Flags, align 2, !tbaa !29
  ret void
}

; Function Attrs: minsize norecurse nounwind optsize ssp uwtable
define void @_ZN8NArchive4NZip10CLocalItem10SetBitMaskEib(%"class.NArchive::NZip::CLocalItem"* nocapture %this, i32 %bitMask, i1 zeroext %enable) local_unnamed_addr #0 align 2 {
entry:
  %Flags = getelementptr inbounds %"class.NArchive::NZip::CLocalItem", %"class.NArchive::NZip::CLocalItem"* %this, i64 0, i32 1
  %0 = load i16, i16* %Flags, align 2, !tbaa !29
  %1 = trunc i32 %bitMask to i16
  %2 = xor i16 %1, -1
  %conv5 = and i16 %0, %2
  %conv2 = or i16 %0, %1
  %storemerge = select i1 %enable, i16 %conv2, i16 %conv5
  store i16 %storemerge, i16* %Flags, align 2, !tbaa !29
  ret void
}

; ITER1-LABEL: _ZN8NArchive4NZip10CLocalItem12SetEncryptedEb:
; ITER1:       orr w8, w8, #0x1
; ITER1-NEXT:  cmp w1, #0
; ITER1-NEXT:  csel w8, w8, w9, ne
; ITER1-NEXT:  b OUTLINED_FUNCTION_0

; ITER2-LABEL: _ZN8NArchive4NZip10CLocalItem12SetEncryptedEb:
; ITER2:       orr w8, w8, #0x1
; ITER2-NEXT:  b OUTLINED_FUNCTION_1

; Function Attrs: minsize norecurse nounwind optsize ssp uwtable
define void @_ZN8NArchive4NZip10CLocalItem12SetEncryptedEb(%"class.NArchive::NZip::CLocalItem"* nocapture %this, i1 zeroext %encrypted) local_unnamed_addr #0 align 2 {
entry:
  %Flags.i = getelementptr inbounds %"class.NArchive::NZip::CLocalItem", %"class.NArchive::NZip::CLocalItem"* %this, i64 0, i32 1
  %0 = load i16, i16* %Flags.i, align 2, !tbaa !29
  %conv5.i = and i16 %0, -2
  %conv2.i = or i16 %0, 1
  %storemerge.i = select i1 %encrypted, i16 %conv2.i, i16 %conv5.i
  store i16 %storemerge.i, i16* %Flags.i, align 2, !tbaa !29
  ret void
}

; ITER1-LABEL: _ZN8NArchive4NZip10CLocalItem7SetUtf8Eb
; ITER1:       orr w8, w8, #0x800
; ITER1-NEXT:  cmp w1, #0
; ITER1-NEXT:  csel w8, w8, w9, ne
; ITER1-NEXT:  b OUTLINED_FUNCTION_0

; ITER2-LABEL: _ZN8NArchive4NZip10CLocalItem7SetUtf8Eb
; ITER2:       orr w8, w8, #0x800
; ITER2-NEXT:  b OUTLINED_FUNCTION_1

; Function Attrs: minsize norecurse nounwind optsize ssp uwtable
define void @_ZN8NArchive4NZip10CLocalItem7SetUtf8Eb(%"class.NArchive::NZip::CLocalItem"* nocapture %this, i1 zeroext %isUtf8) local_unnamed_addr #0 align 2 {
entry:
  %Flags.i = getelementptr inbounds %"class.NArchive::NZip::CLocalItem", %"class.NArchive::NZip::CLocalItem"* %this, i64 0, i32 1
  %0 = load i16, i16* %Flags.i, align 2, !tbaa !29
  %conv5.i = and i16 %0, -2049
  %conv2.i = or i16 %0, 2048
  %storemerge.i = select i1 %isUtf8, i16 %conv2.i, i16 %conv5.i
  store i16 %storemerge.i, i16* %Flags.i, align 2, !tbaa !29
  ret void
}

attributes #0 = { minsize norecurse nounwind optsize ssp uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="cyclone" "target-features"="+aes,+crypto,+fp-armv8,+neon,+sha2,+zcm,+zcz,+zcz-fp-workaround" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { minsize optsize ssp uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="cyclone" "target-features"="+aes,+crypto,+fp-armv8,+neon,+sha2,+zcm,+zcz,+zcz-fp-workaround" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { minsize optsize "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="cyclone" "target-features"="+aes,+crypto,+fp-armv8,+neon,+sha2,+zcm,+zcz,+zcz-fp-workaround" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { minsize optsize }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{!"Facebook clang version 8.0.1 (llvm: 24ad29e5c02c445552bd9739bd0e416bab3ffdb5, cfe: 102c6cffc493305b9d3bf545f595829d29a55ccc, compiler-rt: abdc44a4172e767219762128b320a43cdf55b8bd, lld: 99a3895c4cbf4fc93d188c4d4b65a863db89e17b 5e78c342f952fbbe8a98c845d703ed773e2d5754) (ssh://git.vip.facebook.com/data/gitrepos/osmeta/external/llvm 63f5a476b02475816da9015ec9ee3eb122d7e554) (based on LLVM 8.0.1)"}
!3 = !{!4, !4, i64 0}
!4 = !{!"int", !5, i64 0}
!5 = !{!"omnipotent char", !6, i64 0}
!6 = !{!"Simple C++ TBAA"}
!7 = !{!8, !9, i64 8}
!8 = !{!"_ZTS7CBufferIhE", !9, i64 8, !10, i64 16}
!9 = !{!"long", !5, i64 0}
!10 = !{!"any pointer", !5, i64 0}
!11 = !{!12, !13, i64 0}
!12 = !{!"_ZTSN8NArchive4NZip14CExtraSubBlockE", !13, i64 0, !8, i64 8}
!13 = !{!"short", !5, i64 0}
!14 = !{!8, !10, i64 16}
!15 = !{!5, !5, i64 0}
!16 = !{!17, !4, i64 0}
!17 = !{!"_ZTS9_FILETIME", !4, i64 0, !4, i64 4}
!18 = !{!17, !4, i64 4}
!19 = !{!20, !5, i64 81}
!20 = !{!"_ZTSN8NArchive4NZip5CItemE", !21, i64 80, !13, i64 82, !4, i64 84, !22, i64 88, !17, i64 96, !17, i64 104, !17, i64 112, !23, i64 120, !8, i64 152, !25, i64 176, !25, i64 177, !25, i64 178}
!21 = !{!"_ZTSN8NArchive4NZip8CVersionE", !5, i64 0, !5, i64 1}
!22 = !{!"long long", !5, i64 0}
!23 = !{!"_ZTSN8NArchive4NZip11CExtraBlockE", !24, i64 0}
!24 = !{!"_ZTS13CObjectVectorIN8NArchive4NZip14CExtraSubBlockEE"}
!25 = !{!"bool", !5, i64 0}
!26 = !{!20, !25, i64 177}
!27 = !{i8 0, i8 2}
!28 = !{!20, !4, i64 84}
!29 = !{!30, !13, i64 2}
!30 = !{!"_ZTSN8NArchive4NZip10CLocalItemE", !21, i64 0, !13, i64 2, !13, i64 4, !4, i64 8, !4, i64 12, !22, i64 16, !22, i64 24, !31, i64 32, !23, i64 48}
!31 = !{!"_ZTS11CStringBaseIcE", !10, i64 0, !4, i64 8, !4, i64 12}

; ITER1-LABEL: OUTLINED_FUNCTION_0:
; ITER1:        strh  w8, [x0, #2]
; ITER1-NEXT:   ret

; ITER2-LABEL: OUTLINED_FUNCTION_1:
; ITER2:        cmp     w1, #0
; ITER2-NEXT:   csel    w8, w8, w9, ne
; ITER2-NEXT:   b       OUTLINED_FUNCTION_0

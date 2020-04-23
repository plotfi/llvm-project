; RUN: llc < %s -mtriple=arm64-apple-ios7.0 -objc-call-opt | FileCheck %s

target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"

%struct._class_t = type { %struct._class_t*, %struct._class_t*, %struct._objc_cache*, i8* (i8*, i8*)**, %struct._class_ro_t* }
%struct._objc_cache = type opaque
%struct._class_ro_t = type { i32, i32, i32, i8*, i8*, %struct.__method_list_t*, %struct._objc_protocol_list*, %struct._ivar_list_t*, i8*, %struct._prop_list_t* }
%struct.__method_list_t = type { i32, i32, [0 x %struct._objc_method] }
%struct._objc_method = type { i8*, i8*, i8* }
%struct._objc_protocol_list = type { i64, [0 x %struct._protocol_t*] }
%struct._protocol_t = type { i8*, i8*, %struct._objc_protocol_list*, %struct.__method_list_t*, %struct.__method_list_t*, %struct.__method_list_t*, %struct.__method_list_t*, %struct._prop_list_t*, i32, i32, i8**, i8*, %struct._prop_list_t* }
%struct._ivar_list_t = type { i32, i32, [0 x %struct._ivar_t] }
%struct._ivar_t = type { i32*, i8*, i8*, i32, i32 }
%struct._prop_list_t = type { i32, i32, [0 x %struct._prop_t] }
%struct._prop_t = type { i8*, i8* }

@"OBJC_CLASS_$_HelloWorld" = external global %struct._class_t
@"OBJC_CLASSLIST_REFERENCES_$_" = private global %struct._class_t* @"OBJC_CLASS_$_HelloWorld", section "__DATA,__objc_classrefs,regular,no_dead_strip", align 8
@OBJC_METH_VAR_NAME_ = private unnamed_addr constant [4 x i8] c"new\00", section "__TEXT,__objc_methname,cstring_literals", align 1
@OBJC_SELECTOR_REFERENCES_ = private externally_initialized global i8* getelementptr inbounds ([4 x i8], [4 x i8]* @OBJC_METH_VAR_NAME_, i64 0, i64 0), section "__DATA,__objc_selrefs,literal_pointers,no_dead_strip", align 8
@OBJC_METH_VAR_NAME_.1 = private unnamed_addr constant [6 x i8] c"hello\00", section "__TEXT,__objc_methname,cstring_literals", align 1
@OBJC_SELECTOR_REFERENCES_.2 = private externally_initialized global i8* getelementptr inbounds ([6 x i8], [6 x i8]* @OBJC_METH_VAR_NAME_.1, i64 0, i64 0), section "__DATA,__objc_selrefs,literal_pointers,no_dead_strip", align 8
@llvm.compiler.used = appending global [5 x i8*] [i8* bitcast (%struct._class_t** @"OBJC_CLASSLIST_REFERENCES_$_" to i8*), i8* getelementptr inbounds ([4 x i8], [4 x i8]* @OBJC_METH_VAR_NAME_, i32 0, i32 0), i8* getelementptr inbounds ([6 x i8], [6 x i8]* @OBJC_METH_VAR_NAME_.1, i32 0, i32 0), i8* bitcast (i8** @OBJC_SELECTOR_REFERENCES_ to i8*), i8* bitcast (i8** @OBJC_SELECTOR_REFERENCES_.2 to i8*)], section "llvm.metadata"

declare i8* @objc_msgSend(i8*, i8*, ...) local_unnamed_addr
declare void @llvm.objc.release(i8*)
declare i32 @__gxx_personality_v0(...)

; CHECK-LABEL: __Z3foov:
; CHECK:      bl      _objc_msgSend
; CHECK:      bl      _objc_msgSend
; CHECK:      bl      _OUTLINED_FUNCTION_RELEASE_x19
; CHECK-LABEL: LBB0_2:
; CHECK:      bl      _OUTLINED_FUNCTION_RELEASE_x19
define i32 @_Z3foov() local_unnamed_addr minsize optsize ssp uwtable personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
entry:
  %0 = load i8*, i8** bitcast (%struct._class_t** @"OBJC_CLASSLIST_REFERENCES_$_" to i8**), align 8
  %1 = load i8*, i8** @OBJC_SELECTOR_REFERENCES_, align 8
  %call = tail call i8* bitcast (i8* (i8*, i8*, ...)* @objc_msgSend to i8* (i8*, i8*)*)(i8* %0, i8* %1)
  %2 = load i8*, i8** @OBJC_SELECTOR_REFERENCES_.2, align 8
  invoke void bitcast (i8* (i8*, i8*, ...)* @objc_msgSend to void (i8*, i8*)*)(i8* %call, i8* %2)
          to label %invoke.cont unwind label %lpad

invoke.cont:                                      ; preds = %entry
  tail call void @llvm.objc.release(i8* %call)
  ret i32 0

lpad:                                             ; preds = %entry
  %3 = landingpad { i8*, i32 }
          cleanup
  tail call void @llvm.objc.release(i8* %call)
  resume { i8*, i32 } %3
}

; CHECK-LABEL: __Z3goov:
; CHECK-NOT:  _objc_release
define i32 @_Z3goov() local_unnamed_addr minsize optsize ssp {
  call void @llvm.objc.release(i8* null)
  ret i32 1
}

; CHECK-LABEL: _OUTLINED_FUNCTION_RELEASE_x19:
; CHECK:      mov     x0, x19
; CHECK-NEXT: b       _objc_release


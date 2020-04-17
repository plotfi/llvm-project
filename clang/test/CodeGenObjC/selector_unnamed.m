// RUN: %clang -ObjC -fobjc-runtime=ios -FFoundation %s -o - -S -emit-llvm \
// RUN: -mllvm -method-selector-renamer-mapping-file=%S/Inputs/UnnamedSelectorMap.json \
// RUN: -fobjc-rename-unnamed-selectors | \
// RUN: FileCheck %s

// CHECK-DAG: private unnamed_addr constant [2 x i8] c"a\00"
// CHECK-DAG: private unnamed_addr constant [4 x i8] c"a::\00"
// CHECK-DAG: private unnamed_addr constant [4 x i8] c"b::\00"
// CHECK: { "objc_selector_unnamed" }

@interface C
- (void)testMethod:(int)arg1 bar:(float)arg2 __attribute__((objc_method_unnamed));
- (void)testMethod:(int)arg1 foo:(int)arg2 __attribute__((objc_method_unnamed));
- (void)testMethod:(int)arg1 __attribute__((objc_method_unnamed));
- (void)testMethod __attribute__((objc_method_unnamed));
@end

C *c;

void test() {
  [c testMethod];
  [c testMethod:1 foo:1];
  [c testMethod:1 bar:1.0];
}

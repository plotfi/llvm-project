// RUN: %clang -ObjC -fobjc-runtime=ios -FFoundation %s -o - -S -emit-llvm | FileCheck %s

// CHECK-DAG: private unnamed_addr constant [17 x i8] c"e591f8030c1f86de\00"
// CHECK-DAG: private unnamed_addr constant [19 x i8] c"930f98df830154fa::\00"
// CHECK-DAG: private unnamed_addr constant [19 x i8] c"994e7316fc4cc042::\00"
// CHECK-DAG: private unnamed_addr constant [22 x i8] c"b729086d05d11a47:::::\00"
// CHECK-DAG: private unnamed_addr constant [89 x i8] c"LongNameMethodName2:LongNameArg1Name:LongNameArg2Name:LongNameArg3Name:LongNameArg4Name:\00"

@interface C
- (void)testMethod:(int)arg1 bar:(float)arg2 __attribute__((objc_hash_method_name));
- (void)testMethod:(int)arg1 foo:(int)arg2 __attribute__((objc_hash_method_name));
- (void)testMethod:(int)arg1 __attribute__((objc_hash_method_name));
- (void)testMethod __attribute__((objc_hash_method_name));
- (void)LongNameMethodName:(int)arg1
        LongNameArg1Name:(float)arg2 LongNameArg2Name:(int)arg3
        LongNameArg3Name:(float)arg4 LongNameArg4Name:(int)arg5 __attribute__((objc_hash_method_name));
- (void)LongNameMethodName2:(int)arg1
        LongNameArg1Name:(float)arg2 LongNameArg2Name:(int)arg3
        LongNameArg3Name:(float)arg4 LongNameArg4Name:(int)arg5;
@end

C *c;

void test() {
  [c testMethod];
  [c testMethod:1 foo:1];
  [c testMethod:1 bar:1.0];
  [c LongNameMethodName:1 LongNameArg1Name:1.0 LongNameArg2Name:1 LongNameArg3Name:1.0 LongNameArg4Name:1];
  [c LongNameMethodName2:1 LongNameArg1Name:1.0 LongNameArg2Name:1 LongNameArg3Name:1.0 LongNameArg4Name:1];
}

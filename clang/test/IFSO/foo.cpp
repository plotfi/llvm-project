// RUN: %clang -emit-ifso -fvisibility=hidden %s 2>&1 | FileCheck %s

// CHECK-NOT: top-level-decl: [__Z4fbarff] [fbar]

#include "foo-inline.h"

__attribute__ ((visibility ("hidden"))) int foo(int a, int b) { return a + b; }
__attribute__ ((visibility ("default"))) int foo_default_visi(int a, int b) { return a + b; }


__attribute__ ((visibility ("default"))) int fvih_1(int a, int b) { return a + fvih(); }

int dataA = 34;

namespace baz {
  template <typename T>
  T add(T a, T b) {
    return a + b;
  }
}

namespace n {
  template <typename T>
  struct __attribute__((__visibility__("default"))) S {
    S() = default;
    ~S() = default;
    int __attribute__((__visibility__(("default")))) func() const { return 32; }
    int __attribute__((__visibility__(("hidden")))) operator()() const { return 53; }
  };
}

template <typename T> T neverUsed(T t) { return t + 2; }

template<> int neverUsed<int>(int t);

void g() { n::S<int>()(); }

namespace qux {
int bar(int a, int b) { return baz::add<int>(a, b); }
}

float fbar(float a, float b) { return baz::add<float>(a, b); }


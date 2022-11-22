/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

import java.lang.reflect.Field;

public class AppModuleUsageClass {
  @UsesAppModule({"AppModule"}) private AppModuleUsageOtherClass other_class;

  // Violation 1
  public AppModuleUsageOtherClass other_class2;

  // Violation on allowlist
  public AppModuleUsageOtherClass other_class3;

  @UsesAppModule({"AppModule"})
  public static void method1() {
    AppModuleUsageOtherClass.otherMethod();
  }

  @UsesAppModule({"AppModule", "classes"})
  public static void method2() {
    AppModuleUsageOtherClass.otherMethod2();
    AppModuleUsageOtherClass.otherMethod();
    AppModuleUsageOtherClass.otherMethod2();
    AppModuleUsageOtherClass.otherMethod();
    AppModuleUsageOtherClass.otherMethod2();
    AppModuleUsageOtherClass.otherMethod();
  }

  // Violation 2
  public static void method3() { AppModuleUsageOtherClass.otherMethod3(); }

  // Violation 3
  public void method4() { other_class = new AppModuleUsageOtherClass(); }

  @UsesAppModule({"AppModule"})
  public void method5() {
    other_class.doSomethingToMe();
  }

  // Violation 4
  public int method6() {
    int x = other_class2.field;
    return x;
  }

  // Violation on allowlist
  public void method7() {
    AppModuleUsageOtherClass.otherMethod();
    AppModuleUsageThirdClass.method();
  }

  public static void method8() {
    try {
      Class.forName("AppModuleUsageOtherClass").getDeclaredConstructors();
    } catch (ClassNotFoundException | SecurityException ignoreException) {
      /* okay to ignore in test*/
    }
  }

  @UsesAppModule({"AppModule"})
  public void method9() {
    try {
      other_class.getClass().getDeclaredMethod("doSomethingToMe");
    } catch (NoSuchMethodException | SecurityException ignoreException) {
      /* okay to ignore in test*/
    }
  }

  public static void noAppModuleMethod() {}

  public static void noAppModuleMethod2() { method1(); }
}

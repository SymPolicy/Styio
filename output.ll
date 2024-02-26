; ModuleID = 'styio'
source_filename = "styio"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"

define i32 @main() {
main_entry:
  %0 = call i32 @f(i32 1, i32 2)
  ret i32 0
}

define i32 @f(i32 %a, i32 %b) {
f_entry:
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  %b2 = alloca i32, align 4
  store i32 %b, ptr %b2, align 4
  %0 = load i32, ptr %a1, align 4
  %1 = load i32, ptr %b2, align 4
  %2 = add i32 %0, %1
  ret i32 %2
}

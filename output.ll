; ModuleID = 'styio'
source_filename = "styio"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"

define i32 @main() {
main_entry:
  %a = alloca [4 x i32], i32 4, align 4
  store [4 x i32] [i32 0, i32 1, i32 2, i32 3], ptr %a, align 4
  ret i32 0
}

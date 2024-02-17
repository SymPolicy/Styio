; ModuleID = 'styio'
source_filename = "styio"

define i32 @main() {
entrypoint:
  %a = alloca i32, align 4
  store i32 1, ptr %a, align 4
  %b = alloca i32, align 4
  store i32 2, ptr %b, align 4
  %0 = load i32, ptr %b, align 4
  store i32 %0, ptr %a, align 4
  ret i32 0
}
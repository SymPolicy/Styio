; ModuleID = 'styio'
source_filename = "styio"

define i32 @main() {
entry:
  %0 = call i32 @f(i32 1, i32 2)
  ret i32 0
}

define i32 @f(i32 %a, i32 %b) {
entry:
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  %b2 = alloca i32, align 4
  store i32 %b, ptr %b2, align 4
  %0 = load i32, ptr %a1, align 4
  %1 = load i32, ptr %b2, align 4
  %2 = add i32 %0, %1
  ret i32 %2
}

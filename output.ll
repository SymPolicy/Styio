; ModuleID = 'styio'
source_filename = "styio"

define i32 @main() {
entry:
  ret i32 0
}

define i32 @f(i32 %a, i32 %b) {
entry:
  %a1 = alloca double, align 8
  store i32 %a, ptr %a1, align 4
  %b2 = alloca double, align 8
  store i32 %b, ptr %b2, align 4
  ret i32 0
}

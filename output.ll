; ModuleID = 'styio'
source_filename = "styio"

define i32 @main() {
entrypoint:
  %0 = alloca i32, align 4
  store i32 2, ptr %0, align 4
  ret i32 0
}

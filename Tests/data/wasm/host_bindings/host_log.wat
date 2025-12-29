(module
  (import "env" "host_log" (func $host_log (param i32 i32)))
  (memory (export "memory") 1)
  (data (i32.const 0) "hello")
  (func (export "run")
    i32.const 0
    i32.const 5
    call $host_log)
)
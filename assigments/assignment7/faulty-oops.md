# Kernel Oops Analysis – `/dev/faulty`

## Writing to Faulty Module

```bash
echo "Hello World" > /dev/faulty
```

## Error Summary

Executing the above command causes a kernel oops with the following message:

```
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Internal error: Oops: 96000045 [#1] SMP
Modules linked in: scull(O) faulty(O) hello(O)
```

## Technical Analysis

### Root Cause

According to the crash trace, the issue occurred in the `faulty_write()` function:

```
pc : faulty_write+0x14/0x20 [faulty]
```

From the register dump:

```
x0 : 0000000000000000
```

It is clear that `x0`, which typically holds a function argument (possibly a pointer), is `NULL`. This indicates that `faulty_write()` attempted to dereference a `NULL` pointer.

The instruction at fault:

```
(b900003f)
```

This is an ARM64 store instruction trying to write to memory address `0x0`, which is invalid and results in a crash due to a segmentation fault.

### Module Information

At the time of the crash, the following kernel modules were loaded:

```
Modules linked in: scull(O) faulty(O) hello(O)
```

The fault originated in the `faulty` module when data was written to `/dev/faulty` using the `write()` system call.

### Call Trace

```
faulty_write+0x14/0x20 [faulty]
ksys_write+0x68/0x100
__arm64_sys_write+0x20/0x30
invoke_syscall+0x54/0x130
el0_svc_common.constprop.0+0x44/0xf0
```

This call trace shows the sequence of function calls starting from userspace and leading into the kernel, ultimately crashing inside the faulty module.

## Interpretation

This crash highlights the importance of proper input validation and pointer checking within kernel modules. Writing to a `NULL` pointer in kernel space causes a system crash, which affects the stability and reliability of the entire system.

## Conclusion

Writing to `/dev/faulty` triggered a `NULL pointer dereference` in the kernel module, leading to a critical system failure. This demonstrates how unsafe memory access in kernel code can result in fatal errors, emphasizing the need for careful memory management and robust error handling during kernel development.

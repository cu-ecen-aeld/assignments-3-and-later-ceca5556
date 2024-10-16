# Assignment 7: error with kernel module faulty

## source of error
`echo "hello_world" > /dev/faulty`

## error message output
```
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
  ESR = 0x0000000096000045
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
  FSC = 0x05: level 1 translation fault
Data abort info:
  ISV = 0, ISS = 0x00000045
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=0000000041b6c000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 0000000096000045 [#1] SMP
Modules linked in: hello(O) faulty(O) scull(O)
CPU: 0 PID: 153 Comm: sh Tainted: G           O       6.1.44 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : faulty_write+0x10/0x20 [faulty]
lr : vfs_write+0xc8/0x390
sp : ffffffc008df3d20
x29: ffffffc008df3d80 x28: ffffff8001b30d40 x27: 0000000000000000
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
x23: 000000000000000c x22: 000000000000000c x21: ffffffc008df3dc0
x20: 000000557d261180 x19: ffffff8001c0f300 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
x5 : 0000000000000001 x4 : ffffffc000787000 x3 : ffffffc008df3dc0
x2 : 000000000000000c x1 : 0000000000000000 x0 : 0000000000000000
Call trace:
 faulty_write+0x10/0x20 [faulty]
 ksys_write+0x74/0x110
 __arm64_sys_write+0x1c/0x30
 invoke_syscall+0x54/0x130
 el0_svc_common.constprop.0+0x44/0xf0
 do_el0_svc+0x2c/0xc0
 el0_svc+0x2c/0x90
 el0t_64_sync_handler+0xf4/0x120
 el0t_64_sync+0x18c/0x190
Code: d2800001 d2800000 d503233f d50323bf (b900003f)
---[ end trace 0000000000000000 ]---
```

## Analysis
The very first line of the error message states the cause of the error:
> Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000

This essentially means that the system attempted to dereference an uninitialized pointer which led to the equivalent of a SEGFAULT in userspace occured.

Several lines down from the first message the output shows:
> CPU: 0 PID: 153

This is the Process ID that was running the code and which CPU it was running on. This can be useful in differerntiating between all the different processes running on a system.

A couple of lines below that is the program counter (pc):
> pc : faulty_write+0x10/0x20 [faulty]
This is the address of the instruction that caused the error. In this case, it shows that the instruction was called within a function called `faulty_write`.

Right below is the link register, which seems to point to a sort of write function/instruction: 
> `lr : vfs_write+0xc8/0x390`

Additionally, the call trace several lines down also shows calls to a write function/instruction: 
```
ksys_write+0x74/0x110
__arm64_sys_write+0x1c/0x30
invoke_syscall+0x54/0x130
```
From these, it can be deduced that the specific cause of the error was that the system tried to write a value using an uninitialized pointer. 

Looking into the source code of the driver `faulty.c` for a function by the name of "faulty_write" produced the following:
```
ssize_t faulty_write (struct file *filp, const char __user *buf, size_t count,
		loff_t *pos)
{
	/* make a simple fault by dereferencing a NULL pointer */
	*(int *)0 = 0;
	return 0;
}
```
The line of code `*(int *)0 = 0;` proves that the error was caused from dereferencing an uninitialized pointer and attempting to write a value to it.
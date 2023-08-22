/* int clone3(struct clone_args *args, size_t size, int (*fn)(void *), void *arg); */

.text
.globl clone3;
.type clone3, @function;
.align 16;
.cfi_startproc;
clone3:
  mov	%rcx, %r8
  movl $435, %eax /* SYS_clone3 */
  .cfi_endproc;
  syscall
  test %rax, %rax
  /* jl ERROR */
  jz .L1 /* child process */
  ret
.L1:
  .cfi_startproc;
  .cfi_undefined rip;
  xorl %ebp, %ebp /* clear frame pointer */
  mov %r8, %rdi /* arg */
  call *%rdx /* call fn */
  movq %rax, %rdi
  movl $60, %eax /* SYS_exit */
  syscall
  .cfi_endproc;
  .cfi_startproc;
  .cfi_endproc;
.size clone3, .-clone3

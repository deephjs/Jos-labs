/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>



###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
TRAPHANDLER_NOEC(t_divide, T_DIVIDE);	
TRAPHANDLER_NOEC(t_debug, T_DEBUG);	
TRAPHANDLER(t_nmi, T_NMI);	
TRAPHANDLER_NOEC(t_brkpt, T_BRKPT);	
TRAPHANDLER(t_oflow, T_OFLOW);	
TRAPHANDLER(t_bound, T_BOUND);	
TRAPHANDLER(t_illop, T_ILLOP);	
TRAPHANDLER(t_device, T_DEVICE);	
TRAPHANDLER(t_dblflt, T_DBLFLT);	

TRAPHANDLER(t_tss, T_TSS);	
TRAPHANDLER(t_segnp, T_SEGNP);	
TRAPHANDLER(t_stack, T_STACK);	
TRAPHANDLER(t_gpflt, T_GPFLT);	
TRAPHANDLER(t_pgflt, T_PGFLT);	

TRAPHANDLER(t_fperr, T_FPERR);	
TRAPHANDLER(t_align, T_ALIGN);	
TRAPHANDLER(t_mchk, T_MCHK);	
TRAPHANDLER(t_simderr, T_SIMDERR);
TRAPHANDLER_NOEC(t_syscall, T_SYSCALL);	


.globl sysenter_handler;
.type sysenter_handler, @function;
.align 2;
sysenter_handler:
/*
 * Lab 3: Your code here for system call handling
 */
	pushl %esi
	pushl %ebp

	pushl %edi
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %eax

	movl $GD_KD , %eax
	movw %ax, %ds
	movw %ax, %es

	call syscall

	movl $GD_UD, %ecx
	movw %cx, %ds
	movw %cx, %es

	movl %ebp, %ecx
	movl %esi, %edx
	sysexit

/*
 * Lab 3: Your code here for _alltraps
 */
_alltraps:
	/*pushl	%ds
	pushl	%es
	pushal

	movw	$GD_KD, %ax
	movw	%ax, %ds
	movw	%ax, %es

	pushl	%esp
	call	trap*/

	pushw $0
	pushw %ds
	pushw $0
	pushw %es
	pushal
	movl $GD_KD, %eax
	movw %ax, %ds
	movw %ax, %es
	push %esp
	call trap
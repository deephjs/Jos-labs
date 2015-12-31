/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>
#include <kern/spinlock.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 3: Your code here.
	user_mem_assert(curenv, (void*)s, len, PTE_U);
	
	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	if (e == curenv)
		cprintf("[%08x] exiting gracefully\n", curenv->env_id);
	else
		cprintf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);
	env_destroy(e);
	return 0;
}

static int
sys_map_kernel_page(void* kpage, void* va)
{
	int r;
	struct Page* p = pa2page(PADDR(kpage));
	if(p ==NULL)
		return E_INVAL;
	r = page_insert(curenv->env_pgdir, p, va, PTE_U | PTE_W);
	return r;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.

	// LAB 4: Your code here.
	/*struct Env *e = NULL;
	int r = env_alloc(&e, curenv->env_id);
	if(r == -E_NO_FREE_ENV) return -E_NO_FREE_ENV;
	else if(r == -E_NO_MEM) return -E_NO_MEM;

	e->env_status = ENV_NOT_RUNNABLE;
	e->env_tf =curenv->env_tf;
	e->env_tf.tf_regs.reg_eax = 0;
	return e->env_id;*/

    	struct Env *e;
    	int res = 0;
    	if((res = env_alloc(&e,curenv->env_id))!=0){
        	return res;
   	}
   	e->env_status = ENV_NOT_RUNNABLE;
    	e->env_tf = curenv->env_tf;
    	e->env_tf.tf_regs.reg_eax = 0;
    	return e->env_id;
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.
	
	// LAB 4: Your code here.
	/*struct Env *e = NULL;
	if(envid2env(envid, &e, 1) != 0) return -E_BAD_ENV;

	if(status == ENV_RUNNABLE || status == ENV_NOT_RUNNABLE){
		e->env_status = status;
		return 0;
	}else{
		return -E_INVAL;
	}*/

    	int res = 0;
    	struct Env * e;
    	if((res = envid2env(envid,&e,1))!=0){
        	return res;
    	}
    	if(status != ENV_RUNNABLE && status != ENV_RUNNING){
        	return -E_INVAL;
    	}
    	e->env_status = status;
    	return 0;
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
	/*struct Env *e = NULL;
	if(envid2env(envid, &e, 1) != 0) return -E_BAD_ENV;
	e->env_pgfault_upcall = func;

	return 0;*/

    	int res = 0;
    	struct Env * e;
    	if((res = envid2env(envid,&e,1))!=0){
        	return res;
    	}
    	e->env_pgfault_upcall = func;
	//panic("sys_env_set_pgfault_upcall not implemented");
    	return 0;
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!

	// LAB 4: Your code here.
	/*struct Env *e = NULL;
	if(envid2env(envid, &e, 1) != 0) return -E_BAD_ENV;
	
	if(PDX(va) >= UTOP || va != ROUNDUP(va, PGSIZE)) return -E_INVAL;
	if(((PTE_U|PTE_P) & perm) != (PTE_U|PTE_P)) return -E_INVAL;

	struct Page *p = page_alloc(ALLOC_ZERO);
	if(p == NULL) return -E_NO_MEM;
	if(page_insert(e->env_pgdir, p, va, perm) != 0){
		page_free(p);
		return -E_NO_MEM;
	}
	return 0;*/
    	int res=0;
    	struct Env * e;
    	if((res=envid2env(envid,&e,1))!=0){
        	return res;
    	}
    	if((uint32_t)va >= UTOP||(uint32_t)va%PGSIZE!=0) return -E_INVAL;
    	if((perm&(~PTE_SYSCALL))||((perm&PTE_U) ==0)||((perm&PTE_P) == 0)){
        	return -E_INVAL;
    	}
    	struct Page * pg;
    	pg = page_alloc(ALLOC_ZERO);
    	if(pg==NULL) return -E_NO_MEM;
    	res = page_insert(e->env_pgdir,pg,va,perm);
    	return res;
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// LAB 4: Your code here.
	/*struct Env *srce = NULL;
	struct Env *dste = NULL;
	if(envid2env(srcenvid, &srce, 1) != 0) return -E_BAD_ENV;
	if(envid2env(dstenvid, &dste, 1) != 0) return -E_BAD_ENV;

	if(PDX(srcva) >= UTOP || srcva != ROUNDUP(srcva, PGSIZE)) return -E_INVAL;
	if(PDX(dstva) >= UTOP || dstva != ROUNDUP(dstva, PGSIZE)) return -E_INVAL;

	if(((PTE_U|PTE_P) & perm) != (PTE_U|PTE_P)) return -E_INVAL;

	pte_t *pte = NULL;
	struct Page *page = page_lookup(srce->env_pgdir, srcva, &pte);
	if(page == NULL) panic("sys_page_map: cannot find page in src");

	if((perm & PTE_W) && ((*pte & PTE_W) == 0)) return -E_INVAL;
	if(page_insert(dste->env_pgdir, page, dstva, perm) != 0) return -E_NO_MEM;
	return 0;*/

    	int res=0;
    	struct Env * srce, *dste;
    	if((res=envid2env(srcenvid,&srce,1))!=0){
        	return res;
    	}
    	if((res=envid2env(dstenvid,&dste,1))!=0){
        	return res;
    	}
    	if((uint32_t)srcva >= UTOP||(uint32_t)srcva%PGSIZE!=0) return -E_INVAL;
    	if((uint32_t)dstva >= UTOP||(uint32_t)dstva%PGSIZE!=0) return -E_INVAL;
    	struct Page *pg;
    	pte_t *pte_store;
    	pg = page_lookup(srce->env_pgdir,srcva,&pte_store);
    	if(pg == NULL||pte_store==NULL) return -E_INVAL;
    	if((perm&(~PTE_SYSCALL))||((perm&PTE_U) ==0)||((perm&PTE_P) == 0)){
        	return -E_INVAL;
    	}
    	if((perm&PTE_W)&&((*pte_store&PTE_W) ==0)){
        	return -E_INVAL;
    	}
    	res = page_insert(dste->env_pgdir,pg,dstva,perm);
    	return res;
}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().

	// LAB 4: Your code here.
	/*struct Env *e = NULL;
	if(envid2env(envid, &e, 1) != 0) return -E_BAD_ENV;
	if(PDX(va) >= UTOP || va != ROUNDUP(va, PGSIZE)) return -E_INVAL;
	page_remove(e->env_pgdir, va);
	return 0;*/

    	int res=0;
    	struct Env * e;
    	if((res=envid2env(envid,&e,1))!=0){
        	return res;
    	}
    	if((uint32_t)va >= UTOP || (uint32_t)va % PGSIZE != 0) return -E_INVAL;
    	page_remove(e->env_pgdir,va);
    	return 0;
}

// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// Returns 0 on success, < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in the
//		current environment's address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
	// LAB 4: Your code here.
	//panic("sys_ipc_try_send not implemented");

        struct Env *recv;
        if(envid2env(envid, &recv, 0)<0) {
                cprintf("sys_ipc_try_send : environment envid doesn't currently exist.\n");
                return -E_BAD_ENV;
        }

        if(recv->env_ipc_recving == 0) {
                //return -E_IPC_NOT_RECV;
		//Lab 4 challenge
		curenv->env_ipc_sending = 1;
		curenv->env_ipc_to = envid;
		curenv->env_ipc_pvalue = value;
		curenv->env_ipc_srcva = srcva;
		curenv->env_ipc_pending = perm;
		curenv->env_tf.tf_regs.reg_eax = 0;
		curenv->env_status = ENV_NOT_RUNNABLE;
		sched_yield();
        }

        if((uint32_t)srcva < UTOP) {
                if(PGOFF(srcva)) {
                        cprintf("sys_ipc_try_send : srcva is not page-aligned.\n");
                        return -E_INVAL;
                }

                if(!(perm & (PTE_U|PTE_P))){
                        cprintf("sys_ipc_try_send : perm[%08x] wrong--PTE_U|PTE_P not be set\n", perm);
                        return -E_INVAL;
                }else if(perm & (0x1F8)){//
                        cprintf("sys_ipc_try_send : perm wrong--unknown perm bit.\n");
                        return -E_INVAL;
                }

                int ret;
                if((int)recv->env_ipc_dstva!=-1)
                        ret = sys_page_map(curenv->env_id, srcva, recv->env_id, recv->env_ipc_dstva, perm);

                if(ret < 0)
                        return ret;
                else
                        recv->env_ipc_perm = perm;
        }else
                 recv->env_ipc_perm = 0;

        recv->env_ipc_recving = 0;
        recv->env_ipc_value = value;
        recv->env_ipc_from = curenv->env_id;
        recv->env_status = ENV_RUNNABLE;

        return 0;
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
	// LAB 4: Your code here.
	//panic("sys_ipc_recv not implemented");
	struct Env *e;
	struct Page *p;
	pte_t *pte;
	int i, r;
	for (i = 0; i < NENV; i++) {
		// Search for a pending sender for current env.
		// If error occurs, go on to the next sender.
		if (envs[i].env_type != ENV_TYPE_IDLE && envs[i].env_status ==
				ENV_NOT_RUNNABLE && envs[i].env_ipc_sending &&
				envs[i].env_ipc_to == curenv->env_id) {
			e = &envs[i];
			e->env_ipc_sending = 0;

			if ((uintptr_t) e->env_ipc_srcva < UTOP) {
				if(!(p = page_lookup(e->env_pgdir, e->env_ipc_srcva, &pte))) {
					r = -E_INVAL;
					goto ret;
				}
				if (!(*pte & PTE_W) && (e->env_ipc_pending & PTE_W)) {
					r = -E_INVAL;
					goto ret;
				}
				if ((uintptr_t) dstva < UTOP && (r =
							page_insert(curenv->env_pgdir, p, dstva,
								e->env_ipc_pending)) < 0)
					goto ret;
				curenv->env_ipc_perm = e->env_ipc_pending;
			}
			else
				curenv->env_ipc_perm = 0;
			curenv->env_ipc_from = e->env_id;
			curenv->env_ipc_value = e->env_ipc_pvalue;
			e->env_status = ENV_RUNNABLE;
			return 0;
ret:
			e->env_tf.tf_regs.reg_eax = r;
			e->env_status = ENV_RUNNABLE;
			continue;
		}
	}

        if(curenv->env_ipc_recving) {//it is waiting for a ipc
                panic("curenv is waiting for a ipc already, something wrong\n");
        }else {
                if((int)dstva == -1)
                        curenv->env_ipc_dstva = dstva;
                else if((uint32_t)dstva<UTOP && PGOFF(dstva)) {
                        cprintf("sys_ipc_recv : dstva is illegal.\n");
                        return -E_INVAL;
                }
                curenv->env_ipc_recving = 1;
                curenv->env_ipc_dstva = dstva;
                curenv->env_status = ENV_NOT_RUNNABLE;
                curenv->env_tf.tf_regs.reg_eax = 0;
                sched_yield();
        }
        return 0;
}

static int
sys_sbrk(uint32_t inc)
{
	// LAB3: your code sbrk here...
	int size = ROUNDUP(inc, PGSIZE);
	int i;
	for(i= 0; i< size; i+= PGSIZE){
		struct Page *p = page_alloc(1);
		if(p == NULL)
			panic("out of memory");
		if(page_insert(curenv->env_pgdir, p, (void*)(curenv->brk +i) , PTE_W | PTE_U) < 0){
			panic("out of memory");
		}
	}
	cprintf("%x, %d\n", curenv->brk, size);
	curenv->brk += size;
	cprintf("%x\n\n", curenv->brk);
	return curenv->brk;
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.
	switch(syscallno){
	//LAB 3
	case SYS_cputs:
		sys_cputs((char*)a1, a2);
		return 0;
	case SYS_cgetc:
		return sys_cgetc();
	case SYS_getenvid:
		return (int32_t)sys_getenvid();
	case SYS_env_destroy:
		return sys_env_destroy((envid_t)a1);
	case SYS_map_kernel_page:
		return sys_map_kernel_page((void*)a1, (void*)a2);
	case SYS_sbrk:
		return sys_sbrk(a1);
	//LAB 4
	case SYS_page_alloc:
		return sys_page_alloc((envid_t)a1, (void*)a2, (int)a3);
	case SYS_page_map:
		return sys_page_map((envid_t)(*(uint32_t*)a1), 
							  (void*)(*((uint32_t*)a1+1)), 
							(envid_t)(*((uint32_t*)a1+2)),
							  (void*)(*((uint32_t*)a1+3)), 
								(int)(*((uint32_t*)a1+4)));
	case SYS_page_unmap:
		return sys_page_unmap((envid_t)a1, (void*)a2);
	case SYS_exofork:
		return sys_exofork();
	case SYS_env_set_status:
		return sys_env_set_status((envid_t)a1, a2);
	case SYS_env_set_pgfault_upcall:
		return sys_env_set_pgfault_upcall((envid_t)a1, (void*)a2);
	case SYS_yield:
		sys_yield();
		return 0;
	case SYS_ipc_try_send:
		return sys_ipc_try_send((envid_t)a1, a2, (void*)a3, a4);
	case SYS_ipc_recv:
		return sys_ipc_recv((void*)a1);
	default:
		return -E_INVAL;
	}
}


int32_t syscallWrapper(struct Trapframe *tf){
	lock_kernel();

	curenv->env_tf.tf_regs = tf->tf_regs;
	curenv->env_tf.tf_es = tf->tf_es;
	curenv->env_tf.tf_ds = tf->tf_ds;
	curenv->env_tf.tf_trapno = tf->tf_trapno;
	curenv->env_tf.tf_err = tf->tf_err;
	curenv->env_tf.tf_eip = tf->tf_eip;
	curenv->env_tf.tf_cs = tf->tf_cs;
	curenv->env_tf.tf_eflags = tf->tf_eflags;
	curenv->env_tf.tf_esp = tf->tf_esp;
	curenv->env_tf.tf_ss = tf->tf_ss;

	int32_t ret = syscall(tf->tf_regs.reg_eax, 
						  tf->tf_regs.reg_edx, 
						  tf->tf_regs.reg_ecx, 
						  tf->tf_regs.reg_ebx, 
						  tf->tf_regs.reg_edi, 
						  0);

	tf->tf_eflags |= FL_IF;
	tf->tf_regs.reg_eax = ret;
	curenv->env_tf.tf_eflags |= FL_IF;
	curenv->env_tf.tf_regs.reg_eax = ret;

	//turn interrupt on again
	unlock_kernel();
	return ret;
}

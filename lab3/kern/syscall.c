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

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 3: Your code here.
	user_mem_assert(curenv, s, len, PTE_U);

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

static int
sys_sbrk(uint32_t inc)
{
	// LAB3: your code sbrk here...
	int size = ROUNDUP(inc, PGSIZE);
	int i;
	for(i = 0; i < size; i += PGSIZE){
		struct Page *p = page_alloc(1);
		if(p == NULL)
			panic("fail to alloc a new page");
		if(page_insert(curenv->env_pgdir, p, (void*)(curenv->mbrk +i) , PTE_W | PTE_U) != 0){
			panic("fail to insert a page");
		}
	}
	curenv->mbrk += size;
	return curenv->mbrk;
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code havere.
	int32_t r = 0;
	switch(syscallno){
	case SYS_cputs:
		sys_cputs((const char*)a1, (size_t)a2);
		break;
	case SYS_cgetc:
		r = sys_cgetc();
		break;
	case SYS_getenvid:
		r = sys_getenvid();
		break;
	case SYS_env_destroy:
		r = sys_env_destroy((envid_t)a1);
		break;
	case SYS_map_kernel_page:
		r = sys_map_kernel_page((void*)a1, (void*)a2);
		break;
	case SYS_sbrk:
		r = sys_sbrk(a1);
		break;
	deault:
		r = -E_INVAL;
	}

	return r;

	//panic("syscall not implemented");
}


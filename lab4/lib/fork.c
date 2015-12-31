// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at vpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	/*if((err & FEC_WR) == 0){
		panic("pgfault error: FEC_WR not set in err");
	}
	if((vpt[(uint32_t)addr/PGSIZE] & PTE_COW) == 0){
		panic("pgfault error: not write or cow");
	}*/
	
    	pte_t pte = vpt[PGNUM(addr)];
    	if(!(pte&PTE_COW)||!(err&FEC_WR)){
        	panic("pgfault error! %x %x %x\n",addr,pte,err);
   	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 4: Your code here.
	/*r = sys_page_alloc(0, PFTEMP, PTE_P|PTE_U|PTE_W);
	if(r < 0) panic("pgfault error: %e", r);
	void *va = (void*)ROUNDDOWN(addr, PGSIZE);
	memmove(PFTEMP, va, PGSIZE);
	r = sys_page_map(0, PFTEMP, 0, va, PTE_P|PTE_U|PTE_W);
	if(r < 0) panic("pgfault error: %e", r);*/
	void *pddr = ROUNDDOWN(addr,PGSIZE);
	if ((r = sys_page_alloc(0, PFTEMP, PTE_P|PTE_U|PTE_W)) < 0)
		panic("pgfault sys_page_alloc: %e\n", r);
	memmove(PFTEMP, pddr, PGSIZE);
	if ((r = sys_page_map(0, PFTEMP, 0, pddr, PTE_P|PTE_U|PTE_W)) < 0)
		panic("pgfault sys_page_map: %e\n", r);
	if ((r = sys_page_unmap(0, PFTEMP)) < 0)
		panic("pgfault sys_page_unmap: %e", r);

}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	//int r;

	// LAB 4: Your code here.
	/*if(vpt[pn] & (PTE_W|PTE_COW)){
		//copy on write page:
		r = sys_page_map(sys_getenvid(), (void*)(pn*PGSIZE), envid, (void*)(pn*PGSIZE), PTE_P|PTE_U|PTE_COW);
		if(r < 0) return r;
		r = sys_page_map(sys_getenvid(), (void*)(pn*PGSIZE), sys_getenvid(), (void*)(pn*PGSIZE), PTE_P|PTE_U|PTE_COW);
		if(r < 0) return r;
	}else{
		//read only
		r = sys_page_map(sys_getenvid(), (void*)(pn*PGSIZE), envid, (void*)(pn*PGSIZE), PTE_P|PTE_U);
		if(r < 0) return r;
	}
	return 0;*/
    	int r=sys_getenvid();  
    	int result=0;  
    	int perm=0;  
    	if(pn*PGSIZE==UXSTACKTOP-PGSIZE)  
        	return 0;   
        perm = (perm |PTE_P| PTE_U|PTE_COW );  
    	result=sys_page_map(r, (void*)(pn*PGSIZE),envid, (void*)(pn*PGSIZE), perm);  
    	result=sys_page_map(r, (void*)(pn*PGSIZE),r, (void*)(pn*PGSIZE), perm);  
    	return 0; 
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use vpd, vpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	set_pgfault_handler(pgfault);
	envid_t child = sys_exofork();
	if(child < 0) panic("fork error: %e", child);

	//in children
	if(child == 0){
		thisenv = &envs[ENVX(sys_getenvid())];
	}
	int i;
	int r = 0;
	int pn_utext = UTEXT / PGSIZE;
	int pn_utop = UTOP / PGSIZE;
	for(i= pn_utext; i< pn_utop; ++i){
		if(i == PGNUM(UXSTACKTOP-PGSIZE)){
			r = sys_page_alloc(child, (void*)(UXSTACKTOP-PGSIZE), PTE_P|PTE_U|PTE_W);
		}else if((vpd[i/NPTENTRIES] & PTE_P) && (vpt[i] & PTE_P)){
			r = duppage(child, i);
		}
		if(r < 0) panic("fork error in loop: %e, i=%d", r, i);
	}
	extern void _pgfault_upcall();
	r = sys_env_set_pgfault_upcall(child, (void*)_pgfault_upcall);
	if(r < 0) panic("fork error: %e", r);
	r = sys_env_set_status(child, ENV_RUNNABLE);
	if(r < 0) panic("fork error: %e", r);
	return child;
    
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}

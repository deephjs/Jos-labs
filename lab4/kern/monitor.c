// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/env.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "backtrace the function call", mon_backtrace},
	{ "time", "show the running time of a command", mon_time},
	{ "x", "display the memory", mon_x},
	{ "si", "executing the code instruction by instruction", mon_si},
	{ "c", "continue execution from the current location", mon_c}
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

unsigned read_eip();

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		(end-entry+1023)/1024);
	return 0;
}

// Lab1 only
// read the pointer to the retaddr on the stack
static uint32_t
read_pretaddr() {
    uint32_t pretaddr;
    __asm __volatile("leal 4(%%ebp), %0" : "=r" (pretaddr)); 
    return pretaddr;
}

void
do_overflow(void)
{
    cprintf("Overflow success\n");
}


void
start_overflow(void)
{
	// You should use a techique similar to buffer overflow
	// to invoke the do_overflow function and
	// the procedure must return normally.

    // And you must use the "cprintf" function with %n specifier
    // you augmented in the "Exercise 9" to do this job.

    // hint: You can use the read_pretaddr function to retrieve 
    //       the pointer to the function call return address;
    char str[256] = {};
    int nstr = 0;
    char *pret_addr;
    // overthrough stack operations of do_overflow
    int fun_addr = (int)(&do_overflow) + 6;
	// Your code here.
	memset(str, 0xd, sizeof(str)-1);
	pret_addr = (char*)read_pretaddr();

	nstr = ((char*)&fun_addr)[0] & 0xff;
	str[nstr] = 0;
	cprintf("%s%n", str, pret_addr);
	str[nstr] = 0xd;

	nstr = ((char*)&fun_addr)[1] & 0xff;
	str[nstr] = 0;
	cprintf("%s%n", str, pret_addr+1);
	str[nstr] = 0xd;

	nstr = ((char*)&fun_addr)[2] & 0xff;
	str[nstr] = 0;
	cprintf("%s%n", str, pret_addr+2);
	str[nstr] = 0xd;

	nstr = ((char*)&fun_addr)[3] & 0xff;
	str[nstr] = 0;
	cprintf("%s%n", str, pret_addr+3);
}

void
overflow_me(void)
{
        start_overflow();
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	uint32_t *ebp = (uint32_t*)read_ebp();
	while(ebp != 0){
		cprintf("eip %x ebp %x args %08x %08x %08x %08x %08x\n", *(ebp+1), ebp, *(ebp+2), *(ebp+3), *(ebp+4), *(ebp+5), *(ebp+6));
		struct Eipdebuginfo info;
		if(debuginfo_eip(*(ebp+1), &info) == 0){
			//found
			cprintf("       %s:%d: %.*s %+d\n", 
				info.eip_file, info.eip_line, 
				info.eip_fn_namelen, info.eip_fn_name, 
				*(ebp+1) - info.eip_fn_addr);
		}
		ebp = (uint32_t*)(*ebp);
	}

	//overflow_me();
	cprintf("Backtrace success\n");
	return 0;
}


/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}


unsigned long long get_rdtsc(){
	unsigned long long rdtsc;
	__asm __volatile("rdtsc" : "=r" (rdtsc));

	return rdtsc;
}

int
mon_time(int argc, char **argv, struct Trapframe *tf){
        unsigned long long time1;
	unsigned long long time2;
	time1 = get_rdtsc();
        runcmd(*(argv+1), tf);
	time2 = get_rdtsc();
        cprintf("%s cycles: %llu\n", *(argv+1), time2 - time1);
        return 1;
}


//for debug
int
mon_x(int argc, char **argv, struct Trapframe *tf){
	int addr = strtol(argv[1], NULL, 16);
	cprintf("%d\n",*(int*)addr);
	return 0;
}

int
mon_si(int argc, char **argv, struct Trapframe *tf){
	tf->tf_eflags |= FL_TF;
	cprintf("tf_eip=%08x\n", tf->tf_eip);
	env_run(curenv);
	return 0;
}

int
mon_c(int argc, char **argv, struct Trapframe *tf){
	tf->tf_eflags &= (~FL_TF);
	env_run(curenv);
	return 0;
}




// return EIP of caller.
// does not work if inlined.
// putting at the end of the file seems to prevent inlining.
unsigned
read_eip()
{
	uint32_t callerpc;
	__asm __volatile("movl 4(%%ebp), %0" : "=r" (callerpc));
	return callerpc;
}

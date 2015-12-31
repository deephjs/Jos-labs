#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>


// Choose a user environment to run and run it.
void
sched_yield(void)
{
	struct Env *idle;
	int i;
	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env this CPU was
	// last running.  Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running on this CPU is still ENV_RUNNING, it's okay to
	// choose that environment.
	//
	// Never choose an environment that's currently running on
	// another CPU (env_status == ENV_RUNNING) and ever choose an
	// idle environment (env_type == ENV_TYPE_IDLE).  If there are
	// no runnable environments, simply drop through to the code
	// below to switch to this CPU's idle environment.

	// LAB 4: Your code here.
	/*int end;
	if(curenv != NULL){
		end = ENVX(curenv->env_id);
		i = ENVX(curenv->env_id + 1) % NENV;
	}else{
		end = 0;
		i = 1;
	}
	while(i != end){
		if(envs[i].env_status == ENV_RUNNABLE && envs[i].env_type != ENV_TYPE_IDLE){
			env_run(&envs[i]);
			return;
		}
		i = (i + 1) % NENV;
	}
	if(thiscpu->cpu_env && thiscpu->cpu_env->env_status == ENV_RUNNING){
		env_run(thiscpu->cpu_env);
	}

	// For debugging and testing purposes, if there are no
	// runnable environments other than the idle environments,
	// drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if (envs[i].env_type != ENV_TYPE_IDLE &&
		    (envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING))
			break;
	}
	if (i == NENV) {
		cprintf("No more runnable environments!\n");
		while (1)
			monitor(NULL);
	}

	// Run this CPU's idle environment when nothing else is runnable.
	idle = &envs[cpunum()];
	if (!(idle->env_status == ENV_RUNNABLE || idle->env_status == ENV_RUNNING))
		panic("CPU %d: No idle environment!", cpunum());
	env_run(idle);
	*/

	int CUR, j;
    	if(curenv != NULL) {
        	CUR = ENVX(curenv->env_id);
        	//curenv->env_status = ENV_RUNNABLE;
    	}
   	else CUR = 0;
	for (j = 1; j <= NENV; j++) {
        	i = (CUR+j) % NENV;
        	//cprintf("CPU %d: sched1\n",i);
		if (envs[i].env_status == ENV_RUNNABLE&&envs[i].env_type!=ENV_TYPE_IDLE)
			break;
	}
    	//cprintf("CPU %d: sched1\n",cpunum());
	if (i != CUR) {
		//cprintf("goto run\n");
		env_run(&envs[i]);
	}
	// For debugging and testing purposes, if there are no
	// runnable environments other than the idle environments,
	// drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if (envs[i].env_type != ENV_TYPE_IDLE &&
		    (envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING))
			break;
	}
	if (i == NENV) {
		cprintf("No more runnable environments!\n");
		while (1)
			monitor(NULL);
	}

	// Run this CPU's idle environment when nothing else is runnable.
	idle = &envs[cpunum()];
	if (!(idle->env_status == ENV_RUNNABLE || idle->env_status == ENV_RUNNING))
		panic("CPU %d: No idle environment!", cpunum());
	env_run(idle);
}

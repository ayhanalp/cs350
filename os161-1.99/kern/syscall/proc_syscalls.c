/* Ayhan Alp Aydeniz - aaaydeni */

#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <mips/trapframe.h>
#include <test.h>

	/* this implementation of sys__exit does not do anything with the exit code */
	/* this needs to be fixed to get exit() and waitpid() working properly */

#if OPT_A3

void _sys__exit(int exitcode) {

#else

void sys__exit(int exitcode) {

#endif // Optional for ASSGN3

	struct addrspace *as;
	struct proc *p = curproc;
	/* for now, just include this to keep the compiler from complaining about
		 an unused variable */
	(void)exitcode;

	DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

	KASSERT(curproc->p_addrspace != NULL);
	as_deactivate();
	/*
	 * clear p_addrspace before calling as_destroy. Otherwise if
	 * as_destroy sleeps (which is quite possible) when we
	 * come back we'll be calling as_activate on a
	 * half-destroyed address space. This tends to be
	 * messily fatal.
	 */
	as = curproc_setas(NULL);
	as_destroy(as);

#if OPT_A2

	DEBUG(DB_PROCSYS, "Syscall: _exit (Code %d)\n",exitcode);
	KASSERT(curproc->p_data != NULL);

	lock_acquire(procdata_lock);

	procdata_t *p_data = curproc->p_data;

	DEBUG(DB_PROCSYS, "Free PID (%d)\n", p_data->p_pid);
	
	pid_use[p_data->p_pid] = false;

	procdata_t *child = p_data->p_firstchild;
	
	while (NULL != child)
	{
		procdata_t *next = child->p_nextsibling;
		
		if (child->p_exited == 1)
		{
			procdata_destroy(child);
		}
		
		else
		{
			child->p_parent = NULL;
		}
		
		child = next;
	}
	
	p_data->p_firstchild = NULL;

	if (p_data->p_parent)
       	{
		
#if OPT_A3
		p_data->p_exit_code = exitcode;
#else
		p_data->p_exit_code = _MKWAIT_EXIT(exitcode);
#endif // Optional for ASSGN3

		p_data->p_exited = true;
		cv_broadcast(procdata_cv, procdata_lock);
	}

	else
	{
		procdata_destroy(p_data);
		curproc->p_data = NULL;
	}

	lock_release(procdata_lock);
#endif // Optional for ASSGN2

	/* detach this thread from its process */
	/* note: curproc cannot be used after this call */
	proc_remthread(curthread);

	/* if this is the last user process in the system, proc_destroy()
		 will wake up the kernel menu thread */
	proc_destroy(p);
	
	thread_exit();
	/* thread_exit() does not return, so we should never get here */
	panic("return from thread_exit in sys_exit\n");
}

#if OPT_A3

void sys__exit(int exitcode) {
	_sys__exit(_MKWAIT_EXIT(exitcode));
}

void terminate_kill_exit(int sig) {
	_sys__exit(_MKWAIT_SIG(sig));
}

#endif // Optional for ASSGN3

/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{

#if OPT_A2

	DEBUG(DB_PROCSYS, "Syscall: getpid\n");
	KASSERT(curproc->p_data != NULL);
	lock_acquire(procdata_lock);
	DEBUG(DB_PROCSYS, "PID: %d\n", curproc->p_data->p_pid);
	*retval = curproc->p_data->p_pid;
	lock_release(procdata_lock);
#else

	/* for now, this is just a stub that always returns a PID of 1 */
	/* you need to fix this to make it work properly */
	*retval = 1;

#endif // Optional for ASSGN2
	return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
			userptr_t status,
			int options,
			pid_t *retval)
{
	int exitstatus;
	int result;
	
#if OPT_A2
	DEBUG(DB_PROCSYS, "Syscall: waitpid(%d)\n",pid);

	if (options != 0)
	{
		return(EINVAL);
	}

	if (pid < 0 || pid > PID_MAX)
	{
		DEBUG(DB_PROCSYS, "Invalid PID\n");
		return ESRCH;
	}

	lock_acquire(procdata_lock);

	procdata_t *child = curproc->p_data->p_firstchild;

	while (NULL != child)
	{
		if (child->p_pid == pid)
		{
			break;
		}
		
		child = child->p_nextsibling;
	}
	
	if (NULL == child || child->p_pid != pid)
	{
		if (pid_use[pid])
		{
			lock_release(procdata_lock);
			return ECHILD;
		}
		else
		{
			lock_release(procdata_lock);
		
			return ESRCH;
		}
	}

	while (child->p_exited == 0)
       	{
		cv_wait(procdata_cv, procdata_lock);
	}

	exitstatus = child->p_exit_code;
	DEBUG(DB_PROCSYS, "Child (%d) exited (Code %d)\n", pid, exitstatus);
	
	lock_release(procdata_lock);
#else
	/* this is just a stub implementation that always reports an
		 exit status of 0, regardless of the actual exit status of
		 the specified process.   
		 In fact, this will return 0 even if the specified process
		 is still running, and even if it never existed in the first place.
		 Fix this!
	*/

	if (options != 0) {
		return(EINVAL);
	}
	/* for now, just pretend the exitstatus is 0 */
	exitstatus = 0;

#endif // Optional for ASSGN2

	result = copyout((void *)&exitstatus,status,sizeof(int));
	if (result) {
		return(result);
	}
	*retval = pid;
	return(0);
}

#if OPT_A2

int
sys_fork(pid_t *retval, struct trapframe *tf)
{
	DEBUG(DB_PROCSYS, "Syscall: fork\n");

	lock_acquire(procdata_lock);

	int pid = procdata_find_free_pid(curproc->p_data);

	if (pid < 0)
	{
		lock_release(procdata_lock);
		DEBUG(DB_PROCSYS, "No PID Available\n");
		*retval = -1;
		return ENPROC;
	}

	pid_use[pid] = true;

	lock_release(procdata_lock);

	DEBUG(DB_PROCSYS, "New PID: %d\n", pid);

	struct proc *proc = proc_create_runprogram2(curproc->p_name);
	if (NULL == proc)
	{
		lock_acquire(procdata_lock);
		pid_use[pid] = false;
		lock_release(procdata_lock);
		*retval = -1;
		
		return ENOMEM;
	}

	procdata_t *procdata = procdata_create(pid, curproc->p_data);
	
	if (NULL == procdata)
	{
		proc_destroy(proc);
		lock_acquire(procdata_lock);
		pid_use[pid] = false;

		lock_release(procdata_lock);
		*retval = -1;
		
		return ENOMEM;
	}

	proc->p_data = procdata;

	struct addrspace *as = NULL;
	as_copy(curproc->p_addrspace, &as);
	
	if (NULL == as)
	{
		proc_destroy(proc);
		procdata_destroy(procdata);
		lock_acquire(procdata_lock);
		pid_use[pid] = false;
		lock_release(procdata_lock);
		*retval = -1;
		
		return ENOMEM;
	}

	proc->p_addrspace = as;

	struct trapframe *tf_copy = kmalloc(sizeof(struct trapframe));
	memcpy((void *)tf_copy, (const void*) tf, sizeof(struct trapframe));

	int result = thread_fork(curthread->t_name, proc, sys_fork_new_process, tf_copy, 0);
	if (result == 1)
       	{
		kprintf("thread_fork failed: %s\n", strerror(result));
		proc_destroy(proc);
		procdata_destroy(procdata);
		as_destroy(as);
		kfree(tf_copy);
		lock_acquire(procdata_lock);
		pid_use[pid] = false;
		lock_release(procdata_lock);
		*retval = -1;
		return result;
	}

	*retval = procdata->p_pid;

	return 0;
}

void
sys_fork_new_process(void *ptr, unsigned long nargs)
{
	(void)nargs;
	
	as_activate();

	struct trapframe tf;
	struct trapframe *tf_copy = (struct trapframe *) ptr;
	memcpy((void *)&tf, (const void*) tf_copy, sizeof(struct trapframe));
	kfree(tf_copy);

	enter_forked_process(&tf);
}

int
sys_execv(int *retval, userptr_t program, userptr_t args)
{
	int result;

	*retval = -1;

	int argc = 0;
	char **argv = execv_copyin_args(program, args, &argc);

	if (NULL == argv)
       	{
		return E2BIG;
	}
	
	result = runprogram(argc, argv, true);

	runprogram_argv_destroy(argc, argv);
	return result;
}

char **
execv_copyin_args(userptr_t program, userptr_t args, int *argc_return)
{
	int argc = 1; //(Start with 1 for program)
	char **args_ptr = (char **)args;
	while (NULL != *args_ptr) 
	{
		argc++;
		args_ptr++;
	}

	char **argv = kmalloc(sizeof(char *) * (argc + 1));
	if (NULL == argv)
       	{
		return NULL;
	}

	argv[0] = kstrdup((const char*)program);

	if (NULL == argv[0])
       	{
		kfree(argv);

		return NULL;
	}

	for (int ii = 1; ii < argc + 1; ii++)
       	{
		argv[ii] = NULL;
	}

	bool fail = false;
	args_ptr = (char **)args;

	for (int ii = 1; ii < argc; ii++)
       	{
		argv[ii] = kstrdup((const char*) args_ptr[ii - 1]);
		
		if (NULL == argv[ii])
	       	{
			fail = true;
			break;
		}
	}
	if (1 == fail)
       	{
		runprogram_argv_destroy(argc, argv);
		return NULL;
	}

	*argc_return = argc;

	return argv;
}

#endif // Optional for ASSGN2


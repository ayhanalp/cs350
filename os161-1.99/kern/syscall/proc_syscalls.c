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
#include <kern/fcntl.h>
#include <test.h>
#include <vm.h>
#include "autoconf.h"
#include "opt-A2.h"
#include <synch.h>
#include <limits.h>//PATH_MAX
#include <vfs.h>
#include <mips/trapframe.h>
#include <pid.h>
#include "op-A2.h"

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

// Ayhan Alp Aydeniz - aaaydeni



void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;

#if OPT_A2
  pid_exit(exitcode);
  
  // see the functions in ASSGN1

  cv_broadcast (p->p_waitcv, p->waitlk);
  lock_acquire (p->p_exit_lk);
  lock_release (p->p_exit_l);
#else
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;
#endif /* Optional for ASSGN2 */

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


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{

#if OPT_A2
	*retval = curproc->p_pid;

#else
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = 1;
#endif /* Optional for ASSGN2 */
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

#if OPT_A2
	result = pid_wait(pid, &exitstatus);

  if (result == 1)
  {
    return(result);
  }

  exitstatus = _MKWAIT_EXIT(exitstatus);


#else
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
#endif /* Optional for ASSGN */
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

#if OPT_A2

int
sys_fork(struct trapframe *trp_frm, pid_t *return_val)
{
	struct addrspace *addr_spc_child;
	struct trapframe *trp_frm_copy;
	struct proc *prc_child;
	int thread_error;

	prc_child = proc_create_runprogram(curproc->p_name);

	if (NULL == prc_chil)
	{
		return ENONEM;
	}
	
	trp_frm_copy = kmalloc(sizeof (struct trapframe));
	if (NULL == trp_frm_copy)
	{
		proc_destroy(prc_child);
		
		return ENOMEN;
	}

	thread_error = as_copy(curproc_getas(), &as_child);
	if (thread_error == 1)
	{
		proc_destroy(prc_child);
		pid_fail();

		return thread_error;
	}

	memcpy(trp_frm_copy, trp_frm, sizeof(struct trapframe));

	thread_error = thread_fork(curthread->t_name, prc_child, &enter_forked_prc, trp_frm_copy,
			(unsigned long) addr_spc_child);

	if (thread_error == 1)
	{
		as_destroy(addr_spc_child);
	
		kfree (trp_frm_copy);

		proc_destroy(prc_child);

		return thread_error;
	}

	lock_acquire(prc_child->p_exit_lk);

	*return_val = prc_child->p_pid;

	return 0;
	
}

#endif /* Optional for ASSGN2 */

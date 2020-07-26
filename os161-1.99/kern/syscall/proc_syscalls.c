
#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <mips/trapframe.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include "opt-A2.h"
#include "opt-A3.h"
#include <synch.h>
#include <limits.h>//PATH_MAX
#include <vfs.h>
#include <kern/fcntl.h>
#include <test.h>
#include <vm.h>
#include "autoconf.h"
  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

// Ayhan Alp Aydeniz - aaaydeni
// See the functions implemented in ASSGN1

#if OPT_A3
void _sys__exit(int exitcode) {
#else
void sys__exit(int exitcode) {
#endif // OPT_A3

  struct addrspace *as;
  struct proc *p = curproc;

  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  
	/*  (void)exitcode; */

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);

  for (unsigned ii = array_num(&p->p_arr_child); ii > 0; ii--)
  {
	struct proc *pr = array_get(&p->p_arr_child, ii - 1);
    
	lock_release(pr->p_lk_exit);

	array_remove(&p->p_arr_child, ii - 1);
  }

  KASSERT(array_num(&p->p_arr_child) == 0);

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

#if OPT_A2


#if OPT_A3
  p->p_exitcode = exitcode;
#else
  p->p_exitcode = _MKWAIT_EXIT( exitcode);
#endif // OPT_A3  
  
  p->p_exitcode = true;

  cv_broadcast(p->p_cv_wait, p->p_lk_wait);
  
  lock_acquire(p->p_lk_exit);
  lock_release(p->p_lk_exit);


#endif /* Optional for ASSGN2 */

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

void terminate_exit(int sig) {
	_sys__exit(_MKWAIT_SIG(sig));
}

#endif // OPT_A3


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */

#if OPT_A2
  *retval = curproc->p_proc_id;
  return (0);

#else
  *retval = 1;
  return(0);

#endif /* Optional for ASSGN2 */

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


#if OPT_A2
  struct proc * waitproc = find_proc_w_proc_id(pid);
  
  if (NULL == waitproc)
  {
	return ESRCH;
  }
  if (waitproc == curproc)
  {
	return ECHILD;
  }

#endif /* Optional for ASSGN2 */

  if (options != 0) 
  {
	return EINVAL;
  }

#if OPT_A2
  
  lock_acquire(waitproc->p_lk_wait);
  
  while(1)
  {
	  if (!waitproc->p_iSexit)
	  {
    		cv_wait(waitproc->p_cv_wait, waitproc->p_lk_wait);
	}

	  else
	  {
	  break;
	  }
  }
  lock_release(waitproc->p_lk_wait);
  exitstatus = waitproc->p_iSexit;

#else

  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;

#endif /* Optional for ASSGN2 */
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}


#if OPT_A2
pid_t sys_fork(pid_t *return_val,struct trapframe *trp_frm){
  int error_thread;
  
  struct proc * prc_child = proc_create_runprogram(curproc->p_name);
  
  if(NULL == prc_child)
  {
	return ENOMEM;
  }
  
  as_copy(curproc_getas(), &(prc_child->p_addrspace));
  
  if (NULL == prc_child->p_addrspace)
  {
	proc_destroy(prc_child);
	return ENOMEM;
  }
  
  struct trapframe *child_trp_frm = kmalloc(sizeof(struct trapframe));
  
  if(NULL == child_trp_frm)
  {
	proc_destroy(prc_child);
	
	return ENOMEM;
  }

  memcpy(child_trp_frm, trp_frm, sizeof(struct trapframe));
  error_thread = thread_fork(curthread->t_name, prc_child, &enter_forked_process, child_trp_frm,0);
  if(error_thread == 1)
  {
  	proc_destroy(prc_child);
  	kfree(child_trp_frm);
  	child_trp_frm = NULL;
  	
	return error_thread;
  }
  
  array_add(&curproc->p_arr_child, prc_child, NULL);
  
  lock_acquire(prc_child->p_lk_exit);

  
  *return_val = prc_child->p_proc_id;
  return 0;
}

#endif /* Optional for ASSGN2 */


#if OPT_A2

int sys_execv(const char * p_name, char ** args)
{
  vaddr_t entrypoint, stackptr;
  int result;

  (void)args;

  struct addrspace *addr_spc;
  struct vnode *v_node;

  // copy over program name into kernel space
  size_t prgrm_nm_sz = (strlen(p_name)+1)*sizeof(char);

  char * prgrm_nm_kern = kmalloc(prgrm_nm_sz);
  KASSERT(prgrm_nm_kern != NULL);
  
  int error = copyin((const_userptr_t) p_name, (void *) prgrm_nm_kern, prgrm_nm_sz);
  
  KASSERT(error == 0);


  // count number of args and copy into kernel
  int nargs = 0;
  for (int ii = 0; args[ii] != NULL; ii++)
  {
    nargs++;
  }

  // copy program args into kernel
  size_t args_arr_sz = (nargs+1)*sizeof(char *);
  char ** args_kern = kmalloc(args_arr_sz);
  KASSERT(args_kern != NULL);

  for (int ii = 0; ii <= nargs; ii++)
  {
    if (ii == nargs)
    {
      args_kern[ii] = NULL;
    }
    else
    {
      size_t arg_sz = (strlen(args[ii])+1)*sizeof(char);
      args_kern[ii] = kmalloc(arg_sz);
      KASSERT(args_kern[ii] != NULL);
      int error = copyin((const_userptr_t) args[ii], (void *) args_kern[ii], arg_sz);
      KASSERT(error == 0);
    }
  }


  /* Open the file. */
  result = vfs_open(prgrm_nm_kern, O_RDONLY, 0, &v_node);
  if (result == 1)
  {
  	return result;
  }

  /* We should be a new process. */
  /* KASSERT(curproc_getas() == NULL); */

  /* Create a new address space. */
  addr_spc = as_create();
  if (NULL == addr_spc)
  {
	vfs_close(v_node);
	return ENOMEM;
  }

  /* Switch to it and activate it. */
  struct addrspace * x_addr_spc = curproc_setas(addr_spc);
  as_activate();

  /* Load the executable. */
  result = load_elf(v_node, &entrypoint);
  if (result == 1)
  {
	/* p_addrspace will go away when curproc is destroyed */
	vfs_close(v_node);
	return result;
  }

  /* Done with the file now. */
  vfs_close(v_node);

  /* Define the user stack in the address space */
  result = as_define_stack(addr_spc, &stackptr);
  if (result == 1)
  {
	/* p_addrspace will go away when curproc is destroyed */
	return result;
  }

  // CP ARGS USR_STACK
  vaddr_t tmp_stack_ptr = stackptr;
  vaddr_t *stack_args = kmalloc((nargs+1)*sizeof(vaddr_t));

  for (int ii = nargs; ii >= 0; ii--)
  {
    if (ii == nargs)
    {
      stack_args[ii] = (vaddr_t) NULL;
      continue;
    }
    size_t arg_len = ROUNDUP(strlen(args_kern[ii])+1, 4);
    size_t arg_sz = arg_len*sizeof(char);
    tmp_stack_ptr = tmp_stack_ptr - arg_sz;
    int error = copyout((void *) args_kern[ii], (userptr_t) tmp_stack_ptr, arg_len);
    KASSERT(error == 0);
    stack_args[ii] = tmp_stack_ptr;
  }

  for (int ii = nargs; ii >= 0; ii--)
  {
    size_t str_ptr_sz = sizeof(vaddr_t);
    tmp_stack_ptr = tmp_stack_ptr - str_ptr_sz;
    int error = copyout((void *) &stack_args[ii], (userptr_t) tmp_stack_ptr, str_ptr_sz);
    KASSERT(error == 0);
  }
  // CP ARGS USR_STACK

  as_destroy(x_addr_spc);
  kfree(prgrm_nm_kern);
  // might want to free args_kernel
  for (int ii = 0; ii <= nargs; ii++)
  {
    kfree(args_kern[ii]);
  }
  kfree(args_kern);

	/* Warp to user mode. */
	enter_new_process(nargs /*argc*/, (userptr_t) tmp_stack_ptr /*userspace addr of argv*/,
			  ROUNDUP(tmp_stack_ptr, 8), entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
  return EINVAL;

}

#endif /* Optional for ASSGN2 */

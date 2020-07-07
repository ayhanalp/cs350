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

void sys__exit(int exitcode) {

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

  p->p_exitcode = _MKWAIT_EXIT( exitcode);
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

int sys_execv(const char *program, char **uargs) {
    struct addrspace *as_new;
    struct addrspace *as_old;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;
    size_t size;

    char *program_kernel;
    size_t program_size;
    char **uargs_kernel;
    vaddr_t *uargs_user;
    size_t uargs_size;

    // ensure valid program and arguments
    if (program == NULL || uargs == NULL) {
        return(EFAULT);
    }

    // copy program from user space into kernel space
    program_kernel = (char *) kmalloc(sizeof(char) * PATH_MAX);
    if (program_kernel == NULL) {
        return(ENOMEM);
    }
    result = copyinstr((const_userptr_t) program, program_kernel, PATH_MAX, &program_size);
    if (result || program_size <= 1) {
       kfree(program_kernel);
       return(EINVAL);
    }

    // copy arguments from user space into kernel space
    uargs_size = 0;
    while (uargs[uargs_size] != NULL) {
        uargs_size ++;
    }

    uargs_kernel = (char **) kmalloc(sizeof(char *) * (uargs_size + 1));
    for (size_t i = 0; i < uargs_size; ++i) {
        uargs_kernel[i] = (char *) kmalloc(sizeof(char) * PATH_MAX);
        result = copyinstr((const_userptr_t) uargs[i], uargs_kernel[i], PATH_MAX, &size);
        if (result) {
            kfree(program_kernel);
            kfree(uargs_kernel);
            return(EFAULT);
        }
    }
    uargs_kernel[uargs_size] = NULL;

	// open the file
	result = vfs_open(program_kernel, O_RDONLY, 0, &v);
	if (result) {
        kfree(program_kernel);
        kfree(uargs_kernel);
		return result;
	}

    // create new address space
    as_new = as_create();
    if (as_new == NULL) {
        kfree(program_kernel);
        kfree(uargs_kernel);
        vfs_close(v);
        return(ENOMEM);
    }

    // set new address space, delete old address space, and activate new address space
    as_old = curproc_setas(as_new);
    as_destroy(as_old);
    as_activate();

    // load the executable
	result = load_elf(v, &entrypoint);
	if (result) {
		// p_addrspace will go away when curproc is destroyed
        kfree(program_kernel);
        kfree(uargs_kernel);
		vfs_close(v);
		return result;
	}

    // done with the file now
    vfs_close(v);

    // define the user stack in the address space
	result = as_define_stack(as_new, &stackptr);
	if (result) {
		// p_addrspace will go away when curproc is destroyed
        kfree(program_kernel);
        kfree(uargs_kernel);
		return result;
	}

    // copy argument strings into user space, and keep track of virtual address
    uargs_user = (vaddr_t *) kmalloc(sizeof(vaddr_t) * (uargs_size + 1));
    for (size_t i = 0; i < uargs_size; ++i) {
        size = ROUNDUP(strlen(uargs_kernel[i]) + 1, 8);
        stackptr -= size;
        result = copyoutstr((const char *) uargs_kernel[i], (userptr_t) stackptr, size, &size);
        if (result) {
            kfree(program_kernel);
            kfree(uargs_kernel);
            kfree(uargs_user);
            return result;
        }

        uargs_user[i] = stackptr;
    }
    uargs_user[uargs_size] = (vaddr_t) NULL;

    // copy argument array of virtual address into user space
    size = sizeof(vaddr_t) * (uargs_size + 1);
    stackptr -= ROUNDUP(size, 8);
    result = copyout((const void *) uargs_user, (userptr_t) stackptr, size);
    if (result) {
        kfree(program_kernel);
        kfree(uargs_kernel);
        kfree(uargs_user);
    }

    // free kernel space program and arguments
    kfree(program_kernel);
    kfree(uargs_kernel);

	// warp to user mode
	enter_new_process(uargs_size /*argc*/,
            (userptr_t) stackptr /*userspace addr of argv*/,
            stackptr,
            entrypoint);

	// enter_new_process does not return
	panic("enter_new_process returned\n");
	return(EINVAL);
}

#endif /* OPT_A2 */


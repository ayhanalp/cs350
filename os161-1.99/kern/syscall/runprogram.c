/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>

#if OPT_A2
#include <copyinout.h>
#endif /* Optional for ASSGN2 */

// Ayhan Alp Aydeniz - aaaydeni

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */

#if OPT_A2
int
runprogram(char *progname, char ** args, int nargs)

#else
runprogram(char *progname)
#endif /* Optional for ASSGN2 */
{
	struct addrspace *addr_spc;
	struct vnode *v_node;
	vaddr_t entrypoint, stackptr;
	int result;

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, 0, &v_node);
	
	if (result == 1)
	{
		return result;
	}

	/* We should be a new process. */
	KASSERT(NULL == curproc_getas());

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
#if OPT_A2
  //HARD PART: COPY ARGS TO USER STACK
  char ** args_kern = args;
  vaddr_t tmp_stack_ptr = stackptr;
  vaddr_t *stack_args = kmalloc((nargs + 1) * sizeof(vaddr_t));

  for (int ii = nargs; ii >= 0; ii--)
  {
    if (ii == nargs)
    {
      stack_args[ii] = (vaddr_t) NULL;
      continue;
    }
    size_t arg_len = ROUNDUP(strlen(args_kern[ii]) + 1, 4);
    size_t arg_sz = arg_len * sizeof(char);
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
  // HARD PART: COPY ARGS TO USER STACK
  enter_new_process(nargs /*argc*/, (userptr_t) tmp_stack_ptr /*userspace addr of argv*/,
			  ROUNDUP(tmp_stack_ptr, 8), entrypoint);
  as_destroy(x_addr_spc);
#else
	/* Warp to user mode. */
	enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
			  stackptr, entrypoint);
#endif /* Optional for ASSGN2 */

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}

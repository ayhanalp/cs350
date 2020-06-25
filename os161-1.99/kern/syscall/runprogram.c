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
#include <copyinout.h> // Added for ASSGN2

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */

// Ayhan Alp Aydeniz - aaaydeni

#if OPT_A2
	int
        runprogram(char *progname, char **args)	

#else
int
runprogram(char *progname)

#endif /* Optional for ASSGN2 */
{

#if OPT_A2

	size_t args_sz;
	size_t addr_size;

	vaddr_t *args_usr;

#endif /* Optional for ASSGN2 */

	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* We should be a new process. */
	KASSERT(curproc_getas() == NULL);

	/* Create a new address space. */
	as = as_create();
	if (as ==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	curproc_setas(as);
	as_activate();

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}

#if OPT_A2

	args_sz = 0;

	while (1)
	{
		if (NULL != args[args_sz])
		{
			args_sz = args_sz + 1;
		}

		else
		{
			break;
		}
	}

	args_usr = (vaddr_t *)kmalloc((args_sz+1)*sizeof(vaddr_t));

	for (size_t ii = 0; ii < args_sz; ii++)
	{
		addr_size = ROUNDUP (strlen(args[ii])+1, 8);

		stackptr = stackptr - addr_size;
		
		// USAGE
		// int copyoutstr(const char *src, userptr_t userdest, size_t len, size_t *got);
		result = copyoutstr((const char *) args[ii], (userptr_t) stackptr, addr_size, &addr_size);
	
		if (result == 1)
		{
			kfree(args_usr);
			return result;
		}

		args_usr[ii] = stackptr;
	}

	args_usr[args_sz] = NULL;

	addr_size = (args_sz+1) * sizeof(vaddr_t);


	stackptr = stackptr - ROUNDUP(addr_size, 8);

	// USAGE
	// int copyout(const void *src, userptr_t userdest, size_t len);
	
	result = copyout((const void *) args_usr, (userptr_t) stackptr, addr_size);

	if (result == 1)
	{
		kfree (args_usr);
	}

	/* Warp to user mode. */

	enter_new_process(args_sz, (userptr_t) stackptr, stackptr, entrypoint);

	
#else


	/* Warp to user mode. */
	enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
			stackptr, entrypoint);

#endif /* Optional for ASSGN2 */

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}


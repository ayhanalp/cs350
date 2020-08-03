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

/* Ayhan Alp Aydeniz - aaaydeni */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <syscall.h>
#include "opt-A3.h"

/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground. You should replace all of this
 * code while doing the VM assignment. In fact, starting in that
 * assignment, this file is not included in your kernel!
 */

/* under dumbvm, always have 48k of user stack */
#define DUMBVM_STACKPAGES    12

/*
 * Wrap rma_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

#if OPT_A3

static int frm_1st_free;
static bool isVMready = false;
static paddr_t mem_start_addr;
static paddr_t mem_end_addr;
static uint32_t *frm_sz;
static uint32_t *frm_used;
static int frm_max;

#endif // Optional for ASSGN3

void
vm_bootstrap(void)
{
#if OPT_A3

	ram_getsize(&mem_start_addr, &mem_end_addr);

	frm_used = (uint32_t *) PADDR_TO_KVADDR(mem_start_addr);
	mem_start_addr = mem_start_addr + sizeof(uint32_t) * PAGE_SIZE;

	frm_max = (mem_end_addr - mem_start_addr) / (PAGE_SIZE + 4);

	mem_end_addr = mem_end_addr - sizeof(uint32_t) * frm_max;
	frm_sz = (uint32_t *) PADDR_TO_KVADDR(mem_end_addr);


	frm_1st_free = 0;


	for (int ii = 0; ii < frm_max / 32; ii++)
	{
		frm_used[ii] = 0;
	}

	for (int ii = 0; ii < frm_max; ii++)
	{
		frm_sz[ii] = 0;
	}

	kprintf("vm: %d frames available \n", frm_max);
	isVMready = true;

#endif // Optional for ASSGN3
}

#if OPT_A3

static
bool
frm_get_used(int frm_num)
{
	KASSERT(frm_num < frm_max);
	int indX = frm_num / 32;
	uint32_t msk = ((uint32_t) 1) << (frm_num % 32);
	
	return (frm_used[indX] & msk) != 0;
}

static
void
frm_set_used(int frm_num, bool isUsed)
{
	KASSERT(frm_num < frm_max);
	int indX = frm_num / 32;
	uint32_t msk = ((uint32_t) 1) << (frm_num % 32);

	if (isUsed == 1)
	{
		frm_used[indX] = frm_used[indX] | msk;
	}

	else
	{
		frm_used[indX] = frm_used[indX] &  ~msk;
	}
}

static
paddr_t
allocate_frms(int num_pages)
{
	int frm_num;

	spinlock_acquire(&stealmem_lock);

	frm_num = frm_1st_free;

	while (1)
	{
		if (frm_num < frm_max)
		{
			if (frm_get_used(frm_num) == 0)
			{
			
				bool yeter = true;
			
				for (int ii = 1; ii < num_pages; ii++)
				{
					if (frm_get_used(frm_num + ii))
					{
						yeter = false;
					
						frm_num = frm_num + (ii + 1);

						break;
					}
				}
			
				if (yeter == 1)
				{
					for (int ii = 0; ii < num_pages; ii++)
					{
						frm_set_used(frm_num + ii, true);
						frm_sz[frm_num + ii] = 0;
					}
				
					frm_sz[frm_num] = num_pages;
				
					if (frm_num == frm_1st_free)
					{
						for (int ii = frm_num + 1; ii < frm_max; ii++)
						{
							if (frm_get_used(ii) == 0)
							{
								frm_1st_free = ii;
								
								break;
							}
						}
					}
					
					spinlock_release(&stealmem_lock);
				
					return mem_start_addr + (frm_num * PAGE_SIZE);
				}
			}

			else
			{
				frm_num = frm_num + 1;
			}
		}
		
		else
		{
			break;
		}
	}

	spinlock_release(&stealmem_lock);

	return 0;
}

static
void
freeFrms(paddr_t frm)
{
	int frm_num = (frm - mem_start_addr) / PAGE_SIZE;
	
	int num_pages;

	KASSERT(frm_num >= 0);
	KASSERT((frm - mem_start_addr) % PAGE_SIZE == 0);
	
	KASSERT(frm_num < frm_max);
	KASSERT(frm_get_used(frm_num));

	spinlock_acquire(&stealmem_lock);

	num_pages = frm_sz[frm_num];

	for (int ii = 0; ii < num_pages; ii++)
	{
		frm_set_used(frm_num + ii, false);
		frm_sz[frm_num + ii] = 0;
	}

	if (frm_num < frm_1st_free)
	{
		frm_1st_free = frm_num;
	}

	spinlock_release(&stealmem_lock);
}

static
void
freeMultiFrms(paddr_t * p_frms, int num_pages)
{
	for (int ii = 0; ii < num_pages; ii++)
	{
		freeFrms(p_frms[ii]);
	}

	kfree(p_frms);
}

static
paddr_t *
allocate_mlt_frms(int num_pages)
{
	paddr_t *res;

	res = kmalloc(num_pages * sizeof(paddr_t *));
	
	if (NULL == res)
	{
		return 0;
	}

	for (int ii = 0; ii < num_pages; ii++)
	{
		res[ii] = allocate_frms(1);
		if (res[ii] == 0)
		{
			freeMultiFrms(res, ii);
			
			return 0;
		}
	}

	return res;
}

#endif // Optional for ASSGN3

static
paddr_t
getppages(unsigned long npages)
{
	paddr_t addr;

	spinlock_acquire(&stealmem_lock);

	addr = ram_stealmem(npages);
	
	spinlock_release(&stealmem_lock);
	return addr;
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t 
alloc_kpages(int npages)
{
	paddr_t pa;

#if OPT_A3

	if (isVMready) {
		pa = allocate_frms(npages);
	}
	else {
		pa = getppages(npages);
	}

#else

	pa = getppages(npages);

#endif // Optional for ASSGN3
	
	if (pa == 0)
	{
		return 0;
	}

	return PADDR_TO_KVADDR(pa);
}

void 
free_kpages(vaddr_t addr)
{

#if OPT_A3
	
	freeFrms(KVADDR_TO_PADDR(addr));

#endif // Optional for ASSGN3
}

void
vm_tlbshootdown_all(void)
{
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

#if OPT_A3
	
	uint32_t re_wr;

#endif // Optional for ASSGN3

	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
#if OPT_A3

    		    return EFAULT;

#else

    		    /* We always create pages read-write, so we can't get this */

    		    panic("dumbvm: got VM_FAULT_READONLY\n");

#endif // Optional for ASSGN3

	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

	if (NULL == curproc)
	{
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = curproc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
#if OPT_A3
	
	KASSERT(as->as_vbase1 != 0);
	KASSERT(NULL != as->as_pbase1);
	KASSERT(as->as_npages1 != 0);
	
	KASSERT(as->as_vbase2 != 0);
	KASSERT(NULL != as->as_pbase2);
	KASSERT(as->as_npages2 != 0);
	
	KASSERT(as->as_stackpbase != NULL);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	
	for (int ii = 0; ii < (int) as->as_npages1; ii++)
	{
		KASSERT((as->as_pbase1[ii] & PAGE_FRAME) == as->as_pbase1[ii]);
	}
	
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	
	for (int ii = 0; ii < (int) as->as_npages2; ii++)
	{
		KASSERT((as->as_pbase2[ii] & PAGE_FRAME) == as->as_pbase2[ii]);
	}
	
	for (int ii = 0; ii < DUMBVM_STACKPAGES; ii++)
	{
		KASSERT((as->as_stackpbase[ii] & PAGE_FRAME) == as->as_stackpbase[ii]);
	}

#else
	KASSERT(as->as_vbase1 != 0);
	KASSERT(as->as_pbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	KASSERT(as->as_pbase2 != 0);
	KASSERT(as->as_npages2 != 0);
	KASSERT(as->as_stackpbase != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

#endif // Optional for ASSGN3

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	if (faultaddress >= vbase1 && faultaddress < vtop1)
	{

#if OPT_A3

		re_wr = 0;
		
		int frm = (faultaddress - vbase1) / PAGE_SIZE;
		
		paddr = as->as_pbase1[frm];

#else

		paddr = (faultaddress - vbase1) + as->as_pbase1;

#endif // Optional for ASSGN3
	
	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2)
	{

#if OPT_A3

		re_wr = TLBLO_DIRTY;
		
		int frm = (faultaddress - vbase2) / PAGE_SIZE;
		
		paddr = as->as_pbase2[frm];

#else

		paddr = (faultaddress - vbase2) + as->as_pbase2;

#endif // Optional for ASSGN3
	
	}

	else if (faultaddress >= stackbase && faultaddress < stacktop)
	{

#if OPT_A3
		re_wr = TLBLO_DIRTY;
		
		int frm = (faultaddress - stackbase) / PAGE_SIZE;
		
		paddr = as->as_stackpbase[frm];

#else
		paddr = (faultaddress - stackbase) + as->as_stackpbase;

#endif // Optional for ASSGN3
	
	}
	
	else 
	{
		return EFAULT;
	}

	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

#if OPT_A3
	
	if (as->init == 1)
	{
		re_wr = TLBLO_DIRTY;
	}

#endif // Optional for ASSGN3

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (int ii = 0; ii < NUM_TLB; ii++)
	{
		tlb_read(&ehi, &elo, ii);
		if (elo & TLBLO_VALID)
		{
			continue;
		}
		
		ehi = faultaddress;

#if OPT_A3

		elo = paddr | re_wr | TLBLO_VALID;

#else

		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;

#endif // Optional for ASSGN3

		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, ii);
		splx(spl);
		return 0;
	}

#if OPT_A3

	ehi = faultaddress;
	elo = paddr | re_wr | TLBLO_VALID;
	DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
	tlb_random(ehi, elo);
	splx(spl);
	return 0;

#else

	kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
	splx(spl);
	return EFAULT;

#endif // Optional for ASSGN3
}

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

#if OPT_A3

	as->as_stackpbase = NULL;
        as->init = false;

	as->as_vbase1 = 0;
	as->as_pbase1 = NULL;
	as->as_npages1 = 0;
	
	as->as_vbase2 = 0;
	as->as_pbase2 = NULL;
	as->as_npages2 = 0;

#else
	as->as_vbase1 = 0;
	as->as_pbase1 = 0;
	as->as_npages1 = 0;
	as->as_vbase2 = 0;
	as->as_pbase2 = 0;
	as->as_npages2 = 0;
	as->as_stackpbase = 0;

#endif // Optional for ASSGN3

	return as;
}

void
as_destroy(struct addrspace *as)
{

#if OPT_A3

	freeMultiFrms(as->as_pbase1, as->as_npages1);
	freeMultiFrms(as->as_pbase2, as->as_npages2);
	freeMultiFrms(as->as_stackpbase, DUMBVM_STACKPAGES);

#endif // Optional for ASSGN3

	kfree(as);
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = curproc_getas();
#ifdef UW
        /* Kernel threads don't have an address spaces to activate */
#endif
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
/* nothing */

#if OPT_A3

	int SPL;

	SPL = splhigh();

	for (int ii = 0; ii < NUM_TLB; ii++)
	{
		tlb_write(TLBHI_INVALID(ii), TLBLO_INVALID(), ii);
	}

	splx(SPL);

#endif // Optional for ASSGN3
}

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	size_t npages; 

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* We don't use these - all pages are read-write */
	(void)readable;
	(void)writeable;
	(void)executable;

	if (as->as_vbase1 == 0) {
		as->as_vbase1 = vaddr;
		as->as_npages1 = npages;
		return 0;
	}

	if (as->as_vbase2 == 0) {
		as->as_vbase2 = vaddr;
		as->as_npages2 = npages;
		return 0;
	}

	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return EUNIMP;
}

#if OPT_A3

static
void
as_zero_region(paddr_t *paddr, unsigned npages)
{
	for(int ii = 0; ii < (int) npages; ii++) {
		bzero((void *)PADDR_TO_KVADDR(paddr[ii]), PAGE_SIZE);
	}
}

#else

static
void
as_zero_region(paddr_t paddr, unsigned npages)
{
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}

#endif // Optional for ASSGN3

int
as_prepare_load(struct addrspace *as)
{
#if OPT_A3

	KASSERT(as->as_pbase1 == NULL);

	KASSERT(as->as_pbase2 == NULL);

	KASSERT(as->as_stackpbase == NULL);

	as->as_pbase1 = allocate_mlt_frms(as->as_npages1);
	if (NULL == as->as_pbase1)
	{
		return ENOMEM;
	}

	as->as_pbase2 = allocate_mlt_frms(as->as_npages2);
	if (as->as_pbase2 == 0)
	{
		freeMultiFrms(as->as_pbase1, as->as_npages1);
		return ENOMEM;
	}

	as->as_stackpbase = allocate_mlt_frms(DUMBVM_STACKPAGES);
	if (as->as_stackpbase == 0)
	{
		freeMultiFrms(as->as_pbase1, as->as_npages1);
		freeMultiFrms(as->as_pbase2, as->as_npages2);
		return ENOMEM;
	}

	as->init = true;

#else
	KASSERT(as->as_pbase1 == 0);
	KASSERT(as->as_pbase2 == 0);
	KASSERT(as->as_stackpbase == 0);

	as->as_pbase1 = getppages(as->as_npages1);
	if (as->as_pbase1 == 0) {
		return ENOMEM;
	}

	as->as_pbase2 = getppages(as->as_npages2);
	if (as->as_pbase2 == 0) {
		return ENOMEM;
	}

	as->as_stackpbase = getppages(DUMBVM_STACKPAGES);
	if (as->as_stackpbase == 0) {
		return ENOMEM;
	}

#endif // Optional for ASSGN3
	
	as_zero_region(as->as_pbase1, as->as_npages1);
	as_zero_region(as->as_pbase2, as->as_npages2);
	as_zero_region(as->as_stackpbase, DUMBVM_STACKPAGES);

	return 0;
}

int
as_complete_load(struct addrspace *as)
{
#if OPT_A3

	as->init = false;

#endif // Optional for ASSGN3
	
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	KASSERT(as->as_stackpbase != 0);

	*stackptr = USERSTACK;
	return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *new;

	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

	new->as_vbase1 = old->as_vbase1;
	new->as_npages1 = old->as_npages1;
	new->as_vbase2 = old->as_vbase2;
	new->as_npages2 = old->as_npages2;

	/* (Mis)use as_prepare_load to allocate some physical memory. */
	if (as_prepare_load(new)) {
		as_destroy(new);
		return ENOMEM;
	}

#if OPT_A3

	KASSERT(new->as_pbase1 != NULL);
	
	KASSERT(new->as_pbase2 != NULL);
	
	KASSERT(new->as_stackpbase != NULL);

	for (int ii = 0; ii < (int)old->as_npages1; ii++)
	{
		memmove((void *)PADDR_TO_KVADDR(new->as_pbase1[ii]),
			(const void *)PADDR_TO_KVADDR(old->as_pbase1[ii]),
			PAGE_SIZE);
	}

	for (int ii = 0; ii < (int)old->as_npages2; ii++)
	{
		memmove((void *)PADDR_TO_KVADDR(new->as_pbase2[ii]),
			(const void *)PADDR_TO_KVADDR(old->as_pbase2[ii]),
			PAGE_SIZE);
	}

	for(int ii = 0; ii < DUMBVM_STACKPAGES; ii++)
	{
		memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase[ii]),
			(const void *)PADDR_TO_KVADDR(old->as_stackpbase[ii]),
			PAGE_SIZE);
	}

	as_complete_load(new);

#else

	KASSERT(new->as_pbase1 != 0);
	KASSERT(new->as_pbase2 != 0);
	KASSERT(new->as_stackpbase != 0);

	memmove((void *)PADDR_TO_KVADDR(new->as_pbase1),
		(const void *)PADDR_TO_KVADDR(old->as_pbase1),
		old->as_npages1*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_pbase2),
		(const void *)PADDR_TO_KVADDR(old->as_pbase2),
		old->as_npages2*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase),
		(const void *)PADDR_TO_KVADDR(old->as_stackpbase),
		DUMBVM_STACKPAGES*PAGE_SIZE);

#endif // Optional for ASSGN3
	
	*ret = new;
	return 0;
}

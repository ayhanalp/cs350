#ifndef _PID_H_
#define _PID_H_

#include "opt-A2.h"
#if OPT_A2

#include <limits.h>
#include <proc.h>
#include <synch.h>
#include <types.h>

struct pid_stat {
    pid_t p_parent_pid;
    int p_exitcode;
    struct cv *p_cv;

};

void pid_bootstrap(void);
void pid_assign_kern(struct proc *proc_kern);
void pid_assign_next(struct proc *proc_child);
int pid_wait(pid_t pid, int *exitstatus);
void pid_exit(int exitcode);
void pid_fail(void);

#endif /* Optional for ASSGN2 */
#endif /* _PID_H_ */

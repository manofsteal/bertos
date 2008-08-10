/**
 * \file
 * <!--
 * This file is part of BeRTOS.
 *
 * Bertos is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As a special exception, you may use this file as part of a free software
 * library without restriction.  Specifically, if other files instantiate
 * templates or use macros or inline functions from this file, or you compile
 * this file and link it with other files to produce an executable, this
 * file does not by itself cause the resulting executable to be covered by
 * the GNU General Public License.  This exception does not however
 * invalidate any other reasons why the executable file might be covered by
 * the GNU General Public License.
 *
 * Copyright 2001,2004 Develer S.r.l. (http://www.develer.com/)
 * Copyright 1999,2000,2001 Bernie Innocenti <bernie@codewiz.org>
 *
 * -->
 *
 * \brief Simple realtime multitasking scheduler.
 *        Context switching is only done cooperatively.
 *
 * \version $Id$
 * \author Bernie Innocenti <bernie@codewiz.org>
 * \author Stefano Fedrigo <aleph@develer.com>
 */

#include "proc_p.h"
#include "proc.h"

#include "cfg/cfg_arch.h"  /* ARCH_EMUL */
#include <cfg/debug.h>
#include <cfg/module.h>

// Log settings for cfg/log.h.
#define LOG_LEVEL   KERN_LOG_LEVEL
#define LOG_FORMAT  KERN_LOG_FORMAT
#include <cfg/log.h>

#include <cpu/irq.h>
#include <cpu/types.h>
#include <cpu/attr.h>
#include <cpu/frame.h>

#include <mware/event.h>

#include <string.h>           /* memset() */

/**
 * CPU dependent context switching routines.
 *
 * Saving and restoring the context on the stack is done by a CPU-dependent
 * support routine which usually needs to be written in assembly.
 */
EXTERN_C void asm_switch_context(cpustack_t **new_sp, cpustack_t **save_sp);

/*
 * The scheduer tracks ready processes by enqueuing them in the
 * ready list.
 *
 * \note Access to the list must occur while interrupts are disabled.
 */
REGISTER List ProcReadyList;

/*
 * Holds a pointer to the TCB of the currently running process.
 *
 * \note User applications should use proc_current() to retrieve this value.
 */
REGISTER Process *CurrentProcess;

#if CONFIG_KERN_PREEMPTIVE
/*
 * The time sharing scheduler forces a task switch when the current
 * process has exhausted its quantum.
 */
uint16_t Quantum;
#endif


#if (ARCH & ARCH_EMUL)
/*
 * In hosted environments, we must emulate the stack on the real process stack.
 *
 * Access to this list must be protected by PROC_ATOMIC().
 */
extern List StackFreeList;
#endif

/** The main process (the one that executes main()). */
struct Process MainProcess;


static void proc_init_struct(Process *proc)
{
	/* Avoid warning for unused argument. */
	(void)proc;

#if CONFIG_KERN_SIGNALS
	proc->sig_recv = 0;
#endif

#if CONFIG_KERN_PREEMPTIVE
	proc->forbid_cnt = 0;
#endif

#if CONFIG_KERN_HEAP
	proc->flags = 0;
#endif
}

MOD_DEFINE(proc);

void proc_init(void)
{
	LIST_INIT(&ProcReadyList);

	/*
	 * We "promote" the current context into a real process. The only thing we have
	 * to do is create a PCB and make it current. We don't need to setup the stack
	 * pointer because it will be written the first time we switch to another process.
	 */
	proc_init_struct(&MainProcess);
	CurrentProcess = &MainProcess;

#if CONFIG_KERN_MONITOR
	monitor_init();
	monitor_add(CurrentProcess, "main");
#endif

	MOD_INIT(proc);
}


/**
 * Create a new process, starting at the provided entry point.
 *
 * \return Process structure of new created process
 *         if successful, NULL otherwise.
 */
struct Process *proc_new_with_name(UNUSED(const char *, name), void (*entry)(void), iptr_t data, size_t stack_size, cpustack_t *stack_base)
{
	Process *proc;
	size_t i;
	const size_t PROC_SIZE_WORDS = ROUND2(sizeof(Process), sizeof(cpustack_t)) / sizeof(cpustack_t);
#if CONFIG_KERN_HEAP
	bool free_stack = false;
#endif
	TRACEMSG("name=%s", name);

#if (ARCH & ARCH_EMUL)
	/* Ignore stack provided by caller and use the large enough default instead. */
	PROC_ATOMIC(stack_base = (cpustack_t *)list_remHead(&StackFreeList));

	stack_size = CONFIG_PROC_DEFSTACKSIZE;
#elif CONFIG_KERN_HEAP
	/* Did the caller provide a stack for us? */
	if (!stack_base)
	{
		/* Did the caller specify the desired stack size? */
		if (!stack_size)
			stack_size = CONFIG_PROC_DEFSTACKSIZE + sizeof(Process);

		/* Allocate stack dinamically */
		if (!(stack_base = heap_alloc(stack_size)))
			return NULL;

		free_stack = true;
	}
#else
	/* Stack must have been provided by the user */
	ASSERT_VALID_PTR(stack_base);
	ASSERT(stack_size);
#endif

#if CONFIG_KERN_MONITOR
	/* Fill-in the stack with a special marker to help debugging */
#warning size incorrect
	memset(stack_base, CONFIG_KERN_STACKFILLCODE, stack_size / sizeof(cpustack_t));
#endif

	/* Initialize the process control block */
	if (CPU_STACK_GROWS_UPWARD)
	{
		proc = (Process*)stack_base;
		proc->stack = stack_base + PROC_SIZE_WORDS;
		if (CPU_SP_ON_EMPTY_SLOT)
			proc->stack++;
	}
	else
	{
		proc = (Process*)(stack_base + stack_size / sizeof(cpustack_t) - PROC_SIZE_WORDS);
		proc->stack = (cpustack_t*)proc;
		if (CPU_SP_ON_EMPTY_SLOT)
			proc->stack--;
	}

	proc_init_struct(proc);
	proc->user_data = data;

#if CONFIG_KERN_HEAP | CONFIG_KERN_MONITOR | (ARCH & ARCH_EMUL)
	proc->stack_base = stack_base;
	proc->stack_size = stack_size;
	#if CONFIG_KERN_HEAP
	if (free_stack)
		proc->flags |= PF_FREESTACK;
	#endif
#endif

	/* Initialize process stack frame */
	CPU_PUSH_CALL_FRAME(proc->stack, proc_exit);
	CPU_PUSH_CALL_FRAME(proc->stack, entry);

	/* Push a clean set of CPU registers for asm_switch_context() */
	for (i = 0; i < CPU_SAVED_REGS_CNT; i++)
		CPU_PUSH_WORD(proc->stack, CPU_REG_INIT_VALUE(i));

	/* Add to ready list */
	ATOMIC(SCHED_ENQUEUE(proc));
	ATOMIC(LIST_ASSERT_VALID(&ProcReadyList));

#if CONFIG_KERN_MONITOR
	monitor_add(proc, name);
#endif

	return proc;
}

/** Rename a process */
void proc_rename(struct Process *proc, const char *name)
{
#if CONFIG_KERN_MONITOR
	monitor_rename(proc, name);
#else
	(void)proc; (void)name;
#endif
}


/**
 * System scheduler: pass CPU control to the next process in
 * the ready queue.
 */
void proc_schedule(void)
{
	struct Process *old_process;
	cpuflags_t flags;

	ATOMIC(LIST_ASSERT_VALID(&ProcReadyList));
	ASSERT_USER_CONTEXT();
	ASSERT_IRQ_ENABLED();

	/* Remember old process to save its context later */
	old_process = CurrentProcess;

	/* Poll on the ready queue for the first ready process */
	IRQ_SAVE_DISABLE(flags);
	while (!(CurrentProcess = (struct Process *)list_remHead(&ProcReadyList)))
	{
		/*
		 * Make sure we physically reenable interrupts here, no matter what
		 * the current task status is. This is important because if we
		 * are idle-spinning, we must allow interrupts, otherwise no
		 * process will ever wake up.
		 *
		 * During idle-spinning, an interrupt can occur and it may
		 * modify \p ProcReadyList. To ensure that compiler reload this
		 * variable every while cycle we call CPU_MEMORY_BARRIER.
		 * The memory barrier ensure that all variables used in this context
		 * are reloaded.
		 * \todo If there was a way to write sig_wait() so that it does not
		 * disable interrupts while waiting, there would not be any
		 * reason to do this.
		 */
		IRQ_ENABLE;
		CPU_IDLE;
		MEMORY_BARRIER;
		IRQ_DISABLE;
	}
	IRQ_RESTORE(flags);

	/*
	 * Optimization: don't switch contexts when the active
	 * process has not changed.
	 */
	if (CurrentProcess != old_process)
	{
		cpustack_t *dummy;

		#if CONFIG_KERN_MONITOR
			LOG_INFO("Switch from %p(%s) to %p(%s)\n",
				old_process,    old_process ? old_process->monitor.name : "NONE",
				CurrentProcess, CurrentProcess->monitor.name);
		#endif

		#if CONFIG_KERN_PREEMPTIVE
			/* Reset quantum for this process */
			Quantum = CONFIG_KERN_QUANTUM;
		#endif

		/* Save context of old process and switch to new process. If there is no
		 * old process, we save the old stack pointer into a dummy variable that
		 * we ignore. In fact, this happens only when the old process has just
		 * exited.
		 * TODO: Instead of physically clearing the process at exit time, a zombie
		 * list should be created.
		 */
		asm_switch_context(&CurrentProcess->stack, old_process ? &old_process->stack : &dummy);
	}

	/* This RET resumes the execution on the new process */
}


/**
 * Terminate the current process
 */
void proc_exit(void)
{
	TRACE;

#if CONFIG_KERN_MONITOR
	monitor_remove(CurrentProcess);
#endif

#if CONFIG_KERN_HEAP
	/*
	 * The following code is BROKEN.
	 * We are freeing our own stack before entering proc_schedule()
	 * BAJO: A correct fix would be to rearrange the scheduler with
	 *  an additional parameter which frees the old stack/process
	 *  after a context switch.
	 */
	if (CurrentProcess->flags & PF_FREESTACK)
		heap_free(CurrentProcess->stack_base, CurrentProcess->stack_size);
	heap_free(CurrentProcess);
#endif

#if (ARCH & ARCH_EMUL)
#warning This is wrong
	/* Reinsert process stack in free list */
	PROC_ATOMIC(ADDHEAD(&StackFreeList, (Node *)(CurrentProcess->stack
		- (CONFIG_PROC_DEFSTACKSIZE / sizeof(cpustack_t)))));

	/*
	 * NOTE: At this point the first two words of what used
	 * to be our stack contain a list node. From now on, we
	 * rely on the compiler not reading/writing the stack.
	 */
#endif /* ARCH_EMUL */

	CurrentProcess = NULL;
	proc_schedule();
	/* not reached */
}


/**
 * Co-operative context switch
 */
void proc_switch(void)
{
	ATOMIC(SCHED_ENQUEUE(CurrentProcess));

	proc_schedule();
}


/**
 * Get the pointer to the current process
 */
struct Process *proc_current(void)
{
	return CurrentProcess;
}

/**
 * Get the pointer to the user data of the current process
 */
iptr_t proc_current_user_data(void)
{
	return CurrentProcess->user_data;
}


#if CONFIG_KERN_PREEMPTIVE

/**
 * Disable preemptive task switching.
 *
 * The scheduler maintains a per-process nesting counter.  Task switching is
 * effectively re-enabled only when the number of calls to proc_permit()
 * matches the number of calls to proc_forbid().
 *
 * Calling functions that could sleep while task switching is disabled
 * is dangerous, although supported.  Preemptive task switching is
 * resumed while the process is sleeping and disabled again as soon as
 * it wakes up again.
 *
 * \sa proc_permit()
 */
void proc_forbid(void)
{
	/* No need to protect against interrupts here. */
	++CurrentProcess->forbid_cnt;
}

/**
 * Re-enable preemptive task switching.
 *
 * \sa proc_forbid()
 */
void proc_permit(void)
{
	/* No need to protect against interrupts here. */
	--CurrentProcess->forbid_cnt;
}

#endif /* CONFIG_KERN_PREEMPTIVE */

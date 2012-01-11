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
 * Copyright 2012 Develer S.r.l. (http://www.develer.com/)
 * -->
 *
 * \brief RTask test.
 *
 *
 * \author Daniele Basile <asterix@develer.com>
 *
 * $test$: cp bertos/cfg/cfg_proc.h $cfgdir/
 * $test$: echo  "#undef CONFIG_KERN" >> $cfgdir/cfg_proc.h
 * $test$: echo "#define CONFIG_KERN 1" >> $cfgdir/cfg_proc.h
 * $test$: cp bertos/cfg/cfg_signal.h $cfgdir/
 * $test$: echo  "#undef CONFIG_KERN_SIGNALS" >> $cfgdir/cfg_signal.h
 * $test$: echo "#define CONFIG_KERN_SIGNALS 1" >> $cfgdir/cfg_signal.h
 *
 */

#include <cfg/test.h>

#include <drv/timer.h>

#include <kern/rtask.h>
#include <kern/proc.h>

static bool test1(void *data)
{
	kprintf("Funziona!\n");
	return true;
}

/**
 * Run rtask test
 */
int rtask_testRun(void)
{
	RTask *task = rtask_add(test1, 100, NULL);
	return (task != NULL ? 0 : -1);
}

int rtask_testSetup(void)
{
	kdbg_init();

	kprintf("Init Timer..");
	timer_init();
	kprintf("Done.\n");

	kprintf("Init Process..");
	proc_init();
	kprintf("Done.\n");
	return 0;
}

int rtask_testTearDown(void)
{
	kputs("TearDown rtask test.\n");
	return 0;
}

TEST_MAIN(rtask);

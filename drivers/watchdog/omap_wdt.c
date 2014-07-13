/*
 * omap_wdt.c
 *
 * Watchdog driver for the TI OMAP 16xx & 24xx/34xx 32KHz (non-secure) watchdog
 *
 * Author: MontaVista Software, Inc.
 *	 <gdavis@mvista.com> or <source@mvista.com>
 *
 * 2003 (c) MontaVista Software, Inc. This file is licensed under the
 * terms of the GNU General Public License version 2. This program is
 * licensed "as is" without any warranty of any kind, whether express
 * or implied.
 *
 * History:
 *
 * 20030527: George G. Davis <gdavis@mvista.com>
 *	Initially based on linux-2.4.19-rmk7-pxa1/drivers/char/sa1100_wdt.c
 *	(c) Copyright 2000 Oleg Drokin <green@crimea.edu>
 *	Based on SoftDog driver by Alan Cox <alan@lxorguk.ukuu.org.uk>
 *
 * Copyright (c) 2004 Texas Instruments.
 *	1. Modified to support OMAP1610 32-KHz watchdog timer
 *	2. Ported to 2.6 kernel
 *
 * Copyright (c) 2005 David Brownell
 *	Use the driver model and standard identifiers; handle bigger timeouts.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/moduleparam.h>
#include <linux/clk.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#ifdef CONFIG_OMAP_WATCHDOG_AUTOPET
#include <linux/timer.h>
#endif
#include <mach/hardware.h>
#include <plat/prcm.h>

#include "omap_wdt.h"

typedef enum close_state {
	CLOSE_STATE_NOT,
	CLOSE_STATE_ALLOW = 0x4021
} close_state_t;


static struct platform_device *omap_wdt_dev;

static unsigned timer_margin;
module_param(timer_margin, uint, 0);
MODULE_PARM_DESC(timer_margin, "initial watchdog timeout (in seconds)");

static unsigned int	wdt_trgr_pattern = 0x1234;
static spinlock_t	wdt_lock;
static close_state_t	allow_close;

struct omap_wdt_dev {
	void __iomem    *base;          /* physical */
	struct device   *dev;
	int             omap_wdt_users;
	struct clk      *ick;
	struct clk      *fck;
	struct resource *mem;
	struct miscdevice omap_wdt_miscdev;
#ifdef CONFIG_OMAP_WATCHDOG_AUTOPET
	struct timer_list autopet_timer;
	unsigned long  jiffies_start;
	unsigned long  jiffies_exp;
#endif
};

static void omap_wdt_ping(struct omap_wdt_dev *wdev)
{
	void __iomem    *base = wdev->base;

	/* wait for posted write to complete */
	while ((__raw_readl(base + OMAP_WATCHDOG_WPS)) & 0x08)
		cpu_relax();

	wdt_trgr_pattern = ~wdt_trgr_pattern;
	__raw_writel(wdt_trgr_pattern, (base + OMAP_WATCHDOG_TGR));

	/* wait for posted write to complete */
	while ((__raw_readl(base + OMAP_WATCHDOG_WPS)) & 0x08)
		cpu_relax();
	/* reloaded WCRR from WLDR */
}

static int omap_wdt_panic(struct notifier_block *this, unsigned long event,
				void *ptr)
{
	struct omap_wdt_dev *wdev = platform_get_drvdata(omap_wdt_dev);
	unsigned long flags;

	spin_lock_irqsave(&wdt_lock, flags);

	if (wdev && wdev->omap_wdt_users > 0)
		omap_wdt_ping(wdev);

	spin_unlock_irqrestore(&wdt_lock, flags);

	return NOTIFY_DONE;
}


static void omap_wdt_enable(struct omap_wdt_dev *wdev)
{
	void __iomem *base = wdev->base;

	/* Sequence to enable the watchdog */
	__raw_writel(0xBBBB, base + OMAP_WATCHDOG_SPR);
	while ((__raw_readl(base + OMAP_WATCHDOG_WPS)) & 0x10)
		cpu_relax();

	__raw_writel(0x4444, base + OMAP_WATCHDOG_SPR);
	while ((__raw_readl(base + OMAP_WATCHDOG_WPS)) & 0x10)
		cpu_relax();
}

static void omap_wdt_disable(struct omap_wdt_dev *wdev)
{
	void __iomem *base = wdev->base;

	/* sequence required to disable watchdog */
	__raw_writel(0xAAAA, base + OMAP_WATCHDOG_SPR);	/* TIMER_MODE */
	while (__raw_readl(base + OMAP_WATCHDOG_WPS) & 0x10)
		cpu_relax();

	__raw_writel(0x5555, base + OMAP_WATCHDOG_SPR);	/* TIMER_MODE */
	while (__raw_readl(base + OMAP_WATCHDOG_WPS) & 0x10)
		cpu_relax();
}

static void omap_wdt_adjust_timeout(unsigned new_timeout)
{
	if (new_timeout < TIMER_MARGIN_MIN)
		new_timeout = TIMER_MARGIN_DEFAULT;
	if (new_timeout > TIMER_MARGIN_MAX)
		new_timeout = TIMER_MARGIN_MAX;
	timer_margin = new_timeout;
}

static void omap_wdt_set_timeout(struct omap_wdt_dev *wdev)
{
	u32 pre_margin = GET_WLDR_VAL(timer_margin);
	void __iomem *base = wdev->base;

	/* just count up at 32 KHz */
	while (__raw_readl(base + OMAP_WATCHDOG_WPS) & 0x04)
		cpu_relax();

	__raw_writel(pre_margin, base + OMAP_WATCHDOG_LDR);
	while (__raw_readl(base + OMAP_WATCHDOG_WPS) & 0x04)
		cpu_relax();
}

/*
 *	Allow only one task to hold it open
 */
static int omap_wdt_open(struct inode *inode, struct file *file)
{
	struct omap_wdt_dev *wdev = platform_get_drvdata(omap_wdt_dev);
	void __iomem *base = wdev->base;

	if (test_and_set_bit(1, (unsigned long *)&(wdev->omap_wdt_users)))
		return -EBUSY;

	if (!cpu_is_omap44xx())
		clk_enable(wdev->ick);    /* Enable the interface clock */
	clk_enable(wdev->fck);    /* Enable the functional clock */

	/* initialize prescaler */
	while (__raw_readl(base + OMAP_WATCHDOG_WPS) & 0x01)
		cpu_relax();

	__raw_writel((1 << 5) | (PTV << 2), base + OMAP_WATCHDOG_CNTRL);
	while (__raw_readl(base + OMAP_WATCHDOG_WPS) & 0x01)
		cpu_relax();

	file->private_data = (void *) wdev;
	allow_close = CLOSE_STATE_NOT;

	omap_wdt_set_timeout(wdev);
	omap_wdt_ping(wdev); /* trigger loading of new timeout value */
	omap_wdt_enable(wdev);

	return nonseekable_open(inode, file);
}

static int omap_wdt_release(struct inode *inode, struct file *file)
{
	struct omap_wdt_dev *wdev = file->private_data;

	/*
	 *      Shut off the timer unless NOWAYOUT is defined.
	 */
	if (allow_close == CLOSE_STATE_ALLOW) {
		omap_wdt_disable(wdev);
	
		if (!cpu_is_omap44xx())
			clk_disable(wdev->ick);
		clk_disable(wdev->fck);
	} 
	else {
		printk(KERN_CRIT "omap_wdt: Unexpected close, not stopping!\n");
	}
	allow_close = CLOSE_STATE_NOT;
	wdev->omap_wdt_users = 0;

	return 0;
}

static ssize_t omap_wdt_write(struct file *file, const char __user *data,
		size_t len, loff_t *ppos)
{
	unsigned long flags;
	struct omap_wdt_dev *wdev = file->private_data;

	/* Refresh LOAD_TIME. */
	if (len) {
		size_t i;

		/* In case it was set long ago */
		allow_close = CLOSE_STATE_NOT;

		for (i = 0; i != len; i++) {
			char c;

			if (get_user(c, data + i))
				return -EFAULT;
			if (c == 'V')
				allow_close = CLOSE_STATE_ALLOW;
		}
		spin_lock_irqsave(&wdt_lock, flags);
		omap_wdt_ping(wdev);
		spin_unlock_irqrestore(&wdt_lock, flags);
	}
	return len;
}

static long omap_wdt_ioctl(struct file *file, unsigned int cmd,
						unsigned long arg)
{
	struct omap_wdt_dev *wdev;
	int new_margin;
	unsigned long flags;
	static const struct watchdog_info ident = {
		.identity = "OMAP Watchdog",
		.options = WDIOF_SETTIMEOUT,
		.firmware_version = 0,
	};

	wdev = file->private_data;

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		return copy_to_user((struct watchdog_info __user *)arg, &ident,
				sizeof(ident));
	case WDIOC_GETSTATUS:
		return put_user(0, (int __user *)arg);
	case WDIOC_GETBOOTSTATUS:
		if (cpu_is_omap16xx())
			return put_user(__raw_readw(ARM_SYSST),
					(int __user *)arg);
		if (cpu_is_omap24xx())
			return put_user(omap_prcm_get_reset_sources(),
					(int __user *)arg);
	case WDIOC_KEEPALIVE:
		spin_lock_irqsave(&wdt_lock, flags);
		omap_wdt_ping(wdev);
		spin_unlock_irqrestore(&wdt_lock, flags);
		return 0;
	case WDIOC_SETTIMEOUT:
		if (get_user(new_margin, (int __user *)arg))
			return -EFAULT;
		omap_wdt_adjust_timeout(new_margin);

		spin_lock_irqsave(&wdt_lock, flags);
		omap_wdt_disable(wdev);
		omap_wdt_set_timeout(wdev);
		omap_wdt_enable(wdev);

		omap_wdt_ping(wdev);
		spin_unlock_irqrestore(&wdt_lock, flags);
		/* Fall */
	case WDIOC_GETTIMEOUT:
		return put_user(timer_margin, (int __user *)arg);
	default:
		return -ENOTTY;
	}
}

static const struct file_operations omap_wdt_fops = {
	.owner = THIS_MODULE,
	.write = omap_wdt_write,
	.unlocked_ioctl = omap_wdt_ioctl,
	.open = omap_wdt_open,
	.release = omap_wdt_release,
};

#ifdef CONFIG_OMAP_WATCHDOG_AUTOPET
static void autopet_handler(unsigned long data)
{
	unsigned long flags;
	struct omap_wdt_dev *wdev = (struct omap_wdt_dev *) data;

	spin_lock_irqsave(&wdt_lock, flags);
	omap_wdt_ping(wdev);
	spin_unlock_irqrestore(&wdt_lock, flags);
	wdev->jiffies_start = jiffies;
	wdev->jiffies_exp = (HZ * TIMER_AUTOPET_FREQ);
	mod_timer(&wdev->autopet_timer, jiffies + wdev->jiffies_exp);
}
#endif

static int __devinit omap_wdt_probe(struct platform_device *pdev)
{
	struct resource *res, *mem;
	struct omap_wdt_dev *wdev;
	int ret;

	/* reserve static register mappings */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENOENT;
		goto err_get_resource;
	}

	if (omap_wdt_dev) {
		ret = -EBUSY;
		goto err_busy;
	}

	mem = request_mem_region(res->start, resource_size(res), pdev->name);
	if (!mem) {
		ret = -EBUSY;
		goto err_busy;
	}

	wdev = kzalloc(sizeof(struct omap_wdt_dev), GFP_KERNEL);
	if (!wdev) {
		ret = -ENOMEM;
		goto err_kzalloc;
	}

	wdev->omap_wdt_users = 0;
	wdev->mem = mem;

	if (!cpu_is_omap44xx()) {
		wdev->ick = clk_get(&pdev->dev, "ick");
		if (IS_ERR(wdev->ick)) {
			ret = PTR_ERR(wdev->ick);
			wdev->ick = NULL;
			goto err_clk;
		}
	}
	wdev->fck = clk_get(&pdev->dev, "fck");
	if (IS_ERR(wdev->fck)) {
		ret = PTR_ERR(wdev->fck);
		wdev->fck = NULL;
		goto err_clk;
	}

	wdev->base = ioremap(res->start, resource_size(res));
	if (!wdev->base) {
		ret = -ENOMEM;
		goto err_ioremap;
	}

	platform_set_drvdata(pdev, wdev);

	if (!cpu_is_omap44xx())
		clk_enable(wdev->ick);
	clk_enable(wdev->fck);

	omap_wdt_disable(wdev);
	omap_wdt_adjust_timeout(timer_margin);

	wdev->omap_wdt_miscdev.parent = &pdev->dev;
	wdev->omap_wdt_miscdev.minor = WATCHDOG_MINOR;
	wdev->omap_wdt_miscdev.name = "watchdog";
	wdev->omap_wdt_miscdev.fops = &omap_wdt_fops;

	ret = misc_register(&(wdev->omap_wdt_miscdev));
	if (ret)
		goto err_misc;

	pr_info("OMAP Watchdog Timer Rev 0x%02x: initial timeout %d sec\n",
		__raw_readl(wdev->base + OMAP_WATCHDOG_REV) & 0xFF,
		timer_margin);

	/* autogate OCP interface clock, set smart idle and clockactivity*/
	__raw_writel(0x211, wdev->base + OMAP_WATCHDOG_SYS_CONFIG);

	if (!cpu_is_omap44xx())
		clk_disable(wdev->ick);
	clk_disable(wdev->fck);

	omap_wdt_dev = pdev;

#ifdef CONFIG_OMAP_WATCHDOG_AUTOPET
	setup_timer(&wdev->autopet_timer, autopet_handler,
		    (unsigned long) wdev);
	omap_wdt_startclocks(wdev);
	omap_wdt_set_timeout(wdev);
	wdev->jiffies_start = jiffies;
	wdev->jiffies_exp = (HZ * TIMER_AUTOPET_FREQ);
	mod_timer(&wdev->autopet_timer, jiffies + wdev->jiffies_exp);
	omap_wdt_enable(wdev);
	pr_info("Watchdog auto-pet enabled at %d sec intervals\n",
		TIMER_AUTOPET_FREQ);
#endif
	return 0;

err_misc:
	platform_set_drvdata(pdev, NULL);
	iounmap(wdev->base);

err_ioremap:
	wdev->base = NULL;

err_clk:
	printk("clk error in watchdog\n");
	if (!cpu_is_omap44xx())
		if (wdev->ick)
			clk_put(wdev->ick);
	if (wdev->fck)
		clk_put(wdev->fck);
	kfree(wdev);

err_kzalloc:
	release_mem_region(res->start, resource_size(res));

err_busy:
err_get_resource:

	return ret;
}

static void omap_wdt_shutdown(struct platform_device *pdev)
{
	struct omap_wdt_dev *wdev = platform_get_drvdata(pdev);

	if (wdev->omap_wdt_users)
		omap_wdt_disable(wdev);
}

static int __devexit omap_wdt_remove(struct platform_device *pdev)
{
	struct omap_wdt_dev *wdev = platform_get_drvdata(pdev);
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!res)
		return -ENOENT;

	misc_deregister(&(wdev->omap_wdt_miscdev));
	release_mem_region(res->start, resource_size(res));
	platform_set_drvdata(pdev, NULL);

	if (!cpu_is_omap44xx())
		clk_put(wdev->ick);
	clk_put(wdev->fck);
	iounmap(wdev->base);

	kfree(wdev);
	omap_wdt_dev = NULL;

	return 0;
}

#ifdef	CONFIG_PM

/* REVISIT ... not clear this is the best way to handle system suspend; and
 * it's very inappropriate for selective device suspend (e.g. suspending this
 * through sysfs rather than by stopping the watchdog daemon).  Also, this
 * may not play well enough with NOWAYOUT...
 */

static int omap_wdt_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct omap_wdt_dev *wdev = platform_get_drvdata(pdev);

	if (wdev->omap_wdt_users) {
#ifdef CONFIG_OMAP_WATCHDOG_AUTOPET
		wdev->jiffies_exp -= jiffies - wdev->jiffies_start;
		del_timer(&wdev->autopet_timer);
#endif
		omap_wdt_disable(wdev);
	}

	return 0;
}

static int omap_wdt_resume(struct platform_device *pdev)
{
	struct omap_wdt_dev *wdev = platform_get_drvdata(pdev);

	if (wdev->omap_wdt_users) {
#ifdef CONFIG_OMAP_WATCHDOG_AUTOPET
		mod_timer(&wdev->autopet_timer, jiffies + wdev->jiffies_exp);
#endif
		omap_wdt_enable(wdev);
		omap_wdt_ping(wdev);
	}

	return 0;
}

#else
#define	omap_wdt_suspend	NULL
#define	omap_wdt_resume		NULL
#endif

static struct platform_driver omap_wdt_driver = {
	.probe		= omap_wdt_probe,
	.remove		= __devexit_p(omap_wdt_remove),
	.shutdown	= omap_wdt_shutdown,
	.suspend	= omap_wdt_suspend,
	.resume		= omap_wdt_resume,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "omap_wdt",
	},
};

static struct notifier_block panic_blk = {
	.notifier_call  = omap_wdt_panic,
};

static int __init omap_wdt_init(void)
{
	int rc;

	spin_lock_init(&wdt_lock);
	rc = platform_driver_register(&omap_wdt_driver);
	if (!rc)
		atomic_notifier_chain_register(&panic_notifier_list, &panic_blk);

	return rc;
}

static void __exit omap_wdt_exit(void)
{
	atomic_notifier_chain_unregister(&panic_notifier_list, &panic_blk);

	platform_driver_unregister(&omap_wdt_driver);
}

module_init(omap_wdt_init);
module_exit(omap_wdt_exit);

MODULE_AUTHOR("George G. Davis");
MODULE_LICENSE("GPL");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);
MODULE_ALIAS("platform:omap_wdt");
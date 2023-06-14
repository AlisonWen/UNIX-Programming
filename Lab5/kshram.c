/*
 * Lab problem set for UNIX programming course
 * by Chun-Ying Huang <chuang@cs.nctu.edu.tw>
 * License: GPLv2
 */
#include <linux/module.h>	// included for all kernel modules
#include <linux/kernel.h>	// included for KERN_INFO
#include <linux/init.h>		// included for __init and __exit macros
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/errno.h>
#include <linux/sched.h>	// task_struct requried for current_uid()
#include <linux/cred.h>		// for current_uid();
#include <linux/slab.h>		// for kmalloc/kfree
#include <linux/uaccess.h>	// copy_to_user
#include <linux/string.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include "kshram.h"

static dev_t devnum; // device number
static struct cdev c_dev; // char device
static struct class *clazz;
char * mem_spvm[8];
int file_size[10];
int idx = 0;

static int hellomod_dev_open(struct inode *i, struct file *f) {
	return 0;
}

static int hellomod_dev_close(struct inode *i, struct file *f) {
	return 0;
}

static ssize_t hellomod_dev_read(struct file *f, char __user *buf, size_t len, loff_t *off) {
	printk(KERN_INFO "hellomod: read %zu bytes @ %llu.\n", len, *off);
	return len;
}

static ssize_t hellomod_dev_write(struct file *f, const char __user *buf, size_t len, loff_t *off) {
	printk(KERN_INFO "hellomod: write %zu bytes @ %llu.\n", len, *off);
	return len;
}

static long hellomod_dev_ioctl(struct file *fp, unsigned int cmd, unsigned long arg) {
	if(cmd == KSHRAM_GETSLOTS){ // GETSLOTS
		return 8;
	}else if(cmd == KSHRAM_GETSIZE){ // GETSIZE
		int cur_dev = (fp->f_path.dentry->d_iname)[strlen(fp->f_path.dentry->d_iname)-1] - '0';
		return file_size[cur_dev];
	}else if(cmd == KSHRAM_SETSIZE){ // SETSIZE
		int cur_dev = (fp->f_path.dentry->d_iname)[strlen(fp->f_path.dentry->d_iname) -1] - '0';
		mem_spvm[cur_dev] = krealloc(mem_spvm[cur_dev], arg, GFP_KERNEL);
		file_size[cur_dev] = arg;
	}
	return 0;
}

static int hellomod_dev_mmap(struct file* filp, struct vm_area_struct* vma){
	int cur_dev = (filp->f_path.dentry->d_iname)[strlen(filp->f_path.dentry->d_iname)];
	unsigned long pfn = page_to_pfn(virt_to_page((void *)mem_spvm[cur_dev]));
	unsigned long len = vma->vm_end - vma->vm_start;
	int ret = remap_pfn_range(vma, vma->vm_start, pfn, len, vma->vm_page_prot);
	if (ret < 0) {
		pr_err("could not map the address area\n");
		return -EIO;
	}
	return ret;
}

static const struct file_operations hellomod_dev_fops = {
	.owner = THIS_MODULE,
	.open = hellomod_dev_open,
	.read = hellomod_dev_read,
	.write = hellomod_dev_write,
	.unlocked_ioctl = hellomod_dev_ioctl,
	.release = hellomod_dev_close,
	.mmap = hellomod_dev_mmap
};

static int hellomod_proc_read(struct seq_file *m, void *v) {
	for(int i = 0; i < 8; i++){
		char buf[15];
		sprintf(buf, "0%d: %d\n", i, file_size[i]);
		seq_printf(m, buf);
	}
		
	return 0;
}

static int hellomod_proc_open(struct inode *inode, struct file *file) {
	return single_open(file, hellomod_proc_read, NULL);
}

static const struct proc_ops hellomod_proc_fops = {
	.proc_open = hellomod_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static char *hellomod_devnode(const struct device *dev, umode_t *mode) {
	if(mode == NULL) return NULL;
	*mode = 0666;
	return NULL;
}

static int __init hellomod_init(void)
{
	
	if((clazz = class_create(THIS_MODULE, "upclass")) == NULL)
		goto release_region;
	clazz->devnode = hellomod_devnode;
	cdev_init(&c_dev, &hellomod_dev_fops);
	if(alloc_chrdev_region(&devnum, 0, 8, "updev") < 0)
		return -1;
	// create char dev
	for (idx = 0; idx < 8; idx++){
		char name[10], dump[10];
		sprintf(name, "kshram%d", idx);
		sprintf(dump, "name%d", idx);
		
		if(device_create(clazz, NULL, MKDEV(MAJOR(devnum), MINOR(devnum)+idx), NULL, name) == NULL)
			goto release_class;
		if(cdev_add(&c_dev, MKDEV(MAJOR(devnum), MINOR(devnum)+idx), 1) == -1)
			goto release_device;
		mem_spvm[idx] = kzalloc(4096, GFP_KERNEL); // allocate 4KB and zero it out
		printk(KERN_INFO"kshram%d: 4096 bytes allocated @ %p\n", idx, mem_spvm[idx]);

		file_size[idx] = 4096;
		if (mem_spvm[idx] == NULL) {
			printk(KERN_ALERT "hellomod: unable to allocate memory for device %d\n", idx);
			goto release_memory;
		}
	}
	
	

	// create proc
	proc_create("kshram", 0666, NULL, &hellomod_proc_fops);

	printk(KERN_INFO "hellomod: initialized.\n");
	return 0;    // Non-zero return means that the module couldn't be loaded.
release_memory:
	for (int i = 0; i < idx; i++) {
		if (mem_spvm[i]) {
			kfree(mem_spvm[i]);
			mem_spvm[i] = NULL;
		}
	}
release_device:
	device_destroy(clazz, MKDEV(MAJOR(devnum), MINOR(devnum)+idx));
release_class:
	class_destroy(clazz);
release_region:
	unregister_chrdev_region(MKDEV(MAJOR(devnum), MINOR(devnum)+idx), 1);
	return -1;
}

static void __exit hellomod_cleanup(void)
{
	remove_proc_entry("kshram", NULL);

	cdev_del(&c_dev);
	for(int i = 0; i < 8; i++) device_destroy(clazz, MKDEV(MAJOR(devnum), MINOR(devnum)+i)), kfree(mem_spvm[i]);
	class_destroy(clazz);
	unregister_chrdev_region(devnum, 8);
	
	printk(KERN_INFO "hellomod: cleaned up.\n");
}

module_init(hellomod_init);
module_exit(hellomod_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chun-Ying Huang");
MODULE_DESCRIPTION("The unix programming course demo kernel module.");

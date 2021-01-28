/*
 * @file       debugfs.c
 * @details    Simple use of linux kernel debug fs
 * @author     smalinux <xunilams@gmail.com>
 *
 * What is debugfs?
 * Please read `Documentation/filesystems/debugfs.rst`
 *
 * Supplementary material
 * look at this cute sample: 
 * https://github.com/chadversary/debugfs-tutorial/tree/master/example2
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>                 //kmalloc()
#include <linux/uaccess.h>              //copy_to/from_user()
#include <linux/debugfs.h>


u8 val_u8 = 50;

dev_t dev = 0;
static struct class *dev_class;
static struct cdev sma_cdev;
int *kernel_buffer;

struct dentry * sma_dentry;

/*
** Function Prototypes
*/
static int      __init sma_driver_init(void);
static void     __exit sma_driver_exit(void);
static int      sma_open(struct inode *inode, struct file *file);
static int      sma_release(struct inode *inode, struct file *file);
static ssize_t  sma_read(struct file *filp, char __user *buf, size_t len,loff_t * off);
static ssize_t  sma_write(struct file *filp, const char *buf, size_t len, loff_t * off);


/*
** File Operations structure
*/
static struct file_operations fops =
{
        .owner          = THIS_MODULE,
        .read           = sma_read,
        .write          = sma_write,
        .open           = sma_open,
        .release        = sma_release,
};
 
/*
** This fuction will be called when we open the Device file
*/
static int sma_open(struct inode *inode, struct file *file)
{
        /*Creating Physical memory*/
        if((kernel_buffer = kmalloc(sizeof(int) , GFP_KERNEL)) == 0){
            printk(KERN_INFO "Cannot allocate memory in kernel\n");
            return -1;
        }
        printk(KERN_INFO "Device File Opened...!!!\n");
        return 0;
}

/*
** This fuction will be called when we close the Device file
*/
static int sma_release(struct inode *inode, struct file *file)
{
        kfree(kernel_buffer);
        printk(KERN_INFO "Device File Closed...!!!\n");
        return 0;
}

/*
** This fuction will be called when we read the Device file
*/
static ssize_t sma_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	int error_count = 0;

        //Copy the data from the kernel space to the user-space
        error_count = copy_to_user(buf, kernel_buffer, sizeof(int));
	if (error_count==0){            // if true then have success
        	printk(KERN_INFO "Data Read : Done!\n");
		return 0;  // clear the position to the start and return 0
	}
}

/*
** This fuction will be called when we write the Device file
*/
static ssize_t sma_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
        //Copy the data to kernel space from the user-space
        copy_from_user(kernel_buffer, buf, len);
        printk(KERN_INFO "Data Write : Done!\n");
        return len;
}

/*
** Module Init function
*/
static int __init sma_driver_init(void)
{
        /*Allocating Major number*/
        if((alloc_chrdev_region(&dev, 0, 1, "sma_Dev")) <0){
                printk(KERN_INFO "Cannot allocate major number\n");
                return -1;
        }
        printk(KERN_INFO "Major = %d Minor = %d \n",MAJOR(dev), MINOR(dev));
 
        /*Creating cdev structure*/
        cdev_init(&sma_cdev,&fops);
 
        /*Adding character device to the system*/
        if((cdev_add(&sma_cdev,dev,1)) < 0){
            printk(KERN_INFO "Cannot add the device to the system\n");
            goto r_class;
        }
 
        /*Creating struct class*/
        if((dev_class = class_create(THIS_MODULE,"sma_class")) == NULL){
            printk(KERN_INFO "Cannot create the struct class\n");
            goto r_class;
        }
 
        /*Creating device*/
        if((device_create(dev_class,NULL,dev,NULL,"sma_device")) == NULL){
            printk(KERN_INFO "Cannot create the Device 1\n");
            goto r_device;
        }

	sma_dentry = debugfs_create_dir("sma_debugfs", NULL);

	/*
	 * THERE ARE SIMPLER WAY TO DO THIS
	 * PLEASE LOOK AT debugfs_create_u8
	 **/
	debugfs_create_file("sma_debug_file",
			0666,
			sma_dentry,
			kernel_buffer,
			&fops); /* used fops of already exist cdev :D */
	
	/* easy peasy */
	debugfs_create_u8("sma_u8", 0666, sma_dentry, &val_u8);

        printk(KERN_INFO "Device Driver Insert...Done!!!\n");
        return 0;
 
r_device:
        class_destroy(dev_class);
r_class:
        unregister_chrdev_region(dev,1);
        return -1;
}

/*
** Module exit function
*/
static void __exit sma_driver_exit(void)
{
	debugfs_remove(sma_dentry);
        device_destroy(dev_class,dev);
        class_destroy(dev_class);
        cdev_del(&sma_cdev);
        unregister_chrdev_region(dev, 1);
        printk(KERN_INFO "Device Driver Remove...Done!!!\n");
}
 
module_init(sma_driver_init);
module_exit(sma_driver_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sohaib Mhmd <xunilams@gmail.com>");
MODULE_DESCRIPTION("Simple Linux device driver (Real Linux Device Driver)");
MODULE_VERSION("1.4");

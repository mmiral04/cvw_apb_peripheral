/*
 * Driver for the Seven Segment Display APB peripheral
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/io.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mario Miralles");
MODULE_DESCRIPTION("Driver for the Seven Segment Display APB peripheral");

/* Prototypes */
static int ssd_open(struct inode *, struct file *);
static int ssd_close(struct inode *, struct file *);
static ssize_t ssd_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t ssd_write(struct file *, const char __user *, size_t, loff_t *);

/* Global variables */

#define DRIVER_NAME "ssd_driver"
#define DEVICE_NAME "ssd"

#define DEFAULT_BASE_ADDR 0x100000
#define MAP_SIZE 0x10

#define NUM_DIGITS 8

static void __iomem *iomap;
static resource_size_t base_addr = DEFAULT_BASE_ADDR;

static dev_t dev; /* (major, minor) pair assigned to the device */
static struct cdev ssd_cdev;
static struct class *cls;

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = ssd_open,
	.release = ssd_close,
	.read = ssd_read,
	.write = ssd_write
};

static int __init ssd_init(void)
{
	int ret;

	// Dynamically allocate 1 (major, minor) pair
	ret = alloc_chrdev_region(&dev, 0, 1, DRIVER_NAME);
	if (ret)
		return ret;

	// Register the device
	cdev_init(&ssd_cdev, &fops);
	ret = cdev_add(&ssd_cdev, dev, 1);
	if (ret)
		goto err_add;

	cls = class_create(DEVICE_NAME);
	if (IS_ERR(cls)) {
		pr_err("Failed to create class for device\n");
		ret = PTR_ERR(cls);
		goto err_add;
	}
	device_create(cls, NULL, dev, NULL, DEVICE_NAME);

	iomap = ioremap(base_addr, MAP_SIZE);

	pr_alert("%s driver installed. Major: %d, Minor: %d\n",
			DRIVER_NAME, MAJOR(dev), MINOR(dev));

err_add:
	unregister_chrdev_region(dev, 1);
	return ret;
}

static void __exit ssd_exit(void)
{
	/* Turn off SSD */
	for (int i = 0; i < NUM_DIGITS; i++)
		iowrite32(0, (i * 4) + iomap);

	iounmap(iomap);

	device_destroy(cls, dev);
	class_destroy(cls);

	cdev_del(&ssd_cdev);
	unregister_chrdev_region(dev, 1);
	pr_alert("%s driver removed.\n", DRIVER_NAME);
}

/* Methods */
static int ssd_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int ssd_close(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t ssd_read(struct file *filp, char __user *buffer,
		size_t length, loff_t *offset)
{
	return -EINVAL;
}

static ssize_t ssd_write(struct file *filp, const char __user *buff,
		size_t len, loff_t *off)
{
	int value, reg;
	char *kbuf = kmalloc(len, GFP_KERNEL);
	char *token;
	char *kbufptr = kbuf;
	if (kbuf == NULL) {
		pr_err("%s: Failed to allocated buffer\n", DRIVER_NAME);
		return -ENOMEM;
	}

	if (copy_from_user(kbuf, buff, len)) {
		kfree(kbuf);
		return -EFAULT;
	}

	if (strncmp(kbuf, "OFF", 3) == 0) {
		/* Turn off displays */
		pr_info("%s: Turn off displays.\n", DRIVER_NAME);
		for (int i = 0; i < NUM_DIGITS; i++) {
			iowrite32(0, (i * 4) + iomap);
		}

		return len;

	} else {
		token = strsep(&kbufptr, ":");
		while (token != NULL) {
			if (sscanf(token, "%d,%x", &reg, &value) == 2) {
				pr_info("%s: Update digit %d to: 0x%x\n", DRIVER_NAME, reg, value);
				iowrite32(value, (reg * 4) + iomap);
			} else {
				pr_err("%s: Invalid value: %s", DRIVER_NAME, token);
				kfree(kbuf);
				return -EINVAL;
			}
			token = strsep(&kbufptr, ":");
		}

	}
	kfree(kbuf);
	return len;
}

module_init(ssd_init);
module_exit(ssd_exit);

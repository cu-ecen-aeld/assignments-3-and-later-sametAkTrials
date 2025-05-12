/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/string.h>
#include "aesdchar.h"
#include "aesd-circular-buffer.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Samet Ak"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    struct aesd_dev *dev;
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    struct aesd_dev *dev = filp->private_data;
    struct aesd_circular_buffer *circ_buf = dev->aesd_circ_bufp;

    if(!circ_buf->entry_count) {
        retval = 0;
        goto exit;
    }

    size_t send_data_count = count > circ_buf->entry[circ_buf->out_offs].size - *f_pos ? circ_buf->entry[circ_buf->out_offs].size - *f_pos : count;

    if (send_data_count > 0) {
        if (copy_from_user((circ_buf->entry[circ_buf->out_offs].buffptr + *f_pos), buf, send_data_count) != 0) {
            retval = -EFAULT;
            goto exit;
        }
        retval = send_data_count;

        if(circ_buf->entry[circ_buf->out_offs].size > *f_pos + send_data_count)
            *f_pos += send_data_count;
        else {
            *f_pos = 0;
            circ_buf->out_offs = (circ_buf->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        }
    }
exit:
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t retval = 0;
    size_t entry_buf_size = count;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
    struct aesd_dev *dev = filp->private_data;

    if(dev->incomplete_entry.buffptr) {
        entry_buf_size += dev->incomplete_entry.size;
    }

    char *entry_buf = kmalloc(entry_buf_size * sizeof(char), GFP_KERNEL);
    if(entry_buf == NULL) {
        retval = -ENOMEM;
        goto exit;
    }

    if(dev->incomplete_entry.buffptr) {
        memcpy(entry_buf, dev->incomplete_entry.buffptr, dev->incomplete_entry.size);
        kfree(dev->incomplete_entry.buffptr);
        dev->incomplete_entry.buffptr = NULL;
    }

    if(copy_from_user(entry_buf + dev->incomplete_entry.size, buf, count)) {
        retval = -EFAULT;
        goto exit;
    }

    dev->incomplete_entry.size = entry_buf_size;
    dev->incomplete_entry.buffptr = entry_buf;
    if(dev->incomplete_entry.buffptr[entry_buf_size - 1] == '\n') {
        aesd_circular_buffer_add_entry(dev->aesd_circ_bufp, &dev->incomplete_entry);
        kfree(dev->incomplete_entry.buffptr);
        dev->incomplete_entry.buffptr = NULL;
        dev->incomplete_entry.size = 0;
    }
    retval = count;

exit:
    return retval;
}


struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    aesd_device.aesd_circ_bufp = kmalloc(sizeof(struct aesd_circular_buffer), GFP_KERNEL);
    if(!aesd_device.aesd_circ_bufp) {
        result = -ENOMEM;
        goto fail;
    }
    aesd_circular_buffer_init(aesd_device.aesd_circ_bufp);
    aesd_device.incomplete_entry.buffptr = NULL;
    aesd_device.incomplete_entry.size = 0;

    result = aesd_setup_cdev(&aesd_device);

fail:
    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    kfree(aesd_device.aesd_circ_bufp);
    if(aesd_device.incomplete_entry.buffptr)
        kfree(aesd_device.incomplete_entry.buffptr);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);

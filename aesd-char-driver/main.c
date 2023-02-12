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
#include <linux/fs.h>   // file_operations
#include <linux/slab.h> /* kmalloc() */
#include "aesdchar.h"
#include "aesd-circular-buffer.h"
#include <linux/mutex.h>
#include <linux/gfp.h>
#include "aesd_ioctl.h"

int aesd_major = 0; // use dynamic major
int aesd_minor = 0;
MODULE_AUTHOR("Suresh Kannaian"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

/////////////////////////////////////////////////////////////////////////

struct aesd_buffer_entry *find_entry(struct aesd_circular_buffer *buffer,
                                     size_t char_offset, size_t *entry_offset_byte_rtn)
{

    int start = 0;
    size_t retIndex = 0;
    size_t len = buffer->entry[retIndex].size;
    // printk(KERN_WARNING "start-Req  char_offset=%lu, calc len=%lu, ret index=%lu\n",  char_offset, len, retIndex);

    while (len <= char_offset && retIndex < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED && retIndex < buffer->in_offs)
    {
        ++retIndex;
        len = len + buffer->entry[retIndex].size;
    }

    if (retIndex > AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - 1)
    {
        return NULL;
    }

    if (char_offset > len - 1)
    {
        return NULL;
    }

    start = len - buffer->entry[retIndex].size;
    *entry_offset_byte_rtn = char_offset - start;

    // printk(KERN_WARNING "End Req  char_offset=%lu, calc len=%lu, ret index=%lu\n",  char_offset, len, retIndex);
    // printk(KERN_WARNING "Requested string is %s of size %lu, entry_offset_byte_rtn=%lu \n",
    //                         buffer->entry[retIndex].buffptr, buffer->entry[retIndex].size, *entry_offset_byte_rtn);

    buffer->out_offs = retIndex;

    return &(buffer->entry[retIndex]);
}

///////////////////////////////////////////////////////////////////////////////////////

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open-start");
    struct aesd_dev *dev;
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
    PDEBUG("open-end");
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    filp->private_data = NULL;
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                  loff_t *f_pos)
{
    ssize_t retval = 0, ret, data_size;
    struct aesd_buffer_entry *aesd_entry;
    struct aesd_dev *data = filp->private_data;
    size_t entry_offset_byte_rtn;
    struct aesd_circular_buffer *temp_aesd_buffer;
    PDEBUG("aesd_read %zu bytes with offset %lld", count, *f_pos);

    /**
     * TODO: handle read
     */
    if (mutex_lock_interruptible(&aesd_device.lock))
        return -ERESTARTSYS;

    temp_aesd_buffer = &data->aesd_buffer;
    if ((aesd_entry = find_entry(
             &data->aesd_buffer, *f_pos, &entry_offset_byte_rtn)) != NULL)
    {
        data_size = ((aesd_entry->size - entry_offset_byte_rtn) > count) ? count : (aesd_entry->size - entry_offset_byte_rtn);
        // printk(KERN_WARNING "from ret=%0x and original %0x with data size %d", aesd_entry->buffptr, data->aesd_buffer.entry[0].buffptr, data_size);
        ret = copy_to_user(buf, (aesd_entry->buffptr + entry_offset_byte_rtn), data_size);
        if (ret)
        {
            retval = -EFAULT;
            PDEBUG("aesd_read copying to user space failed");
        }
        else
        {
            retval = data_size;
            *f_pos += data_size;
        }
    }

    mutex_unlock(&data->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    char *kernel_buffer = NULL;
    struct aesd_buffer_entry *prev_entry = NULL;
    bool append = false;
    struct aesd_buffer_entry *entry = NULL;
    PDEBUG("aesd_write %zu bytes with offset %lld", count, *f_pos);

    /**
     * TODO: handle write
     */

    struct aesd_dev *driver_data = filp->private_data;

    if (mutex_lock_interruptible(&driver_data->lock))
    {
        PDEBUG("aesd_write buffer full checking if append required");
        return -ERESTARTSYS;
    }

    if (driver_data->aesd_buffer.full)
    {
        PDEBUG("aesd_write buffer full checking if append required");
        prev_entry = &(driver_data->aesd_buffer.entry[driver_data->aesd_buffer.in_offs]);
        if (prev_entry->buffptr[prev_entry->size - 1] != '\n')
        {
            append = true;
            PDEBUG("aesd_write buffer full and need to append");
        }
        else
        {
            PDEBUG("aesd_write buffer full freeing memory at 0");
            driver_data->aesd_buffer.total_size = driver_data->aesd_buffer.total_size - driver_data->aesd_buffer.entry[0].size;
            kfree(driver_data->aesd_buffer.entry[0].buffptr);
        }
    }
    else
    {
        PDEBUG("aesd_write buffer not full");
        if (driver_data->aesd_buffer.in_offs > 0 &&
            driver_data->aesd_buffer.in_offs < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
        {
            PDEBUG("aesd_write checking prev entry");
            prev_entry = &(driver_data->aesd_buffer.entry[driver_data->aesd_buffer.in_offs - 1]);
            if (prev_entry->buffptr[prev_entry->size - 1] != '\n')
            {
                append = true;
                PDEBUG("aesd_write prev entry require append");
            }
        }
    }

    if (append && prev_entry != NULL)
    {
        PDEBUG("aesd_write appending to existing entry and realloc");
        prev_entry->buffptr = krealloc(prev_entry->buffptr, (prev_entry->size + count), GFP_KERNEL);
        if (copy_from_user(prev_entry->buffptr + prev_entry->size, buf, count))
        {
            PDEBUG("aesd_write appending to existing entry failed");
            retval = -EFAULT;
            goto exit;
        }
        else
        {
            retval = count;
            prev_entry->size = prev_entry->size + count;
            driver_data->aesd_buffer.total_size = driver_data->aesd_buffer.total_size + count;
            goto exit;
        }
    }
    else
    {
        PDEBUG("aesd_write creating new entry");
        entry = kmalloc(sizeof(struct aesd_buffer_entry), GFP_KERNEL);
        kernel_buffer = kmalloc(sizeof(count), GFP_KERNEL);
        memset(kernel_buffer, 0, count);
        if (copy_from_user(kernel_buffer, buf, count))
        {
            PDEBUG("aesd_write copy from user failed");
            retval = -EFAULT;
            goto exit;
        }
        else
        {
            PDEBUG("aesd_write copy from user success with");

            retval = count;
            entry->buffptr = kernel_buffer;
            entry->size = count;

            aesd_circular_buffer_add_entry(&driver_data->aesd_buffer, entry);
        }
    }

exit:
    PDEBUG("exit execution");
    mutex_unlock(&aesd_device.lock);
    return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t offset, int whence)
{
    PDEBUG("aesd_llseek-Begin");
    off_t retval = 0;
    struct aesd_dev *driver_data;
    if (filp->private_data)
    {
        driver_data = filp->private_data;
    }
    else
    {
        PDEBUG("aesd_llseek error while opening file");
        retval = -EINVAL;
        goto exit;
    }
    retval = mutex_lock_interruptible(&driver_data->lock);
    if (retval != 0)
    {
        PDEBUG("aesd_llseek error locking mutex");
        goto exit;
    }

    retval = fixed_size_llseek(filp, offset, whence, driver_data->aesd_buffer.total_size);
    if (retval == -EINVAL)
    {
        PDEBUG("aesd_llseek fixed_size_llseek failed");
    }
    mutex_unlock(&driver_data->lock);

exit:
    return retval;
}

long aesd_ioctl(struct file *filp, uint32_t cmd, unsigned long arg)
{
    PDEBUG("aesd ioctl");
    struct aesd_dev *driver_data;
    struct aesd_seekto seekto;
    long retval;
    int ret;
    loff_t f_pos = 0;
    int i = 0;

    if (filp->private_data)
    {
        driver_data = filp->private_data;
    }
    else
    {
        retval = -EINVAL;
        PDEBUG("Wrong file pointer");
        goto exit;
    }

    if (_IOC_TYPE(cmd) != AESD_IOC_MAGIC || _IOC_NR(cmd) > AESDCHAR_IOC_MAXNR)
    {
        retval = -ENOTTY;
        PDEBUG("aesd_ioctl-Invalid command and offset");
        goto exit;
    }
    if (cmd == AESDCHAR_IOCSEEKTO)
    {
        ret = copy_from_user(&seekto, (void __user *)arg, sizeof(seekto));
        if (ret != 0)
        {
            PDEBUG("Copy from user failed");
            retval = -EFAULT;
            goto exit;
        }
        else
        {
            if ((seekto.write_cmd >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) || 
                (seekto.write_cmd_offset >= driver_data->aesd_buffer.entry[seekto.write_cmd].size))
            {
                retval = -EINVAL;
                goto exit;
            }  
            retval = mutex_lock_interruptible(&driver_data->lock);
            if (retval != 0)
            {
                PDEBUG("aesd_offset error locking mutex");
                goto exit;
            }                      
            while (i < seekto.write_cmd)
            {
                f_pos += driver_data->aesd_buffer.entry[i].size;
                i++;
            }
            f_pos += seekto.write_cmd_offset;
            filp->f_pos = f_pos;

            mutex_unlock(&driver_data->lock);
        }
    }
    else
    {
        retval = -ENOTTY;
    }
exit:
    return retval;
}

struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .read = aesd_read,
    .write = aesd_write,
    .open = aesd_open,
    .release = aesd_release,
    .llseek = aesd_llseek,
    .unlocked_ioctl = aesd_ioctl,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
    {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    printk(KERN_ALERT "init-begin\n");

    result = alloc_chrdev_region(&dev, aesd_minor, 1,
                                 "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0)
    {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }

    memset(&aesd_device, 0, sizeof(struct aesd_dev));
    mutex_init(&(aesd_device.lock));

    result = aesd_setup_cdev(&aesd_device);

    if (result)
    {
        unregister_chrdev_region(dev, 1);
    }
    printk(KERN_ALERT "init-done\n");
    return result;
}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);
    int index = 0;

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    printk(KERN_ALERT "aesd_cleanup_module");

    while (index < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
    {
        if (aesd_device.aesd_buffer.entry[index].buffptr)
        {
            kfree(aesd_device.aesd_buffer.entry[index].buffptr);
        }
        ++index;
    }
    mutex_destroy(&aesd_device.lock);
    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);

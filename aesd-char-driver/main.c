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
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Cedric Camacho"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    struct aesd_dev *tmp_dev;
    tmp_dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = tmp_dev;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    /* 
    * as of now dont need to do anything due to filp->private_data 
    * allocated in init module
    */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    
    int rc = 0;
    ssize_t tmp_off_byte = 0;
    int bytes_left = 0;
    int bytes_read = 0;
    struct aesd_dev *tmp_dev = NULL;
    // struct aesd_buffer_entry *buf_entry = NULL;

    tmp_dev = (struct aesd_dev*)filp->private_data;

    rc = mutex_lock_interruptible(&tmp_dev->device_lock);
    if(rc){
        PDEBUG("ERROR: mutex lock failed with code: %d", rc);
        retval = -rc;
        goto end;
    }

    tmp_dev->buf_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&tmp_dev->circ_buf,
                                                                         *f_pos,
                                                                         &tmp_off_byte);


    if(!tmp_dev->buf_entry){
        retval = 0;
        goto mutex_cleanup;
    }
    
    bytes_left = copy_to_user(buf,tmp_dev->buf_entry->buffptr,tmp_dev->buf_entry->size);

    bytes_read = tmp_dev->buf_entry->size - bytes_left;

    PDEBUG("DEBUG: %d out of %d bytes read from buffer", bytes_read, tmp_dev->buf_entry->size );

    if(!bytes_left){
        retval = tmp_dev->buf_entry->size;
        *f_pos += tmp_dev->buf_entry->size - tmp_off_byte;
    }
    else{
        PDEBUG("ERROR: could not read all necessary bytes from buffer", bytes_read, tmp_dev->buf_entry->size );
        // goto mutex_cleanup;
        retval = bytes_read;

        // goto end;
    }


 mutex_cleanup:
    mutex_unlock(&tmp_dev->device_lock);
    // rc = mutex_unlock(&tmp_dev->device_lock);
    // if(rc){

    //     PDEBUG("ERROR: could not read all necessary bytes from buffer", bytes_read, tmp_dev->buf_entry->size );
    //     retval = -EFAULT;

    // }

 end: 
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
    
    int rc = 0;
    int tmp_off_byte = 0;
    int bytes_left = 0;
    struct aesd_dev *tmp_dev = NULL;

    // define temporary aesd_dev struct
    tmp_dev = (struct aesd_dev*)filp->private_data;


    rc = mutex_lock_interruptible(&tmp_dev->device_lock);
        if(rc){
            PDEBUG("ERROR: mutex lock failed with code: %d", rc);
            retval = -rc;
            goto end;
        }

    // check size of buffer
    if(!tmp_dev->buf_entry->size){// if zero -> hasn't been allocated

        tmp_dev->buf_entry->buffptr = kmalloc(count, GFP_KERNEL);

        if(!tmp_dev->buf_entry->buffptr){

            PDEBUG("ERROR: unable to allocate enough memory");
            retval = -ENOMEM;
            // goto mutex_cleanup;
            goto end;

        }


    }
    else{ // if non zero, has been allocated before, realloc to increase size

        tmp_dev->buf_entry->buffptr = krealloc(tmp_dev->buf_entry->buffptr, tmp_dev->buf_entry->size + count, GFP_KERNEL);

        if(!tmp_dev->buf_entry->buffptr){

            PDEBUG("ERROR: unable to allocate enough memory");
            retval = -ENOMEM;
            // goto mutex_cleanup;
            goto end;

        }

    }

    // copy user buffer to entry
    rc = copy_from_user(tmp_dev->buf_entry->buffptr[tmp_dev->buf_entry->size], buf, count);
    if(rc){

        PDEBUG("ERROR: unable to copy from user buffer");
        retval = -EFAULT;
        goto end;

    }

    tmp_dev->buf_entry->size += count;
    retval = count;

    // check last character of buffer
    if(tmp_dev->buf_entry->buffptr[tmp_dev->buf_entry->size - 1] == (char)'\n'){

        // lock circular buffer
        // rc = mutex_lock_interruptible(&tmp_dev->device_lock);
        // if(rc){
        //     PDEBUG("ERROR: mutex lock failed with code: %d", rc);
        //     retval = -rc;
        //     goto end;
        // }

        // write to circular buffer
        aesd_circular_buffer_add_entry(&tmp_dev->circ_buf, tmp_dev->buf_entry);

        // mutex_unlock(&tmp_dev->device_lock);

        // free buffer entry
        tmp_dev->buf_entry->size = 0;
        kfree(tmp_dev->buf_entry->buffptr);
        tmp_dev->buf_entry->buffptr = NULL;


    }




//  mutex_cleanup:
    mutex_unlock(&tmp_dev->device_lock);

//  free_alloc:
//     kfree(tmp_dev->buf_entry->buffptr);

 end: 
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
    aesd_device.buf_entry = kmalloc(sizeof(struct aesd_buffer_entry), GFP_KERNEL);
    aesd_device.buf_entry->size = 0;
    aesd_circular_buffer_init(&aesd_device.circ_buf);
    mutex_init(&aesd_device.device_lock);
    result = aesd_setup_cdev(&aesd_device);

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

    int index =0;
    struct aesd_buffer_entry *buf_entry;
    
    kfree(aesd_device.buf_entry);

    AESD_CIRCULAR_BUFFER_FOREACH(buf_entry, &aesd_device.circ_buf, index){

        if(buf_entry->buffptr){
            buf_entry->size = 0;
            kfree(buf_entry->buffptr);
            buf_entry->buffptr = NULL;
        }
        

    }
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);

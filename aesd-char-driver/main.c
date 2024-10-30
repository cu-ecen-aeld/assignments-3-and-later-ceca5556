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
#include <linux/mutex.h>
#include <linux/slab.h>

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
    // PDEBUG("entry buffer pointer val: %p",tmp_dev->buf_entry);
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


loff_t aesd_llseek(struct file *filp, loff_t desired_offset, int whence){

    struct aesd_dev *tmp_dev = NULL;
    struct aesd_buffer_entry *circ_buf_entry = NULL;
    loff_t new_off = 0;
    loff_t tot_size = 0;
    int index = 0;

    tmp_dev = (struct aesd_dev *)filp->private_data;

    rc = mutex_lock_interruptible(&tmp_dev->device_lock);
    if(rc){
        // PDEBUG("ERROR: mutex lock failed with code: %d", rc);
        retval = -rc;
        goto end;
    }

    AESD_CIRCULAR_BUFFER_FOREACH(circ_buf_entry, &tmp_dev->circ_buf, index){

        tot_size += circ_buf_entry->size;

    }



    switch(whence){
        case SEEK_SET:
            new_off = desired_offset % tot_size;
            break;
        
        case SEEK_CUR:
            new_off = (tmp_dev->f_pos + desired_offset) % tot_size;
            break;

        case SEEK_END:
            new_off = (tot_size + desired_offset) % tot_size;
            break;

        default:
            return -EINVAL;
    }

    tmp_dev->f_pos = new_off;

 mutex_cleanup:
    mutex_unlock(&tmp_dev->device_lock);

 end:
    return new_off;

}

static long aesd_adjust_file_offset(struct file *filp, uint32_t write_cmd, uint32_t write_cmd_offset){

    int rc = 0;
    struct aesd_dev *tmp_dev = NULL;
    struct aesd_buffer_entry *circ_buf_entry = NULL;
    int cmd_idx = 0;
    loff_t new_off = 0;


    tmp_dev = (struct aesd_dev *)filp->private_data;

    if(write_cmd >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED){
        PDEBUG("write command exceeds max supported opperations");
        return -EINVAL;
    }
    else if(write_cmd_offset >= tmp_dev->circ_buf.entry[write_cmd].size){
        PDEBUG("write command offset exceeds command size");
        return -EINVAL;
    }


    rc = mutex_lock_interruptible(&tmp_dev->device_lock);
    if(rc){
        PDEBUG("mutex lock failed with code: %d", rc);
        retval = -rc;
        goto end;
    }

    AESD_CIRCULAR_BUFFER_FOREACH(circ_buf_entry, &tmp_dev->circ_buf, cmd_idx){

        if(cmd_idx == write_cmd){
            new_off += write_cmd_offset;
            break;
        }
        else{
            new_off += circ_buf_entry->size;
        }

    }

    tmp_dev->f_pos = new_off;

    mutex_unlock(&tmp_dev->device_lock);

    return new_off;


}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){

    loff_t new_off = 0;
    struct aesd_seekto llseek_struct;

    if (_IOC_TYPE(cmd) != AESD_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR) return -ENOTTY;


    switch(cmd){

        case AESDCHAR_IOCSEEKTO:
            if(copy_from_user(&llseek_struct, (const void __user *) arg, sizeof(llseek_struct))){
                return -EFAULT;
            }
            else{
                new_off = aesd_adjust_file_offset(filp, llseek_struct.write_cmd, llseek_struct.write_cmd_offset);
            }
            break;
        
        default:
            return -ENOTTY;
        
    }

    return new_off;

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
    int bytes_sent = 0;
    int bytes_return = 0;
    struct aesd_dev *tmp_dev = NULL;
    struct aesd_buffer_entry *tmp_buf_entry = NULL;

    tmp_dev = (struct aesd_dev*)filp->private_data;

    tmp_buf_entry = tmp_dev->buf_entry;

    rc = mutex_lock_interruptible(&tmp_dev->device_lock);
    if(rc){
        // PDEBUG("ERROR: mutex lock failed with code: %d", rc);
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
    
    PDEBUG("string of size %ld read from buffer: %s",tmp_dev->buf_entry->size,tmp_dev->buf_entry->buffptr);

    // calculate how many bytes to return
    bytes_return = tmp_dev->buf_entry->size - tmp_off_byte;

    // copy from kernel to user
    bytes_left = copy_to_user(buf,tmp_dev->buf_entry->buffptr,bytes_return);

    // check total bytes sent
    bytes_sent = tmp_dev->buf_entry->size - bytes_left;

    PDEBUG("%d out of %ld bytes copied to user", bytes_sent, tmp_dev->buf_entry->size );

    if(!bytes_left){ // if all sent
        retval = bytes_return;
    }
    else{// if not all sent

        retval = bytes_sent;
    }

    *f_pos += retval;


 mutex_cleanup:
    mutex_unlock(&tmp_dev->device_lock);

 end:
    tmp_dev->buf_entry = tmp_buf_entry;
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
    struct aesd_dev *tmp_dev = NULL;
    struct aesd_buffer_entry removed_data; 
    char *local_tmp_buf;

    // assign temporary aesd_dev struct
    tmp_dev = (struct aesd_dev*)filp->private_data;


    // lock the mutex
    rc = mutex_lock_interruptible(&tmp_dev->device_lock);
    if(rc){
        PDEBUG("mutex lock failed with code: %d", rc);
        retval = -rc;
        goto end;
    }


    // check size of buffer
    PDEBUG("tmp dev buf entry pointer: %p", tmp_dev->buf_entry);
    if(!tmp_dev->buf_entry->size){// if zero -> hasn't been allocated

        tmp_dev->buf_entry->buffptr = kmalloc(count*sizeof(char), GFP_KERNEL);
        if(!tmp_dev->buf_entry->buffptr){

            // PDEBUG("ERROR: unable to allocate enough memory");
            retval = -ENOMEM;
            goto mutex_cleanup;
            // goto end;

        }
        local_tmp_buf = tmp_dev->buf_entry->buffptr;

    }
    else{ // if non zero, has been allocated before, realloc to increase size

        tmp_dev->buf_entry->buffptr = krealloc(tmp_dev->buf_entry->buffptr, tmp_dev->buf_entry->size + count, GFP_KERNEL);
        if(!tmp_dev->buf_entry->buffptr){

            // PDEBUG("ERROR: unable to allocate enough memory");
            retval = -ENOMEM;
            goto mutex_cleanup;
            // goto end;

        }

        local_tmp_buf = tmp_dev->buf_entry->buffptr + tmp_dev->buf_entry->size;

    }

    // PDEBUG(" buffer pointer: %p",tmp_dev->buf_entry->buffptr);
    PDEBUG(" temp buffer size before: %ld",tmp_dev->buf_entry->size);

    // copy user buffer to entry
    rc = copy_from_user(local_tmp_buf, buf, count);
    if(rc){ // success = 0, anything else failed

        // PDEBUG("ERROR: unable to copy from user buffer");
        retval = -EFAULT;
        goto mutex_cleanup;

    }



    tmp_dev->buf_entry->size += count;
    retval = count;

    PDEBUG(" temp buffer size after: %ld",tmp_dev->buf_entry->size);
    PDEBUG(" temp buffer content: %s",tmp_dev->buf_entry->buffptr);

    // check last character of buffer
    if(tmp_dev->buf_entry->buffptr[tmp_dev->buf_entry->size - 1] == (char)'\n'){


        // write to circular buffer
        PDEBUG("placing %s into circular buffer", tmp_dev->buf_entry->buffptr);

        removed_data = aesd_circular_buffer_add_entry(&tmp_dev->circ_buf, tmp_dev->buf_entry);

        tmp_dev->buf_entry->size = 0;

        if(removed_data.buffptr){ // check if removed entry pointer already null
            removed_data.size = 0;
            PDEBUG("removed buffer of pointer: %p",removed_data.buffptr);
            kfree(removed_data.buffptr);
            removed_data.buffptr = NULL;
            PDEBUG("overwritten buffer has been freed");
        }


    }




 mutex_cleanup:
    // unlock mutex
    mutex_unlock(&tmp_dev->device_lock);

 end: 
    return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek =   aesd_llseek,
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
    
    // private entry struct for writes 
    aesd_device.buf_entry = kmalloc(sizeof(struct aesd_buffer_entry), GFP_KERNEL);
    aesd_device.buf_entry->size = 0;

    // initialize circular buffer
    aesd_circular_buffer_init(&aesd_device.circ_buf);

    // mutex initialization
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
    struct aesd_buffer_entry *circ_buf_entry = NULL;
    
    // PDEBUG("cleanup: buf entry pointer: %p", aesd_device.buf_entry);

    // free the private entry struct
    kfree(aesd_device.buf_entry);
    aesd_device.buf_entry = NULL;

    // PDEBUG("cleanup: buf entry pointer after: %p", aesd_device.buf_entry);

    // free all entries in circular buffer
    AESD_CIRCULAR_BUFFER_FOREACH(circ_buf_entry, &aesd_device.circ_buf, index){


        if(circ_buf_entry->buffptr != NULL){

            // PDEBUG("cleanup loop: freeing circ buf entry");
            circ_buf_entry->size = 0;
            kfree(circ_buf_entry->buffptr);
            circ_buf_entry->buffptr = NULL;

        }

    }    
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);

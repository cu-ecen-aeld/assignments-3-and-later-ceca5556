/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#include <stdio.h>
#endif

#include "aesd-circular-buffer.h"

static uint8_t update_offset(uint8_t offset){

    // offset = (offset + 1) & (AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - 1);
    offset = (offset + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    return offset;
    
}

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    /**
    * TODO: implement per description
    */

    // if((buffer->out_offs == buffer->in_offs) && !buffer->full){

    //     return NULL;

    // }

    uint8_t current_entry = buffer->out_offs;

    while(1){
        if(char_offset < buffer->entry[current_entry].size){ //check offset compared to size
            *entry_offset_byte_rtn = char_offset;
            break;
        }
        else{// go to next entry
            char_offset -=  buffer->entry[current_entry].size; // subtract offset from already checked
            current_entry = update_offset(current_entry);
        }

        if(current_entry == buffer->in_offs){ // means no more data available to check
            return NULL;
        }
    }

    return &buffer->entry[current_entry];
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: implement per description
    */
    if(buffer->full){// check if full -> if true overwrite and update out
        buffer->out_offs = update_offset(buffer->out_offs);
        buffer->full = false;
    }

    buffer->entry[buffer->in_offs] = *add_entry;

    buffer->in_offs = update_offset(buffer->in_offs);

    if(buffer->in_offs == buffer->out_offs){// check if full after update
        buffer->full = true;
    }

}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}

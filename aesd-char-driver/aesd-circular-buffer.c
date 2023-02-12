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
#endif

#include "aesd-circular-buffer.h"

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
                                                                          size_t char_offset, size_t *entry_offset_byte_rtn)
{

    int start = 0;
    size_t retIndex = 0;
    size_t len = buffer->entry[retIndex].size;
    //printf("char_offset = %d, len = %d ,retIdex = %d\n", (int)char_offset, (int)len, (int)retIndex);
    while (len <= char_offset && retIndex < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED && retIndex < buffer->in_offs)
    {
        ++retIndex;
        len = len + buffer->entry[retIndex].size;
        //printf("char_offset = %d, len = %d ,retIdex = %d\n", (int)char_offset, (int)len, (int)retIndex);
    }

    if (retIndex > AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - 1)
    {
        return NULL;
    }

    if (char_offset > len -1)
    {
        return NULL;
    }

    start = len - buffer->entry[retIndex].size;
    *entry_offset_byte_rtn = char_offset - start;

    buffer->out_offs = retIndex;

    return &(buffer->entry[retIndex]);
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
    int idx = 0;
    if (buffer->full)
    {
        for (idx = 1; idx <= buffer->in_offs; ++idx)
        {
            // printf("reorg prev = %d, curr = %d, max=%d\n", idx-1, idx, buffer->in_offs);
            buffer->entry[idx - 1] = buffer->entry[idx];
        }
    }

    buffer->entry[buffer->in_offs].buffptr = add_entry->buffptr;
    buffer->entry[buffer->in_offs].size = add_entry->size;
    buffer->total_size += add_entry->size;
    if (buffer->in_offs == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - 1)
    {
        buffer->full = true;
    }
    else
    {
        ++buffer->in_offs;
    }
}

/**
 * Initializes the circular buffer described by @param buffer to an empty struct
 */
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer, 0, sizeof(struct aesd_circular_buffer));
}

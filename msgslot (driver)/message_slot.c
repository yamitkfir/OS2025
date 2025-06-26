#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/
#include <linux/slab.h>     /* for kmalloc and kfree */
#include "message_slot.h"
MODULE_LICENSE("GPL");

/* Forward declarations */
static int device_open(struct inode* inode, struct file* file);
static ssize_t device_read(struct file* file, char __user* buffer, size_t length, loff_t* offset);
static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset);
static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long ioctl_param);
static int __init simple_init(void);
static void __exit simple_cleanup(void);

typedef struct message_slot {
  unsigned int minor_num;           // Minor number of the device file
  struct message_channel *channels; // Array or hash table of channels
  int num_channels;                 // Track active channels
  int censor_flag;             // Censor flag (0 or 1)
} message_slot;

typedef struct message_channel {
  unsigned int id;          // Channel ID (non-zero)
  char *message;            // Pointer to the message (max 128 bytes)
  size_t msg_len;           // Length of the message
  // int censor_flag;          // Censor flag (0 or 1)
  struct message_channel *next; // Pointer to the next channel in the list
} message_channel;

// static struct chardev_info device_info; // TODO raises error in compilation
// message_slot *this_message_slot = NULL;
// int censor_flag = 0;
static message_slot* all_message_slots[MAX_MINOR_NUM]; // "there can be at most 256 different message slots device files"
message_slot* this_message_slot = NULL;


//================== DEVICE FUNCTIONS ===========================
static int device_open( struct inode* inode,
                        struct file*  file )
{
  int minornum = iminor(inode);
  
  // Check if message_slot already exists for this minor number
  if (all_message_slots[minornum] != NULL) {
    this_message_slot = all_message_slots[minornum];
    return 0;
  }

  this_message_slot = (message_slot*)kmalloc(sizeof(message_slot), GFP_KERNEL);
  if (!this_message_slot) {
    printk(KERN_ERR "Failed to allocate memory for message_slot\n");
    // TODO set errno to ENOMEM
    return -ENOMEM;
  }
  this_message_slot->minor_num = minornum;
  this_message_slot->num_channels = 0;
  this_message_slot->channels = NULL;
  this_message_slot->censor_flag = 0;
  all_message_slots[minornum] = this_message_slot;
  return 0;
}

//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to read from it
static ssize_t device_read( struct file* file,
                            char __user* buffer,
                            size_t       length,
                            loff_t*      offset )
{
  message_channel *channel = (message_channel *)file->private_data;
  int minornum = iminor(file->f_inode);
  this_message_slot = all_message_slots[minornum];

  int the_message_length = channel->msg_len;
  int slot_channel = channel->id;
  char* the_message = channel->message;
  int min_length; int i;
  printk("message_length: %d, slot_channel: %d, message ptr: %p\n", the_message_length, slot_channel, the_message);

  if (slot_channel == 0) {
    // TODO set errno to EINVAL
    return -EINVAL;
  }
  if (the_message == NULL || the_message_length == 0) {
    // TODO set errno to EWOULDBLOCK
    return -EWOULDBLOCK;
  }

  if (length < the_message_length) {
    // TODO set errno to ENOSPC
    return -ENOSPC;
  }
  min_length = min((size_t)length, (size_t)the_message_length);
  for (i = 0; i < min_length; i++) {
    put_user(channel->message[i], &buffer[i]);
  }
  // printk("read performed coolely\n");
  return min_length;
}

//---------------------------------------------------------------
// a processs which has already opened
// the device file attempts to write to it
static ssize_t device_write( struct file*       file,
                             const char __user* buffer,
                             size_t             length,
                             loff_t*            offset)
{
  message_channel *channel;
  ssize_t i;
  int slot_channel;
  int minornum = iminor(file->f_inode);
  this_message_slot = all_message_slots[minornum];

  channel = (message_channel *)file->private_data;
  slot_channel = channel->id;

  printk("Invoking device_write(%p,%ld) for channel %d\n", file, length, slot_channel);

  if (slot_channel == 0) {
    // TODO set errno to EINVAL
    return -EINVAL;
  }
  if (length > BUF_LEN || length <= 0) {
    // TODO set errno to EMSGSIZE
    return -EMSGSIZE;
  }

  // Free old message if it exists
  if (channel->message != NULL) {
    kfree(channel->message);
  }

  channel->message = kmalloc(length, GFP_KERNEL);
  if (!channel->message) {
    printk(KERN_ERR "Failed to allocate memory for message\n");
    // TODO set errno to ENOMEM
    return -ENOMEM;
  }
  channel->msg_len = length;
  // printk("device_write: allocated message=%p, set len=%ld\n", channel->message, length);

  for( i = 0; i < length; i++ ) {
    if (i % 3 == 2 && this_message_slot->censor_flag == 1) {
      channel->message[i] = '#';  // Direct assignment in kernel space
    } else {
      get_user(channel->message[i], &buffer[i]);
    }
  }
  // printk("write of length %ld performed coolely\n", length);
  return length;
}

//----------------------------------------------------------------
static long device_ioctl( struct   file* file,
                          unsigned int   ioctl_command_id,
                          unsigned long  ioctl_param )
{
  message_channel *channels;
  // message_slot *this_message_slot = all_message_slots[iminor(inode)];

  // Switch according to the ioctl called
  if( MSG_SLOT_CHANNEL == ioctl_command_id ) {
    if (ioctl_param == 0) {
      // TODO set errno to EINVAL
      return -EINVAL;
    }
    // Get the parameter given to ioctl by the process
    printk( "Invoking ioctl: setting slot_channel "
            "to %ld\n", ioctl_param );
    
    channels = this_message_slot->channels;
    while(channels != NULL) {
      if (channels->id == ioctl_param) {
        break;
      }
      channels = channels->next;
    } 
    if (channels == NULL) { // create the new channel
      channels = kmalloc(sizeof(message_channel), GFP_KERNEL);
      if (!channels) {
        printk(KERN_ERR "Failed to allocate memory for channels\n");
        // TODO set errno to ENOMEM
        return -ENOMEM;
      }
      channels->id = ioctl_param;
      channels->message = NULL;
      channels->msg_len = 0;
      channels->next = this_message_slot->channels;
      // channels->censor_flag = 0;
      this_message_slot->channels = channels; // putting the new one in beggining of LL
      this_message_slot->num_channels++;
    }
    file->private_data = channels;
  }

  else if (MSG_SLOT_SET_CEN == ioctl_command_id) {
    if (ioctl_param != 0 && ioctl_param != 1) {
      // TODO set errno to EINVAL
      return -EINVAL;
    }
    printk( "Invoking ioctl: setting cencor_flag of this channel "
            "to %ld\n", ioctl_param );

    // channel = (message_channel *)file->private_data;
    // channel->censor_flag = ioctl_param;
    this_message_slot->censor_flag = ioctl_param;
  }
  
  else {
    // TODO set errno to EINVAL
    return -EINVAL;
  }
  return SUCCESS;
}

//==================== DEVICE SETUP =============================
// This structure will hold the functions to be called
// when a process does something to the device we created
struct file_operations Fops = {
  .owner	        = THIS_MODULE, 
  .read           = device_read,
  .write          = device_write,
  .open           = device_open,
  .unlocked_ioctl = device_ioctl,
  // .release        = device_release,
};

//---------------------------------------------------------------
// Initialize the module - Register the character device
static int __init simple_init(void)
{
  int rc = -1;
  // init dev struct // TODO does problems in compilation
  // memset( &device_info, 0, sizeof(struct chardev_info) );

  // Register driver capabilities. Obtain major num
  rc = register_chrdev( MAJOR_NUM, DEVICE_RANGE_NAME, &Fops );

  // Negative values signify an error
  if( rc < 0 ) {
    printk( KERN_ERR "%s registraion failed for  %d\n",
                       DEVICE_FILE_NAME, MAJOR_NUM );
    return rc;
  }

  printk( "Registeration is successful. ");
  printk( "If you want to talk to the device driver,\n" );
  printk( "you have to create a device file:\n" );
  printk( "mknod /dev/%s c %d 0\n", DEVICE_FILE_NAME, MAJOR_NUM );
  printk( "You can echo/cat to/from the device file.\n" );
  printk( "Dont forget to rm the device file and "
          "rmmod when you're done\n" );

  return 0;
}

//---------------------------------------------------------------
static void __exit simple_cleanup(void)
{
  int i;
  struct message_channel *current_msg_channel = NULL;
  struct message_channel *next_msg_channel = NULL;
  
  // Clean up all message slots
  for (i = 0; i < MAX_MINOR_NUM; i++) {
    if (all_message_slots[i] != NULL) {
      if (all_message_slots[i]->channels) {
        current_msg_channel = all_message_slots[i]->channels;
        while (current_msg_channel) {
          next_msg_channel = current_msg_channel->next;
          kfree(current_msg_channel->message);
          kfree(current_msg_channel);
          current_msg_channel = next_msg_channel;
        }
      }
      kfree(all_message_slots[i]);
      all_message_slots[i] = NULL;
    }
  }
  
  unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}

module_init(simple_init);
module_exit(simple_cleanup);
//========================= END OF FILE =========================
#ifndef MSGSLOT_H
#define MSGSLOT_H

#include <linux/ioctl.h>

#define MAJOR_NUM 235
#define BUF_LEN 128

// Set the message of the device driver
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned long)
#define MSG_SLOT_SET_CEN _IOW(MAJOR_NUM, 1, unsigned long)

#define DEVICE_RANGE_NAME "msgslot"
#define DEVICE_FILE_NAME "msgslot"
#define SUCCESS 0
#define MAX_MINOR_NUM 257

#define ERROR_EXIT_VAL 1
#define ERROR_WRITING_MSG "Error writing to device"
#define ERROR_READING_MSG "Error reading from device"
#define ERROR_OPENING_FILE "Error opening device file"
#define ERROR_SETTING_CHANNEL "Error setting channel id"
#define ERROR_SETTING_CENSORSHIP "Error setting censorship mode"

#endif

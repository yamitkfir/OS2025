#include "message_slot.h"    

#include <fcntl.h>      /* open */ 
#include <unistd.h>     /* exit */
#include <sys/ioctl.h>  /* ioctl */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
  if (argc != 5) {
    printf("Usage: %s <device_file> <channel_id> <cencorship_mode> <message>\n", argv[0]);
    exit(ERROR_EXIT_VAL);
  }

  char *device_file = argv[1];
  unsigned long channel_id = strtoul(argv[2], NULL, 10);
  int cencorship_mode = strtoul(argv[3], NULL, 10);
  char *message = argv[4];

  int file_desc = open(device_file, O_RDWR);
  if (file_desc < 0) {
    perror(ERROR_OPENING_FILE);
    exit(ERROR_EXIT_VAL);
  }

  int ret_val = ioctl(file_desc, MSG_SLOT_SET_CEN, cencorship_mode);
  if (ret_val < 0) {
    perror(ERROR_SETTING_CENSORSHIP);
    close(file_desc);
    exit(ERROR_EXIT_VAL);
  }

  ret_val = ioctl(file_desc, MSG_SLOT_CHANNEL, channel_id);
  if (ret_val < 0) {
    perror(ERROR_SETTING_CHANNEL);
    close(file_desc);
    exit(ERROR_EXIT_VAL);
  }

  ret_val = write(file_desc, message, strlen(message));
  if (ret_val < 0) {
    perror(ERROR_WRITING_MSG);
    close(file_desc);
    exit(ERROR_EXIT_VAL);
  }

  // printf("great i sent %s of length %ld\n", message, strlen(message));
  close(file_desc); 
  return 0;
}
#include "message_slot.h"    

#include <fcntl.h>      /* open */ 
#include <unistd.h>     /* exit */
#include <sys/ioctl.h>  /* ioctl */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
  if (argc != 3) {
    printf("Usage: %s <device_file> <channel_id>\n", argv[0]);
    exit(ERROR_EXIT_VAL);
  }

  char *device_file = argv[1];
  unsigned long channel_id = strtoul(argv[2], NULL, 10);
  char buffer[BUF_LEN];

  int file_desc = open(device_file, O_RDWR);
  if (file_desc < 0) {
    perror(ERROR_OPENING_FILE);
    exit(ERROR_EXIT_VAL);
  }

  int ret_val = ioctl(file_desc, MSG_SLOT_CHANNEL, channel_id);
  if (ret_val < 0) {
    perror(ERROR_SETTING_CHANNEL);
    close(file_desc);
    exit(ERROR_EXIT_VAL);
  }

  ret_val = read(file_desc, buffer, BUF_LEN);
  if (ret_val < 0) {
    perror(ERROR_READING_MSG);
    close(file_desc);
    exit(ERROR_EXIT_VAL);
  }

  close(file_desc);
  
  ssize_t bytes_written = write(STDOUT_FILENO, buffer, ret_val);
  printf("\n");
  
  if (bytes_written < 0) {
    perror("Error writing to stdout");
    exit(ERROR_EXIT_VAL);
  }

  return 0;
}
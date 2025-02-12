#ifndef _MY_LIB_H_
#define _MY_LIB_H_

#include <stdio.h>      
#include <stdlib.h>    
#include <errno.h>      
#include <string.h>     // For strerror
#include <unistd.h>    
#include <sys/syscall.h>// For syscall 
#include <sys/types.h>  // For mode_t
#include <sys/stat.h>   // For S_IRUSR, S_IWUSR
#include <fcntl.h>      // For O_RDONLY, O_WRONLY
#include <pthread.h>   


#define MAX_HANDLES 1024


typedef int myos_handle_t;

typedef struct 
{
    int active;    
    int os_fd;     // OS file descriptor from syscall() 
    pthread_mutex_t mtx;
} file_context_t;



void myos_init_subsystem(void);
myos_handle_t myos_allocate_handle(int os_fd);
myos_handle_t myos_create_handle(const char *path, int flags, mode_t mode);
int myos_handle_to_fd(myos_handle_t handle);
ssize_t myos_read(myos_handle_t handle, void *buffer, size_t len);
ssize_t myos_write(myos_handle_t handle, const void *buffer, size_t len);
int myos_close_handle(myos_handle_t handle);


#endif

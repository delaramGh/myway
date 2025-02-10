#include "my_lib.h"



// Global array for storing file contexts
static file_context_t g_contexts[MAX_HANDLES];

// A stack to store free handle indices for quick reuse
// top_of_free_stack points to the next free slot in free_handles
static myos_handle_t free_handles[MAX_HANDLES];
static int top_of_free_stack = -1;

// A mutex to protect access to g_contexts and free_handles in multithreaded scenarios
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;



//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
// Populate the free_handle stack 
void myos_init_subsystem(void)
{
    pthread_mutex_lock(&g_mutex);
    top_of_free_stack = -1;

    for (int i=0; i<MAX_HANDLES; i++) 
    {
        g_contexts[i].active = 0;
        g_contexts[i].os_fd  = -1;
        pthread_mutex_init(&g_contexts[i].mtx, NULL);
    }

    for (int i=MAX_HANDLES-1; i>=0; i--) 
        free_handles[++top_of_free_stack] = i;
    
    pthread_mutex_unlock(&g_mutex);
}


// Gives a new handle and bind it to the OS_fd
myos_handle_t myos_allocate_handle(int os_fd)
{
    myos_handle_t handle = -1;

    pthread_mutex_lock(&g_mutex);
    if (top_of_free_stack < 0) // No free handles available 
    {
        pthread_mutex_unlock(&g_mutex);
        return -1;
    }

    handle = free_handles[top_of_free_stack--];
    g_contexts[handle].active = 1;
    g_contexts[handle].os_fd  = os_fd;
    pthread_mutex_unlock(&g_mutex);

    return handle;
}


/**
 * Create a new file handle by calling the Linux syscall
 * @param path  : The path of the file to open
 * @param flags : Flags for opening (O_RDONLY, O_WRONLY, O_RDWR)
 * @param mode  : Permissions (S_IRUSR | S_IWUSR) if creating files
 * @return our opaque handle, or -1 on error
 */
myos_handle_t myos_create_handle(const char *path, int flags, mode_t mode)
{
    // Directly calls the Linux open syscall 
    int fd = syscall(SYS_open, path, flags, mode);
    if (fd < 0) 
        // File could not be opened 
        return -1;
    
    // Allocate an internal handle and store fd
    myos_handle_t handle = myos_allocate_handle(fd);
    if (handle < 0) 
    {
        // We ran out of handle slots, close the OS_fd to avoid leaks 
        syscall(SYS_close, fd);
        return -1;
    }
    return handle;
}


// Look up the OS_fd from our internal table given the handle
int myos_handle_to_fd(myos_handle_t handle)
{
    if (handle < 0 || handle >= MAX_HANDLES) 
        return -1;
    
    pthread_mutex_lock(&g_mutex);
    if (!g_contexts[handle].active) 
    {
        pthread_mutex_unlock(&g_mutex);
        return -1;
    }
    int fd = g_contexts[handle].os_fd;
    pthread_mutex_unlock(&g_mutex);
    return fd;
}


/**
 * Read bytes from a file handle using a direct Linux syscall
 * @param handle : Our opaque handle
 * @param buffer : Buffer to receive data
 * @param len    : Number of bytes to read
 * @return number of bytes read, or -1 on error
 */
ssize_t myos_read(myos_handle_t handle, void *buffer, size_t len)
{
    int fd = myos_handle_to_fd(handle);
    if (fd < 0) 
    {
        errno = EINVAL; // Invalid handle 
        return -1;
    }

    if (pthread_mutex_trylock(&g_contexts[handle].mtx) != 0) 
    {
        fprintf(stderr, "myos_read: handle %d is busy elsewhere.\n", handle);
        errno = EBUSY;
        return -1;
    }

    // Move file offset to the beginning for reading. 
    syscall(SYS_lseek, fd, 0, SEEK_SET);
    ssize_t bytes_read = syscall(SYS_read, fd, buffer, len);
    pthread_mutex_unlock(&g_contexts[handle].mtx);
    if (bytes_read < 0) 
        return -1;
    return bytes_read;
}


/**
 * Write bytes to a file handle using a direct Linux syscall
 * @param handle : Our opaque handle
 * @param buffer : Buffer containing data to write
 * @param len    : Number of bytes to write
 * @return number of bytes written, or -1 on error
 */
ssize_t myos_write(myos_handle_t handle, const void *buffer, size_t len)
{
    int fd = myos_handle_to_fd(handle);
    if (fd < 0) 
    {
        errno = EINVAL; // Invalid handle 
        return -1;
    }

    if (pthread_mutex_trylock(&g_contexts[handle].mtx) != 0) 
    {
        fprintf(stderr, "myos_write: handle %d is busy elsewhere.\n", handle);
        errno = EBUSY;
        return -1;
    }
    ssize_t bytes_written = syscall(SYS_write, fd, buffer, len);
    pthread_mutex_unlock(&g_contexts[handle].mtx);
    if (bytes_written < 0) 
        return -1;
    return bytes_written;
}


/**
 * Close/retire a handle, freeing it back to the subsystem and closing the OS_fd
 * @param handle : The handle to retire
 * @return 0 on success, -1 on error
 */
int myos_close_handle(myos_handle_t handle)
{
    pthread_mutex_lock(&g_mutex);
    if (handle < 0 || handle >= MAX_HANDLES || !g_contexts[handle].active) 
    {
        pthread_mutex_unlock(&g_mutex);
        errno = EINVAL; // Invalid handle 
        return -1;
    }

    pthread_mutex_lock(&g_contexts[handle].mtx);
    int fd = g_contexts[handle].os_fd;
    g_contexts[handle].active = 0;
    g_contexts[handle].os_fd  = -1;

    // Push this handle index back onto the free stack 
    free_handles[++top_of_free_stack] = handle;
    pthread_mutex_unlock(&g_mutex);
    pthread_mutex_unlock(&g_contexts[handle].mtx);
    
    // Close the OS file descriptor using the direct syscall 
    if (syscall(SYS_close, fd) < 0) 
        return -1;

    return 0;
}

#include "my_lib.h"


int test_1(void);
int test_2(void);
void* thread_func(void*);

typedef struct 
{
    myos_handle_t handle;
    int thread_id;
} thread_arg_t;


int main(void)
{
    printf("*** STEP 0: Initialize our subsystem once at the beginning.\n");
    myos_init_subsystem();

    printf("\n-------------------- test 1 --------------------\n");
    test_1();
    
    printf("\n-------------------- test 2--------------------\n");
    test_2();


    return EXIT_SUCCESS;
}




int test_1(void)
{
    printf("*** STEP 1: ");
    myos_handle_t handle = myos_create_handle("demo.txt", O_RDWR | O_CREAT, 0666);
    if (handle < 0) 
    {
        fprintf(stderr, "myos_create_handle failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    printf("Created handle: %d\n", handle);

    printf("*** STEP 2: Write something to the file.\n");
    const char *text = "Hello from my handle subsystem!";
    ssize_t written = myos_write(handle, text, strlen(text));
    if (written < 0) 
    {
        fprintf(stderr, "myos_write failed: %s\n", strerror(errno));
        myos_close_handle(handle);
        return EXIT_FAILURE;
    }
    printf("            Wrote %zd bytes.\n", written);


    printf("*** STEP 3: Read from file.\n");
    char buffer[128];
    ssize_t bytes_read = myos_read(handle, buffer, sizeof(buffer));
    if (bytes_read < 0) 
    {
        fprintf(stderr, "myos_read failed: %s\n", strerror(errno));
        myos_close_handle(handle);
        return EXIT_FAILURE;
    }
    printf("            %s\n", buffer);

    
    printf("*** STEP 4: Close our handle.\n");
    if (myos_close_handle(handle) < 0) 
    {
        fprintf(stderr, "myos_close_handle failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    printf("            Closed handle: %d\n", handle);

    return EXIT_SUCCESS;
}


int test_2(void)
{
    printf("*** STEP 1: ");
    myos_handle_t handle = myos_create_handle("demo_multithread.txt", O_RDWR | O_CREAT, 0666);
    if (handle < 0) 
    {
        fprintf(stderr, "myos_create_handle failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    printf("Created handle: %d\n", handle);


    // Creating multiple threads that simultaneously read/write one handle 
    const int NUM_THREADS = 2;
    pthread_t threads[NUM_THREADS];
    thread_arg_t thread_args[NUM_THREADS];

    //Creating threads
    for (int i=0; i<NUM_THREADS; i++)
    {
        thread_args[i].handle = handle;
        thread_args[i].thread_id = i + 1;
        pthread_create(&threads[i], NULL, thread_func, &thread_args[i]);
    }

    // Wait for all threads to finish 
    for (int i=0; i<NUM_THREADS; i++) 
        pthread_join(threads[i], NULL);
    

    if (myos_close_handle(handle)<0) 
    {
        fprintf(stderr, "myos_close_handle failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    printf("Closed handle: %d\n", handle);

    return EXIT_SUCCESS;
}


void* thread_func(void* arg)
{
    thread_arg_t *thread_arg = (thread_arg_t *)arg;

    // Write
    char write_buf[64];
    snprintf(write_buf, sizeof(write_buf), "Thread %d says hello!\n", thread_arg->thread_id);
    ssize_t w = myos_write(thread_arg->handle, write_buf, strlen(write_buf));
    if (w < 0)
    {
        if (errno == EBUSY)
            fprintf(stderr, "[Thread %d] write: EBUSY - handle is busy.\n", thread_arg->thread_id);
        else 
            fprintf(stderr, "[Thread %d] write failed: %s\n", thread_arg->thread_id, strerror(errno));
    } 
    else 
        printf("[Thread %d] wrote %zd bytes.\n", thread_arg->thread_id, w);
    

    usleep(50*1000); // 50ms sleep

    // Read
    char read_buf[100];
    ssize_t r = myos_read(thread_arg->handle, read_buf, sizeof(read_buf)-1);
    if (r < 0) 
    {
        if (errno == EBUSY) 
            fprintf(stderr, "[Thread %d] read: EBUSY - handle is busy.\n", thread_arg->thread_id);
        else 
            fprintf(stderr, "[Thread %d] read failed: %s\n", thread_arg->thread_id, strerror(errno));
    } 
    else 
        printf("[Thread %d] read %zd bytes: '%s'\n", thread_arg->thread_id, r, read_buf);
    
    return NULL;
}
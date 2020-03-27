## thread_fork()

This project is an excercise of "thread_fork" implementation;
fork() is not supported on Win32;

thread_fork() is an experiment of avoiding to start a child thread;

Example:

main thread:
int *buffer = malloc(sizeof(int) * 1024 * 1024);    *// 1millions of integer*

if (thread_fork() != 0) {
	>[ child thread branch ]
	>  // manipulate buffer....
	>  free(buffer);	// the child thread will deallocate buffer
	>   return;
}

.. other action on main thread..


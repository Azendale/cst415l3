#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Opaque pointer used to define a connection
typedef void * rsp_connection_t;

// Initialize the RSP stack
// window_size: The maximum window size supported by the RSP stack
void rsp_init(int window_size);

// Finalize the RSP stack
// This function should be called after all RSP operations are completed. 
// It should clean up any memory or other resources used by the RSP stack.
void rsp_shutdown();

// Create a connection
// This call blocks until a connection is established or until the connection
// process detects an unrecoverable error
//
// connection_name: Null terminated string that identifies the connection
//
// Return value: 
//    On success, returns a connection object that can be passed to
//                rsp_close, rsp_write, rsp_read
//    On failure: returns NULL
//
rsp_connection_t rsp_connect(const char *connection_name);

// Closes a connection
// The call blocks until the connection is closed at both ends
//
// rsp: the connection object returned by rsp_connect
//
// Return value:
//     On success: zero
//     On failure: non-zero
//
// Note: It is erroneous to pass the connection object to any other function
//       after it is closed
int rsp_close(rsp_connection_t rsp);

// Write a block of data to the connection
//
// rsp: the connection object returned by rsp_connect
// buff: pointer to the data to be sent over the connection
// size: the number of bytes to send
//
// Return value:
//     The number of bytes sent
//     A value <0 indicates an error
int rsp_write(rsp_connection_t rsp, void *buff, int size);

// Read a block of data from the connection
//
// rsp: the connection object returned by rsp_connect
// buff: pointer to a buffer to receive the data
// size: The size of the buffer
//
// Return value:
//     The number of bytes read
//     zero means the connection was closed by the other end
//     A value <0 indicates an error
int rsp_read(rsp_connection_t rsp, void *buff, int size);

#ifdef __cplusplus
}
#endif


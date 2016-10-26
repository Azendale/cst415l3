#pragma once
//******************************************************
// getport.h
//
// Author: Philip Howard
// Date: 10/4/2016
//
// Functions for requesting ports and doing lookups on assigned ports.
//

#ifdef __cplusplus
extern "C" {
#endif

// Set the host name and port number for the name servier
// host: null terminated string indicating the nameserver's host
//       A value of NULL will use the default "unix.cset.oit.edu"
// port: The port number for the nameserver.
//       A value of zero will use the default port 35000
int setup_ns(const char *host, int port);

// Request a port number for the specified name
// service_name: null terminated string < 50 characters long
// return value: The assigned port. A value < 0 means an error occurred
int request_port(const char *service_name);

// Look up a port number for the specified name
// service_name: null terminated string < 50 characters long
// return value: The assigned port. A value < 0 means an error occurred
int lookup_port(const char *service_name);

// Release a port back into the pool
// service_name: null terminated string < 50 characters long
// port: the port number the service is running on
// return value: 0 = success, non-zero = error
int release_port(const char *service_name, int port);

// Keep Alive: indicate a port is still in use
// service_name: null terminated string < 50 characters long
// port: the port number the service is running on
// return value: 0 = success, non-zero = error
int keep_alive_port(const char *service_name, int port);

void Fatal_Error(const char *msg, ...);

#ifdef __cplusplus
}
#endif

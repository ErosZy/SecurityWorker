/* Copyright JS Foundation and other contributors, http://js.foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "jerryscript-debugger-transport.h"
#include "jerryscript-ext/debugger.h"
#include "jext-common.h"

#ifdef JERRY_DEBUGGER

#include <errno.h>

#ifdef WIN32
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#include <WS2tcpip.h>
#include <winsock2.h>

/* On Windows the WSAEWOULDBLOCK value can be returned for non-blocking operations */
#define JERRYX_EWOULDBLOCK WSAEWOULDBLOCK

/* On Windows the invalid socket's value of INVALID_SOCKET */
#define JERRYX_SOCKET_INVALID INVALID_SOCKET

/* On Windows sockets have a SOCKET typedef */
typedef SOCKET jerryx_socket;

#else /* !WIN32 */

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

/* On *nix the EWOULDBLOCK errno value can be returned for non-blocking operations */
#define JERRYX_EWOULDBLOCK EWOULDBLOCK

/* On *nix the invalid socket has a value of -1 */
#define JERRYX_SOCKET_INVALID (-1)

/* On *nix the sockets are integer identifiers */
typedef int jerryx_socket;
#endif /* WIN32 */

/**
 * Implementation of transport over tcp/ip.
 */
typedef struct
{
  jerry_debugger_transport_header_t header; /**< transport header */
  jerryx_socket tcp_socket; /**< tcp socket */
} jerryx_debugger_transport_tcp_t;

/**
 * Get the network error value.
 *
 * On Windows this returns the result of the `WSAGetLastError ()` call and
 * on any other system the `errno` value.
 *
 *
 * @return last error value.
 */
static inline int
jerryx_debugger_tcp_get_errno (void)
{
#ifdef WIN32
  return WSAGetLastError ();
#else /* !WIN32 */
  return errno;
#endif /* WIN32 */
} /* jerryx_debugger_tcp_get_errno */

/**
 * Correctly close a single socket.
 */
static inline void
jerryx_debugger_tcp_close_socket (jerryx_socket socket_id) /**< socket to close */
{
#ifdef WIN32
  closesocket (socket_id);
#else /* !WIN32 */
  close (socket_id);
#endif /* WIN32 */
} /* jerryx_debugger_tcp_close_socket */

/**
 * Log tcp error message.
 */
static void
jerryx_debugger_tcp_log_error (int errno_value) /**< error value to log */
{
  if (errno_value == 0)
  {
    return;
  }

#ifdef WIN32
  char *error_message = NULL;
  FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL,
                 errno_value,
                 MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
                 (LPTSTR) &error_message,
                 0,
                 NULL);
  jerry_port_log (JERRY_LOG_LEVEL_ERROR, "TCP Error: %s\n", error_message);
  LocalFree (error_message);
#else /* !WIN32 */
  jerry_port_log (JERRY_LOG_LEVEL_ERROR, "TCP Error: %s\n", strerror (errno_value));
#endif /* WIN32 */
} /* jerryx_debugger_tcp_log_error */

/**
 * Close a tcp connection.
 */
static void
jerryx_debugger_tcp_close (jerry_debugger_transport_header_t *header_p) /**< tcp implementation */
{
  JERRYX_ASSERT (!jerry_debugger_transport_is_connected ());

  jerryx_debugger_transport_tcp_t *tcp_p = (jerryx_debugger_transport_tcp_t *) header_p;

  JERRYX_DEBUG_MSG ("TCP connection closed.\n");

  jerryx_debugger_tcp_close_socket (tcp_p->tcp_socket);

  jerry_heap_free ((void *) header_p, sizeof (jerryx_debugger_transport_tcp_t));
} /* jerryx_debugger_tcp_close */

/**
 * Send data over a tcp connection.
 *
 * @return true - if the data has been sent successfully
 *         false - otherwise
 */
static bool
jerryx_debugger_tcp_send (jerry_debugger_transport_header_t *header_p, /**< tcp implementation */
                          uint8_t *message_p, /**< message to be sent */
                          size_t message_length) /**< message length in bytes */
{
  JERRYX_ASSERT (jerry_debugger_transport_is_connected ());

  jerryx_debugger_transport_tcp_t *tcp_p = (jerryx_debugger_transport_tcp_t *) header_p;

  do
  {
#ifdef __linux__
    ssize_t is_err = recv (tcp_p->tcp_socket, NULL, 0, MSG_PEEK);

    if (is_err == 0 && errno != JERRYX_EWOULDBLOCK)
    {
      int err_val = errno;
      jerry_debugger_transport_close ();
      jerryx_debugger_tcp_log_error (err_val);
      return false;
    }
#endif /* __linux__ */

    ssize_t sent_bytes = send (tcp_p->tcp_socket, message_p, message_length, 0);

    if (sent_bytes < 0)
    {
      int err_val = jerryx_debugger_tcp_get_errno ();

      if (err_val == JERRYX_EWOULDBLOCK)
      {
        continue;
      }

      jerry_debugger_transport_close ();
      jerryx_debugger_tcp_log_error (err_val);
      return false;
    }

    message_p += sent_bytes;
    message_length -= (size_t) sent_bytes;
  }
  while (message_length > 0);

  return true;
} /* jerryx_debugger_tcp_send */

/**
 * Receive data from a tcp connection.
 */
static bool
jerryx_debugger_tcp_receive (jerry_debugger_transport_header_t *header_p, /**< tcp implementation */
                             jerry_debugger_transport_receive_context_t *receive_context_p) /**< receive context */
{
  jerryx_debugger_transport_tcp_t *tcp_p = (jerryx_debugger_transport_tcp_t *) header_p;

  uint8_t *buffer_p = receive_context_p->buffer_p + receive_context_p->received_length;
  size_t buffer_size = JERRY_DEBUGGER_TRANSPORT_MAX_BUFFER_SIZE - receive_context_p->received_length;

  ssize_t length = recv (tcp_p->tcp_socket, buffer_p, buffer_size, 0);

  if (length <= 0)
  {
    int err_val = jerryx_debugger_tcp_get_errno ();

    if (err_val != JERRYX_EWOULDBLOCK || length == 0)
    {
      jerry_debugger_transport_close ();
      jerryx_debugger_tcp_log_error (err_val);
      return false;
    }
    length = 0;
  }

  receive_context_p->received_length += (size_t) length;

  if (receive_context_p->received_length > 0)
  {
    receive_context_p->message_p = receive_context_p->buffer_p;
    receive_context_p->message_length = receive_context_p->received_length;
  }

  return true;
} /* jerryx_debugger_tcp_receive */

/**
 * Utility method to prepare the server socket to accept connections.
 *
 * The following steps are performed:
 *  * Configure address re-use.
 *  * Bind the socket to the given port
 *  * Start listening on the socket.
 *
 * @return true if everything is ok
 *         false if there was an error
 */
static bool
jerryx_debugger_tcp_configure_socket (jerryx_socket server_socket, /** < socket to configure */
                                      uint16_t port) /** < port number to be used for the socket */
{
  struct sockaddr_in addr;

  addr.sin_family = AF_INET;
  addr.sin_port = htons (port);
  addr.sin_addr.s_addr = INADDR_ANY;

  int opt_value = 1;

  if (setsockopt (server_socket, SOL_SOCKET, SO_REUSEADDR, &opt_value, sizeof (int)) != 0)
  {
    return false;
  }

  if (bind (server_socket, (struct sockaddr *)&addr, sizeof (struct sockaddr_in)) != 0)
  {
    return false;
  }

  if (listen (server_socket, 1) != 0)
  {
    return false;
  }

  return true;
} /* jerryx_debugger_tcp_configure_socket */

/**
 * Create a tcp connection.
 *
 * @return true if successful,
 *         false otherwise
 */
bool
jerryx_debugger_tcp_create (uint16_t port) /**< listening port */
{
#ifdef WIN32
  WSADATA wsaData;
  int wsa_init_status = WSAStartup (MAKEWORD (2, 2), &wsaData);
  if (wsa_init_status != NO_ERROR)
  {
    JERRYX_ERROR_MSG ("WSA Error: %d\n", wsa_init_status);
    return false;
  }
#endif /* WIN32*/

  jerryx_socket server_socket = socket (AF_INET, SOCK_STREAM, 0);
  if (server_socket == JERRYX_SOCKET_INVALID)
  {
    jerryx_debugger_tcp_log_error (jerryx_debugger_tcp_get_errno ());
    return false;
  }

  if (!jerryx_debugger_tcp_configure_socket (server_socket, port))
  {
    int error = jerryx_debugger_tcp_get_errno ();
    jerryx_debugger_tcp_close_socket (server_socket);
    jerryx_debugger_tcp_log_error (error);
    return false;
  }

  JERRYX_DEBUG_MSG ("Waiting for client connection\n");

  struct sockaddr_in addr;
  socklen_t sin_size = sizeof (struct sockaddr_in);

  jerryx_socket tcp_socket = accept (server_socket, (struct sockaddr *)&addr, &sin_size);

  jerryx_debugger_tcp_close_socket (server_socket);

  if (tcp_socket == JERRYX_SOCKET_INVALID)
  {
    jerryx_debugger_tcp_log_error (jerryx_debugger_tcp_get_errno ());
    return false;
  }

  /* Set non-blocking mode. */
#ifdef WIN32
  u_long nonblocking_enabled = 1;
  if (ioctlsocket (tcp_socket, FIONBIO, &nonblocking_enabled) != NO_ERROR)
  {
    jerryx_debugger_tcp_close_socket (tcp_socket);
    return false;
  }
#else /* !WIN32 */
  int socket_flags = fcntl (tcp_socket, F_GETFL, 0);

  if (socket_flags < 0)
  {
    close (tcp_socket);
    return false;
  }

  if (fcntl (tcp_socket, F_SETFL, socket_flags | O_NONBLOCK) == -1)
  {
    close (tcp_socket);
    return false;
  }
#endif /* WIN32 */

  JERRYX_DEBUG_MSG ("Connected from: %s\n", inet_ntoa (addr.sin_addr));

  size_t size = sizeof (jerryx_debugger_transport_tcp_t);

  jerry_debugger_transport_header_t *header_p;
  header_p = (jerry_debugger_transport_header_t *) jerry_heap_alloc (size);

  if (!header_p)
  {
    jerryx_debugger_tcp_close_socket (tcp_socket);
    return false;
  }

  header_p->close = jerryx_debugger_tcp_close;
  header_p->send = jerryx_debugger_tcp_send;
  header_p->receive = jerryx_debugger_tcp_receive;

  ((jerryx_debugger_transport_tcp_t *) header_p)->tcp_socket = tcp_socket;

  jerry_debugger_transport_add (header_p,
                                0,
                                JERRY_DEBUGGER_TRANSPORT_MAX_BUFFER_SIZE,
                                0,
                                JERRY_DEBUGGER_TRANSPORT_MAX_BUFFER_SIZE);

  return true;
} /* jerryx_debugger_tcp_create */

#else /* !JERRY_DEBUGGER */

/**
 * Dummy function when debugger is disabled.
 *
 * @return false
 */
bool
jerryx_debugger_tcp_create (uint16_t port)
{
  JERRYX_UNUSED (port);
  return false;
} /* jerryx_debugger_tcp_create */

#endif /* JERRY_DEBUGGER */

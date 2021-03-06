/**
 @file  unix.c
 @brief ENet Unix system specific functions
*/
#ifndef _WIN32

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>

#define ENET_BUILDING_LIB 1
#include "enet/enet.h"

#ifdef __APPLE__
#ifdef HAS_POLL
#undef HAS_POLL
#endif
#ifndef HAS_FCNTL
#define HAS_FCNTL 1
#endif
#ifndef HAS_INET_PTON
#define HAS_INET_PTON 1
#endif
#ifndef HAS_INET_NTOP
#define HAS_INET_NTOP 1
#endif
#ifndef HAS_MSGHDR_FLAGS
#define HAS_MSGHDR_FLAGS 1
#endif
#ifndef HAS_SOCKLEN_T
#define HAS_SOCKLEN_T 1
#endif
#ifndef HAS_GETADDRINFO
#define HAS_GETADDRINFO 1
#endif
#ifndef HAS_GETNAMEINFO
#define HAS_GETNAMEINFO 1
#endif
#endif

#ifdef HAS_FCNTL
#include <fcntl.h>
#endif

#ifdef HAS_POLL
#include <sys/poll.h>
#endif

#ifndef HAS_SOCKLEN_T
typedef int socklen_t;
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

static enet_uint32 timeBase = 0;

static inline int
enet_address_get_size (const ENetAddress * address)
{
    switch (address -> family)
    {
        case AF_INET:  return (sizeof (struct sockaddr_in));
        case AF_INET6: return (sizeof (struct sockaddr_in6));
    };
	return 0;
}

int
enet_initialize (void)
{
    return 0;
}

void
enet_deinitialize (void)
{
}

enet_uint32
enet_host_random_seed (void)
{
    return (enet_uint32) time (NULL);
}

enet_uint32
enet_time_get (void)
{
    struct timeval timeVal;

    gettimeofday (& timeVal, NULL);

    return timeVal.tv_sec * 1000 + timeVal.tv_usec / 1000 - timeBase;
}

void
enet_time_set (enet_uint32 newTimeBase)
{
    struct timeval timeVal;

    gettimeofday (& timeVal, NULL);

    timeBase = timeVal.tv_sec * 1000 + timeVal.tv_usec / 1000 - newTimeBase;
}

const static struct addrinfo hints = {
  /*.ai_flags     =*/ AI_PASSIVE, // For wildcard IP address
  /*.ai_family    =*/ AF_UNSPEC,  // Allow IPv4 or IPv6
  /*.ai_socktype  =*/ SOCK_DGRAM, // Datagram socket
  /*.ai_protocol  =*/ 0,          // Any protocol
  /*.ai_addrlen   =*/ 0,
  /*.ai_addr      =*/ NULL,
  /*.ai_canonname =*/ NULL,
  /*.ai_next      =*/ NULL,
};

int
enet_address_set_host (ENetAddress * address, const char * name)
{
    struct addrinfo * resultList;
    int error_code;

    error_code = getaddrinfo(name, NULL, &hints, &resultList);
    if (error_code != 0)
    {
      if (resultList != NULL)
        freeaddrinfo (resultList);
      return error_code;
    }
    if (resultList == NULL) return -1;

    // We simply grab the first information (IPv6 is sorted first so we get IPv6 information when IPv6 is enabled!
    memcpy(address, resultList -> ai_addr, resultList -> ai_addrlen);
    address -> port = ENET_NET_TO_HOST_16 (address -> port);

    freeaddrinfo (resultList);
    return error_code;
}

int
enet_address_set_host_and_port(ENetAddress * address, const char * name, enet_uint16 port)
{
	struct addrinfo * resultList;
	int error_code;

	char portAsString[10];
	int count = snprintf(portAsString, 10, "%d", port);
	if (count > 9)
		count = 9;
	portAsString[count] = 0;

	error_code = getaddrinfo(name, portAsString, &hints, &resultList);
	if (error_code != 0)
	{
		if (resultList != NULL)
			freeaddrinfo(resultList);
		return error_code;
	}
	if (resultList == NULL) return -1;

	// We simply grab the first information (IPv6 is sorted first so we get IPv6 information when IPv6 is enabled!
	memcpy(address, resultList->ai_addr, resultList->ai_addrlen);
	address->port = ENET_NET_TO_HOST_16(address->port);

	freeaddrinfo(resultList);
	return error_code;
}

int
enet_address_get_host_ip (const ENetAddress * address, char * name, size_t nameLength)
{
    const void * host_ptr;
    switch (address -> family) {
        case AF_INET:
             host_ptr = & address -> ip.v4.host;
             break;
        case AF_INET6:
             host_ptr = & address -> ip.v6.host;
             break;
        default:
             host_ptr = NULL; // avoid wild pointer
    }
    return (inet_ntop (address -> family, (void *)host_ptr, name, nameLength) == NULL) ? -1 : 0;
}

int
enet_address_get_host (const ENetAddress * address, char * name, size_t nameLength)
{
    int error_code = getnameinfo((struct sockaddr *) address, enet_address_get_size(address),
                                 name, nameLength,
                                 NULL, 0,   // disregard service/socket name
                                 NI_DGRAM); // lookup via UPD when different

    return (error_code == 0) ? 0 : enet_address_get_host_ip (address, name, nameLength);
}

int
enet_socket_bind (ENetSocket socket, const ENetAddress * address)
{
    socklen_t length = enet_address_get_size (address);
    ENetAddress clone;

    memcpy (& clone, address, length);
    clone.port = ENET_HOST_TO_NET_16 (address -> port);

    return bind (socket, (struct sockaddr *) &clone, length);
}

int
enet_socket_get_address (ENetSocket socket, ENetAddress * address)
{
	socklen_t length = enet_address_get_size(address);

    if (getsockname (socket, (struct sockaddr *)address, & length) == -1)
      return -1;

    address -> port = ENET_NET_TO_HOST_16 (address -> port);

    return 0;
}

int
enet_socket_listen (ENetSocket socket, int backlog)
{
    return listen (socket, backlog < 0 ? SOMAXCONN : backlog);
}

ENetSocket
enet_socket_create (ENetSocketType type, enet_uint16 family)
{
    return socket (family, type == ENET_SOCKET_TYPE_DATAGRAM ? SOCK_DGRAM : SOCK_STREAM, 0);
}

int
enet_socket_set_option (ENetSocket socket, ENetSocketOption option, int value)
{
    int result = -1;
    switch (option)
    {
        case ENET_SOCKOPT_NONBLOCK:
#ifdef HAS_FCNTL
            result = fcntl (socket, F_SETFL, (value ? O_NONBLOCK : 0) | (fcntl (socket, F_GETFL) & ~O_NONBLOCK));
#else
            result = ioctl (socket, FIONBIO, & value);
#endif
            break;

        case ENET_SOCKOPT_BROADCAST:
            result = setsockopt (socket, SOL_SOCKET, SO_BROADCAST, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_REUSEADDR:
            result = setsockopt (socket, SOL_SOCKET, SO_REUSEADDR, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_RCVBUF:
            result = setsockopt (socket, SOL_SOCKET, SO_RCVBUF, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_SNDBUF:
            result = setsockopt (socket, SOL_SOCKET, SO_SNDBUF, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_RCVTIMEO:
        {
            struct timeval timeVal;
            timeVal.tv_sec = value / 1000;
            timeVal.tv_usec = (value % 1000) * 1000;
            result = setsockopt (socket, SOL_SOCKET, SO_RCVTIMEO, (char *) & timeVal, sizeof (struct timeval));
            break;
        }

        case ENET_SOCKOPT_SNDTIMEO:
        {
            struct timeval timeVal;
            timeVal.tv_sec = value / 1000;
            timeVal.tv_usec = (value % 1000) * 1000;
            result = setsockopt (socket, SOL_SOCKET, SO_SNDTIMEO, (char *) & timeVal, sizeof (struct timeval));
            break;
        }

        case ENET_SOCKOPT_NODELAY:
            result = setsockopt (socket, IPPROTO_TCP, TCP_NODELAY, (char *) & value, sizeof (int));
            break;

        case ENET_SOCKOPT_IPV6_V6ONLY:
            result = setsockopt (socket, IPPROTO_IPV6, IPV6_V6ONLY, (char *) & value, sizeof (int));
            break;

        default:
            break;
    }
    return result == -1 ? -1 : 0;
}

int
enet_socket_get_option (ENetSocket socket, ENetSocketOption option, int * value)
{
    int result = -1;
    socklen_t len;
    switch (option)
    {
        case ENET_SOCKOPT_ERROR:
            len = sizeof (int);
            result = getsockopt (socket, SOL_SOCKET, SO_ERROR, value, & len);
            break;

        default:
            break;
    }
    return result == -1 ? -1 : 0;
}

int
enet_socket_connect (ENetSocket socket, const ENetAddress * address)
{
    int result;
    socklen_t length = enet_address_get_size (address);
    ENetAddress clone;

    memcpy (& clone, address, length);
    clone.port = ENET_HOST_TO_NET_16 (address -> port);

    result = connect (socket, (struct sockaddr *) & clone, length);

    if (result == -1 && errno == EINPROGRESS)
      return 0;
    return result;
}

ENetSocket
enet_socket_accept (ENetSocket socket, ENetAddress * address)
{
    int result;
    socklen_t length = address != NULL ? enet_address_get_size (address) : 0;

    result = accept (socket,
                     address != NULL ? (struct sockaddr *) address : NULL,
                     address != NULL ? & length : NULL);

    if (result == -1)
        return ENET_SOCKET_NULL;

    if (address != NULL)
        address -> port = ENET_NET_TO_HOST_16 (address -> port);

    return result;
}

int
enet_socket_shutdown (ENetSocket socket, ENetSocketShutdown how)
{
    return shutdown (socket, (int) how);
}

void
enet_socket_destroy (ENetSocket socket)
{
    if (socket != -1)
      close (socket);
}

int
enet_socket_send (ENetSocket socket,
                  const ENetAddress * address,
                  const ENetBuffer * buffers,
                  size_t bufferCount)
{
    struct msghdr msgHdr;
    ENetAddress address_clone;
    int sentLength;

    memset (& msgHdr, 0, sizeof (struct msghdr));

    if (address != NULL)
    {
        msgHdr.msg_name = & address_clone;
        msgHdr.msg_namelen = enet_address_get_size (address);

        memcpy (& address_clone, address, msgHdr.msg_namelen);

        address_clone.port = ENET_HOST_TO_NET_16 (address -> port);
    }

    msgHdr.msg_iov = (struct iovec *) buffers;
    msgHdr.msg_iovlen = bufferCount;

    sentLength = sendmsg (socket, & msgHdr, MSG_NOSIGNAL);

    if (sentLength == -1)
    {
       if (errno == EWOULDBLOCK)
         return 0;

       return -1;
    }

    return sentLength;
}

int
enet_socket_receive (ENetSocket socket,
                     ENetAddress * address,
                     ENetBuffer * buffers,
                     size_t bufferCount)
{
    struct msghdr msgHdr;
    int recvLength;

    memset (& msgHdr, 0, sizeof (struct msghdr));

    if (address != NULL)
    {
        msgHdr.msg_name = address;
        msgHdr.msg_namelen =  enet_address_get_size (address);
    }

    msgHdr.msg_iov = (struct iovec *) buffers;
    msgHdr.msg_iovlen = bufferCount;

    recvLength = recvmsg (socket, & msgHdr, MSG_NOSIGNAL);

    if (recvLength == -1)
    {
       if (errno == EWOULDBLOCK)
         return 0;

       return -1;
    }

#ifdef HAS_MSGHDR_FLAGS
    if (msgHdr.msg_flags & MSG_TRUNC)
      return -1;
#endif

    if (address != NULL)
        address -> port = ENET_NET_TO_HOST_16 (address -> port);

    return recvLength;
}

int
enet_socketset_select (ENetSocket maxSocket, ENetSocketSet * readSet, ENetSocketSet * writeSet, enet_uint32 timeout)
{
    struct timeval timeVal;

    timeVal.tv_sec = timeout / 1000;
    timeVal.tv_usec = (timeout % 1000) * 1000;

    return select (maxSocket + 1, readSet, writeSet, NULL, & timeVal);
}

int
enet_socket_wait (ENetSocket socket, enet_uint32 * condition, enet_uint32 timeout)
{
#ifdef HAS_POLL
    struct pollfd pollSocket;
    int pollCount;

    pollSocket.fd = socket;
    pollSocket.events = 0;

    if (* condition & ENET_SOCKET_WAIT_SEND)
      pollSocket.events |= POLLOUT;

    if (* condition & ENET_SOCKET_WAIT_RECEIVE)
      pollSocket.events |= POLLIN;

    pollCount = poll (& pollSocket, 1, timeout);

    if (pollCount < 0)
    {
        if (errno == EINTR && * condition & ENET_SOCKET_WAIT_INTERRUPT)
        {
            * condition = ENET_SOCKET_WAIT_INTERRUPT;

            return 0;
        }

        return -1;
    }

    * condition = ENET_SOCKET_WAIT_NONE;

    if (pollCount == 0)
      return 0;

    if (pollSocket.revents & POLLOUT)
      * condition |= ENET_SOCKET_WAIT_SEND;

    if (pollSocket.revents & POLLIN)
      * condition |= ENET_SOCKET_WAIT_RECEIVE;

    return 0;
#else
    fd_set readSet, writeSet;
    struct timeval timeVal;
    int selectCount;

    timeVal.tv_sec = timeout / 1000;
    timeVal.tv_usec = (timeout % 1000) * 1000;

    FD_ZERO (& readSet);
    FD_ZERO (& writeSet);

    if (* condition & ENET_SOCKET_WAIT_SEND)
      FD_SET (socket, & writeSet);

    if (* condition & ENET_SOCKET_WAIT_RECEIVE)
      FD_SET (socket, & readSet);

    selectCount = select (socket + 1, & readSet, & writeSet, NULL, & timeVal);

    if (selectCount < 0)
    {
        if (errno == EINTR && * condition & ENET_SOCKET_WAIT_INTERRUPT)
        {
            * condition = ENET_SOCKET_WAIT_INTERRUPT;

            return 0;
        }

        return -1;
    }

    * condition = ENET_SOCKET_WAIT_NONE;

    if (selectCount == 0)
      return 0;

    if (FD_ISSET (socket, & writeSet))
      * condition |= ENET_SOCKET_WAIT_SEND;

    if (FD_ISSET (socket, & readSet))
      * condition |= ENET_SOCKET_WAIT_RECEIVE;

    return 0;
#endif
}

#endif

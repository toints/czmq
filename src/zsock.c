/*  =========================================================================
    zsock - high-level socket API that hides libzmq contexts and sockets

    Copyright (c) the Contributors as noted in the AUTHORS file.
    This file is part of CZMQ, the high-level C binding for 0MQ:
    http://czmq.zeromq.org.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

/*
@header
    The zsock class wraps the libzmq socket handle (a void *) with a proper
    structure that follows the CLASS rules for construction and destruction.
    Some zsock methods take a void * "polymorphic" reference, which can be
    either a zsock_t or a zactor_r reference, or a libzmq void *.
@discuss
@end
*/

#include "../include/czmq.h"

//  zsock_t instances always have this tag as the first 4 octets of
//  their data, which lets us do runtime object typing & validation.
#define ZSOCK_TAG           0x0004cafe

//  This port range is defined by IANA for dynamic or private ports
//  We use this when choosing a port for dynamic binding.
#define DYNAMIC_FIRST       0xc000    // 49152
#define DYNAMIC_LAST        0xffff    // 65535

//  Structure of our class

struct _zsock_t {
    uint32_t tag;               //  Object tag for runtime detection
    void *handle;               //  The libzmq socket handle
    int type;                   //  The socket type
    char *endpoint;             //  Last bound endpoint, if any
};


//  --------------------------------------------------------------------------
//  Create a new socket. This macro passes the caller source and line
//  number so that CZMQ can report socket leaks intelligently. To switch
//  off this checking, which may be costly if you use a LOT of sockets,
//  define ZSOCK_NOCHECK before compiling your code. Returns the new
//  socket, or NULL if the new socket could not be created.

zsock_t *
zsock_new_ (int type, const char *filename, size_t line_nbr)
{
    zsock_t *self = (zsock_t *) zmalloc (sizeof (zsock_t));
    if (!self)
        return NULL;

    self->tag = ZSOCK_TAG;
    self->type = type;
    self->handle = zsys_socket (type, filename, line_nbr);
    if (!self->handle) {
        free (self);
        return NULL;
    }
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the socket. You must use this for any socket created via the
//  zsock_new method.

void
zsock_destroy_ (zsock_t **self_p, const char *filename, size_t line_nbr)
{
    assert (self_p);
    if (*self_p) {
        zsock_t *self = *self_p;
        assert (zsock_is (self));
        self->tag = 0xDeadBeef;
        int rc = zsys_close (self->handle, filename, line_nbr);
        assert (rc == 0);
        free (self->endpoint);
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Smart constructors, which create sockets with additional set-up. In all of
//  these, the endpoint is NULL, or starts with '@' (connect) or '>' (bind).
//  Multiple endpoints are allowed, separated by commas. If endpoint does not
//  start with '@' or '>', default action depends on socket type.
//  Create a PUB socket. Default action is bind.

zsock_t *
zsock_new_pub_ (const char *endpoints, const char *filename, size_t line_nbr)
{
    zsock_t *sock = zsock_new_ (ZMQ_PUB, filename, line_nbr);
    if (zsock_attach (sock, endpoints, true))
        zsock_destroy (&sock);
    return sock;
}


//  --------------------------------------------------------------------------
//  Create a SUB socket, and optionally subscribe to some prefix string. Default
//  action is connect.

zsock_t *
zsock_new_sub_ (const char *endpoints, const char *subscribe, const char *filename, size_t line_nbr)
{
    zsock_t *sock = zsock_new_ (ZMQ_SUB, filename, line_nbr);
    if (zsock_attach (sock, endpoints, false))
        zsock_destroy (&sock);
    return sock;
}


//  --------------------------------------------------------------------------
//  Create a REQ socket. Default action is connect.

zsock_t *
zsock_new_req_ (const char *endpoints, const char *filename, size_t line_nbr)
{
    zsock_t *sock = zsock_new_ (ZMQ_REQ, filename, line_nbr);
    if (zsock_attach (sock, endpoints, false))
        zsock_destroy (&sock);
    return sock;
}


//  --------------------------------------------------------------------------
//  Create a REP socket. Default action is bind.

zsock_t *
zsock_new_rep_ (const char *endpoints, const char *filename, size_t line_nbr)
{
    zsock_t *sock = zsock_new_ (ZMQ_REP, filename, line_nbr);
    if (zsock_attach (sock, endpoints, true))
        zsock_destroy (&sock);
    return sock;
}


//  --------------------------------------------------------------------------
//  Create a DEALER socket. Default action is connect.

zsock_t *
zsock_new_dealer_ (const char *endpoints, const char *filename, size_t line_nbr)
{
    zsock_t *sock = zsock_new_ (ZMQ_DEALER, filename, line_nbr);
    if (zsock_attach (sock, endpoints, false))
        zsock_destroy (&sock);
    return sock;
}


//  --------------------------------------------------------------------------
//  Create a ROUTER socket. Default action is bind.

zsock_t *
zsock_new_router_ (const char *endpoints, const char *filename, size_t line_nbr)
{
    zsock_t *sock = zsock_new_ (ZMQ_ROUTER, filename, line_nbr);
    if (zsock_attach (sock, endpoints, true))
        zsock_destroy (&sock);
    return sock;
}


//  --------------------------------------------------------------------------
//  Create a PUSH socket. Default action is connect.

zsock_t *
zsock_new_push_ (const char *endpoints, const char *filename, size_t line_nbr)
{
    zsock_t *sock = zsock_new_ (ZMQ_PUSH, filename, line_nbr);
    if (zsock_attach (sock, endpoints, false))
        zsock_destroy (&sock);
    return sock;
}


//  --------------------------------------------------------------------------
//  Create a PULL socket. Default action is bind.

zsock_t *
zsock_new_pull_ (const char *endpoints, const char *filename, size_t line_nbr)
{
    zsock_t *sock = zsock_new_ (ZMQ_PULL, filename, line_nbr);
    if (zsock_attach (sock, endpoints, true))
        zsock_destroy (&sock);
    return sock;
}


//  --------------------------------------------------------------------------
//  Create an XPUB socket. Default action is bind.

zsock_t *
zsock_new_xpub_ (const char *endpoints, const char *filename, size_t line_nbr)
{
#if defined ZMQ_XPUB
    zsock_t *sock = zsock_new_ (ZMQ_XPUB, filename, line_nbr);
    if (zsock_attach (sock, endpoints, true))
        zsock_destroy (&sock);
    return sock;
#else
    return NULL;            //  Not implemented
#endif
}


//  --------------------------------------------------------------------------
//  Create an XSUB socket. Default action is connect.

zsock_t *
zsock_new_xsub_ (const char *endpoints, const char *filename, size_t line_nbr)
{
#if defined ZMQ_XSUB
    zsock_t *sock = zsock_new_ (ZMQ_XSUB, filename, line_nbr);
    if (zsock_attach (sock, endpoints, false))
        zsock_destroy (&sock);
    return sock;
#else
    return NULL;            //  Not implemented
#endif
}


//  --------------------------------------------------------------------------
//  Create a PAIR socket. Default action is connect.

zsock_t *
zsock_new_pair_ (const char *endpoints, const char *filename, size_t line_nbr)
{
    zsock_t *sock = zsock_new_ (ZMQ_PAIR, filename, line_nbr);
    if (zsock_attach (sock, endpoints, false))
        zsock_destroy (&sock);
    return sock;
}


//  --------------------------------------------------------------------------
//  Create a STREAM socket. Default action is connect.

zsock_t *
zsock_new_stream_ (const char *endpoints, const char *filename, size_t line_nbr)
{
#if defined ZMQ_STREAM
    zsock_t *sock = zsock_new_ (ZMQ_STREAM, filename, line_nbr);
    if (zsock_attach (sock, endpoints, false))
        zsock_destroy (&sock);
    return sock;
#else
    return NULL;            //  Not implemented
#endif
}


//  --------------------------------------------------------------------------
//  Bind a socket to a formatted endpoint. For tcp:// endpoints, supports
//  ephemeral ports, if you specify the port number as "*". By default
//  zsock uses the IANA designated range from C000 (49152) to FFFF (65535).
//  To override this range, follow the "*" with "[first-last]". Either or
//  both first and last may be empty. To bind to a random port within the
//  range, use "!" in place of "*".
//
//  Examples:
//  tcp://127.0.0.1:*                bind to first free port from C000 up
//  tcp://127.0.0.1:!                bind to random port from C000 to FFFF
//  tcp://127.0.0.1:*[60000-]        bind to first free port from 60000 up
//  tcp://127.0.0.1:![55000-55999]   bind to random port from 55000-55999
//
//  On success, returns the actual port number used, for tcp:// endpoints,
//  and 0 for other transports. On failure, returns -1. Note that when using
//  ephemeral ports, a port may be reused by different services without
//  clients being aware. Protocols that run on ephemeral ports should take
//  this into account.

int
zsock_bind (zsock_t *self, const char *format, ...)
{
    assert (self);
    assert (zsock_is (self));

    //  Expand format to get full endpoint
    va_list argptr;
    va_start (argptr, format);
    char *endpoint = zsys_vprintf (format, argptr);
    va_end (argptr);
    int rc;
    
    //  If tcp:// endpoint, parse to get or make port number
    zrex_t *rex = zrex_new (NULL);
    if (zrex_eq (rex, endpoint, "^tcp://.*:(\\d+)$")) {
        assert (zrex_hits (rex) == 2);
        if (zmq_bind (self->handle, endpoint) == 0)
            rc = atoi (zrex_hit (rex, 1));
        else
            rc = -1;
    }
    else
    if (zrex_eq (rex, endpoint, "^(tcp://.*):([*!])(\\[(\\d+)?-(\\d+)?\\])?$")) {
        assert (zrex_hits (rex) == 6);
        const char *hostname, *opcode, *group, *first_str, *last_str;
        zrex_fetch (rex, &hostname, &opcode, &group, &first_str, &last_str, NULL);
        
        int first = *first_str? atoi (first_str): DYNAMIC_FIRST;
        int last = *last_str? atoi (last_str): DYNAMIC_LAST;

        //  This is how many times we'll try before giving up
        int attempts = last - first + 1;

        //  If operator is '*', take first available port.
        //  If operator is '!', take a random leap into our port space; we'll
        //  still scan sequentially to make sure we find a free slot rapidly.
        int port = first;
        if (streq (opcode, "!"))
            port += randof (attempts);
            
        rc = -1;                //  Assume we don't succeed
        while (rc == -1 && attempts--) {
            free (endpoint);
            endpoint = zsys_sprintf ("%s:%d", hostname, port);
            if (zmq_bind (self->handle, endpoint) == 0)
                rc = port;
            if (++port > last)
                port = first;
        }
    }
    else
        rc = zmq_bind (self->handle, endpoint);

    //  Store successful endpoint for later reference
    if (rc >= 0) {
        free (self->endpoint);
        self->endpoint = endpoint;
    }
    else
        free (endpoint);
    
    zrex_destroy (&rex);
    return rc;
}


//  --------------------------------------------------------------------------
//  Returns last bound endpoint, if any.

char *
zsock_endpoint (zsock_t *self)
{
    assert (self);
    return self->endpoint;
}


//  --------------------------------------------------------------------------
//  Unbind a socket from a formatted endpoint.
//  Returns 0 if OK, -1 if the endpoint was invalid or the function
//  isn't supported.

int
zsock_unbind (zsock_t *self, const char *format, ...)
{
    assert (self);
    assert (zsock_is (self));

#if (ZMQ_VERSION >= ZMQ_MAKE_VERSION (3,2,0))
    //  Expand format to get full endpoint
    va_list argptr;
    va_start (argptr, format);
    char *endpoint = zsys_vprintf (format, argptr);
    va_end (argptr);
    int rc = zmq_unbind (self->handle, endpoint);
    free (endpoint);
    return rc;
#else
    return -1;
#endif
}


//  --------------------------------------------------------------------------
//  Connect a socket to a formatted endpoint
//  Returns 0 if the endpoint is valid, -1 if the connect failed.

int
zsock_connect (zsock_t *self, const char *format, ...)
{
    assert (self);
    assert (zsock_is (self));

    //  Expand format to get full endpoint
    va_list argptr;
    va_start (argptr, format);
    char *endpoint = zsys_vprintf (format, argptr);
    va_end (argptr);
    int rc = zmq_connect (self->handle, endpoint);
    
#if (ZMQ_VERSION < ZMQ_MAKE_VERSION (4,0,0))
    int retries = 4;
    while (rc == -1 && zmq_errno () == ECONNREFUSED && retries) {
        //  This bruteforces a synchronization between connecting and
        //  binding threads on ZeroMQ v3.2 and earlier, where the bind
        //  MUST happen before the connect on inproc transports.
        zclock_sleep (250);
        rc = zmq_connect (self->handle, endpoint);
        retries--;
    }
#endif
    free (endpoint);
    return rc;
}


//  --------------------------------------------------------------------------
//  Disconnect a socket from a formatted endpoint
//  Returns 0 if disconnection is complete -1 if the disconnection failed.

int
zsock_disconnect (zsock_t *self, const char *format, ...)
{
    assert (self);
    assert (zsock_is (self));

#if (ZMQ_VERSION >= ZMQ_MAKE_VERSION (3,2,0))
    //  Expand format to get full endpoint
    va_list argptr;
    va_start (argptr, format);
    char *endpoint = zsys_vprintf (format, argptr);
    va_end (argptr);
    int rc = zmq_disconnect (self->handle, endpoint);
    free (endpoint);
    return rc;
#else
    return -1;
#endif
}


//  --------------------------------------------------------------------------
//  Attach a socket to zero or more endpoints. If endpoints is not null,
//  parses as list of ZeroMQ endpoints, separated by commas, and prefixed by
//  '@' (to bind the socket) or '>' (to attach the socket). Returns 0 if all
//  endpoints were valid, or -1 if there was a syntax error. If the endpoint
//  does not start with '@' or '>', the serverish argument defines whether
//  it is used to bind (serverish = true) or connect (serverish = false).

int
zsock_attach (zsock_t *self, const char *endpoints, bool serverish)
{
    assert (self);
    if (!endpoints)
        return 0;

    //  We hold each individual endpoint here
    char endpoint [256];
    while (*endpoints) {
        const char *delimiter = strchr (endpoints, ',');
        if (!delimiter)
            delimiter = endpoints + strlen (endpoints);
        if (delimiter - endpoints > 255)
            return -1;
        memcpy (endpoint, endpoints, delimiter - endpoints);
        endpoint [delimiter - endpoints] = 0;

        int rc;
        if (endpoint [0] == '@')
            rc = zsock_bind (self, "%s", endpoint + 1);
        else
        if (endpoint [0] == '>')
            rc = zsock_connect (self, "%s", endpoint + 1);
        else
        if (serverish)
            rc = zsock_bind (self, "%s", endpoint);
        else
            rc = zsock_connect (self, "%s", endpoint);
        
        if (rc == -1)
            return -1;          //  Bad endpoint syntax
            
        if (*delimiter == 0)
            break;
        endpoints = delimiter + 1;
    }
    return 0;
}


//  --------------------------------------------------------------------------
//  Returns socket type as printable constant string. 

const char *
zsock_type_str (zsock_t *self)
{
    assert (self);
    assert (zsock_is (self));

    //  This array matches ZMQ_XXX type definitions
    assert (ZMQ_PAIR == 0);
    char *type_names [] = {
        "PAIR", "PUB", "SUB", "REQ", "REP",
        "DEALER", "ROUTER", "PULL", "PUSH",
        "XPUB", "XSUB", "STREAM"
    };
    return type_names [self->type];
}


//  --------------------------------------------------------------------------
//  Send a 'picture' message to the socket (or actor). The picture is a
//  string that defines the type of each frame. This makes it easy to send
//  a complex multiframe message in one call. The picture can contain any
//  of these characters, each corresponding to one or two arguments:
//  
//     i = int
//     s = char *
//     b = byte *, size_t (2 arguments)
//     c = zchunk_t *
//     f = zframe_t *
//
//  Note that b, c, and f are encoded the same way and the choice is offered
//  as a convenience to the sender, which may or may not already have data
//  in a zchunk or zframe. Does not change or take ownership of any arguments.
//  Returns 0 if successful, -1 if sending failed for any reason.

int
zsock_send (void *self, const char *picture, ...)
{
    assert (self);
    assert (picture);

    va_list argptr;
    va_start (argptr, picture);
    zmsg_t *msg = zmsg_new ();
    while (*picture) {
        if (*picture == 'i')
            zmsg_addstrf (msg, "%d", va_arg (argptr, int));
        else
        if (*picture == 's')
            zmsg_addstr (msg, va_arg (argptr, char *));
        else
        if (*picture == 'b') {
            //  Note function arguments may be expanded in reverse order,
            //  so we cannot use va_arg macro twice in a single call
            byte *data = va_arg (argptr, byte *);
            zmsg_addmem (msg, data, va_arg (argptr, size_t));
        }
        else
        if (*picture == 'c') {
            zchunk_t *chunk = va_arg (argptr, zchunk_t *);
            assert (zchunk_is (chunk));
            zmsg_addmem (msg, zchunk_data (chunk), zchunk_size (chunk));
        }
        else
        if (*picture == 'f') {
            zframe_t *frame = va_arg (argptr, zframe_t *);
            assert (zframe_is (frame));
            zmsg_addmem (msg, zframe_data (frame), zframe_size (frame));
        }
        else {
            zsys_error ("zsock: invalid picture element '%c'", *picture);
            assert (false);
        }
        picture++;
    }
    va_end (argptr);
    return zmsg_send (&msg, self);
}


//  --------------------------------------------------------------------------
//  Receive a 'picture' message to the socket (or actor). See zsock_send for
//  the format and meaning of the picture. Returns the picture elements into
//  a series of pointers as provided by the caller:
//
//     i = int *
//     s = char ** (allocates new string)
//     b = byte **, size_t * (2 arguments) (allocates memory)
//     c = zchunk_t ** (creates zchunk)
//     f = zframe_t ** (creates zframe)
//
//  Note that zsock_recv creates the returned objects, and the caller must
//  destroy them when finished with them. The supplied pointers do not need
//  to be initialized. Returns 0 if successful, or -1 if it failed to recv
//  a message, in which case the pointers are not modified.

int
zsock_recv (void *self, const char *picture, ...)
{
    assert (self);
    assert (picture);
    zmsg_t *msg = zmsg_recv (self);
    if (!msg)
        return -1;              //  Interrupted

    va_list argptr;
    va_start (argptr, picture);
    while (*picture) {
        if (*picture == 'i') {
            char *string = zmsg_popstr (msg);
            *(va_arg (argptr, int *)) = atoi (string);
            free (string);
        }
        else
        if (*picture == 's')
            *(va_arg (argptr, char **)) = zmsg_popstr (msg);
        else
        if (*picture == 'b') {
            zframe_t *frame = zmsg_pop (msg);
            byte **data = va_arg (argptr, byte **);
            size_t *size = va_arg (argptr, size_t *);
            *size = zframe_size (frame);
            *data = malloc (*size);
            memcpy (*data, zframe_data (frame), *size);
            zframe_destroy (&frame);
        }
        else
        if (*picture == 'c') {
            zframe_t *frame = zmsg_pop (msg);
            *(va_arg (argptr, zchunk_t **)) = zchunk_new (zframe_data (frame), zframe_size (frame));
            zframe_destroy (&frame);
        }
        else
        if (*picture == 'f')
            *(va_arg (argptr, zframe_t **)) = zmsg_pop (msg);
        else {
            zsys_error ("zsock: invalid picture element '%c'", *picture);
            assert (false);
        }
        picture++;
    }
    va_end (argptr);
    zmsg_destroy (&msg);
    return 0;
}


//  --------------------------------------------------------------------------
//  Set socket to use unbounded pipes (HWM=0); use this in cases when you are
//  totally certain the message volume can fit in memory. This method works
//  across all versions of ZeroMQ. Takes a polymorphic socket reference.

void
zsock_set_unbounded (void *self)
{
#if (ZMQ_VERSION_MAJOR == 2)
    zsock_set_hwm (self, 0);
#else
    zsock_set_sndhwm (self, 0);
    zsock_set_rcvhwm (self, 0);
#endif
}


//  --------------------------------------------------------------------------
//  Send a signal over a socket. A signal is a short message carrying a
//  success/failure code (by convention, 0 means OK). Signals are encoded
//  to be distinguishable from "normal" messages. Accepts a zock_t or a
//  zactor_t argument, and returns 0 if successful, -1 if the signal could
//  not be sent. Takes a polymorphic socket reference.

int
zsock_signal (void *self, byte status)
{
    assert (self);
    int64_t signal_value = 0x7766554433221100L + status;
    zmsg_t *msg = zmsg_new ();
    zmsg_addmem (msg, &signal_value, 8);
    return zmsg_send (&msg, self);
}


//  --------------------------------------------------------------------------
//  Wait on a signal. Use this to coordinate between threads, over pipe
//  pairs. Blocks until the signal is received. Returns -1 on error, 0 or
//  greater on success. Accepts a zsock_t or a zactor_t as argument.
//  Takes a polymorphic socket reference.

int
zsock_wait (void *self)
{
    assert (self);

    //  A signal is a message containing one frame with our 8-byte magic 
    //  value. If we get anything else, we discard it and continue to look
    //  for the signal message
    while (true) {
        zmsg_t *msg = zmsg_recv (self);
        if (!msg)
            return -1;
        if (zmsg_size (msg) == 1
        &&  zmsg_content_size (msg) == 8) {
            zframe_t *frame = zmsg_first (msg);
            int64_t signal_value = *((int64_t *) zframe_data (frame));
            if ((signal_value & 0xFFFFFFFFFFFFFF00L) == 0x7766554433221100L) {
                zmsg_destroy (&msg);
                return signal_value & 255;
            }
        }
        zmsg_destroy (&msg);
    }
    return -1;
}


//  --------------------------------------------------------------------------
//  Probe the supplied object, and report if it looks like a zsock_t.
//  Takes a polymorphic socket reference.

bool
zsock_is (void *self)
{
    assert (self);
    return ((zsock_t *) self)->tag == ZSOCK_TAG;
}


//  --------------------------------------------------------------------------
//  Probe the supplied reference. If it looks like a zsock_t instance,
//  return the underlying libzmq socket handle; else if it looks like
//  a libzmq socket handle, return the supplied value. Takes a
//  polymorphic socket reference.

void *
zsock_resolve (void *self)
{
    assert (self);
    if (zsock_is (self))
        return ((zsock_t *) self)->handle;
    else
    if (zactor_is (self))
        return zactor_resolve (self);
    else 
        return self;
}


//  --------------------------------------------------------------------------
//  Selftest

void
zsock_test (bool verbose)
{
    printf (" * zsock: ");
    if (verbose)
        printf ("\n");

    //  @selftest
    zsock_t *writer = zsock_new_push ("@tcp://127.0.0.1:5560");
    assert (writer);
    assert (zsock_resolve (writer) != writer);
    assert (streq (zsock_type_str (writer), "PUSH"));

    int rc;
#if (ZMQ_VERSION >= ZMQ_MAKE_VERSION (3,2,0))
    //  Check unbind
    rc = zsock_unbind (writer, "tcp://127.0.0.1:%d", 5560);
    assert (rc == 0);

    //  In some cases and especially when running under Valgrind, doing
    //  a bind immediately after an unbind causes an EADDRINUSE error.
    //  Even a short sleep allows the OS to release the port for reuse.
    zclock_sleep (100);

    //  Bind again
    rc = zsock_bind (writer, "tcp://127.0.0.1:%d", 5560);
    assert (rc == 5560);
    assert (streq (zsock_endpoint (writer), "tcp://127.0.0.1:5560"));
#endif

    zsock_t *reader = zsock_new_pull (">tcp://127.0.0.1:5560");
    assert (reader);
    assert (zsock_resolve (reader) != reader);
    assert (streq (zsock_type_str (reader), "PULL"));

    zstr_send (writer, "Hello, World");
    zmsg_t *msg = zmsg_recv (reader);
    assert (msg);
    char *string = zmsg_popstr (msg);
    assert (streq (string, "Hello, World"));
    free (string);
    zmsg_destroy (&msg);

    //  Test zsock_send/recv pictures
    zchunk_t *chunk = zchunk_new ("HELLO", 5);
    zframe_t *frame = zframe_new ("WORLD", 5);
    
    zsock_send (writer, "isbcf", 12345, "This is a string", "ABCDE", 5, chunk, frame);
    msg = zmsg_recv (reader);
    assert (msg);
    if (verbose)
        zmsg_print (msg);
    zmsg_destroy (&msg);
    
    zsock_send (writer, "isbcf", 12345, "This is a string", "ABCDE", 5, chunk, frame);
    zframe_destroy (&frame);
    zchunk_destroy (&chunk);
    int integer;
    byte *data;
    size_t size;
    rc = zsock_recv (reader, "isbcf", &integer, &string, &data, &size, &chunk, &frame);
    assert (rc == 0);
    assert (integer == 12345);
    assert (streq (string, "This is a string"));
    assert (memcmp (data, "ABCDE", 5) == 0);
    assert (size == 5);
    assert (memcmp (zchunk_data (chunk), "HELLO", 5) == 0);
    assert (zchunk_size (chunk) == 5);
    assert (memcmp (zframe_data (frame), "WORLD", 5) == 0);
    assert (zframe_size (frame) == 5);
    free (string);
    free (data);
    zframe_destroy (&frame);
    zchunk_destroy (&chunk);
    
    //  Test binding to ephemeral ports, sequential and random
    int port = zsock_bind (writer, "tcp://127.0.0.1:*");
    assert (port >= DYNAMIC_FIRST && port <= DYNAMIC_LAST);
    port = zsock_bind (writer, "tcp://127.0.0.1:*[50000-]");
    assert (port >= 50000 && port <= DYNAMIC_LAST);
    port = zsock_bind (writer, "tcp://127.0.0.1:*[-50001]");
    assert (port >= DYNAMIC_FIRST && port <= 50001);
    port = zsock_bind (writer, "tcp://127.0.0.1:*[60000-60010]");
    assert (port >= 60000 && port <= 60010);
    
    port = zsock_bind (writer, "tcp://127.0.0.1:!");
    assert (port >= DYNAMIC_FIRST && port <= DYNAMIC_LAST);
    port = zsock_bind (writer, "tcp://127.0.0.1:![50000-]");
    assert (port >= 50000 && port <= DYNAMIC_LAST);
    port = zsock_bind (writer, "tcp://127.0.0.1:![-50001]");
    assert (port >= DYNAMIC_FIRST && port <= 50001);
    port = zsock_bind (writer, "tcp://127.0.0.1:![60000-60010]");
    assert (port >= 60000 && port <= 60010);

    //  Test zsock_endpoint method
    rc = zsock_bind (writer, "inproc://test.%s", "writer");
    assert (rc == 0);
    assert (streq (zsock_endpoint (writer), "inproc://test.writer"));
    
    //  Test error state when connecting to an invalid socket type
    //  ('txp://' instead of 'tcp://', typo intentional)
    rc = zsock_connect (reader, "txp://127.0.0.1:5560");
    assert (rc == -1);

    rc = zsock_signal (writer, 123);
    assert (rc == 0);
    rc = zsock_wait (reader);
    assert (rc == 123);

    zsock_destroy (&reader);
    zsock_destroy (&writer);

    //  Test zsock_attach method
    zsock_t *server = zsock_new (ZMQ_DEALER);
    rc = zsock_attach (server, "@inproc://myendpoint,tcp://127.0.0.1:5556,inproc://others", true);
    assert (rc == 0);
    rc = zsock_attach (server, "", false);
    assert (rc == 0);
    rc = zsock_attach (server, NULL, true);
    assert (rc == 0);
    rc = zsock_attach (server, ">a,@b, c,, ", false);
    assert (rc == -1);
    zsock_destroy (&server);
    //  @end

    printf ("OK\n");
}

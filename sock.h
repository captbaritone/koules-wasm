#ifndef _SOCK_INCLUDED
#define _SOCK_INCLUDED
#include <netinet/in.h>
extern void SetTimeout (int, int);
extern int GetPortNum (int);
extern char *GetSockAddr (int);
extern int SetSocketReceiveBufferSize (int, int);
extern int SetSocketSendBufferSize (int, int);
extern int SetSocketNonBlocking (int, int);
extern int GetSocketError (int);
extern int SocketReadable (int);
extern int DgramConnect (int, char *, int);
extern int DgramSend (int, char *, int, char *, int);
extern int DgramReceiveAny (int, char *, int);
extern int DgramReceiveConnected (int, char *, int);
extern int DgramConnect (int, char *, int);
extern int CreateDgramSocket (int);
extern int SocketClose (int);
extern char *DgramLastaddr (void);
extern int DgramLastport (void);
/* WebSocket transports know when a peer has gone; UDP did not. Returns
 * 1 and fills buf with the peer id, or 0 when there is nothing queued. */
extern int DgramTakeGone (char *buf, int max);
#endif

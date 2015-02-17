/******************************************************************************
* echo_server.c                                                               *
*                                                                             *
* Description: This file contains the C source code for an echo server.  The  *
*              server runs on a hard-coded port and simply write back anything*
*              sent to it by connected clients.  It does not support          *
*              concurrent clients.                                            *
*                                                                             *
* Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                         *
*          Wolf Richter <wolf@cs.cmu.edu>
* Modifier: Billy Li <junchenl@andrew.cmu.edu>                                *
*                                                                             *
*******************************************************************************/

#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/select.h>
#include <fcntl.h>

#define ECHO_PORT 9999
#define BUF_SIZE 4096
/*
 * Mechanism of Select:
Add listening socket into select's READ set;
for each select() return, do
if existing socket is ready to read, read data into buffer
if existing socket is ready to write, write buffer to socket
if listening socket is ready to read, accept new connection.
Add new connection into READ set
If existing socket's buffer is not full, add it to READ set
If existing socket's buffer is not empty, add it to WRITE set
If existing socket has closed, remove it from list.
*/

typedef struct
{
    int maxfd;
    fd_set read_set;
    fd_set ready_set;
    int nready;
    int maxi;
    int clientfd[FD_SETSIZE];
} clientpool;

void init_clientpool(int sock, clientpool *p)
{
    int i;
    p->maxi = -1;
    /* Mark unavailable client file descriptor as -1*/
    for (i = 0; i < FD_SETSIZE; i++)
        p->clientfd[i] = -1;

    /* sock is the only member of select read set */
    p->maxfd = sock;
    FD_ZERO(&p->read_set);
    FD_SET(sock, &p->read_set);
}

int add_client(int client_sock, clientpool *p)
{
    int i, x;
    p->nready--;

    x = fcntl(client_sock, F_GETFL, 0);
    fcntl(client_sock, F_SETFL, x | O_NONBLOCK);

    for (i = 0; i < FD_SETSIZE; i++)
    {
        if (p->clientfd[i] < 0)
        {
            /* add connected descriptor to the pool */
            p->clientfd[i] = client_sock;

            /* add the descriptor to the descriptor set */
            FD_SET(client_sock, &p->read_set);

            /* update max descriptor and clientpool maxmium */
            p->maxfd = (client_sock > p->maxfd) ? client_sock : p->maxfd;
            p->maxi = (i > p->maxi) ? i : p->maxi;
            break;
        }
    }
    /* in case when there are too many clients */
    if (i == FD_SETSIZE)
        return 1;
    return 0;
}

int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}

int check_clients(clientpool *p)
{
    int i, client_sock, malicious;
    ssize_t readret;
    char buf[BUF_SIZE];

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++)
    {
        client_sock = p->clientfd[i];

        /* if the descriptor is ready, receive it and send it back */
        if ((client_sock > 0) && FD_ISSET(client_sock, &p->ready_set))
        {
            p->nready--;
            readret = 0;
            malicious = -1;
            while ((readret = recv(client_sock, buf, BUF_SIZE, 0)) >= 1)
            {
                printf("%d\n", FD_ISSET(client_sock, &p->ready_set));
                if (send(client_sock, buf, readret, 0) != readret)
                {
                    close_socket(client_sock);
                    fprintf(stderr, "Error sending to client.\n");
                    return 1;
                }
                memset(buf, 0, BUF_SIZE);
                malicious++;
            }

	    if (readret == 0 || malicious)
            {
	    /* if malicious connection or receiving is done,
	     close the client socket */
	        if (close_socket(client_sock))
                {
                    fprintf(stderr, "Error closing client socket.\n");
		    return 1;
	        }
	        FD_CLR(client_sock, &p->read_set);
	        p->clientfd[i] = -1;
	    }
	}
    }

    return 0;
}

int main(int argc, char* argv[])
{
    int sock, client_sock, port;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;
    static clientpool pool;

    if (argc != 2)
    {
		fprintf(stderr, "please input: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
    }
    port = atoi(argv[1]);

    fprintf(stdout, "----- Echo Server -----\n");

    /* all networked programs must create a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Failed to create a socket.\n");
        return EXIT_FAILURE;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)))
    {
        close_socket(sock);
        fprintf(stderr, "Failed binding socket.\n");
        return EXIT_FAILURE;
    }

    if (listen(sock, 5))
    {
        close_socket(sock);
        fprintf(stderr, "Error listening on socket.\n");
        return EXIT_FAILURE;
    }
    init_clientpool(sock, &pool);

    /* finally, loop waiting for input and then write it back */
    while (1)
    {
       cli_size = sizeof(cli_addr);
       pool.ready_set = pool.read_set;
       pool.nready = select(pool.maxfd + 1, &pool.ready_set, NULL, NULL, NULL);
       if (FD_ISSET(sock, &pool.ready_set))
       {
	       if ((client_sock = accept(sock, (struct sockaddr *) &cli_addr,
                                 &cli_size)) == -1)
       {
           close(sock);
           fprintf(stderr, "Error accepting connection.\n");
           return EXIT_FAILURE;
       }
       /* add the client to the pool */
	if (add_client(client_sock, &pool) != 0)
	{
		close_socket(sock);
		fprintf(stderr, "Error adding client: Too many clients.\n");
       		return EXIT_FAILURE;
	}
       }

       /* check each client and process the ready connected descriptor */
	if (check_clients(&pool) != 0)
       	{
		close_socket(sock);
		return EXIT_FAILURE;
	}
    }

    close_socket(sock);

    return EXIT_SUCCESS;
}

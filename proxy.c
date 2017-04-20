// proxy 
/*----------------------------------------------------------------------------
 * Name : Choi ho yong
 * Student ID : 20130672
 * File name : proxy.c
 *
 * Project 2. Building an HTTP Proxy
 *--------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#define BACKLOG 10   // how many pending connections queue will hold
#define PACKETSIZE 1024     // maximum of capacity of one time transmission
#define MAXDATASIZE 1000000 // maximum of file capacity

// check_port_number : check command line correctness
// ./server -p PORTNUM
// 1024 <= PORTNUM <= 65535
void check_port_number (int argc, char** argv)
{
  if (argc != 2)
  {
    perror ("server : argument\n");
    exit (1);
  }
  
  for (int i = 0; i < strlen (argv[1]); i++)
  {
    if (isdigit (argv[1][i]) == 0)
    {
      perror ("server : PORT number\n");
      exit (1);
    }
  }

  if ((atoi (argv[1]) > 65535) || (atoi (argv[1]) < 1024))
  {
    perror ("server : PORT number\n");
    exit (1);
  }
}
  
// main : make server which listen to client's messages
int main (int argc, char** argv)
{
  int sockfd, new_sockfd;
  int bytes, location;
  socklen_t sock_in_size;
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr;
  int yes = 1, success;
  char s[INET6_ADDRSTRLEN];
  char packet[PACKETSIZE];
  char buff[MAXDATASIZE];

  memset (buff, 0, MAXDATASIZE);

  check_port_number (argc, argv);

  memset (&hints, 0, sizeof (hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((success = getaddrinfo (NULL, argv[1], &hints, &servinfo)) != 0)
  {
    fprintf (stderr, "getaddrinfo : %s\n", gai_strerror (success));
    return 1;
  }

  for (p = servinfo; p != NULL; p = p->ai_next)
  {
    if ((sockfd = socket (p->ai_family, p->ai_socktype,\
                          p->ai_protocol)) == -1)
    {
      perror ("server : socket\n");
      continue;
    }

    if (setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
      perror ("server : setsockopt\n");
      exit (1);
    }

    if (bind (sockfd, p->ai_addr, p->ai_addrlen) == -1)
    {
      close (sockfd);
      perror ("server : bind\n");
      continue;
    }

    break;
  }

  if (p == NULL)
  {
    fprintf (stderr, "server : failed to bind\n");
    return 2;
  }

  freeaddrinfo (servinfo);
  // server bind certain PORT number which you put into server.
  // server are ready for listening client's connection request.

  if (listen (sockfd, BACKLOG) == -1)
  {
    perror ("server : listen\n");
    exit (1);
  }

  while (1)
  {
    sock_in_size = sizeof (their_addr);
    new_sockfd = accept (sockfd, (struct sockaddr *) & their_addr, &sock_in_size);
    if (new_sockfd == -1)
    {
      perror ("server : accept\n");
      continue;
    }
    
    // server and client are connected.
    // For connection of other clients, Use fork function to perform listening
    // and transmissing at the same time.
    // We divide big file at packet, and transmit separately.
    if (!fork ()) // child process come here
    {
      while (1)
      {
        memset (packet, 0, PACKETSIZE);
        if ((bytes = recv (new_sockfd, packet, PACKETSIZE, 0)) == -1)
        {
          perror ("server : recv\n");
          exit (1);
        }
        
        if (bytes == 0) //client is finished.
        {
          close (new_sockfd);
          return 0;
        }

        if (bytes != PACKETSIZE) //last packet is arrived.
        {

          if (strlen (buff) != 0)
          {
            if (!((bytes == 1) && (packet[0] == '\n'))) strcat (buff, packet);
            strcat (buff, "\0");
            fputs (buff, stdout);
            memset (buff, 0, MAXDATASIZE); 
          }

          else
          {
            if (packet[0] == -1) close (new_sockfd); 
            packet[PACKETSIZE] = '\0';
            fputs (packet, stdout); 
          }
        }

        else //the file is big, so last packet will send after this.
        { 
          if (strlen (buff) == 0) strncpy (buff, packet, PACKETSIZE);
          else strncat (buff, packet, PACKETSIZE);

          if (packet[PACKETSIZE - 1] == -1)
          {
            fputs (buff, stdout);
            memset (buff, 0, strlen (buff));
          }
        }
      }
    }
    else close (new_sockfd); // parent process come here

    while (waitpid (-1, NULL, WNOHANG) > 0); // reap the zombie processes
  }

}








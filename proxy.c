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
#define MAXDATASIZE 1000000 // maximum of file capacity
#define REQUESTMAX 50
#define LINESIZE 1000

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

int check_request (char* message, char* method, char* url, \
                   char* version, char* host)
{
  char buff[REQUESTMAX][LINESIZE];
  char* temp;
  int i = 0;
  
  memset (buff[0], 0, LINESIZE);
  temp = strtok (message, "\n");
  do
  {
    strncpy (buff[i++], temp, LINESIZE);
    memset (buff[i], 0, LINESIZE);
    temp = strtok (message, "\n");
    if (strcmp (temp, "\r\n") == 0) break;
    if (i == REQUESTMAX) 
      return 1;
  } while (temp != NULL);

  temp = NULL;

  sscanf (buff[0], "%s %s %s %s", method, url, version, temp);
  if ((strcmp (method, "GET") != 0) && (strcmp (method, "HEAD") != 0) &&\
      (strcmp (method, "POST") != 0) && (strcmp (method, "PUT") != 0) &&\
      (strcmp (method, "DELETE") != 0))
    return 2;
  // but, we not consider to send head, post, put, and delete method
  if ((strcmp (version, "HTTP/1.0") != 0) && (strcmp (version, "HTTP/1.1")))
    return 3;
  // but, we not consider to send HTTP/1.1
  if (strlen (temp) == 0)
    return 4;

  sscanf (buff[1], "Host: %s", host);
  return 0;
}

int check_url (char* url, char* host, int* port, char* path)
{
  char protocol[LINESIZE];
  char host_port[LINESIZE];
  char host_check[LINESIZE];
  char port_num[LINESIZE];
  char* colon;
  
  memset (protocol, 0, LINESIZE);
  memset (host_port, 0, LINESIZE);
  memset (port_num, 0, LINESIZE);

  sscanf (url, "%s://%s/%s", protocol, host_port, path);
  //assume that url is protocol://absolute_URI(:port)/path

  *port = 80;
  colon = strstr (host_port, ":");
  if (colon != NULL)
  {
    sscanf (host_port, "%s:%s", host_check, port_num);
    for (int i = 0; i < strlen (port_num); i++)
      if (isdigit (port_num[i]) == 0) return 1;
    
    if ((atoi (port_num) > 65536) || (atoi (port_num) < 1))
      return 1;
  }

  else strcpy (host_check, host_port);

  if (strcmp (host_check, host) != 0) return 2;

  return 0;
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
  char buff[MAXDATASIZE];
  char method[LINESIZE], url[LINESIZE], version[LINESIZE], host[LINESIZE];
  char path[LINESIZE];
  int port;
  char error_message[LINESIZE];
  int error;

  memset (buff, 0, MAXDATASIZE);
  memset (method, 0, LINESIZE);
  memset (url, 0, LINESIZE);
  memset (version, 0, LINESIZE);
  memset (host, 0, LINESIZE);

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
      if ((bytes = recv (new_sockfd, buff, MAXDATASIZE, 0)) == -1)
      {
        perror ("server : recv\n");
        exit (1);
      }
      
      if (bytes == 0)
      {
        perror ("server : wrong message");
        break;
      }

      error = check_request (buff, method, url, version, host);
      if (error != 0)
      {
        memset (error_message, 0, LINESIZE);
        if (error == 1) 
          strcpy (error_message,\
              "Wrong request : Request message is Too Long\n");
        else if (error == 2) 
          strcpy (error_message,\
              "Wrong request : Request method is Wrong\n");
        else if (error == 3) 
          strcpy (error_message,\
              "Wrong request : Request version is Wrong\n");
        else strcpy (error_message,\
            "HTTP/1.0 400 Bad Request\nHost does not exist.\n");
        send (new_sockfd, error_message, strlen (error_message) + 1, 0);
        close (new_sockfd);
        return -1;
      }
      
      error = check_url (url, host, &port, path);
      if (error != 0)
      {
        memset (error_message, 0, LINESIZE);
        if (error == 1)
          strcpy (error_message,\
              "Wrong request : Port Number is wrong\n");
        else if (error == 2)
          strcpy (error_message,\
              "Wrong request : URL Host is different from Host header\n");
        send (new_sockfd, error_message, strlen (error_message) + 1, 0);
      }









        printf ("%s\n", buff);
    }
    else close (new_sockfd); // parent process come here

    while (waitpid (-1, NULL, WNOHANG) > 0); // reap the zombie processes
  }

}








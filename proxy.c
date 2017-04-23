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

/*
#include "zlib.h"  // if zlib.h is in computer (same folder), 
#define zlib       // remove comment command. (compression)
*/

#define BACKLOG 10   // how many pending connections queue will hold
#define MAXDATASIZE 100000 // maximum of file capacity
#define REQUESTMAX 50 // maximum line of request message 
#define LINESIZE 1000 // maximum of line capacity

// check_port_number : check command line correctness
// ./proxy PORTNUM
// 1024 <= PORTNUM <= 65535
void check_port_number (int argc, char **argv)
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

// check_request : check request message correctness
//                 and parse the message (by parameter)
// Message : method(GET) URL version(HTTP/1.0)
//           Host: host
//           Header ...
int check_request (char *message, char *method, char *url, \
                   char *version, char *host, char header[][LINESIZE])
{
  char buff[REQUESTMAX][LINESIZE];
  char *temp;
  int i = 0;
 
  memset (buff[0], 0, LINESIZE);
  temp = strtok (message, "\r\n");
  if (temp == NULL) return 4;
  do
  {
    strncpy (buff[i++], temp, LINESIZE);
    if (i > 2) strncpy (header[i-3], temp, LINESIZE);
    memset (buff[i], 0, LINESIZE);
    temp = strtok (NULL, "\r\n");
    if (temp == NULL || strcmp (temp, "\r\n") == 0) 
      break;
    if (i == REQUESTMAX) 
      return 1;
  } while (temp != NULL);

  temp = NULL;

  sscanf (buff[0], "%s %s %s %s", method, url, version, temp);
  if ((method == NULL) ||
      ((strcmp (method, "GET") != 0) && (strcmp (method, "HEAD") != 0) &&\
       (strcmp (method, "POST") != 0) && (strcmp (method, "PUT") != 0) &&\
       (strcmp (method, "DELETE") != 0)))
    return 2;
  // but, we not consider to send head, post, put, and delete method
  if ((method == NULL) ||
      ((strcmp (version, "HTTP/1.0") != 0) && (strcmp (version, "HTTP/1.1"))))
    return 3;
  // but, we not consider to send HTTP/1.1
  if (temp != NULL)
    return 4;

  sscanf (buff[1], "Host: %s", host);
  if (host == NULL)
    return 5;

  return 0;
}

// check_url : check URL correctness and parse the URL (by parameter)
// URL : protocol://host(:port)/path
int check_url (char *url, char *host, int *port, char *path)
{
  char protocol[LINESIZE];
  char host_port[LINESIZE];
  char host_check[LINESIZE];
  char port_num[LINESIZE];
  char copy_url[LINESIZE];
  struct servent *service;
  char *colon, *temp, *origin;
  
  memset (protocol, 0, LINESIZE);
  memset (host_port, 0, LINESIZE);
  memset (port_num, 0, LINESIZE);
  memset (copy_url, 0, LINESIZE);

  strcpy (copy_url, url);
  origin = copy_url;

  temp = strtok (copy_url, "/");
  if (temp == NULL) return 1;
  strcpy (protocol, temp);

  protocol[strlen(protocol) - 1] = '\0';

  temp = strtok (NULL, "/");
  if (temp == NULL) return 2;
  strcpy (host_port, temp);

  temp = strtok (NULL, "\0");
  if (temp == NULL) path = NULL;
  else strcpy (path, temp);
  //assume that url is protocol://absolute_URI(:port)/path

  *port = 80;
  colon = strstr (host_port, ":");
  if (colon != NULL)
  {
    temp = strtok (host_port, ":");
    strcpy (host_check, temp);

    temp = strtok (NULL, "\0");
    strcpy (port_num, temp);
    for (int i = 0; i < strlen (port_num); i++)
      if (isdigit (port_num[i]) == 0) return 3;
    
    if ((atoi (port_num) > 65536) || (atoi (port_num) < 1))
      return 3;

    *port = atoi (port_num);
  }

  else if (strcmp (protocol, "http") != 0)
  {
    service = getservbyname (protocol, "tcp");
    if (service == NULL) return 1;

    *port = ntohs(service->s_port);
  } 

  else strcpy (host_check, host_port);

  if (strcmp (host_check, host) != 0) return 4;

  return 0;
}

// main : make proxy server which deliver client's request to server
int main (int argc, char **argv)
{
  int sockfd, new_sockfd;
  int bytes, location;
  socklen_t sock_in_size;
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr;
  int yes = 1, success;
  char s[INET6_ADDRSTRLEN];

  // open the port and ready to connect
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
    if (!fork ()) // child process come here
    {
      int server_sockfd;
      char buff[MAXDATASIZE];
      char temp[LINESIZE];
      char method[LINESIZE], url[LINESIZE], version[LINESIZE], host[LINESIZE];
      char path[LINESIZE], port_str[LINESIZE], header[REQUESTMAX][LINESIZE];
      int port;
      struct hostent *host_entry;
      char request_message[LINESIZE];
      char error_message[LINESIZE];
      int error;
#ifdef zlib
      int compress_flag = 0; 
      uLong compress_size = MAXDATASIZE;
      char compress_buff[MAXDATASIZE];
      char compress_result[MAXDATASIZE];
      char compress_copy[MAXDATASIZE];
      char compress_message[MAXDATASIZE];
      char *header_p;
#endif
      memset (buff, 0, MAXDATASIZE);
      memset (temp, 0, LINESIZE);
      memset (method, 0, LINESIZE);
      memset (url, 0, LINESIZE);
      memset (version, 0, LINESIZE);
      memset (host, 0, LINESIZE);
      memset (request_message, 0, LINESIZE);
      for (int i = 0; i < REQUESTMAX; i++)
        memset (header[i], 0, LINESIZE);

      // take the message
      while (1)
      {
        if ((bytes = recv (new_sockfd, temp, LINESIZE - 1, 0)) == -1)
        {
          perror ("server : recv\n");
          exit (1);
        }
     
        if (bytes == 0) return -1;
        
        if (bytes == 2) break; // \r\n\

        strcat (buff, temp);
        if (strstr (buff, "\r\n\r\n") != NULL) break;
        memset (temp, 0, LINESIZE);
      }

      if (strlen (buff) == 2)
      {
        memset (error_message, 0, LINESIZE);
        strcpy (error_message,\
            "Wrong request : Request message empty\n");
        send (new_sockfd, error_message, strlen (error_message) + 1, 0);
        close (new_sockfd);
        return -1;
      }

      // parsing message
      error = check_request (buff, method, url, version, host, header);
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
        else if (error == 4)
          strcpy (error_message,\
              "Wrong request : Request message is Wrong\n");
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
              "Wrong request : Service name is wrong\n");
        else if (error == 2)
          strcpy (error_message,\
              "Wrong request : URL is wrong\n");
        else if (error == 3)
          strcpy (error_message,\
              "Wrong request : Port Number is wrong\n");
        else
          strcpy (error_message,\
              "Wrong request : URL Host is different from Host header\n");
        send (new_sockfd, error_message, strlen (error_message) + 1, 0);
        close (new_sockfd);
        return -1;
      }

      // preparing to connect server
      host_entry = gethostbyname (host);
      if (host_entry == NULL)
      {
        memset (error_message, 0, LINESIZE);
        strcpy (error_message,\
            "Wrong request : Host name is wrong\n");
        send (new_sockfd, error_message, strlen (error_message) + 1, 0);
        close (new_sockfd);
        return -1;
      }

      if ((server_sockfd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
      {
        perror ("server : socket\n");
        close (new_sockfd);
        return -1;
      }

      memset (&hints, 0, sizeof (hints));
      servinfo = NULL;
      hints.ai_family = AF_INET;
      hints.ai_socktype = SOCK_STREAM;
      for (int i = 0; host_entry->h_addr_list[i] != NULL; i++)
      {
        sprintf (port_str, "%d", port);
        success = getaddrinfo (inet_ntoa(*(struct in_addr*)\
                                 host_entry->h_addr_list[i]),\
                               port_str, &hints, &servinfo);
        if (success != 0)
        {
          memset (error_message, 0, LINESIZE);
          sprintf (error_message,\
              "Wrong request : getaddrinfo is Wrong : %s\n",\
              gai_strerror (success));
          send (new_sockfd, error_message, strlen (error_message) + 1, 0);
          close (new_sockfd);
          close (server_sockfd);
          return -1;
        }

        for (p = servinfo; p != NULL; p = p->ai_next)
        {
          if (connect (server_sockfd, p->ai_addr, p->ai_addrlen) == -1)
          {
            close (server_sockfd);
            continue;
          }

          break;
        }

        if (p != NULL) break;
      }

      if (p == NULL)
      {
        memset (error_message, 0, LINESIZE);
        strcpy (error_message,\
            "Fail to connect\n");
        send (new_sockfd, error_message, strlen (error_message) + 1, 0);
        close (new_sockfd);
        return 2;
      }

      //make the message to send to server
      sprintf (request_message,\
              "GET http://%s/%s HTTP/1.0\r\nHost: %s\r\n",\
               host, path, host);
      for (int i = 0; strlen (header[i]) != 0; i++)
      {
        memset (temp, 0, LINESIZE);
#ifdef zlib
        if (strcmp (header[i],\
              "Accept-Encoding: gzip, deflate") == 0)
          compress_flag = 1;
#endif
        sprintf (temp, "%s\r\n", header[i]);
        strcat (request_message, temp);
      }
      strcat (request_message, "\r\n");

      if (send (server_sockfd, request_message,\
                strlen (request_message) + 1, 0) == -1)
      {
        memset (error_message, 0, LINESIZE);
        strcpy (error_message,\
            "Fail to send to server\n");
        send (new_sockfd, error_message, strlen (error_message) + 1, 0);
        close (new_sockfd);
        close (server_sockfd);
        return 2;
      }

#ifdef zlib // If zlib.h can use, proxy server tries compression.
      if (compress_flag == 1)
      {
        memset (compress_buff, 0, MAXDATASIZE);
        memset (compress_result, 0, MAXDATASIZE);
        memset (compress_copy, 0, MAXDATASIZE);
        memset (compress_message, 0, MAXDATASIZE);
        while (1)
        {
          if ((bytes = recv (server_sockfd, buff, MAXDATASIZE-1, 0)) == -1)
          {
            memset (error_message, 0, LINESIZE);
            strcpy (error_message,\
                "Fail to recevie message from server\n");
            send (new_sockfd, error_message, strlen (error_message) + 1, 0);
            close (new_sockfd);
            close (server_sockfd);
            return 2;
          }

          if (bytes == 0) break;

          if (strlen (compress_buff) == 0) strcpy (compress_buff, buff);
          else strcat (compress_buff, buff);
        }

        strcpy (compress_copy, compress_buff);
        header_p = strstr (compress_copy, "\r\n\r\n");
        *header_p = '\0';
        
        if (strstr (compress_copy, "Content_Encoding:") != NULL)
        {
          header_p = header_p + 4;
          strcpy (compress_result, compress_copy);
          strcpy (compress_message, header_p);
          memset (compress_copy, 0, MAXDATASIZE);
          if (compress (compress_copy, &compress_size,\
                        compress_message, strlen (compress_message)) == Z_OK)
          {
            strcat (compress_result, \
                "\r\nContent_Encoding: gzip,deflate\r\n\r\n");
            strcat (compress_result, compress_copy);

            if (send (new_sockfd, compress_result, MAXDATASIZE-1, 0) == -1)
            {
              perror ("server : Fail to send to cliend\n");
              close (new_sockfd);
              return -1;
            }

            close (new_sockfd);
            return 0;
          }
        }

        if (send (new_sockfd, buff, MAXDATASIZE-1, 0) == -1)
        {
          perror ("server : Fail to send to client\n");
          close (new_sockfd);
          return -1;
        }

        close (new_sockfd);
        return 0;
      }
#endif

      while (1) // If not, normally deliver the message.
      {
        memset (buff, 0, MAXDATASIZE);
        if ((bytes = recv (server_sockfd, buff, MAXDATASIZE-1, 0)) == -1)
        {
          memset (error_message, 0, LINESIZE);
          strcpy (error_message,\
              "Fail to receive message from server\n");
          send (new_sockfd, error_message, strlen (error_message) + 1, 0);
          close (new_sockfd);
          close (server_sockfd);
          return 2;
        }

        if (bytes == 0) break;

        if (send (new_sockfd, buff, MAXDATASIZE-1, 0) == -1)
        {
          perror ("server : Fail to send to client\n");
          close (new_sockfd);
          close (server_sockfd);
          return -1;
        }
      }
      close (new_sockfd);
      return 0;
    }
    else close (new_sockfd); // parent process come here

    while (waitpid (-1, NULL, WNOHANG) > 0); // reap the zombie processes
  }

}


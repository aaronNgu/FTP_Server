#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include "dir.h"
#include "usage.h"

#include <ifaddrs.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include "util.h"
#include "command.h"
// Here is an example of how to use the above function. It also shows
// one how to get the arguments passed on the command line.

// returns false on any failed command

char rootDirectory[256];
int passiveSocket;

int getSize (FILE *fp) {
    // code from https://stackoverflow.com/questions/238603/how-can-i-get-a-files-size-in-c
    int start =ftell(fp);
    fseek(fp, 0L, SEEK_END);
    int size =ftell(fp);
    fseek(fp, start,SEEK_SET); //reset pointer
    return size;
}

int getDataSocket(int clientSocket, int pasvSock)
{
  fd_set rfds;

  int val;
  struct timeval tv;
  tv.tv_sec = 10;
  int dataSocket;

  FD_ZERO(&rfds);
  FD_SET(pasvSock, &rfds);
  val = select(pasvSock + 1, &rfds, &rfds, NULL, &tv);

  if (val)
  {
    sendResponse(clientSocket, "150", "Opening Data Connection");
    dataSocket = accept(pasvSock, (struct sockaddr *)NULL, NULL);
    return dataSocket;
  }

  sendResponse(clientSocket, "425", "Can't open data connection");
  return -1;
}

int retr(int clientSocket, char *argument)
{
  if (!isLogged())
  {
    return sendNotLoggedInResponse(clientSocket);
  }
  if (passiveSocket == -1)
  {
    return sendResponse(clientSocket, "425", "Use Pasv before reunning nlst");
  }
  if (argument == NULL)
  {
    close(passiveSocket);
    return sendResponse(clientSocket, "501", "Syntax Error. No Argument provided");
  }

  // check if has read permission
  if (access(argument, R_OK) != 0)
  {
    return sendResponse(clientSocket, "550", "Requested File action not taken. Access Denied");
  }

  int dataSocket = getDataSocket(clientSocket, passiveSocket);

  FILE *fs = fopen(argument, "r");
  int fileSize = getSize(fs); 
  int blockSize;
  char buffer[256 * 2];

  bzero(buffer, 256 * 2);
  while ((blockSize = fread(buffer, sizeof(char), 256 * 2, fs)) > 0)
  {
    if (write(dataSocket, buffer, blockSize) < 0)
    {
      printf("Error Sending Data\n");
    }
    bzero(buffer, 256 * 2);
  }
  fclose(fs);

  close(passiveSocket);
  close(dataSocket);
  dataSocket = -1;
  passiveSocket = -1;

  return sendResponse(clientSocket, "226", "Directory send ok ");
}

int nlst(int clientSocket, char *argument)
{
  if (!isLogged())
  {
    return sendNotLoggedInResponse(clientSocket);
  }
  if (passiveSocket == -1)
  {
    return sendResponse(clientSocket, "425", "Use Pasv before reunning nlst");
  }
  if (argument != NULL)
  {
    close(passiveSocket);
    return sendResponse(clientSocket, "501", "Syntax Error. No Argument accepted");
  }

  int dataSocket = getDataSocket(clientSocket, passiveSocket);

  // get working directory
  char buf[256];
  getcwd(buf, sizeof(buf));

  int result = listFiles(dataSocket, buf);

  close(passiveSocket);
  close(dataSocket);
  dataSocket = -1;
  passiveSocket = -1;
  if (result == -1)
  {
    return sendResponse(clientSocket, "450", "Requested file action not taken. File unavailable");
  }
  if (result == -2)
  {
    return sendResponse(clientSocket, "451", "Requested action aborted");
  }

  return sendResponse(clientSocket, "226", "Directory send ok ");
}

// returns the first non loopback ipv4 address seen from getifaddrs - helper for pasv()
char *getInet()
{
  // this part of the code is from :
  // https://stackoverflow.com/questions/4139405/how-can-i-get-to-know-the-ip-address-for-interfaces-in-c
  struct ifaddrs *ifap, *ifa;
  struct sockaddr_in *sa;
  char *addr;

  getifaddrs(&ifap);
  for (ifa = ifap; ifa; ifa = ifa->ifa_next)
  {
    if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET)
    {
      {
        sa = (struct sockaddr_in *)ifa->ifa_addr;
        addr = inet_ntoa(sa->sin_addr);
        if (strcmp(addr, "127.0.0.1") != 0)
        {
          return addr;
        }
      }
    }
  }
  return NULL;
}

int pasv(int clientSocket, char *argument)
{
  struct sockaddr_in passiveAddress;
  struct sockaddr_in pasv;
  socklen_t pasvlength;

  int passivePort;
  char *ipAdd;

  char message[256];

  if (!isLogged())
  {
    return sendNotLoggedInResponse(clientSocket);
  }
  if (argument != NULL)
  {
    return sendResponse(clientSocket, "501", "Argument provided. PASV command does not require an argument");
  };

  passiveSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (passiveSocket < 0)
  {
    perror("Fail to create passive socket");
    return 0;
  }

  // address with ephemeral port number
  bzero((char *)&passiveAddress, sizeof(passiveAddress));
  passiveAddress.sin_family = AF_INET;
  passiveAddress.sin_port = 0;
  passiveAddress.sin_addr.s_addr = INADDR_ANY;

  // bind passive socket to passive address
  if (bind(passiveSocket, (struct sockaddr *)&passiveAddress, sizeof(passiveAddress)) < 0)
  {
    perror("Failed to bind port");
    return 0;
  }

  listen(passiveSocket, 1);

  // get port number
  pasvlength = sizeof(struct sockaddr);
  getsockname(passiveSocket, (struct sockaddr *)&pasv, &pasvlength);
  passivePort = (int)ntohs(pasv.sin_port);

  // get ip
  ipAdd = getInet();
  char buf[256];
  char *first = strtok(ipAdd, ".\r\n");
  char *second = strtok(NULL, ".\r\n");
  char *third = strtok(NULL, ".\r\n");
  char *fourth = strtok(NULL, ".\r\n");

  sprintf(buf, "Entering Passive Mode (%s,%s,%s,%s,%d,%d) ", first, second, third, fourth,
          passivePort / 256, passivePort % 256);

  // printf("Message to send for pasv is %s\n", buf);
  return sendResponse(clientSocket, "227", buf);
}

int handleCommand(int clientSocket, char *responseBuffer)
{
  char *command;
  char *argument;
  const char s[5] = " \r\n";
  char *token;
  ssize_t length;

  /* get the first token */
  command = strtok(responseBuffer, s);
  argument = strtok(NULL, s);

  printf("Client Command is %s\n", command);

  if (strcasecmp(command, "USER") == 0)
  {
    return user(clientSocket, argument);
  }

  if (strcasecmp(command, "QUIT") == 0)
  {
    return sendResponse(clientSocket, "221", "Service Closing Control Connection") ? 2 : 0;
  }

  if (strcasecmp(command, "CWD") == 0)
  {
    return cwd(clientSocket, argument);
  }

  if (strcasecmp(command, "CDUP") == 0)
  {
    return cpud(clientSocket, argument, rootDirectory);
  }

  if (strcasecmp(command, "TYPE") == 0)
  {
    return type(clientSocket, argument);
  }

  if (strcasecmp(command, "MODE") == 0)
  {
    return mode(clientSocket, argument);
  }

  if (strcasecmp(command, "STRU") == 0)
  {
    return stru(clientSocket, argument);
  }

  if (strcasecmp(command, "RETR") == 0)
  {
    return retr(clientSocket, argument);
  }

  if (strcasecmp(command, "PASV") == 0)
  {
    return pasv(clientSocket, argument);
  }

  if (strcasecmp(command, "NLST") == 0)
  {
    return nlst(clientSocket, argument);
  }

  return sendResponse(clientSocket, "500", "Invalid or Unsupported Command");
}

void *interact(void *args)
{
  int clientSocket = *(int *)args;

  // Interact with the client
  char responseBuffer[1024];

  while (true)
  {
    bzero(responseBuffer, 1024);

    // receive clint message
    ssize_t length = recv(clientSocket, responseBuffer, 1024, 0);
    if (length < 0)
    {
      perror("Failed to read from socket \n");
      break;
    }

    if (length == 0)
    {
      printf("EOF\n");
      break;
    };

    int result = handleCommand(clientSocket, responseBuffer);
    if (result == 0)
    {
      perror("Failed to handle command \n");
      break;
    };

    if (result == 2)
    {
      printf("Client exiting \n");
      break;
    }
  }

  chdir(rootDirectory);
  setLogin(0);
  close(passiveSocket);
  passiveSocket = -1;
  close(clientSocket);
  clientSocket = -1;
  return NULL;
}

int main(int argc, char *argv[])
{
  int i;

  // Check the command line arguments
  if (argc != 2)
  {
    usage(argv[0]);
    return -1;
  }

  // Create socket : PF_INET - No idea what's this, SOCK_STREAM - TCP , 0 - Internet Protocol
  int socketfd = socket(PF_INET, SOCK_STREAM, 0);
  if (socketfd < 0)
  {
    perror("Failed to create the socket.");
    exit(-1);
  }
  // SOL_SOCKET : set options for socket level, SO_REUSEADDR : option to set, &value : set it to true
  // Not sure why sizeof(int)
  int value = 1;
  if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(int)) != 0)
  {
    perror("Failed to set socket options.");
    exit(-1);
  }

  // Bind Socket to port
  struct sockaddr_in address;
  bzero(&address, sizeof(struct sockaddr_in));
  address.sin_family = AF_INET;
  int port = atoi(argv[1]);
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  if (bind(socketfd, (const struct sockaddr *)&address, sizeof(struct sockaddr_in)) != 0)
  {
    perror("Failed to bind the socket\n");
    exit(-1);
  }

  if (listen(socketfd, 4) != 0)
  {
    perror("Failed to listen for connections \n");
    exit(-1);
  }

  getcwd(rootDirectory, sizeof(rootDirectory));
  while (true)
  {

    // Accept incoming connection
    struct sockaddr_in clientAddress;
    socklen_t clientAddressLength = sizeof(struct sockaddr_in);
    printf("Waiting for incoming connection \n");
    int clientSocket = accept(socketfd, (struct sockaddr *)&clientAddress, &clientAddressLength);

    if (clientSocket < 0)
    {
      perror("Failed to accept the client connection \n");
      continue;
    }

    // respond upon successful connection
    if (sendResponse(clientSocket, "220", "Awaiting Input") == 0)
    {
      continue;
    };

    printf("Accepted the client connection from %s:%d.\n", inet_ntoa(clientAddress.sin_addr),
           ntohs(clientAddress.sin_port));

    // Create pthread
    pthread_t thread;
    if (pthread_create(&thread, NULL, &interact, &clientSocket) != 0)
    {
      perror("Failed to create the thread \n");
      continue;
    }

    pthread_join(thread, NULL);

    printf("Interaction thread has finished \n");
  }
  return 0;
}

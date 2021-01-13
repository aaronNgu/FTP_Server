#include <sys/types.h>
#include <strings.h>
#include <sys/socket.h>
#include <stdio.h>

int sendResponse(int clientSocket, char* code, char* message) {
    char responseBuffer[1024];
    bzero(responseBuffer, 1024);
    ssize_t length = snprintf(responseBuffer, 1024, "%s %s\r\n", code,message);
    if (send(clientSocket, responseBuffer, length, 0) < 0)
    {
      perror("Failed to send response \n");
      return 0;
    }
    return 1;
};

int sendNotLoggedInResponse(int clientSocket) {
    char responseBuffer[1024];
    bzero(responseBuffer, 1024);
    ssize_t length = snprintf(responseBuffer, 1024, "%s %s\r\n", "530", "Not Logged In. Please Login before proceeding");
    if (send(clientSocket, responseBuffer, length, 0) < 0)
    {
      perror("Failed to send response \n");
      return 0;
    }
    return 1; 
}
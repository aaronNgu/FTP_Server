#include "util.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int login = 0;
char Type[5] = "ASCII";

void setLogin(int lg)
{
    login = lg;
}

int isLogged()
{
    return login;
}

char *getType()
{
    return Type;
}

int user(int clientSocket, char *argument)
{
    if (!login)
    {
        if (argument == NULL)
        {
            return sendResponse(clientSocket, "501", "No User Provided. Please Provide User");
        }
        else if (strcmp(argument, "cs317") != 0)
        {
            return sendResponse(clientSocket, "530", "Server is only for cs317");
        }
        login = 1;
        return sendResponse(clientSocket, "230", "User login successful");
    }
    else
    {
        return sendResponse(clientSocket, "503", "Already Logged in as cs317");
    }
}

int cwd(int clientSocket, char *argument)
{
    // user not logged in
    if (!login)
    {
        return sendNotLoggedInResponse(clientSocket);
    };
    // insufficient argument
    if (argument == NULL)
    {
        return sendResponse(clientSocket, "501", "No file path provided");
    };

    char dir[256];
    char *token;
    strcpy(dir, argument);

    // get first token
    token = strtok(argument, "/\r\n");

    // if path has ./ or ../
    while (token != NULL)
    {
        if (strcmp(token, "..") == 0 || strcmp(token, ".") == 0)
        {
            return sendResponse(clientSocket, "550", "Requested action not taken. Directory starting with ./ or ../ are not allowed");
        }
        token = strtok(NULL, "/\n");
    }

    int result = chdir(dir);
    return result == 0 ? sendResponse(clientSocket, "250", "Requested file action okay, completed.") : sendResponse(clientSocket, "550", "Requested action not taken. either file not found or no access");
}

int cpud(int clientSocket, char *argument, char *rootDirectory)
{
    if (!login)
    {
        return sendNotLoggedInResponse(clientSocket);
    };
    if (argument != NULL)
    {
        return sendResponse(clientSocket, "501", "CPUD does not need arugment but argument provided");
    };

    // if working directory == root directory
    char buf[256];
    getcwd(buf, sizeof(buf));
    if (strcmp(buf, rootDirectory) == 0)
    {
        return sendResponse(clientSocket, "550", "Requested action not taken. Not allowed to go beyond current Directory");
    };

    int result = chdir("..");
    return result == 0 ? sendResponse(clientSocket, "200", "Command Okay. Operation successful") : sendResponse(clientSocket, "550", "Requested action not taken. Unable to go to parent directory");
}

int type(int clientSocket, char *argument)
{
    // TODO: not sure if we need to store type
    if (!login)
    {
        return sendNotLoggedInResponse(clientSocket);
    };
    if (argument == NULL)
    {
        return sendResponse(clientSocket, "501", "No argument provided. Please provide an argument");
    };
    if (strcasecmp(argument, "I") == 0)
    {
        return sendResponse(clientSocket, "200", "Command Okay. Switching to Binary Mode");
    }
    if (strcasecmp(argument, "A") == 0)
    {
        return sendResponse(clientSocket, "200", "Command Okay. Switching to ASCII Mode");
    }
    return sendResponse(clientSocket, "504", "Command not implemented for the type");
}

int mode(int clientSocket, char *argument)
{
    // TODO: not sure if we need to store mode as well
    if (!login)
    {
        return sendNotLoggedInResponse(clientSocket);
    }
    if (argument == NULL)
    {
        return sendResponse(clientSocket, "501", "No argument provided. Please provide an argument");
    };
    if (strcasecmp(argument, "S") == 0)
    {
        return sendResponse(clientSocket, "200", "Command Okay, Mode set to Stream ");
    }
    else if (strcasecmp(argument, "B") == 0 || strcasecmp(argument, "C") == 0)
    {
        return sendResponse(clientSocket, "504", "Command not implemented for the mode");
    };
    return sendResponse(clientSocket, "501", "Invalid Argument");
}

int stru(int clientSocket, char *argument)
{
    // TODO: not sure if we need to store structure type as well
    if (!login)
    {
        return sendNotLoggedInResponse(clientSocket);
    }
    if (argument == NULL)
    {
        return sendResponse(clientSocket, "501", "No argument provided. Please provide an argument");
    };
    if (strcasecmp(argument, "F") == 0)
    {
        return sendResponse(clientSocket, "200", "Command Okay, File structure selected");
    }
    else if (strcasecmp(argument, "R") == 0 || strcasecmp(argument, "P") == 0)
    {
        return sendResponse(clientSocket, "504", "Command not implemented for the struct type");
    }
    return sendResponse(clientSocket, "501", "Invalid Argument");
}


int retr(int clientSocket, char *argument)
{
    if (!login)
    {
        return sendNotLoggedInResponse(clientSocket);
    }
    return 0;
}
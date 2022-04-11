#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define ADDRESS "0.0.0.0"
#define PORT 44444
#define BACKLOG 15
#define LOWERBOUND (-3037000499)
#define UPPERBOUND 3037000499
#define MAXLEN 1024
#define EM 0x19

void check(int returnValue, char *errorMessage)
{
    if (returnValue < 0)
    {
        perror(errorMessage);
        exit(1);
    }
}

int setup_server()
{
    int serverSocket;
    check((serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)), "Error in socket()");

    // struct sockaddr
    struct sockaddr_in servAddr;            // Server address
    memset(&servAddr, 0, sizeof(servAddr)); // Zero out structure
    servAddr.sin_family = AF_INET;          // IPv4 address family
    servAddr.sin_port = ntohs(PORT);
    servAddr.sin_addr.s_addr = INADDR_ANY;

    // bind()
    check(bind(serverSocket, (struct sockaddr *)&servAddr, sizeof(servAddr)), "Error in bind()");
    // listen()
    check(listen(serverSocket, BACKLOG), "Error in listen()");
    return serverSocket;
}

int accept_new_connection(int serverSocket)
{
    int clientSocket;
    struct sockaddr_in newAddr;
    int addrlen = sizeof(newAddr);
    check((clientSocket = accept(serverSocket, (struct sockaddr *)&newAddr, (socklen_t *)&addrlen)), "Error in accept()");
    return clientSocket;
}

void send_square(long value, int clientSocket)
{
    printf("Sending");
    char returnBuffer[MAXLEN];
    memset(returnBuffer, 0, sizeof(returnBuffer));
    if (value > UPPERBOUND || value < LOWERBOUND) // Out of bounds
    {
        char *stringBuf = "ERROR: out of range.\n";
        check(send(clientSocket, stringBuf, strlen(stringBuf), 0), "Error in send()");
    }
    else // in bound
    {
        // convert to string (delimited with \n)
        check((snprintf(returnBuffer, sizeof(returnBuffer), "%ld\n", value * value)), "Error in snprintf()");
        // send to client
        check((send(clientSocket, returnBuffer, strlen(returnBuffer), 0)), "Error in send()");
        printf("----------\n");
        check((write(1, returnBuffer, strlen(returnBuffer))), "Error in write()");
        printf("----------\n");
    }
}

int isValidInput(char *buffer, int size)
{
    for (int i = 0; i < size; i++)
    {
        printf("%c ", buffer[i]);
        if (buffer[i] == '-' && i != 0)
        {
            printf("invalid!\n");
            return 0;
        }
    }
    printf("\n");
    return 1;
}

void sendInvalidString(int clientSocket)
{
    // send delimited message to client and close connection
    char *stringBuf = "ERROR: invalid input.\n";
    check((send(clientSocket, stringBuf, strlen(stringBuf), 0)), "Error in send()");
}

void *handle_connection(int clientSocket)
{
    // read from the client one byte at a time, breaking when there is a EM
    char byteBuf[sizeof(char)] = {0};
    int readVal;
    int i = 0;
    int isValid = 1;
    char valueBuffer[MAXLEN]; // max value is 19 characters
    long parsedValue;
    int allowWrite = 1;
    memset(valueBuffer, 0, sizeof(valueBuffer));
    // FIXME: try numbers like 9-1241241 in the middle of cli.c
    // FIXME: Keeping track of length
    // FIXME: simple clients
    while ((readVal = recv(clientSocket, byteBuf, sizeof(byteBuf), MSG_DONTWAIT)) && isValid)
    {
        if (readVal == sizeof(byteBuf)) // one byte read
        {
            // checking the Values of the Byte read
            if (*byteBuf == EM)
            {
                if (isValidInput(valueBuffer, i))
                {
                    // send to client
                    if (allowWrite)
                    {
                        parsedValue = atol(valueBuffer);
                        send_square(parsedValue, clientSocket);
                    }
                }
                else
                {
                    allowWrite = 0;
                    sendInvalidString(clientSocket);
                }
                memset(valueBuffer, 0, sizeof(valueBuffer));
                i = 0;
            }
            else if ((*byteBuf < '0' && *byteBuf != '-') || *byteBuf > '9') // invalid input
            {
                sendInvalidString(clientSocket);
                allowWrite = 0;
                isValid = 0;
            }

            else // add read input to the valueBuffer and increment position
            {
                valueBuffer[i] += *byteBuf;
                i++;
            }
        }
        else if (readVal < 0)
        {
            isValid = 0;
            if (!(isValidInput(valueBuffer, i)))
            {
                sendInvalidString(clientSocket);
            }
            else
            {
                if (allowWrite)
                {
                    parsedValue = atol(valueBuffer);
                    send_square(parsedValue, clientSocket);
                }
            }
        }
    }
    // last value in buffer before EOF
    if (isValid)
    {
        if (isValidInput(valueBuffer, i))
        {
            parsedValue = atol(valueBuffer);
            send_square(parsedValue, clientSocket);
        }
    }

    // close
    close(clientSocket);
    return NULL;
}

int main()
{
    int serverSocket = setup_server();

    fd_set currentSockets, readySockets;
    FD_ZERO(&currentSockets);
    FD_SET(serverSocket, &currentSockets);

    while (1)
    {
        readySockets = currentSockets;

        check(select(FD_SETSIZE, &readySockets, NULL, NULL, NULL), "Error in select()");
        for (int i = 0; i < FD_SETSIZE; i++)
        {
            if (FD_ISSET(i, &readySockets))
            {
                if (i == serverSocket)
                {
                    int clientSocket = accept_new_connection(serverSocket);
                    FD_SET(clientSocket, &currentSockets);
                    printf("New Connection %d !\n", clientSocket);
                }
                else
                {
                    handle_connection(i);
                    FD_CLR(i, &currentSockets);
                    printf("Connection %d handled!\n\n", i);
                }
            }
        }
    }
    return 0;
}

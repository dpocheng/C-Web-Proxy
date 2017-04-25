// Ian Stephenson - 44419093
// Pok On Cheng - 74157306
// Cassie Liu - 52504836
// Sung Mo Koo - 51338217


#include <netinet/in.h>

#include "csapp.h"
#define LOGFILE "proxy.log"

/* Function prototypes */
int parse_uri(char *uri, char *hostname, char *pathname, int *port);
void format_log_entry(char *ipAddress, char *url, unsigned int size);

/* Global variables */
FILE *logFileFP;

/* Main program */
int main(int argc, char **argv) {
	/* Check arguments */
	if (argc != 2) {
		fprintf(stderr, "Invalid number of arguments\n");
		exit(0);
	}

	/* Set up listen port for requests from client */
	int port = atoi(argv[1]);
	int listenFD = Open_listenfd(port);
	while (1) {

		printf("Accepting incoming connections...\n");

		/* Accept incoming connection from client */
		SA *clientIPAddress;
		socklen_t  *lengthOfSocketStruct;
		int connectionFD = Accept(listenFD, clientIPAddress, lengthOfSocketStruct);

		printf("Request received...\n");

		/* Reading & parsing data from incoming request */
		printf("Reading request...\n");
		char buf[MAXLINE], hostname[MAXLINE], pathname[MAXLINE];
		int incomingPort;
		// Read the request into buf, up to MAXLINE chars, 0 means no flags. Returns the size read.
		ssize_t requestDataSizeReceived = recv(connectionFD, buf, MAXLINE, 0);

		/* Copy buf into restOfRequest before buf is modified by strtok and parse_uri */
		char *restOfRequest = malloc(sizeof(buf));
		strcpy(restOfRequest, buf);

		/* Tokenize the GET request line */
		printf("Parsing request...\n");
		char *httpMethod = strtok(buf, " ");
		char *url = strtok(NULL, " ");
        // goto the label CLoseConnection if the url is NULL
        // and it fix the segmentation fault
        if (url == NULL) {
            goto CloseConnection;
        }
		char *httpVersion = strtok(NULL, " ");
		parse_uri(url, hostname, pathname, &incomingPort);

		/* Retrieve the IP address of the client's hostname */
		printf("Forwarding request to proxy...\n");
		struct hostent *h = gethostbyname(hostname);	// Get the IP of the host
		char *clientHostIPAddress = inet_ntoa(*(struct in_addr*)(h->h_addr_list[0]));		// Decode to string
		/* Capture the rest of the request to be forwarded:
		 * 		Be sure to eat the \r\n at the end of the line. This stops after the \r,
		 * 		but before the \n so add one to move the pointer past the \n */
		restOfRequest = strpbrk(restOfRequest, "\n") + 1;

		/* Open connection with address of end server specified by incoming request */
		printf("Opening connection to server...\n");
		int serverFD = open_clientfd(hostname, incomingPort);	// Open file descriptor (connection) with server

		/* Send the request across the connection with the end server */
		printf("Sending request to server...\n");
		rio_writen(serverFD, "GET /", strlen("GET /"));						// Write request to server
		rio_writen(serverFD, pathname, strlen(pathname));					// ""
		rio_writen(serverFD, " HTTP/1.0\r\n", strlen(" HTTP/1.1\r\n"));		// ""
		rio_writen(serverFD, restOfRequest, strlen(restOfRequest));			// When this line finishes it sends the request off. It does that because
																			// this string ends with "\r\n\r\n" which means end of request

		/* Receive the reply from end server */
		printf("Waiting for response from server...\n");
		char serverBuf[MAXLINE];

		// Try to read entire response from server and stop reading when needed. This also
		// forwards the reply to the client (browser)
		//
		// A lot of the errors come from here
		rio_t rio;
		int n, response_len;
		rio_readinitb(&rio, serverFD);
		response_len = 0;
		while((n = recv(serverFD, serverBuf, MAXLINE, MSG_WAITALL)) > 0 ) {
			printf("\n---%d\n", n);
			response_len += n;
			rio_writen(connectionFD, serverBuf, n);
			bzero(serverBuf, MAXLINE);
		}
		printf("Response sent to client...\n");

		// Old stuff I was trying to try and fix the errors
//		ssize_t serverResponseLengthReceived = recv(serverFD, serverBuf, 100*MAXLINE, 0);
//		printf("Reponse fully received from server...\n");
//
//		/* Forward the reply to the client if the request is not blocked by server */
//		printf("Forwarding response from proxy to client...\n");
//		rio_writen(connectionFD, serverBuf, serverResponseLengthReceived);

		/* Log request to file */
		printf("Logging event...\n");
		logFileFP = fopen(LOGFILE, "a");
		format_log_entry(clientHostIPAddress, url, requestDataSizeReceived);
		fclose(logFileFP);

    CloseConnection:
		/* Close connection socket */
		printf("Closing connection to server...\n");
		close(serverFD);
		printf("Closing connection to client...\n");
		close(connectionFD);
	}
	exit(0);
}

/*
 * parse_uri - URI parser
 *
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{
	char *hostbegin;
	char *hostend;
	char *pathbegin;
	int len;

	if (strncasecmp(uri, "http://", 7) != 0) {
		hostname[0] = '\0';
		return -1;
	}

	/* Extract the host name */
	hostbegin = uri + 7;
	hostend = strpbrk(hostbegin, " :/\r\n\0");
	len = hostend - hostbegin;
	strncpy(hostname, hostbegin, len);
	hostname[len] = '\0';

	/* Extract the port number */
	*port = 80; /* default */
	if (*hostend == ':')
		*port = atoi(hostend + 1);

	/* Extract the path */
	pathbegin = strchr(hostbegin, '/');
	if (pathbegin == NULL) {
		pathname[0] = '\0';
	}
	else {
		pathbegin++;
		strcpy(pathname, pathbegin);
	}

	return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring.
 */
void format_log_entry(char *ipAddress, char *url, unsigned int size)
{
	time_t now;
	char time_str[MAXLINE];
	unsigned long host;
	unsigned char a, b, c, d;

	/* Get a formatted time string */
	now = time(NULL);
	strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));


	/* Return the formatted log entry string */
	fprintf(logFileFP, "%s\t%s\t%-110s\t%zdB\n", time_str, ipAddress, url, size);
}

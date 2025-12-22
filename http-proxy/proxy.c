#include <stdio.h>
#include "csapp.h"
#include <string.h>

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

// Aid: https://csapp.cs.cmu.edu/2e/ch10-preview.pdf

// Goals/Plan: 
// Given "GET http://www.wsu.edu:8080/eecs/index.html HTTP/1.1"
// Parse and determine if valid
// If valid, extract method, hostname, port, path
// Connect to hostname:port
// And forward response back to client

static int parse_request(char *request, char *method, 
                         char *hostname, 
                         char *port, char *path) {

    // ex GET http://www.wsu.edu:8080/eecs/index.html HTTP/1.1
    char url[MAXLINE], ver[MAXLINE], hostport[MAXLINE];

    // Check if malformed 
    if (sscanf(request, "%s %s %s", method, url, ver) != 3) {
        return -1; // malformed request
    }
    
    char *prefix = "http://";
    size_t prefix_len = strlen(prefix);
    
    // Check if malformed
    // if (method == NULL || url == NULL || ver == NULL) {
    //     return -1; // malformed request
    // }

    // Check if method is GET or get
    if (strncasecmp(method, "GET", 3) != 0) {
        return -1; // unsupported method
    }

    // Ensure starts w/ http://
    if (strncasecmp(url, prefix, prefix_len) != 0) {
        return -1; // unsupported URL format
    }

    // Find indices of host, port, path for parse
    const char *host_begin = url + prefix_len;
    const char *path_begin = strchr(host_begin, '/');
    
    // Set Path and Hostport
    if (path_begin != NULL) {
        // Path
        strcpy(path, path_begin);

        // Hostport
        size_t hostport_len = path_begin - host_begin;
        strncpy(hostport, host_begin, hostport_len);
        hostport[hostport_len] = '\0'; // null terminate

    } else {

        // Default path to /
        strcpy(path, "/"); 
        strcpy(hostport, host_begin); 
    }

    char *colon_begin = strchr(hostport, ':');
    if (colon_begin != NULL) {

        // Port specified
        size_t host_len = colon_begin - hostport;
        memcpy(hostname, hostport, host_len);
        hostname[host_len] = '\0';
        strcpy(port, colon_begin + 1); // copy port part

    } else {

        // No port specified, default to 80
        strcpy(hostname, hostport);
        strcpy(port, "80");
    }

    
    return 0;
}


// Read and print client request
static void handle_client(int connfd) {

    // Variable inits
    rio_t rio;
    char buf[MAXLINE]; // buffer to hold client request 

    char method[MAXLINE], hostname[MAXLINE];
    char port[MAXLINE], path[MAXLINE];

    char host_header[MAXLINE] = "";
    int has_host = 0;

    char other_headers[MAXBUF] = "";
    other_headers[0] = '\0';

    Rio_readinitb(&rio, connfd); // init buffer

    printf(" ================================== New Request\n");

    // Find request line
    ssize_t n = Rio_readlineb(&rio, buf, MAXLINE);

    // Check for read error or empty request
    if (n <= 0) {
        return; 
    }

    printf("Request Line:  %s", buf); 

    if (parse_request(buf, method, hostname, port, path) < 0) {
        printf("Malformed request or unsupported method use\n");
        return;
    }

    printf("Request - Method: %s, Hostname: %s, Port: %s, Path: %s\n", 
            method, hostname, port, path);

    // Read request lines while we dont have blank line read
    while(1) {
        n = Rio_readlineb(&rio, buf, MAXLINE);

        if (n <= 0) {
            printf("Closed connection...");
            return; // error or eof
        }

        printf("%s", buf); // print each 

        if (strcmp(buf, "\r\n") == 0) { 
            printf("End of request headers\n");
            break;
        }

        // Case insensitive check for Host header
        if (!strncasecmp(buf, "Host:", 5)) {
            strcpy(host_header, buf);
            has_host = 1;
        } else if (strncasecmp(buf, "Connection:", 11) == 0) {
            // Ignore this header
            continue;
        } else if (strncasecmp(buf, "Proxy-Connection:", 17) == 0) {
            // Ignore this headers
            continue;
        } else if (strncasecmp(buf, "User-Agent:", 11) == 0) {
            // Ignore this header
            continue;
        }
        else {
            // Append to other headers
            strcat(other_headers, buf);
        }
    }

    // Open connection to target server
    int serverfd = Open_clientfd(hostname, port);
    if (serverfd < 0) {
        printf("Failed to connect to server %s:%s\n", hostname, port);
        return;
    }

    rio_t server_rio;
    Rio_readinitb(&server_rio, serverfd);

    char request_buf[MAXBUF];
    int len;

    len = snprintf(request_buf, sizeof(request_buf),
                   "%s %s HTTP/1.0\r\n", 
                   method, path);
    Rio_writen(serverfd, request_buf, len);

    if (has_host) {
        Rio_writen(serverfd, host_header, strlen(host_header));
    } else {
        len = snprintf(request_buf, sizeof(request_buf),
                       "Host: %s\r\n", hostname);
        Rio_writen(serverfd, request_buf, len);
    }

    Rio_writen(serverfd, (void *)user_agent_hdr, strlen(user_agent_hdr));

    len = snprintf(request_buf, sizeof(request_buf),
                   "Connection: close\r\n");
    Rio_writen(serverfd, request_buf, len);

    len = snprintf(request_buf, sizeof(request_buf),
                   "Proxy-Connection: close\r\n");
        // TODO forward the request to the target server TODO
    Rio_writen(serverfd, request_buf, len);

    // Write other headers
    if (other_headers[0] != '\0') {
        Rio_writen(serverfd, other_headers, strlen(other_headers));
    }

    Rio_writen(serverfd, "\r\n", 2); // End of headers

    while ((n = Rio_readnb(&server_rio, buf, MAXLINE)) > 0) {
        Rio_writen(connfd, buf, n);
    }

    Close(serverfd); // Close server connection
}

int main(int argc, char **argv)
{
    // Variable Inits
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    char client_hostname[MAXLINE], client_port[MAXLINE];

    // Check args are present

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // Open listening socket - from csapp.c
    listenfd = Open_listenfd(argv[1]);

    // Main server loop
    while(1) {
        clientlen = sizeof(clientaddr);
        // Accept connection from client
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        // Get client hostname and port
        Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, 
                    MAXLINE, client_port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", 
               client_hostname, client_port);

        // Handle the client request
        handle_client(connfd);

        // Close connection to client
        Close(connfd);
    }

    printf("%s", user_agent_hdr);
    return 0;
}

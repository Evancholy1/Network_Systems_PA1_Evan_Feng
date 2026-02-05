//first commit

#include "nethelp.h" 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>

#define DOCUMENT_ROOT "./www"
#define MAX_REQUEST_SIZE 8192 
#define MAXBUF 4096 



int parse_http_request(char * request, char *method, char *uri, char *version){
    if(sscanf(request, "%15s %255s %15s", method, uri, version) != 3){
        return -1;
    }
    return 0; 
}

void send_http_response(int fd, int status_code, const char *status_message,
                const char *content_type, const char *body, size_t body_len) {
    //create the header from params 
    char header[MAXBUF];

    //HEADER from sample header format
    int header_len = snprintf(header, sizeof(header), 
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n",
             status_code, status_message, content_type, body_len);
    
    if (body && body_len > 0 && header_len + body_len < MAXBUF){
        char response[MAXBUF];
        memcpy(response, header, header_len);//copy header to start of message
        memcpy(response + header_len, body, body_len); //copy body to right after header 
        write(fd, response, header_len + body_len); 
    } else { //response is too big for max buffer so send seperately 
        write(fd, header, header_len);
        if(body && body_len > 0) {
            write(fd, body, body_len);
        }
    }
}

void send_error_response(int fd, int status_code, const char *status_message){
    char body[MAXBUF];
    int body_len = snprintf(body, sizeof(body),
        "<html><body><h1>%d %s</h1><p>%s</p></body></html>",
        status_code, status_message, status_message);

        send_http_response(fd, status_code, status_message, 
                            "text/html", body, body_len);
}

char *get_mime_type(char *file_path) {
    if (strstr(file_path, ".html")) return "text/html";
    if (strstr(file_path, ".htm")) return "text/html";
    if (strstr(file_path, ".txt")) return "text/plain";
    if (strstr(file_path, ".css")) return "text/css";
    if (strstr(file_path, ".js")) return "application/javascript";
    if (strstr(file_path, ".png")) return "image/png";
    if (strstr(file_path, ".gif")) return "image/gif";
    if (strstr(file_path, ".jpg")) return "image/jpg";
    if (strstr(file_path, ".jpeg")) return "image/jpg";
    if (strstr(file_path, ".ico")) return "image/x-icon";
    return "text/plain";
}

//handles sending file given socket, filepath, and size of said file 
void send_file_response(int fd, const char *file_path, off_t fileSize){
    FILE *file = fopen(file_path, "rb");
    if(!file) {
        send_error_response(fd, 500, "Internal Server Error"); 
        return;
    }

    char *fileContent = malloc(fileSize + 1);
    if(!fileContent){
        fclose(file);
        send_error_response(fd, 500, "Internal Server Error");
        return; 
    }
     //copy requested data from file into file_content
    size_t bytesRead = fread(fileContent, 1, fileSize, file);
    fclose(file);

    //if didnt read the whole file try again send erro out 
    if ((long)bytesRead != fileSize) {
        free(fileContent);
        send_error_response(fd, 500, "Internal Server Error");
        return;
    }
    //send contents over it over 
    send_http_response(fd, 200, "OK", get_mime_type((char*)file_path), fileContent, fileSize);

    //free memory 
    free(fileContent);
}

//handle request function that each thread performs
//reads HTTP request from client
// parse it by extractng the method, the path, and version 
// open the requested file from ./www/ 
//send HRRP response headers 
//send the file contents
void handle_request(int connfd){
    char request[MAX_REQUEST_SIZE]; //buffer stores teh HTTP request line
    char method[16], uri[256], version[16]; //components of the request
    char filePath[512]; // ./www + uri
    struct stat fileStat; //file size and permissions
    
    //read and parse the request line 
    readline(connfd, request, MAX_REQUEST_SIZE);

    //clear the rest of request lines from the socket
    char trash[MAXBUF];
    while (readline(connfd, trash, MAXBUF) > 0) {
        if (strcmp(trash, "\r\n") == 0) {
            break;  // Done, stop reading
        }
    }

    //bad request handling and get method, uri, and version
    if(parse_http_request(request, method, uri, version) < 0){
        send_error_response(connfd, 400, "Bad Request");
        return;
    }

    //if not a GET method
    if(strcmp(method, "GET") != 0){
        send_error_response(connfd, 405, "Method Not Allowed");
        return; 
    }

    //if not equal to one 1.0 or 1.1 then send error 
    if (strcmp(version, "HTTP/1.0") != 0 && strcmp(version, "HTTP/1.1") != 0) {
        send_error_response(connfd, 505, "HTTP Version Not Supported");
        return;
    }

    // /=root take to /index.html
    if (strcmp(uri, "/") == 0) {
        strcpy(uri, "/index.html");
    }

    //update our uri based if request was a directory and exists .html/htm
    if(uri[strlen(uri)-1] == '/'){ 
        char indexPath[512];

        //build path 
        snprintf(indexPath, sizeof(indexPath), "%s%sindex.html", DOCUMENT_ROOT, uri);

        //check does the path exist? 
        if(access(indexPath, F_OK) == 0) {
            strcat(uri, "index.html");
        }
        else { 
            //check for .htm extension
            snprintf(indexPath, sizeof(indexPath), "%s%sindex.htm", DOCUMENT_ROOT, uri);
            if (access(indexPath, F_OK) == 0) {
                strcat(uri, "index.htm");
            }
        }
    }

    //create filePath by combining documentroot and uri
    snprintf(filePath, sizeof(filePath), "%s%s", DOCUMENT_ROOT, uri);

     //check for file existence and correct permissions
    if(stat(filePath, &fileStat) < 0) {
        if (errno == ENOENT) {
            send_error_response(connfd, 404, "Not Found");
        } else {
            send_error_response(connfd, 403, "Forbidden");
        }
        return;
    }

    //only serve regular files 
    if (!S_ISREG(fileStat.st_mode)) {
        send_error_response(connfd, 403, "Forbidden");
        return;
    }


    send_file_response(connfd, filePath, fileStat.st_size);
}


void * thread(void *arg){
    //self cleanup 
    pthread_detach(pthread_self()); 

    // get socket number locally
    int connfd = *((int *)arg);
    free(arg);

    handle_request(connfd);
    close(connfd);

    return NULL;
}

int main (int argc, char **argv) {
    int listenfd, *connfd;
    socklen_t clientLen = sizeof(struct sockaddr_in); 
    struct sockaddr clientAddr;
    pthread_t tid;

    //check command line arguments 
    if (argc != 2) {
        fprintf(stderr, "usage %s <port>\n", argv[0]); 
        exit(1);
    }

    int port = atoi(argv[1]); 

    //use helper function to create listening socket
    listenfd = open_listenfd(port); 
    if (listenfd < 0){
        fprintf(stderr, "Failed to create listening socket\n"); 
        exit(1); 
    }

    printf("HTTP Server running on port %d\n", port);
    printf("Document root: %s\n", DOCUMENT_ROOT);

    while (1){
        connfd = malloc(sizeof(int));
        *connfd= accept(listenfd, (struct sockaddr*)&clientAddr, &clientLen);
        if (*connfd < 0) {
            free(connfd);
            continue;
        }
        pthread_create(&tid, NULL, thread, connfd);

    }
}
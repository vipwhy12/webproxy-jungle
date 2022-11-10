#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* Thread가 생성될 때 수행하게 될 함수를 선언*/
void * thread(void *vargp);

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

/* main() : 클라이언트를 연결 할 때마다 연결을 수행하는 Thread를 만들어 동시성 수행 */
int main(int argc, char **argv) {
  int listenfd, *connfdp;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr, sockaddr_storage;
  pthread_t tid;  /* Thread의 식별자 */

  if(argc != 2){
    /* 명령어의 유효성 검사 */
    /* argc : 2 */
    /* argv : ./tiny 5000 */
    fprintf(stderr, "usage: %s<port>\n", argv[0]);
    exit(1);
  }

  /* argv[0] = ./tiny */
  /* argv[1] = 8000 */
  /* 8000포트 오픈해주세요. 그리고 그건 linstenfd 식별자야 */
  listenfd = Open_listenfd(argv[1]);

  /* 클라이언트에게 요청을 받을 수 있도록 while(1) 수행 */
  while (1){
    clientlen = sizeof(clientaddr);
    connfdp = Malloc(sizeof(int));  /* pthread_create의 argp인자는 void형이라 안전하게 포인터를 생성해준다. */
    *connfdp = Accept(listenfd, (SA*)&clientaddr, &clientlen);  /* 포인터값을 connfd로 */
    Getnameinfo((SA*)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    /* sequential handle the client transaction */
    /* doit(connfd); */
    /* Close(connfd); */

    pthread_create(&tid, NULL, thread, connfdp);
  }
}

/* thread() : 생성된 Thread안에서 클라이언트와 통신을 수행 */
void *thread(void *vargp){
  int connfd = *((int *)vargp);
  Pthread_detach(pthread_self()); /* Thread 자신을 분리 */
  Free(vargp);                    /* 동적으로 할당한 파일 식별자 포인터를 free */
  doit(connfd);  
  Close(connfd);
  return NULL;
}

/* doit() : 클라이언트의 요청 파싱 후 서버에 요청을 보냄 */
void doit(int fd)
{
  char HTTP_VERSION = "HTTP/1.0";
  struct stat sbuf;
  rio_t rio;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], port[MAXLINE], hostname[MAXLINE], filename[MAXLINE], cgiargs[MAXLINE], content_length[MAXLINE];
  int clientfd;
  char *proxy_srcp;

  /* 클라이언트가 보낸 요청 헤더에서 method, uri, version을 가져온다. */
  /* GET http://localhost:8000/home.html HTTP/1.1 */
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);

  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s http://%s %s", method, uri, version);
  if(strcasecmp(method, "GET")){
    printf("Proxy does not implement the method");
  }

  /* proxy가 서버에게 보낼 정보를 추출 */
  parse_uri(port, uri, hostname, filename, cgiargs, version, method);

  /* proxy와 서버가 연결할 clientfd open */
  clientfd = Open_clientfd(hostname,port);

  /* clientfd의 유효성 검사 */
  if(clientfd < 0){
    printf("connection faild\n");
    return;
  }
  
  /* 추출한 정보들을 조합해서 서버에 Header를 보내줍니다. */
  make_http_header(method, filename, version, user_agent_hdr, hostname, clientfd);
  read_response(&rio, content_length, fd, clientfd);  /* 서버에서 proxy로 보낸 response를 분해합니다. */

  proxy_srcp = (char*)malloc(atoi(content_length)); /* 헤더가 가지고 있는 content-length를 사용해 body를 읽어올 준비를 합니다. */
  Rio_readnb(&rio, proxy_srcp, atoi(content_length));
  Rio_writen(fd, proxy_srcp, atoi(content_length));
  free(proxy_srcp);
  Close(clientfd);
}

/* make_http_header(): 서버에 보낼 HTTP_HEADER 생성 */
void make_http_header(char *method, char *filename, char* version, char* user_agent_hdr, char* hostname, char* clientfd){
  /* the Make HTTP Header */
  char buf[MAXLINE];

  sprintf(buf, "%s %s %s\r\n", method, filename, version);
  sprintf(buf, "%sHost: %s\r\n", buf, hostname);
  sprintf(buf, "%s%s", buf, user_agent_hdr);
  sprintf(buf, "%sConnection: %s\r\n", buf, "close");
  sprintf(buf, "%sProxy-Connection: %s\r\n\r\n", buf, "close");

  /* 작성한 헤더를 서버에 보내는 함수 */
  Rio_writen(clientfd, buf, strlen(buf));
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""fffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

/* parse_uri() : 원하는 값을 추출 */
int parse_uri(char *port, char *uri, char *hostname, char *filename, char *cgiargs, char *version, char *method){
  
  *port = 8000;
  //*filename = "home/index.html";
  sscanf(uri, "GET %s HTTP/1.0", hostname);
  
  /* 아스키코드 58 : : */
  /* port번호 전에 :이 온다는 것을 기준으로 parse를 진행함. */
  char *parsing_ptr = strchr(uri, 58);

  if(parsing_ptr != NULL){
    *parsing_ptr = '\0';
    strcpy(hostname, uri);
    strcpy(port, parsing_ptr + 1);
    parsing_ptr = strchr(port, 47);

    if (parsing_ptr != NULL){
      strcpy(filename, parsing_ptr);
      *parsing_ptr = '\0';
      strcpy(port, port);
    }
  }else{
    parsing_ptr = strchr(uri, '/');

    if(parsing_ptr != NULL){
      strcpy(filename, parsing_ptr);
      *parsing_ptr = '\0';
      strcpy(hostname, uri);
    }else{
      sscanf(uri,"%s",hostname);
    }
  }
}

/* read_response() : 서버에서 프록시로 보낸 답변을 읽어들입니다. */
void read_response(rio_t* rp, char* content_length, int fd,int clientfd){
  char buf[MAXLINE],header[MAXLINE];

  Rio_readinitb(rp, clientfd);
  Rio_readlineb(rp, buf, MAXLINE);
  sprintf(header,"%s",buf);

  while (strcmp(buf, "\r\n")){ /* HTTP 요청의 Header는 '\r\n'끝난다. */
    /* strstr 함수는 출현하는 주소값을 리턴함 */
    Rio_readlineb(rp, buf, MAXLINE); 
    sprintf(header,"%s%s",header,buf);
    if(strstr(buf,"Content-length") != NULL){
      //파싱해서 Content-length 다음의 것을 가져옵니다!
      char *parsing_ptr = strchr(buf, 58);
      *parsing_ptr = '\0';
      strcpy(content_length, parsing_ptr + 1);
      atoi(content_length);
    }
  }

  /* 추출한 header를 클라이언트에 전송 */
  Rio_writen(fd, header, strlen(header));
  return;
}
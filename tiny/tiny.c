/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */


#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

/* port번호를 인자로 받는다. */
int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr; /* 클라이언트에서 연결 요청 후 클라이언트 연결 소켓 주소*/

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  /* argv[1] : port 번호 */
  /* 해당 포트 번호에 해당하는 듣기 소켓 식별자를 열어준다. */
  listenfd = Open_listenfd(argv[1]);

  /* 클라이언트에게서 받은 연결 요청을 accept 한다. */
  while (1) {
    clientlen = sizeof(clientaddr);
    /* connfd : 서버와 연결할 식별자입니다. */
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept
    /* Getnameinfo를 호출하면서 hostname과 port가 반환 됨 */
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    
    doit(connfd); 
    Close(connfd);  /* 서버 연결 식별자를 닫아준다. */
  }
}

void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* Read reqeust line and headers */
  /* 클라이언트가 rio로 보낸 request라인과 헤더를 읽고 분석한다. */
  Rio_readinitb(&rio, fd);  /* connfd를 연결하여 rio에 저장 */
  Rio_readlineb(&rio, buf, MAXLINE);  /* rio에 있는 string 한 줄을 모두 buffer에 옮긴다. */
  printf("Request headers:\n");
  printf("%s", buf);  /* print : GET /godzilla.gif HTTP/1.1\0 */
  sscanf(buf, "%s %s %s", method, uri, version);  /* buf를 Parse. */

  /* 만약 method가 GET방식이나 HEAD 방식이 아니라면 clienterror로 연결합니다. */
  if (!(strcasecmp(method, "GET") == 0 || strcasecmp(method, "HEAD") == 0)) {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  /* read_requesthdrs 함수는 헤더까지 반복문을 돌면서 헤더를 출력하는 함수 */
  read_requesthdrs(&rio);

  /* Parse URI from GET request */
  /* if static = 1 */
  is_static = parse_uri(uri, filename, cgiargs);

  /* filename 유효성 검사 */
  if(stat(filename, &sbuf) < 0){
    clienterror(fd, filename, "404", "NOT found", "Tiny couldn't find this file");
    return;
  }

  if(is_static){  /* Serve static content */
    /* 일반파일인가요? 혹은 읽기 권한이 있나요? 안돼 돌아가 */
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    /* 유효성 검사를 통과한 static이라면 serve_static 실행 */
    serve_static(fd, filename, sbuf.st_size, method);
  }
  else{ /* Serve dynamic content */
    if(!(S_ISREG(sbuf.st_mode) || !(S_IXUSR & sbuf.st_mode))){
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    /* 유효성 검사를 통과한 dynamic이라면 serve_dynamic 실행 */
    serve_dynamic(fd, filename, cgiargs, method);
  }
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
  
  /* 에러메세지와 응답 본체를 서버 소켓을 통해 클라이언트에 보낸다. */
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

/* uri를 받아 요청받은 filename, cgiargs를 반환한다. */
int parse_uri(char *uri, char * filename, char *cgiargs){
  char *ptr;
  
  /* 과제 요건사항 : cgi-bin은 동적파일로 분류하자 */
  /* 만약 static content 요구라면, 1을 리턴한다. */
  if(!strstr(uri, "cgi-bin")){
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
        printf("%s\n",uri);
    if (uri[strlen(uri)-1] == '/') 
      strcat(filename, "home.html");
    return 1;
  }
else {
  ptr = index(uri, '?');
  if (ptr){
    strcpy(cgiargs, ptr+1);
    *ptr = '\0';
  }
  else{
    strcpy(cgiargs, "");
  }
  strcpy(filename, ".");
  strcat(filename, uri);
  return 0;
  }
}

/* 정적 컨텐츠의 디렉토리를 받아 request 헤더 작성 후 서버에게 보낸다. */
void serve_static(int fd, char *filename, int filesize, char *method)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];
  
  /* Send response headers to cilent */

  /* 응답 라인, 헤더 작성 */
  get_filetype(filename, filetype);     /* find filetype */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");  /* write response */
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: Close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("%s", buf);

  if (strcasecmp(method, "HEAD") == 0)
    return;

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0);
  
  //mmap 함수는 요청한 파일을 가상메모리 영역으로 매핑한다.
  //mmap 함수를 호출하면 파일 srcfd의 첫번째 filesize 바이트 주소 srcp에서 시작하는 사적 읽기 허용 가상메모리 영역으로 매
  srcp = (char*)Malloc(filesize);
  Rio_readn(srcfd, srcp, filesize);
  Close(srcfd);
  Rio_writen(fd, srcp, filesize);  //주소 srcp에서 시작하는 filesizw 바이트를 클라이언트의 연결 식별자로 복사한다,
  free(srcp);
}

/* filename을 조사해 각각의 식별자에 맞는 MIME타입을 filetype에 입력해준다. */
void get_filetype(char * filename, char *filetype)
{
  if(strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if(strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if(strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if(strstr(filename,".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

/* dynamic content 실행 후 응답라인, 헤더 작성 */
/* CGI 자식 프로세스를 fork 후 프로세스 표준 출력을 클라이언트 출력과 연결 함. */
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method)
{
  char buf[MAXLINE], *emptylist[] = {NULL};

  /* Return fist part of HTTP respense */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server : Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if(Fork() == 0){ /* Child */
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1);
    setenv("REQUEST_METHOD", method, 1);
    Dup2(fd, STDOUT_FILENO);
    /* filename을 실행시켜줘 Exeve 리턴을 안해 자식이 나 죽어.... 시그널보내 시그널 보내*/
    Execve(filename, emptylist, environ);
  }
  Wait(NULL); /* 신호를 보내면 부모의 Wait이 끝납니다. */
}

void read_requesthdrs(rio_t * rp)
{
  char buf[MAXLINE];
  Rio_readlineb(rp, buf, MAXLINE);
  printf("%s", buf);
  while(strcmp(buf, "\r\n")){
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}



/*
  * fork에 대해 알아보자 
  * fork()를 실행하면 부모프로세스와 자식 프로세스가 동시에 실행
  * fork()의 반환값이 0 : 자식프로세스라면 if문을 수행
  * fork()의 반환값이 0이 아니라면 : 내가 부모프로세스라면 if문을 수행하지 않고 Wait함수로 이동
  * Wait() : 부모프로세스가 먼저 도달도 자식 프로세스가 종료될 때까지 기다리는 함수 
  * if문 안에서 setnv 시스템콜을 수행해 "Query_String"의 값을 cgiargs로 바꿔준다.(우선순위 0순위)
  * dup2() : CGI 프로세스 출력을 fd로 복사한다.
  * dup2() : 실행 후 STDOUT_FILENO의 값은 fd이다. 
  * dup2() : CGI 프로세스에서 표준 출력을 하면 바로 출력되지 않고 서버 연결 식별자를 거쳐 클라이언트 함수에 출력  
  * execuv() : 파일이름이 첫번째 인자인 것과 같은 파일을 실행한다. 
*/
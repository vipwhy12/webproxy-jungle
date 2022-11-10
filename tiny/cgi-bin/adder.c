/*
 * head-adder.c - a minimal CGI program that adds two numbers together
 */
#include "../csapp.h"

int main(void) {
  char *buf, *p, *method;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0, n2=0;

  /* Extract the two arguments */
  //getenv 함수로 buf에 저장
  //문자가 처음나오는 곳을 포인터(주소값으로)로 지정
  //C언어에서 문자열에 한에서 NULL은 끝을 의미 
  //buf[0] 문자열은 배열이야 문자열을 (arg 문자열의 제일 처음 주소값을 가리킨다)

  if ((buf = getenv("QUERY_STRING")) != NULL) {
    p = strchr(buf, '&'); //fist=1  fist=2
    *p = '\0'; //\0 =NULL
    //NULL을 만나면 끝나요
    //아스키 코드의 
    sscanf(buf, "arg1=%d", &n1);
    sscanf(p+1, "arg2=%d", &n2);

    //strcpy(arg1, buf);
    //strcpy(arg2, p+1);
    n1 = atoi(arg1);
    n2 = atoi(arg2);
                        
  }

  method = getenv("REQUEST_METHOD");

  /* Make the response body */
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>",
      content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  /* Generate the HTTP response */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");

  if (strcasecmp(method, "HEAD") != 0)
    printf("%s", content);

  fflush(stdout);

  exit(0);
}
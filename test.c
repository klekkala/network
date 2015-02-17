#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<netdb.h>
int con[14];
int main(int argc, char* argv[])
{
struct addrinfo addr, *caddr;
struct sockaddr *dup;
memset(&addr, 0, sizeof(addr));
addr.ai_family = AF_UNSPEC;
addr.ai_socktype = SOCK_STREAM;
getaddrinfo("www.google.com", "80", &addr, &caddr);
dup = caddr->ai_addr;
int i=0;
for(i=0;i<14;i++){
printf("%d\n",dup->sa_data[i]);
}
int sock = socket(caddr->ai_family, caddr->ai_socktype,
caddr->ai_protocol);
connect(sock, caddr->ai_addr, caddr->ai_addrlen);
return EXIT_SUCCESS;
}


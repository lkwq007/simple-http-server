//#include <stdio.h>
//#include <pthread.h>
//#include "thpool.h"
//
//
//void task1(){
//	printf("Thread #%u working on task1\n", (int)pthread_self());
//}
//
//
//void task2(){
//	printf("Thread #%u working on task2\n", (int)pthread_self());
//}


//int main(){
//	

//}
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/select.h>
#include "libinetsocket.h"
#include "thpool.h"
#define MIMETYPE "mime-types.tsv"
#define HTTP_200 0
#define HTTP_404 1
#define HTTP_500 2
#define HTTP_GET 0
#define HTTP_POST 1
#define VER_LOGIN 111111
#define VER_PASS 999999
char http_status[3][64] = { "200 OK","404 Not Found","500 Internal Server Error" };
char http_template[256] = "HTTP/1.1 %s\nServer: Lnyan's Simple HTTP Server\nContent-Length: %d\nContent-Type: %s\n\n";
char verify_template[256] = "<!DOCTYPE html><html><head><title>%s</title></head><body><p>%s</p></body></html>";
char LOGIN[128] = "username";
char PASS[128] = "password";
struct Client_
{
	char *host;
	char *port;
	int cfd;
	int sfd;
};
typedef struct Client_ *Client;
Client initClient(char *host, char *port, int cfd, int sfd)
{
	Client p = (Client)malloc(sizeof(*p));
	p->host = host;
	p->port = port;
	p->cfd = cfd;
	p->sfd = sfd;
	return p;
}

void urldecode2(char *dst, const char *src)
{
	char a, b;
	while (*src)
	{
		if ((*src == '%') &&
			((a = src[1]) && (b = src[2])) &&
			(isxdigit(a) && isxdigit(b)))
		{
			if (a >= 'a')
				a -= 'a' - 'A';
			if (a >= 'A')
				a -= ('A' - 10);
			else
				a -= '0';
			if (b >= 'a')
				b -= 'a' - 'A';
			if (b >= 'A')
				b -= ('A' - 10);
			else
				b -= '0';
			*dst++ = 16 * a + b;
			src += 3;
		}
		else if (*src == '+')
		{
			*dst++ = ' ';
			src++;
		}
		else
		{
			*dst++ = *src++;
		}
	}
	*dst++ = '\0';
}

char * get_mime_type(char *name) {
	char *ext = strrchr(name, '.');
	char delimiters[] = " \n";
	char *mime_type = NULL;
	char *pline,*temp;
	char *token,*ttoken;
	int line_counter = 1;
	ext++; // skip the '.';
	FILE *mime_type_file = fopen(MIMETYPE, "r");
	if (mime_type_file != NULL)
	{
		pline = malloc(128 * sizeof(char));
		temp = pline;
		while (fgets(pline, 128, mime_type_file) != NULL)
		{
			if (line_counter > 1)
			{
				ttoken = pline;
				if ((token = strtok_r(pline, delimiters,&pline)) != NULL)
				{
					token = ttoken;
					if (strcmp(token, ext) == 0)
					{
						ttoken = pline;
						token = strtok_r(pline, delimiters,&pline);
						token = ttoken;
						mime_type = malloc(128 * sizeof(char));
						strcpy(mime_type, token);
						break;
					}
				}
			}
			line_counter++;
			pline = temp;
		}
		fclose(mime_type_file);
	}
	else
	{
		perror("open");
	}
	return mime_type;
}

int is_regular_file(const char *path)
{
	struct stat path_stat;
	stat(path, &path_stat);
	return S_ISREG(path_stat.st_mode);
}

int server(Client client)
{
	fd_set input;
	int ret, length, header_done = 0, bytes_left = 0, buf_offset = 0;
	int method, get_file;
	struct stat get_stat;
	off_t offset = 0L;
	char buf[8192], header[8192], body[8192], *delimiter, field[128], url[2048] ,durl[2048] ,path[2048]=".";
	char rep[8192], *rep_type, *match, dir_buf[512];;
	int rep_stat=0, rep_len=0;
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 2000;
	FD_ZERO(&input);
	FD_SET(client->cfd, &input);
	while (1)
	{
		//ret = select(client->cfd + 1, &input, NULL, NULL, &timeout);
		//if (ret == 0)// timeout
		//{
		//	continue;
		//}
		//else if (ret == -1 || !(FD_ISSET(client->cfd, &input)))
		//{
		//	printf("Connectiont Error with %s:%s", client->host, client->port);
		//	destroy_inet_socket(client->cfd);
		//	return 1;// error 
		//}
		ioctl(client->cfd, FIONREAD, &length);
		if (length < 0)
		{
			break;
		}
		// assume buf is large enough
		if (length == 0)
		{
			length = 1;
		}
		ret = recv(client->cfd, buf + buf_offset, length*sizeof(char), 0);
		printf("%u: recv %d bytes from %s:%s\n", (int)pthread_self(),ret, client->host, client->port);
		if (ret < 0)
		{
			printf("Connectiont Error with %s:%s\n", client->host, client->port);
			destroy_inet_socket(client->cfd);
			free(client);
			return 1;// error 
		}
		else if (ret == 0)
		{
			printf("Connectiont Closed with %s:%s\n", client->host, client->port);
			shutdown_inet_stream_socket(client->cfd, LIBSOCKET_BOTH);
			destroy_inet_socket(client->cfd);
			free(client);
			return 1;// error 
		}
		buf[ret + buf_offset] = '\0';
		buf_offset += ret;
		if (!header_done)
		{
			if ((delimiter = strstr(buf, "\n\n")))
			{
				char temp;
				temp = *(delimiter + 2);
				*(delimiter + 2) = '\0';
				strcpy(header, buf);
				*(delimiter + 2) = temp;
				delimiter += 2;
				header_done = 1;
			}
			else if ((delimiter = strstr(buf, "\r\n\r\n")))
			{
				char temp;
				temp = *(delimiter + 4);
				*(delimiter + 4) = '\0';
				strcpy(header, buf);
				*(delimiter + 4) = temp;
				delimiter += 4;
				header_done = 1;
			}
			if (header_done)
			{
				char *temp, c,*token;
				//match = malloc(8192 * sizeof(char));
				//strcpy(match, header);
				match = header;
				token = match;
				while ((temp = strtok_r(match, "\n", &match))!=NULL)
				{
					// using temp may cause seg fault, i cannot figure out why it happened
					temp = token;
					sscanf(temp, "%[^:^ ]", field);
					if (!strcmp(field, "GET"))
					{
						method = HTTP_GET;
						sscanf(temp, "%*s%s", url);
						printf("get %s from %s:%s \n", url, client->host, client->port);
					}
					else if (!strcmp(field, "POST"))
					{
						method = HTTP_POST;
						sscanf(temp, "%*s%s", url);
						printf("post %s from %s:%s\n", url, client->host, client->port);
					}
					else if (!strcmp(field, "Content-Length"))
					{
						int stream_left = strlen(delimiter);
						sscanf(temp, "%*s%d", &bytes_left);
						printf("%d %d\n", stream_left, bytes_left);
						if (stream_left >= bytes_left)
						{
							strncpy(body, delimiter, bytes_left);
							body[bytes_left] = '\0';
							strcpy(rep, delimiter + bytes_left);
							bytes_left = 0;
							strcpy(buf, rep);
							buf_offset = strlen(buf);
						}
						else
						{
							// shall keep recv until bytes_left==0
							bytes_left -= strlen(delimiter);
							
						}
					}
					token = match;
					//match = NULL;
				}
				if (delimiter[0] == '\0')
				{
					bytes_left = 0;
					buf_offset = 0;
				}
				//temp = strdup(url); may cause seg fault
				strcpy(durl, url);
				urldecode2(url, durl);
				//free(temp);
				if (bytes_left > 0)
				{
					while (bytes_left > 0)
					{
						ret = recv(client->cfd, buf + buf_offset, bytes_left, 0);
						if (ret < 0)
						{
							printf("Connectiont Error with %s:%s", client->host, client->port);
							destroy_inet_socket(client->cfd);
							free(client);
							return 1;// error 
						}
						buf[buf_offset + ret] = '\0';
						bytes_left -= ret;
						buf_offset += ret;
					}
					strcpy(body, delimiter);
					buf_offset = 0;
				}
				rep_len = 0;
				switch (method)
				{
				case HTTP_GET:
				{
					strcpy(path, ".");
					strcat(path, url);
					rep_type = get_mime_type(path);
					rep_len = 0;
					if (!rep_type)
					{
						rep_type = malloc(128 * sizeof(char));
						strcpy(rep_type, "application/octet-stream");
					}
					break;
				}
				case HTTP_POST:
				{
					//int login, pass;
					char login[128], pass[128];
					//rep_type = strdup("text/html");
					rep_type = malloc(128 * sizeof(char));
					strcpy(rep_type, "text/html");
					if (!strcmp(url, "/dopost"))
					{
						rep_stat = HTTP_200;
						printf("\n%s\n", body);
						//if (((sscanf(body, "login=%lld&pass=%lld", &login, &pass)) > 0 ||
						//	(sscanf(body, "pass=%lld&login=%d", &pass, &login)) > 0) && login == VER_LOGIN&&pass == VER_PASS)
						if (((sscanf(body, "login=%[^&]&pass=%s", login, pass)) > 0 ||
							(sscanf(body, "pass=%[^&]&login=%s", pass, login)) > 0) && !strcmp(login,LOGIN)&&!strcmp(pass,PASS))
						{
							sprintf(rep, verify_template, "login success", "login success");
						}
						else
						{
							sprintf(rep, verify_template, "login fail", "login fail");
						}
						rep_len = strlen(rep);
					}
					else
					{
						rep_stat = HTTP_404;
						rep_len = 0;
					}
					break;
				}
				default:
				{
					rep_type = malloc(128 * sizeof(char));
					strcpy(rep_type, "text/html");
					rep_stat = HTTP_500;
					rep_len = 0;
				}
				}
				if (rep_stat == HTTP_200&&method == HTTP_GET)
				{
					//FILE *file;
					//file = fopen(path, "rb");
					//if (file != NULL)
					//{
					//	fseek(file, 0, SEEK_END);
					//	long fsize = ftell(file);
					//	fseek(file, 0, SEEK_SET);
					//	char *fbuf = malloc(fsize + 1);
					//	fread(fbuf, fsize, 1, file);
					//	fclose(file);
					//	fbuf[fsize] = 0;
					//	sprintf(header, http_template, http_status[rep_stat], fsize, rep_type);
					//	free(rep_type);
					//	send(client->cfd, header, strlen(header), 0);
					//	send(client->cfd, fbuf, fsize, 0);
					//	free(fbuf);
					//}
					//else
					//{
					//	rep_len = 0;
					//	rep_stat = HTTP_500;
					//	sprintf(header, http_template, http_status[rep_stat], rep_len, rep_type);
					//	free(rep_type);
					//	send(client->cfd, header, strlen(header), 0);
					//}
					if (path[strlen(path) - 1] != '/'&&is_regular_file(path))
					{
						get_file = open(path, O_RDONLY);
						if (get_file == -1)
						{
							rep_stat = HTTP_404;
							rep_len = 0;
							sprintf(header, http_template, http_status[rep_stat], rep_len, rep_type);
							free(rep_type);
							send(client->cfd, header, strlen(header), 0);
						}
						else
						{
							rep_stat = HTTP_200;
							fstat(get_file, &get_stat);
							rep_len = get_stat.st_size;
							sprintf(header, http_template, http_status[rep_stat], rep_len, rep_type);
							free(rep_type);
							send(client->cfd, header, strlen(header), 0);
							sendfile(client->cfd, get_file, &offset, rep_len);
							close(get_file);
						}
					}
					else
					{
						DIR *dir;
						struct dirent *ent;
						rep_type = malloc(128 * sizeof(char));
						strcpy(rep_type, "text/html");
						rep_len = 0;
						rep[0] = '\0';
						if (dir = opendir(path))
						{
							strcat(rep, "<!DOCTYPE html><html><head><title>Directory</title></head><body>");
							if (url[strlen(url) - 1] != '/')
							{
								strcat(url, "/");
							}
							while ((ent = readdir(dir)) != NULL) {
								sprintf(dir_buf,"<p><a href=\"%s%s\">%s</a></p>", url, ent->d_name,ent->d_name);
								strcat(rep, dir_buf);
							}
							close(dir);
							strcat(rep, "</body></html>");
							rep_stat = HTTP_200;
							rep_len = strlen(rep);
						}
						else
						{
							rep_stat = HTTP_404;
							rep_len = 0;
						}
						sprintf(header, http_template, http_status[rep_stat], rep_len, rep_type);
						free(rep_type);
						send(client->cfd, header, strlen(header), 0);
						send(client->cfd, rep, rep_len, 0);
					}
				}
				else
				{
					sprintf(header, http_template, http_status[rep_stat], rep_len, rep_type);
					free(rep_type);
					send(client->cfd, header, strlen(header), 0);
					send(client->cfd, rep, rep_len, 0);
				}
				if (buf_offset != 0)
				{
					header_done = 0;
				}
				else
				{
					break;
				}
			}
		}
	}
	shutdown_inet_stream_socket(client->cfd, LIBSOCKET_BOTH);
	destroy_inet_socket(client->cfd);
	printf("end\n");
	free(client);
	return 0;
}

int main(void)
{
	char buf[8192];
	char address[32] = "0.0.0.0", port[32] = "8080";
	int sfd = create_inet_server_socket(address, port, LIBSOCKET_TCP, LIBSOCKET_IPv4, 0);
	if (sfd < 0)
	{
		printf("Error, cannot create server socket");
		exit(1);
	}
	printf("Listening %s at port %s..\n", address, port);
	threadpool thpool = thpool_init(16);
	printf("Thead pool has started.\n");
	while (1)
	{
		char *host = (char *)malloc(128);
		char *service = (char *)malloc(128);
		int cfd = accept_inet_stream_socket(sfd, host, 127, service, 127, LIBSOCKET_NUMERIC, 0);
		if (cfd < 0)
		{
			free(host);
			free(service);
			continue;
		}
		Client client = initClient(host, service, cfd, sfd);
		//server(client);
		thpool_add_work(thpool, (void *)server, client);
	}
	thpool_destroy(thpool);
	destroy_inet_socket(sfd);
	return 0;
}
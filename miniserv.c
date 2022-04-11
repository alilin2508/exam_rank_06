#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>


fd_set fds, rfds, wfds;
int client = 0, fd_max = 0;
int idx[65536];
char *msg[65536];
char rbuf[1024], wbuf[100];

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void fatal()
{
  write(2, "Fatal error\n", 12);
  exit(1);
}

void notify(int fd, char *str)
{
  for (int i = 0; i <= fd_max; i++)
  {
    if (FD_ISSET(i, &wfds) && fd != i)
      send(i, str, strlen(str), 0);
  }
}

void add_client(int fd)
{
  fd_max = fd > fd_max ? fd : fd_max;
  idx[fd] = client++;
  msg[fd] = NULL;
  FD_SET(fd, &fds);
  sprintf(wbuf, "server: client %d just arrived\n", idx[fd]);
  notify(fd, wbuf);
}

void remove_client(int fd)
{
  sprintf(wbuf, "server: client %d just left\n", idx[fd]);
  notify(fd, wbuf);
  free(msg[fd]);
  msg[fd] = NULL;
  FD_CLR(fd, &fds);
  close(fd);
}

void deliver(int fd)
{
  char *str;

  while (extract_message(&msg[fd], &str))
  {
    sprintf(wbuf, "client %d: ", idx[fd]);
    notify(fd, wbuf);
    notify(fd, str);
    free(str);
    str = NULL;
  }
}


int main(int ac, char **av)
{

  if (ac != 2)
  {
    write(2, "Wrong number of arguments\n", 26);
    exit(1);
  }

  FD_ZERO(&fds);

	int sockfd;

	// socket create and verification
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
    fatal();
  FD_SET(sockfd, &fds);

  fd_max = sockfd;


  struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));

	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1]));

	// Binding newly created socket to given IP and verification
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
    fatal();
	if (listen(sockfd, 10) != 0)
    fatal();

  while(1)
  {
    rfds = wfds = fds;
    if (select(fd_max + 1, &rfds, &wfds, NULL, NULL) < 0)
      fatal();
    for (int fd = 0; fd <= fd_max; fd++)
    {
      if (!FD_ISSET(fd, &rfds))
        continue;
      if (fd == sockfd)
      {
        socklen_t addrlen = sizeof(servaddr);
        int clients = accept(sockfd, (struct sockaddr *)&servaddr, &addrlen);
        if (clients >= 0)
        {
          add_client(clients);
          break;
        }
      }
      else
      {
        int readed = recv(fd, rbuf, 1024, 0);
        if (readed <= 0)
        {
          remove_client(fd);
          break;
        }
        rbuf[readed]= '\0';
        msg[fd] = str_join(msg[fd], rbuf);
        deliver(fd);
      }
    }
  }
  return (0);
}

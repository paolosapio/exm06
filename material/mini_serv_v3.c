#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

typedef struct s_miniserv
{
	int		max_fd;
	int		client_count;
	int		client_id[FD_SETSIZE];
	char	*msgs[FD_SETSIZE];
	fd_set	all_fd_sock;
	fd_set	rfds;
	fd_set	wfds;
	char	buf_read[1024];
	char	buf_write[1024];
	int		socket_server;
}				t_miniserv;

int	extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int		i;

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

char	*str_join(char *buf, char *add)
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

void	fatal_error(void)
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

void	notify_other(int fd_author, char *str, t_miniserv *server)
{
	int	fd;

	for (fd = 0; fd <= server->max_fd; fd++)
	{
		if (FD_ISSET(fd, &server->wfds) && fd != fd_author && fd != server->socket_server)
			send(fd, str, strlen(str), 0);
	}
}

void	register_client(int fd, t_miniserv *server)
{
	server->max_fd = fd > server->max_fd ? fd : server->max_fd;
	server->client_id[fd] = server->client_count++;
	server->msgs[fd] = NULL;
	FD_SET(fd, &server->all_fd_sock);
	sprintf(server->buf_write, "server: client %d just arrived\n",
		server->client_id[fd]);
	notify_other(fd, server->buf_write, server);
}

void	remove_client(int fd, t_miniserv *server)
{
	sprintf(server->buf_write, "server: client %d just left\n",
		server->client_id[fd]);
	notify_other(fd, server->buf_write, server);
	free(server->msgs[fd]);
	FD_CLR(fd, &server->all_fd_sock);
	close(fd);
}

void	send_msg(int fd, t_miniserv *server)
{
	char	*msg;

	while (extract_message(&(server->msgs[fd]), &msg))
	{
		sprintf(server->buf_write, "client %d: ", server->client_id[fd]);
		notify_other(fd, server->buf_write, server);
		notify_other(fd, msg, server);
		free(msg);
	}
}

void	create_socket(t_miniserv *server)
{
	server->max_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server->max_fd == -1)
		fatal_error();
	FD_SET(server->max_fd, &server->all_fd_sock);
	server->socket_server = server->max_fd;
}

int	main(int ac, char **av)
{
	t_miniserv			server = {0};
	struct sockaddr_in	servaddr;
	int					fd;
	int					client_fd;
	int					read_bytes;

	if (ac != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}
	FD_ZERO(&server.all_fd_sock);
	create_socket(&server);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433);
	servaddr.sin_port = htons(atoi(av[1]));
	if (bind(server.socket_server, (const struct sockaddr *)&servaddr,
			sizeof(servaddr)))
		fatal_error();
	if (listen(server.socket_server, SOMAXCONN))
		fatal_error();
	while (1)
	{
		server.rfds = server.all_fd_sock;
		server.wfds = server.all_fd_sock;
		if (select(server.max_fd + 1, &server.rfds, &server.wfds,
				NULL, NULL) == -1)
			fatal_error();
		for (fd = 3; fd <= server.max_fd; fd++)
		{
			if (!FD_ISSET(fd, &server.rfds))
				continue ;
			if (fd == server.socket_server)
			{
				client_fd = accept(server.socket_server, NULL, NULL);
				if (client_fd >= 0)
				{
					register_client(client_fd, &server);
					break ;
				}
			}
			else
			{
				read_bytes = recv(fd, server.buf_read, 1000, 0);
				if (read_bytes <= 0)
				{
					remove_client(fd, &server);
					break ;
				}
				server.buf_read[read_bytes] = '\0';
				server.msgs[fd] = str_join(server.msgs[fd], server.buf_read);
				send_msg(fd, &server);
			}
		}
	}
	return (0);
}

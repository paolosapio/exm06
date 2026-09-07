#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <sys/select.h>

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

void err_msg()
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

/*
	structuras de clientes y del server:
*/
typedef struct	s_clientes
{
	char		read_msg[1024];
	int			clientes_id[1024];
	long		corrent_id;
}				t_clientes;

typedef struct	s_server
{
	t_clientes	clientes;
	fd_set		read_fds;
	fd_set		write_fds;
	fd_set		master_fds;
	int			sock_fd;
}				t_server;



int main(int argn, char **argv)
{
	if (argn != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		return (1);
	}
	
	int					port = atoi(argv[1]);
	struct	sockaddr_in	servaddr;
	t_server			server = {0};
	
	// socket create and verification
	server.sock_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (server.sock_fd == -1)
			err_msg();

	// assign IP, PORT
	bzero(servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port);

	// Binding newly created socket to given IP and verification
	if ((bind(server.sock_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		err_msg();

	if (listen(server.sock_fd, 10) != 0)
		err_msg();

	FD_ZERO(&server.master_fds);
	FD_SET(server.sock_fd, &server.master_fds);

	//que empiece la fiesta!
	while(1)
	{
		// reset trenes de fds en  lectura y eescritura:
		server.read_fds = server.master_fds;
		server.write_fds = server.master_fds;

		//select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
		if (select(FD_SETSIZE, &server.read_fds, &server.write_fds, NULL, NULL) != 0)
			continue ;

		// GESTION NUEVA CONECCION!
		// int  FD_ISSET(int fd, fd_set *set);
		int new_client_coneccion;
		if (FD_ISSET(server.sock_fd, &server.read_fds) != 0)
		{
			new_client_coneccion = accept(server.sock_fd, NULL, NULL);
			if (new_client_coneccion == -1)
				continue ;
			
		}

	}
}
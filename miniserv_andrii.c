#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
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

void msg_err()
{
	write(2, "Fatal error\n", 12);
	exit(1);
}
 
// =========================================================================

typedef struct	s_clients
{
	int			fd[9999];
	char		*msg_r_parzial[9999];
	int			id_clientes[9999];
}				t_clients;

typedef struct	s_server
{
	t_clients	clients;

	fd_set		bkp;
	fd_set		read_fds;
	fd_set		writefds;
}				t_server;

int main(int argn, char **argv)
{
	if (argn != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}

	int puerto = atoi(argv[1]);

	int fd_server; //sockfd
	int fd_client; //connfd
	
	struct sockaddr_in servaddr; //la structura para decir all kerner donde escuchar

	// socket create and verification 
	fd_server = socket(AF_INET, SOCK_STREAM, 0); 
	if (fd_server == -1)
	{ 
		msg_err();
	}
	bzero(&servaddr, sizeof(servaddr));

	// assign IP, PORT
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(puerto);

	// Binding newly created socket to given IP and verification
	if ((bind(fd_server, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
	{
		msg_err();
	} 
	
	if (listen(fd_server, 10) != 0)
	{
		msg_err(); 
	}

	// aqui el server se ha inicializado bien
	t_server server = {0};
 
	while (1)
	{

	}
	fd_client = accept(fd_server, 0 , 0);
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
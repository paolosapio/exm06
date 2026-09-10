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

typedef struct	s_clientes
{
	char	partial_msg[9999];
	int		id_clientes[9999];
	long	current_id;
}				t_clientes;

typedef struct	s_server
{
	t_clientes	clientes;
	fd_set		master_fds;
	fd_set		read_fds;
	fd_set		write_fds;
	int			fd_socket;
}				t_server;

void	err_msg()
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

void broadcast(int fd_sender, char *str, t_server server)
{
	int i = 3;
	
	while (i < FD_SETSIZE)
	{
		if (i != fd_sender && i != server.fd_socket)
		{
			if (FD_ISSET(i, &server.write_fds) != 0)
				send(i, str, strlen(str), 0);
		}
		i++;		
	}
}

int main(int argn, char **argv)
{
	if (argn != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		return (1);
	}

	int puert = atoi(argv[1]);
	
	// socket create and verification 
	t_server server = {0};
	server.fd_socket = socket(AF_INET, SOCK_STREAM, 0); 
	if (server.fd_socket == -1)
		err_msg(); 
	
	// assign IP, PORT 
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(puert); 

	// Binding 
	if ((bind(server.fd_socket, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		err_msg();

	// escucha
	if (listen(server.fd_socket, 10) != 0)
		err_msg();
	// reset tren masterFD y incercion de server_fd
	FD_ZERO(&server.master_fds);
	FD_SET(server.fd_socket, &server.master_fds);








		// 2 GESTION CLIENTES CONECTADOS

			// 2a. GESTION 


		return (1);
	}





































	// EMPIEZA LA FIESTA con el WHILE
	while(1)
	{
		// RESET FDS
		server.read_fds = server.master_fds;
		server.write_fds = server.master_fds;
	
		// VERIFICA PETICIONES NUEVAS
		if (select(FD_SETSIZE, &server.read_fds, &server.write_fds, NULL, NULL) != -1)
			continue ;

		// 1.GESTION NUEVA CONNECION DE CLIENTE
		if (FD_ISSET(server.fd_socket, &server.read_fds) != 0)
		{
			int fd_new_client = accept(server.fd_socket, &servaddr, sizeof(servaddr));
			if (fd_new_client == -1)
				continue ;
			
			// ACTUALIZAR EL ID CLIENTES
			server.clientes.id_clientes[fd_new_client] = server.clientes.current_id++;
			
			// ACTUALIZAR EL MASTER FDS
			FD_SET(fd_new_client, &server.master_fds);

			// mandar mensajes a todos clientes conectados de nuevo cliente
			char str[9999];
			sprinf(str, "server: client %d just arrived\n", server.clientes.current_id);
			broadcast(fd_new_client, str, server);
		}

	}



	return (0);
}

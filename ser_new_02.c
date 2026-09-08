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


typedef struct	s_clientes
{
	char		msg_read[1024];
	int			clients_id[1024];
	long		contador_id;
}				t_clientes;

typedef struct	s_server
{
	t_clientes	clientes;
	fd_set		read_fds;
	fd_set		write_fds;
	fd_set		master_fds;
	int			sockfd;
}				t_server;

void send_all(int sender, char *str, t_server *server)
{
	int i = 3;

	while (i < FD_SETSIZE)
	{
		if (i != sender && i != server->sockfd)
		{
			if (FD_ISSET(i, &server->write_fds) != 0)
				send(i, str, strlen(str), 0);
			printf("caca2\n");
		}
		i++;
	}
}

int main(int argn, char **argv)
{
	// CONTROL ARGUENTOS
	if (argn != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		return (1);
	}

	int					port = atoi(argv[1]);
	struct sockaddr_in	servaddr;
	t_server			server = {0};

	// CREAR SOCKET
	server.sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (server.sockfd == -1)
		msg_err(); 

	// LIMPIAR MEMORIA SERVERADDR
	bzero(&servaddr, sizeof(servaddr));

	// ASSIGNAR IP y PUERTO
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port);

	// Binding (ENLAZAR) newly created socket to given IP and verification
	if ((bind(server.sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		msg_err();

	// ACTIVAR LA ESCUCHA DEL SERVER
	if (listen(server.sockfd, 10) != 0)
		msg_err();

	// RESET Y INICIALIZACION LISTA DE FDS
	FD_ZERO(&server.master_fds);
	FD_SET(server.sockfd, &server.master_fds);

	// QUE EMPIECE LA FIESTA
	while(1)
	{
		// COPIA FDS DESDE MASTER_FDS
		server.read_fds = server.master_fds;
		server.write_fds = server.master_fds;

		// UTILIZA SELECT PARA SEGUIR, ver man SELECT y pasale NUL, NNUL como ultimos  argumentos
		if (select(FD_SETSIZE, &server.read_fds, &server.write_fds, NULL, NULL) == -1)
			continue ;

		// 1. GESTIÓN DE NUEVAS CONEXIONES (SERVER SOCKET)
		// utilizar un if para verificar que FD_IFSET server.sockfd esta en read_fds de != 0
		// osea confirmar que hai nueva conecion
		if (FD_ISSET(server.sockfd, &server.read_fds) != 0)
		{
			// int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
			int fd_new_client;
			fd_new_client = accept(server.sockfd, NULL, NULL);
			if (fd_new_client == -1)
				continue ;

			// meter el nuevo id de los clientes en el array:
			server.clientes.clients_id[fd_new_client] = server.clientes.contador_id++;
			
			// actualizar el master_fds
			FD_SET(fd_new_client, &server.master_fds);
			
			// mandar msg a todos los clientes conectados con funcion especifica!!
			char str[1024];
			sprintf(str, "server: client %d just arrived\n", server.clientes.clients_id[fd_new_client]);
			send_all(fd_new_client, str, &server);
		}
		// 2. GESTIÓN DE CLIENTES YA CONECTADOS




	}
}
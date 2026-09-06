#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <stdbool.h>

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
	char		*msg_r_parzial[9999]; 
	int			id_clientes[9999];
	long		current_id; 
}				t_clients;

typedef struct	s_server
{
	t_clients	clients;
	fd_set		bkp_fds;
	fd_set		read_fds;
	fd_set		writefds;
	int			fd_socket;
}				t_server;

// =========================================================================
// Función auxiliar para enviar mensajes a todos los clientes (menos al que lo envía)
void send_all(int sender_fd, char *str, t_server *server)
{
	int i = 3;
	while (i < FD_SETSIZE)
	{
		if (i != server->fd_socket && i != sender_fd)
		{
			if (FD_ISSET(i, &server->writefds) == true)
			{
				send(i, str, strlen(str), 0);
			}
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

	int					puerto = atoi(argv[1]);
	t_server			server = {0};
	
	// INIZIALIZO PUERTO Y IP
	struct sockaddr_in	servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(puerto);

	// CREO SOCKET
	server.fd_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server.fd_socket == -1)
		msg_err();

	// BINDING - UNIR el SOCKET con el "IP Y PUERTO"
	if ((bind(server.fd_socket, (const struct sockaddr *)&servaddr, sizeof(servaddr))) == -1)
		msg_err();

    // activa SERVIDOR para eschuchar con LISTEN
	if (listen(server.fd_socket, 10) == -1)
		msg_err();

    // limpieza tren master de fds y insercion socket del server en bkp_fds
	FD_ZERO(&server.bkp_fds); 
	FD_SET(server.fd_socket, &server.bkp_fds); 

    // empieza la fiesta
	while (1)
	{

        server.read_fds = server.bkp_fds;
		server.writefds = server.bkp_fds;
        
		if (select(FD_SETSIZE, &server.read_fds, &server.writefds, NULL, NULL) == -1)
        continue ;
        
		// 1. GESTIÓN DE NUEVAS CONEXIONES (SERVER SOCKET)
        int	fd_new_connect;
		if (FD_ISSET(server.fd_socket, &server.read_fds) == true)
		{
			fd_new_connect = accept(server.fd_socket, NULL, NULL);
			if (fd_new_connect == -1)
				continue ;
			else
			{
				server.clients.id_clientes[fd_new_connect] = server.clients.current_id++;
				FD_SET(fd_new_connect, &server.bkp_fds);

				char str[1024];
				sprintf(str, "server: client %d just arrived\n", server.clients.id_clientes[fd_new_connect]);
				send_all(fd_new_connect, str, &server);
			}
		}

		// 2. GESTIÓN DE CLIENTES YA CONECTADOS
		int i = 3;
		while (i < FD_SETSIZE)
		{
			if (i == server.fd_socket)
			{
				i++;
				continue ; 
			}

			if (FD_ISSET(i, &server.read_fds) == true)
			{
				char buf[1024];
				int ret = recv(i, buf, sizeof(buf) - 1, 0); //ret: return value // recv: recive

				// 2.A. CLIENTE SE DESCONECTA
				if (ret <= 0)
				{
					char str[1024];
					sprintf(str, "server: client %d just left\n", server.clients.id_clientes[i]);
					send_all(i, str, &server);

					// Limpiar buffer del cliente si quedó algo colgado
					if (server.clients.msg_r_parzial[i] != NULL)
					{
						free(server.clients.msg_r_parzial[i]);
						server.clients.msg_r_parzial[i] = NULL;
					}

					close(i);
					FD_CLR(i, &server.bkp_fds);
				}
				// 2.B. CLIENTE ENVÍA UN MENSAJE
				else
				{
					buf[ret] = 0; 
					server.clients.msg_r_parzial[i] = str_join(server.clients.msg_r_parzial[i], buf);
					if (server.clients.msg_r_parzial[i] == 0)
						msg_err();
					
					char *line;
					// Extraemos línea a línea los mensajes completos (separados por \n)
					while (extract_message(&server.clients.msg_r_parzial[i], &line) == 1)
					{
						char prefix[64];
						sprintf(prefix, "client %d: ", server.clients.id_clientes[i]);
						
						send_all(i, prefix, &server);
						send_all(i, line, &server);
						
						free(line); // Importante liberar la línea que nos devuelve extract_message
					}
				}
			}
			i++;
		}
	}
	return (0);
}
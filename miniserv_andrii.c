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
	char		*msg_r_parzial[9999];
	int			id_clientes[9999];
	long		current_id;  // pico maximo usuario de todo el proyecto

}				t_clients;

typedef struct	s_server
{
	t_clients	clients;
	fd_set		bkp_fds;
	fd_set		read_fds;
	fd_set		writefds;
	int			fd_socket; //connfd

}				t_server;

// =========================================================================


/*
Entonces, para que el enfoque manual funcione bien, el orden lógico tiene que ser:

Primero creas y limpias tu struct t_server server = {0};
Luego haces FD_ZERO(&server.bkp) (sobre el campo del struct, no una variable suelta)
Luego creas el socket con socket() y lo guardas directo en server.fd_server (no en una variable fd_server aparte)
Haces FD_SET(server.fd_server, &server.bkp) para que el propio socket del servidor entre en la lista maestra
Ahí sí, bind() y listen() sobre server.fd_server
Y entonces entras al while(1), donde en cada vuelta copias server.bkp a read_fds/writefds,
llamas a select(), y recorres con un for para ver qué fd está listo (y ahí es donde va el accept(),
dentro del bucle, no después) */
int main(int argn, char **argv)
{
	if (argn != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}

	t_server			server = {0};
	int					puerto = atoi(argv[1]);
	struct sockaddr_in	servaddr; // la structura para decir all kerner donde escuchar

	// socket create and verification 
	server.fd_socket = socket(AF_INET, SOCK_STREAM, 0); 
	if (server.fd_socket == -1)
		msg_err();

	bzero(&servaddr, sizeof(servaddr));

	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(puerto);

	// Binding (associar) newly created socket to given IP and verification
	if ((bind(server.fd_socket, (const struct sockaddr *)&servaddr, sizeof(servaddr))) == -1)
		msg_err();
	
	// desde ahora el servido se activa para eschuchar peticiones de clientes
	// El argumento *backlog* define la longitud máxima que puede alcanzar la cola de conexiones pendientes para *sockfd*.
	if (listen(server.fd_socket, 10) == -1)
		msg_err(); 

	FD_ZERO(&server.bkp_fds); // inizializa a cero ls lista de FDS!

	while (1)
	{
		// reset fds
		server.read_fds = server.bkp_fds;
		server.writefds = server.bkp_fds;
		
		int	fd_new_connect; //connfd

		fd_new_connect = accept(server.fd_socket, NULL, NULL);
		if (fd_new_connect == -1)
			continue ;
		else
		{
			server.clients.id_clientes[fd_new_connect] = ++server.clients.current_id; 
			FD_SET(fd_new_connect, &server.bkp_fds); //actuallizamos la structura de bkp_fd con  el nuvevo fd
		}
	}
}


//! structura del sistema para dejar reflejado el tipo de socket
/* 
struct sockaddr_in
{
    short          sin_family;   // familia de direcciones (IPv4, IPv6...)
    unsigned short sin_port;     // número de puerto
    struct in_addr sin_addr;     // dirección IP
    char           sin_zero[8];  // relleno, no se usa
};
*/

	// FD_SET(server->max_fd, &server->bkp); // bkp es un listado de todo los fd (server y clientes)
	// server->fd_server = server->max_fd; // poruqe?

int	accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen); // return a file descriptor for the accepted socket (a nonnegative integer).  On error, -1 is returned
int	select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
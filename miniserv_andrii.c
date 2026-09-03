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
// los indices de abajo de las variables abajo se basan en fd y no es un id
typedef struct	s_clients
{
	char		*msg_r_parzial[9999]; // array de buffers
	int			id_clientes[9999];
	long		current_id; // pico maximo usuario de todo el proyecto
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
		return (1);
	}

	int					puerto = atoi(argv[1]);
	t_server			server = {0};
	
	// INIZIALIZO PUERTO Y IP
	struct sockaddr_in	servaddr; // la structura para decir al kerner donde escuchar
	bzero(&servaddr, sizeof(servaddr));
	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(puerto);

	// CREO SOCKET
	server.fd_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server.fd_socket == -1)
		msg_err();

	// Binding (ASSOCIO el SOCKET con el "IP Y PUERTO" creados arriba)
	if ((bind(server.fd_socket, (const struct sockaddr *)&servaddr, sizeof(servaddr))) == -1)
		msg_err();

	// desde ahora el servidor se activa para eschuchar (listen) peticiones de clientes
	// El argumento <10> define la longitud máxima que puede alcanzar la cola de conexiones pendientes para <server.fd_socket>.
	if (listen(server.fd_socket, 10) == -1)
		msg_err();

	FD_ZERO(&server.bkp_fds); // inizializa a cero ls lista de FDS!
	FD_SET(server.fd_socket, &server.bkp_fds); // añadimos en bkp_fds el fd_socket del server

	while (1)
	{
		// reset fds
		server.read_fds = server.bkp_fds;
		server.writefds = server.bkp_fds;

		// select si bloquea hasta que reciba algun cambio (peticion de  cliente que activaremos con accept)
		if (select(FD_SETSIZE, &server.read_fds, &server.writefds, NULL, NULL) == -1)// se non si accende nessuno rimane in attesa che qualcuno si attivi
			continue ;

		int	fd_new_connect; // (connfd en el main), fd del nuevo cliente conectado
		
		// Si el server esta encendido entonces encendemos otro cliente
		// (porque arriba SELECT nos ha dicho que alguen esta pidiendo)
		if (FD_ISSET(server.fd_socket, &server.read_fds) == true) // solo stiamo verificando se il server ha richieste nueve!
		{
			// llamar a accept() si FD_ISSET(server.fd_socket, &read_fds)
			// acept acepta aqui una nueva coneccion
			fd_new_connect = accept(server.fd_socket, NULL, NULL); // return the new socket's descriptor (FD -> socket del cliente)
			if (fd_new_connect == -1)
				continue ; // deve termiinar el programa con error? o repetir el while?
			else
			{
				// ✅ El id del prime cliente ahora empieza en 0 y los siguentes tendra id+1 con el post-incremento.
				server.clients.id_clientes[fd_new_connect] = server.clients.current_id++;
				
				// ✅ Actualizas bkp_fds para que el nuevo cliente entre en el "maestro" de fds vigilados.
				FD_SET(fd_new_connect, &server.bkp_fds); //actuallizamos la structura de bkp_fd con el nuvevo fd

				char str[1024];
				int i = 3;

				while (i < FD_SETSIZE)
				{
					if (i == server.fd_socket)
						i++;

					if (FD_ISSET(i, &server.writefds) == true)
					{
						printf("caca %d\n", i);
						sprintf(str, "server: client %d just arrived\n", server.clients.id_clientes[fd_new_connect]);
						send(i, str, strlen(str), 0);
					}
					i++;
				}

				// AQUI ES DONDE HA LLEGADO EL CLIENTE
				// gestion mensajes en llegada
			}
		}

	// appunti	https://excalidraw.com/#json=HHRrs_nctM2TEhxb784A_,cig1CJIqkVnOX7ZRS67cvA

	}
}

/*

Message collapsed
✅ Hecho y sólido (~25-30%):

    Validación de argumentos
    socket + bind + listen con manejo de errores (msg_err)
    El patrón select() → FD_ISSET() → accept() correctamente entendido y aplicado — esto es conceptualmente la parte más difícil de entender del proyecto, aunque en líneas de código sea poco. Haberlo interiorizado bien (por qué hay que reinicializar los sets cada vuelta, qué es value-result, etc.) te va a ahorrar muchísimos dolores de cabeza en el resto.
    Asignación correcta de ids empezando en 1 (pre-incremento)
    Registro del cliente en bkp_fds
    Uso de htons/htonl confirmado como válido en tu enunciado real

⏳ Pendiente (~70-75%), en orden de aparición lógica:

    1 Broadcast de "just arrived" — en lo que estás ahora mismo. Mecánico una vez entiendes el patrón FD_ISSET+send.

    2 Bucle de recv sobre todos los clientes ya conectados (mismo patrón for + FD_ISSET que el de arriba, pero sobre read_fds para fds de clientes, no del servidor).

    3 Integrar str_join y extract_message — ya los tienes escritos arriba pero cero conectados al bucle principal todavía. Esta es la parte donde manejas mensajes parciales (un cliente puede mandar medio mensaje sin \n y el resto llega en el siguiente recv).

    4 Reenvío con prefijo "client %d: " a los demás clientes, línea por línea.

    5 Detectar desconexión (recv devuelve 0 o -1), hacer close(), FD_CLR(), liberar el buffer parcial de ese cliente, y broadcast de "just left".

    6 Gestión de memoria sin leaks en todo el ciclo de vida de un cliente (esto suele ser donde más se falla al final, con Valgrind). // liberra los buffers
*/

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

	int	select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
	int	accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen); // return a file descriptor for the accepted socket (a nonnegative integer).  On error, -1 is returned
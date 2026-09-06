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

void	msg_err(void)
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

// =========================================================================
// los indices de abajo de las variables se basan en fd, no en id
typedef struct	s_clients
{
	char		*msg_r_parzial[FD_SETSIZE]; // array de buffers, indexado por fd
	int			id_clientes[FD_SETSIZE];    // array de ids "bonitos", indexado por fd
	long		current_id;                  // contador que genera el siguiente id
}				t_clients;

typedef struct	s_server
{
	t_clients	clients;
	fd_set		bkp_fds;   // lista MAESTRA de fds vigilados (server + clientes)
	fd_set		read_fds;  // copia de trabajo, machacada por select() -> lectura
	fd_set		writefds;  // copia de trabajo, machacada por select() -> escritura
	int			fd_socket; // fd del socket de escucha del servidor
	int			max_fd;    // el fd MAS ALTO que tenemos registrado ahora mismo
}				t_server;

// =========================================================================

int	main(int argn, char **argv)
{
	if (argn != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		return (1);
	}

	int					puerto = atoi(argv[1]);
	t_server			server = {0}; // ⚠️ el "= {0}" es OBLIGATORIO, ver explicacion abajo

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

	// Binding
	if ((bind(server.fd_socket, (const struct sockaddr *)&servaddr, sizeof(servaddr))) == -1)
		msg_err();

	if (listen(server.fd_socket, 10) == -1)
		msg_err();

	FD_ZERO(&server.bkp_fds);
	FD_SET(server.fd_socket, &server.bkp_fds);

	// el fd del server es, de momento, el fd mas alto que conocemos
	server.max_fd = server.fd_socket;

	while (1)
	{
		// reset fds
		server.read_fds = server.bkp_fds;
		server.writefds = server.bkp_fds;

		// -----------------------------------------------------------------
		// 👉 LA LINEA IMPORTANTE: usamos max_fd + 1, NO un numero fijo
		// (ni 9999, ni FD_SETSIZE). select() necesita saber "hasta que
		// numero de fd tengo que mirar". Si le pones un numero mas grande
		// del que hace falta, select() puede leer/escribir memoria que no
		// le pertenece (esto lo comprobamos con valgrind: corrompia la
		// pila y hacia caer el programa con "Fatal error" sin motivo).
		// max_fd+1 es SIEMPRE el valor correcto: ni te quedas corto (no
		// vigilarias a algun cliente) ni te pasas (corrupcion de memoria).
		// -----------------------------------------------------------------
		if (select(server.max_fd + 1, &server.read_fds, &server.writefds, NULL, NULL) == -1)
			continue ;

		int	fd_new_connect;

		// =================================================================
		// BLOQUE 1: ¿hay una conexion NUEVA esperando en el socket del server?
		// =================================================================
		if (FD_ISSET(server.fd_socket, &server.read_fds) == true)
		{
			fd_new_connect = accept(server.fd_socket, NULL, NULL);
			if (fd_new_connect != -1)
			{
				server.clients.id_clientes[fd_new_connect] = server.clients.current_id++;
				FD_SET(fd_new_connect, &server.bkp_fds);

				// -------------------------------------------------------
				// 👉 aqui iba el ternario "fd > max_fd ? fd : max_fd".
				// Es exactamente lo mismo que este if, solo que escrito
				// en una sola linea. El ternario "condicion ? A : B" se
				// lee: "si la condicion es verdad, el valor es A, si no,
				// es B". Aqui: "si fd_new_connect es mayor que max_fd,
				// el nuevo max_fd es fd_new_connect; si no, se queda
				// igual". Lo dejamos como if normal, mas facil de leer:
				// -------------------------------------------------------
				if (fd_new_connect > server.max_fd)
					server.max_fd = fd_new_connect;

				char str[1024];
				int i = 3;

				while (i < FD_SETSIZE)
				{
					if (i == server.fd_socket)
						i++;

					if (FD_ISSET(i, &server.writefds) == true)
					{
						sprintf(str, "server: client %d just arrived\n",
							server.clients.id_clientes[fd_new_connect]);
						send(i, str, strlen(str), 0);
					}
					i++;
				}
			}
		}

		// =================================================================
		// BLOQUE 2: recorremos TODOS los clientes ya conectados (se ejecuta
		// SIEMPRE, haya llegado alguien nuevo o no) para ver si alguno nos
		// mando un mensaje que hay que leer.
		// =================================================================
		int j = 3;

		while (j < FD_SETSIZE)
		{
			if (j == server.fd_socket)
			{
				j++;
				continue ;
			}

			if (FD_ISSET(j, &server.read_fds) == true)
			{
				char buf[1024];
				int ret = recv(j, buf, sizeof(buf) - 1, 0);

				// =========================================================
				// BLOQUE 2A: el cliente se desconecto (ret == 0) o hubo
				// un error de lectura (ret == -1)
				// =========================================================
				if (ret <= 0)
				{
					int id_que_se_va = server.clients.id_clientes[j];

					close(j);
					FD_CLR(j, &server.bkp_fds);
					if (server.clients.msg_r_parzial[j] != 0)
					{
						free(server.clients.msg_r_parzial[j]);
						server.clients.msg_r_parzial[j] = 0;
					}

					char str[1024];
					int k = 3;

					while (k < FD_SETSIZE)
					{
						if (k == server.fd_socket || k == j)
						{
							k++;
							continue ;
						}
						if (FD_ISSET(k, &server.writefds) == true)
						{
							sprintf(str, "server: client %d just left\n", id_que_se_va);
							send(k, str, strlen(str), 0);
						}
						k++;
					}
				}
				// =========================================================
				// BLOQUE 2B: llegaron datos de verdad -> acumulamos y vamos
				// sacando (con extract_message) todas las lineas completas.
				// =========================================================
				else
				{
					buf[ret] = 0;

					server.clients.msg_r_parzial[j] = str_join(server.clients.msg_r_parzial[j], buf);
					if (server.clients.msg_r_parzial[j] == 0)
						msg_err();

					char *linea_completa;
					int hay_linea = extract_message(&server.clients.msg_r_parzial[j], &linea_completa);

					// puede haber MAS de una linea en un solo recv(),
					// por eso esto es un while, no un if
					while (hay_linea == 1)
					{
						char prefijo[32];
						char str_final[1024 + 32];

						sprintf(prefijo, "client %d: ", server.clients.id_clientes[j]);
						sprintf(str_final, "%s%s", prefijo, linea_completa);
						free(linea_completa);

						int m = 3;
						while (m < FD_SETSIZE)
						{
							if (m == server.fd_socket || m == j)
							{
								m++;
								continue ;
							}
							if (FD_ISSET(m, &server.writefds) == true)
								send(m, str_final, strlen(str_final), 0);
							m++;
						}

						hay_linea = extract_message(&server.clients.msg_r_parzial[j], &linea_completa);
					}
					if (hay_linea == -1)
						msg_err();
				}
			}
			j++;
		}
	}
}

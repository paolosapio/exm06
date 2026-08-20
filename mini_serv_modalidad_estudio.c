/* ========================================================================
 * MINI_SERV — servidor de chat TCP con select()
 * ========================================================================
 * IDEA GENERAL:
 *   - Un socket servidor escucha en 127.0.0.1:<puerto>.
 *   - Varios clientes se conectan (nc, telnet, etc).
 *   - Todo lo que un cliente escribe (línea por línea, delimitada por '\n')
 *     se reenvía (broadcast) a TODOS los demás clientes conectados,
 *     con el formato: "client <id>: <mensaje>".
 *   - Se avisa a todos cuando alguien entra o sale del chat.
 *   - Todo es NO-bloqueante gracias a select(): un único hilo, un único
 *     bucle, vigilando muchos fd a la vez.
 * ======================================================================== */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

/* ------------------------------------------------------------------------
 * ESTRUCTURA PRINCIPAL: guarda TODO el estado del servidor en un solo sitio
 * (evita variables globales, todo se pasa por puntero a las funciones).
 *
 * - rfds/wfds son SIEMPRE copias de all_fd_sock, porque select() destruye
 *   (sobreescribe) el fd_set que le pasas, dejando solo los fd listos.
 *   Si usáramos all_fd_sock directamente, perderíamos la lista maestra.
 *
 * - Los arrays usan FD_SETSIZE (típicamente 1024, el límite de select())
 *   porque se indexan directamente por el número de fd: msgs[fd], no hace
 *   falta buscar al cliente, el propio fd es la clave.
 * ------------------------------------------------------------------------ */
typedef struct s_miniserv
{
	int		max_fd;					// fd más alto vigilado ahora mismo (server + clientes)
	int		client_count;			// contador incremental -> da IDs únicos y legibles (0,1,2...)
	int		client_id[FD_SETSIZE];	// client_id[fd] = id "humano" de ese cliente
	char	*msgs[FD_SETSIZE];		// msgs[fd] = buffer acumulado, aún no forma línea completa
	fd_set	all_fd_sock;			// MAESTRO: todos los fd vivos (server + clientes)
	fd_set	rfds;					// copia de trabajo para select() - lectura
	fd_set	wfds;					// copia de trabajo para select() - escritura
	char	buf_read[1024];			// buffer temporal para recv()
	char	buf_write[1024];		// buffer temporal para construir mensajes a enviar
	int		socket_server;			// fd del socket que escucha (listen)
}				t_miniserv;

/* ========================================================================
 * FUNCIONES DADAS POR EL ENUNCIADO (copy-paste obligatorio, no se tocan)
 * ======================================================================== */

/*
 * extract_message: saca UNA línea completa (hasta el primer '\n' incluido)
 * del buffer acumulado *buf.
 *
 *   1) *msg = 0                -> por defecto, no hay mensaje.
 *   2) *buf == NULL             -> nada acumulado, devuelve 0.
 *   3) recorre *buf buscando '\n' en la posición i.
 *   4) si lo encuentra:
 *        - newbuf = lo que queda DESPUÉS del '\n' (se guarda para luego)
 *        - corta *buf justo tras el '\n' -> eso pasa a ser *msg
 *        - *buf = newbuf (el "sobrante" para la próxima llamada)
 *        - return 1  ("sí, extraje un mensaje completo")
 *   5) si no hay '\n' en todo el buffer -> return 0 ("aún incompleto")
 *
 * Se llama en un while() porque un solo recv() puede traer VARIAS líneas
 * de golpe ("hola\nadios\n"); hay que vaciarlas todas antes de volver a leer.
 *
 * return -1 sólo si calloc() falla (memoria agotada) -> en rigor debería
 * tratarse como fatal_error, aquí simplemente corta el while (no crashea,
 * pero es un punto que un evaluador estricto puede señalar).
 */
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

/*
 * str_join: concatena add al final de buf, LIBERA buf (ownership!) y
 * devuelve un puntero nuevo con el resultado.
 *
 *   1) len = strlen(buf) (0 si buf es NULL)
 *   2) reserva len + strlen(add) + 1 bytes
 *   3) copia buf (si existía) en el buffer nuevo
 *   4) free(buf)              <- consume el puntero que le diste
 *   5) le pega add al final
 *   6) devuelve el puntero nuevo
 *
 * Por qué es clave: TCP no garantiza mensajes "completos" ni "limpios" en
 * cada recv(), puede cortar en cualquier byte. str_join permite ir
 * acumulando en msgs[fd] todo lo que llega, trocito a trocito, hasta que
 * extract_message encuentre un '\n'.
 */
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

/* ========================================================================
 * FUNCIONES PROPIAS
 * ======================================================================== */

/*
 * fatal_error: para cuando algo que "no debería fallar nunca" (socket,
 * bind, listen, select) falla. Mensaje EXACTO pedido por el enunciado,
 * por stderr (fd 2). write() y no printf() porque es async-signal-safe
 * y no depende de buffering.
 */
void	fatal_error(void)
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

/*
 * notify_other: broadcast de str a TODOS los clientes conectados,
 * EXCEPTO el autor (fd_author) y el propio socket servidor.
 *
 *   - FD_ISSET(fd, &wfds)        -> ¿está listo para escribir? (según el
 *                                    último select(); casi siempre sí)
 *   - fd != fd_author            -> no te reenvías tu propio mensaje
 *   - fd != server->socket_server -> nunca send() sobre el socket de escucha
 *
 * Se usa en 3 sitios: cliente nuevo, cliente que se va, mensaje de chat.
 * Es el corazón del broadcast.
 */
void	notify_other(int fd_author, char *str, t_miniserv *server)
{
	int	fd;

	for (fd = 0; fd <= server->max_fd; fd++)
	{
		if (FD_ISSET(fd, &server->wfds) && fd != fd_author
			&& fd != server->socket_server)
			send(fd, str, strlen(str), 0);
	}
}

/*
 * register_client: se llama justo después de un accept() exitoso.
 *
 *   1) actualiza max_fd si hace falta -> CRÍTICO: select() usa max_fd+1,
 *      si no lo actualizas, select() nunca vigilará al cliente nuevo.
 *   2) asigna client_id único y legible (post-incremento de client_count)
 *   3) msgs[fd] = NULL -> ese fd pudo pertenecer antes a otro cliente ya
 *      desconectado; hay que arrancar limpio (str_join espera NULL, no basura)
 *   4) FD_SET en all_fd_sock -> a partir de ahora select() lo vigila
 *   5) broadcast: "server: client X just arrived\n"
 */
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

/*
 * remove_client: se llama cuando recv() devuelve <= 0 (conexión cerrada
 * por el cliente, o error de socket).
 *
 *   1) avisa a los demás ANTES de limpiar nada (necesita client_id[fd] vivo)
 *   2) free(msgs[fd]) -> libera lo que quedara a medio acumular (evita leak)
 *   3) FD_CLR en all_fd_sock -> select() deja de vigilarlo
 *   4) close(fd)
 *
 * Nota: no se baja max_fd aunque el cliente eliminado lo tuviera más alto.
 * No es un bug: sólo hace que el for() del main itere un poco de más sobre
 * fd ya no presentes (FD_ISSET los filtra). Ineficiente en el peor caso,
 * pero correcto.
 */
void	remove_client(int fd, t_miniserv *server)
{
	sprintf(server->buf_write, "server: client %d just left\n",
		server->client_id[fd]);
	notify_other(fd, server->buf_write, server);
	free(server->msgs[fd]);
	FD_CLR(fd, &server->all_fd_sock);
	close(fd);
}

/*
 * send_msg: se llama tras cada recv() + str_join() exitosos.
 *
 *   - en bucle, va sacando líneas completas de msgs[fd] con extract_message
 *   - por cada línea completa:
 *       * construye y envía el prefijo "client X: "
 *       * envía el contenido real del mensaje (msg, ya trae su '\n')
 *       * free(msg) -> extract_message te da ownership de esa memoria
 *   - el while termina cuando extract_message devuelve 0 (ya no queda
 *     ninguna línea completa, sólo un resto parcial que se queda guardado
 *     en msgs[fd] esperando más bytes)
 */
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

/*
 * create_socket: crea el socket servidor.
 *
 *   1) socket(AF_INET, SOCK_STREAM, 0) -> TCP / IPv4
 *   2) si falla (-1) -> fatal_error()
 *   3) como es el primer fd creado, se usa directo como max_fd inicial
 *   4) FD_SET en all_fd_sock -> el servidor también se vigila con select()
 *      (así detectamos conexiones entrantes)
 *   5) se guarda también en socket_server para identificarlo fácil luego
 */
void	create_socket(t_miniserv *server)
{
	server->max_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server->max_fd == -1)
		fatal_error();
	FD_SET(server->max_fd, &server->all_fd_sock);
	server->socket_server = server->max_fd;
}

/* ========================================================================
 * MAIN
 * ======================================================================== */
int	main(int ac, char **av)
{
	t_miniserv			server;
	struct sockaddr_in	servaddr;
	int					fd;
	int					client_fd;
	int					read_bytes;

	/* 1) Validación de argumentos: mensaje EXACTO pedido por el enunciado */
	if (ac != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}

	/* 2) all_fd_sock debe arrancar vacío: obligatorio antes de cualquier
	 *    FD_SET/FD_ISSET sobre él, si no -> comportamiento indefinido */
	FD_ZERO(&server.all_fd_sock);

	/* 3) socket() + registro en all_fd_sock */
	create_socket(&server);

	/* 4) Configuración de la dirección: 127.0.0.1 (localhost) */
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;					// IPv4
	servaddr.sin_addr.s_addr = htonl(2130706433);	// 127.0.0.1, en network byte order
	servaddr.sin_port = htons(atoi(av[1]));		// puerto, string->int->big endian

	/* 5) bind: asocia el socket a la dirección/puerto
	 *    listen: pone el socket en modo escucha (cola de pendientes) */
	if (bind(server.socket_server, (const struct sockaddr *)&servaddr,
			sizeof(servaddr)))
		fatal_error();
	if (listen(server.socket_server, SOMAXCONN))
		fatal_error();

	/* 6) Bucle principal: un único select() vigila TODO */
	while (1)
	{
		/* select() destruye/modifica los fd_set que recibe, dejando sólo
		 * los fd listos -> hay que refrescar rfds/wfds desde el maestro
		 * en CADA vuelta */
		server.rfds = server.all_fd_sock;
		server.wfds = server.all_fd_sock;

		/* bloquea hasta que al menos un fd esté listo para leer o escribir.
		 * max_fd + 1 porque select() pide "cuántos fd vigilar" y los fd
		 * empiezan en 0. NULL en exceptfds (no nos interesan excepciones)
		 * y NULL en timeout (bloqueo indefinido) */
		if (select(server.max_fd + 1, &server.rfds, &server.wfds,
				NULL, NULL) == -1)
			fatal_error();

		/* empieza en 3: 0/1/2 son stdin/stdout/stderr, nunca están en
		 * nuestro fd_set */
		for (fd = 3; fd <= server.max_fd; fd++)
		{
			if (!FD_ISSET(fd, &server.rfds))
				continue ;

			if (fd == server.socket_server)
			{
				/* conexión entrante pendiente -> accept() da un fd nuevo
				 * y propio para ese cliente */
				client_fd = accept(server.socket_server, NULL, NULL);
				if (client_fd >= 0)
				{
					register_client(client_fd, &server);
					/* IMPORTANTE: break, no continue.
					 * Acabamos de modificar all_fd_sock y max_fd; los
					 * datos de rfds/wfds de esta vuelta ya no son fiables
					 * -> volvemos a select() con el estado actualizado */
					break ;
				}
			}
			else
			{
				read_bytes = recv(fd, server.buf_read, 1000, 0);
				if (read_bytes <= 0)
				{
					/* 0 = el cliente cerró la conexión, -1 = error */
					remove_client(fd, &server);
					/* IMPORTANTE: break, mismo motivo que en accept().
					 * remove_client() cierra un fd que Linux puede
					 * RECICLAR en el próximo accept(); si seguimos
					 * iterando en esta misma vuelta podríamos leer/
					 * escribir sobre la conexión equivocada */
					break ;
				}
				server.buf_read[read_bytes] = '\0';	// recv() no añade '\0'
				server.msgs[fd] = str_join(server.msgs[fd], server.buf_read);
				send_msg(fd, &server);
			}
		}
	}
	return (0);
}

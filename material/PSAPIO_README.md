# Chuleta oral — mini_serv (select)

## Preguntas rápidas y respuesta corta

**¿Por qué `rfds = all_fd_sock` en cada vuelta del while?**
Porque `select()` sobreescribe el `fd_set` que le pasas, dejando solo los fd listos. Si no lo refrescas desde el maestro, pierdes la lista completa de fd vigilados.

**¿Por qué `break` y no `continue` tras `accept()` o `remove_client()`?**
Porque acabas de modificar `all_fd_sock` (y a veces `max_fd`). Los datos de `rfds`/`wfds` de esa vuelta ya no son fiables. Además, Linux **recicla** números de fd: el próximo `accept()` puede devolver el mismo fd que acabas de cerrar. Si sigues iterando sin `break`, podrías tratar ese fd reciclado como si fuera la conexión anterior.

**¿Por qué `max_fd + 1` en `select()`?**
`select()` necesita saber cuántos fd vigilar, no cuál es el más alto. Como los fd empiezan en 0, para cubrir hasta `max_fd` inclusive hace falta `max_fd + 1`.

**¿Por qué el `for` empieza en 3?**
Los fd 0, 1, 2 son stdin, stdout, stderr — nunca están en `all_fd_sock`.

**¿Qué pasa si `recv()` devuelve 0? ¿Y -1?**
0 = el cliente cerró la conexión ordenadamente (FIN). -1 = error de socket. En este proyecto ambos casos se tratan igual: se llama a `remove_client`.

**¿Por qué `msgs[fd] = NULL` en `register_client`?**
Porque ese número de fd pudo pertenecer antes a otro cliente ya desconectado. Sin resetear, `str_join`/`extract_message` trabajarían sobre un puntero colgante (dangling) o basura.

**¿Por qué `str_join` hace `free(buf)` internamente?**
Porque "consume" el puntero que recibe: reserva un buffer nuevo, copia lo viejo + lo nuevo, y libera el viejo. El llamador solo debe quedarse con el valor de retorno.

**¿Por qué `extract_message` se llama en un `while`?**
Porque un solo `recv()` puede traer varias líneas completas de golpe (ej. el cliente mandó dos mensajes seguidos antes de que tú leyeras). Hay que vaciarlas todas antes de volver a `recv()`.

**¿Qué significa que `extract_message` devuelva 0 / 1 / -1?**
- `0`: no hay ninguna línea completa todavía (no hay `\n` en el buffer).
- `1`: se extrajo una línea completa, queda en `*msg`.
- `-1`: fallo de `calloc` (memoria agotada).

**¿Por qué en `notify_other` excluyes `socket_server`?**
Porque no es una conexión de cliente, es el socket de escucha. Hacer `send()` sobre él no tiene sentido y es un error de socket.

**¿Por qué usas `FD_ISSET(fd, &wfds)` en `notify_other` en vez de mandar directo?**
Porque el enunciado exige comprobar que el fd está listo para escritura según el último `select()`, aunque en la práctica un socket casi siempre está listo para escribir salvo que su buffer de envío esté lleno.

**¿Por qué `127.0.0.1` como IP fija?**
El proyecto exige que el servidor escuche solo en localhost (no en todas las interfaces de red), por eso se usa `INADDR_LOOPBACK` (2130706433 en decimal = 127.0.0.1).

**¿Por qué `htonl`/`htons`?**
Las structs de red (`sockaddr_in`) esperan los valores en *network byte order* (big-endian), independientemente de la arquitectura de la máquina. `htonl`/`htons` (*host to network long/short*) hacen esa conversión.

**¿Por qué `write()` y no `printf()` en `fatal_error`?**
`write()` es una syscall directa, sin buffering, async-signal-safe. `printf()` puede quedarse en buffer y no imprimir nada si el proceso muere abruptamente (`exit`).

**¿Por qué no bajas `max_fd` en `remove_client`?**
No es obligatorio: solo genera que el `for` del main itere un poco de más sobre fd ya no presentes en el set (los filtra `FD_ISSET`). Es una ineficiencia menor, no un bug.

**¿Qué pasaría si no usaras `select()` y usaras `recv()` bloqueante directo?**
El servidor se quedaría bloqueado esperando datos de UN cliente y no podría atender a los demás ni aceptar nuevas conexiones mientras tanto. `select()` permite multiplexar: vigilar muchos fd a la vez con un solo hilo.

## Flujo de datos de un mensaje (para dibujar si te lo piden)

```
cliente escribe "hola\n"
        │
        ▼
   recv(fd, buf_read)        <- llegan bytes crudos, quizá parciales
        │
        ▼
   str_join(msgs[fd], buf_read)   <- se acumulan en el buffer del cliente
        │
        ▼
   send_msg(fd)
        │
        ▼
   extract_message(&msgs[fd], &msg)  <- saca líneas completas (con '\n')
        │
        ▼
   notify_other(fd, "client X: ")   <- broadcast del prefijo
   notify_other(fd, msg)            <- broadcast del mensaje
        │
        ▼
   todos los demás clientes reciben "client X: hola"
```

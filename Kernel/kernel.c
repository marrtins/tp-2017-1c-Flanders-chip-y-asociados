#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include "../Compartidas/pcb.h"

//#include "../Compartidas/funcionesPaquetes.h"
//#include "../Compartidas/funcionesCompartidas.h"
#include "../Compartidas/funcionesPaquetes.c"
#include "../Compartidas/funcionesCompartidas.c"

#include "../Compartidas/tiposErrores.h"
#include "../Compartidas/tiposPaquetes.h"

#include "kernelConfigurators.h"
#include "auxiliaresKernel.h"

#define BACKLOG 20

#define MAX_IP_LEN 16   // aaa.bbb.ccc.ddd -> son 15 caracteres, 16 contando un '\0'
#define MAX_PORT_LEN 6  // 65535 -> 5 digitos, 6 contando un '\0'
#define MAXMSJ 100

/* MAX(A, B) es un macro que devuelve el mayor entre dos argumentos,
 * lo usamos para actualizar el maximo socket existente, a medida que se crean otros nuevos
 */
#define MAX(A, B) ((A) > (B) ? (A) : (B))



int main(int argc, char* argv[]){
	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	int sem, stat, ready_fds;
	int fd, new_fd;
	int fd_max = -1;
	int sock_fs, sock_mem;
	int sock_lis_cpu, sock_lis_con;

	// Creamos e inicializamos los conjuntos que retendran sockets para el select()
	fd_set read_fd, master_fd;
	FD_ZERO(&read_fd);
	FD_ZERO(&master_fd);

	tKernel *kernel = getConfigKernel(argv[1]);
	mostrarConfiguracion(kernel);

	// Se trata de conectar con Memoria
	if ((sock_mem = establecerConexion(kernel->ip_memoria, kernel->puerto_memoria)) < 0){
		printf("Error fatal! No se pudo conectar con la Memoria! sock_mem: %d\n", sock_mem);
		return FALLO_CONEXION;
	}

	// No permitimos continuar la ejecucion hasta lograr un handshake con Memoria
	while ((sem = handshakeCon(sock_mem, kernel->tipo_de_proceso)) < 0)
		;
	printf("Se enviaron: %d bytes a MEMORIA\n", sem);

	fd_max = MAX(sock_mem, fd_max);

	// Se trata de conectar con Filesystem
	if ((sock_fs  = establecerConexion(kernel->ip_fs, kernel->puerto_fs)) < 0){
		printf("Error fatal! No se pudo conectar con el Filesystem! sock_fs: %d\n", sock_fs);
		return FALLO_CONEXION;
	}

	// No permitimos continuar la ejecucion hasta lograr un handshake con Filesystem
	while ((sem = handshakeCon(sock_fs, kernel->tipo_de_proceso)) < 0)
		;
	printf("Se enviaron: %d bytes a FILESYSTEM\n", sem);

	fd_max = MAX(sock_fs, fd_max);

	// Creamos sockets para hacer listen() de CPUs
	if ((sock_lis_cpu = makeListenSock(kernel->puerto_cpu)) < 0){
		printf("No se pudo crear socket para escuchar! sock_lis_cpu: %d\n", sock_lis_cpu);
		return FALLO_CONEXION;
	}

	fd_max = MAX(sock_lis_cpu, fd_max);

	// Creamos sockets para hacer listen() de Consolas
	if ((sock_lis_con = makeListenSock(kernel->puerto_prog)) < 0){
		printf("No se pudo crear socket para escuchar! sock_lis_con: %d\n", sock_lis_con);
		return FALLO_CONEXION;
	}

	fd_max = MAX(sock_lis_con, fd_max);

	// fd_max ahora va a tener el valor mas alto de los sockets hasta ahora hechos
	// la implementacion sigue siendo ineficiente.. a futuro se va a hacer algo mas power!

	// Se agregan memoria, fs, listen_cpu, listen_consola y stdin al set master
	FD_SET(sock_mem, &master_fd);
	FD_SET(sock_fs, &master_fd);
	FD_SET(sock_lis_cpu, &master_fd);
	FD_SET(sock_lis_con, &master_fd);
	FD_SET(0, &master_fd);


	listen(sock_lis_cpu, BACKLOG);
	listen(sock_lis_con, BACKLOG);


	tPackHeader *header_tmp  = malloc(HEAD_SIZE); // para almacenar cada recv
	while (1){

		read_fd = master_fd;

		ready_fds = select(fd_max + 1, &read_fd, NULL, NULL, NULL);
		if(ready_fds == -1)
			return FALLO_SELECT;

		for (fd = 0; fd <= fd_max; ++fd){
		if (FD_ISSET(fd, &read_fd)){

			printf("Aca hay uno! el fd es: %d\n", fd);

			// Controlamos el listen de CPU o de Consola
			if (fd == sock_lis_con || fd == sock_lis_cpu){

				new_fd = handleNewListened(fd, &master_fd);
				if (new_fd < 0){
					perror("Fallo en manejar un listen. error");
					return FALLO_CONEXION;
				}

				fd_max = MAX(new_fd, fd_max);
				break;
			}

			// Como no es un listen, recibimos el header de lo que llego
			if ((stat = recv(fd, header_tmp, HEAD_SIZE, 0)) == -1){
				perror("Error en recv() de algun socket. error");
				break;

			} else if (stat == 0){
				printf("Se desconecto el socket %d\nLo sacamos del set listen...\n", fd);
				clearAndClose(&fd, &master_fd);
				break;
			}

			// Se recibio un header sin conflictos, procedemos con el flujo
			if (header_tmp->tipo_de_mensaje == SRC_CODE){

				puts("Se recibio un paquete de codigo fuente.\nReenviamos a Memoria...");

				uint32_t cant_pags = kernel->stack_size; // TODO falta sumar la cantidad de pags que requiere el codigo recibido

				pcb PCB = nuevoPcb(cant_pags);

				recibirCodYReenviar(header_tmp, fd, sock_mem);

				puts("Listo!");
				break;
			}

			if (fd == sock_mem){
				printf("Llego algo desde memoria!\n\tTipo de mensaje: %d\n", header_tmp->tipo_de_mensaje);
				break;

			} else if (fd == sock_fs){
				printf("llego algo desde fs!\n\tTipo de mensaje: %d\n", header_tmp->tipo_de_mensaje);
				break;

			} else if (fd == 0){ //socket del stdin
				printf("Ingreso texto por pantalla!\nCerramos Kernel!\n");
				goto limpieza; // nos vamos del ciclo infinito...
			}

			if (header_tmp->tipo_de_proceso == CON){
				printf("Llego algo desde Consola!\n\tTipo de mensaje: %d\n", header_tmp->tipo_de_mensaje);
				break;
			}

			if (header_tmp->tipo_de_proceso == CPU){
				printf("Llego algo desde CPU!\n\tTipo de mensaje: %d\n", header_tmp->tipo_de_mensaje);
				break;
			}

			puts("Si esta linea se imprime, es porque el header_tmp tiene algun valor rarito");

		}} // aca terminan el for() y el if(FD_ISSET)
	}



limpieza:
	// Un poco mas de limpieza antes de cerrar

	free(header_tmp);

	FD_ZERO(&read_fd);
	FD_ZERO(&master_fd);
	close(sock_mem);
	close(sock_fs);
	close(sock_lis_con);
	close(sock_lis_cpu);

	liberarConfiguracionKernel(kernel);
	return stat;
}


char* serializarOperandos(t_PackageEnvio *package){

	char *serializedPackage = malloc(package->total_size);
	int offset = 0;
	int size_to_send;


	size_to_send =  sizeof(package->tipo_de_proceso);
	memcpy(serializedPackage + offset, &(package->tipo_de_proceso), size_to_send);
	offset += size_to_send;


	size_to_send =  sizeof(package->tipo_de_mensaje);
	memcpy(serializedPackage + offset, &(package->tipo_de_mensaje), size_to_send);
	offset += size_to_send;

	size_to_send =  sizeof(package->message_size);
	memcpy(serializedPackage + offset, &(package->message_size), size_to_send);
	offset += size_to_send;

	size_to_send =  package->message_size;
	memcpy(serializedPackage + offset, package->message, size_to_send);

	return serializedPackage;
}

int recieve_and_deserialize(t_PackageRecepcion *packageRecepcion, int socketCliente){

	int status;
	int buffer_size;
	char *buffer = malloc(buffer_size = sizeof(uint32_t));
	clearBuffer(buffer,buffer_size);

	uint32_t tipo_de_proceso;
	status = recv(socketCliente, buffer, sizeof(packageRecepcion->tipo_de_proceso), 0);
	memcpy(&(tipo_de_proceso), buffer, buffer_size);
	if (status < 0) return FALLO_RECV;

	uint32_t tipo_de_mensaje;
	status = recv(socketCliente, buffer, sizeof(packageRecepcion->tipo_de_mensaje), 0);
	memcpy(&(tipo_de_mensaje), buffer, buffer_size);
	if (status < 0) return FALLO_RECV;


	uint32_t message_size;
	status = recv(socketCliente, buffer, sizeof(packageRecepcion->message_size), 0);
	memcpy(&(message_size), buffer, buffer_size);
	if (status < 0) return FALLO_RECV;

	status = recv(socketCliente, packageRecepcion->message, message_size, 0);
	if (status < 0) return FALLO_RECV;



	//TIPODEMENSAJE=2 significa reenviar el paquete a memoria

	if(tipo_de_mensaje == 2 ){
		printf("\nNos llego un paquete para reenviar a memoria\n");
		printf("Reenviando..\n");
		t_PackageEnvio packageEnvio;
		packageEnvio.tipo_de_proceso = 1;

		//

		packageEnvio.tipo_de_mensaje = tipo_de_mensaje;
		packageEnvio.message = malloc(message_size);
		packageEnvio.message_size = message_size;
		packageEnvio.total_size = sizeof(packageEnvio.tipo_de_mensaje)+sizeof(packageEnvio.tipo_de_proceso)+sizeof(packageEnvio.message_size)+packageEnvio.message_size;
		strcpy(packageEnvio.message, packageRecepcion->message);
		char *serializedPackage;
		serializedPackage = serializarOperandos(&packageEnvio);
		int enviar;
		enviar = send(3,serializedPackage,packageEnvio.total_size,0);
		printf("%d Enviado\n", enviar);




	}

	free(buffer);
	return status;
}






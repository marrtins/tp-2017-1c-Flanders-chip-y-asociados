#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <netdb.h>
#include <commons/config.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write
#include <errno.h>
#include <semaphore.h>

#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <funcionesPaquetes/funcionesPaquetes.h>
#include <funcionesCompartidas/funcionesCompartidas.h>

#include "apiMemoria.h"
#include "manejadoresMem.h"
#include "manejadoresCache.h"
#include "memoriaConfigurators.h"
#include "structsMem.h"
#include "flujosMemoria.h"

#define BACKLOG 20

void* kernel_handler(void *sock_ker);
void* cpu_handler(void *sock_cpu);

tMemoria *memoria;          // configuracion del proceso Memoria
char *MEM_FIS;              // Memoria Fisica
char *CACHE;                // memoria CACHE
tCacheEntrada *CACHE_lines; // vector de lineas a CACHE
int  *CACHE_accs;           // vector de accesos hechos a CACHE

sem_t mem_access; // todo: cambiar por pthread_mutex

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	int stat;

	sem_init(&mem_access, 0, 1);

	memoria = getConfigMemoria(argv[1]);
	mostrarConfiguracion(memoria);

	if ((stat = setupMemoria()) != 0)
		return ABORTO_MEMORIA;

	pthread_t kern_thread;
	bool kernExists = false;
	int sock_entrada , client_sock , clientSize;

	struct sockaddr_in client;
	clientSize = sizeof client;

	if ((sock_entrada = makeListenSock(memoria->puerto_entrada)) < 0){
		fprintf(stderr, "No se pudo crear un socket de listen. fallo: %d", sock_entrada);
		return FALLO_GRAL;
	}
	//Listen
	if ((stat = listen(sock_entrada , BACKLOG)) == -1){
		perror("No se pudo hacer listen del socket. error");
		return FALLO_GRAL;
	}

	//acepta y escucha comunicaciones
	tPackHeader head;
	puts("esperando comunicaciones entrantes...");
	while((client_sock = accept(sock_entrada, (struct sockaddr*) &client, (socklen_t*) &clientSize)) != -1){
		puts("Conexion aceptada");

		if ((stat = recv(client_sock, &head, HEAD_SIZE, 0)) < 0){
			perror("Error en la recepcion de handshake. error");
			return FALLO_RECV;
		}

		switch(head.tipo_de_proceso){

		case KER:

			if (!kernExists){
				int *sock_ker = malloc(sizeof(int));
				*sock_ker     = client_sock;
				kernExists    = true;

				if ((stat = contestarMemoriaKernel(memoria->marco_size, memoria->marcos, *sock_ker)) == -1){
					puts("No se pudo enviar la informacion relevante a Kernel!");
					return FALLO_GRAL;
				}

				if( pthread_create(&kern_thread, NULL, (void*) kernel_handler, (void*) sock_ker) < 0){
					perror("No pudo crear hilo. error");
					return FALLO_GRAL;
				}

			} else
				fprintf(stderr, "Se trato de conectar otro Kernel. Ignoramos el paquete...\n");

			break;

		case CPU:
			puts("Ingresa CPU");

			pthread_t cpu_thread;
			int *sock_cpu = malloc(sizeof(int));
			*sock_cpu    = client_sock;

			head.tipo_de_proceso = MEM; head.tipo_de_mensaje = MEMINFO;
			if ((stat = contestarProcAProc(head, memoria->marco_size, *sock_cpu)) == -1){
				puts("No se pudo enviar la informacion relevante a Kernel!");
				return FALLO_GRAL;
			}

			if( pthread_create(&cpu_thread, NULL, (void*) cpu_handler, (void*) sock_cpu) < 0){
				perror("no pudo crear hilo. error");
				return FALLO_GRAL;
			}
			break;

		default:
			puts("Trato de conectarse algo que no era ni Kernel ni CPU!");
			printf("El tipo de proceso y mensaje son: %d y %d\n", head.tipo_de_proceso, head.tipo_de_mensaje);
			printf("Se recibio esto del socket: %d\n", client_sock);
			return CONEX_INVAL;
		}
	}

	// Si salio del ciclo es porque fallo el accept()
	perror("Fallo el accept(). error");

	liberarConfiguracionMemoria();
	return 0;
}


/* dado el socket de Kernel, maneja las acciones que de este reciba
 */
void* kernel_handler(void *sock_kernel){

	int *sock_ker = (int *) sock_kernel;
	int stat, new_page, pack_size;
	int pid;
	char *buffer;

	tPackHeader head = {.tipo_de_proceso = KER, .tipo_de_mensaje = THREAD_INIT};
	tPackPidPag *pp;
	tPackVal val;

	printf("Esperamos que lleguen cosas del socket Kernel: %d\n", *sock_ker);

	do {

		switch(head.tipo_de_mensaje){
		case INI_PROG:
			puts("Kernel quiere inicializar un programa.");

			if ((buffer = recvGeneric(*sock_ker)) == NULL)
				break;

			if ((pp = deserializePIDPaginas(buffer)) == NULL)
				break;
			freeAndNULL((void **) buffer);

			sem_wait(&mem_access);
			stat = inicializarPrograma(pp->pid, pp->pageCount);
			sem_post(&mem_access);

			freeAndNULL((void **) pp);
			if (stat != 0)
				puts("No se pudo inicializar el programa");

			puts("Fin case INI_PROG.");
			break;

		case BYTES:
			puts("Kernel quiere Solicitar Bytes");

			sem_wait(&mem_access);
			stat = manejarSolicitudBytes(*sock_ker);
			sem_post(&mem_access);

			if (stat != 0){
				printf("Fallo el manejo de la Solicitud de Bytes. status: %d\n", stat);
				head.tipo_de_proceso = MEM; head.tipo_de_mensaje = stat;
				informarFallo(*sock_ker, head);

			} else
				puts("Se completo Solicitud de Bytes");

			break;

		case ALMAC_BYTES:
			puts("Kernel quiere almacenar bytes");

			sem_wait(&mem_access);
			stat = manejarAlmacenamientoBytes(*sock_ker);
			sem_post(&mem_access);

			if (stat != 0)
				printf("Fallo el manejo de la Almacenamiento de Bytes. status: %d\n", stat);

			puts("Fin case ALMAC_BYTES.");
			break;

		case ASIGN_PAG:
			puts("Kernel quiere asignar paginas!");

			if ((buffer = recvGeneric(*sock_ker)) == NULL)
				break;

			if ((pp = deserializePIDPaginas(buffer)) == NULL)
				break;
			freeAndNULL((void **) &buffer);

			sem_wait(&mem_access);
			new_page = asignarPaginas(pp->pid, pp->pageCount);
			sem_post(&mem_access);

			pp->head.tipo_de_mensaje = (new_page < 0)? FALLO_ASIGN : ASIGN_SUCCS;
			pp->head.tipo_de_proceso = MEM;
			pp->pageCount = new_page;
			pack_size = 0;
			buffer = serializePIDPaginas(pp, &pack_size);

			if ((stat = send(*sock_ker, buffer, pack_size, 0)) == -1){
				perror("Fallo send de pagina asignada a Kernel. error");
				break;
			}

			freeAndNULL((void **) &pp);
			freeAndNULL((void **) &buffer);
			puts("Fin case ASIGN_PAG.");
			break;

		case FIN_PROG:

			recv(*sock_ker, &pid, sizeof pid, 0);
			// TODO: desalojarDatosPrograma(pid)

			break;

		case FIN:
			// se quiere desconectar el Kernel de forma normal. Vamos a apagarnos aca...
			// TODO: limpiarProcesamientosDeThreadsYTodasLasCosasAllocatedDeMemoria(void *cualquierCosa);
			break;

		default:
			break;
		}
	} while((stat = recv(*sock_ker, &head, HEAD_SIZE, 0)) > 0);

	if (stat == -1){
		perror("Se perdio conexion con Kernel. error");
		return NULL;
	}

	//se desconecto Kernel de forma normal. Vamos a apagarnos aca...
	//todo: limpiarProcesamientosDeThreadsYTodasLasCosasAllocatedDeMemoria(void *cualquierCosa);

	return NULL;
}


/* dado un socket de CPU, maneja las acciones que de estos reciba
 */
void* cpu_handler(void *socket_cpu){

	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = THREAD_INIT};
	int stat;
	int *sock_cpu = (int*) socket_cpu;

	printf("Esperamos que lleguen cosas del socket CPU: %d\n", *sock_cpu);

	do {
		printf("proc: %d  \t msj: %d\n", head.tipo_de_proceso, head.tipo_de_mensaje);

		switch(head.tipo_de_mensaje){
		case BYTES:
			puts("CPU quiere Solicitar Bytes");
			sem_wait(&mem_access);

			if ((stat = manejarSolicitudBytes(*sock_cpu)) != 0)
				fprintf(stderr, "Fallo el manejo de la Solicitud de Byes. status: %d\n", stat);

			sem_post(&mem_access);
			puts("Se completo Solicitud de Bytes");
			break;

		case ALMAC_BYTES:
			puts("CPU quiere Almacenar bytes CPU");
			sem_wait(&mem_access);

			if ((stat = manejarAlmacenamientoBytes(*sock_cpu)) != 0)
				fprintf(stderr, "Fallo el manejo de la Almacenamiento de Byes. status: %d\n", stat);

			sem_post(&mem_access);
			puts("Se completo Peticion de Almacenamiento CPU");
			break;

		case INSTR:
			puts("Se recibio pedido de instrucciones");
			sem_wait(&mem_access);

			if ((stat = manejarSolicitudBytes(*sock_cpu)) != 0)
				fprintf(stderr, "Fallo el manejo de la Solicitud de Bytes. status: %d\n", stat);

			sem_post(&mem_access);
			break;

		default:
			puts("Se recibio un mensaje no considerado");
			break;
		}
	} while((stat = recv(*sock_cpu, &head, HEAD_SIZE, 0)) > 0);

	if (stat == -1){
		perror("Fallo el recv de un mensaje desde CPU. error");
		return NULL;
	}

	printf("El CPU de socket %d cerro su conexion. Cerramos el thread\n", *sock_cpu);
	return NULL;
}

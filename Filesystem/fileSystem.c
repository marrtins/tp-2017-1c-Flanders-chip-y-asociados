#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <pthread.h>

#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <commons/bitarray.h>

#include <fuse.h>

#include <funcionesCompartidas/funcionesCompartidas.h>
#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>

#include "fileSystemConfigurators.h"
#include "operacionesFS.h"
#include "manejadorSadica.h"

#ifndef BACKLOG
#define BACKLOG 20
#endif

enum {
	KEY_VERSION,
	KEY_HELP,
};

static struct fuse_opt fuse_options[] = {
		// Este es un parametro definido por nosotros
		//CUSTOM_FUSE_OPT_KEY("--welcome-msg %s", welcome_msg, 0),

		// Estos son parametros por defecto que ya tiene FUSE
		FUSE_OPT_KEY("-V", KEY_VERSION),
		FUSE_OPT_KEY("--version", KEY_VERSION),
		FUSE_OPT_KEY("-h", KEY_HELP),
		FUSE_OPT_KEY("--help", KEY_HELP),
		FUSE_OPT_END,
};

int *ker_manejador(void);
int recibirConexionKernel(void);

int sock_kern;
extern char *rutaBitMap;
extern tMetadata* meta;
tFileSystem* fileSystem;

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	int stat, fuseret, *retval;
	pthread_t kern_th;

	fileSystem = getConfigFS(argv[1]);
	mostrarConfiguracion(fileSystem);

	setupFuseOperations();

	char* argumentos[] = {"", fileSystem->punto_montaje, ""};
	struct fuse_args args = FUSE_ARGS_INIT(3, argumentos);

	// Limpio la estructura que va a contener los parametros
	memset(&runtime_options, 0, sizeof(struct t_runtime_options));

	// Esta funcion de FUSE lee los parametros recibidos y los intepreta
	if (fuse_opt_parse(&args, &runtime_options, fuse_options, NULL) == -1){
		perror("Invalid arguments!");
		return EXIT_FAILURE;
	}

	//bitmap
	crearDirMontaje();
	if ((fuseret = fuse_main(args.argc, args.argv, &oper, NULL)) == -1){
		perror("Error al operar fuse_main. error");
		return FALLO_GRAL;
	}

	crearDirectoriosBase();
	if (inicializarMetadata() != 0){
		puts("No se pudo levantar el Metadata del Filesystem!");
		return ABORTO_FILESYSTEM;
	}
	if (inicializarBitmap() != 0){
		puts("No se pudo levantar el Bitmap del Filesystem!");
		return ABORTO_FILESYSTEM;
	}

	if ((stat = recibirConexionKernel()) < 0){
		puts("No se pudo conectar con Kernel!");
		//todo: limpiarFilesystem();
	}

	// todo: en vez del ker, habria que combinarlo con el manejador del manejadorSadica.c
	pthread_create(&kern_th, NULL, (void *) ker_manejador, &retval);
	pthread_join(kern_th, (void **) &retval);

	close(sock_kern);
	liberarConfiguracionFileSystem(fileSystem);
	return fuseret; // en todos los ejemplos que vi se retorna el valor del fuse_main..
}


int *ker_manejador(void){

	int stat;
	int *retval = malloc(sizeof(int));
	tPackHeader head = {.tipo_de_proceso = KER,.tipo_de_mensaje = 9852};//9852, iniciar escuchaKernel
	tPackHeader header;
	char * buffer;
	tPackBytes * abrir;
	tPackRecibirRW * leer;
	tPackRecibirRW * escribir;
	tPackBytes * borrar;
	struct fuse_file_info* fi;
	int operacion;
	header.tipo_de_proceso = FS;
	do {
	switch(head.tipo_de_mensaje){

	case VALIDAR_ARCHIVO:
		puts("Se pide validacion de archivo");
		buffer = recvGeneric(sock_kern);
		abrir = deserializeBytes(buffer);
		if((operacion = validarArchivo(abrir->bytes)) == 0){
			puts("El archivo fue validado");
			header.tipo_de_mensaje = VALIDAR_RESPUESTA;
			informarResultado(sock_kern,header);
		}else{
			puts("El archivo no fue validado, debe crearlo");
			header.tipo_de_mensaje = INVALIDAR_RESPUESTA;
			informarResultado(sock_kern,header);
		}
		freeAndNULL((void **)&buffer);
		freeAndNULL((void **)&abrir);
		puts("Fin case VALIDAR_ARCHIVO");
		break;

	case CREAR_ARCHIVO:
		puts("Se pide crear un archivo");
		buffer = recvGeneric(sock_kern);
		abrir = deserializeBytes(buffer);
		if(true){//todo: deberia ser crearArchivo, pero crearArchivo es void.
			puts("El archivo fue abierto con exito");
			header.tipo_de_mensaje = CREAR_ARCHIVO;
			informarResultado(sock_kern,header);
		}else{
			puts("El archivo no pudo ser abierto");
			header.tipo_de_mensaje = INVALIDAR_RESPUESTA;
			informarResultado(sock_kern,header);
		}
		freeAndNULL((void **)&buffer);
		puts("Fin case CREAR_ARCHIVO");
		break;

	case BORRAR:
		puts("Se peticiona el borrado de un archivo");
		buffer = recvGeneric(sock_kern);
		borrar = deserializeBytes(buffer);
		if((operacion = unlink2(borrar->bytes)) == 0){
			puts("El archivo fue borrado con exito");
			header.tipo_de_mensaje = ARCHIVO_BORRADO;
			informarResultado(sock_kern,header);
		}else{
			puts("El archivo no pudo ser borrado");
			header.tipo_de_mensaje = INVALIDAR_RESPUESTA;
			informarResultado(sock_kern,header);
		}
		puts("Fin case BORRAR");
		freeAndNULL((void **)&buffer);
		break;

	case LEER:
		puts("Se peticiona la lectura de un archivo");
		buffer = recvGeneric(sock_kern);
		leer = deserializeLeerFS(buffer);
		if(true){//(operacion = read2(leer->direccion,)) == 0
			puts("El archivo fue borrado con exito");
			header.tipo_de_mensaje = ARCHIVO_LEIDO;
			informarResultado(sock_kern,header);
		}else{
			puts("El archivo no pudo ser borrado");
			header.tipo_de_mensaje = INVALIDAR_RESPUESTA;
			informarResultado(sock_kern,header);
		}
		puts("Fin case LEER");
		freeAndNULL((void **)&buffer);
		break;

	case ESCRIBIR:
		puts("Se peticiona la escritura de un archivo");
		buffer = recvGeneric(sock_kern);
		escribir = deserializeLeerFS(buffer);
		if(true){//(operacion = write2(escribir->direccion)) == 0
			puts("El archivo fue borrado con exito");
			header.tipo_de_mensaje = ARCHIVO_ESCRITO;
			informarResultado(sock_kern,header);
		}else{
			puts("El archivo no pudo ser borrado");
			header.tipo_de_mensaje = INVALIDAR_RESPUESTA;
			informarResultado(sock_kern,header);
		}
		puts("Fin case ESCRIBIR");
		freeAndNULL((void **)&buffer);
		break;

	default:
		puts("Se recibio un mensaje no manejado!");
		printf("Proc %d, Mensaje %d\n", head.tipo_de_proceso, head.tipo_de_mensaje);
		break;

	}} while((stat = recv(sock_kern, &head, HEAD_SIZE, 0)) > 0);

	if (stat == -1){
		perror("Fallo recepcion de Kernel. error");
		*retval = FALLO_RECV;
		return retval;
	}

	puts("Kernel cerro la conexion");
	*retval = FIN;
	return retval;
}

int recibirConexionKernel(void){

	int sock_lis_kern;
	tPackHeader head, h_esp;
	if ((sock_lis_kern = makeListenSock(fileSystem->puerto_entrada)) < 0){
		printf("No se pudo crear socket listen en puerto: %s\n", fileSystem->puerto_entrada);
		return FALLO_GRAL;
	}

	if(listen(sock_lis_kern, BACKLOG) == -1){
		perror("Fallo de listen sobre socket Kernel. error");
		return FALLO_GRAL;
	}

	h_esp.tipo_de_proceso = KER; h_esp.tipo_de_mensaje = HSHAKE;
	while (1){
		if((sock_kern = makeCommSock(sock_lis_kern)) < 0){
			puts("No se pudo acceptar conexion entrante del Kernel");
			return FALLO_GRAL;
		}

		if (validarRespuesta(sock_kern, h_esp, &head) != 0){
			printf("Rechazo proc %d mensaje %d\n", h_esp.tipo_de_proceso, h_esp.tipo_de_mensaje);
			close(sock_kern);
			continue;
		}
		printf("Se establecio conexion con Kernel. Socket %d\n", sock_kern);
		break;
	}

	return 0;
}

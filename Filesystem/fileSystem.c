#include <stdio.h>
#include <stdlib.h>
#include "fileSystem.h"
#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <string.h>
#include <netdb.h>

int main(int argc, char* argv[]){
	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	int sock_kern;

	tFileSystem* fileSystem = getConfigFS(argv[1]);
	mostrarConfiguracion(fileSystem);

	printf("Conectando con kernel...\n");
	sock_kern = establecerConexion(fileSystem->ip_kernel, fileSystem->puerto);
	printf("socket de kernel es: %d\n",sock_kern);

	liberarConfiguracionFileSystem(fileSystem);
	return EXIT_SUCCESS;
}

tFileSystem* getConfigFS(char* ruta){
	printf("Ruta del archivo de configuracion: %s\n", ruta);
	tFileSystem *fileSystem= malloc(sizeof(tFileSystem));

	fileSystem->puerto = malloc(MAX_PORT_LEN * sizeof(fileSystem->puerto));
	fileSystem->punto_montaje = malloc(sizeof(fileSystem->punto_montaje));
	fileSystem->ip_kernel = malloc(MAX_IP_LEN * sizeof(fileSystem->ip_kernel));

	t_config *fileSystemConfig = config_create(ruta);

	strcpy(fileSystem->puerto, config_get_string_value(fileSystemConfig, "PUERTO"));
	strcpy(fileSystem->punto_montaje, config_get_string_value(fileSystemConfig, "PUNTO_MONTAJE"));
	strcpy(fileSystem->ip_kernel, config_get_string_value(fileSystemConfig, "IP_KERNEL"));

	config_destroy(fileSystemConfig);
	return fileSystem;
}

void mostrarConfiguracion(tFileSystem *fileSystem){

	printf("Puerto: %s\n", fileSystem->puerto);
	printf("Punto de montaje: %s\n", fileSystem->punto_montaje);
	printf("IP del kernel: %s\n", fileSystem->ip_kernel);

}

void liberarConfiguracionFileSystem(tFileSystem *fileSystem){

	free(fileSystem->puerto);
	free(fileSystem->punto_montaje);
	free(fileSystem->ip_kernel);
	free(fileSystem);
}

void setupHints(struct addrinfo *hint, int fam, int sockType, int flags){

	memset(hint, 0, sizeof *hint);
	hint->ai_family = fam;
	hint->ai_socktype = sockType;
	hint->ai_flags = flags;
}

int establecerConexion(char *ip_dest, char *port_dest){

	char * msj = "Hola soy File System";

	int stat, bytes_sent;
	int sock_dest; // file descriptor para el socket del destino a conectar
	struct addrinfo hints, *destInfo;

	setupHints(&hints, AF_UNSPEC, SOCK_STREAM, 0);

	if ((stat = getaddrinfo(ip_dest, port_dest, &hints, &destInfo)) != 0){
		fprintf("getaddrinfo: %s\n", gai_strerror(stat));
		return FALLO_GRAL;
	}

	if ((sock_dest = socket(destInfo->ai_family, destInfo->ai_socktype, destInfo->ai_protocol)) < 0)
		return FALLO_GRAL;

	connect(sock_dest, destInfo->ai_addr, destInfo->ai_addrlen);
	freeaddrinfo(destInfo);

	bytes_sent = send(sock_dest, msj, strlen(msj), 0);
	printf("Se enviaron: %d bytes\n", bytes_sent);


	return sock_dest;
}



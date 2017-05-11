/*
 Estos includes los saque de la guia Beej... puede que ni siquiera los precisemos,
 pero los dejo aca para futuro, si algo mas complejo no anda tal vez sirvan...

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#include "../Compartidas/funcionesCompartidas.c"
#include "../Compartidas/tiposPaquetes.h"
#include "../Compartidas/tiposErrores.h"
#include "consolaConfigurators.h"

#define MAXMSJ 100

/* Con este macro verificamos igualdad de strings;
 * es mas expresivo que strcmp porque devuelve true|false mas humanamente
 */
#define STR_EQ(BUF, CC) (!strcmp((BUF),(CC)))

void Iniciar_Programa();
void Finalizar_Programa(int process_id);
void Desconectar_Consola();
void Limpiar_Mensajes();
void enviarArchivo(FILE*, uint32_t, uint32_t);

//void readPackage(t_PackageEnvio*, tConsola*, char*);

tPackSrcCode *readFileIntoPack(tProceso sender, char* ruta);
unsigned long fsize(FILE* f);

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	char *buf = malloc(MAXMSJ);
	int stat;
	int sock_kern;

	tConsola *cons_data = getConfigConsola(argv[1]);
	mostrarConfiguracionConsola(cons_data);

	printf("Conectando con kernel...\n");
	sock_kern = establecerConexion(cons_data->ip_kernel, cons_data->puerto_kernel);
	if (sock_kern < 0){
		errno = FALLO_CONEXION;
		perror("No se pudo establecer conexion con Kernel. error");
		return sock_kern;
	}


	puts("Creando codigo fuente...");
	tPackSrcCode *src_code = readFileIntoPack(cons_data->tipo_de_proceso, "/home/utnso/git/tp-2017-1c-Flanders-chip-y-asociados/CPU/facil.ansisop");

	puts("codigo:");
	puts(src_code->sourceCode);

	puts("Enviando codigo fuente...");
	unsigned long packSize = sizeof src_code->head + sizeof src_code->sourceLen + src_code->sourceLen;
	if ((stat = send(sock_kern, src_code, packSize, 0)) < 0){
		perror("No se pudo enviar codigo fuente a Kernel. error");
		return FALLO_SEND;
	}

	printf("Se envio el paquete de codigo fuente...");
	// enviamos el codigo fuente, lo liberamos ahora antes de olvidarnos..
//	free(src_code);

	while(!(STR_EQ(buf, "terminar\n")) && (stat != -1)){

		printf("Ingrese su mensaje: (esto no realiza ninguna accion en realidad)\n");
		fgets(buf, MAXMSJ, stdin);
		buf[MAXMSJ -1] = '\0';

//		stat = send(sock_kern, buf, MAXMSJ, 0);

		clearBuffer(buf, MAXMSJ);
	}


	printf("Cerrando comunicacion y limpiando proceso...\n");

	close(sock_kern);
	liberarConfiguracionConsola(cons_data);
	return 0;
}


// De momento no necesitamos esta para enviar codigo fuente..
// puede que sea necesaria para enviar otras cosas. La dejo comentada
//char* serializarOperandos(t_PackageEnvio *package){
//
//	char *serializedPackage = malloc(package->total_size);
//	int offset = 0;
//	int size_to_send;
//
//
//	size_to_send =  sizeof(package->tipo_de_proceso);
//	memcpy(serializedPackage + offset, &(package->tipo_de_proceso), size_to_send);
//	offset += size_to_send;
//
//
//	size_to_send =  sizeof(package->tipo_de_mensaje);
//	memcpy(serializedPackage + offset, &(package->tipo_de_mensaje), size_to_send);
//	offset += size_to_send;
//
//	size_to_send =  sizeof(package->message_size);
//	memcpy(serializedPackage + offset, &(package->message_size), size_to_send);
//	offset += size_to_send;
//
//	size_to_send =  package->message_size;
//	memcpy(serializedPackage + offset, package->message, size_to_send);
//
//	return serializedPackage;
//}
int recieve_and_deserialize(t_PackageRecepcion *package, int socketCliente){

	int status;
	int buffer_size;
	char *buffer = malloc(buffer_size = sizeof(uint32_t));
	clearBuffer(buffer,buffer_size);

	uint32_t tipo_de_proceso;
	status = recv(socketCliente, buffer, sizeof(package->tipo_de_proceso), 0);
	memcpy(&(tipo_de_proceso), buffer, buffer_size);
	if (!status) return 0;

	uint32_t tipo_de_mensaje;
	status = recv(socketCliente, buffer, sizeof(package->tipo_de_mensaje), 0);
	memcpy(&(tipo_de_mensaje), buffer, buffer_size);
	if (!status) return 0;


	uint32_t message_size;
	status = recv(socketCliente, buffer, sizeof(package->message_size), 0);
	memcpy(&(message_size), buffer, buffer_size);
	if (!status) return 0;

	status = recv(socketCliente, package->message, message_size, 0);
	if (!status) return 0;

	printf("%d %d %s",tipo_de_proceso,tipo_de_mensaje,package->message);

	free(buffer);

	return status;
}

/* Dado un archivo, lo lee e inserta en un paquete apto para enviarse
 */
tPackSrcCode *readFileIntoPack(tProceso sender, char* ruta){
// todo: en la estructura tPackSrcCode podriamos aprovechar mejor el espacio. Sin embargo, funciona bien

	FILE *file = fopen(ruta, "rb");
	tPackSrcCode *src_code = malloc(sizeof *src_code);

	src_code->head.tipo_de_proceso = sender;
	src_code->head.tipo_de_mensaje = SRC_CODE;

	unsigned long fileSize = fsize(file);

	src_code = realloc(src_code, sizeof src_code->head + sizeof src_code->sourceLen + fileSize);
	src_code->sourceLen = fileSize;
	src_code->sourceCode = (char *) (uint32_t) src_code + sizeof src_code->head + sizeof src_code->sourceLen;

	// todo: por algun motivo misterioso, si no hacemos alguna asignacion arbitraria al sourceCode, su escritura a memoria falla...
	*src_code->sourceCode = 0;

	fread(src_code->sourceCode, src_code->sourceLen, 1, file);
	fclose(file);

	return src_code;
}

unsigned long fsize(FILE* f){

    fseek(f, 0, SEEK_END);
    unsigned long len = (unsigned long) ftell(f);
    fseek(f, 0, SEEK_SET);

    return len;
}

//
//void readPackage(t_PackageEnvio* package, tConsola* cons_data, char* ruta){
//	package->tipo_de_proceso = cons_data->tipo_de_proceso;
//	package->tipo_de_mensaje = 2;
//	FILE *f = fopen(ruta, "rb");
//	fseek(f, 0, SEEK_END);
//	long fsize = ftell(f);
//	fseek(f, 0, SEEK_SET);  //same as rewind(f);
//
//	char *string = malloc(fsize + 1);
//	package->message = malloc(fsize);
//	fread(string, fsize, 1, f);
//	fclose(f);
//	string[fsize] = 0;
//
//	strcpy(package->message,string);
//
//	package->message_size = strlen(package->message)+1;
//	package->total_size = sizeof(package->tipo_de_proceso)+sizeof(package->tipo_de_mensaje)+sizeof(package->message_size)+package->message_size+sizeof(package->total_size);
//
//}


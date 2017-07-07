#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <commons/collections/list.h>
#include <commons/log.h>

#include <funcionesPaquetes/funcionesPaquetes.h>
#include <funcionesCompartidas/funcionesCompartidas.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/tiposErrores.h>

#include "consolaConfigurators.h"
#include "apiConsola.h"

#define MAXMSJ 100




/*Agarra los ultimos char de un string (para separar la ruta en la instruccion Nuevo programa <ruta>)
 */
void strncopylast(char *,char *,int );


/* Con este macro verificamos igualdad de strings;
 * es mas expresivo que strcmp porque devuelve true|false mas humanamente
 */
#define STR_EQ(BUF, CC) (!strcmp((BUF),(CC)))

t_list *listaAtributos;

tConsola *cons_data;

t_log *logger;
int sock_kern;

int main(int argc, char* argv[]){


	//CREOARCHIVOLOGGER!

	logger = log_create("/home/utnso/logConsola.txt","CONSOLA",1,LOG_LEVEL_INFO);


	log_info(logger,"Inicia nueva ejecucion de CONSOLA");


	if(argc!=2){
		log_error(logger,"Error en la cantidad de parametros");
		//printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	//Creo lista de programas para ir agregando a medida q vayan iniciandose.

	listaAtributos = list_create();

	cons_data = getConfigConsola(argv[1]);
	mostrarConfiguracionConsola(cons_data);


	//TODO:No habria q hacer handsakhe aca en lugar de hacerlo cuando iniciamos un programa?

	printf("\n \n \nIngrese accion a realizar:\n");
	printf ("Para iniciar un programa: 'nuevo programa <ruta>'\n");
	printf ("Para finlizar un programa: 'finalizar <PID>'\n");
	printf ("Para desconectar consola: 'desconectar'\n");
	printf ("Para limpiar mensajes: 'limpiar'\n");
	int finalizar = 0;
	while(finalizar !=1){
		printf("Seleccione opcion: \n");
		char *opcion=malloc(MAXMSJ);
		fgets(opcion,MAXMSJ,stdin);
		opcion[strlen(opcion) - 1] = '\0';
		if (strncmp(opcion,"nuevo programa",14)==0)
		{
			log_trace(logger,"nuevo programa");
			//printf("Iniciar un nuevo programa\n");
			char *ruta = opcion+15;

			//TODO:Chequear error de ruta..
			tAtributosProg *atributos = malloc(sizeof *atributos);
			atributos->sock = sock_kern;
			atributos->path = ruta;

			//printf("Ruta del programa: %s\n",atributos->path);
			log_info(logger,ruta);

			int status = Iniciar_Programa(atributos);
			if(status<0){
				log_error(logger,"error al iniciar programa");
				//puts("Error al iniciar programa");
				//TODO: Crear FALLO_INICIARPROGRAMA
				return -1000;
			}

		}
		if(strncmp(opcion,"finalizar",9)==0)
		{
			log_trace(logger,"finalizar programa");
			char* pidString=malloc(MAXMSJ);
			int longitudPid = strlen(opcion) - 10;
			strncopylast(opcion,pidString,longitudPid);
			int pidElegido = atoi(pidString);
			//printf("Eligio finalizar el programa %d\n",pidElegido);
			log_info(logger,pidString);

			//TODO: Buscar de la lista de hilos cual sería el q corresponde al pid q queremos matar
			int status = Finalizar_Programa(pidElegido);
		}
		if(strncmp(opcion,"desconectar",11)==0){
			log_trace(logger,"desconectar consola");
			Desconectar_Consola(cons_data);
			finalizar = 1;

			close(sock_kern);
			liberarConfiguracionConsola(cons_data);
		}
		if(strncmp(opcion,"limpiar",7)==0){
			log_trace(logger,"limpiar pantalla");
			Limpiar_Mensajes();
			//printf("Eligio limpiar esta consola \n");
		}

	}




	/*tPathYSock *args = malloc(sizeof *args);
	args->sock = sock_kern;
	args->path = "/home/utnso/git/tp-2017-1c-Flanders-chip-y-asociados/CPU/facil.ansisop";
	int status = Iniciar_Programa(args);

	printf("El satus es: %d\n",status);

	while(!(STR_EQ(buf, "terminar\n")) && (stat != -1)){

		printf("Ingrese su mensaje: (esto no realiza ninguna accion en realidad)\n");
		fgets(buf, MAXMSJ, stdin);
		buf[MAXMSJ -1] = '\0';

//		stat = send(sock_kern, buf, MAXMSJ, 0);

		clearBuffer(buf, MAXMSJ);
	}
*/

	log_trace(logger,"cerrando comunicacion y limpiando proceso");
	//printf("Cerrando comunicacion y limpiando proceso...\n");


	log_destroy(logger);

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

	log_trace(logger,"mensaje deserializacdo y recibido");
	log_info(logger,package->message);
	//printf("%d %d %s",tipo_de_proceso,tipo_de_mensaje,package->message);

	free(buffer);

	return status;
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


//Agarra los ultimos char de un string (para separar la ruta en la instruccion Nuevo programa <ruta>)


void strncopylast(char *str1,char *str2,int n)
{   int i;
    int l=strlen(str1);
    if(n>l)
    {
       log_error(logger,"Can't extract more characters from a smaller string.");
    	//printf("\nCan't extract more characters from a smaller string.\n");
        exit(1);
    }
    for(i=0;i<l-n;i++)
         str1++;
    for(i=l-n;i<l;i++)
    {
        *str2=*str1;
         str1++;
         str2++;
    }
    *str2='\0';
}




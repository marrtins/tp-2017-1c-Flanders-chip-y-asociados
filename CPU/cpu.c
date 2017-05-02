#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <commons/config.h>

#include "../Compartidas/funcionesCompartidas.c"
#include "../Compartidas/tiposErrores.h"
#include "cpuConfigurators.h"
#include "pcb.h"

#define MAX_IP_LEN 16 // Este largo solo es util para IPv4
#define MAX_PORT_LEN 6

#define MAXMSJ 100 // largo maximo de mensajes a enviar. Solo utilizado para 1er checkpoint

void ejecutarPrograma(pcb*);
void ejecutarAsignacion();
void obtenerDireccion(variable*);
uint32_t obtenerValor(uint32_t, uint32_t, uint32_t);
void almacenar(uint32_t,uint32_t,uint32_t,uint32_t);
uint32_t getPagDelIndiceStack(uint32_t);
uint32_t getOffsetDelIndiceStack(uint32_t);
uint32_t getSizeDelIndiceStack(uint32_t);

int sock_mem; // SE PASA A VAR GLOBAL POR AHORA

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	char *buf = malloc(MAXMSJ);
	int stat;
	int sock_kern;
	int bytes_sent;

	tCPU *cpu_data = getConfigCPU(argv[1]);
	mostrarConfiguracionCPU(cpu_data);


	strcpy(buf, "Hola soy CPU");

		// CONECTAR CON MEMORIA...
		printf("Conectando con memoria...\n");
		sock_mem = establecerConexion(cpu_data->ip_memoria, cpu_data->puerto_memoria);
		if (sock_mem < 0)
			return sock_mem;
		//paso estructura dinamica
		t_Package package;
		package.tipo_de_proceso = cpu_data->tipo_de_proceso;
		package.tipo_de_mensaje = 2;
		package.message = buf;
		package.message_long = strlen(package.message)+1;
		package.total_size = sizeof(package.tipo_de_proceso)+sizeof(package.tipo_de_mensaje)+sizeof(package.message_long)+package.message_long+sizeof(package.total_size);

		char *serializedPackage;
		serializedPackage = serializarOperandos(&package);

		send(sock_mem, serializedPackage, package.total_size, 0);





		//fin envio estructura dinamica




		stat = recv(sock_mem, buf, MAXMSJ, 0);
		printf("%s\n", buf);
		clearBuffer(buf, MAXMSJ);

		if (stat < 0){
			printf("Error en la conexion con Memoria! status: %d \n", stat);
			return FALLO_CONEXION;
		}

		printf("socket de memoria es: %d\n",sock_mem);

	pcb* pcbDePrueba = malloc(sizeof(pcb));
	pcbDePrueba->id = 1;
	pcbDePrueba->pc = 1;
	ejecutarPrograma(pcbDePrueba);


	printf("Conectando con kernel...\n");
	sock_kern = establecerConexion(cpu_data->ip_kernel, cpu_data->puerto_kernel);
	if (sock_kern < 0)
		return sock_kern;


	while((stat = recv(sock_kern, buf, MAXMSJ, 0)) > 0){
		printf("%s\n", buf);

		clearBuffer(buf, MAXMSJ);
	}


	if (stat < 0){
		printf("Error en la conexion con Kernel! status: %d \n", stat);
		return FALLO_CONEXION;
	}


	printf("Kernel termino la conexion\nLimpiando proceso...\n");
	close(sock_kern);
	close(sock_mem);
	liberarConfiguracionCPU(cpu_data);
	return 0;
}


void ejecutarPrograma(pcb* pcb){
	++pcb->pc;
	ejecutarAsignacion();

}

void ejecutarAsignacion(){
	variable* var = malloc(sizeof(variable));
	obtenerDireccion(var);
	/*
	 * POR AHORA SE HARDCODEA PARA PROBAR UN EJEMPLO DE UNA SUMA SIMPLE
	 */

	var->valor = obtenerValor(var->pag, var->offset, var->size);
	variable* varRes = malloc(sizeof(variable));
	obtenerDireccion(varRes);
	varRes->valor = var->valor+3;
	printf("El resultado de la suma es: %d\n", varRes->valor);
	almacenar(varRes->pag, varRes->offset, varRes->size, varRes->valor);

}

void obtenerDireccion(variable* var){
	var->pag = getPagDelIndiceStack(var->id);
	var->offset = getOffsetDelIndiceStack(var->id);
	var->size = getSizeDelIndiceStack(var->id);
}
uint32_t obtenerValor(uint32_t pag, uint32_t offset, uint32_t size){
	/*char* buf = malloc(MAXMSJ);
	char* recibido = malloc(MAXMSJ);
	sprintf(buf, "Pido valor de variable, pag: %d, offset:%d, size: %d", pag, offset, size);
	send(sock_mem, buf, strlen(buf), 0);
	recv(sock_mem, recibido, MAXMSJ, 0);
	int valor = atoi(recibido);
	return valor;
	*/
	/*
	 * HABRIA QUE DEFINIR EL PASO DE MENSAJES ENTRE MEMORIA Y CPU
	 * PARA ESTE EJEMPLO SE VA A PASAR UN VALOR PARA PROBAR QUE FUNCIONE
	 */
	return 5;
}

void almacenar(uint32_t pag, uint32_t off, uint32_t size, uint32_t valor){
	char* buf = malloc(MAXMSJ);
	sprintf(buf, "Envio valor de variable, pag: %d, offset: %d, size: %d, valor: %d", pag, off, size, valor);
	send(sock_mem, buf, strlen(buf), 0);
}
/*
 * ESTAS FUNCIONES QUE VIENEN SE HACEN PARA QUE FUNCIONE, CREO QUE LE TIENE QUE
 * PEDIR A MEMORIA ESTOS VALORES
 *
 */
uint32_t getPagDelIndiceStack(uint32_t id){
	if(id==1) return 1;
	return 2;
}

uint32_t getOffsetDelIndiceStack(uint32_t id){
	if(id==1) return 1;
	return 2;
}

uint32_t getSizeDelIndiceStack(uint32_t id){
	if(id==1) return 1;
	return 2;
}
char* serializarOperandos(t_Package *package){

	char *serializedPackage = malloc(package->total_size);
	int offset = 0;
	int size_to_send;


	size_to_send =  sizeof(package->tipo_de_proceso);
	memcpy(serializedPackage + offset, &(package->tipo_de_proceso), size_to_send);
	offset += size_to_send;


	size_to_send =  sizeof(package->tipo_de_mensaje);
	memcpy(serializedPackage + offset, &(package->tipo_de_mensaje), size_to_send);
	offset += size_to_send;

	size_to_send =  sizeof(package->message_long);
	memcpy(serializedPackage + offset, &(package->message_long), size_to_send);
	offset += size_to_send;

	size_to_send =  package->message_long;
	memcpy(serializedPackage + offset, package->message, size_to_send);

	return serializedPackage;
}





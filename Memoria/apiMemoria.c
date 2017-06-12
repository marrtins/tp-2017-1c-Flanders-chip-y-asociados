#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <commons/log.h>
#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>

#include "manejadoresMem.h"
#include "manejadoresCache.h"
#include "apiMemoria.h"
#include "structsMem.h"
#include "auxiliaresMemoria.h"

#ifndef PID_MEM // es para la distinguir la Memoria de un PID cualquiera
#define PID_MEM 0
#endif

float retardo_mem; // latencia de acceso a Memoria Fisica
extern tMemoria *memoria;
extern tCacheEntrada *CACHE_lines;
extern t_log * logger;

// OPERACIONES DE LA MEMORIA

void retardo(int ms){
	retardo_mem = ms / 1000.0;
	log_info(logger,"Se cambio la latencia de acceso a Memoria a %f segundos", retardo_mem);

}

void flush(void){

	int i;
	for (i = 0; i < memoria->entradas_cache; ++i){
		(CACHE_lines +i)->pid  = 0;
		(CACHE_lines +i)->page = 0;
	}
}

void dump(int pid){


	log_info(logger,"Comienzo de DUMP");

	dumpCache();
	dumpMemStructs();
	dumpMemContent(pid);

	log_info(logger,"Fin del DUMP");

}


void size(int pid, int *proc_size, int *mem_frs, int *mem_ocup, int *mem_free){

	if (pid < 0){
		log_error(logger, "Se intento pedir el tamanio de un proceso invalido");

		*proc_size = *mem_frs = *mem_ocup = *mem_free = MEM_EXCEPTION;
	}

	// inicializamos todas las variables en 0, luego distinguimos si pide tamanio de Memoria o de un proc
	*proc_size = *mem_frs = *mem_ocup = *mem_free = 0;

	if (pid == PID_MEM){
		*mem_frs  = memoria->marcos; // TODO:
		*mem_free = pageQuantity(PID_MEM);
		*mem_ocup = *mem_frs - *mem_free;

	} else {
		*proc_size = pageQuantity(pid);
	}
}




// API DE LA MEMORIA

int inicializarPrograma(int pid, int pageCount){

	int reservadas = reservarPaginas(pid, pageCount);
	if (reservadas == pageCount)
		log_info(logger,"Se reservo bien la cantidad de paginas solicitadas");


	return 0;
}

int almacenarBytes(int pid, int page, int offset, int size, char *buffer){

	int stat;

	if ((stat = escribirBytes(pid, page, offset, size, buffer)) < 0){
		log_error(logger,"No se pudieron escribri los bytes a la pagina. Stat: %d",stat);

		abortar(pid);
		return FALLO_ESCRITURA;
	}

	return 0;
}

char *solicitarBytes(int pid, int page, int offset, int size){
	log_info(logger,"Se solicitan para el PID %d: %d bytes de la pagina %d",pid,size,page);


	char *buffer;
	if ((buffer = leerBytes(pid, page, offset, size)) == NULL){
		log_error("No se pudieron leer los bytes de la pagina");

		abortar(pid);
		return NULL;
	}

	return buffer;
}

int asignarPaginas(int pid, int page_count){

	int stat;

	if((stat = reservarPaginas(pid, page_count)) != 0){
		log_error(logger,"No se pudieron reservar paginas para el proceso: %d",stat);

		abortar(pid);
	}
	log_info(logger,"Se reservaron correctametne %d paginas", page_count);

	return 0;
}

/* Llamado por Kernel, libera una pagina de HEAP.
 * Retorna MEM_EXCEPTION si no puede liberar la pagina porque no existe o
 * porque simplemente no puede hacerse
 */
void liberarPagina(int pid, int page){
	log_info(logger,"Se libera la pagina %d del PID %d",page,pid);

}


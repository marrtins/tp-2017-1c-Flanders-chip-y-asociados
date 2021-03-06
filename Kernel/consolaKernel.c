#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAXOPCION 200
#define MAXPID_DIG 6

#include <commons/config.h>
#include <tiposRecursos/tiposErrores.h>

#include "defsKernel.h"
#include "auxiliaresKernel.h"
#include "kernelConfigurators.h"
#include "planificador.h"
#include "consolaKernel.h"
#include "commons/log.h"

extern tKernel *kernel;
extern t_queue *New, *Ready, *Exit;
extern t_list *Exec, *Block;
extern int grado_mult;
extern t_log * logTrace;

extern pthread_mutex_t mux_new, mux_ready, mux_exec, mux_block, mux_exit, mux_infoProc, mux_listaFinalizados, mux_gradoMultiprog;

extern t_dictionary *tablaGlobal;
extern t_list *tablaProcesos, *list_infoProc, *finalizadosPorConsolas;

bool planificacionBloqueada;

pthread_mutex_t mux_planificacionBloqueada;

void setupGlobales_consolaKernel(void){
	log_trace(logTrace,"setup globales cons kernel");
	pthread_mutex_init(&mux_planificacionBloqueada, NULL);
}

void showOpciones(void){

	printf("\n \n \nIngrese accion a realizar:\n");
	printf ("1-Para obtener los procesos en todas las colas o 1 especifica: 'procesos <cola>/<todos>'\n");
	printf ("2-Para ver info de un proceso determinado: 'info <PID>'\n");
	printf ("3-Para obtener la tabla global de archivos: 'tabla'\n");
	printf ("4-Para modificar el grado de multiprogramacion: 'nuevoGrado <GRADO>'\n");
	printf ("5-Para finalizar un proceso: 'finalizar <PID>'\n");
	printf ("6-Para detener la planificacion: 'stop'\n");
	printf ("7-Para estado de semaforos: 'sems'\n");

}

void consolaKernel(void){

	char opcion[MAXOPCION];
	int finalizar = 0;
	showOpciones();
	while(finalizar != 1){
		printf("Seleccione opcion: \n");
		fgets(opcion, MAXOPCION, stdin);
		opcion[strlen(opcion) - 1] = '\0';

		if (strncmp(opcion, "\0", 1) == 0){
			showOpciones();
			continue;
		}

		if (strncmp(opcion, "sems", 4) == 0){
			mostrarSemaforos(kernel);
			continue;
		}

		if (strncmp(opcion, "globales", 8) == 0){
			mostrarGlobales(kernel);
			continue;
		}

		if (strncmp(opcion, "procesos", 8) == 0){
			puts("Opcion procesos");
			log_trace(logTrace,"opcion procesos seleccionada");
			char *cola = opcion+9;
			mostrarColaDe(cola);
			continue;

		}
		if (strncmp(opcion, "stop", 4) == 0){
			puts("Opcion stop");
			log_trace(logTrace,"opcion stop seleccionada");

			stopKernel();
			continue;
		}
		if (strncmp(opcion,"tabla",5)==0){
			puts("Opcion tabla");
			log_trace(logTrace,"opcion tabla seleccionada");

			mostrarTablaGlobal();
			continue;
		}
		if (strncmp(opcion,"nuevoGrado",10)==0){
			puts("Opcion nuevoGrado");
			log_trace(logTrace,"opcion nuevo grado seleccionada");

			char *grado = opcion+11;
			int nuevoGrado = atoi(grado);
			cambiarGradoMultiprogramacion(nuevoGrado);
			continue;
		}
		if (strncmp(opcion,"info",4)==0){
			puts("Opcion info");
			log_trace(logTrace,"opcion info seleccionada");

			char *pidInfo=opcion+5;
			int pidElegido = atoi(pidInfo);
			mostrarInfoDe(pidElegido);
			continue;
		}

		if(!planificacionBloqueada){

			if (strncmp(opcion,"finalizar",9)==0){
				puts("Opcion finalizar");
				log_trace(logTrace,"opcion finalizar seleccionada");

				char *pidAFinalizar = opcion+9;
				int pidFin = atoi(pidAFinalizar);
				finalizarProceso(pidFin);
				continue;
			}
		}else{
			puts("No se puede acceder a este comando con la planificacion bloqueada :)");
		}
	}
}

void mostrarColaDe(char* cola){

	if(!planificacionBloqueada){
		puts("Muestro estado de las colas con la planificacion no bloqueada");
		log_trace(logTrace,"mostrar la(s) cola(s) con la planificaicon no bloqueada");

		if (strncmp(cola,"todos",5)==0){
			puts("Mostrar estado de todas las colas:");
			log_trace(logTrace,"mostrar todas las colas");

			LOCK_PLANIF;
			mostrarColaNew();
			mostrarColaReady();
			mostrarColaExec();
			mostrarColaExit();
			mostrarColaBlock();
			UNLOCK_PLANIF;
		}
		if (strncmp(cola,"new",3)==0){
			log_trace(logTrace,"mostrar cola new");

			puts("Mostrar estado de cola NEW");
			MUX_LOCK(&mux_new);
			mostrarColaNew(); MUX_UNLOCK(&mux_new);
		}
		if (strncmp(cola,"ready",5)==0){
			log_trace(logTrace,"mostrar cola ready");
			puts("Mostrar estado de cola REDADY");
			MUX_LOCK(&mux_ready);
			mostrarColaReady(); MUX_UNLOCK(&mux_ready);
		}
		if (strncmp(cola,"exec",4)==0){
			log_trace(logTrace,"mostrar cola exec");
			puts("Mostrar estado de cola EXEC");
			MUX_LOCK(&mux_exec);
			mostrarColaExec(); MUX_UNLOCK(&mux_exec);
		}
		if (strncmp(cola,"exit",4)==0){
			log_trace(logTrace,"mostrar cola exit");
			puts("Mostrar estado de cola EXIT");
			MUX_LOCK(&mux_exit);
			mostrarColaExit(); MUX_UNLOCK(&mux_exit);
		}
		if (strncmp(cola,"block",5)==0){
			log_trace(logTrace,"mostrar cola block");
			puts("Mostrar estado de cola BLOCK");
			MUX_LOCK(&mux_block);
			mostrarColaBlock(); MUX_UNLOCK(&mux_block);
		}

	}else{
		puts("Muestro estado de las colas con planificacion bloqueada");
		log_trace(logTrace,"mostrar la(s) cola(s) con la planificacion bloqueada");
		if (strncmp(cola,"todos",5)==0){
			puts("Mostrar estado de todas las colas:");
			log_trace(logTrace,"mostrar todas las colas");
			mostrarColaNew();
			mostrarColaReady();
			mostrarColaExec();
			mostrarColaExit();
			mostrarColaBlock();

		}
		if (strncmp(cola,"new",3)==0){
			log_trace(logTrace,"mostrar cola new");
			puts("Mostrar estado de cola NEW");
			mostrarColaNew();
		}
		if (strncmp(cola,"ready",5)==0){
			log_trace(logTrace,"mostrar cola ready");
			puts("Mostrar estado de cola REDADY");
			mostrarColaReady();
		}
		if (strncmp(cola,"exec",4)==0){
			log_trace(logTrace,"mostrar cola exec");
			puts("Mostrar estado de cola EXEC");
			mostrarColaExec();
		}
		if (strncmp(cola,"exit",4)==0){
			log_trace(logTrace,"exit");
			puts("Mostrar estado de cola EXIT");
			mostrarColaExit();
		}
		if (strncmp(cola,"block",5)==0){
			log_trace(logTrace,"mostrar cola block");
			puts("Mostrar estado de cola BLOCK");
			mostrarColaBlock();
		}
	}
}


void mostrarColaNew(){
	log_trace(logTrace,"inicio mostrar cola new ");
	puts("###Cola New### ");
	int k=0;
	tPCB * pcbAux;
	if(queue_size(New)==0){
		printf("No hay procesos en esta cola\n");
		log_trace(logTrace,"fin mostrar cola new ");
		return;
	}
	for(k=0;k<queue_size(New);k++){
		pcbAux = (tPCB*) queue_get(New,k);
		printf("En la posicion %d, el proceso %d\n",k,pcbAux->id);
	}
	log_trace(logTrace,"fin mostrar cola new");
}

void mostrarColaReady(){
	log_trace(logTrace,"inicio mostrar cola ready ");
	puts("###Cola Ready###");
	int k=0;
	tPCB * pcbAux;
	if(queue_size(Ready)==0){
		printf("No hay procesos en esta cola\n");
		log_trace(logTrace,"fin mostrar cola ready");
		return;
	}
	for(k=0;k<queue_size(Ready);k++){
		pcbAux = (tPCB*) queue_get(Ready,k);
		printf("En la posicion %d, el proceso %d\n",k,pcbAux->id);
	}
	log_trace(logTrace,"fin mostrar cola rady ");
}

void mostrarColaExec(){
	log_trace(logTrace,"inicio mostrar cola exec");
	puts("###Cola Exec###");
	int k=0;
	tPCB * pcbAux;
	if(list_size(Exec)==0){
		printf("No hay procesos en esta cola\n");
		log_trace(logTrace,"fin mostrar cola exec");
		return;
	}
	for(k=0;k<list_size(Exec);k++){
		pcbAux = (tPCB*) list_get(Exec,k);
		printf("En la posicion %d, el proceso %d\n",k,pcbAux->id);
	}
	log_trace(logTrace,"fin mostrar cola exec");
}

void mostrarColaExit(){
	log_trace(logTrace,"inicio mostrar cola exit");
	puts("###Cola Exit###");
	int k=0;
	tPCB * pcbAux;
	if(queue_size(Exit)==0){
		printf("No hay procesos en esta cola\n");
		log_trace(logTrace,"fin mostrar cola exit ");
		return;
	}
	for(k=0;k<queue_size(Exit);k++){
		pcbAux = (tPCB*) queue_get(Exit,k);
		printf("En la posicion %d, el proceso %d con un ExitCode: %d\n",k,pcbAux->id,pcbAux->exitCode);
	}
	log_trace(logTrace,"fin mostrar cola exit");
}

void mostrarColaBlock(){
	log_trace(logTrace,"inicio mostrar cola block");
	puts("###Cola Block###");
	int k=0;
	tPCB * pcbAux;
	if(list_size(Block)==0){
			printf("No hay procesos en esta cola\n");
			log_trace(logTrace,"fin mostrar cola block");
			return;
		}
	for(k=0;k<list_size(Block);k++){
		pcbAux = (tPCB*) list_get(Block,k);
		printf("En la posicion %d, el proceso %d\n",k,pcbAux->id);
	}
	log_trace(logTrace,"fin mostrar cola block");
}


void mostrarInfoDe(int pidElegido){
	log_trace(logTrace,"inicio mostrar info de %d", pidElegido);

	printf("############PROCESO %d############\n", pidElegido);
	mostrarCantHeapUtilizadasDe(pidElegido);
	mostrarCantSyscallsUtilizadasDe(pidElegido);
	mostrarCantRafagasEjecutadasDe(pidElegido);
	mostrarTablaDeArchivosDe(pidElegido);
	mostrarTablaGlobal();
	return;
}

void mostrarCantRafagasEjecutadasDe(int pid){
	log_trace(logTrace,"mostrar cant rafagas ejecutadas");

	int pos;
	MUX_LOCK(&mux_infoProc);
	if ((pos = getInfoProcPosByPID(list_infoProc, pid)) == -1){
		MUX_UNLOCK(&mux_infoProc);
		return;
	}
	t_infoProcess *ip = list_get(list_infoProc, pos);
	MUX_UNLOCK(&mux_infoProc);

	printf("Cantidad de rafagas ejecutadas:         %d\n", ip->rafagas_exec);
	log_trace(logTrace,"fin mostrar cant rafagas ejecutadas");
}

void armarStringPermisos(char* permisos, int creacion, int lectura, int escritura){

	log_trace(logTrace,"inicio armar string permisos");
	if (creacion)
		string_append(&permisos, "c");

	if (lectura)
		string_append(&permisos, "r");

	if (escritura)
		string_append(&permisos, "w");

	log_trace(logTrace,"fin armar string permisos");
}

void mostrarTablaDeArchivosDe(int pid){
	log_trace(logTrace, "inicio mostrar tabla de archivos");

	bool encontrarPid(t_procesoXarchivo * proceso){
			return proceso->pid == pid;
	}

	tProcesoArchivo *archProceso;
	t_procesoXarchivo *pa;

	if ((pa = list_find(tablaProcesos, (void *) encontrarPid)) == NULL
			|| list_is_empty(pa->archivosPorProceso)){
		printf("La tabla de procesos del proceso %d se encuentra vacía\n", pid);
		log_trace(logTrace,"fin mostrar tabla de archivos");
		return;
	}

	char *permisos = string_new();
	printf("####PROCESO %d####\nTabla de archivos abiertos del proceso\n", pid);
	int i;
	for(i = 0; i < list_size(pa->archivosPorProceso); i++){
		archProceso = list_get(pa->archivosPorProceso, i);
		armarStringPermisos(permisos, archProceso->flag.creacion, archProceso->flag.escritura, archProceso->flag.lectura);

		printf("Permisos: %s -- fdGlobalAsociado: %d -- cursor: %d", permisos, archProceso->fdGlobal, archProceso->posicionCursor);
	}
	log_trace(logTrace,"fin mostrar tabla de archivos");
}

void mostrarCantHeapUtilizadasDe(int pid){
	log_trace(logTrace,"inicio mostrar cant heap utilizadas");

	int pos;

	MUX_LOCK(&mux_infoProc);
	if ((pos = getInfoProcPosByPID(list_infoProc, pid)) == -1){
		MUX_UNLOCK(&mux_infoProc);
		return;
	}
	t_infoProcess *ip = list_get(list_infoProc, pos);
	MUX_UNLOCK(&mux_infoProc);

	printf("Cantidad de paginas de heap utilizadas: %d\n", ip->ih.cant_heaps);
	printf("Cantidad de allocaciones realizadas:    %d\n", ip->ih.cant_alloc);
	printf("Cantidad de bytes allocados:            %d\n", ip->ih.bytes_allocd);
	printf("Cantidad de liberaciones realizadas:    %d\n", ip->ih.cant_frees);
	printf("Cantidad de bytes liberados:            %d\n", ip->ih.bytes_freed);

	log_trace(logTrace,"fin mostrar cant heap utilizadas");
}

int getInfoProcPosByPID(t_list *infoProc, int pid){
	log_trace(logTrace,"inicio get info proc pos by pid");
	int i;
	t_infoProcess *ip;

	for (i = 0; i < list_size(infoProc); ++i){
		ip = list_get(infoProc, i);
		if (ip->pid == pid){
			log_trace(logTrace,"fin get info proc pos by pid");
			return i;
		}
	}
	log_error(logTrace,"no se encontro pid en la lista de infoproc");
	printf("No se encontro PID %d en la lista infoProc\n", pid);
	return -1;
}

void mostrarCantSyscallsUtilizadasDe(int pid){
	log_trace(logTrace,"inicio mostrar cant syscal utilizadas");

	int pos;

	MUX_LOCK(&mux_infoProc);
	if ((pos = getInfoProcPosByPID(list_infoProc, pid)) == -1){
		MUX_UNLOCK(&mux_infoProc);
		return;
	}
	t_infoProcess *ip = list_get(list_infoProc, pos);
	MUX_UNLOCK(&mux_infoProc);
	printf("Cantidad de syscalls utilizadas:        %d\n", ip->cant_syscalls);
	log_trace(logTrace,"fin mostrar cant syscall utilizadas");
}

void cambiarGradoMultiprogramacion(int nuevoGrado){
	log_trace(logTrace,"inicio cambiar grado mult");
	printf("vamos a cambiar el grado a %d\n",nuevoGrado);
	MUX_LOCK(&mux_gradoMultiprog);
	grado_mult=nuevoGrado;
	MUX_UNLOCK(&mux_gradoMultiprog);
	log_trace(logTrace,"fin cambiar grado mult");
}

void finalizarProceso(int pidAFinalizar){
	log_trace(logTrace,"inicio finalizar proceso");
	printf("vamos a finalizar el proceso %d\n",pidAFinalizar);


	t_finConsola *fc = malloc (sizeof(fc));
	fc->pid=pidAFinalizar ;
	fc->ecode=CONS_FIN_PROG;

	MUX_LOCK(&mux_listaFinalizados);
	list_add(finalizadosPorConsolas, fc);
	MUX_UNLOCK(&mux_listaFinalizados);
	log_trace(logTrace,"fin finalizar proceso");
}

void mostrarTablaGlobal(void){
	log_trace(logTrace,"inicio mostrar tabla global");

	if(dictionary_is_empty(tablaGlobal)){
		printf("La tabla global de archivos se encuentra vacía\n");
		log_trace(logTrace,"fin mostrar tabla global");
		return;
	}

	tDatosTablaGlobal *datosGlobal;
	printf("Los datos de la tabla global son: \n");

	int i;
	char contAux[MAXPID_DIG];
	for(i = 3; i < dictionary_size(tablaGlobal); i++){
		sprintf(contAux, "%d", i);

		if (!dictionary_has_key(tablaGlobal, contAux))
			continue;

		datosGlobal = dictionary_get(tablaGlobal, contAux);
		printf("FD: %d --- Direccion: %s --- Cantidad aperturas: %d\n",
				datosGlobal->fd, datosGlobal->direccion, datosGlobal->cantidadOpen);
	}
}

void stopKernel(){
	log_trace(logTrace,"inicio stop kernel");

	MUX_LOCK(&mux_planificacionBloqueada);
	if(!planificacionBloqueada){
		log_trace(logTrace,"planificacion bloqueada");
		puts("#####DETENEMOS LA PLANIFICACION#####");
		LOCK_PLANIF;
		planificacionBloqueada=true;

	}else{
		log_trace(logTrace,"planificaicon desbloqueada");
		puts("#####REANUDAMOS LA PLANIFICACION#####");
		UNLOCK_PLANIF;
		planificacionBloqueada=false;
	}

	log_trace(logTrace,"fin stop kernel");

	MUX_UNLOCK(&mux_planificacionBloqueada);
}

int getQueuePositionByPid(int pid, t_queue *queue){

	int i, size;
	tPCB *pcb;

	size = queue_size(queue);
	for (i = 0; i < size; ++i){
		pcb = queue_get(queue, i);
		if (pcb->id == pid)
			return i;
	}
	return -1;
}

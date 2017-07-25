#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>

#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/tiposErrores.h>
#include <commons/log.h>

#include "funcionesSyscalls.h"
#include "kernelConfigurators.h"
#include "planificador.h"
#include "defsKernel.h"

extern tKernel *kernel;
extern t_valor_variable *shared_vals;
extern t_log *logTrace;

t_dictionary *dict_sems_queue; // queue de PIDs que esperan signal de algun SEM
pthread_mutex_t mux_sems_queue;

void setupGlobales_syscalls(void){
	log_trace(logTrace,"setup globales syscalls");
	dict_sems_queue = dictionary_create();
	pthread_mutex_init(&mux_sems_queue, NULL);
}

int getSemPosByID(const char *sem){
	log_trace(logTrace,"get sem pos by id");
	int i;
	for (i = 0; i < kernel->sem_quant; ++i)
		if (strcmp(kernel->sem_ids[i], sem) == 0)
			return i;

	log_error(logTrace, "No se encontro el semaforo %s en la config del Kernel\n", sem);
	return -1;
}

int waitSyscall(const char *sem, int pid){
	log_trace(logTrace,"wait syscall");
	int p;
	if ((p = getSemPosByID(sem)) == -1)
		return VAR_NOT_FOUND;

	kernel->sem_vals[p]--;
	if (kernel->sem_vals[p] < 0){
		puts("Ejecucion espera semaforo");
		enqueuePIDtoSem((char *) sem, pid);
		return PCB_BLOCK;
		// todo: blockByPID(pid) en planificador...
	}
	return S_WAIT; // este retorno indica en realidad que no se bloquea el PCB
}

int signalSyscall(const char *sem){
	log_trace(logTrace,"signal syscall");
	int p, *pid;
	if ((p = getSemPosByID(sem)) == -1)
		return VAR_NOT_FOUND;

	kernel->sem_vals[p]++;
	if (kernel->sem_vals[p] >= 0){
		if ((pid = unqueuePIDfromSem((char *) sem)) == NULL)
			return 0;
		unBlockByPID(*pid);
		free(pid);
		puts("Ejecucion continua por semaforo");
	}

	return 0;
}

int setGlobalSyscall(tPackValComp *val_comp){
	log_trace(logTrace,"set global syscall");
	int i;
	int nlen = strlen(val_comp->nom) + 2; // espacio para el ! y el '\0'
	char *aux = NULL;
	for (i = 0; i < kernel->shared_quant; ++i){
		aux = realloc(aux, nlen); aux[0] = '!'; memcpy(aux + 1, val_comp->nom, nlen); aux[nlen] = '\0';

		if (strcmp(kernel->shared_vars[i], aux) == 0){
			shared_vals[i] = val_comp->val;
			free(aux);
			return 0;
		}
	}
	free(aux);
	return GLOBAL_NOT_FOUND;
}

t_valor_variable getGlobalSyscall(t_nombre_variable *var, bool* found){

	log_trace(logTrace,"get global syscall");
	int i;
	int nlen = strlen(var) + 2; // espacio para el ! y el '\0'
	char *aux = NULL;
	*found = true;
	for (i = 0; i < kernel->shared_quant; ++i){
		aux = realloc(aux, nlen); aux[0] = '!'; memcpy(aux + 1, var, nlen); aux[nlen] = '\0';

		if (strcmp(kernel->shared_vars[i], aux) == 0){
			free(aux);
			return shared_vals[i];
		}
	}
	free(aux);
	*found = false;
	//printf("No se encontro la global %s en la config del Kernel\n", var);
	log_error(logTrace,"no se encontro la global %s en la config del kernel",var);
	return GLOBAL_NOT_FOUND;
}

void enqueuePIDtoSem(char *sem, int pid){
	log_trace(logTrace,"enqueue pid to sem");
	t_queue *pids_blk;
	int *pid_b = malloc(sizeof(int));
	*pid_b = pid; // creamos una copia del PID


	MUX_LOCK(&mux_sems_queue);
	if (!dictionary_has_key(dict_sems_queue, sem)){
		pids_blk = queue_create();
		queue_push(pids_blk, pid_b);
		dictionary_put(dict_sems_queue, sem, pids_blk);

	} else {
		pids_blk = dictionary_get(dict_sems_queue, sem);
		queue_push(pids_blk, pid_b);
	}
	MUX_UNLOCK(&mux_sems_queue);
}

int *unqueuePIDfromSem(char *sem){

	log_trace(logTrace,"unqueue pid from sem");
	MUX_LOCK(&mux_sems_queue);
	if (!dictionary_has_key(dict_sems_queue, sem)){
		log_trace(logTrace,"el semaforo no tiene registrado ningun pid");
		//printf("El semaforo %s no tiene registrado ningun PID\n", sem);
		MUX_UNLOCK(&mux_sems_queue);
		return NULL;
	}

	t_queue *pids_blk = dictionary_get(dict_sems_queue, sem);
	if (queue_is_empty(pids_blk)){
		log_trace(logTrace,"no hay pids esperando al sem");
		//printf("No hay PIDs esperando a %s\n", sem);
		MUX_UNLOCK(&mux_sems_queue);
		return NULL;
	}
	int *pid = queue_pop(pids_blk);
	MUX_UNLOCK(&mux_sems_queue);

	return pid;
}

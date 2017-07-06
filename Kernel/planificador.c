#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>

#include <commons/collections/queue.h>

#include "planificador.h"
#include "kernelConfigurators.h"
#include "auxiliaresKernel.h"


#include <tiposRecursos/misc/pcb.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/tiposErrores.h>
#include <funcionesCompartidas/funcionesCompartidas.h>
#include <funcionesPaquetes/funcionesPaquetes.h>


#define FIFO_INST -1

void planificar(void);

void finalizarPrograma(int pid, tPackHeader * header, int socket);
void setearQuamtumS(void);
void pausarPlanif(){

}

/* Obtiene la posicion en que se encuentra un PCB en una lista,
 * dado el pid por el cual ubicarlo.
 * Es requisito necesario que la lista contenga tPCB*
 * Retorna la posicion si la encuentra.
 * Retorna valor negativo en caso contrario.
 */
int getPCBPositionByPid(int pid, t_list *cola_pcb);
int obtenerCPUociosa(void);

extern int sock_mem;
extern int frame_size;
extern t_list *gl_Programas;
t_list *listaDeCpu; // el cpu_manejador deberia crear el entry para esta lista.

t_queue *New, *Exit, *Block,*Ready;
t_list	*cpu_exec,*Exec;
char *recvHeader(int sock_in, tPackHeader *header);

int grado_mult;
extern tKernel *kernel;

extern sem_t hayProg;

void setupSemaforosColas(void){
	pthread_mutex_init(&mux_new,   NULL);
	pthread_mutex_init(&mux_ready, NULL);
	pthread_mutex_init(&mux_exec,  NULL);
	pthread_mutex_init(&mux_block, NULL);
	pthread_mutex_init(&mux_exec,  NULL);
}

void setupPlanificador(void){

	grado_mult = kernel->grado_multiprog;

	New   = queue_create();
	Ready = queue_create();
	Exit  = queue_create();

	Exec  = list_create();
	Block = queue_create();

	setupSemaforosColas();

	listaDeCpu = list_create();
	cpu_exec   = list_create();

	planificar();
}

void mandarPCBaCPU(tPCB *pcb, t_RelCC * cpu){

	int pack_size, stat;
	pack_size = 0;
	tPackHeader head = { .tipo_de_proceso = KER, .tipo_de_mensaje = PCB_EXEC };
	puts("Comenzamos a serializar el PCB");

	pcb->proxima_rafaga = (kernel->algo == FIFO)? FIFO_INST : kernel->quantum;

	char *pcb_serial;
	if ((pcb_serial = serializePCB(pcb, head, &pack_size)) == NULL){
		puts("Fallo serializacion de pcb");
		return;
	}
	printf("pack_size: %d\n", pack_size);

	puts("Enviamos el PCB a CPU");
	if ((stat = send(cpu->cpu.fd_cpu, pcb_serial, pack_size, 0)) == -1){
		perror("Fallo envio de PCB a CPU. error");
		return;
	}
	printf("Se enviaron %d bytes a CPU\n", pack_size);

	printf("Se agrego sock_cpu #%d a lista \n",cpu->cpu.fd_cpu);

	//freeAndNULL((void **) &pcb_serial); todo: ver si falla
}

/* Enlaza el CPU y el Programa, por medio del PID del PCB que comparten.
 * Se copian los valores de Programa en CPU y se reasignan los punteros.
 * Se apuntan mutuamente, hasta que se deba reasignar el PCB enlazante.
 */
void asociarProgramaACPU(t_RelCC *cpu){

	t_RelPF *pf = getProgByPID(cpu->cpu.pid);

	cpu->con->fd_con = pf->prog->con->fd_con;
	cpu->con->pid    = pf->prog->con->pid;
	freeAndNULL((void **) &pf->src->sourceCode);
	freeAndNULL((void **) &pf->src);
	free(pf->prog->con); pf->prog->con = cpu->con;

	pf->prog->cpu.fd_cpu = cpu->cpu.fd_cpu;
	pf->prog->cpu.pid    = cpu->cpu.pid;
}

void planificar(void){

	grado_mult = kernel->grado_multiprog;
	tPCB * pcbAux;
	t_RelCC * cpu;

	while(1){

	sem_wait(&hayProg);
	switch(kernel->algo){

	case (FIFO):
		printf("Estoy en fifo\n");
		if(!queue_is_empty(New)){
			pcbAux = (tPCB*) queue_pop(New);
			if(list_size(Exec) < grado_mult){
				encolarDeNewEnReady(pcbAux);
				// queue_push(Ready,pcbAux); esto quedo dentro de encolarDeNewEnReady();
			}
		}

		if(!queue_is_empty(Ready)){
			if(list_size(listaDeCpu) > 0) { // todo: actualizar esta lista...
				pcbAux = (tPCB*) queue_peek(Ready);
				cpu = (t_RelCC *) list_get(listaDeCpu, obtenerCPUociosa());
				cpu->cpu.pid = pcbAux->id;

				pthread_mutex_lock(&mux_ready);
				pcbAux = (tPCB*) queue_pop(Ready);
				pthread_mutex_unlock(&mux_ready);
				pthread_mutex_lock(&mux_exec);
				list_add(Exec, pcbAux);
				pthread_mutex_unlock(&mux_exec);

				mandarPCBaCPU(pcbAux, cpu);
				asociarProgramaACPU(cpu);
			}
		}
		break;

		//Para saber que hacer con BLOCK y EXIT, recibo mensajes de las cpus activas
		/*int i;
		int j;
		char *paquete_pcb_serial;
		int stat;
		tPackHeader * header = malloc(HEAD_SIZE);
		tPackHeader * headerMemoria = malloc(sizeof headerMemoria);
		t_cpuInfo p;*/


			/*if((paquete_pcb_serial = recvHeader(cpu_executing->fd_cpu, header)) == NULL){
					printf("Fallo recvPCB");
					break;
			}
			*/
			//pcbAux = deserializarPCB(paquete_pcb_serial);

			/*switch(header->tipo_de_mensaje){


				case(FIN_PROCESO):case(ABORTO_PROCESO):case(RECURSO_NO_DISPONIBLE): //COLA EXIT
					//queue_push(Exit,pcbAux);

					printf("Se finaliza un proceso, libero memoria y luego consola\n");
					headerMemoria->tipo_de_mensaje = FIN_PROG;
					headerMemoria->tipo_de_proceso = KER;
					//ConsolaAsociada()

					//Aviso a memoria
					finalizarPrograma(cpu->cpu.pid, headerMemoria, cpu->con.fd_con);
					// Le informo a la Consola asociada:

					for(j = 0;j<list_size(listaProgramas);j++){
						consolaAsociada = (t_consola *) list_get(listaProgramas,j);
						if(consolaAsociada->pid == cpu->cpu.pid){
						headerMemoria->tipo_de_mensaje = FIN_PROG;
						headerMemoria->tipo_de_proceso = KER;
						if((stat = send(consolaAsociada->fd_con,headerMemoria,sizeof (tPackHeader),0))<0){
							perror("error al enviar a la consola");
							break;
						}

						// Libero la Consola asociada y la saco del sistema:
						list_remove(listaProgramas,j);

						free(consolaAsociada);consolaAsociada = NULL;
						}
					}
					list_remove(cpu_exec,i);
					list_add(listaDeCpu,cpu);
					queue_push(Exit,pcbAux);

					break;
				default:
					break;
				}
		}
			if(!queue_is_empty(Block)){
				pcbAux = (tPCB *) queue_pop(Block);
				queue_push(Ready,pcbAux);*/
				//Todo: tengo que pensar como saber si están o no disponibles los recursos
			//}

	case (RR):
		break;

	} // cierra Switch

		setearQuamtumS();
		//(pcb *) queue_pop(Ready);
		//list_add(Exec, pcb);

	} // cierra While

	//free(cpu);cpu = NULL;
	//free(pcbAux); pcbAux = NULL;
	//list_destroy(cpu_exec);
	}

/* Una vez que lo se envia el pcb a la cpu, la cpu debería avisar si se pudo ejecutar todo o no
 *
 * *///}


void setearQuamtumS(void){
	int i;
	for(i = 0; i < Ready->elements->elements_count ; i++){
		tPCB * pcbReady = (tPCB*) list_get(Ready->elements,i);
	//	pcbReady->quantum = kernel-> quamtum;
	//	pcbReady->quamtumSleep = kernel -> quamtumSleep;

	}
}

void cpu_handler_planificador(t_RelCC *cpu){ // todo: revisar este flujo de acciones
	tPCB *pcbAux, *pcbPlanif;

	int j;
	int stat;
	tPackHeader * headerMemoria = malloc(sizeof headerMemoria); //Uso el mismo header para avisar a la memoria y consola
	char *buffer;

	switch(cpu->msj){
	case (FIN_PROCESO):
		puts("Se recibe FIN_PROCESO de CPU");

		buffer = recvGeneric(cpu->cpu.fd_cpu);

		pcbAux = deserializarPCB(buffer); // todo: rompe en deserializar Stack

		pthread_mutex_lock(&mux_exec);
		pcbPlanif = list_remove(Exec, getPCBPositionByPid(pcbAux->id, Exec));
		pthread_mutex_unlock(&mux_exec);

		pcbPlanif->exitCode = pcbAux->exitCode; // todo: que valores nos importan retener?

		pthread_mutex_lock(&mux_exit);
		queue_push(Exit, pcbPlanif);
		pthread_mutex_unlock(&mux_exit);

		break;

	case (ABORTO_PROCESO): case (RECURSO_NO_DISPONIBLE): //COLA EXIT
		//queue_push(Exit,pcbAux);




			printf("Se finaliza un proceso, libero memoria y luego consola\n");
	headerMemoria->tipo_de_mensaje = FIN_PROG;
	headerMemoria->tipo_de_proceso = KER;
	//ConsolaAsociada()

	//Aviso a memoria
	finalizarPrograma(cpu->cpu.pid, headerMemoria, cpu->con->fd_con);
	// Le informo a la Consola asociada:

	for(j = 0;j<list_size(listaDeCpu);j++){
		cpu = (t_RelCC *) list_get(listaDeCpu,j);
		headerMemoria->tipo_de_mensaje = FIN_PROG;
		headerMemoria->tipo_de_proceso = KER;

		if((stat = send(cpu->con->fd_con,headerMemoria,sizeof (tPackHeader),0))<0){
			perror("error al enviar a la consola");
			break;
		}
	}

	pcbAux = list_remove(Exec, getPCBPositionByPid(cpu->cpu.pid, Exec));
	queue_push(Exit,pcbAux);

	break;
	default:
		break;
	}
}

int getPCBPositionByPid(int pid, t_list *cola_pcb){

	int i;
	tPCB *pcb;
	for (i = 0; i < list_size(cola_pcb); ++i){
		pcb = list_get(cola_pcb, i);
		if (pcb->id == pid)
			return i;
	}

	return -1;

}
void largoPlazo(int multiprog){

	if(queue_size(Ready) < multiprog){
		// controlamos que nadie mas este usando este recurso
		queue_push(Ready, queue_pop(New));

	} else if(queue_size(Ready) > multiprog){
		// controlamos que el programa se termine de ejecutar
		queue_push(Exit, queue_pop(Ready));
	}
}


void cortoPlazo(){}

void encolarEnNew(tPCB *pcb){
	puts("Se encola el programa");

	pthread_mutex_lock(&mux_new);
	queue_push(New, pcb);
	pthread_mutex_unlock(&mux_new);
}

t_RelPF *getProgByPID(int pid){
	puts("Obtener Programa mediante PID");

	t_RelPF *pf;
	int i;
	for (i = 0; i < list_size(gl_Programas); ++i){
		pf = list_get(gl_Programas, i);
		if (pid == pf->prog->con->pid)
			return pf;
	}

	printf("No se encontro el programa relacionado al PID %d\n", pid);
	return NULL;
}

void encolarDeNewEnReady(tPCB *pcb){
	printf("Se encola el PID %d en Ready\n", pcb->id);

	t_RelPF *pf = getProgByPID(pcb->id);

	t_metadata_program *meta = metadata_desde_literal(pf->src->sourceCode);
	t_size indiceCod_size = meta->instrucciones_size * sizeof(t_intructions);
	int code_pages = (int) ceil((float) pf->src->sourceLen / frame_size);

	pcb->pc                     = meta->instruccion_inicio;
	pcb->paginasDeCodigo        = code_pages;
	pcb->etiquetas_size         = meta->etiquetas_size;
	pcb->cantidad_etiquetas     = meta->cantidad_de_etiquetas;
	pcb->cantidad_instrucciones = meta->instrucciones_size;

	if ((pcb->indiceDeCodigo    = malloc(indiceCod_size)) == NULL){
		printf("Fallo malloc de %d bytes para pcb->indiceDeCodigo\n", indiceCod_size);
		return;
	} memcpy(pcb->indiceDeCodigo, meta->instrucciones_serializado, indiceCod_size);

	if (pcb->cantidad_etiquetas){
		if ((pcb->indiceDeEtiquetas = malloc(meta->etiquetas_size)) == NULL){
			printf("Fallo malloc de %d bytes para pcb->indiceDeEtiquetas\n", meta->etiquetas_size);
			return;
		}
		memcpy(pcb->indiceDeEtiquetas, meta->etiquetas, pcb->etiquetas_size);
	}

	avisarPIDaPrograma(pf->prog->con->pid, pf->prog->con->fd_con);
	iniciarYAlojarEnMemoria(pf, code_pages + kernel->stack_size);

	pthread_mutex_lock(&mux_ready);
	queue_push(Ready, pcb);
	pthread_mutex_unlock(&mux_ready);

	metadata_destruir(meta);
}

void iniciarYAlojarEnMemoria(t_RelPF *pf, int pages){

	int stat;
	char *pidpag_serial;
	int pack_size = 0;
	tPackHeader ini = { .tipo_de_proceso = KER, .tipo_de_mensaje = INI_PROG };
	tPackHeader src = { .tipo_de_proceso = KER, .tipo_de_mensaje = ALMAC_BYTES };

	tPackPidPag pp;
	pp.head = ini;
	pp.pid = pf->prog->con->pid;
	pp.pageCount = pages;

	if ((pidpag_serial = serializePIDPaginas(&pp, &pack_size)) == NULL){
		puts("fallo serialize PID Paginas");
		return;
	}

	puts("Enviamos inicializacion de programa a Memoria");
	if ((stat = send(sock_mem, pidpag_serial, pack_size, 0)) == -1){
		puts("Fallo pedido de inicializacion de prog en Memoria...");
		return;
	}

	tPackByteAlmac *pbal;
	if ((pbal = malloc(sizeof *pbal)) == NULL){
		printf("Fallo mallocar %d bytes para pbal\n", sizeof *pbal);
		return;
	}

	memcpy(&pbal->head, &src, HEAD_SIZE);
	pbal->pid    = pf->prog->con->pid;
	pbal->page   = 0;
	pbal->offset = 0;
	pbal->size   = pf->src->sourceLen;
	pbal->bytes  = pf->src->sourceCode;

	char *packBytes; pack_size = 0;
	if ((packBytes = serializeByteAlmacenamiento(pbal, &pack_size)) == NULL){
		puts("fallo serialize Bytes");
		return;
	}

	puts("Enviamos el srccode");
	if ((stat = send(sock_mem, packBytes, pack_size, 0)) == -1)
		puts("Fallo envio src code a Memoria...");

	printf("se enviaron %d bytes\n", stat);

	free(pidpag_serial);
	free(pbal);
	free(packBytes);
}

void avisarPIDaPrograma(int pid, int sock_prog){

	int pack_size, stat;
	char *pid_serial;
	tPackPID *pack_pid;

	if ((pack_pid = malloc(sizeof *pack_pid)) == NULL){
		printf("No se pudieron mallocar %d bytes para packPID\n", sizeof *pack_pid);
		return;
	}
	pack_pid->head.tipo_de_proceso = KER; pack_pid->head.tipo_de_mensaje = PID;
	pack_pid->val = pid;

	pack_size = 0;
	if ((pid_serial = serializePID(pack_pid, &pack_size)) == NULL){
		puts("No se serializo bien");
		return;
	}
	freeAndNULL((void **) &pack_pid);

	printf("Aviso al hilo_consola %d su numero de PID\n", sock_prog);
	if ((stat = send(sock_prog, pid_serial, pack_size, 0)) == -1){
		perror("Fallo envio de PID a Consola. error");
		return;
	}
	printf("Se enviaron %d bytes al hilo_consola\n", stat);

	free(pid_serial);
}

void encolarEnNewPrograma(tPCB *nuevoPCB, int sock_con){
	puts("Se encola el programa");
	int stat;
	int pack_size;

	tPackPID *pack_pid;
	if ((pack_pid = malloc(sizeof *pack_pid)) == NULL){
		printf("Fallo mallocar %d bytes para pack_pid\n", sizeof *pack_pid);
		return;
	}

	pack_pid->head.tipo_de_proceso = KER;
	pack_pid->head.tipo_de_mensaje = PID;
	pack_pid->val = nuevoPCB->id;

	pack_size = 0;
	char *pid_serial = serializePID(pack_pid, &pack_size);
	if (pid_serial == NULL){
		puts("No se serializo bien");
		return;
	}

	printf("Aviso al hilo_consola %d su numero de PID\n", sock_con);
	if ((stat = send(sock_con, pid_serial, pack_size, 0)) == -1){
		perror("Fallo envio de PID a Consola. error");
		return;
	}
	printf("Se enviaron %d bytes al hilo_consola\n", stat);

	t_RelCC *cpu_inex   = malloc(sizeof *cpu_inex); cpu_inex->con = malloc(sizeof *cpu_inex->con);
	cpu_inex->con->fd_con = sock_con;
	cpu_inex->cpu.pid     = -1;
	list_add(listaDeCpu, cpu_inex);

	queue_push(New, nuevoPCB);

	freeAndNULL((void **) &pack_pid);
}

void finalizarPrograma(int pid, tPackHeader * header, int socket){


	printf("Aviso a memoria que libere la memoria asiganda al proceso\n");

	int* exit_pid = malloc(sizeof exit_pid);
	*exit_pid = pid;
	send(socket,header,sizeof(tPackHeader),0);

	free(exit_pid);exit_pid = NULL;


}


char *recvHeader(int sock_in, tPackHeader *header){
	puts("Se recibe el paquete serializado..");

	int stat, pack_size;
	char *p_serial;

	if((stat = recv(sock_in, header, sizeof(tPackHeader),0)) <= 0){
		perror("Fallo de recv. error");
		return NULL;
	}
	if ((stat = recv(sock_in, &pack_size, sizeof(int), 0)) <= 0){
		perror("Fallo de recv. error");
		return NULL;
	}

	printf("Paquete de size: %d\n", pack_size);

	if ((p_serial = malloc(pack_size-12)) == NULL){
		printf("No se pudieron mallocar %d bytes para paquete generico\n", pack_size);
		return NULL;
	}

	if ((stat = recv(sock_in, p_serial, pack_size-12, 0)) <= 0){
		perror("Fallo de recv. error");
		return NULL;
	}

	return p_serial;
}

void pasarNewAReady(tPCB *pcb){

	//procesarSrcAPCB(pcb); // obtiene src_code del pcb, crea el metadata del programa, y lo termina de asignar al PCB


}


void pasarABlock(int sock_cpu){


}

int obtenerCPUociosa(void){
	t_RelCC * cpuOciosa;
	int cantidadCpu;
	for(cantidadCpu = 0; cantidadCpu < list_size(listaDeCpu); cantidadCpu ++){
		cpuOciosa = (t_RelCC *) list_get(listaDeCpu,cantidadCpu);
		if(cpuOciosa->cpu.pid < 0)
			return cantidadCpu;
	}
	return -1;
}


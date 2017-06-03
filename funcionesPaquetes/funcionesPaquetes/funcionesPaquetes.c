#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>

#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>
#include "funcionesPaquetes.h"


int handshakeCon(int sock_dest, int id_sender){

	int stat;
	char *package;
	tPackHeader head;
	head.tipo_de_proceso = id_sender;
	head.tipo_de_mensaje = HSHAKE;

	if ((package = malloc(HEAD_SIZE)) == NULL){
		fprintf(stderr, "No se pudo hacer malloc\n");
		return FALLO_GRAL;
	}
	memcpy(package, &head, HEAD_SIZE);

	if ((stat = send(sock_dest, package, HEAD_SIZE, 0)) == -1){
		perror("Fallo send de handshake. error");
		printf("Fallo send() al socket: %d\n", sock_dest);
		return FALLO_SEND;
	}

	return stat;
}


int contestarMemoriaKernel(int marco_size, int marcos, int sock_ker){

	int stat;

	tHShakeMemAKer *h_shake = malloc(sizeof *h_shake);
	h_shake->head.tipo_de_proceso = MEM;
	h_shake->head.tipo_de_mensaje = MEMINFO;
	h_shake->marco_size = marco_size;
	h_shake->marcos = marcos;

	if((stat = send(sock_ker, h_shake, sizeof *h_shake, 0)) == -1)
		perror("Error de envio informacion Memoria a Kernel. error");

	return stat;
}

int recibirInfoMem(int sock_mem, int *frames, int *frame_size){

	int stat;

	if ((stat = recv(sock_mem, frames, sizeof frames, 0)) == -1){
		perror("Error de recepcion de frames desde la Memoria. error");
		return FALLO_GRAL;
	}

	if ((stat = recv(sock_mem, frame_size, sizeof frame_size, 0)) == -1){
		perror("Error de recepcion de frames desde la Memoria. error");
		return FALLO_GRAL;
	}

	return stat;
}


tPackSrcCode *recvSourceCode(int sock_in){

	int stat;
	tPackSrcCode *src_pack;
	if ((src_pack = malloc(sizeof *src_pack)) == NULL){
		perror("No se pudo mallocar espacio para el paquete src_pack. error");
		return NULL;
	}

	// recibimos el valor de size que va a tener el codigo fuente
	if ((stat = recv(sock_in, &src_pack->sourceLen, sizeof (unsigned long), 0)) <= 0){
		perror("El socket cerro la conexion o hubo fallo de recepcion. error");
		errno = FALLO_RECV;
		return NULL;
	}

	// hacemos espacio para el codigo fuente
	if ((src_pack->sourceCode = malloc(src_pack->sourceLen)) == NULL){
		perror("No se pudo mallocar espacio para el src_pack->sourceCode. error");
		return NULL;
	}

	if ((stat = recv(sock_in, src_pack->sourceCode, src_pack->sourceLen, 0)) <= 0){
		perror("El socket cerro la conexion o hubo fallo de recepcion. error");
		errno = FALLO_RECV;
		return NULL;
	}

	return src_pack;
}


tPackSrcCode *deserializeSrcCode(int sock_in){
	unsigned long bufferSize;
	char *bufferCode;
	int offset = 0;
	int stat;

	tPackSrcCode *line_pack;

	// recibimos el valor del largo del codigo fuente
	if ((stat = recv(sock_in, &bufferSize, sizeof bufferSize, 0)) == -1){
		perror("No se pudo recibir el size del codigo fuente. error");
		return NULL;
	}
	bufferSize++; // hacemos espacio apra el /0

	// hacemos espacio para toda la estructura en serie
	if ((line_pack = malloc(HEAD_SIZE + sizeof (int) + bufferSize)) == NULL){
		perror("No se pudo mallocar para el src_pack");
		return NULL;
	}

	offset += sizeof (tPackHeader);
	memcpy(line_pack + offset, &bufferSize, sizeof bufferSize);
	offset += sizeof bufferSize;

	if ((bufferCode = malloc(bufferSize)) == NULL){
		perror("No se pudo almacenar memoria para el buffer del codigo fuente. error");
		return NULL;
	}

	// recibimos el codigo fuente
	if ((stat = recv(sock_in, bufferCode, bufferSize, 0)) == -1){
		perror("No se pudo recibir el size del codigo fuente. error");
		return NULL;
	}
	bufferCode[bufferSize -1] = '\0';

	memcpy(line_pack + offset, bufferCode, bufferSize);

	return line_pack;
}

void *serializeSrcCodeFromRecv(int sock_in, tPackHeader head, int *packSize){

	unsigned long bufferSize;
	void *bufferCode;
	int offset = 0;
	void *src_pack;

	int stat;

	// recibimos el valor del largo del codigo fuente
	if ((stat = recv(sock_in, &bufferSize, sizeof bufferSize, 0)) == -1){
		perror("No se pudo recibir el size del codigo fuente. error");
		return NULL;
	}

	// hacemos espacio para toda la estructura en serie
	if ((src_pack = malloc(HEAD_SIZE + sizeof (unsigned long) + bufferSize)) == NULL){
		perror("No se pudo mallocar para el src_pack");
		return NULL;
	}

	// copiamos el header, y el valor del bufferSize en el paquete
	memcpy(src_pack, &head, sizeof head);

	offset += sizeof head;
	memcpy(src_pack + offset, &bufferSize, sizeof bufferSize);
	offset += sizeof bufferSize;

	if ((bufferCode = malloc(bufferSize)) == NULL){
		perror("No se pudo almacenar memoria para el buffer del codigo fuente. error");
		return NULL;
	}


	// recibimos el codigo fuente
	if ((stat = recv(sock_in, bufferCode, bufferSize, 0)) == -1){
		perror("No se pudo recibir el size del codigo fuente. error");
		return NULL;
	}

	memcpy(src_pack + offset, bufferCode, bufferSize);

	// size total del paquete serializado
	*packSize = HEAD_SIZE + sizeof (unsigned long) + bufferSize;
	free(bufferCode);
	return src_pack;
}

char *serializePID(tPackPID *ppid){

	int off = 0;
	char *pid_serial;
	if ((pid_serial = malloc(sizeof *ppid)) == NULL){
		perror("No se pudo crear espacio de memoria para PID serial. error");
		return NULL;
	}

	memcpy(pid_serial + off, &ppid->head.tipo_de_proceso, sizeof ppid->head.tipo_de_proceso);
	off += sizeof ppid->head.tipo_de_proceso;
	memcpy(pid_serial + off, &ppid->head.tipo_de_mensaje, sizeof ppid->head.tipo_de_mensaje);
	off += sizeof ppid->head.tipo_de_mensaje;
	memcpy(pid_serial + off, &ppid->pid, sizeof ppid->pid);
	off += sizeof ppid->pid;

	return pid_serial;
}

char *serializePCB(tPackPCBaCPU *ppcb){

	char *pcb_serial;
	if ((pcb_serial = malloc(sizeof *ppcb)) == NULL){
		perror("No se pudo crear espacio de memoria para PCB serial. error");
		return NULL;
	}

	int off = 0;
	memcpy(pcb_serial + off, &ppcb->head.tipo_de_proceso, sizeof (ppcb->head.tipo_de_proceso));
	off += sizeof ppcb->head.tipo_de_proceso;
	memcpy(pcb_serial + off, &ppcb->head.tipo_de_mensaje, sizeof (ppcb->head.tipo_de_mensaje));
	off += sizeof ppcb->head.tipo_de_mensaje;
	memcpy(pcb_serial + off, &ppcb->pid, sizeof (ppcb->pid));
	off += sizeof ppcb->pid;
	memcpy(pcb_serial + off, &ppcb->pc, sizeof (ppcb->pc));
	off += sizeof ppcb->pc;
	memcpy(pcb_serial + off, &ppcb->pages, sizeof (ppcb->pages));
	off += sizeof ppcb->pages;
	memcpy(pcb_serial + off, &ppcb->exit, sizeof (ppcb->exit));
	off += sizeof ppcb->exit;

	return pcb_serial;
}









#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <commons/log.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/tiposErrores.h>
#include <funcionesPaquetes/funcionesPaquetes.h>
#include <funcionesCompartidas/funcionesCompartidas.h>

#include "apiMemoria.h"
extern t_log *logTrace;
int manejarSolicitudBytes(int sock_in){
	log_trace(logTrace,"funcion manejar solicitud byttes sock %d",sock_in);
	int ret_val = 0; // setamos 0 como el valor inicial de retorno
	int stat;
	int solic_size = 0;  // size del buffer de bytes a send'ear al sock_in
	char * solic_serial; // solicitud serializada recibida del sock_in
	char *bytes_solic;   // bytes obtenidos de Memoria Fisica
	char *bytes_tx_serial; // bytes aptos para send()
	tPackHeader head = {.tipo_de_proceso = MEM, .tipo_de_mensaje = BYTES};
	tPackByteReq *pbyte_req;

	if ((solic_serial = recvGeneric(sock_in)) == NULL){
		log_error(logTrace,"fallo la recepcion de solicitud");
		puts("Fallo la recepcion de solicitud");
		return FALLO_RECV;
	}

	if ((pbyte_req = deserializeByteRequest(solic_serial)) == NULL){
		log_error(logTrace,"fallo la deserializacion deol paquete solicitud de bytes");
		fprintf(stderr, "Fallo la deserializacion del paquete Solicitud de Bytes\n");
		ret_val = FALLO_DESERIALIZAC;
		goto retorno;
	}

	// Hacemos la solicitud de bytes propiamente dicha a Memoria Fisica
	if ((bytes_solic = solicitarBytes(pbyte_req->pid, pbyte_req->page, pbyte_req->offset, pbyte_req->size)) == NULL){
		log_error(logTrace,"fallo la carga de bytes en el buffer para la solicitud");
		fprintf(stderr, "Fallo la carga de bytes en el buffer para la solicitud\n");
		ret_val = FALLO_GRAL;
		goto retorno;
	}

	if ((bytes_tx_serial = serializeBytes(head, bytes_solic, pbyte_req->size + 1, &solic_size)) == NULL){
		log_error(logTrace,"fallo la carga de bytes en el buffer para la solciitud");
		fprintf(stderr, "Fallo la carga de bytes en el buffer para la solicitud\n");
		ret_val = FALLO_SERIALIZAC;
		goto retorno;
	}

	if ((stat = send(sock_in, bytes_tx_serial, solic_size, 0)) == -1){
		log_error(logTrace,"fallo envio de bytes serializados");
		perror("Fallo envio de Bytes serializados. error");
		ret_val = FALLO_SEND;
	}
	//printf("Se enviaron %d bytes al socket %d\n", solic_size, sock_in);
	log_trace(logTrace,"se enviaron %d bytes al sock %d",solic_size,sock_in);
retorno:
	freeAndNULL((void**) &pbyte_req);
	freeAndNULL((void**) &bytes_solic);
	return ret_val;
}

int manejarAlmacenamientoBytes(int sock_in){
	log_trace(logTrace,"funcion manejar almacenamiento bytes sock%d",sock_in);
	int stat;
	char *buffer;
	tPackByteAlmac *pbyte_al;

	if ((buffer = recvGeneric(sock_in)) == NULL){
		log_error(logTrace,"fallo la recepcion del paqte del socket %d",sock_in);
		printf("Fallo la recepcion de paquete del socket %d\n", sock_in);
		return FALLO_RECV;
	}

	if ((pbyte_al = deserializeByteAlmacenamiento(buffer)) == NULL){
		log_error(logTrace,"fallo la deserializacion del paq solicitud de bytes");
		fprintf(stderr, "Fallo la deserializacion del paquete Solicitud de Bytes\n");
		return FALLO_DESERIALIZAC;
	}

	// Hacemos el almacenamiento de bytes propiamente dicho a Memoria Fisica
	if ((stat = almacenarBytes(pbyte_al->pid, pbyte_al->page, pbyte_al->offset, pbyte_al->size, pbyte_al->bytes)) != 0){
		log_error(logTrace,"fallo el guardado de bytes en mem fisica. status: %d",stat);
		fprintf(stderr, "Fallo el guardado de bytes en Memoria Fisica. status: %d\n", stat);
		return stat;
	}

	return 0;
}

#include <stdbool.h>
#include <netdb.h>

#include <commons/string.h>

#include <tiposRecursos/tiposPaquetes.h>
#include <funcionesCompartidas/funcionesCompartidas.h>
#include <funcionesPaquetes/funcionesPaquetes.h>
#include <tiposRecursos/tiposErrores.h>

#include "funcionesAnsisop.h"

extern bool termino;
extern AnSISOP_funciones functions;
extern int pag_size;
char *eliminarWhitespace(char *string);

void setupCPUFunciones(void){
	functions.AnSISOP_definirVariable		= definirVariable;
	functions.AnSISOP_obtenerPosicionVariable= obtenerPosicionVariable;
	functions.AnSISOP_finalizar 				= finalizar;
	functions.AnSISOP_dereferenciar			= dereferenciar;
	functions.AnSISOP_asignar				= asignar;
	functions.AnSISOP_asignarValorCompartida = asignarValorCompartida;
	functions.AnSISOP_irAlLabel				= irAlLabel;
	functions.AnSISOP_llamarSinRetorno		= llamarSinRetorno;
	functions.AnSISOP_llamarConRetorno		= llamarConRetorno;
	functions.AnSISOP_retornar				= retornar;
	functions.AnSISOP_obtenerValorCompartida = obtenerValorCompartida;
}

void setupCPUFuncionesKernel(void){
	kernel_functions.AnSISOP_wait 					= wait;
	kernel_functions.AnSISOP_signal					= signal;
	kernel_functions.AnSISOP_abrir					= abrir;
	kernel_functions.AnSISOP_borrar					= borrar;
	kernel_functions.AnSISOP_cerrar					= cerrar;
	kernel_functions.AnSISOP_escribir				= escribir;
	kernel_functions.AnSISOP_leer					= leer;
	kernel_functions.AnSISOP_liberar				= liberar;
	kernel_functions.AnSISOP_moverCursor			= moverCursor;
	kernel_functions.AnSISOP_reservar				= reservar;
}

void obtenerUltimoEnStack(t_list *stack, int *pag, int *off, int *size){

	*pag = *off = *size = 0;
	indiceStack* ultimoStack = list_get(stack, pcb->contextoActual);

	if (!list_size(stack)) // es decir que es el StackVacio()
		return;

	posicionMemoria*   ultimoArg = list_get (ultimoStack->args, list_size(ultimoStack->args)-1);
	posicionMemoriaId* ultimaVar = list_get (ultimoStack->vars, list_size(ultimoStack->vars)-1);

	if (ultimoArg == NULL && ultimaVar == NULL)
		return;

	if (ultimoArg == NULL){
		SET_VAR_OPS(ultimaVar->pos, *off, *pag, *size);
		return;

	} else if (ultimaVar == NULL){
		SET_ARG_OPS(ultimoArg, *off, *pag, *size);
		return;

	} else if(ultimoArg->pag > ultimaVar->pos.pag){
		SET_ARG_OPS(ultimoArg, *off, *pag, *size);
		return;

	} else if(ultimoArg->pag < ultimaVar->pos.pag){
		SET_VAR_OPS(ultimaVar->pos, *off, *pag, *size);
		return;

	} else if(ultimoArg->offset > ultimaVar ->pos.offset){
		SET_ARG_OPS(ultimoArg, *off, *pag, *size);
		return;
	}

	SET_VAR_OPS(ultimaVar->pos, *off, *pag, *size);
}

void obtenerVariable(t_nombre_variable variable, posicionMemoria* pm, indiceStack* stack){
	int i;
	posicionMemoriaId* var;
	for(i=0; i < list_size(stack->vars); i++){
		var = list_get(stack->vars, i);
		if(var->id == variable){
			pm->offset = var->pos.offset;
			pm->pag    = var->pos.pag;
			pm->size   = var->pos.size;
			return;
		}
	}
	pm = NULL;
}

//FUNCIONES DE ANSISOP
t_puntero definirVariable(t_nombre_variable variable) {
	printf("definir la variable %c\n", variable);

	indiceStack* ult_stack;
	int pag,
		off,
		size;
	obtenerUltimoEnStack(pcb->indiceDeStack, &pag, &off, &size);
	posicionMemoriaId* var = malloc(sizeof(posicionMemoriaId));
	var->id = variable;
	var->pos.offset= off + size; // todo: arreglar para que off y pag no se vayan del tamanio maximo de pagina (ej: off > pag_size)
	var->pos.pag = pag;
	var->pos.size = sizeof (t_valor_variable);

	printf("La variable '%c' se define en (p,o,s) %d, %d, %d\n", variable, var->pos.pag, var->pos.offset, var->pos.size);

	if (list_size(pcb->indiceDeStack) == 0){
		ult_stack = crearStackVacio();
		list_add(ult_stack->vars, var);
		list_add(pcb->indiceDeStack, ult_stack);

	} else {
		ult_stack = list_get(pcb->indiceDeStack, pcb->contextoActual);
		list_add(ult_stack->vars, var);
	}

	return (pag + pcb->paginasDeCodigo) * pag_size + off;
}

t_puntero obtenerPosicionVariable(t_nombre_variable variable){
	printf("Obtener posicion de %c\n", variable);

	int i;
	indiceStack* stack = list_get(pcb->indiceDeStack, pcb->contextoActual);
	posicionMemoria pm;

	posicionMemoriaId* var;
	for(i = 0; i < list_size(stack->vars); i++){
		var = list_get(stack->vars, i);
		if(var->id == variable){
			pm.offset = var->pos.offset;
			pm.pag    = var->pos.pag;
			pm.size   = var->pos.size;
			break;
		}
	}

	return (var->id != variable)? VAR_NOT_FOUND : pm.pag * pag_size + pm.offset;
}

void finalizar(void){
	printf("Finalizar\n");
	if(pcb->contextoActual==0){
		termino = true;
		return;
	}
	indiceStack* stackActual = list_get(pcb->indiceDeStack, pcb->contextoActual);
	int i;
	for(i=0 ; list_size(stackActual->args) ; i++){
		posicionMemoria* arg = list_get(stackActual->args, i);
		free(arg); // se liberan los argumentos
	}

	for(i=0 ; list_size(stackActual->vars) ; i++){
		posicionMemoriaId* var = list_get(stackActual->vars, i);
		free(var);//se liberan las variables
	}
	free(stackActual);
	list_remove(pcb->indiceDeStack, pcb->contextoActual);
	pcb->contextoActual--;
	indiceStack* nuevoContexto=list_get(pcb->indiceDeStack,pcb->contextoActual);
	pcb->pc=nuevoContexto->retPos;
}

t_valor_variable dereferenciar(t_puntero puntero) {

	t_valor_variable var;
	tPackByteReq pbr;
	tPackBytes* bytes;
	int pack_size, stat;
	char *byterq_serial, *bytes_serial;
	tPackHeader h = {.tipo_de_proceso = CPU, .tipo_de_mensaje = BYTES};

	memcpy(&pbr.head, &h, HEAD_SIZE);
	pbr.pid    = pcb->id;
	pbr.page   = puntero / pag_size + pcb->paginasDeCodigo;
	pbr.offset = puntero % pag_size;
	pbr.size   = sizeof(t_puntero);

	if ((byterq_serial = serializeByteRequest(&pbr, &pack_size)) == NULL){
		puts("Fallo serializacion Pedido de Bytes");
		return FALLO_SERIALIZAC;
	}

	if((stat = send(sock_mem, byterq_serial, pack_size, 0)) == -1){
		perror("Fallo send de byte request. error");
		// bool algo_fallo = true;
		return FALLO_SEND;
	}

	if ((stat = recv(sock_mem, &h, HEAD_SIZE, 0)) == -1){
		perror("Fallo recepcion de header desde Memoria. error");
		return FALLO_RECV;
	}

	if ((bytes_serial = recvGeneric(sock_mem)) == NULL){
		puts("Fallo recepcion generica desde Memoria");
		return FALLO_GRAL;
	}

	if ((bytes = deserializeBytes(bytes_serial)) == NULL){
		puts("Fallo deserializacion de Bytes");
		return FALLO_DESERIALIZAC;
	}

	memcpy(&var, bytes->bytes, sizeof(var));
	printf("Dereferenciar %d y su valor es: %d\n", puntero, var);
	return var;
}

void asignar(t_puntero puntero, t_valor_variable variable) {
	printf("Asignando en %d el valor %d\n", puntero, variable);

	tPackByteAlmac pbal;
	int pack_size, stat;
	tPackHeader h = {.tipo_de_proceso = CPU, .tipo_de_mensaje = ALMAC_BYTES};

	memcpy(&pbal.head, &h, HEAD_SIZE);
	pbal.pid    = pcb->id;
	pbal.page   = puntero / pag_size + pcb->paginasDeCodigo;
	pbal.offset = puntero % pag_size;
	pbal.size   = sizeof (t_valor_variable);
	pbal.bytes  = (char *) &variable;

	pack_size = 0;
	char *byteal_serial = serializeByteAlmacenamiento(&pbal, &pack_size);
	if((stat = send(sock_mem, byteal_serial, pack_size, 0)) == -1){
		perror("Fallo send de byte request. error");
		// bool algo_fallo = true;
		freeAndNULL((void **) &byteal_serial);
		return;
	}

	//freeAndNULL((void **) &byteal_serial); //todo: rompe
}

t_valor_variable asignarValorCompartida(t_nombre_compartida variable, t_valor_variable valor){
// `variable' llega con un '\0' al final
	printf("Asignando en %s el valor %d\n", variable, valor);

	char *valor_serial;
	int pack_size, stat;

	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = SET_GLOBAL};
	pack_size = 0;
	if ((valor_serial = serializeValorYVariable(head, valor, variable, &pack_size)) == NULL){
		puts("No se pudo serializar el valor y variable");
		return 0xFFFF; // no se me ocurre algo mejor que retornar un valor bien power
	}

	if ((stat = send(sock_kern, valor_serial, pack_size, 0)) == -1){
		perror("No se pudo enviar el paquete de Valor y Variable a Kernel. error");
		return 0xFFFF; // no se me ocurre algo mejor que retornar un valor bien power
	}

	free(valor_serial);
	return valor;
}

void irAlLabel (t_nombre_etiqueta etiqueta){
	char* label = eliminarWhitespace(etiqueta);
	printf("Se va al label %s\n", label);

	pcb->pc = metadata_buscar_etiqueta(label, pcb->indiceDeEtiquetas, pcb->etiquetas_size);
	pcb->pc--; // es porque ejecutarInstruccion() incrementa el pc. Si, esto es efectivamente un asco
	free(label);
}

void llamarSinRetorno (t_nombre_etiqueta etiqueta){
	printf("Se llama a la funcion %s\n", etiqueta);
	//uint32_t tamlineaStack = sizeof(uint32_t) + 2*sizeof(t_list) + sizeof(posicionMemoria);
	indiceStack *nuevoStack = crearStackVacio();
	//pcb->etiquetaSize = tamlineaStack;
	list_add(pcb->indiceDeStack, nuevoStack);
	pcb->contextoActual++;

	irAlLabel(etiqueta);
}

void llamarConRetorno (t_nombre_etiqueta etiqueta, t_puntero donde_retornar){
	printf("Se llama a la funcion %s y se guarda el retorno\n", etiqueta);
	uint32_t tamlineaStack = sizeof(uint32_t) + 2*sizeof(t_list) + sizeof(posicionMemoria);
	//posicionMemoria* varRetorno = malloc(sizeof(posicionMemoria));
	//varRetorno->pag=donde_retornar/tamPagina
	//varRetorno->offset = donde_retornar%tamPagina
	indiceStack *nuevoStack = crearStackVacio();
	//nuevoStack->retVar = varRetorno;
	pcb->cantidad_etiquetas = tamlineaStack; // todo: revisar correctitud de esto
	list_add(pcb->indiceDeStack, nuevoStack);
	pcb->contextoActual++;
	irAlLabel(etiqueta);
}

void retornar (t_valor_variable retorno){
	int contextoActual= pcb->contextoActual;
	indiceStack* stackActual = list_get(pcb->indiceDeStack,contextoActual);
	int i;
	for(i=0 ; list_size(stackActual->args) ; i++){
		posicionMemoria* argumento = list_get(stackActual->args, i);
		free(argumento); // se liberan los argumentos
	}

	for(i=0 ; list_size(stackActual->vars) ; i++){
		posicionMemoria* var = list_get(stackActual->vars, i);
		free(var); // se liberan las variables
	}
	posicionMemoria retVar = stackActual->retVar;
	t_puntero direcVariable = (retVar.pag) + retVar.offset; // TODO: la pag habria que dividirla por el tam de la pagina (Propuesta: obtener frame_size en el handshake con Memoria, como hace el Kernel)
	asignar(direcVariable,retorno);
	pcb->pc = stackActual->retPos;
	free(stackActual);
	list_remove(pcb->indiceDeStack,pcb->contextoActual);
	pcb->contextoActual--;
}

t_valor_variable obtenerValorCompartida (t_nombre_compartida variable){
	printf("Se obtiene el valor de la variable compartida %s\n", variable);

	t_valor_variable valor;
	tPackValComp *val_var;
	int pack_size, stat, var_len;
	char *var_serial, *var;
	var = eliminarWhitespace(variable);
	var_len = strlen(var) + 1;

	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = GET_GLOBAL};
	pack_size = 0;
	if ((var_serial = serializeBytes(head, var, var_len, &pack_size)) == NULL){
		puts("No se pudo serializar el valor y variable");
		return 0xFFFF; // no se me ocurre algo mejor que retornar un valor bien power
	}

	if ((stat = send(sock_kern, var_serial, pack_size, 0)) == -1){
		perror("No se pudo enviar el paquete de Valor y Variable a Kernel. error");
		return 0xFFFF; // no se me ocurre algo mejor que retornar un valor bien power
	}
	printf("Se enviaron %d bytes a Kernel\n", stat);
	freeAndNULL((void **) &var_serial);

	if ((stat = recv(sock_kern, &head, HEAD_SIZE, 0)) == -1){
		perror("No se recibir header con Valor Global de Kernel. error");
		return 0xFFFF; // no se me ocurre algo mejor que retornar un valor bien power
	}

	if (head.tipo_de_proceso != KER || head.tipo_de_mensaje != GET_GLOBAL){
		printf("Error de comunicacion. Se recibio Header con: proc %d, msj %d\n",
				head.tipo_de_proceso, head.tipo_de_mensaje);
		return 0xFFFF; // no se me ocurre algo mejor que retornar un valor bien power
	}

	if ((var_serial = recvGeneric(sock_kern)) == NULL){
		perror("No se pudo enviar el paquete de Valor y Variable a Kernel. error");
		return 0xFFFF; // no se me ocurre algo mejor que retornar un valor bien power
	}

	if ((val_var = deserializeValorYVariable(var_serial)) == NULL){
		puts("No se pudo deserializar el valor de la variable.");
		return 0xFFFF; // no se me ocurre algo mejor que retornar un valor bien power
	}

	memcpy(&valor, &val_var->val, sizeof(t_valor_variable));
	free(var_serial);
	free(val_var);
	free(var);
	return valor;
}



//FUNCIONES ANSISOP QUE LE PIDE AL KERNEL
void wait (t_nombre_semaforo identificador_semaforo){
	printf("Se pide al kernel un wait para el semaforo %s\n", identificador_semaforo);

	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = S_WAIT};
	int pack_size = 0;

	char * sem = eliminarWhitespace(identificador_semaforo);
	int lenId = strlen(sem) + 1;
	char *wait_serial;
	if ((wait_serial = serializeBytes(head, sem, lenId, &pack_size)) == NULL){
		puts("No se pudo serializar el semaforo de wait");
		return;
	}

	enviar(wait_serial, pack_size);
	free(wait_serial);
	free(sem);
}

void signal (t_nombre_semaforo identificador_semaforo){
	printf("Se pide al kernel un signal para el semaforo %s\n", identificador_semaforo);

	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = S_SIGNAL};
	int pack_size = 0;

	char * sem = eliminarWhitespace(identificador_semaforo);
	int lenId = strlen(sem) + 1;
	char *sig_serial;
	if ((sig_serial = serializeBytes(head, sem, lenId, &pack_size)) == NULL){
		puts("No se pudo serializar el semaforo de signal");
		return;
	}

	enviar(sig_serial, pack_size);
	free(sig_serial);
	free(sem);
}

void liberar (t_puntero puntero){
	printf("Se pide al kernel liberar memoria. Inicio: %d\n", puntero);

	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = LIBERAR};
	int pack_size = 0;

	char *free_serial;
	if ((free_serial = serializeBytes(head, (char*) &puntero, sizeof puntero, &pack_size)) == NULL){
		puts("No se pudo serializar el semaforo de signal");
		return;
	}

	enviar(free_serial, pack_size);
}

t_descriptor_archivo abrir(t_direccion_archivo direccion, t_banderas flags){
	printf("Se pide al kernel abrir el archivo %s\n", direccion);

	int pack_size = 0;

	char *abrir_serial;
	if ((abrir_serial = serializeAbrir(direccion, flags, &pack_size)) == NULL){
		puts("No se pudo serializar el semaforo de signal");
		return FALLO_SERIALIZAC;
	}

	enviar(abrir_serial, pack_size);
	return 10;
}

void borrar (t_descriptor_archivo direccion){
	printf("Se pide al kernel borrar el archivo %d\n", direccion);

	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = BORRAR};
	int pack_size = 0;

	char *borrar_serial;
	if ((borrar_serial = serializeBytes(head, (char*) &direccion, sizeof direccion, &pack_size)) == NULL){
		puts("No se pudo serializar el semaforo de signal");
		return;
	}

	enviar(borrar_serial, pack_size);
}

void cerrar (t_descriptor_archivo descriptor_archivo){
	printf("Se pide al kernel cerrar el archivo %d\n", descriptor_archivo);

	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = CERRAR};
	int pack_size = 0;

	char *cerrar_serial;
	if ((cerrar_serial = serializeBytes(head, (char*) &descriptor_archivo, sizeof descriptor_archivo, &pack_size)) == NULL){
		puts("No se pudo serializar el semaforo de signal");
		return;
	}

	enviar(cerrar_serial, pack_size);
}

void moverCursor (t_descriptor_archivo descriptor_archivo, t_valor_variable posicion){
	printf("Se pide al kernel mover el archivo %d a la posicion %d\n", descriptor_archivo, posicion);

	int pack_size = 0;

	char *mov_serial;
	if ((mov_serial = serializeMoverCursor(descriptor_archivo, posicion, &pack_size)) == NULL){
		puts("No se pudo serializar la posicion a mover el cursor");
		return;
	}

	enviar(mov_serial, pack_size);
}

void escribir (t_descriptor_archivo descriptor_archivo, void* informacion, t_valor_variable tamanio){
	printf("Se pide al kernel escribir el archivo %d con la informacion %s, cantidad de bytes: %d\n", descriptor_archivo, (char*)informacion, tamanio);

	int pack_size = 0;

	char *esc_serial;
	if ((esc_serial = serializeEscribir(descriptor_archivo, informacion, tamanio, &pack_size)) == NULL){
		puts("No se pudo serializar la posicion a mover el cursor");
		return;
	}

	enviar(esc_serial, pack_size);
}


void leer (t_descriptor_archivo descriptor_archivo, t_puntero informacion, t_valor_variable tamanio){
	printf("Se pide al kernel leer el archivo %d, se guardara en %d, cantidad de bytes: %d\n", descriptor_archivo, informacion, tamanio);

	int pack_size = 0;

	char *leer_serial;
	if ((leer_serial = serializeLeer(descriptor_archivo, informacion, tamanio, &pack_size)) == NULL){
		puts("No se pudo serializar la posicion a mover el cursor");
		return;
	}

	enviar(leer_serial, pack_size);
}

t_puntero reservar (t_valor_variable espacio){ // todo: ya casi esta
	printf("Se pide al kernel reservar %d espacio de memoria\n", espacio);

	int stat;
	char *ptr_serial;
	tPackVal *val;
	tPackHeader head;

	val = malloc(sizeof *val);
	val->head.tipo_de_proceso = CPU; val->head.tipo_de_mensaje = RESERVAR;
	val->val = espacio;

	int pack_size = 0;
	char *reserva_serial;
	if ((reserva_serial = serializeVal(val, &pack_size)) == NULL){
		puts("No se pudo serializar el semaforo de signal");
		return FALLO_SERIALIZAC;
	}
	freeAndNULL((void **) &val);

	enviar(reserva_serial, pack_size);
	if ((stat = recv(sock_kern, &head, HEAD_SIZE, 0)) == -1){
		perror("Fallo recv de puntero alojado de Kernel. error");
		// todo: setear error del err_handler y etc
		return -1;
	}

	ptr_serial = recvGeneric(sock_kern);
	val = deserializeVal(ptr_serial);

	return val->val;
}

void enviar(char *op_kern_serial, int pack_size){
	int stat;
	if ((stat = send(sock_kern, op_kern_serial, pack_size, 0)) == -1){
		perror("No se pudo enviar la operacion privilegiada a Kernel. error");
		return;
	}
	printf("Se enviaron %d bytes a Kernel\n", stat);
}

char *eliminarWhitespace(char *string){

	int var_len;
	char *var = NULL;

	var_len = strlen(string);
	if (string_ends_with(string, "\n") || string_ends_with(string, "\t")){
		var = malloc(var_len);
		memcpy(var, string, var_len);
		var[var_len - 1] = '\0';
		return var;
	}
	var = malloc(var_len + 1);
	memcpy(var, string, var_len + 1);
	var[var_len] = '\0';
	return var;
}

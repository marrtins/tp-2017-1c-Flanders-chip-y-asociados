/*
 * Implementa las cosas que hay en defsKernel.h
 */

#include "defsKernel.h"
#include "capaMemoria.h"
#include "auxiliaresKernel.h"
#include "funcionesSyscalls.h"
#include "planificador.h"
#include "manejadores.h"

void setupVariablesPorSubsistema(void){

	setupGlobales_manejadores();
	setupGlobales_capaMemoria();
	setupGlobales_auxiliares();
	setupGlobales_syscalls();
	setupGlobales_planificador();
 // todo: verificar que esten todos los mutexes y demas cosas inicilizados..
	// creo que ya estan todos
}

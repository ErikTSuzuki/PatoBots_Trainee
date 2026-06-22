#ifndef ROBOT_STATES_H_
#define ROBOT_STATES_H_

typedef enum {
<<<<<<< HEAD
=======
    ESTRATEGIA_PARADO,
    ESTRATEGIA_FRENTE_TOTAL,  // Botão 2: Anda reto e empurra
    ESTRATEGIA_PERCORRER_BORDA, // Botão 3: Contorna a arena para pegar o lado do rival
} EstrategiaAutonoma;

typedef enum {
	MODO_DESATIVADO,
>>>>>>> upstream/main
    MODO_RC,
    MODO_AUTONOMO
} ModoRobo;

<<<<<<< HEAD
extern ModoRobo modo_atual;

#endif /* ROBOT_STATES_H_ */
=======
typedef enum {
    STARTUP_IDLE = 0,    // Não iniciou
    STARTUP_COUNTING,    // Contando 5s
    STARTUP_DONE,        // Pronto para ativar
} StartupFase;

extern ModoRobo modo_atual;
extern EstrategiaAutonoma estrategia_atual;
extern StartupFase startup_fase;

#endif /* ROBOT_STATES_H_ */
>>>>>>> upstream/main

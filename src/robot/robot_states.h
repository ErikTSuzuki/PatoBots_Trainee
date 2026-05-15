#ifndef ROBOT_STATES_H_
#define ROBOT_STATES_H_

typedef enum {
    MODO_RC,
    MODO_AUTONOMO
} ModoRobo;

extern ModoRobo modo_atual;

#endif /* ROBOT_STATES_H_ */

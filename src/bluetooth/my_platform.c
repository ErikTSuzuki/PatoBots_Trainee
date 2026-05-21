// Example file - Public Domain
// Need help? https://tinyurl.com/bluepad32-help

#include <string.h>
#include <uni.h>
#include "bt_control.h"
#include "robot_states.h"
#include "config.h"

typedef struct my_platform_instance_s {
    uni_gamepad_seat_t gamepad_seat;
} my_platform_instance_t;

static void trigger_event_on_gamepad(uni_hid_device_t* d);
static my_platform_instance_t* get_my_platform_instance(uni_hid_device_t* d);

static void my_platform_init(int argc, const char** argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    logi("custom: init()\n");
}

static void my_platform_on_init_complete(void) {
    logi("custom: on_init_complete()\n");

    uni_bt_start_scanning_and_autoconnect_unsafe();
    uni_bt_allow_incoming_connections(true);

    if (1)
        uni_bt_del_keys_unsafe();
    else
        uni_bt_list_keys_unsafe();
}

static uni_error_t my_platform_on_device_discovered(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi) {
    if (((cod & UNI_BT_COD_MINOR_MASK) & UNI_BT_COD_MINOR_KEYBOARD) == UNI_BT_COD_MINOR_KEYBOARD) {
        logi("Ignoring keyboard\n");
        return UNI_ERROR_IGNORE_DEVICE;
    }
    return UNI_ERROR_SUCCESS;
}

static void my_platform_on_device_connected(uni_hid_device_t* d) {
    logi("custom: device connected: %p\n", d);
}

static void my_platform_on_device_disconnected(uni_hid_device_t* d) {
    logi("custom: device disconnected: %p\n", d);
}

static uni_error_t my_platform_on_device_ready(uni_hid_device_t* d) {
    logi("custom: device ready: %p\n", d);
    my_platform_instance_t* ins = get_my_platform_instance(d);
    ins->gamepad_seat = GAMEPAD_SEAT_A;

    trigger_event_on_gamepad(d);
    return UNI_ERROR_SUCCESS;
}

static void my_platform_on_controller_data(uni_hid_device_t* d, uni_controller_t* ctl) {
    if (ctl->klass != UNI_CONTROLLER_CLASS_GAMEPAD) return;

    uni_gamepad_t* gp = &ctl->gamepad;
    motor_config_t* motor_dir = get_motor_direito();
    motor_config_t* motor_esq = get_motor_esquerdo();

    // Aceleração (throttle right trigger)
    int aceleracao = gp->throttle;

    // Freio
    int freio = -(gp->brake);

    // Direção (analog left X axis)
    int direcao = gp->axis_x;

    // Normaliza e aplica zona morta
    if (direcao > -ZONA_MORTA && direcao < ZONA_MORTA) direcao = 0;

    int raw = aceleracao + freio;
    // Cálculo arcade: mistura aceleração com direção
    int ajuste_curva = (direcao * raw) / 512;

    int vel_direita = raw - ajuste_curva;
    int vel_esquerda = raw + ajuste_curva;

    // Limitação a 0-1023
    if (vel_esquerda > 1023) vel_esquerda = 1023;
    if (vel_esquerda < -1023) vel_esquerda = -1023;
    if (vel_direita > 1023) vel_direita = 1023;
    if (vel_direita < -1023) vel_direita = -1023;

    // Deadzone de aceleração
    if (aceleracao < 20 && freio > -20) {
        vel_direita = 0;
        vel_esquerda = 0;
    }

    motor_set_speed(motor_dir, vel_direita);
    motor_set_speed(motor_esq, vel_esquerda);

    logi("raw: %d | RT: %d  | LT: %d | X: %d | Dir: %d | Esq: %d\n", raw,aceleracao, freio, direcao, vel_direita, vel_esquerda);

    // Troca de modo com botão B
    static bool b_ant = false;
    bool b_pres = (gp->buttons & BUTTON_B);
    if (b_pres && !b_ant) {
        modo_atual = (modo_atual == MODO_RC) ? MODO_AUTONOMO : MODO_RC;
        motor_set_speed(motor_dir, 0);
        motor_set_speed(motor_esq, 0);
        printf("[MODO] %s\n", (modo_atual == MODO_RC) ? "Manual" : "Autonomo");
    }
    b_ant = b_pres;
}

static const uni_property_t* my_platform_get_property(uni_property_idx_t idx) {
    ARG_UNUSED(idx);
    return NULL;
}

static void my_platform_on_oob_event(uni_platform_oob_event_t event, void* data) {
    switch (event) {
        case UNI_PLATFORM_OOB_GAMEPAD_SYSTEM_BUTTON: {
            uni_hid_device_t* d = data;
            if (d == NULL) {
                loge("ERROR: my_platform_on_oob_event: Invalid NULL device\n");
                return;
            }
            logi("custom: on_device_oob_event(): %d\n", event);

            my_platform_instance_t* ins = get_my_platform_instance(d);
            ins->gamepad_seat = ins->gamepad_seat == GAMEPAD_SEAT_A ? GAMEPAD_SEAT_B : GAMEPAD_SEAT_A;

            trigger_event_on_gamepad(d);
            break;
        }

        case UNI_PLATFORM_OOB_BLUETOOTH_ENABLED:
            logi("custom: Bluetooth enabled: %d\n", (bool)(data));
            break;

        default:
            logi("my_platform_on_oob_event: unsupported event: 0x%04x\n", event);
            break;
    }
}

static my_platform_instance_t* get_my_platform_instance(uni_hid_device_t* d) {
    return (my_platform_instance_t*)&d->platform_data[0];
}

static void trigger_event_on_gamepad(uni_hid_device_t* d) {
    my_platform_instance_t* ins = get_my_platform_instance(d);

    if (d->report_parser.play_dual_rumble != NULL) {
        d->report_parser.play_dual_rumble(d, 0, 150, 128, 40);
    }

    if (d->report_parser.set_player_leds != NULL) {
        d->report_parser.set_player_leds(d, ins->gamepad_seat);
    }

    if (d->report_parser.set_lightbar_color != NULL) {
        uint8_t red = (ins->gamepad_seat & 0x01) ? 0xff : 0;
        uint8_t green = (ins->gamepad_seat & 0x02) ? 0xff : 0;
        uint8_t blue = (ins->gamepad_seat & 0x04) ? 0xff : 0;
        d->report_parser.set_lightbar_color(d, red, green, blue);
    }
}

struct uni_platform* get_my_platform(void) {
    static struct uni_platform plat = {
        .name = "custom",
        .init = my_platform_init,
        .on_init_complete = my_platform_on_init_complete,
        .on_device_discovered = my_platform_on_device_discovered,
        .on_device_connected = my_platform_on_device_connected,
        .on_device_disconnected = my_platform_on_device_disconnected,
        .on_device_ready = my_platform_on_device_ready,
        .on_oob_event = my_platform_on_oob_event,
        .on_controller_data = my_platform_on_controller_data,
        .get_property = my_platform_get_property,
    };

    return &plat;
}

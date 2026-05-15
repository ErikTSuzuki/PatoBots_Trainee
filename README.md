# PatoBots Trainee - Robô Sumo ESP32

Sistema de controle para robô sumo com ESP32, suportando modo RC (Bluetooth/Xbox) e autônomo.

## Estrutura de Pastas

```
PatoBots_Trainee/
├── main/
│   ├── main.c                 # Ponto de entrada da aplicação
│   ├── CMakeLists.txt         # Configuração de build
│   ├── Kconfig.projbuild      # Kconfig ESP-IDF
│   └── idf_component.yml      # Definição do componente IDF
│
├── src/
│   ├── utils/
│   │   └── config.h           # Configurações centralizadas (pinos, constantes)
│   │
│   ├── hardware/
│   │   ├── motor/
│   │   │   ├── motor.h        # Interface de controle de motores
│   │   │   └── motor.c        # Implementação: PWM e direção
│   │   └── sensor/
│   │       ├── sensor_distancia.h  # Interface de leitura de sensores
│   │       └── sensor_distancia.c  # Implementação: ADC e calibração
│   │
│   ├── robot/
│   │   ├── robot_states.h     # Estados do robô (RC vs Autônomo)
│   │   ├── robot_task.h       # Interface da task do robô
│   │   └── robot_task.c       # Lógica de combate autônoma
│   │
│   └── bluetooth/
│       ├── bt_control.h       # Interface de controle Bluetooth
│       └── my_platform.c      # Callbacks do Bluepad32
│
├── components/
│   └── bluepad32/             # Componente externo de Bluetooth
│
├── build/                     # Diretório de build (gerado)
├── CMakeLists.txt
├── sdkconfig
└── README.md
```

## Benefícios da Nova Organização

### 1. **Separação de Responsabilidades**
- **hardware/** - Controla a eletrônica (motores, sensores)
- **robot/** - Implementa a lógica de combate
- **bluetooth/** - Gerencia comunicação com controle
- **utils/** - Configurações compartilhadas

### 2. **Reutilizabilidade**
- Código de motor pode ser extraído para outro projeto
- Sensor pode ser usado em diferentes contextos
- Lógica de combate é independente de hardware

### 3. **Testabilidade**
- Cada módulo pode ser testado isoladamente
- Fácil mockar sensores ou motores para testes

### 4. **Manutenibilidade**
- Mudança de pino? Edite apenas `src/utils/config.h`
- Nova estratégia de combate? Edite apenas `src/robot/robot_task.c`
- Novo sensor? Adicione em `src/hardware/sensor/`

### 5. **Escalabilidade**
- Adicionar novos sensores é trivial
- Múltiplos comportamentos autônomos podem coexistir
- Fácil integrar novos componentes ESP-IDF

## Como Compilar

```bash
# Dentro da pasta do projeto
idf.py build

# Ou com monitoramento
idf.py build monitor
```

## Pins Utilizados

Veja `src/utils/config.h` para todas as definições. Principais:

| Função | GPIO |
|--------|------|
| Motor Direito PWM | 18 |
| Motor Direito IN1/IN2 | 19, 21 |
| Motor Esquerdo PWM | 22 |
| Motor Esquerdo IN1/IN2 | 23, 25 |
| Standby | 26 |
| IR Frontal | 34 (ADC1_CH6) |
| IR Direita | 35 (ADC1_CH7) |
| IR Esquerda | 36 (ADC1_CH0) |

## Modos de Operação

### RC (Manual)
- Controle com Xbox/Bluetooth
- Throttle: aceleração
- Analógico esquerdo: direção
- Botão B: alterna para autônomo

### Autônomo
- Lógica automática
- Sensor frontal detecta inimigos
- Ataque agressivo se detectado
- Giração de busca caso contrário

## Próximos Passos

1. Implementar sensores de linha (QTR)
2. Adicionar calibração dinâmica de sensores
3. Múltiplos comportamentos autônomos
4. Sistema de logging
5. Testes unitários

---

**Autor:** Erik Suzuki  
**Último atualizado:** 2026-05-15

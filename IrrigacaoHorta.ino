/*
 * ==========================================================================
 *  SISTEMA DE IRRIGAÇÃO DA HORTA COM REAPROVEITAMENTO DE ÁGUA
 *  Placa: Arduino UNO
 * ==========================================================================
 *
 *  COMO O SISTEMA FUNCIONA
 *  -----------------------
 *  Existem três níveis:
 *    1) Reservatório de ALTA  -> tem a água e a BOMBA DE ALTA PRESSÃO (12V),
 *                                que alimenta os sprinklers da horta.
 *    2) Horta                 -> recebe a água dos sprinklers.
 *    3) Reservatório do MEIO  -> recebe a sobra da horta. Nele ficam o
 *                                SENSOR DE CHUVA (nível) e a BOMBA SUBMERSA (5V),
 *                                que devolve a água para o reservatório de alta.
 *
 *  SEQUÊNCIA DO CICLO (só começa ao apertar o botão):
 *    1. Botão pressionado  -> liga a bomba de alta (sprinklers).
 *    2. Sensor detecta água no reservatório do meio
 *                          -> espera TEMPO_ANTES_DESLIGAR_ALTA.
 *    3. Desliga a bomba de alta.
 *    4. Espera TEMPO_ANTES_LIGAR_SUBMERSA (5 segundos).
 *    5. Liga a bomba submersa (devolve a água para o reservatório de alta).
 *    6. Quando o sensor NÃO detecta mais água -> desliga a submersa.
 *    7. Ciclo completo. O sistema fica parado esperando o botão de novo.
 *
 *  SEGURANÇA EXTRA:
 *    - Apertar o botão DURANTE o ciclo desliga tudo (parada de emergência).
 *    - Cada bomba tem um tempo máximo de funcionamento. Se passar disso,
 *      tudo desliga (evita bomba queimar a seco ou transbordar se o
 *      sensor falhar).
 *
 *  LIGAÇÕES (pode mudar os pinos abaixo, se quiser)
 *  ------------------------------------------------
 *    Botão            -> pino 2 e GND  (usa o resistor interno, sem resistor externo)
 *    Sensor de chuva  -> saída DO no pino 3, VCC no 5V, GND no GND
 *    Relé bomba ALTA  -> IN no pino 7
 *    Relé SUBMERSA    -> IN no pino 8
 *    LED de status    -> LED da própria placa (pino 13)
 * ==========================================================================
 */

// ----------------------------- PINOS -------------------------------------
const int PINO_BOTAO          = 2;   // Botão de início (outro lado no GND)
const int PINO_SENSOR_CHUVA   = 3;   // Saída digital (DO) do sensor de chuva
const int PINO_RELE_ALTA      = 7;   // Relé da bomba de alta pressão (12V)
const int PINO_RELE_SUBMERSA  = 8;   // Relé da bomba submersa (5V)
const int PINO_LED            = 13;  // LED da placa: aceso = ciclo rodando

// --------------------------- CONFIGURAÇÕES -------------------------------
// A maioria dos módulos de relé para Arduino liga com sinal LOW (nível baixo).
// Se o seu relé ligar com HIGH, troque para:  RELE_LIGADO = HIGH, RELE_DESLIGADO = LOW
const int RELE_LIGADO    = LOW;
const int RELE_DESLIGADO = HIGH;

// O sensor de chuva comum (placa + módulo com LM393) manda LOW quando está
// MOLHADO. Se o seu funcionar ao contrário, troque para HIGH.
const int SENSOR_MOLHADO = LOW;

// Tempos (em milissegundos: 1000 ms = 1 segundo)
const unsigned long TEMPO_ANTES_DESLIGAR_ALTA  = 2000UL;    // espera após detectar água, antes de desligar a alta
const unsigned long TEMPO_ANTES_LIGAR_SUBMERSA = 5000UL;    // espera entre desligar a alta e ligar a submersa (5 s)
const unsigned long TEMPO_CONFIRMA_SENSOR      = 1000UL;    // sensor precisa ficar estável esse tempo (evita respingos)

// Tempos máximos de segurança (ajuste conforme o tamanho dos reservatórios)
const unsigned long TEMPO_MAX_BOMBA_ALTA     = 10UL * 60UL * 1000UL;  // 10 minutos
const unsigned long TEMPO_MAX_BOMBA_SUBMERSA = 10UL * 60UL * 1000UL;  // 10 minutos

const unsigned long TEMPO_DEBOUNCE_BOTAO = 50UL;  // filtro do ruído mecânico do botão

// ------------------------- ESTADOS DO CICLO ------------------------------
// O programa funciona como uma "máquina de estados": em cada momento ele
// está em UMA etapa do ciclo e só passa para a próxima quando a condição
// daquela etapa é cumprida.
enum Estado {
  PARADO,                  // esperando o botão
  IRRIGANDO,               // bomba de alta ligada, esperando o sensor detectar água
  ESPERANDO_DESLIGAR_ALTA, // água detectada, aguardando para desligar a alta
  ESPERANDO_SUBMERSA,      // alta desligada, aguardando 5 s para ligar a submersa
  RETORNANDO_AGUA          // submersa ligada, esperando o sensor secar
};

Estado estadoAtual = PARADO;
unsigned long inicioEstado = 0;   // momento (millis) em que entrou no estado atual

// Variáveis do botão (debounce)
int leituraBotaoAnterior = HIGH;
int estadoBotaoEstavel   = HIGH;
unsigned long ultimaMudancaBotao = 0;

// Variáveis do sensor (filtro de estabilidade)
bool sensorMolhadoEstavel = false;
bool ultimaLeituraSensor  = false;
unsigned long ultimaMudancaSensor = 0;

// ======================== FUNÇÕES AUXILIARES =============================

// Liga ou desliga a bomba de alta pressão
void bombaAlta(bool ligar) {
  digitalWrite(PINO_RELE_ALTA, ligar ? RELE_LIGADO : RELE_DESLIGADO);
}

// Liga ou desliga a bomba submersa
void bombaSubmersa(bool ligar) {
  digitalWrite(PINO_RELE_SUBMERSA, ligar ? RELE_LIGADO : RELE_DESLIGADO);
}

// Troca de estado, guarda o horário e mostra no Monitor Serial
void mudarEstado(Estado novoEstado, const char* mensagem) {
  estadoAtual  = novoEstado;
  inicioEstado = millis();
  Serial.println(mensagem);
}

// Tempo (ms) desde que entrou no estado atual
unsigned long tempoNoEstado() {
  return millis() - inicioEstado;
}

// Desliga tudo e volta ao estado PARADO
void pararTudo(const char* motivo) {
  bombaAlta(false);
  bombaSubmersa(false);
  digitalWrite(PINO_LED, LOW);
  mudarEstado(PARADO, motivo);
}

// Retorna true UMA vez quando o botão é apertado (já com debounce)
bool botaoFoiPressionado() {
  int leitura = digitalRead(PINO_BOTAO);
  bool pressionou = false;

  if (leitura != leituraBotaoAnterior) {
    ultimaMudancaBotao = millis();       // houve mudança: reinicia a contagem
  }

  if (millis() - ultimaMudancaBotao > TEMPO_DEBOUNCE_BOTAO) {
    if (leitura != estadoBotaoEstavel) {
      estadoBotaoEstavel = leitura;
      if (estadoBotaoEstavel == LOW) {   // LOW = apertado (usa INPUT_PULLUP)
        pressionou = true;
      }
    }
  }

  leituraBotaoAnterior = leitura;
  return pressionou;
}

// Atualiza a leitura do sensor: só aceita a mudança se ela ficar
// estável por TEMPO_CONFIRMA_SENSOR (evita respingos e ruído)
void atualizarSensor() {
  bool leitura = (digitalRead(PINO_SENSOR_CHUVA) == SENSOR_MOLHADO);

  if (leitura != ultimaLeituraSensor) {
    ultimaMudancaSensor = millis();
    ultimaLeituraSensor = leitura;
  }

  if (millis() - ultimaMudancaSensor >= TEMPO_CONFIRMA_SENSOR) {
    sensorMolhadoEstavel = leitura;
  }
}

// ============================== SETUP ====================================
void setup() {
  Serial.begin(9600);

  // Primeiro coloca os relés em "desligado" e só depois define como saída,
  // assim as bombas não dão um pulso ao ligar o Arduino.
  digitalWrite(PINO_RELE_ALTA, RELE_DESLIGADO);
  digitalWrite(PINO_RELE_SUBMERSA, RELE_DESLIGADO);
  pinMode(PINO_RELE_ALTA, OUTPUT);
  pinMode(PINO_RELE_SUBMERSA, OUTPUT);

  pinMode(PINO_BOTAO, INPUT_PULLUP);   // resistor interno: solto = HIGH, apertado = LOW
  pinMode(PINO_SENSOR_CHUVA, INPUT);
  pinMode(PINO_LED, OUTPUT);
  digitalWrite(PINO_LED, LOW);

  Serial.println("Sistema pronto. Aperte o botao para iniciar o ciclo.");
}

// =============================== LOOP ====================================
void loop() {
  atualizarSensor();
  bool apertou = botaoFoiPressionado();

  // Parada de emergência: botão apertado com o ciclo em andamento
  if (apertou && estadoAtual != PARADO) {
    pararTudo("Botao pressionado durante o ciclo: PARADA DE EMERGENCIA.");
    return;
  }

  switch (estadoAtual) {

    // ---------------------------------------------------------------
    case PARADO:
      // Só começa quando o botão for apertado
      if (apertou) {
        digitalWrite(PINO_LED, HIGH);
        bombaAlta(true);
        mudarEstado(IRRIGANDO, "Ciclo iniciado: bomba de ALTA ligada (irrigando).");
      }
      break;

    // ---------------------------------------------------------------
    case IRRIGANDO:
      // Espera a água chegar no reservatório do meio
      if (sensorMolhadoEstavel) {
        mudarEstado(ESPERANDO_DESLIGAR_ALTA, "Agua detectada no reservatorio do meio. Aguardando para desligar a ALTA...");
      }
      // Segurança: bomba de alta ligada tempo demais
      else if (tempoNoEstado() > TEMPO_MAX_BOMBA_ALTA) {
        pararTudo("ERRO: tempo maximo da bomba de ALTA atingido. Verifique o sensor/reservatorio.");
      }
      break;

    // ---------------------------------------------------------------
    case ESPERANDO_DESLIGAR_ALTA:
      if (tempoNoEstado() >= TEMPO_ANTES_DESLIGAR_ALTA) {
        bombaAlta(false);
        mudarEstado(ESPERANDO_SUBMERSA, "Bomba de ALTA desligada. Aguardando 5 s para ligar a SUBMERSA...");
      }
      break;

    // ---------------------------------------------------------------
    case ESPERANDO_SUBMERSA:
      if (tempoNoEstado() >= TEMPO_ANTES_LIGAR_SUBMERSA) {
        bombaSubmersa(true);
        mudarEstado(RETORNANDO_AGUA, "Bomba SUBMERSA ligada: devolvendo agua para o reservatorio de alta.");
      }
      break;

    // ---------------------------------------------------------------
    case RETORNANDO_AGUA:
      // Desliga quando o sensor não detectar mais água
      if (!sensorMolhadoEstavel) {
        bombaSubmersa(false);
        digitalWrite(PINO_LED, LOW);
        mudarEstado(PARADO, "Sensor seco: bomba SUBMERSA desligada. CICLO COMPLETO. Aperte o botao para novo ciclo.");
      }
      // Segurança: submersa ligada tempo demais (evita queimar a seco)
      else if (tempoNoEstado() > TEMPO_MAX_BOMBA_SUBMERSA) {
        pararTudo("ERRO: tempo maximo da bomba SUBMERSA atingido. Verifique o sensor.");
      }
      break;
  }
}

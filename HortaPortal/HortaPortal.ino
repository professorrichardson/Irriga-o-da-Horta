/* ESP32 DevKit V1 / ESP32-WROOM-32.
 * Baseado no projeto IrrigacaoHorta fornecido. Consulte o manual antes de ligar.
 * Nomes comuns impressos: D18, D19, D25, D26 (algumas placas usam só o número).
 * Relés ativos em LOW, exigem interface compatível com lógica de 3,3 V.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "Portal.h"

const uint8_t PINO_BOTAO=18, PINO_SENSOR=19, PINO_ALTA=25, PINO_SUBMERSA=26;
const int RELE_LIGADO=LOW, RELE_DESLIGADO=HIGH, SENSOR_MOLHADO=LOW;
const uint32_t CONFIRMA_SENSOR=1000, DEBOUNCE=50;
const uint32_t ESPERA_ALTA=2000, ESPERA_SUBMERSA=5000;
const uint32_t MAX_ALTA=600000, MAX_SUBMERSA=600000;
const char* AP_SSID="Horta";
const char* AP_PASSWORD="Horta2026"; // Troque; mínimo de 8 caracteres.
IPAddress apIP(192,168,4,1), mask(255,255,255,0);
WebServer server(80);
DNSServer dns;
enum Estado {PARADO, IRRIGANDO, ESPERANDO_DESLIGAR_ALTA, ESPERANDO_SUBMERSA, RETORNANDO_AGUA};
Estado estado=PARADO;
uint32_t inicioEstado=0,inicioCiclo=0,inicioAlta=0,inicioSubmersa=0;
uint32_t tempoAlta=0,tempoSubmersa=0,tempoCiclo=0;
bool alta=false,submersa=false,sensorEstavel=false,ultimaLeitura=false;
uint32_t mudancaSensor=0,mudancaBotao=0;
int botaoAnterior=HIGH,botaoEstavel=HIGH;
const char* resultado="Nenhum ciclo iniciado.";

void acionarAlta(bool ligar){alta=ligar;digitalWrite(PINO_ALTA,ligar?RELE_LIGADO:RELE_DESLIGADO);}
void acionarSubmersa(bool ligar){submersa=ligar;digitalWrite(PINO_SUBMERSA,ligar?RELE_LIGADO:RELE_DESLIGADO);}
void mudar(Estado e){estado=e;inicioEstado=millis();}
void parar(const char* motivo){
 uint32_t agora=millis();
 if(alta)tempoAlta=agora-inicioAlta;
 if(submersa)tempoSubmersa=agora-inicioSubmersa;
 if(estado!=PARADO)tempoCiclo=agora-inicioCiclo;
 acionarAlta(false);acionarSubmersa(false);resultado=motivo;mudar(PARADO);Serial.println(motivo);
}
void iniciar(){
 if(estado!=PARADO)return; // Idempotente: novo comando não para nem reinicia.
 tempoAlta=tempoSubmersa=tempoCiclo=0;inicioCiclo=inicioAlta=millis();
 acionarSubmersa(false);acionarAlta(true);resultado="Ciclo em andamento.";mudar(IRRIGANDO);
 Serial.println("Ciclo iniciado: alta acionada.");
}
bool lerBotao(){
 int leitura=digitalRead(PINO_BOTAO);uint32_t agora=millis();bool evento=false;
 if(leitura!=botaoAnterior)mudancaBotao=agora;
 if(agora-mudancaBotao>=DEBOUNCE&&leitura!=botaoEstavel){botaoEstavel=leitura;evento=leitura==LOW;}
 botaoAnterior=leitura;return evento;
}
void atualizarCiclo(){
 uint32_t agora=millis();bool leitura=digitalRead(PINO_SENSOR)==SENSOR_MOLHADO;
 if(leitura!=ultimaLeitura){ultimaLeitura=leitura;mudancaSensor=agora;}
 if(agora-mudancaSensor>=CONFIRMA_SENSOR)sensorEstavel=leitura;
 if(lerBotao()){if(estado==PARADO)iniciar();else parar("Interrompido pelo botao fisico.");return;}
 if(alta&&agora-inicioAlta>=MAX_ALTA){parar("Limite de tempo da alta atingido.");return;}
 if(submersa&&agora-inicioSubmersa>=MAX_SUBMERSA){parar("Limite de tempo da submersa atingido.");return;}
 switch(estado){
 case PARADO:break;
 case IRRIGANDO:if(sensorEstavel)mudar(ESPERANDO_DESLIGAR_ALTA);break;
 case ESPERANDO_DESLIGAR_ALTA:
  if(agora-inicioEstado>=ESPERA_ALTA){tempoAlta=agora-inicioAlta;acionarAlta(false);mudar(ESPERANDO_SUBMERSA);}break;
 case ESPERANDO_SUBMERSA:
  if(agora-inicioEstado>=ESPERA_SUBMERSA){inicioSubmersa=agora;acionarSubmersa(true);mudar(RETORNANDO_AGUA);}break;
 case RETORNANDO_AGUA:if(!sensorEstavel)parar("Ciclo concluido.");break;
 }
}
String statusJSON(){
 uint32_t agora=millis();String s;s.reserve(300);
 s="{\"state\":";s+=String((int)estado);
 s+=",\"alta\":";s+=alta?"true":"false";
 s+=",\"sub\":";s+=submersa?"true":"false";
 s+=",\"wet\":";s+=sensorEstavel?"true":"false";
 s+=",\"elapsed\":";s+=String(estado!=PARADO?agora-inicioCiclo:tempoCiclo);
 s+=",\"highMs\":";s+=String(alta?agora-inicioAlta:tempoAlta);
 s+=",\"lowMs\":";s+=String(submersa?agora-inicioSubmersa:tempoSubmersa);
 s+=",\"result\":\"";s+=resultado;s+="\"}";return s;
}
void responderStatus(){server.sendHeader("Cache-Control","no-store");server.send(200,"application/json; charset=utf-8",statusJSON());}
void servirPagina(){server.sendHeader("Cache-Control","no-store");server.send_P(200,"text/html; charset=utf-8",PORTAL_HTML);}
bool comandoPermitido(){
 // Exige cabeçalho que formulários de outros sites não podem enviar.
 // Sem CORS: acesso ao controle é pela rede local e pelo próprio painel.
 if(server.header("X-Horta-Control")!="1"){server.send(403,"application/json","{\"error\":\"Use o painel local\"}");return false;}
 return true;
}
void setup(){
 Serial.begin(115200);
 // O hardware deve garantir relés desligados durante boot/reset.
 pinMode(PINO_ALTA,OUTPUT);pinMode(PINO_SUBMERSA,OUTPUT);acionarAlta(false);acionarSubmersa(false);
 pinMode(PINO_BOTAO,INPUT_PULLUP);pinMode(PINO_SENSOR,INPUT);
 botaoAnterior=botaoEstavel=digitalRead(PINO_BOTAO); // Não inicia por botão já segurado no boot.
 ultimaLeitura=digitalRead(PINO_SENSOR)==SENSOR_MOLHADO;mudancaSensor=millis();
 WiFi.mode(WIFI_AP);
 bool redeOK=WiFi.softAPConfig(apIP,apIP,mask)&&WiFi.softAP(AP_SSID,AP_PASSWORD);
 if(redeOK){
  dns.start(53,"*",apIP);
  const char* headers[]={"X-Horta-Control"};server.collectHeaders(headers,1);
  server.on("/",HTTP_GET,servirPagina);
  server.on("/api/status",HTTP_GET,responderStatus);
  server.on("/api/start",HTTP_POST,[]{if(comandoPermitido()){iniciar();responderStatus();}});
  server.on("/api/stop",HTTP_POST,[]{if(comandoPermitido()){if(estado!=PARADO)parar("Interrompido pelo painel.");responderStatus();}});
  server.onNotFound([]{
   if(server.uri().startsWith("/api/")){server.send(404,"application/json","{\"error\":\"Rota ou metodo invalido\"}");return;}
   server.sendHeader("Location","http://192.168.4.1/",true);server.send(302,"text/plain","");
  });
  server.begin();Serial.println("Wi-Fi Horta: http://192.168.4.1");
 }else Serial.println("Falha no Wi-Fi. Controle fisico continua disponivel.");
 Serial.println("Pronto. Aguardando comando.");
}
void loop(){
 atualizarCiclo();
 dns.processNextRequest();server.handleClient();
 atualizarCiclo(); // Reavalia sensor, botão e limites após atender a rede.
}

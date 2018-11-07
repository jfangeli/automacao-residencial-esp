#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <ArduinoJson.h>


// MQTT
/* Topico gerado a partir da hash md5 "$TOPICO_PUBLISH$".
 * Publish padrao para envio do status eh /get_situacao
 * Publish padrao para setar o staus eh /set_situacao
 */
#define topicoPublish "$TOPICO_PUBLISH$"
#define topicoSubscribe "$TOPICO_SUBSCRIBE$"
#define idMQTT "$ID_MQTT$"

const char* brokerMQTT = "iot.eclipse.org"; //URL do broker MQTT que se deseja utilizar
int brokerPort = 1883; // Porta do Broker MQTT



//WIFI
const char* ssid = "$WIFI_SSID$";
const char* password = "$WIFI_PASSWORD$";

WiFiClient espClient; // Cria o objeto espClient
PubSubClient MQTT(espClient); // Instancia o Cliente MQTT passando o objeto espClient



//HORA
WiFiUDP ntpUDP;

int16_t utc = -3; //UTC -3:00 Brazil
uint32_t currentMillis = 0;
uint32_t previousMillis = 0;

NTPClient timeClient(ntpUDP, "a.st1.ntp.br", utc*3600, 60000);



//PIN
/* Pino para leitura do estado da lampada . 
 * HIGH: Caso esteja ligado indica que lampada está ligada. 
 * LOW:  Caso esteja desligado indica que lampada esta desligada.
 */
#define dPinEstadoLampada 2
int situacaoEstadoLampada = 0;

/* Pino para escrita para ligar e desligar a lampada. 
 * Por padrão este pino devera iniciar em zero.
 */
#define dPinLigaDesligaLampada 0
int situacaoLigaDesligaLampada = 0;

void iniciarSerial();
void iniciarMQTT();
void iniciarPino(char* nomePino, unsigned int pino);
void iniciartWiFi();
void reconectarWiFi();
void iniciarMQTT();
void callbackSubscribe(char* topic, byte* payload, unsigned int length);
void reconectarMQTT();
void verificarConexoes();
void publicarSituacao();
void iniciarHorario();
void atualizarHorario();
void alterarSituacaoLampada();


void setup() {

  iniciarSerial();

  Serial.println("Iniciando");

  //Necessario iniciar em low para iniciar corretamente
  pinMode(dPinLigaDesligaLampada, OUTPUT);
  
  //Habilitando a leitura do do estado da lampada
  pinMode(dPinEstadoLampada, INPUT);
  
  iniciartWiFi();

  /**********************************************
  Acoes para carregamento do ESP on air
  **********************************************/
  // Port defaults to 8266
   ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
   ArduinoOTA.setHostname("$ESP_HOSTNAME$");

  // No authentication by default
   ArduinoOTA.setPassword("$ESP_PASSWORD$");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
   // ArduinoOTA.setPasswordHash("7ef6156c32f427d713144f67e2ef14d2");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();

  iniciarMQTT();

  iniciarHorario();
  atualizarHorario();
  
  Serial.println("ESP Pronto!");
}

/*
 * Inicia Serial 
 */
void iniciarSerial() 
{
  Serial.begin(115200);
  Serial.println("Serial iniciada em 115200 bauds");
}

/*
 * Inicia horario
 */
void iniciarHorario()
{
  Serial.println("Iniciando horario");
  timeClient.begin();
  timeClient.update();
}

/*
 * Atualiza horario
 */
void atualizarHorario()
{
  currentMillis = millis();//Tempo atual em ms
  
  if (previousMillis < 0 || currentMillis - previousMillis > 120000) {
    previousMillis = currentMillis;    // Salva o tempo atual
  
    Serial.println("Atualizando horario");
    timeClient.forceUpdate();
    
    printf("Time Epoch: %d: ", timeClient.getEpochTime());
    Serial.println(timeClient.getFormattedTime());
    
  }
  
}

/*
 * Inicia a conexao WIFI.
 */
void iniciartWiFi() 
{
    delay(300);
    Serial.println("------Conexao WI-FI------");
    Serial.print("Conectando-se na rede: ");
    Serial.println(ssid);
    Serial.println("Aguarde");
     
    reconectarWiFi();
}

/*
 * Tenta reconectar a rede WIFI, caso nao consiga restarta ESP.
 */
void reconectarWiFi() 
{
  //se ja esta conectado a rede WI-FI, nada e feito. 
  //Caso contrario, sao efetuadas tentativas de conexao
  if (WiFi.status() == WL_CONNECTED){
      return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password); // Conecta na rede WI-FI
   
  while (WiFi.waitForConnectResult() != WL_CONNECTED) 
  {
      Serial.println("Falha na tentativa de conectar a rede WIFI! Restartando ESP...");
      delay(5000);
      ESP.restart();
  }
 
  Serial.println();
  Serial.print("Conectado com sucesso na rede WIFI");
  Serial.print(ssid);
  Serial.println("IP obtido: ");
  Serial.println(WiFi.localIP());
}

/*
 * Inicia MQTT conectando com servidor e setando funcao de calback para subscribe do topico
 */
void iniciarMQTT() 
{
    MQTT.setServer(brokerMQTT, brokerPort);   //informa qual broker e porta deve ser conectado
    MQTT.setCallback(callbackSubscribe);            //atribui função de callback (função chamada quando qualquer informação de um dos tópicos subescritos chega)
}

/*
 * Leitura dos callbacks para topicos subscritos
 */
void callbackSubscribe(char* topic, byte* payload, unsigned int length) 
{
    String msg;
    
    /* Obtem a string do payload recebido
     * Exemplo: {componente:angeliESPCasaLigaLampada,estado:1|0,data_envio:hh:mi:ss}
     */
    for(int i = 0; i < length; i++) 
    {
       char c = (char)payload[i];
       msg += c;
    }
   
    Serial.printf("Mensagem recebida [%s]:", topic);
    Serial.println(msg);

    if (!msg.equals(""))
    {
      StaticJsonBuffer<200> jsonBuffer;
      JsonObject& root = jsonBuffer.parseObject(msg);
      situacaoLigaDesligaLampada = root["estado"];
      Serial.println("Situacao solicitada para estado: ");
      Serial.println(situacaoLigaDesligaLampada);
    }
}

/*
 * Reconecta ao broker MQTT
 */
void reconectarMQTT() 
{
  //se ja esta conectado a rede WI-FI, nada e feito. 
  //Caso contrario, sao efetuadas tentativas de conexao
  if (MQTT.connected()){
      return;
  }

  int tentativas = 0;
  while (!MQTT.connected()) 
  {
      Serial.print("* Tentando se conectar ao Broker MQTT: ");
      Serial.println(brokerMQTT);
      
      if (MQTT.connect(idMQTT)) 
      {
          Serial.println("Conectado com sucesso ao broker MQTT!");
          MQTT.subscribe(topicoSubscribe); 
      } 
      else
      {
        Serial.println("Falha ao conectar no broker MQTT.");
        
        if (tentativas < 10 )
        {  
          Serial.println("Havera nova tentativa de conexao em 5s");
          delay(5000);
        }
        else
        {
          Serial.println("Restartando ESP após 10 tentativas de conectar ao broker MQTT...");
          delay(5000);
          ESP.restart();
        }
      }
      tentativas++;
  }
}

/*
 * Verifica as conexoes e restarta se necessario.
 */
void verificarConexoes()
{
  reconectarWiFi(); //se não há conexão com o WiFI, a conexão é refeita
  
  reconectarMQTT(); //se não há conexão com o Broker, a conexão é refeita
    
}
 
/*
 * Publica a situacao da lampada
 */
void publicarSituacao()
{
  
  String json;
  json.concat("{componente:$ESP_HOSTNAME$,tempo_em_execucao:");
  json.concat(millis() / 1000);                    //tempo total em execucao, pode ser que reinicie depois de certo tempo
  json.concat(",estado:");
  json.concat(situacaoLigaDesligaLampada);              //situacao da lampada
  json.concat(",data_atualizacao:");
  json.concat(timeClient.getFormattedTime());
  //timeClient.getEpochTime() //time in seconds since Jan. 1, 1970
  json.concat("}");
  
  //Serial.println(json);
  //Serial.println(json.length());
  
  char payload[json.length()];
  json.toCharArray(payload, json.length()+1);
  
  MQTT.publish(topicoPublish, payload);

  Serial.println("Publicado no broker: ");
  Serial.println(payload);
}

/**
 * Altera situacao da lampada conforme recebido.
 */
void alterarSituacaoLampada(){
   
   if(situacaoLigaDesligaLampada != situacaoEstadoLampada){ 
    if(situacaoLigaDesligaLampada == 1 ){
      digitalWrite(dPinLigaDesligaLampada,HIGH);
      Serial.println("Lampada ligada");
    }else{
      digitalWrite(dPinLigaDesligaLampada,LOW);
      Serial.println("Lampada desligada");
    }
    situacaoEstadoLampada = situacaoLigaDesligaLampada;
  }
}
 

void loop() {
  //Acao para ESP on air
  ArduinoOTA.handle();

  //Implementacao
  verificarConexoes();

  atualizarHorario();

  alterarSituacaoLampada();
 
  publicarSituacao();

  MQTT.loop();

  delay(2000);
}

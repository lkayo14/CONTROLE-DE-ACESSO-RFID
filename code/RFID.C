#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <MFRC522.h>

// Configurações Wi-Fi
const char* ssid = "ID wifi";
const char* password = "Senha";

// Configuração do CallMeBot
String phoneNumber = "Número wapp"; 
String callMeBotApiKey = "API key do callmebot";  

// Configuração do RFID
#define SS_PIN  5  // Pino SS do RFID
#define RST_PIN 4 // Pino RST do RFID
MFRC522 rfid(SS_PIN, RST_PIN);

// // Configuração do LCD
// LiquidCrystal_I2C lcd(0x27, 16, 2); // 0x27 pode ser 0x3F, dependendo do display
// #define SDA_PIN 8
// #define SCL_PIN 9

//Configuração dos LEDs
#define LED_VERDE 2    // Acesso permitido
#define LED_AZUL 7     // LENDO TAG e cadastro
#define LED_VERMELHO 15 // Acesso negado


struct Pessoa {
  String uid;
  String nome;
};

// Lista dinâmica de pessoas cadastradas
#define MAX_PESSOAS 50
Pessoa cadastrados[MAX_PESSOAS];
int totalCadastrados = 0;

const String MASTER_TAG = "0D29FA00";  
bool modoCadastro = false;

// buscar nome pelo UID
String buscarNomePorUID(String uid) {
  for (int i = 0; i < totalCadastrados; i++) {
    if (cadastrados[i].uid == uid) {
      return cadastrados[i].nome;
    }
  }
  return "Desconhecido";
}

// Vai verificar se tag está cadastrada
bool estaCadastrado(String uid) {
  for (int i = 0; i < totalCadastrados; i++) {
    if (cadastrados[i].uid == uid) return true;
  }
  return false;
}

// Cadastra novo UID
bool cadastrarUID(String uid) {
  if (totalCadastrados < MAX_PESSOAS) {
    cadastrados[totalCadastrados].uid = uid;
    cadastrados[totalCadastrados].nome = "Usuario" + String(totalCadastrados + 1); // Nome padrão
    totalCadastrados++;
    return true;
  }
  return false;
}

// Exclui UID existente
bool excluirUID(String uid) {
  for (int i = 0; i < totalCadastrados; i++) {
    if (cadastrados[i].uid == uid) {
      for (int j = i; j < totalCadastrados - 1; j++) {
        cadastrados[j] = cadastrados[j + 1];
      }
      totalCadastrados--;
      return true;
    }
  }
  return false;
}

// Função para codificar URL CallMeBot
String urlEncode(const String &str) {
  String encoded = "";
  char c;
  char code0;
  char code1;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c)) {
      encoded += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
      code0 = ((c >> 4) & 0xf) + '0';
      if (((c >> 4) & 0xf) > 9) code0 = ((c >> 4) & 0xf) - 10 + 'A';
      encoded += '%';
      encoded += code0;
      encoded += code1;
    }
  }
  return encoded;
}

// Enviar mensagem via CallMeBot para o whatsapp
void sendWhatsAppMessage(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String encodedMessage = urlEncode(message);
    String url = String("https://api.callmebot.com/whatsapp.php?phone=") +
                 phoneNumber +
                 "&text=" + encodedMessage +
                 "&apikey=" + callMeBotApiKey;

    Serial.println("Enviando mensagem via WhatsApp...");
    Serial.println("URL: " + url);

    http.begin(url);
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      Serial.println("Mensagem enviada! Código de resposta: " + String(httpResponseCode));
    } else {
      Serial.println("Erro ao enviar mensagem. Código: " + String(httpResponseCode));
    }

    http.end();
  } else {
    Serial.println("Wi-Fi desconectado. Não foi possível enviar a mensagem.");
  }
}

// Inicializa o sistema e conecta no wifi
void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando sistema...");

  // Wire.begin(SDA_PIN, SCL_PIN);
  // Serial.println("I2C inicializado corretamente!");

  WiFi.begin(ssid, password);
  Serial.println("Tentando conectar ao Wi-Fi...");
  delay(5000);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Conectado ao Wi-Fi!");
  } else {
    Serial.println("Falha na conexão Wi-Fi! Continuando sem Wi-Fi.");
  }

  SPI.begin();
  Serial.println("Inicializando RFID...");
  rfid.PCD_Init();
  Serial.println("RFID inicializado!");
  Serial.println("RFID funcionando! (teste sem self-check)");

  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(LED_AZUL, OUTPUT);

  Serial.println("Setup concluído!");
}


void loop() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  // Obtém o UID do cartão
  String tagID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    tagID += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
    tagID += String(rfid.uid.uidByte[i], HEX);
  }
  tagID.toUpperCase();
  Serial.println("Cartão detectado: " + tagID);

  // Se for a tag mestra
  if (tagID == MASTER_TAG) {
    modoCadastro = !modoCadastro;
    String estado = modoCadastro ? "ATIVO" : "DESATIVADO";
    digitalWrite(LED_AZUL, HIGH);
    Serial.println("Modo Cadastro: " + estado);
    sendWhatsAppMessage("[SISTEMA RFID] Modo Cadastro " + estado + " via TAG MESTRA");
    delay(1500);
    digitalWrite(LED_AZUL, LOW);
    rfid.PICC_HaltA();
    return;
  }

  // Se estiver no modo cadastro
  if (modoCadastro) {
    if (estaCadastrado(tagID)) {
      if (excluirUID(tagID)) {
        digitalWrite(LED_VERDE, HIGH);
        digitalWrite(LED_VERMELHO, HIGH);
        Serial.println("Tag removida: " + tagID);
        sendWhatsAppMessage("[SISTEMA RFID] Tag REMOVIDA: " + tagID);
        delay(1500);
        digitalWrite(LED_VERDE, LOW);
        digitalWrite(LED_VERMELHO, LOW);
      } else {
        sendWhatsAppMessage("[SISTEMA RFID] Erro ao remover tag: " + tagID);
      }
    } else {
      if (cadastrarUID(tagID)) {
        digitalWrite(LED_VERDE, HIGH);
        Serial.println("Tag cadastrada: " + tagID);
        sendWhatsAppMessage("[SISTEMA RFID] Nova TAG CADASTRADA: " + tagID);
        delay(1500);
        digitalWrite(LED_VERDE, LOW);
      } else {
        sendWhatsAppMessage("[SISTEMA RFID] Limite máximo de cadastros atingido!");
      }
    }
    delay(1500);
    rfid.PICC_HaltA();
    return;
  }

  // Verifica se tag está autorizada
  if (estaCadastrado(tagID)) {
    String nome = buscarNomePorUID(tagID);
    Serial.println("Acesso Permitido a: " + nome);
    digitalWrite(LED_VERDE, HIGH);
    sendWhatsAppMessage("[SISTEMA RFID] Acesso Permitido: " + nome + " (" + tagID + ")");
    delay(3000);
    digitalWrite(LED_VERDE, LOW);
  } else {
    Serial.println("Acesso Negado!");
    digitalWrite(LED_VERMELHO, HIGH);
    sendWhatsAppMessage("[SISTEMA RFID] Acesso NEGADO: " + tagID);
    delay(3000);
    digitalWrite(LED_VERMELHO, LOW);
  }

  rfid.PICC_HaltA();
}

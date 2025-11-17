
// Bibliotecas de Hardware
#include <Arduino.h> 
#include <Keypad.h>    
#include <SPI.h>       
#include <MFRC522.h>   

// Bibliotecas de Rede
#include <WiFi.h>          
#include <HTTPClient.h>    
#include <ArduinoJson.h>   

// ---------------------------------------------------
// CONFIGURAÇÃO DO WIFI
// ---------------------------------------------------
const char* ssid = "vemamigo_202";
const char* password = "visitante933202";

// ---------------------------------------------------
// CONFIGURAÇÃO DO IP DA API: (ip do servidor na rede)
// ---------------------------------------------------
String apiBaseUrl = "http://192.168.18.143:8000"; 

// ---------------------------------------------------
// PINOS DO TECLADO DE MEMBRANA
// ---------------------------------------------------
const byte ROWS = 4; 
const byte COLS = 4; 
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {32, 33, 25, 26}; 
byte colPins[COLS] = {15, 21, 4, 16}; 
Keypad customKeypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS); 

// === CONFIGURAÇÃO DO RFID MFRC522 ===
#define RST_PIN    22  
#define SS_PIN     5   
MFRC522 mfrc522(SS_PIN, RST_PIN);  

// === Tags rfid usadas: ===
// 35-06-69-45
// E6-2C-8F-BB

// === CONTROLE DA MÁQUINA DE ESTADOS ===
enum Estado {
  MENU_PRINCIPAL,      
  AGUARDANDO_RFID,     // Para Op 1, 2 (Movimentação)
  COLETANDO_QUANTIDADE, // Para Op 1, 2
  AGUARDANDO_RFID_PARA_DELETE, // Para Op 4
  AGUARDANDO_CONFIRMACAO_DELETE, // Para Op 4
  AGUARDANDO_RFID_PARA_PUT, // Para Op 5
  COLETANDO_NOVO_NOME_PUT // Para Op 5
};
Estado estadoAtual = MENU_PRINCIPAL; 

// === VARIÁVEIS GLOBAIS DE DADOS ===
String acaoSelecionada = ""; 
String rfidLida = "";        
String inputBuffer = "";     

// === FUNÇÕES DO MENU C 
void mostrarMenuPrincipal() {
  Serial.println("\n=========================");
  Serial.println("=== MENU PRINCIPAL ===");
  Serial.println("1: Registrar ENTRADA");
  Serial.println("2: Registrar SAÍDA");
  Serial.println("3: Listar Itens (API)");
  Serial.println("4: DELETAR Item (Cuidado!)"); // NOVO
  Serial.println("5: ATUALIZAR Nome/Código do Item"); // NOVO
  Serial.println("(*) Para Cancelar");
  Serial.println("Selecione uma opção:");
}

void resetarParaMenu() {
  acaoSelecionada = "";
  rfidLida = "";
  inputBuffer = "";
  estadoAtual = MENU_PRINCIPAL;
  mostrarMenuPrincipal();
}

String uidBytesToString(byte *buffer, byte bufferSize) {
  String uid = "";
  for (byte i = 0; i < bufferSize; i++) {
    uid += (buffer[i] < 0x10 ? "0" : ""); 
    uid += String(buffer[i], HEX);
    if (i < bufferSize - 1) uid += "-";
  }
  uid.toUpperCase();
  return uid;
}

// === FUNÇÕES DE REDE E API ===

void connectToWiFi() {
  Serial.print("Conectando ao WiFi ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Conectado!");
    Serial.print("Endereço IP: ");
    Serial.println(WiFi.localIP());
  }
  else {
    Serial.println("\n❌ Falha ao conectar ao WiFi. Reiniciando em 5s...");
    delay(5000);
    ESP.restart();
  }
}

// === (OPÇÃO 1 e 2) Chama o endpoint POST /movimentacoes ===
void chamarApiMovimentacao(String rfid_uid, String acao, int quantidade) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ERRO] WiFi desconectado.");
    return;
  }

  HTTPClient http;
  String endpoint = apiBaseUrl + "/movimentacoes";
  http.begin(endpoint);
  http.addHeader("Content-Type", "application/json");

  JsonDocument doc; 
  doc["rfid_uid"] = rfid_uid;
  doc["acao"] = acao;
  doc["quantidade"] = quantidade;
  
  String jsonPayload;
  serializeJson(doc, jsonPayload);

  Serial.println("\nEnviando POST para " + endpoint);
  Serial.println("Payload: " + jsonPayload);

  int httpCode = http.POST(jsonPayload);

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.print("Código HTTP: "); Serial.println(httpCode);
    Serial.print("Resposta da API: "); Serial.println(payload);

    if (httpCode != 200) {
      Serial.println("❌ ERRO NA MOVIMENTAÇÃO (ver resposta da API acima)");
    } else {
      Serial.println("✅ Movimentação registrada com sucesso!");
    }
  } else {
    Serial.println("[ERRO] Falha na chamada HTTP: " + http.errorToString(httpCode));
  }
  http.end();
}

// === (OPÇÃO 3) Chama o endpoint GET /itens ===
  
void chamarApiListarItens() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ERRO] WiFi desconectado.");
    return;
  }

  HTTPClient http;
  String endpoint = apiBaseUrl + "/itens";
  http.begin(endpoint);

  Serial.println("\nEnviando GET para " + endpoint);

  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.print("Código HTTP: "); Serial.println(httpCode);

    if (httpCode == 200) {
      JsonDocument doc; // DynamicJsonDocument foi substituído por JsonDocument
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.print("[ERRO] Falha ao ler o JSON da API: ");
        Serial.println(error.c_str());
        return;
      }

      JsonArray array = doc.as<JsonArray>();
      Serial.println("--- LISTA DE ITENS NO ESTOQUE ---");
      for (JsonObject item : array) {
        String rfid = item["rfid_uid"];
        String nome = item["nome"];
        int qtd = item["quantidade"];
        Serial.printf(" - RFID: %s, Nome: %s, Qtd: %d\n", rfid.c_str(), nome.c_str(), qtd);
      }
      Serial.println("-----------------------------------");

    } else {
      Serial.println("❌ ERRO: API retornou código " + String(httpCode));
    }
  } else {
    Serial.println("[ERRO] Falha na chamada HTTP: " + http.errorToString(httpCode));
  }
  http.end();
}


// === (OPÇÃO 4) Chama o endpoint DELETE /itens/{rfid} ===
 
void chamarApiDeletarItem(String rfid_uid) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ERRO] WiFi desconectado.");
    return;
  }

  HTTPClient http;
  String endpoint = apiBaseUrl + "/itens/" + rfid_uid;
  http.begin(endpoint);

  Serial.println("\nEnviando DELETE para " + endpoint);

  int httpCode = http.sendRequest("DELETE"); 

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.print("Código HTTP: "); Serial.println(httpCode);
    Serial.print("Resposta da API: "); Serial.println(payload);
    
    if (httpCode == 200) {
      Serial.println("✅ Item deletado com sucesso!");
    } else {
      Serial.println("❌ ERRO AO DELETAR (ver resposta da API acima)");
    }
  } else {
    Serial.println("[ERRO] Falha na chamada HTTP: " + http.errorToString(httpCode));
  }
  http.end();
}

// === (OPÇÃO 5) Chama o endpoint PUT /itens/{rfid} ===

void chamarApiAtualizarItem(String rfid_uid, String novo_nome) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ERRO] WiFi desconectado.");
    return;
  }

  HTTPClient http;
  String endpoint = apiBaseUrl + "/itens/" + rfid_uid;
  http.begin(endpoint);
  http.addHeader("Content-Type", "application/json");

  // Monta o JSON: {"nome": "novo_nome"}
  JsonDocument doc; 
  doc["nome"] = novo_nome;
  
  String jsonPayload;
  serializeJson(doc, jsonPayload);

  Serial.println("\nEnviando PUT para " + endpoint);
  Serial.println("Payload: " + jsonPayload);

  int httpCode = http.PUT(jsonPayload); // Faz a requisição PUT

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.print("Código HTTP: "); Serial.println(httpCode);
    Serial.print("Resposta da API: "); Serial.println(payload);
    
    if (httpCode == 200) {
      Serial.println("✅ Item atualizado com sucesso!");
    } else {
      Serial.println("❌ ERRO AO ATUALIZAR (ver resposta da API acima)");
    }
  } else {
    Serial.println("[ERRO] Falha na chamada HTTP: " + http.errorToString(httpCode));
  }
  http.end();
}


// ========= SETUP =========
void setup() { 
  delay(5000);  // Espera 5 segundos para abrir o monitor serial
  Serial.begin(115200); 
  Serial.println("========= REINICIANDO: Sistema de Estoque =========");

  connectToWiFi(); // Conecta ao WiFi

  SPI.begin();             
  mfrc522.PCD_Init();    
  
  Serial.println("Leitor RFID MFRC522: Pronto");
  
  mostrarMenuPrincipal();
}



// === LOOP ===
void loop() {
  
  char customKey = customKeypad.getKey();

  // Lógica de Cancelamento
  if (customKey == '*') {
    Serial.println("\n[Operação Cancelada]");
    resetarParaMenu();
    return;
  }

  // Máquina de Estados
  switch (estadoAtual) {
    
    case MENU_PRINCIPAL:
      if (customKey) { 
        if (customKey == '1') {
          Serial.println(">> Opção 1: ENTRADA selecionada.");
          acaoSelecionada = "entrada";
          estadoAtual = AGUARDANDO_RFID; 
          Serial.println("Aproxime a tag RFID...");
        
        } 
        else if (customKey == '2') {
          Serial.println(">> Opção 2: SAÍDA selecionada.");
          acaoSelecionada = "saida";
          estadoAtual = AGUARDANDO_RFID;
          Serial.println("Aproxime a tag RFID...");
        
        } 
        else if (customKey == '3') {
          Serial.println(">> Opção 3: Listando Itens (API)...");
          chamarApiListarItens();
          resetarParaMenu(); 
        
        } 
        else if (customKey == '4') { 
          Serial.println(">> Opção 4: DELETAR ITEM selecionada.");
          estadoAtual = AGUARDANDO_RFID_PARA_DELETE;
          Serial.println("Aproxime a tag que você quer DELETAR:");
        
        }
        else if (customKey == '5') { 
          Serial.println(">> Opção 5: ATUALIZAR ITEM selecionado.");
          estadoAtual = AGUARDANDO_RFID_PARA_PUT;
          Serial.println("Aproxime a tag que você quer ATUALIZAR:");
        
        }
        else {
          Serial.println("Opção inválida. Tente novamente.");
        }
      }
      break; 

    // Estados para Opção 1 e 2
    case AGUARDANDO_RFID:
      if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        rfidLida = uidBytesToString(mfrc522.uid.uidByte, mfrc522.uid.size);
        Serial.print("Tag lida: ");
        Serial.println(rfidLida);
        mfrc522.PICC_HaltA(); 
        estadoAtual = COLETANDO_QUANTIDADE; 
        Serial.println("Digite a quantidade e pressione '#' para confirmar:");
      }
      break; 

    case COLETANDO_QUANTIDADE:
      if (customKey) { 
        if (isDigit(customKey)) {
          inputBuffer += customKey; 
          Serial.print(customKey);    
        
        } 
        else if (customKey == '#') {
          if (inputBuffer.length() > 0) {
            Serial.println("\n✅ MOVIMENTAÇÃO PRONTA. ENVIANDO À API...");
            chamarApiMovimentacao(rfidLida, acaoSelecionada, inputBuffer.toInt());
            resetarParaMenu();
          }
          else {
            Serial.println("\n[Nenhuma quantidade digitada. Tente novamente]");
          }
        }
      }
      break;
    
    // ESTADOS PARA OPÇÃO 4 (DELETE)
    case AGUARDANDO_RFID_PARA_DELETE:
      if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        rfidLida = uidBytesToString(mfrc522.uid.uidByte, mfrc522.uid.size);
        Serial.print("Tag para deletar: "); Serial.println(rfidLida);
        mfrc522.PICC_HaltA(); 
        estadoAtual = AGUARDANDO_CONFIRMACAO_DELETE; 
        Serial.println("🚨 TEM CERTEZA? 🚨");
        Serial.println("Pressione '#' para DELETAR PERMANENTEMENTE.");
        Serial.println("Pressione '*' para CANCELAR.");
      }
      break;

    case AGUARDANDO_CONFIRMACAO_DELETE:
      if (customKey) {
          if (customKey == '#') {
              Serial.println("\nCONFIRMADO. Deletando item...");
              chamarApiDeletarItem(rfidLida);
              resetarParaMenu();
          } else {
              Serial.println("Tecla inválida (use '#', ou '*' para cancelar).");
          }
      }
      break;

    // ESTADOS PARA OPÇÃO 5 (PUT/UPDATE)
    case AGUARDANDO_RFID_PARA_PUT:
      if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        rfidLida = uidBytesToString(mfrc522.uid.uidByte, mfrc522.uid.size);
        Serial.print("Tag para atualizar: "); Serial.println(rfidLida);
        mfrc522.PICC_HaltA(); 
        estadoAtual = COLETANDO_NOVO_NOME_PUT;
        Serial.println("Digite o NOVO nome/código (use '*' para '_') e pressione '#':");
        inputBuffer = ""; // Limpa o buffer para a nova entrada
      }
      break;

    case COLETANDO_NOVO_NOME_PUT:
      if (customKey) { 
        // Aceita números ou o 'A' (que será o '_')
        if (isDigit(customKey) || customKey == '*') {
            char charToPrint = (customKey == '*') ? '_' : customKey;
            inputBuffer += charToPrint; // Adiciona o caractere (ou o '_')
            Serial.print(charToPrint); // Feedback visual
        
        } else if (customKey == '#') {
            if (inputBuffer.length() > 0) {
                Serial.println("\nOK. Atualizando item...");
                chamarApiAtualizarItem(rfidLida, inputBuffer);
                resetarParaMenu();
            } else {
                Serial.println("\n[Nenhum nome/código digitado]");
            }
        }
      }
      break;

  } // Fim do switch
}
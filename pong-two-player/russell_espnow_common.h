

/* ===== PACKETS ===== */
#define PKT_DISCOVERY 0x01
#define PKT_INPUT     0x02
#define PKT_READY 0x03
#define PKT_STATE 0x04
#define PKT_FORCE_READY 0x05
#define PKT_YOUARECLIENT 0x06
#define PKT_STATE_COMBAT 0x07


/* ===== PACKET STRUCTS ===== */

struct DiscoveryPacket {
  uint8_t type;
  uint8_t nonce;
};

struct InputPacket {
  uint8_t type;           // PKT_INPUT
  int8_t paddleY;
  uint8_t claimedMaster;  // 1 = I think I'm master, 0 = I think I'm client
  uint8_t click; // used for client to tell master when it's ready to serve.
};

struct ReadyPacket {
  uint8_t type;   // PKT_READY
};


bool localReady  = false;
bool remoteReady = false;

struct StatePacket {
  uint8_t type;
  int8_t ballX;
  int8_t ballY;
  int8_t paddleLeftY;
  int8_t paddleRightY;
  uint8_t scoreLeft;
  uint8_t scoreRight;
  uint8_t sound_trigger;
};

struct ForceReadyPacket {
  uint8_t type;   // PKT_FORCE_READY
  uint8_t scoreLeft;
  uint8_t scoreRight;
  uint8_t sound_trigger;
};

struct YouAreClient {
  uint8_t type;   // PKT_FORCE_READY
};

struct StateCombatPacket {
  uint8_t type;
  int8_t tank_M_X;
  int8_t tank_M_Y;
  int8_t tank_M_dir;
  int8_t tank_C_X;
  int8_t tank_C_Y;
  int8_t tank_C_dir;
  int8_t shell_M_X; // -1 if no shell
  int8_t shell_M_Y;
  int8_t shell_C_X; // -1 if no shell
  int8_t shell_C_Y;
  uint8_t scoreLeft;
  uint8_t scoreRight;
  uint8_t sound_trigger;
};


/* ===================================================== */
/* ================== ESP-NOW CALLBACK ================= */
/* ===================================================== */
void onDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  if (len < 1) return;
  uint8_t type = incomingData[0];

  // Ignore discovery once we have a partner (prevents re-triggering)
  if (type == PKT_DISCOVERY && partnerFound) 
    {
    Serial.println("Bogus Discovery packet received");
    return;
    };

  if (partnerFound) {
    // Quick MAC compare (only first 3 bytes for speed, or full 6 if paranoid)
    bool fromPartner = (memcmp(mac, partnerMac, 6) == 0);
    if (fromPartner) {
      lastPacketFromPartner = millis();
      // Optional debug:
      // Serial.print("Packet from partner → timeout reset (type ");
      // Serial.print(type); Serial.println(")");
    }
  }

  if (type == PKT_YOUARECLIENT) {
    if (!partnerFound) {
      memcpy(partnerMac, mac, 6);
      esp_now_add_peer(partnerMac, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
    }
    partnerFound = true;
    isMaster = false;
    gameState = STATE_READY;
    Serial.println("Received YOUARECLIENT → forced to CLIENT role");
    return;
  }

  if (type == PKT_DISCOVERY && !partnerFound) {
    DiscoveryPacket pkt;
    memcpy(&pkt, incomingData, sizeof(pkt));

    memcpy(partnerMac, mac, 6);
    esp_now_add_peer(partnerMac, ESP_NOW_ROLE_COMBO, 1, NULL, 0);

    // I received first → I become MASTER and tell the other to be client
    isMaster = true;
    partnerFound = true;
    gameState = STATE_READY;

    // Immediately force the sender to client role
    YouAreClient force;
    force.type = PKT_YOUARECLIENT;
    esp_now_send(partnerMac, (uint8_t*)&force, sizeof(force));

    Serial.printf("Received discovery first → I am MASTER (sent YouAreClient)\n");
    Serial.printf("  Partner MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    return;
  }

if (type == PKT_READY) {
  remoteReady = true;
  Serial.printf("Received PKT_READY from partner! remoteReady now true (sender MAC: %02X:%02X:...)\n",
                mac[0], mac[1]);
  return;
}

  if (type == PKT_INPUT && partnerFound) {
    InputPacket pkt;
    memcpy(&pkt, incomingData, sizeof(pkt));

    // Role conflict check
    bool peerClaimsMaster = (pkt.claimedMaster == 1);
    if (peerClaimsMaster == isMaster) {
      // Both claim same role → conflict!
      roleConflictDetected = true;
      Serial.println("ROLE CONFLICT DETECTED — both think same role! Resetting...");
      // Fall through to reset logic below
    }

    // Apply paddle if no conflict
    if (!roleConflictDetected) {
      if (isMaster) {
        paddleRightY = pkt.paddleY;
      } else {
        paddleLeftY = pkt.paddleY;
      }
    }
    if (ballState==WAITINGFORSERVE_RIGHT) {
      if (pkt.click==1) {
        // the client is serving, so we can start the ball moving. 
        ballState=CLIENT_SERVING;
        Serial.println("Received click from client → starting serve!");
      }
    } 
    return;
  }

  if (type == PKT_STATE) {
    if (isMaster) {
      Serial.println("Error: Received STATE as Master");
    } else {
      StatePacket pkt;
      memcpy(&pkt, incomingData, sizeof(pkt));
      ballX        = pkt.ballX;
      ballY        = pkt.ballY;
      paddleLeftY  = pkt.paddleLeftY;
      paddleRightY = pkt.paddleRightY;
      scoreLeft    = pkt.scoreLeft;
      scoreRight   = pkt.scoreRight;
      process_sound_trigger(pkt.sound_trigger);
      
    }
    return;
  }

  if (type == PKT_FORCE_READY) {
    ForceReadyPacket pkt;
    memcpy(&pkt, incomingData, sizeof(pkt));
    gameState = STATE_READY;
    scoreLeft  = pkt.scoreLeft;
    scoreRight = pkt.scoreRight;
    process_sound_trigger(pkt.sound_trigger);
    localReady = remoteReady = false;
    return;
  }
}
/* ===================================================== */
/* ================== ESP-NOW SEND ===================== */
/* ===================================================== */

void sendDiscovery() {
  DiscoveryPacket pkt;
  pkt.type = PKT_DISCOVERY;
  pkt.nonce = myNonce;

  uint8_t broadcastAddr[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  esp_now_send(broadcastAddr, (uint8_t*)&pkt, sizeof(pkt));
}

void sendInput(int paddleY) {
  InputPacket pkt;
  pkt.type         = PKT_INPUT;
  pkt.paddleY      = paddleY;
  pkt.claimedMaster = isMaster ? 1 : 0;
  pkt.click = (digitalRead(BTN_PIN) == LOW ) ? 1 : 0; // tell master I am serving.

  esp_now_send(partnerMac, (uint8_t*)&pkt, sizeof(pkt));
}

void sendReady() {
  ReadyPacket pkt;
  pkt.type = PKT_READY;
  esp_now_send(partnerMac, (uint8_t*)&pkt, sizeof(pkt));
}



void sendState() {
  StatePacket pkt;
  pkt.type = PKT_STATE;
  pkt.ballX = ballX;
  pkt.ballY = ballY;
  pkt.paddleLeftY = paddleLeftY;
  pkt.paddleRightY = paddleRightY;
  pkt.scoreLeft = scoreLeft;
  pkt.scoreRight = scoreRight;
  pkt.sound_trigger=triggered_sound;

  esp_now_send(partnerMac, (uint8_t*)&pkt, sizeof(pkt));
  triggered_sound=TRIGGER_SOUND_NONE; // only sendthe triggerd sound once. 
}



void sendForceReady() {
  ForceReadyPacket pkt;
  pkt.type = PKT_FORCE_READY;
  pkt.scoreLeft = scoreLeft;
  pkt.scoreRight = scoreRight;
  pkt.sound_trigger=triggered_sound;
  esp_now_send(partnerMac, (uint8_t*)&pkt, sizeof(pkt));
}
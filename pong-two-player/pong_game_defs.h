
/* ===== GAME CONSTANTS ===== */
#define PADDLE_HEIGHT 12
#define PADDLE_WIDTH  2
#define LEFT_PADDLE_X  2
#define RIGHT_PADDLE_X (SCREEN_WIDTH - 4)
#define SCORE_TO_WIN 11
#define BALL_SPEED_X 2
#define PADDLE_MOTION_INFLUENCE 0.6f  // tunable factor for how much paddle motion affects ball
#define vlimit(v)   constrain((v), -4, 4)

/* ===== TIMING (add this new one) ===== */
unsigned long lastPacketFromPartner = 0;   // updated every time ANY packet arrives from partner
const unsigned long PARTNER_TIMEOUT_MS = 5000;  // 5 seconds

/* ===== GAME STATE ===== */
enum GameState {
  STATE_SEARCHING,
  STATE_READY,
  STATE_PLAYING
};


GameState gameState = STATE_SEARCHING;

/* ===== Ball STATE ===== */
enum BallState {
  BALLINPLAY,
  WAITINGFORSERVE_LEFT,
  WAITINGFORSERVE_RIGHT,
  CLIENT_SERVING
};



BallState ballState = BALLINPLAY;

/* ===== ROLE ===== */
bool isMaster = false;
bool partnerFound = false;
uint8_t partnerMac[6];
bool roleConflictDetected = false;
int send_youareclient=0; // flag to trigger sending of YOU_ARE_CLIENT packet.

/* ===== PADDLES ===== */
/* ===== PADDLES (updated globals) ===== */
int paddleLeftY = 26;
int paddleRightY = 26;
int lastPaddleLeftY = 26;   // ← ADD THESE
int lastPaddleRightY = 26;  // ← FOR MASTER TRACKING





/* ===== TIMING ===== */
unsigned long lastDiscoverySend = 0;
unsigned long lastInputSend = 0;

uint8_t scoreLeft  = 0;
uint8_t scoreRight = 0;


#define BALL_SIZE 2

int ballX = SCREEN_WIDTH / 2;
int ballY = SCREEN_HEIGHT / 2;
int ballVX = 2;
int ballVY = 1;


/* ===== RANDOM NONCE ===== */
uint8_t myNonce;
uint8_t peerNonce = 0;
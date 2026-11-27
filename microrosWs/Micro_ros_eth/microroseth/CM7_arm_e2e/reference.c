// Definizione del pin PWM collegato al Gate del MOSFET
// Sull'Arduino Nano RP2040 Connect, quasi tutti i pin digitali supportano il PWM
const int pumpPin = 9;

// Variabile per memorizzare l'attuale duty cycle (0-255)
int currentSpeed = 0;

void setup() {
  // Configura il pin come uscita
  pinMode(pumpPin, OUTPUT);

  // Inizializza la pompa da spenta per sicurezza
  analogWrite(pumpPin, 0);

  // Inizializza la comunicazione seriale
  Serial.begin(115200);
  while(!Serial) {
    ; // Attende la connessione del monitor seriale (utile per l'RP2040)
  }

  Serial.println("--- Sistema di Controllo Pompa Grothen ---");
  Serial.println("Inserisci un valore percentuale (0 - 100) per regolare la portata.");
}

void loop() {
  // Controlla se ci sono dati in ingresso sulla Seriale
  if (Serial.available() > 0) {
    // Legge il numero intero inviato
    int targetPercentage = Serial.parseInt();

    // Pulisce il buffer da eventuali caratteri di a capo (\n o \r)
    while(Serial.available() > 0) {
      Serial.read();
    }

    // Limita il valore tra 0 e 100 per evitare errori
    if (targetPercentage >= 0 && targetPercentage <= 100) {

      // Mappa la percentuale (0-100) nel range del PWM (0-255)
      // Il core RP2040 di default usa una risoluzione a 8 bit per analogWrite
      currentSpeed = map(targetPercentage, 0, 100, 0, 255);

      // Aggiorna il segnale sul MOSFET
      analogWrite(pumpPin, currentSpeed);

      Serial.print("Nuova portata impostata al: ");
      Serial.print(targetPercentage);
      Serial.print("% (Valore PWM: ");
      Serial.print(currentSpeed);
      Serial.println(")");
    } else {
      Serial.println("Errore: Inserisci un valore valido tra 0 e 100.");
    }
  }
}

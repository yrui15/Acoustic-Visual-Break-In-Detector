#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- NFC Reader Pins ---
#define SS_PIN 5               // SDA / SS pin
#define RST_PIN 17             // Reset pin
MFRC522 nfc(SS_PIN, RST_PIN);  // Create nfc instance
LiquidCrystal_I2C lcd(0x27, 16, 2);

const int sound = 36, vibration = 39, buzz = 4, led = 16, button1 = 27, button2 = 14;
bool armed = 0, ledtoggle;
byte validcard[10][4] = { { 0x39, 0x00, 0x74, 0x11 } };
unsigned long prevtime, nfcresettime;
int sensitivity, boundary;

void setup() {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  pinMode(sound, INPUT);
  pinMode(vibration, INPUT);
  pinMode(buzz, OUTPUT);
  pinMode(led, OUTPUT);
  pinMode(button1, INPUT);
  pinMode(button2, INPUT);
  sensitivity = 3;
  boundary = 2000 + (6 - sensitivity) * 400;
  lcd.print(" System Ready. ");
  delay(1000);
  lcd.clear();
  lcd.print("System disarmed.");
  SPI.begin();
  nfc.PCD_Init();
}

//flowchart logic begins here

//Main Loop
void loop() {
  nfcarmed(); //this function scan nfc card, if card valid, assign 'true' to 'armed', else 'false'
  resetnfc(); //function to reset nfc module, neglect this
  //enter setup menu: 
  if (digitalRead(button1) == HIGH && digitalRead(button2) == HIGH && (!armed)) {
    lcd.setCursor(0, 0);
    lcd.print("     Setup     ");
    buzzfeedback();
    lcd.setCursor(0, 1);
    lcd.print("Scan Valid Card");
    unsigned long previoustime = millis();
    while (!nfcread()) {
      if ((millis() - previoustime) >= 5000) {
        buzzfeedbackneg();
        lcd.clear();
        lcd.print("   Invalid   ");
        delay(500);
        lcd.setCursor(0, 0);
        lcd.print("System disarmed.");
        break;
      }
    }
    if ((millis() - previoustime < 5000)) {
      buzzfeedback();
      lcd.clear();
      menu();
    }
  }
  //if armed=1, keep on sense for vibration&sound
  if (armed) {
    lcd.setCursor(0, 0);
    lcd.print(" System armed     ");
    prevtime = millis();
    while (armed) {
      if (digitalRead(button1) == HIGH || digitalRead(button2) == HIGH) {//neglect this logic, this is to avoid security glitches 
        resetnfc();
        lcd.setCursor(0, 0);
        lcd.print(" Disarm first! ");
        delay(2000);
        lcd.setCursor(0, 0);
        lcd.print(" System armed    ");
      }
      if ((millis() - prevtime) >= 500) { //led blink
        prevtime = millis();
        ledtoggle = !ledtoggle;
        digitalWrite(led, ledtoggle);
        
      }

      if (analogRead(sound) > boundary && digitalRead(vibration) == HIGH) { //when sound exceed boundary, vibration detected, alarm triggered.
        lcd.setCursor(0, 0);
        lcd.print("Breakin Detected.");
        prevtime = millis();
        while (1) {

          if ((millis() - prevtime) > 1000) {//led blink
            prevtime = millis();
            ledtoggle = !ledtoggle;
            digitalWrite(led, ledtoggle);
            digitalWrite(buzz, ledtoggle);
            resetnfc();
          }
          nfcarmed();//keep scanning card
          if (!armed) {//check for bool status of 'armed', if =0, disarmed.
            digitalWrite(led, LOW);
            digitalWrite(buzz, LOW);
            lcd.clear();
            lcd.print("System disarmed.");
            break;
          } else {
            continue;
          }
        }
      }

      nfcarmed();
    }
  }
}

bool nfcuid(byte uid[4]) { //this function reads the unique id of card,and return them in 4-element array.
  if (!nfc.PICC_IsNewCardPresent()) {//if no card present, function ends, return 0.
    return 0;
  }
  if (!nfc.PICC_ReadCardSerial()) {//if card read error, func ends, return 0.
    return 0;
  }
  buzzfeedback();
  for (int i = 0; i < 4; i++) { //read and assign card uid into an array.
    uid[i] = nfc.uid.uidByte[i];
  }
  return 1; //return 1 means card uid read sucessfully.
}

//this function test whether card scanned is a valid card, based on the stored card uid list.
bool nfcread() {
  if (!nfc.PICC_IsNewCardPresent()) {
    return 0;
  }
  if (!nfc.PICC_ReadCardSerial()) {
    return 0;
  }
  bool cardmatch = 0;
  int bytematch = 0;
  for (int i = 0; i < 10; i++) {
    bytematch = 0;
    for (int j = 0; j < 4; j++) { //compare scanned card with stored array
      if (nfc.uid.uidByte[j] == validcard[i][j]) {
        bytematch += 1;
      }
    }
    if (bytematch == 4) {
      cardmatch = 1; //only return 1 (means card matched) when 4 bytes of uid array are met.
      break;
    }
  }
  if (cardmatch == 1) {//feedback sound and led blink based on card read status.
    buzzfeedback();
  } else {
    buzzfeedbackneg();
  }

  return cardmatch;
}

//this function allows user to add new card.
void addnew() {
  lcd.setCursor(0, 0);
  lcd.print("Choice: Add New");
  delay(500);
  int occupied = 0;
  int count = 0;
  for (int i = 0; i < 10; i++) { //this code check number of empty row 
    if (validcard[i][0] != 0x00) {
      occupied += 1;
    }
  }

  if (occupied == 10) { // if no empty row, display full, return function
    lcd.setCursor(0, 0);
    lcd.print("       FULL.     ");
    delay(1000);
    lcd.clear();
    lcd.print("System disarmed.");
    return;
  } else {
    for (int i = 0; i < 10; i++) { //neglect this logic in flowchart
      count = i;
      if (validcard[i][0] == 0x00) {
        break;
      }
    }
  }
  char buffer[20];
  sprintf(buffer, "Scan card %d .", count + 1); //if not full, prompt user to scan new card
  lcd.setCursor(0, 0);
  lcd.print(buffer);
  lcd.setCursor(0, 1);
  lcd.print("Press 1&2 to exit");//or press both button to exit
  delay(1000);
  byte uid[4];
  while (1) {
    if (nfcuid(uid)) { //use nfcuid function to scan new card uid ans store it into valid card array.
      for (int i = 0; i < 4; i++) {
        validcard[count][i] = uid[i];
      }
      lcd.clear();
      lcd.print(" Card Added. ");
      delay(1000);
      lcd.clear();
      lcd.print("System disarmed.");
      break;
    } else if (digitalRead(button1) == HIGH && digitalRead(button2) == HIGH) { 
      //when both button pressed, abort scanning new card, return function
      lcd.clear();
      lcd.print("Exiting...");
      delay(1000);
      lcd.clear();
      lcd.print("System disarmed.");
      break;
    }
  }
  return;
}


//this function allows user to delete cards
void deletecard() {
  int index = 1;
  lcd.setCursor(0, 0);
  lcd.print("Choice: Delete ");
  prevtime = millis();
  //user select card to be deleted by pressing button 1 or button 2, and confirm action by pressing both button
  while (1) {
    lcd.setCursor(0, 1);
    lcd.print("Card   : ");
    lcd.print(index + 1);
    lcd.print(" ");
    if (digitalRead(button1) == HIGH || digitalRead(button2) == HIGH) {
      prevtime = millis();
      delay(100);
      if (digitalRead(button1) == HIGH && digitalRead(button2) == LOW) {
        prevtime = millis();
        if (index > 1) {
          index -= 1;
          delay(500);
        }
      } else if (digitalRead(button2) == HIGH && digitalRead(button1) == LOW) {
        prevtime = millis();
        if (index < 9) {
          index += 1;
          delay(500);
        }
      } else if (digitalRead(button1) == HIGH && digitalRead(button2) == HIGH) {
        lcd.setCursor(0, 0);
        lcd.print("Deleting...");
        delay(500);
        break;
      }
    } else if ((millis() - prevtime) > 5000) {
      lcd.setCursor(0, 0);
      lcd.print("Timeout! Exiting...");
      delay(1000);
      lcd.clear();
      lcd.print("System disarmed.");
      return;
    }
  }
  //card uid in validcard array was deleted based on index set by user earlier
  //card is deleted by setting all column of the specific index of row into 0x00.
  for (int i = 0; i < 4; i++) {
    validcard[index][i] = 0x00;
  }
  lcd.setCursor(0, 0);
  lcd.print("  Deleted  ");
  delay(1000);
  lcd.clear();
  lcd.print("System disarmed.");
}

//menu function
void menu() {
  while (digitalRead(button1) == HIGH || digitalRead(button2) == HIGH) {
    delay(100);
  }//neglect this logic, this avoid button glitches
  int choice = 1;
  lcd.setCursor(0, 1);
  lcd.print("Confirm? 1&2   ");
  // this code prompt user to select menu
  while (1) {
    switch (choice) {
      case 1:
        lcd.setCursor(0, 0);
        lcd.print("1) Add New Card");
        break;
      case 2:
        lcd.setCursor(0, 0);
        lcd.print("2) Delete Card ");
        break;
      case 3:
        lcd.setCursor(0, 0);
        lcd.print("3) Sensitivity ");
        break;
      case 4:
        lcd.setCursor(0, 0);
        lcd.print("4) Exit Menu   ");
        break;
    }
    if (digitalRead(button1) == HIGH || digitalRead(button2) == HIGH) {
      delay(50);
      if (digitalRead(button1) == HIGH && digitalRead(button2) == LOW && choice != 1) {
        choice -= 1;
        while (digitalRead(button1) == HIGH || digitalRead(button2) == HIGH) { delay(10); }

      } else if (digitalRead(button2) == HIGH && digitalRead(button1) == LOW && choice != 4) {
        choice += 1;
        while (digitalRead(button1) == HIGH || digitalRead(button2) == HIGH) { delay(10); }

      } else if (digitalRead(button1) == HIGH && digitalRead(button2) == HIGH) {
        lcd.setCursor(0, 1);
        lcd.print("   Confirm    ");
        delay(500);
        while (digitalRead(button1) == HIGH || digitalRead(button2) == HIGH) { delay(10); }
        break;
      }
    }
  }
  switch (choice) {//user enter their desired setup page
    case 1:
      addnew();
      break;
    case 2:
      deletecard();
      break;
    case 3:
      sensitive();
      break;
    case 4:
      lcd.clear();
      lcd.print("   Exiting...   ");
      delay(300);
      lcd.clear();
      lcd.print("System disarmed");
      break;
  }
}

//this function scan for a valid card, if card is valid, it toggle the boolean 'armed' and return it.
void nfcarmed() {
  if (nfcread()) {
    armed = !armed;
    nfc.PICC_HaltA();
    nfc.PCD_StopCrypto1();
    delay(1000);
    if (armed == 0) {
      lcd.clear();
      lcd.print("System disarmed");
    }
  }
  return;
}

void buzzfeedback() { //positive feedback
  digitalWrite(buzz, HIGH);
  digitalWrite(led, HIGH);
  delay(50);
  digitalWrite(buzz, LOW);
  digitalWrite(led, LOW);
  delay(25);
  digitalWrite(buzz, HIGH);
  digitalWrite(led, HIGH);
  delay(50);
  digitalWrite(buzz, LOW);
  digitalWrite(led, LOW);
}

void buzzfeedbackneg() { //negative feedback
  digitalWrite(buzz, HIGH);
  digitalWrite(led, HIGH);
  delay(250);
  digitalWrite(buzz, LOW);
  digitalWrite(led, LOW);
}

void resetnfc() { //neglect this logic
  if (digitalRead(button1) == HIGH && digitalRead(button2) == LOW) {
    nfcresettime = millis();
    while (digitalRead(button1) == HIGH && digitalRead(button2) == LOW) {
      if ((millis() - nfcresettime) > 2000) {
        buzzfeedback();
        SPI.end();
        delay(10);
        SPI.begin();
        nfc.PCD_Init();
        return;
      }
    }
  }
}

//this function set sensitivity of the sound sensor, basically by setting the boundary of analog output value to trigger alarm
void sensitive() {
  lcd.clear();
  lcd.print("Sensitivity: ");
  lcd.setCursor(0, 1);
  lcd.print("1)- 2)+ 1&2)OK");
  while (1) {
    lcd.setCursor(14, 0);
    lcd.print(sensitivity);
    if (digitalRead(button1) == HIGH || digitalRead(button2) == HIGH) {
      delay(100);
      if (digitalRead(button1) == HIGH && digitalRead(button2) == LOW && sensitivity != 1) {
        sensitivity -= 1;
        while (digitalRead(button1) == HIGH || digitalRead(button2) == HIGH) { delay(10); }
      } else if (digitalRead(button1) == LOW && digitalRead(button2) == HIGH && sensitivity != 5) {
        sensitivity += 1;
        while (digitalRead(button1) == HIGH || digitalRead(button2) == HIGH) { delay(10); }
      } else if (digitalRead(button1) == HIGH && digitalRead(button2) == HIGH) {
        lcd.setCursor(0, 1);
        lcd.print("       OK        ");
        while (digitalRead(button1) == HIGH || digitalRead(button2) == HIGH) { delay(10); }
        delay(200);
        break;
      }
    }
  }
  boundary = 2000 + (6 - sensitivity) * 400;
  lcd.clear();
  lcd.print("System disarmed.");
  return;
}

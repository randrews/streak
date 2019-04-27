// All the control pins are on ports B and D:
// SCLK - B6
// MISO - B5
// MOSI - B4
// /SS - D7
// RST - D6

// Commands:
// status - print status register
// id - print chip id (if this is nonzero you know the chip works)
// erase - erase chip
// read - read bytes from chip
// @xxxxxx - set 24-bit hex start address for reads / writes
// +nnnn - set size of reads (decimal)
// =xxyyzz... - write bytes starting at start address
// Each command ends with a newline, returns a response when it's finished.
// Initial start address is 0x000000, size is 1
// Reads and writes start at start address, writes also increment that address

const int DLY = 5;
String command;
byte address[3]; // 0=high, 2=low
long length;

void setup() {
  // Set mosi / sclk low
  // Technically this isn't necessary since DDRB should
  // go 0x00 at reset
  PORTB &= 0b10001111; // Set low level. This should be the last time we touch PORTB
  DDRB |= 0b01010000; // Set outputs

  // Set /ss / rst high
  PORTD &= 0b00111111; // Set low level. Last time we touch PORTD
  DDRD &= 0b00111111;  // Set inputs / HighZ

  command.reserve(512);
  address[0] = 0;
  address[1] = 0;
  address[2] = 0;
  length = 1;

  Serial.begin(9600);
  while(!Serial);
}

void loop() {
  if(Serial.available()) {
    command += Serial.readStringUntil(';');
    command.trim();
    delay(10);

    if(command.startsWith("@")) setAddressCommand(command);
    else if(command.startsWith("+")) setSizeCommand(command);
    else if(command.startsWith("=")) writeCommand(command);
    else if(command == "id") idCommand();
    else if(command == "status") statusCommand();
    else if(command == "erase") eraseCommand();
    else if(command == "read") readCommand();
    else if(command == "resethigh") resetCommand(true);
    else if(command == "resetlow") resetCommand(false);
    else { Serial.println("unrecognized: " + command); }
    command.remove(0);
  }
}

void resetCommand(boolean value) {
  value ? rstHigh() : rstLow();
  Serial.println("OK");
}

void readCommand() {
  csLow();
  spiSend(0x0b);
  spiSend(address[0]);
  spiSend(address[1]);
  spiSend(address[2]);
  spiSend(0x00);
  for(int n = 0; n < length; n++) {
    printByte(spiSend(0x00));
    Serial.print(" ");
  }
  Serial.println("");
  csHigh();
}

void writeCommand(String command) {
  enableWrite();
  delayMicroseconds(DLY);
  int stringIdx = 1;
  while(stringIdx <= command.length() - 2) {
    csLow();
    spiSend(0xaf);
    if(stringIdx == 1) {
      spiSend(address[0]);
      spiSend(address[1]);
      spiSend(address[2]);      
    }
    byte b = parseByte(command.charAt(stringIdx), command.charAt(stringIdx + 1));
    spiSend(b);
    csHigh();

    delayMicroseconds(DLY);

    address[2]++;
    if(address[2] == 0) {
      address[1]++;
      if(address[1] == 0) {
        address[0]++;
      }
    }

    stringIdx += 2;
  }

  csLow();
  spiSend(0x04);
  csHigh();

  do { delayMicroseconds(DLY); } while(busy());

  Serial.println("OK");
}

void setAddressCommand(String command) {
  address[0] = parseByte(command.charAt(1), command.charAt(2));
  address[1] = parseByte(command.charAt(3), command.charAt(4));
  address[2] = parseByte(command.charAt(5), command.charAt(6));
  statusCommand();
}

void setSizeCommand(String command) {
  length = command.substring(1).toInt();
  statusCommand();
}

void idCommand() {
  csLow();
  spiSend(0x90);
  spiSend(0x00);
  spiSend(0x00);
  spiSend(0x00);
  byte manufacturerId = spiSend(0x00);
  byte deviceId = spiSend(0x00);
  csHigh();

  Serial.print("manufacturer=0x");
  Serial.print(manufacturerId, HEX);
  Serial.print(",device=0x");
  Serial.println(deviceId, HEX);
}

void statusCommand() {
  csLow();
  spiSend(0x05);
  byte status = spiSend(0x00);
  csHigh();
  if(status & 0x01) Serial.print("busy=1");
  else Serial.print("busy=0");
  if(status & 0x02) Serial.print(",wel=1");
  else Serial.print(",wel=0");
  if(status & 0x04) Serial.print(",bp0=1");
  else Serial.print(",bp0=0");
  if(status & 0x08) Serial.print(",bp1=1");
  else Serial.print(",bp1=0");
  if(status & 0x40) Serial.print(",aai=1");
  else Serial.print(",aai=0");
  if(status & 0x80) Serial.print(",bpL=1");
  else Serial.print(",bpL=0");
  Serial.print(",address=0x");
  printByte(address[0]);
  printByte(address[1]);
  printByte(address[2]);
  Serial.print(",length=");
  Serial.println(length);
}

void eraseCommand() {
  enableWrite();
  delayMicroseconds(DLY);
  csLow();
  spiSend(0x60);
  csHigh();
  delay(110);
  statusCommand();
}

//////////////////////////////////////////////////////////////////////////

bool busy() {
  csLow();
  spiSend(0x05);
  byte status = spiSend(0x00);
  csHigh();
  return status & 0x01;
}

void enableWrite() {
  // First send write enable
  csLow();
  spiSend(0x06);
  csHigh();

  delayMicroseconds(DLY);

  // Enable writing the status register:
  csLow();
  spiSend(0x50);
  csHigh();

  delayMicroseconds(DLY);

  // Write 0s to the protection bits
  csLow();
  spiSend(0x01);
  spiSend(0x00);
  csHigh();
}

byte parseByte(char high, char low) {
  return parseDigit(low) + (parseDigit(high) << 4);
}

byte parseDigit(char ch) {
  byte digit = 0;
  switch(ch) {
    case '0': digit = 0x00; break;
    case '1': digit = 0x01; break;
    case '2': digit = 0x02; break;
    case '3': digit = 0x03; break;
    case '4': digit = 0x04; break;
    case '5': digit = 0x05; break;
    case '6': digit = 0x06; break;
    case '7': digit = 0x07; break;
    case '8': digit = 0x08; break;
    case '9': digit = 0x09; break;
    case 'a':
    case 'A': digit = 0x0a; break;
    case 'b':
    case 'B': digit = 0x0b; break;
    case 'c':
    case 'C': digit = 0x0c; break;
    case 'd':
    case 'D': digit = 0x0d; break;
    case 'e':
    case 'E': digit = 0x0e; break;
    case 'f':
    case 'F': digit = 0x0f; break;
  }
  return digit;
}

void printByte(byte b) {
  if(b < 16) Serial.print("0");
  Serial.print(b, HEX);
}

void csLow() {
  DDRD |= 0b10000000; // Chip select low
}

void csHigh() {
  DDRD &= 0b01111111; // Chip select high
}

void rstLow() {
  DDRD |= 0b01000000; // Reset low
}

void rstHigh() {
  DDRD &= 0b10111111; // Reset high
}

byte spiSend(byte si) {
  byte so = 0x00;

  for(int n = 0; n < 8; n++) {
    // Set MOSI high or low
    if(si & 0x80) {
      DDRB &= 0b11101111; // Input for high
    } else {
      DDRB |= 0b00010000; // Output for low
    }

    // Shift si over
    si <<= 1;

    // SCLK high
    DDRB &= 0b10111111; // Input for high

    delayMicroseconds(DLY);

    // Read MISO
    so <<= 1;
    if(PINB & 0b00100000){
      so |= 1;
    }

    // SCLK low
    DDRB |= 0b01000000; // Output for low
  }
  return so;
}

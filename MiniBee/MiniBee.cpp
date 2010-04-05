#include "MiniBee.h"
#include <Wire.h>

// #include <NewSoftSerial.h>

uint8_t MiniBee::pwm_pins[] = { 3,5,6, 8,9,10 };

MiniBee::MiniBee() {
// 	pwm_pins = { 3,5,6, 8,9,10 };
	
	shtOn = false;
	twiOn = false;
	pingOn = false;
	prev_msg = 0;
	curSample = 0;
	datacount = 0;
	msg_id_send = 0;
	
	for ( i = 0; i<8; i++ ){
	    analog_precision[i] = false; // false is 8bit, true is 10bit
	    analog_in[i] = false;
	}
	for ( i = 0; i<19; i++ ){
	    digital_in[i] = false;
	    digital_out[i] = false;
	    digital_values[i] = 0;
	}
	for ( i=0; i<6; i++ ){
	    pwm_on[i] = false;
	    pwm_values[i] = 0;
	}

	smpInterval = 50; // default value
	msgInterval = 50;
	samplesPerMsg = 1;
	
// 	useSoftSerial = false;

	status = STARTING;
	msg_type = S_NO_MSG;
	message = (char*)malloc(sizeof(char) * MAX_MESSAGE_SIZE);
}

MiniBee Bee = MiniBee();

void MiniBee::begin(int baud_rate) {
	//config array, this should move to the firmware or something?
	//id, twi, sht, ping, sht pins, 
	config = (char*)malloc(sizeof(char) * CONFIG_BYTES);

	Serial.begin(baud_rate);
	delay(200);
  	pinMode(XBEE_SLEEP_PIN, OUTPUT);
  	digitalWrite(XBEE_SLEEP_PIN, 0);
	delay(500);

	send( N_INFO, "starting", 8 );

	readXBeeSerial();
	// allow some delay before sending data
	delay(500);
	

	sendSerialNumber();

// 	send(N_INFO, dest_addr );
// 	send(N_INFO, my_addr );
	
	status = WAITFORHOST;
	send( N_INFO, "waitforhost", 11 );
}

// void MiniBee::setSoftSerial(bool onoff, int baud_rate){
//     useSoftSerial = onoff;
//     if ( onoff )
//       initSoftSerial( baud_rate );
// }
// 
// void MiniBee::initSoftSerial(int baud_rate){
//   // define pin modes for tx, rx, led pins:
//   softSerial =  NewSoftSerial(SoftRX, SoftTX);
//   pinMode(SoftRX, INPUT);
//   pinMode(SoftTX, OUTPUT);
//   // set the data rate for the SoftwareSerial port
//   softSerial.begin(baud_rate);
// }

void MiniBee::doLoopStep(void){
  // read any new data from XBee:
  int bytestoread = Serial.available();
  if ( bytestoread > 0 ){
//         send( N_INFO, "reading" );
	for ( i = 0; i < bytestoread; i++ ){
		read();      
	}
//   } else {
//       send( N_INFO, "no data" );
  }
  
  // do something based on current status:
  switch( status ){
    case SENSING:
      //send( N_INFO, "sensing" );
	// read sensors:
	readSensors( datacount );
	if ( curSample > samplesPerMsg ){
	    sendData();
	    curSample = 0;
	    datacount = 0;
	}
	delay( smpInterval );
	break;
    case STARTING:
      send( N_INFO, "starting", 8 );
      delay( 100 );
      break;
    case WAITFORCONFIG:
//       send( N_INFO, "waitforconfig" );
      delay( 100 );
      break;
    case WAITFORHOST:
//       send( N_INFO, "waitforhost" );
      delay( 100 );
      break;
  }
}


/// read the serial number of the XBee
void MiniBee::readXBeeSerial(void){
  
    char *response;// = (char *)malloc(sizeof(char)*6);
    char *response2;// = (char *)malloc(sizeof(char)*8);
    
    //populate whatever xBee properties we'll need.
    atEnter();

//     my_addr = atGet( "MY" );

    response = atGet( "SH" );
    response2 = atGet( "SL" );

    serial = (char *)malloc(sizeof(char)* (
      // length of both strings plus null-termination
      strlen(response) + strlen(response2) + 1 )
      ) ;

    serial = strcpy( serial, response );
    serial = strcat( serial, response2 );

    free( response );
    free( response2 );

//      response = atGet( "DH" );
//      response2 = atGet( "DL" );
//  
//      dest_addr = (char *)malloc(sizeof(char)* (
//        // length of both strings plus null-termination
//        strlen(response) + strlen(response2) + 1 )
//      ) ;
//  
//      dest_addr = strcpy( dest_addr, response );
//      dest_addr = strcat( dest_addr, response2 );
//  
// //     if(strcmp(dest_addr, DEST_ADDR) != 0) {
// //     //make sure we're on the default destination address
// //       atSet("DH", 0);
// //       atSet("DL", 1);
// //     }
//  
//      free( response );
//      free( response2 );
     
    atExit();  
}

//**** Xbee AT Commands ****//
int MiniBee::atGetStatus() {
	incoming = 0;
	int status = 0;
	
	while(incoming != CR ) {
		if(Serial.available()) {
			incoming = Serial.read();
		  	status += incoming;
		}
	}
	
	return status;
}

void MiniBee::atSend(char *c) {
	Serial.print("AT");
	Serial.print(c);
	Serial.print('\r');
}

void MiniBee::atSend(char *c, uint8_t v) {
	Serial.print("AT");
	Serial.print(c);
	Serial.print(v);
	Serial.print('\r');
}

int MiniBee::atEnter() {
	delay( 1000 );
	Serial.print("+++");
	delay( 500 );
	return atGetStatus();
}

int MiniBee::atExit() {
	atSend("CN");
	return atGetStatus();
}

int MiniBee::atSet(char *c, uint8_t val) {
	atSend(c, val);
	return atGetStatus();
}

char* MiniBee::atGet(char *c) {
	char *response = (char *)malloc(sizeof(char)*128);
	incoming = 0;
	i = 0;
	
	atSend( c );
	
	while(incoming != CR ) {
		if(Serial.available()) {
		  incoming = Serial.read();
		  response[i] = incoming;
		  i++;
		}
	}
	response[i-1] = '\0';
	realloc(response, sizeof(char)*(i));
	
	return response;
}

//**** END AT COMMAND STUFF ****//

void MiniBee::send(char type, char *p, int size) {
//   if ( useSoftSerial ){
// 	softSerial.print(ESC_CHAR, BYTE);
// 	softSerial.print(type, BYTE);
// 	for(i = 0;i < strlen(p);i++) slipSoft(p[i]);
// 	softSerial.print(DEL_CHAR, BYTE);
//   } else {
	Serial.print(ESC_CHAR, BYTE);
	Serial.print(type, BYTE);
	for(i = 0;i < size;i++) slip(p[i]);
	Serial.print(DEL_CHAR, BYTE);
//   }
}

// void MiniBee::slipSoft(char c) {
// 	if((c == ESC_CHAR) || (c == DEL_CHAR) || (c == CR))
// 	    softSerial.print(ESC_CHAR, BYTE);
// 	softSerial.print(c, BYTE);
// }

void MiniBee::slip(char c) {
	if((c == ESC_CHAR) || (c == DEL_CHAR) || (c == CR))
	    Serial.print(ESC_CHAR, BYTE);
	Serial.print(c, BYTE);
}

void MiniBee::read() {
	incoming = Serial.read();
 	if(escaping) {	//escape set
		if((incoming == ESC_CHAR)  || (incoming == DEL_CHAR) || (incoming == CR)) {
			//escape to integer
			if ( msg_type != S_NO_MSG ){ // only add if message type set
// 			if ( incoming != 255 ){
			  message[byte_index] = incoming;
			  byte_index++;
			}
		} else {	//escape to char
			msg_type = incoming;
		}
		escaping = false;
	} else {	//escape not set
		if(incoming == ESC_CHAR) {
			escaping = true;
		} else if(incoming == DEL_CHAR) {	//end of msg
			message[byte_index] = '\0'; // null-termination
			routeMsg(msg_type, message, byte_index);	//route completed message
			msg_type = S_NO_MSG;
			byte_index = 0;	//reset buffer index
		} else {
			if ( msg_type != S_NO_MSG ){ // only add if message type set
// 			if ( incoming != 255 ){
			  message[byte_index] = incoming; 
			  byte_index++;
			}
		}
	}
}

boolean MiniBee::checkNodeMsg( uint8_t nid, uint8_t mid ){
	boolean res = ( (nid == id)  && ( mid != prev_msg) );
	prev_msg = mid;
	return res;
}

boolean MiniBee::checkMsg( uint8_t mid ){
	boolean res = ( mid != prev_msg);
	prev_msg = mid;
	return res;
}

void MiniBee::routeMsg(char type, char *msg, uint8_t size) {
  	int len;
	char * ser;
	char typeSize[2];
	typeSize[0] = type;
	typeSize[1] = size;
// 	typeSize[2] = '\0';
	// msg loopback;
	send( N_INFO, typeSize, 2 );
	send( N_INFO, msg, size );
	switch(type) {
		case S_ANN:
			sendSerialNumber();
			status = WAITFORHOST;
// 			configure();
			send( N_INFO, "waitforhost", 11 );
			break;
		case S_QUIT:
			status = WAITFORHOST;
			//do something to stop doing anything
			send( N_INFO, "waitforhost", 11 );
			break;
		case S_ID:
			// check for msg ID
// 			if ( checkMsg( msg[0] ) ){
			  len = strlen(serial);
			  ser = (char *)malloc(sizeof(char)* (len + 1 ) );
			  
// 			  char ser[8];

			  for(i = 0;i < len;i++){ ser[i] = msg[i]; }
			  ser[len] = '\0';

			  if(strcmp(ser, serial) == 0){
			      id = msg[len];	//writeConfig(msg);
			      if ( size == (len+2) ){
				  config_id = msg[len+1];
// 				  waitForConfig();
				  status = WAITFORCONFIG;
				  send( N_INFO, "waitforconfig", 13 );
			      } else if ( size == (len+1) ) {
				  readConfig();
				  status = SENSING;
				  send( N_INFO, "sensing", 7 );
			      }
			  } else {
			      send( N_INFO, "wrong serial number", 19 );
			      send( N_INFO, ser, len );
			  }
// 			}
			break;
		case S_CONFIG:
		      // check if right config_id:
		      if ( msg[0] == config_id ){
// 			writeConfig( msg );
// 			readConfig();
			readConfigMsg( msg );
			status = SENSING;
			send( N_INFO, "sensing", 7 );
		      }
		case S_PWM:
			if ( checkNodeMsg( msg[0], msg[1] ) ){
			    for( i=0; i<6; i++){
			      pwm_values[i] = msg[2+i];
			    }
			    setPWM();
			}
			break;
		case S_DIGI:
			if ( checkNodeMsg( msg[0], msg[1] ) ){
			    for( i=0; i< (size-2); i++){
			      digital_values[i] = msg[2+i];
			    }
			    setDigital();
			}
			break;
// 		default:
// 			break;
		}
}

void MiniBee::setPWM(){
	for( i=0; i<6; i++){
	  if ( pwm_on[i] ){
	    analogWrite( pwm_pins[i], pwm_values[i] );
	  }
	} 
}

void MiniBee::setDigital(){
	for( i=0; i<19; i++){
	  if ( digital_out[i] ){
	    digitalWrite( i, digital_values[i] );
	  }
	} 
}

void MiniBee::dataFromInt( int output, int offset ){
  data[offset]   = byte(output/256);
  data[offset+1] = byte(output%256);
}

void MiniBee::readSensors( uint8_t db ){
    int value;
    // read analog sensors
    for ( i = 0; i < 8; i++ ){
      if ( analog_in[i] ){
	    if ( analog_precision[i] ){
		value = analogRead(i);
		dataFromInt( value, db );
		db += 2;
	    } else {
		data[db] = analogRead(i)/4;
		db++;      
	    }
      }
    }
    // read digital sensors
    for ( i = 0; i < 19; i++ ){
      if ( digital_in[i] ){
	// this can be done way more clever by shifting the results into 3 bytes
	data[db] = digitalRead(i);
	db++;
      }
    }
    // read I2C/two wire interface accelero
    if ( twiOn ){
      readAccelleroTWI( accel1Address, db );
      db += 3;
    }
    
    // read SHT sensor
    if ( shtOn ){
      measureSHT( SHT_T_CMD );
      dataFromInt( valSHT, db );
      db += 2;
      measureSHT( SHT_H_CMD );
      dataFromInt( valSHT, db );
      db += 2;      
    }
    
    // read ultrasound sensor
    if ( pingOn ){
      dataFromInt( readPing(), db );
      db += 2;
    }
}

void MiniBee::sendData(void){
    msg_id_send++;
    msg_id_send = msg_id_send%256;
    outMessage[0] = id;
    outMessage[1] = msg_id_send;
    for ( i=0; i < datacount; i++ ){
	outMessage[i+2] = data[i];
    }
    send( N_DATA, outMessage, datacount+2 );
}

uint8_t MiniBee::getId(void) { 
	return id;
}

void MiniBee::sendSerialNumber(void){
	send(N_SER, serial, strlen(serial) );
}  

// void MiniBee::waitForConfig(void){
// 	long timeout = millis();
// 	while(millis() < timeout + 30000) read();
// 	// waits 30 seconds for a configuration
// }  

// void MiniBee::configure(void) {
// 	send(N_SER, serial);
// 	long timeout = millis();
// 	while(millis() < timeout + 5000) read();
// 	readConfig();
// }

void MiniBee::writeConfig(char *msg) {
// 	eeprom_write_byte((uint8_t *) i, id ); // writing id
	for(i = 0;i < CONFIG_BYTES;i++){
	    eeprom_write_byte((uint8_t *) i, msg[i+1]);
	    //write byte to memory
	}
}

void MiniBee::readConfigMsg(char *msg){
	for(i = 0;i < CONFIG_BYTES;i++){
	   config[i] = msg[i+1];
	}
	parseConfig();
}

void MiniBee::readConfig(void) {
	for(i = 0;i < CONFIG_BYTES;i++) config[i] = eeprom_read_byte((uint8_t *) i);
	
	parseConfig();
}

void MiniBee::parseConfig(void){
	int datasize = 0;

	msgInterval = config[0]*256 + config[1];
	samplesPerMsg = config[2];
	for(i = 3;i < CONFIG_BYTES;i++){
	    switch( config[i] ){
	      case AnalogIn10bit:
		if ( i > 13 ){
		    analog_precision[i-14] = true;
		    analog_in[i-14] = true;
		    pinMode( i, INPUT );
		    datasize += 2;
		}
		break;
	      case AnalogIn:
		if ( i > 13 ){
		    analog_precision[i-14] = false;
		    analog_in[i-14] = true;
		    pinMode( i, INPUT );
		    datasize += 1;
		}
		break;
	      case DigitalIn:
		pinMode( i, INPUT );
		digital_in[i-3] = true;
		datasize += 1;
		break;
	      case AnalogOut:
		for ( int j=0; j < 6; j++ ){
		    if ( pwm_pins[j] == i ){
			pinMode( i, OUTPUT );
			pwm_on[j] = true;
		    }
		}
		break;
	      case DigitalOut:
		digital_out[i-3] = true;
		pinMode( i, OUTPUT );
		break;
	      case SHTClock:
		sht_pins[0] = i;
		shtOn = true;
		pinMode( i, OUTPUT );
		break;
	      case SHTData:
		sht_pins[1] = i;
		shtOn = true;
		pinMode( i, OUTPUT );
		datasize += 4;
		break;
	      case TWIClock:
	      case TWIData:
		twiOn = true;
		datasize += 3;
		break;
	      case Ping:
		pingOn = true;
		ping_pin = i;
		datasize += 2;
		break;
	      case NotUsed:
		break;
	      case UnConfigured:
		break;
	    }
	}
	
	datasize = datasize * samplesPerMsg;
	data = (char*)malloc(sizeof(char) * datasize);
	outMessage = (char*)malloc( sizeof(char) * (datasize + 2 ) );
	smpInterval = msgInterval / samplesPerMsg;

	if ( twiOn ){
	    setupAccelleroTWI();
	}
	if ( shtOn ){
	    setupSHT();
	}
	// no need to setup ping
// 	if ( pingOn ){
// 	    setupPing();
// 	}
	
}

//TWI --- for LIS302DL accelerometer
boolean MiniBee::getFlagTWI(void) { 
 	return twiOn;
} 

void MiniBee::setupAccelleroTWI(void) {
	//start I2C bus
//   Wire.begin();
  setupTWI();

//------- LIS302DL setup --------------
  Wire.beginTransmission(accel1Address);
  Wire.send(0x21); // CTRL_REG2 (21h)
  Wire.send(B01000000);
  Wire.endTransmission();
  
  	//SPI 4/3 wire
  	//1=ReBoot - reset chip defaults
  	//n/a
  	//filter off/on
  	//filter for freefall 2
  	//filter for freefall 1
  	//filter freq MSB
  	//filter freq LSB - Hipass filter (at 400hz) 00=8hz, 01=4hz, 10=2hz, 11=1hz (lower by 4x if sample rate is 100hz)   

  Wire.beginTransmission(accel1Address);
  Wire.send(0x20); // CTRL_REG1 (20h)
  Wire.send(B01000111);
  Wire.endTransmission();
  
  	//sample rate 100/400hz
  	//power off/on
  	//2g/8g
  	//self test
  	//self test
  	//z enable
  	//y enable
  	//x enable 

//-------end LIS302DL setup --------------
}

/// reading LIS302DL
void MiniBee::readAccelleroTWI( int address, int dboff ){
    data[dboff]   = readTWI( address, accelResultX, 1 ) + 128 % 256;
    data[dboff+1] = readTWI( address, accelResultY, 1 ) + 128 % 256;
    data[dboff+2] = readTWI( address, accelResultZ, 1 ) + 128 % 256;
}

void MiniBee::setupTWI(void) {
	//start I2C bus
	Wire.begin();
}

int MiniBee::readTWI(int address, int bytes) {
	i = 0;
	int twi_reading[bytes];
	Wire.requestFrom(address, bytes);
  	while(Wire.available()) {   
		twi_reading[i] = Wire.receive();
		i++;
	}
	return *twi_reading;
}

//read a specific register on a particular device.
int MiniBee::readTWI(int address, int reg, int bytes) {
	i = 0;
	int twi_reading[bytes];
	Wire.beginTransmission(address);
	Wire.send(reg);                   //set x register
	Wire.endTransmission();
	Wire.requestFrom(address, bytes);            //retrieve x value
  	while(Wire.available()) {   
		twi_reading[i] = Wire.receive();
		i++;
	}
	return *twi_reading;
}

//SHT
boolean MiniBee::getFlagSHT(void) { 
    return shtOn;
}
 
int* MiniBee::getPinSHT(void) {
	 return sht_pins; 
}

void MiniBee::setupSHT() {
// void MiniBee::setupSHT(int* pins) {
	//pins[0] is scl pins[1] is sda
// 	*sht_pins = *pins;
	pinMode(sht_pins[0], OUTPUT);
	digitalWrite(sht_pins[0], HIGH);     
	pinMode(sht_pins[1], OUTPUT);   
	startSHT();
}

void MiniBee::startSHT(void) {
	pinMode(sht_pins[1], OUTPUT);
	digitalWrite(sht_pins[1], HIGH);
	digitalWrite(sht_pins[0], HIGH);    
	digitalWrite(sht_pins[1], LOW);
	digitalWrite(sht_pins[0], LOW);
	digitalWrite(sht_pins[0], HIGH);
	digitalWrite(sht_pins[1], HIGH);
	digitalWrite(sht_pins[0], LOW);
}

void MiniBee::resetSHT(void) {
	shiftOut(sht_pins[1], sht_pins[0], LSBFIRST, 0xff);
	shiftOut(sht_pins[1], sht_pins[0], LSBFIRST, 0xff);
	startSHT();
}

void MiniBee::softResetSHT(void) {
	resetSHT();
	ioSHT = SHT_RST_CMD;
	ackSHT = 1;
	writeByteSHT();
	delay(15);
}

void MiniBee::waitSHT(void) {
	delay(5);
	i = 0;
	while(i < 600) {
		if(digitalRead(sht_pins[1]) == 0) i = 2600;
		delay(1);
		i++;
	}
}

void MiniBee::measureSHT(int cmd) {
	softResetSHT();
	startSHT();
	ioSHT = cmd;

	writeByteSHT();   
	waitSHT();          
	ackSHT = 0;      

	readByteSHT();
	int msby;                  
	msby = ioSHT;
	ackSHT = 1;

	readByteSHT();          
	valSHT = msby;           
	valSHT = valSHT * 0x100;
	valSHT = valSHT + ioSHT;
	if(valSHT <= 0) valSHT = 1;
}

void MiniBee::readByteSHT(void) {
	ioSHT = shiftInSHT();
	digitalWrite(sht_pins[1], ackSHT);
	pinMode(sht_pins[1], OUTPUT);
	digitalWrite(sht_pins[0], HIGH);
	digitalWrite(sht_pins[0], LOW);
	pinMode(sht_pins[1], INPUT);
	digitalWrite(sht_pins[1], LOW);
}

void MiniBee::writeByteSHT(void) {
	pinMode(sht_pins[1], OUTPUT);
	shiftOut(sht_pins[1], sht_pins[0], MSBFIRST, ioSHT);
	pinMode(sht_pins[1], INPUT);
	digitalWrite(sht_pins[1], LOW);
	digitalWrite(sht_pins[0], LOW);
	digitalWrite(sht_pins[0], HIGH);
	ackSHT = digitalRead(sht_pins[1]);
	digitalWrite(sht_pins[0], LOW);
}

int MiniBee::getStatusSHT(void) {
	softResetSHT();
	startSHT();
	ioSHT = SHT_R_STAT;	//R_STATUS

	writeByteSHT();
	waitSHT();
	ackSHT = 1;

	readByteSHT();
	return ioSHT;
}

int MiniBee::shiftInSHT(void) {
	int cwt = 0;
	for(mask = 128;mask >= 1;mask >>= 1) {
		digitalWrite(sht_pins[0], HIGH);
		cwt = cwt + mask * digitalRead(sht_pins[1]);
		digitalWrite(sht_pins[0], LOW);
	}
	return cwt;
}

//PING
boolean MiniBee::getFlagPing(void) { 
  return pingOn;
} 

int MiniBee::getPinPing(void) { 
	return ping_pin; 
}

// void MiniBee::setupPing(int *pins) {
// 	*ping_pins = *pins;	//ping pins = ping pins
// 	//no Ping setup
// }	

int MiniBee::readPing(void) {
	long ping;

	// The Devantech US device is triggered by a HIGH pulse of 10 or more microseconds.
	// We give a short LOW pulse beforehand to ensure a clean HIGH pulse.
	pinMode(ping_pin, OUTPUT);
	digitalWrite(ping_pin, LOW);
	delayMicroseconds(2);
	digitalWrite(ping_pin, HIGH);
	delayMicroseconds(11);
	digitalWrite(ping_pin, LOW);

	// The same pin is used to read the signal from the Devantech Ultrasound device: a HIGH
	// pulse whose duration is the time (in microseconds) from the sending
	// of the ping to the reception of its echo off of an object.
	pinMode(ping_pin, INPUT);
	ping = pulseIn(ping_pin, HIGH);
 
	//max value is 30000 so easily fits in an int
	return int(ping);
}

// //**** EVENTS ****//
// void MiniBee::setupDigitalEvent(void (*event)(int, int)) {
// 	dEvent = event;
// 	
// 	//set up digital interrupts 
// 	EICRA = (1 << ISC10) | (1 << ISC00);	
// 	PCICR = (1 << PCIE0);
// 	PCMSK0 = (1 << PCINT0);
// }
// 
// void MiniBee::digitalUpdate(int pin, int status) {
// 	(*dEvent)(pin, status);
// }
// 
// void PCINT0_vect(void) {
// 	Bee.digitalUpdate(0, PINB & (1 << PINB0));
// }

//serial rx event
/*void MiniBee::attachSerialEvent(void (*event)(void)) {
	//usart registers
	UCSR0B |= (1 << RXCIE0) | (1 << TXCIE0) | (1 << RXEN0) | (1 << TXEN0); //turn on rx + tx and enable interrupts
	UCSR0C |= (1 << UCSZ01) | (1 << UCSZ00);	//set frame format (81NN)
	UBRR0L = BAUD_PRESCALE; // Load lower 8-bits of the baud rate value into the low char of the UBRR register 
	UBRR0H = (BAUD_PRESCALE >> 8); // Load upper 8-bits of the baud rate value into the high char of the UBRR register
}

void USART_RX_vect(void) {
	
}*/

//**** REST OF THE EVENTS ****//
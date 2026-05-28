// Регистры MAX7219
#define REG_NOOP        0x00
#define REG_DIGIT0      0x01
#define REG_DIGIT1      0x02
#define REG_DIGIT2      0x03
#define REG_DIGIT3      0x04
#define REG_DIGIT4      0x05
#define REG_DIGIT5      0x06
#define REG_DIGIT6      0x07
#define REG_DIGIT7      0x08
#define REG_DECODEMODE  0x09
#define REG_INTENSITY   0x0A
#define REG_SCANLIMIT   0x0B
#define REG_SHUTDOWN    0x0C
#define REG_DISPLAYTEST 0x0F

#define maxChipNo 3 // максимальный номер MAX7219 в цепочке. Микросхема, вход DIN которой подключён к Arduino непосредственно, имеет номер 1. Следующая
                    // MAX7219, DIN которой подключён к DOUT первой MAX7219, имеет номер 2, и т.д. Самая дальняя от Arduino микросхема имеет номер maxChipNo

#include <SPI.h>

const uint8_t PIN_CS_MASK = 0b00000100; // маска, указывающая пин Ардуино, к которому подключена линия SS (LOAD) MAX7219 (0b00000100 соотв. пину 10)




void loop() {
    processSerial(); // обработка поступлений из последовательного порта
}



void setup() {
    // DDRB |= PIN_CS_MASK; // пин, указываемый маской, настроить как выход
    // PORTB |= PIN_CS_MASK; // пину, указываемому маской, задать высокий уровень (чтобы подключённый к этому пину slave SPI не был enabled)
    // Две строки выше не нужны, соответствующие операции есть в SPI.begin()
    
    Serial.begin(9600);
    SPI.begin();

    // SPI.setClockDivider(SPI_CLOCK_DIV32);  // 500 кГц вместо 4 МГц по умолчанию -- использовалось в отладке, оказалось ненужным

    // Инициализация всех MAX7219
    init7219Register(REG_SHUTDOWN,    1);  // нормальный режим (выход из shutdown)
    init7219Register(REG_DISPLAYTEST, 0);  // тестовый режим ВЫКЛ
    init7219Register(REG_DECODEMODE,  0);  // без декодирования
    init7219Register(REG_SCANLIMIT,   7);  // в динамической индикации участвуют все 8 общих катодов
    init7219Register(REG_INTENSITY,   1);  // яркость (0-15)
    
    erase(); // запись нулей в регистры 1-8 каждой MAX7219 
}


void erase() { // записывает нули в регистры 1-8 каждой MAX7219
 for (uint8_t i = 1; i < 9; i++) {
  PORTB &= ~PIN_CS_MASK; // установить низкий уровень на линии SS
  // __builtin_avr_nop(); // задержка на 1 такт процессора. Использовалось при отладке, не потребовалось.
  for (uint8_t chip =  maxChipNo; chip > 0; chip--) {SPI.transfer(i); SPI.transfer(0);}
  PORTB |= PIN_CS_MASK; // установить высокий уровень на линии SS
 }
}


void init7219Register(uint8_t reg, uint8_t val) { // записывает значение val в регистр reg всех MAX7219
  PORTB &= ~PIN_CS_MASK; // установить низкий уровень на линии SS
  for (uint8_t chip =  maxChipNo; chip > 0; chip--) {SPI.transfer(reg); SPI.transfer(val);}
  PORTB |= PIN_CS_MASK; // установить высокий уровень на линии SS
}



void setSlaveSelect(uint8_t val_byte) { // устанавливает лог. 0 на линии SS, если val_byte=0, и лог. 1 при любом другом значении
  if (val_byte) PORTB |= PIN_CS_MASK; else PORTB &= ~PIN_CS_MASK;
  // Serial.println("ss: " + val_byte);
}


void writeTo7219(uint8_t reg, uint8_t data) { // передача в MAX7219 -- правильно работает только при одной микросхеме в цепочке
    PORTB &= ~PIN_CS_MASK; // установить низкий уровень на линии SS и тем выбрать MAX7219 
    SPI.transfer(reg);     // передать адрес регистра, куда будут записываться данные
    SPI.transfer(data);    // передать данные
    PORTB |= PIN_CS_MASK;} // установить высокий уровень на линии SS и тем не выбрать MAX7219


/*
void writeTo7219InGroup(uint8_t chip, uint8_t reg, uint8_t data) { // передача в MAX7219 при нескольких микросхемах в цепочке
   PORTB &= ~PIN_CS_MASK; // установить низкий уровень на линии SS
   for (uint8_t i = maxChipNo; i > 0; i--) {
     if (i == chip) { 
       SPI.transfer(reg);
       SPI.transfer(data);
     } else {
       SPI.transfer(0);
       SPI.transfer(0);
     }
   }
   PORTB |= PIN_CS_MASK;} // установить высокий уровень на линии SS
*/

void writeTo7219InGroup(const uint8_t& chip, const uint8_t& reg, const uint8_t& data) { // передача в MAX7219 при нескольких микросхемах в цепочке
// Для всех микросхем, кроме заданной номером chip, передаются пустышки (0, any_value), означающие запись в регистр 0, и лищь для заданной
// микросхемы передаются фактические данные (регистр, значение). После усановки низкого уровня на SS в длинный сдвиговый регистр, образованный
// всеми последовательно соединёнными MAX7219, в нужной последовательности записываются (регистр, значение) и пустышки. После установки высокого уровня
// на SS микросхема с номером chip реагирует на переданные в неё адрес и данные, остальные микросхемы ничего не делают.  
// Для лучшего понимания логики работы см. выше закомментированный первый вариант этой функции

PORTB &= ~PIN_CS_MASK; // установить низкий уровень на линии SS
for (uint8_t i = maxChipNo; i > 0; i--) {
  SPI.transfer((i == chip) * reg); // i==chip равно 1 для i=chip и 0 для i<>chip, так передаётся reg для заданной мсх и 0 для остальных
  SPI.transfer(data);}
PORTB |= PIN_CS_MASK;} // установить высокий уровень на линии SS




void processSerial() { // приём данных через Serial и их обработка

//  Для управления вводят в мониторе последовательного порта команды вида параметр=значение. Монитор п.п. 
//  должен быть настроен на дополнение введённых данных символом новой строки \n.
//  Допустимые команды: nx=y, где
//  n -- номер микросхемы (один символ 1...maxChipNo, может быть опущен, тогда считается =1), 
//  x -- 16-ричный адрес регистра MAX7219 (один символ 0-f),
//  y -- 16-ричное значение, передаваемое в этот регистр (0-ff, 1 или 2 символа)
//  Например, 9=ff и, эквивалентно, 19=ff передадут в регистр 9 первой микросхемы значение ff,  
//  29=ff и 39=ff передадут в регистр 9 второй и третьей микросхемы значение ff.
//  Также поддерживаются команды ?= (вывод информации о программе и параметрах), 
//  s=значение (управление линией SS), w=значение (передача значения в SPI)


  if (Serial.available()) {
    String key = Serial.readStringUntil('=');         
    String val = Serial.readStringUntil('\n');
    if (key.length() > 2 or val.length() > 2) return;   // проверка длины  
    //  Serial.println("Key: " + key);                
    //  Serial.println("Val: " + val);                
    
    if (key == "?") { // обработка команды ?= 
     if (val == "") { // параметр пуст
       Serial.println(F("Ind-y-Ardu MAX7219_2: chained MAX7219 chips")); //F() -- это ардуинский макрос, помещающий строку
       Serial.println(F("nx=y command writes the byte y to the register x (0-f) of the n-th chip, e.g. 29=ff")); // во флэш-память вместо оперативной памяти, которой всего 2048 байт
       Serial.println(F("s and w are used to manually send a byte via SPI"));
       return;}
     if (val == "s") {
       Serial.println(F("s=0 sets SPI's SS line low, s=1 sets it high"));
       return;}
     if (val == "w") {
       Serial.println(F("w=byte sends the byte to SPI"));
       return;}  
     } // конец обработки команды ?=

    uint8_t val_byte;

    if (key == "w") { // передача val в SPI
      if ( hexStringToByte(val, val_byte) ) SPI.transfer(val_byte);
      return;
    }

    if (key == "s")  { // управление линией SS
      if ( hexStringToByte(val, val_byte) ) setSlaveSelect(val_byte);
      return;
    }

    String key0, key1;
    uint8_t chipNo = 1; // номер микросхемы: ближняя 1, следующая 2 и т.д.
    uint8_t key0_byte, key1_byte; // два байта, образующих key -- номер микросхемы и адрес регистра в ней
    
    // если длина key=1, то номером микросхемы считается 1
    if (key.length() == 2) {key0 = key[0]; key1 = key[1];} else {key0 = "1"; key1 = key[0];};
  
    if ( hexStringToByte(key0, key0_byte) and hexStringToByte(key1, key1_byte) and hexStringToByte(val, val_byte) ) writeTo7219InGroup(key0_byte, key1_byte, val_byte);

  } // if (Serial.available )
} //processSerial()




bool hexStringToByte(const String& hexStr, uint8_t& output) {
// преобразование 16-числа, представленного строкой из 1 или 2 символов 0-9,a-f, в байт 
    
    if (hexStr.length() == 0 or hexStr.length() > 2) return false;
    output = 0;
    
    for (uint8_t i = 0; i < hexStr.length(); i++) {
        char c = hexStr[i];
        uint8_t nibble;

        if (c >= '0' and c <= '9') nibble = c - '0';
        else if (c >= 'a' and c <= 'f') nibble = c - 'a' + 10;
        else if (c >= 'A' and c <= 'F') nibble = c - 'A' + 10;
        else return false;  // недопустимый символ
        
        output = (output << 4) | nibble;
    }
    
    return true;
}



/*
bool hexCharToByte(const char& c, uint8_t& output) {
//Преобразование символа '0'-'9', 'a'-'f', 'A'-'F' в число 0-15
    if (c >= '0' and c <= '9') {output = c - '0'; return true;};       // '0' → 0, '9' → 9
    if (c >= 'a' and c <= 'f') {output = c - 'a' + 10; return true;};  // 'a' → 10, 'f' → 15
    if (c >= 'A' and c <= 'F') {output = c - 'A' + 10; return true;};  // 'A' → 10, 'F' → 15
    return false;                                                      // недопустимый символ
} */
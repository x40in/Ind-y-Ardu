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

#include <SPI.h>

const uint8_t PIN_CS_MASK = 0b00000100; // маска, указывающая пин Ардуино, к которому подключена линия SS (LOAD) MAX7219 (0b00000100 соотв. пину 10)




void loop() {
    processSerial(); // обработка поступлений из последовательного порта
}



void setup() {
    Serial.begin(9600);
    
    SPI.begin();
    DDRB |= PIN_CS_MASK; // пин, указываемый маской, настроить как выход
    PORTB |= PIN_CS_MASK; // пину, указываемому маской, задать высокий уровень (чтобы подключённый к этому пину slave SPI не был enabled)
    
    // Инициализация MAX7219
    writeTo7219(REG_SHUTDOWN,    1);  // нормальный режим (выход из shutdown)
    writeTo7219(REG_DECODEMODE,  0);  // без декодирования (не нужно для матрицы)
    writeTo7219(REG_SCANLIMIT,   7);  // в динамической индикации участвуют все 8 общих катодов
    writeTo7219(REG_INTENSITY,   1);  // яркость (0-15)
    writeTo7219(REG_DISPLAYTEST, 0);  // тестовый режим ВЫКЛ

    clear(); // очистить матрицу
}



void writeTo7219(uint8_t reg, uint8_t data) { // передача в MAX7219
    PORTB &= ~PIN_CS_MASK; // установить низкий уровень на линии SS и тем выбрать MAX7219 
    SPI.transfer(reg);     // передать адрес регистра, куда будут записываться данные
    SPI.transfer(data);    // передать данные
    PORTB |= PIN_CS_MASK;} // установить высокий уровень на линии SS и тем не выбрать MAX7219



void clear() { // очистка матрицы
   for (uint8_t i = 0; i < 8; i++) writeTo7219(REG_DIGIT0 + i, 0);
}



void processSerial() { // приём данных через Serial и их обработка

//  Для управления вводят в мониторе последовательного порта команды вида параметр=значение. Монитор п.п. 
//  должен быть настроен на дополнение введённых данных символом новой строки \n.
//  Допустимые команды: x=y, где x -- 16-ричный адрес регистра MAX7219 (0-f, один или два символа, можно f или 0f),
//  y -- 16-ричное значение, передаваемое в этот регистр (0-ff, 1 или 2 символа) 

  if (Serial.available()) {
    String key = Serial.readStringUntil('=');         
    String val = Serial.readStringUntil('\n');
    if (key.length() > 2 or val.length() > 2) return;   // проверка длины  
    //  Serial.println("Key: " + key);                
    //  Serial.println("Val: " + val);                
    
    if (key == "?") { // обработка команды ?= 
     if (val == "") { // параметр пуст
       Serial.println(F("Ind-y-Ardu MAX7219_1: 8x8 matrix.")); //F() -- это ардуинский макрос, помещающий строку
       Serial.println(F("Params are the numbers of MAX7219 registers: 0..f. Values allowed: 00..ff. ?=param<ENTER> doesn't work in this program.")); // во флэш-память вместо оперативной памяти, которой всего 2048 байт
     }} // конец обработки команды ?=

    uint8_t key_byte, val_byte;
    if ( hexStringToByte(key, key_byte) and hexStringToByte(val, val_byte) ) writeTo7219(key_byte, val_byte); // передача значения в регистр MAX7219
  }
}



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
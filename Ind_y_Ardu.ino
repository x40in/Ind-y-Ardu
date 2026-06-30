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

#define MAX_CHIP_NO 3 // максимальный номер MAX7219 в цепочке. Микросхема, вход DIN которой подключён к Arduino непосредственно, имеет номер 1. Следующая
                      // MAX7219, DIN которой подключён к DOUT первой MAX7219, имеет номер 2, и т.д. Самая дальняя от Arduino микросхема имеет номер MAX_CHIP_NO

#include <SPI.h>

struct DisplayAddress { // структура для указания адреса на индикаторе
    uint8_t chip;    // физический индикатор (1-MAX_CHIP_NO)
    uint8_t digit;   // разряд (1-8)
};


const DisplayAddress SECONDS_ADDRESS_MAP[5] PROGMEM = { // Адреса на индикаторе для каждой позиции блока символов seconds
    {2, 2},   // seconds[0] → индикатор 2, разряд 3
    {2, 3},   // seconds[1] → индикатор 2, разряд 4
    {2, 4},   // и т.д.
    {2, 5},
    {2, 6}
};

const DisplayAddress RANDOM_N_ADDRESS_MAP[3] PROGMEM = { // Адреса на индикаторе для каждой позиции блока символов randomNumber
    {1, 8},  
    {2, 8},  
    {3, 8},
};

const uint8_t PIN_CS_MASK = 0b00000100; // маска, указывающая пин Ардуино, к которому подключена линия SS (LOAD) MAX7219 (0b00000100 соотв. пину 10)
uint8_t buffer[][11] = {{0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0}};
bool tick = false; // флаг тика таймера
uint8_t randomCounter = 0; // счётчик прерываний, используется дла определения момента отображения случайного числа
uint8_t maxRandomCounter = random(20, 256); // псевдослучайное число, определяющее момент отображения псевдосл. числа randomNumber
uint8_t seconds[5] = {0, 0, 0, 0, 0}; // блок символов -- цифр количества секунд. Байт 0 соответствует младшей цифре.
uint8_t randomNumber[3] = {1, 2, 3}; // блок символов -- цифр случайного числа



ISR(TIMER1_COMPA_vect)  //ISR для режима CTC
{
 tick = true; // установить флаг
}



void loop() {
    // processSerial(); // обработка поступлений из последовательного порта
    if (tick) { // событие таймера
                tick = false; // снять флаг
                updateSecondsZone(updateSeconds(0)); // сформировать новое показание счётчика секунд и вывести его в соотв. зону
                PORTB ^= 0b00000001; // инвертировать пин 8 (контроль частоты);
       
                if (randomCounter == maxRandomCounter) { // если счётчик тиков достиг максимума, псевдослучайно заданного ранее
                                                         randomCounter = 0; // обнулить счётчик
                                                         maxRandomCounter = random(20, 128); // задать новое значение для maxRandomCounter
                                                         uint8_t n = intToDigits(random(0, 1000), randomNumber, 3); // определить псевдосл. число для вывода на индикатор
                                                         updateRandomZone(2); } // обновить все три цифры зоны для псевдосл. числа
                                                       else randomCounter += 1; 
                display(); };
}



void setup() {
    // Настроить таймер 1 в режим CTC, частота 10 Гц
    cli(); // запретить прерывания
    TCCR1A = 0;     
    TCCR1B = (1 << WGM12) | (1 << CS12); // включить режим CTC и задать коэф-т деления 256
    // TCCR1B = (1 << WGM12) | (1 << CS11); // включить режим CTC и задать коэф-т деления 8
    OCR1A = 6249;    // задать максимальное значение счётчика (6249 в сочетании с коэф.дел. 256 даёт частоту прерываний 10 Гц)
    // OCR1A = 19999; // задать максимальное значение счётчика (19999 в сочетании с коэф.дел. 8 даёт частоту прерываний 100 Гц)
    TIMSK1 = (1 << OCIE1A);    // Разрешить прерывание по совпадению значения в счётчике со значением в компараторе
    PORTB &= 0b11111110; // низкий уровень на пин 8 (контроль частоты)
    sei(); // разрешить прерывания
    
    // Serial.begin(9600);
    SPI.begin();

    // Инициализация всех MAX7219
    init7219Register(REG_SHUTDOWN,    1);  // нормальный режим (выход из shutdown)
    init7219Register(REG_DISPLAYTEST, 0);  // тестовый режим ВЫКЛ
    init7219Register(REG_DECODEMODE,  0);  // без декодирования
    init7219Register(REG_SCANLIMIT,   7);  // в динамической индикации участвуют все 8 общих катодов
    init7219Register(REG_INTENSITY,   1);  // яркость (0-15)
    
    randomSeed(analogRead(0)); // засевание генератра псевдослучайных чисел начальным случайным числом

    wipe();
}



void wipe() {
// переводит все MAX7219 в режим с декодированием и гасит все индикаторы
  for (uint8_t chip =  MAX_CHIP_NO; chip > 0; chip--) {
    writeToBuffer(chip, 9, 0xFF); // включить режим с декодированием
    for (uint8_t digit = 1; digit < 9; digit++) writeToBuffer(chip, digit, 0x0F); // погасить
  }
  display(); // выдать
}



void init7219Register(uint8_t reg, uint8_t val) {
// записывает значение val в регистр reg всех MAX7219
  PORTB &= ~PIN_CS_MASK; // установить низкий уровень на линии SS
  for (uint8_t chip =  MAX_CHIP_NO; chip > 0; chip--) {SPI.transfer(reg); SPI.transfer(val);}
  PORTB |= PIN_CS_MASK; // установить высокий уровень на линии SS
}



uint8_t updateSeconds(uint8_t N) {
// увеличивает на 1 N-й разряд десятичного числа, содержащегося в массиве seconds[5] в виде пяти байтов, соответствующих цифрам этого числа,
// и, при необходимости, старшие разряды. Разряды считаются с нуля (seconds[0] -- младший разряд)
// Возвращает номер самого старшего изменённого разряда (т.е. если изменён только младший разряд, то 0, если 2 младших разряда, то 1, и если все разяды, то 4)

  if (N > 4) return 4;          // условие выхода из рекурсивного цикла -- выход за верхнюю границу массива

// (а) Если цифра N, подлежащая увеличению, не 9, то надо лишь увеличить её
// (б) Если цифра N равна 9, то её увеличение влечёт увеличение соседнего старшего разряда, и, возможно, других старших разрядов. В этом случае
// текущее значение 9 в seconds заменить на 0 и рекурсивно вызвать эту функцию для соседнего старшего разряда.

  if (seconds[N] == 9) { seconds[N] = 0; updateSeconds(N+1); } else { seconds[N] += 1; return N; };

}



void updateSecondsZone(uint8_t noOfDigits) {
// Записывает данные секундомера в буфер составного индикатора, в зону, заданную в SECONDS_ADDRESS_MAP
  DisplayAddress addr;
  for (uint8_t i=0; i < noOfDigits + 1; i++) { 
    addr.chip = pgm_read_byte(&SECONDS_ADDRESS_MAP[i].chip); // pgm_read_byte() is a macro used to read a single byte of data from Flash memory
    addr.digit = pgm_read_byte(&SECONDS_ADDRESS_MAP[i].digit); 
    writeToBuffer(addr.chip, addr.digit, seconds[i] + 0x80*(i==1)); // 0x80*(i==1) включает десятичную точку при i=1, т.е. в разряде единиц секунд
  }
}


void updateRandomZone(uint8_t noOfDigits) {
// Записывает случайное число в буфер составного индикатора, в зону, заданную в ANDOM_N_ADDRESS_MAP  
  DisplayAddress addr;
  for (uint8_t i=0; i < noOfDigits + 1; i++) {
    addr.chip = pgm_read_byte(&RANDOM_N_ADDRESS_MAP[i].chip);
    addr.digit = pgm_read_byte(&RANDOM_N_ADDRESS_MAP[i].digit);
    writeToBuffer(addr.chip, addr.digit, randomNumber[i]);
  }  
}




void writeToBuffer(const uint8_t& chip, const uint8_t& reg, const uint8_t& data) {
// записывает data (0...ff) в буфер составного индикатора -- в регистр reg (1...9) физического индикатора chip (1...MAX_CHIP_NO)  
  if (reg < 1 or reg > 9 or chip < 1 or chip > MAX_CHIP_NO) return;           // проверка попадания в диапазон
  if ( buffer[chip-1][reg] == data ) return;                                  // если в буфере уже нужный байт, то ничего делать не нужно
  buffer[chip-1][reg] = data;                                                 // запись data в буфер
  if (reg == 9) buffer[chip-1][10] = 1; else buffer[chip-1][0] |= 1 << reg-1; // установка флага новизны, соответствующего reg и chip
};



void display() { 
// выводит изменения, сделанные в буфере и отмеченные флагами новизны, на индикатор
  while (displayRequired()) { // циклировать, пока все изменения, сделанные в буфере, не будут переданы в индикатор, а флаги не будут сняты
    PORTB &= ~PIN_CS_MASK; // установить низкий уровень на линии SS
    for (uint8_t chip = MAX_CHIP_NO; chip > 0; chip--) { // начиная с самого дальнего субиндикатора передать адреса и данные для отображения
      uint8_t address = getFlagged(chip-1);  // пробежаться по флаговым полям текущей строки буфера, найти адрес (1..8 или 9) ещё не отображённого изменения
      SPI.transfer(address);                 // передать найденный адрес
      SPI.transfer(buffer[chip-1][address]); // передать данные, содержащиеся в буфере по найденному адресу
      resetFlag(chip-1, address);              // сбросить флаг, соответствующий только что переданному адресу
    }
    PORTB |= PIN_CS_MASK; // установить высокий уровень на линии SS    
  }
}


bool displayRequired() { 
// установлен ли в буфере хотя бы один флаг ?
  for (uint8_t chip = 0; chip < MAX_CHIP_NO; chip++) if (buffer[chip][0] or buffer[chip][10]) return true;
  return false;
}


uint8_t getFlagged(uint8_t chip) {
// возвращает адрес (первого попавшегося) регистра, отмеченного флагом (подлежащего обновлению на индикаторе), для физ.индикатора chip (chip=1..MAX_CHIP_NO)
  if ( not(buffer[chip][0] or buffer[chip][10]) ) return 0; // если ни один флаг не установлен
  if ( buffer[chip][0] ) return firstSetFlag(buffer[chip][0]); // если есть флаги для регистров 1-8
  if ( buffer[chip][10] ) return 9;                            // если есть флаг для регистра 9
}



uint8_t firstSetFlag(uint8_t flags) {
// часть фукнции getFlagged для регистров 1-8
  if (not flags) return 0;  // ни один флаг не установлен
  for (uint8_t pos = 0; pos < 8; pos++) if (flags & (1 << pos)) return pos + 1;  // +1 потому, что адреса 1-8, а не 0-7
}



void resetFlag(uint8_t chip, uint8_t addr) {
// сбрасывает флаг для адреса addr в физ.индикаторе chip
  if (addr==9) { buffer[chip][10]=0; return; } // если получен адрес регистра режима отображения, сбросить флаг (весь байт 10) и выйти
  if (addr > 0 and addr < 9) buffer[chip][0] &= ~( 1<<(addr-1) ); // если адрес относится к регистрам 1-8, то сбросить флаг, соответствующий регистру (бит в байте 0)
}




bool hexStringToByte(const String& hexStr, uint8_t& output) {
// преобразует 16-число, представленное строкой из 1 или 2 символов 0-9,a-f, в байт 
    
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




uint8_t intToDigits(uint32_t value, uint8_t digits[], uint8_t maxDigits) {
// преобразует целое value в массив его десятичных цифр, возвращает количество цифр, 0 при ошибке
    if (maxDigits == 0) return 0;
    uint8_t count = 0;
    
    do { if (count >= maxDigits) return 0;
         digits[count++] = value % 10;
         value /= 10;
       } while (value > 0);
    
    return count;
}



/*

Закомментированы подпрограммы, использовавшиеся при отладке, и отдельные перспективные варианты


void printChipBuffer(uint8_t chip) {
// вывод в Serial строки chip из буфера составноо индикатора  
  Serial.print(chip);
  Serial.print(":  ");
  for (uint8_t i = 0; i < 11; i++) {Serial.print(buffer[chip-1][i]); Serial.print(" ");}
  Serial.println();
}



void printWholeBuffer() {
// вывод в Serial всего буфера составноо индикатора 
  for (uint8_t chip = 1; chip < MAX_CHIP_NO + 1; chip++) printChipBuffer(chip);
}



void showStaticTest() {
// отображение на индикаторе статического теста. Данные для него в этой константе:
// const uint8_t staticTest[][11] = {{0xff,7,0x40,0x40,0x40,0x40,0x40,0x40,0x46,1,1},{0xff,1,0,4,3,2,1,0,6,0x3d,1},{0xff,0x38,8,8,8,8,8,8,0x0e,0,1}};
  for (uint8_t i = 0; i < MAX_CHIP_NO; i++) {
    for (uint8_t j = 0; j < 11; j++) {
      buffer[i][j] = staticTest[i][j]; 
    }
  }
  // display();
}



void processSerial() { // приём данных через Serial и их обработка


// ***** В окончательной версии вообще убрана работа с последовательным портом, а при отладке он интенсивно использовался.
// ***** но многое осталось от предыдущего подпроекта и к этому подпроекту отношение имеет слабое

//  Для управления вводят в мониторе последовательного порта команды вида параметр=значение. Монитор п.п. 
//  должен быть настроен на дополнение введённых данных символом новой строки \n.
//  
//  Команда записи в буфер составного индикатора nx=y, где
//  n -- номер физического индикатора (один символ 1...MAX_CHIP_NO, может быть опущен, тогда считается =1), 
//  x -- адрес регистра MAX7219 этого индикатора (один символ 0-f),
//  y -- 16-ричное значение указанного регистра, подлежащее записи в буфер (0-ff, 1 или 2 символа)



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

    if (key == "d")  { // вывод на индикатор
      if ( val == "0" ) {display(); return;}
      // if ( val == "1" ) {setDisplayModes(); return;}
      if ( val == "8" ) {showStaticTest(); return;}
      if ( val == "9" ) {printWholeBuffer(); return;}
    }

    String key0, key1;
    uint8_t chipNo = 1; // номер микросхемы: ближняя 1, следующая 2 и т.д.
    uint8_t key0_byte, key1_byte; // два байта, образующих key -- номер микросхемы и адрес регистра в ней
    
    // если длина key=1, то номером микросхемы считается 1
    if (key.length() == 2) {key0 = key[0]; key1 = key[1];} else {key0 = "1"; key1 = key[0];};
  
    // if ( hexStringToByte(key0, key0_byte) and hexStringToByte(key1, key1_byte) and hexStringToByte(val, val_byte) ) writeTo7219InGroup(key0_byte, key1_byte, val_byte);
    if ( hexStringToByte(key0, key0_byte) and hexStringToByte(key1, key1_byte) and hexStringToByte(val, val_byte) ) writeToBuffer(key0_byte, key1_byte, val_byte);

  } // if (Serial.available )
} //processSerial()
*/





/*
bool hexCharToByte(const char& c, uint8_t& output) {
//Преобразование символа '0'-'9', 'a'-'f', 'A'-'F' в число 0-15
    if (c >= '0' and c <= '9') {output = c - '0'; return true;};       // '0' → 0, '9' → 9
    if (c >= 'a' and c <= 'f') {output = c - 'a' + 10; return true;};  // 'a' → 10, 'f' → 15
    if (c >= 'A' and c <= 'F') {output = c - 'A' + 10; return true;};  // 'A' → 10, 'F' → 15
    return false;                                                      // недопустимый символ
} */


/* Альтернативный вариант таблицы адресов, компактный
// Если строк ≤4 и разрядов ≤16, адрес можно упаковать в 1 байт
// Старшие 2 бита — строка, младшие 4 — разряд

const uint8_t ADDRESS_MAP_COMPACT[BUFFER_SIZE] PROGMEM = {
    0x02,  // 0b00000010 → row 0, digit 2
    0x05,  // 0b00000101 → row 0, digit 5
    0x10,  // 0b00010000 → row 1, digit 0
    0x13,  // 0b00010011 → row 1, digit 3
    // ...
};

// Распаковка:
uint8_t row   = (addr >> 4) & 0x03;  // старшие 2 бита
uint8_t digit = addr & 0x0F;          // младшие 4 бита




void setSlaveSelect(uint8_t val_byte) { // устанавливает лог. 0 на линии SS, если val_byte=0, и лог. 1 при любом другом значении
  if (val_byte) PORTB |= PIN_CS_MASK; else PORTB &= ~PIN_CS_MASK;
  // Serial.println("ss: " + val_byte);
}



void writeTo7219(uint8_t reg, uint8_t data) { // передача в MAX7219 -- правильно работает только при одной микросхеме в цепочке
    PORTB &= ~PIN_CS_MASK; // установить низкий уровень на линии SS и тем выбрать MAX7219 
    SPI.transfer(reg);     // передать адрес регистра, куда будут записываться данные
    SPI.transfer(data);    // передать данные
    PORTB |= PIN_CS_MASK;} // установить высокий уровень на линии SS и тем не выбрать MAX7219



void writeTo7219InGroup(uint8_t chip, uint8_t reg, uint8_t data) { // передача в MAX7219 при нескольких микросхемах в цепочке
   PORTB &= ~PIN_CS_MASK; // установить низкий уровень на линии SS
   for (uint8_t i = MAX_CHIP_NO; i > 0; i--) {
     if (i == chip) { 
       SPI.transfer(reg);
       SPI.transfer(data);
     } else {
       SPI.transfer(0);
       SPI.transfer(0);
     }
   }
   PORTB |= PIN_CS_MASK;} // установить высокий уровень на линии SS
 


void writeTo7219InGroup(const uint8_t& chip, const uint8_t& reg, const uint8_t& data) { // передача в MAX7219 при нескольких микросхемах в цепочке
// Для всех микросхем, кроме заданной номером chip, передаются пустышки (0, any_value), означающие запись в регистр 0, и лищь для заданной
// микросхемы передаются фактические данные (регистр, значение). После усановки низкого уровня на SS в длинный сдвиговый регистр, образованный
// всеми последовательно соединёнными MAX7219, в нужной последовательности записываются (регистр, значение) и пустышки. После установки высокого уровня
// на SS микросхема с номером chip реагирует на переданные в неё адрес и данные, остальные микросхемы ничего не делают.  
// Для лучшего понимания логики работы см. выше закомментированный первый вариант этой функции

PORTB &= ~PIN_CS_MASK; // установить низкий уровень на линии SS
for (uint8_t i = MAX_CHIP_NO; i > 0; i--) {
  SPI.transfer((i == chip) * reg); // i==chip равно 1 для i=chip и 0 для i<>chip, так передаётся reg для заданной мсх и 0 для остальных
  SPI.transfer(data);}
PORTB |= PIN_CS_MASK;} // установить высокий уровень на линии SS



void erase() {
// записывает нули в регистры 1-8 каждой MAX7219, что в режиме без декодирования,  
// устанавливающемся в MAX7219 при включении, эквивалентно гашению всех индикаторов
 for (uint8_t i = 1; i < 9; i++) {
  PORTB &= ~PIN_CS_MASK; // установить низкий уровень на линии SS
  for (uint8_t chip =  MAX_CHIP_NO; chip > 0; chip--) {SPI.transfer(i); SPI.transfer(0);}
  PORTB |= PIN_CS_MASK; // установить высокий уровень на линии SS
 }
}



uint8_t getFlagged(uint8_t chip) {
// находит адрес регистра, отмеченного флагом (подлежащего обновлению на индикаторе), для субиндикатора chip (chip=1..MAX_CHIP_NO)
  if ( not(buffer[chip-1][0] or buffer[chip-1][10]) ) return 0; // если ни один флаг не установлен
  if ( buffer[chip-1][0] ) return firstSetFlag(buffer[chip-1][0]);
  if ( buffer[chip-1][10] ) return 9;
}



void resetFlag(uint8_t chip, uint8_t addr) {
// сбрасывает флаг для субиндикатора chip и адреса addr  
  if (addr==9) { buffer[chip-1][10]=0; return; } // если получен адрес регистра режима отображения, сбросить флаг (весь байт 10) и выйти
  if (addr > 0 and addr < 9) { buffer[chip-1][0] &= ~( 1<<(addr-1) ); return; } // если адрес относится к регистрам 1-8, то сбросить флаг, соответствующий регистру (бит в байте 0)
}



void setDisplayModes() { //передаёт в индикатор из буфера состояния регистра 9 режима отображения - только для тех субиндикаторов, для которых это состояние изменилось
  PORTB &= ~PIN_CS_MASK; // установить низкий уровень на линии SS
  for (uint8_t chip = MAX_CHIP_NO; chip > 0; chip--) {  //цикл по субиндикаторам
      if (buffer[chip-1][10]) SPI.transfer(9); else SPI.transfer(0); // если установлен флаг изменения способа декодировния, то передать адрес 9, иначе 0 (пустышка)
      SPI.transfer(buffer[chip-1][9]); // передать новые данные для регистра 9 режима отображения (произвольные данные в случае пустышки)
      buffer[chip-1][10] = 0;} // сбросить флаг изменения способа декодирования (в т.ч. у субиндикаторов, где он и не был установлен -- просто чтобы не делать лишнюю проверку)
  PORTB |= PIN_CS_MASK; // установить высокий уровень на линии SS

// Интересное наблюдение. Первоначально цикл был таким: for (uint8_t chip = MAX_CHIP_NO-1; chip >= 0; chip--) {...,
// а обращения к буферу были такими: buffer[chip][y], а не buffer[chip-1][y] как сейчас.
// Вроде бы конструкция эквивалентная и более рациональная, т.к. не содержит в цикле вычитаний chip-1, но она не работала.
// После команды d=1 программа подвисала, явно обратившись куда-то за пределы буфера. Ничего видимого не происходило, реакции на ввод команды ?= не было  
}
*/
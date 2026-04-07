const byte mask = 0b00100000;  // маска бита 5 в PORTB
uint16_t counterInitValue = 34286; // начальное значение, записываемое в счётчик в режиме Normal (34286 в сочетании с коэф.дел. 256 даёт 1 Гц)
const String modes[2] = {"0", "1"}; // возможные значения режима, передаваемые в команде m=значение
constexpr size_t length_modes = sizeof(modes) / sizeof(modes[0]); //количество элементов массива modes. length на константных массивах не работает.
                                                                  //size_t — тип, предназначенный для хранения размеров и индексов в памяти
                                                                  //constexpr — это ключевое слово С++11, которое говорит компилятору:
                                                                  //«вычисли значение во время компиляции, если возможно, и сделай его константой».  
byte mode = 0;  // Режим Normal (0) или CTC (1)



void setup() { // Инициализация
 
 Serial.begin(9600);
 cli(); // запретить прерывания
 DDRB |= mask;   // пин 13 (PortB5) настроить как выход
 
 // Инициализировать регистры TCCR1x управления таймером (Timer1/Counter Control Register) для режима Normal
 TCCR1A = 0;     
 TCCR1B = 0;     // 0 делает таймер неактивным
 TIMSK1 = 0;     // 0 отключает все виды прерываний от таймера
 TCNT1 = counterInitValue;  // задать начальное значение счётчика (34286 в сочетании с коэф.дел. 256 даёт 1 Гц)
 TCCR1B |= (1 << CS12);     // Задать коэф-т деления 256, при котором светодиод мигает приблизительно 1 раз в секунду (эквивалентно TCCR1B |= 4)
 TIMSK1 |= (1 << TOIE1);    // Разрещить прерывание по переполнению счётчика
 sei(); // разрешить прерывания    
}

/*
|  2   |  1   |  0   | <-- номер бита в TCCR1B (Timer1/Counter Control Register B)
| CS12 | CS11 | CS10 | Режим/ коэф-т деления                             
|:----:|:----:|:----:|:-----------------------------------
|   0  |   0  |   0  | Таймер остановлен                  
|   0  |   0  |   1  | CLK/1 (нет предделителя)           
|   0  |   1  |   0  | CLK/8                              
|   0  |   1  |   1  | CLK/64                             
|   1  |   0  |   0  | CLK/256                            
|   1  |   0  |   1  | CLK/1024                           
|   1  |   1  |   0  | Внешний тактовый сигнал на T1, срез
|   1  |   1  |   1  | Внешний тактовый сигнал на T1, фронт

*/

ISR(TIMER1_OVF_vect) //ISR для режима Normal
{
 PORTB ^= mask;             // инвертировать бит 5 в PORTB, тем самым изменить состояние светодиода
 TCNT1 = counterInitValue;  // записать начальное значение в счётчик
}


ISR(TIMER1_COMPA_vect)  //ISR для режима CTC
{
 PORTB ^= mask;             // инвертировать бит 5 в PORTB, тем самым изменить состояние светодиода
}


void loop() { // Основной цикл
  processSerial();
}



void processSerial() { // Принимает данные через Serial и обрабатывает их

 /* Для управления вволят в мониторе последовательного порта команды вида параметр=значение. Монитор п.п. 
 должен быть настроен на дополнение введённых данных символом новой строки \n.
 Допускаются следующие команды:
 * d (divider, делитель) -- задаёт режим работы делителя, 
 * с (counter, счётчик)  -- задаёт начальное значение счётчика.  */ 
 
 if (Serial.available()) {
    String key = Serial.readStringUntil('=');         
    String val = Serial.readStringUntil('\n');        
    //  Serial.println("Key: " + key);                
    //  Serial.println("Val: " + val);                
    
    if (key == "?") { // обработка команды ?
     if (val == "") {
       Serial.println(F("Ind-y-Ardu builtin23: Clock freq division in Normal and CTC modes.")); //F() -- это ардуинский макрос, помещающий строку
       Serial.println(F("Params: m, c, d. Use ?=param<ENTER> to get help, e.g. ?=c.")); // во флэш-память вместо оперативной памяти, которой всего 2048 байт
     }
     if (val == "d") {
       Serial.println(F("d=1, 10, 11, 100 or 101 sets the prescaler's division factor 1, 8, 64, 256 or 1024"));
     }
     if (val == "c") {
       Serial.println(F("c=1..65535 loads the value to the counter or comparator deprnding of the mode (ask ?=m)"));
     }
    if (val == "m") {
       Serial.println(F("m=0 or 1 sets Normal or CTC mode"));
     } 
    }
    
    if (key == "m" and isAllowed(val)) {  // обработка команды m=значение (задание режима Normal или CTC)
     mode = val[0] - '0';
     Serial.print(F("mode: "));
     Serial.println(mode); 
    }

    if (key == "d") {  // обработка команды d=значение (установка коэф-та деления)
     byte value = StrToBin(val);   
     if (value > 0) {
       TCCR1B &= 0b11111000;  // очистка трёх младших битов в TCCR1B, задающих коэф-т деления
       TCCR1B |= value;       // установка коэф-та деления   
       Serial.print(F("TCCR1B: "));
       Serial.println(val);
     }
    }
    
    if (key == "c") {  // обработка команды c=значение (установка нач.знач. счётчика в режиме Normal или значения в компараторе в режиме CTC)
     cli();          // запретить все прерывания
     TIMSK1 = 0; // запретить обработку прерываний от таймера
     uint16_t X = StrTo16bit(val); // принятое значение
     if (mode == 0) {  // режим Normal         
      TCCR1B &= ~(1 << WGM12); // выключить режим CTC
      counterInitValue = X; // начальное значение счётчика = принятое значение            
      TIMSK1 |= (1 << TOIE1);    // Разрещить прерывание по переполнению счётчика
      Serial.print(F("TCNT1: "));
     }
     if (mode == 1) {  // режим CTC         
      TCCR1B |= (1 << WGM12); // включить режим CTC
      OCR1A = X; // значение в компараторе = принятое значение            
      TIMSK1 |= (1 << OCIE1A);    // Разрешить прерывание по совпадению значения в счётчике со значением в компараторе
      Serial.print(F("OCR1A: "));
     }
     Serial.println(X);
     sei();          // разрешить прерывания 
    }
 }
}


uint16_t StrTo16bit(String inString) { // Преобразует входную строку в 16-разрядное беззнаковое целое

  /* Функция принимает строку из 1-5 цифр и преобразует её в 16-разрядное беззнаковое целое как двоичное число.
  Если передано 0 или более 5 символов, если среди символов есть отличные от цифр, или если результат преобразования >65535, то результат 0 */
 
 int len = inString.length();
 if (len < 1 or len > 5) return 0;
 
 long value = inString.toInt();
 if (value < 0 or value > 65535) return 0;

 return (uint16_t)value;

}


byte StrToBin(String inString) { // Преобразует входную строку в байт

  /* Функция принимает строку из нулей и единиц длиной 1-3 символа и преобразует её в байт как двоичное число
  Если передано 0 или более 3 символов, если среди символов есть отличные от 0 и 1, то результат 0 */

 int len = inString.length();
 if (len < 1 or len > 3) return 0;
 byte result = 0;
 byte mask = 1;
 for (int i = len - 1; i >= 0; i--) {
  if (inString[i] != '0' and inString[i] != '1') return 0;
  if (inString[i] == '1') result = result|mask;
  mask = mask << 1;
 };
 return result;  
}

bool isAllowed(String value) { // Проверяет, находится ли принятая строка value в списке modes
 bool result = false;
 for (byte i = 0; i < length_modes; i++) { 
   result = (value == modes[i]);
   if (result) break;
 }
 return result; 
}

/* Иллюстративная программа. Мигает встроенным светодиодом под управлением 16-разрядного таймера. Можно задать кожффициент деления предделителя (команда d=);
 режим работы таймера -- Normal или CTC (команда m=); число, записываемое в счётчик в качестве начального значения в режиме Normal или в компаратор в качестве 
 значения для сравнения в режиме CTC */
 
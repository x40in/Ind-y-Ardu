const byte mask = 0b00100000;  // маска бита 5 в PORTB
uint16_t counterInitValue = 0; // начальное значение счётчика

void setup() { // Инициализация
 Serial.begin(9600);
 DDRB |= mask;   // пин 13 (PortB5) настроить как выход
 
 // Инициализировать регистры TCCR1x управления таймером (Timer1/Counter Control Register)
 TCCR1A = 0;     // TCCR1A не используется в этой программе, но инициализировать надо
 TCCR1B = 0;     // В TCCR1B в этой программе используются только 3 младших бита, управляющие делителем частоты. 0 делает таймер неактивным

 // Настроить таймер
 TCCR1B |= (1 << CS12);  // Задать коэф-т деления 256, при котором светодиод мигает приблизительно 1 раз в секунду (эквивалентно TCCR1B |= 4)
 TIMSK1 |= (1 << TOIE1);  // Разрещить прерывание по переполнению счётчика              
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

ISR(TIMER1_OVF_vect) // действия при переполнении счётчика
{
 PORTB ^= mask;             // инвертировать бит 5 в PORTB, тем самым изменить состояние светодиода
 TCNT1 = counterInitValue;  // записать начальное значение в счётчик
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
       Serial.println(F("Ind-y-Ardu builtin22: Clock freq division while pre-loading the counter in each interrupt.")); //F() -- это ардуинский макрос, помещающий строку
       Serial.println(F("Params: c, d. Use ?=param<ENTER> to get help, e.g. ?=c.")); // во флэш-память вместо оперативной памяти, которой всего 2048 байт
     }
     if (val == "d") {
       Serial.println(F("d=1, 10, 11, 100 or 101 sets the prescaler's division factor 1, 8, 64, 256 or 1024"));
     }
     if (val == "c") {
       Serial.println(F("c=1..65535 loads the initial value to the counter"));
     }
    }
    
    if (key == "d") {  // обработка команды d=значение
     byte value = StrToBin(val);   // обработка команды d=значение (установка коэф-та деления)
     if (value > 0) {
       TCCR1B = 0;                   // очистка битов установки коэф-та деления
       TCCR1B |= value;           
       Serial.print("TCCR1B: ");
       Serial.println(val);
     }
       
    }
    
    if (key == "c") {  // обработка команды c=значение
     counterInitValue = StrTo16bit(val);           
     Serial.print("TCNT1: ");
     Serial.println(counterInitValue); 
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




 
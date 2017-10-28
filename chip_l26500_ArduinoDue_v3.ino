#define CHIP_EEPROM_READ 0xBF
#define CHIP_EEPROM_WRITE 0xBE

#define _BV(bit) (1 << (bit))

#define sda   PIO_PB12
#define scl   PIO_PB13

// Управление пинами для записи
#define setscl  (PIOB->PIO_SODR = scl)
#define clrscl  (PIOB->PIO_CODR = scl)
#define setsda  (PIOB->PIO_SODR = sda)
#define clrsda  (PIOB->PIO_CODR = sda)

#define rxsda  (PIOB->PIO_ODR = sda)
#define txsda  (PIOB->PIO_OER = sda)

// Параметры i2c
#define i2c_time  6
//#define i2c_time  12

// Переменные
char data_hex[7];
uint8_t eeprom[128]; // Массив, хранящий данные EEPROM
char convertByte[2]; // Массив для конвертации вводимых через UART байта
byte data_write_row[4] = {0, 0, 0, 0}; // Массив для записи 4-х байт

// -------------------------------------------------------
// Тестовый массив, удалить после теста
// -------------------------------------------------------
uint8_t eeprom_test[] = {
  0xEF, 0x9F, 0x03, 0x85, 0x4F, 0xAA, 0x5D, 0x33, 0x97, 0x2A, 0x65, 0xCC, 0x2B, 0xFF, 0x00, 0x70, 
  0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x10, 0xAA, 0xB4, 0x56, 0x80, 0x1C, 0x17, 
  0x81, 0x81, 0x58, 0x40, 0x49, 0x88, 0xB0, 0x37, 0xB1, 0x90, 0x46, 0x04, 0x8A, 0x0A, 0x2D, 0x3F, 
  0x46, 0x32, 0x6C, 0x64, 0xE0, 0x00, 0x60, 0x5A, 0xA5, 0x2D, 0xF9, 0x0E, 0x6D, 0x8E, 0xE5, 0xD0, 
  0x62, 0x10, 0x7F, 0xC6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x23, 0x92, 0x00, 0x00, 0x1E, 0x00, 0x18, 0x00, 
  0x93, 0xB0, 0x17, 0x46, 0x00, 0x00, 0x1D, 0xAE, 0x8F, 0xD0, 0x53, 0xB0, 0xCF, 0xC2, 0x67, 0x00, 
  0x00, 0x00, 0x00, 0x09, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x1E, 0xFF, 0x65, 0x1E, 0x00, 0x1D
};
// -------------------------------------------------------

/* Адреса чипов
// ----------------------------------------
// 0x59 - LC
// 0x5A - LM
// 0x5C - B
// 0x5D - C
// 0x5E - M
// 0x5F - Y
// --------------------------------------
*/
// Начальный и конечный адрес сканирования.
// Для определения цвета подключенного чипа 
uint8_t addressStart = 0x59; 
uint8_t addressEnd = 0x5F;
uint8_t addressCurent;
uint8_t addressCurentRead;
uint8_t addressCurentWrite;


// Конфигурируем сторожевого таймера
void disable_wdt()
{
  WDT->WDT_MR = WDT_MR_WDDIS; // disable WDT
}

// Инициализация i2c шины
void i2c_init(void)
{
  // Enable IO
  PIOB->PIO_PER = (sda | scl); 

  // Set to output
  PIOB->PIO_OER = (sda | scl);
  
  
  // Disable pull-up
  PIOB->PIO_PUDR = (sda | scl);
  

  // Enable Multi Drive Control (Open Drain)
  PIOB->PIO_MDER = (sda | scl);

  setsda;
  setscl;

  pmc_enable_periph_clk(12);
}

// Формирование сигнала начала передачи
void i2c_start(void)
{
  clrsda;
  delayMicroseconds(i2c_time);
  clrscl;
  delayMicroseconds(i2c_time);
}

// Формирование сигнала окончание передачи
void i2c_stop(void)
{
  clrsda;
  delayMicroseconds(i2c_time);
  setscl;
  delayMicroseconds(i2c_time);
  setsda;
  delayMicroseconds(i2c_time);
}

// ------------ передача байта
uint8_t i2c_tx(uint8_t data)
{
  uint8_t x;
  uint8_t b = 1;

  // цикл на 8 передаваемых бит
  for(x = 8; x; x--)
  {
    clrscl;

    if((data & 0x80) == 0)
    {
      clrsda;
    } else {
      setsda;
    }

    delayMicroseconds(i2c_time);

    data <<= 1;

    setscl;
    delayMicroseconds(i2c_time);
  }

  clrscl;
  delayMicroseconds(i2c_time);

  setscl;
  setsda;
  delayMicroseconds(i2c_time);
  
  // считываем возможный ACK бит 
  if (!(PIOB->PIO_PDSR & sda))
  {
    b = 0;
  }

  clrscl;

  return b;
}

// ---------- Чтение байта данных -----------------
uint8_t i2c_rx(uint8_t ack)
{
  uint8_t x;                       // счетчик
  uint8_t data = 0;                // принимаемый байт
  uint8_t data_bit = 7;

  rxsda;
  
  delayMicroseconds(i2c_time);
  setsda;

  for(x=0; x < 8 ; x++)
  {
    setscl;
      
    if (REG_PIOB_PDSR & sda)
    {
      data |= _BV(data_bit);
    } 
    delayMicroseconds(i2c_time);  
    
    clrscl;
    data_bit--;
    delayMicroseconds(i2c_time);
  }

  txsda;
  
  if(ack == 1)
  {
    clrsda;
  }
  else
  {
    setsda;
  }

  setscl;
  delayMicroseconds(i2c_time);
  
  clrscl;
  delayMicroseconds(i2c_time);
  
  return data;
}

void l26500_print_help()
{
  Serial.println(F("\nCommand: "));
  Serial.println(F("Scaner:"));
  Serial.println(F("\n\t\'s\' = scan color chip"));
  Serial.println(F("\n\t\'i\' = init color chip"));
  Serial.println(F("\n\t\'p\' = print all data from chip."));
  Serial.println(F("Write:"));
  Serial.println(F("\n\t\'w\' = write data to chip."));
  Serial.println(F("Other:"));
  Serial.println(F("\n\t\'e\' = erase chip"));
  Serial.println(F("\n\t\'t\' = test"));
  Serial.println(F("Help:"));
  Serial.println(F("\n\t\'h\' = help - this page"));
  Serial.println();
}

// Чтение команды через UART
char getCommand()
{
  char c = '\0';
  if (Serial.available())
  {
    c = Serial.read();
  }
  return c;
}

// ----- Сканирование адреса чипа ----------
// Предпологается 1 чип на шины i2c
void l26500_scan()
{
  uint8_t addressTemp;
  uint8_t addressTempRead;
  uint8_t addressSelected = 0;

  for(addressTemp = addressStart; addressTemp <= addressEnd; addressTemp++)
  {
    addressTempRead = (addressTemp << 1) | 1;

    i2c_start();
    
    if(!i2c_tx(addressTempRead))
    {
      addressSelected = addressTemp;
    }

    i2c_stop();

    delay(1);
  }

  addressCurent = addressSelected;
  addressCurentRead = (addressSelected << 1) | 1;
  addressCurentWrite = (addressSelected << 1) | 0;
}

// Адреса чипов
// ----------------------------------------
// 0x59 - LC
// 0x5A - LM
// 0x5C - B
// 0x5D - C
// 0x5E - M
// 0x5F - Y
// --------------------------------------
// ----- Определение цвета чипа ----------
void l26500_color()
{
  switch (addressCurent)
  {
    case 0x59:
      Serial.println(F("Ligt Cyan."));
      break;

    case 0x5A:
      Serial.println(F("Ligt Magenta."));
      break;

    case 0x5C:
      Serial.println(F("Black."));
      break;
      
    case 0x5D:
      Serial.println(F("Cyan."));
      break;

    case 0x5E:
      Serial.println(F("Magenta."));
      break;

    case 0x5F:
      Serial.println(F("Yellow."));
      break;
    
    default:
      break;
  }
}

// --------- Чтение одного байта из чипа --------------
// принимает  - адресс читаемого байта
// возвращает - считанный байт
// ----------------------------------------------------
byte l26500_read_byte(byte address)
{
  byte data; 
  
  i2c_start();
  i2c_tx(addressCurentRead);
  i2c_tx(address);
  data = i2c_rx(0);
  i2c_stop();

  return data;
}

// ------- Чтение 4-х байт --------------
void l26500_read_row(byte address, byte pointer)
{
    i2c_start();
    i2c_tx(addressCurentRead);
    i2c_tx(address);
    eeprom[(pointer)] = i2c_rx(1);
    eeprom[(pointer + 1)] = i2c_rx(1);
    eeprom[(pointer + 2)] = i2c_rx(1);
    eeprom[(pointer + 3)] = i2c_rx(0);
    i2c_stop();
}

// --------- Инициализация чипа ------------
void l26500_init()
{
  byte color;
  byte protect;
  byte counter = 0;

  color = l26500_read_byte(0);
  delayMicroseconds(800);

  l26500_read_row(0, 0);
  delayMicroseconds(450);

  protect = l26500_read_byte(0x7B);
  delayMicroseconds(400);

  while(counter < 0x78)
    {
      l26500_read_row(counter, counter);
      counter = counter + 4;
      delayMicroseconds(300);  
    }
    
  counter = 0;
  
  eeprom[0x78] = l26500_read_byte(0x78);
  eeprom[0x79] = l26500_read_byte(0x79);
  delayMicroseconds(300);

  eeprom[0x7A] = l26500_read_byte(0x7A);
  delayMicroseconds(850);

  while(counter < 0x18)
  {
    l26500_write_row(counter);
    delay(11);
    l26500_read_row(counter, counter);
    counter = counter + 4;
    delayMicroseconds(300); 
  }
  
  l26500_write_byte(0x18, 0xA5);
  l26500_write_byte(0x18, 0xA5);
  l26500_write_byte(0x18, 0xA5);
  l26500_write_byte(0x18, 0xA5);

}

// ----- Чтение всего чипа -----------
void l26500_read_all()
{
  byte i = 0;
  byte point = 0;
  
    while(i < 0x80)
    {
      l26500_read_row(i, point);
      i = i + 4;
      point = point + 4;
      delayMicroseconds(300);  
    }
    
    delay(30);
}

//-----------------------------------------------------------
// Вывод содержимого EEPROM через UART
//-----------------------------------------------------------
void l26500_print_all()
{
  for(byte l = 0; l < 0x80; l++)
  {
    Serial.write(eeprom[l]);
  }
}

// ----- Записать байт по определенному адресу -------------
void l26500_write_byte(uint8_t address, uint8_t data)
{
  i2c_start();
  i2c_tx(addressCurentWrite);
  i2c_tx(address);
  i2c_tx(data);
  i2c_stop();
  //delay(11);
}

// ---- Запись 4-х байт по определенному адресу ------------
void l26500_write_row(byte address)
{
  i2c_start();
  i2c_tx(addressCurentWrite);
  i2c_tx(address);
  i2c_tx(data_write_row[0]);
  i2c_tx(data_write_row[1]);
  i2c_tx(data_write_row[2]);
  i2c_tx(data_write_row[3]);
  i2c_stop();
}

void inputByte()
{
  char count = 0;
  
  while(Serial.available() > 0)
  {
    if(count < 2)
    {
      convertByte[count] = Serial.read();  
    }
    count++;
    delay(5);
  }
}

// Очистка UART буфера
void serialFlash()
{
  while(Serial.available() > 0)
  {
    Serial.read();
  }
}

// -----  Ввод адреса и байта с последующей его записью ----
void l26500_write_inputByte(void)
{
  uint8_t addr;
  uint8_t data;
  
  Serial.print("Enter address: ");
  // Ожидание ввода
  serialFlash();
  while(Serial.available() == 0) { }
  inputByte();
  addr = d2h();
  Serial.println();
  
  Serial.print("Enter data: ");
  // Ожидание ввода
  serialFlash();
  while(Serial.available() == 0) { }
  inputByte();
  data = d2h();
  Serial.println();

  Serial.print("Address - ");
  Serial.println(addr, HEX);

  Serial.print("Data - ");
  Serial.println(data, HEX);
}

// -----  Прием адреса и байта с последующей его записью ----
void l26500_reciveByte_writeByte()
{
  uint8_t addr;
  uint8_t data;
  
  Serial.println("Write data to EEPROM");
  
  if(Serial.available() == 2) { 
    addr = Serial.read();
    delay(10);
    data = Serial.read();
    delay(10);
  }
  
  Serial.write(0xCC);
  delay(12);

  Serial.write(0xDD);
  delay(12);
  
}

//---------------------------------------------------------------
// Конвертация ASCII в HEX
byte getVal(char c)
{
  if(c >= '0' && c <= '9')
    return (byte)(c - '0');
  else if ( c >= 'A' && c <= 'F')
    return (byte)(c - 'A' + 10);
}

byte d2h()
{
  byte value;
  
  value = getVal(convertByte[1]) + (getVal(convertByte[0]) << 4);
  return value;
}
//---------------------------------------------------------------
void l26500_erase_all(void)
{
  for(char i = 0x0; i <= 0x80; i++)
  {
    l26500_write_byte(i, 0xFF);
    delay(20);
  }
}

//---------------------------------------------------------------

void l26500_test()
{
  /*
  for(byte l = 0; l < 0x80; l++)
  {
    Serial.write(eeprom_test[l]);
  }
  */
  //l26500_write_byte(0xD, 0xFF);

  byte c;
  c = l26500_read_byte(0);
  Serial.println(c, HEX);
}

void setup() {
  
  disable_wdt();
  i2c_init();
  
  Serial.begin(9600);
  Serial.println("Start EEPROM reader...");
  
  l26500_print_help();
}

void loop() {

    char command = getCommand();

    switch(command)
    {
      case 's':
        l26500_scan();
        
        if (addressCurent != 0)
        {
          Serial.print("Chip found at 0x");
          Serial.print(addressCurent, HEX);
          Serial.println(" h");
          l26500_color();
        }
        else
        {
          Serial.println("Chip not found!");  
        }
        break;
        
      case 'i':
        l26500_init();
        Serial.println("Init chip completed!"); 
        break;
        
      case 'p':
      
        // Чтение данных из EEPROM
        l26500_read_all();
        // Вывод данных из EEPROM
        l26500_print_all();
        break;

      case 't':
        l26500_test();
        //Serial.println("Test completed!");
        break;
      
      case 'w':
        l26500_reciveByte_writeByte();
        break; 

      case 'e':
        l26500_erase_all();
        l26500_print_help();
        break;
      
      case 'h':
        l26500_print_help();
        break;
        
      default:
        break;
    }
    
    /*
    
    byte addr = 0x0;
    uint8_t data;
    
    i2c_start();
    i2c_tx(CHIP_EEPROM_READ);
    i2c_tx(addr);
    data = i2c_rx(0);
    i2c_stop();
    */
    
    // Serial.write(data);
    
    // Отправка значение data в HEX формате
    // sprintf(data_hex, "%02X", data);
    // Serial.print(data_hex);
    //delay(1000);
}

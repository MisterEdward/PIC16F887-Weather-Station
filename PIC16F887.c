#include <xc.h>

// Configurare biti
#pragma config FOSC = INTRC_NOCLKOUT
#pragma config WDTE = OFF
#pragma config PWRTE = ON
#pragma config MCLRE = ON
#pragma config CP = OFF
#pragma config CPD = OFF
#pragma config BOREN = ON
#pragma config IESO = OFF
#pragma config FCMEN = OFF
#pragma config LVP = OFF

#define _XTAL_FREQ 4000000
#define LM35_PIN     RA0        // LM35 pe RA0/AN0
#define HIH_PIN      RA1        // HIH-5030 pe RA1/AN1
#define LDR_PIN      RA2        // LDR pe RA2/AN2
#define LM35_CHANNEL 0          // Canal 0 pentru AN0
#define HIH_CHANNEL  1          // Canal 1 pentru AN1
#define LDR_CHANNEL  2          // Canal 2 pentru AN2

// Pinii pentru buzzer si butoane
#define BUZZER_PIN  RA3         // Buzzer pe RA3
#define BUTTON_PIN  RA4         // Buton alarma pe RA4
#define ANALOG_BUTTON_PIN RB2   // Buton senzori analogici pe RB2
#define DIGITAL_BUTTON_PIN RB0  // Buton senzor digital pe RB0
#define LDR_BUTTON_PIN RB3      // Buton senzor lumina pe RB3
#define TIME_BUTTON_PIN RB1     // Buton timp pe RB1

// Moduri de afisare
#define DISP_WELCOME  0
#define DISP_LM35     1
#define DISP_SHT21    2
#define DISP_LDR      3
#define DISP_TIME     4

// Pinii pentru SHT21 (I2C)
#define SHT_SDA_PIN      PORTCbits.RC4   // SDA pe RC4
#define SHT_SCL_PIN      PORTCbits.RC3   // SCL pe RC3
#define SHT_SDA_DIR      TRISCbits.TRISC4
#define SHT_SCL_DIR      TRISCbits.TRISC3

// Adresa I2C pentru SHT21
#define SHT21_ADDRESS          0x40 // adresa pe 7 biti
#define SHT21_ADDRESS_WRITE    (SHT21_ADDRESS << 1)
#define SHT21_ADDRESS_READ     ((SHT21_ADDRESS << 1) | 0x01)

// Comenzi pentru SHT21
#define SHT21_CMD_MEASURE_TEMP_HOLD       0xE3
#define SHT21_CMD_MEASURE_HUMID_HOLD      0xE5
#define SHT21_CMD_MEASURE_TEMP_NO_HOLD    0xF3
#define SHT21_CMD_MEASURE_HUMID_NO_HOLD   0xF5
#define SHT21_CMD_WRITE_USER_REG          0xE6
#define SHT21_CMD_READ_USER_REG           0xE7
#define SHT21_CMD_SOFT_RESET              0xFE

// Pinii pentru LCD
#define RS RC0    // Register Select pe RC0
#define EN RC1    // Enable pe RC1
// D4-D7 sunt conectati la RD4-RD7

// Pinii pentru UART
#define UART_TX_PIN  RC6       // UART TX pe RC6
#define UART_RX_PIN  RC7       // UART RX pe RC7

// Declararea functiilor
void setupPins(void), LCD_Command(unsigned char cmd), LCD_Init(void);
void LCD_Char(unsigned char data), LCD_String(const char *str);
unsigned int readADC(unsigned char channel);
void setupADC(void), LCD_WriteTemp(float temp), LCD_WriteInt(int value);
void I2C_Init(void), I2C_Start(void), I2C_Stop(void);
unsigned char I2C_Write(unsigned char data), I2C_Read(unsigned char send_ack);
void SHT21_Init(void);
unsigned char SHT21_Measure(unsigned char cmd, unsigned int *value);
float SHT21_CalcTemperature(unsigned int rawValue), SHT21_CalcHumidity(unsigned int rawValue);
float getLM35Temperature(void), getHIH5030Humidity(void), getLDRValue(void);
unsigned char isButtonPressed(unsigned char pin);
void soundBuzzer(unsigned int duration_ms), displayAlarmCountdown(unsigned int seconds);
void displayLoadingBar(unsigned int duration_ms), setupUART(void);
void UART_SendByte(unsigned char data), UART_SendString(const char *str);
void UART_SendFloat(float value, unsigned char precision);
void setupTimer1(void), incrementTime(void), processUARTData(void);
void __interrupt() timer_isr(void);
unsigned char my_strlen(const char* str);
char* my_strstr(const char* haystack, const char* needle);

// Stringuri pentru afisare
const char welcome1[] = "Apasa un buton";
const char welcome2[] = "pentru meniu";
const char alarm_txt[] = "Alarma: ";
const char alarm_add[] = "+ Apasa pt 15s";

// Variabile globale
char time_str[9] = "00:00:00", date_str[11] = "01/01/2024";
unsigned char time_valid = 0, uart_index = 0, uart_data_ready = 0;
char uart_buffer[64] = "";
volatile unsigned char timer1_count = 0;
// Valori pentru Timer1 - intrerupere la ~0.1 secunde
#define TMR1_PRELOAD_H 0xCF
#define TMR1_PRELOAD_L 0x2C

void setupPins() {
    TRISC0 = 0;    // RS ca output
    TRISC1 = 0;    // EN ca output
    TRISC2 = 1;    // nefolosit
    TRISC3 = 0;    // SHT21 SCL ca output
    TRISC4 = 1;    // SHT21 SDA ca input initial
    TRISD = 0x0F;  // nibble-ul superior ca output
    
    TRISA3 = 0;    // Buzzer ca output
    TRISA4 = 1;    // Buton alarma ca input
    TRISB0 = 1;    // Buton senzori digitali
    TRISB1 = 1;    // Buton timp 
    TRISB2 = 1;    // Buton senzori analogici
    TRISB3 = 1;    // Buton LDR
    
    PORTA &= ~(1 << 3);  // Buzzer oprit initial
    
    PORTC = 0;     // Sterge portul C
    PORTD = 0;     // Sterge portul D
}

void LCD_Command(unsigned char cmd) {
    RS = 0;                    // Mod comanda
    PORTD &= 0x0F;            // Sterge nibble-ul superior
    PORTD |= (cmd & 0xF0);    // Trimite nibble-ul superior
    
    EN = 1;                   // Impuls Enable
    __delay_us(5);
    EN = 0;
    __delay_us(5);
    
    PORTD &= 0x0F;            // Sterge nibble-ul superior
    PORTD |= ((cmd << 4) & 0xF0); // Trimite nibble-ul inferior
    
    EN = 1;                   // Impuls Enable
    __delay_us(5);
    EN = 0;
    __delay_us(5);
    
    __delay_ms(2);            // Asteapta executia comenzii
}

void LCD_Init() {
    __delay_ms(20);           // Asteapta pornirea
    
    RS = 0;
    PORTD &= 0x0F;
    PORTD |= 0x30;           // Initializare
    EN = 1;
    __delay_us(5);
    EN = 0;
    __delay_ms(5);
    
    PORTD &= 0x0F;
    PORTD |= 0x30;           // Initializare
    EN = 1;
    __delay_us(5);
    EN = 0;
    __delay_ms(1);
    
    PORTD &= 0x0F;
    PORTD |= 0x30;           // Initializare
    EN = 1;
    __delay_us(5);
    EN = 0;
    __delay_ms(1);
    
    PORTD &= 0x0F;
    PORTD |= 0x20;           // Mod 4-bit
    EN = 1;
    __delay_us(5);
    EN = 0;
    __delay_ms(1);
    
    LCD_Command(0x28);        // 2 linii, matrice 5x7
    LCD_Command(0x0C);        // Display ON, cursor OFF
    LCD_Command(0x01);        // Sterge display-ul
    LCD_Command(0x06);        // Incrementeaza cursorul
}

void LCD_Char(unsigned char data) {
    RS = 1;                    // Mod date
    PORTD &= 0x0F;            // Sterge nibble-ul superior
    PORTD |= (data & 0xF0);   // Trimite nibble-ul superior
    
    EN = 1;
    __delay_us(5);
    EN = 0;
    __delay_us(5);
    
    PORTD &= 0x0F;
    PORTD |= ((data << 4) & 0xF0);
    
    EN = 1;
    __delay_us(5);
    EN = 0;
    __delay_us(5);
    
    __delay_ms(2);
}

void LCD_String(const char *str) {
    while(*str) LCD_Char(*str++);
}

unsigned int readADC(unsigned char channel) {
    ADCON0 &= 0b11000011;       // Sterge bitii de selectie canal
    ADCON0 |= (channel << 2);   // Selecteaza canalul dorit
    __delay_us(20);             // Timp de achizitie
    
    ADCON0bits.GO = 1;          // Porneste conversia
    while(ADCON0bits.GO);       // Asteapta conversia
    return (unsigned int)((ADRESH << 8) + ADRESL);   // Returneaza rezultatul
}

void setupADC() {
    TRISA |= 0x07;              // RA0, RA1, RA2 ca intrari
    ANSEL |= 0x07;              // AN0, AN1, AN2 ca analogice
    ANSELH = 0x00;              // Fara pini analogici suplimentari
    
    ADCON1 = 0x80;              // Aliniere dreapta, Vref este VDD
    ADCON0 = 0x01;              // Porneste ADC, selecteaza canalul 0
    __delay_us(100);            // Asteapta stabilizarea ADC
}

void LCD_WriteInt(int value) {
    unsigned int uval = (unsigned int)value;
    if (uval >= 100U) {
        LCD_Char((unsigned char)('0' + (uval / 100U)));
        uval %= 100U;
    }
    if (uval >= 10U) {
        LCD_Char((unsigned char)('0' + (uval / 10U)));
        uval %= 10U;
    }
    LCD_Char((unsigned char)('0' + uval));
}

void LCD_WriteTemp(float temp) {
    unsigned int integer = (unsigned int)temp;
    unsigned int decimal = (unsigned int)((temp - (float)integer) * 10.0f);
    
    LCD_WriteInt(integer);
    LCD_Char('.');
    LCD_Char((unsigned char)('0' + decimal));
    LCD_Char(' ');
    LCD_Char('C');
}

// Functii I2C simple
#define I2C_DELAY() __delay_us(5) // Delay pentru timing I2C

void I2C_Init(void) {
    SHT_SCL_DIR = 0; // SCL ca output
    SHT_SDA_DIR = 0; // SDA ca output initial
    SHT_SCL_PIN = 1; // SCL high
    SHT_SDA_PIN = 1; // SDA high
    __delay_ms(1);   // Lasa liniile sa se stabilizeze
}

void I2C_Start(void) {
    SHT_SDA_DIR = 0; // SDA ca output
    SHT_SDA_PIN = 1; // SDA high
    I2C_DELAY();
    SHT_SCL_PIN = 1; // SCL high
    I2C_DELAY();
    SHT_SDA_PIN = 0; // SDA low in timp ce SCL e high
    I2C_DELAY();
    SHT_SCL_PIN = 0; // SCL low
    I2C_DELAY();
}

void I2C_Stop(void) {
    SHT_SDA_DIR = 0; // SDA ca output
    SHT_SCL_PIN = 0; // SCL low
    I2C_DELAY();
    SHT_SDA_PIN = 0; // SDA low
    I2C_DELAY();
    SHT_SCL_PIN = 1; // SCL high
    I2C_DELAY();
    SHT_SDA_PIN = 1; // SDA high in timp ce SCL e high
    I2C_DELAY();
    SHT_SDA_DIR = 1; // Elibereaza SDA
}

unsigned char I2C_Write(unsigned char data) {
    unsigned char i;
    unsigned char ack;

    SHT_SDA_DIR = 0; // SDA ca output
    SHT_SCL_PIN = 0; // SCL low

    for (i = 0; i < 8; i++) {
        if (data & 0x80) {
            SHT_SDA_PIN = 1;
        } else {
            SHT_SDA_PIN = 0;
        }
        data <<= 1;
        I2C_DELAY();
        SHT_SCL_PIN = 1; // Clock high
        I2C_DELAY();
        SHT_SCL_PIN = 0; // Clock low
        I2C_DELAY();
    }

    // Citeste bitul ACK
    SHT_SDA_DIR = 1; // SDA ca input pentru ACK
    I2C_DELAY();
    SHT_SCL_PIN = 1; // Clock high pentru ACK
    I2C_DELAY();
    ack = SHT_SDA_PIN; // Citeste ACK/NACK
    SHT_SCL_PIN = 0; // Clock low
    I2C_DELAY();
    
    return (ack == 0); // Returneaza 1 daca ACK primit
}

unsigned char I2C_Read(unsigned char send_ack) {
    unsigned char i;
    unsigned char data = 0;

    SHT_SDA_DIR = 1; // SDA ca input

    for (i = 0; i < 8; i++) {
        data <<= 1;
        SHT_SCL_PIN = 1; // Clock high
        I2C_DELAY();
        if (SHT_SDA_PIN) {
            data |= 0x01;
        }
        SHT_SCL_PIN = 0; // Clock low
        I2C_DELAY();
    }

    // Trimite ACK/NACK
    SHT_SDA_DIR = 0; // SDA ca output pentru ACK/NACK
    if (send_ack) {
        SHT_SDA_PIN = 0; // Trimite ACK
    } else {
        SHT_SDA_PIN = 1; // Trimite NACK
    }
    I2C_DELAY();
    SHT_SCL_PIN = 1; // Clock high pentru ACK/NACK
    I2C_DELAY();
    SHT_SCL_PIN = 0; // Clock low
    I2C_DELAY();
    SHT_SDA_DIR = 1; // Elibereaza SDA
    
    return data;
}

// Functii pentru SHT21
void SHT21_Init(void) {
    I2C_Init(); // Initializeaza pinii I2C
    
    // Reset software
    I2C_Start();
    if (I2C_Write(SHT21_ADDRESS_WRITE)) {
        I2C_Write(SHT21_CMD_SOFT_RESET);
    }
    I2C_Stop();
    __delay_ms(15); // Asteapta reset-ul
}

unsigned char SHT21_Measure(unsigned char sht_measure_cmd, unsigned int *value) {
    unsigned char msb, lsb, crc_byte;
    unsigned char error = 0;
    unsigned char retry_count = 0;

    // Trimite comanda de masurare
    I2C_Start();
    if (!I2C_Write(SHT21_ADDRESS_WRITE)) {
        I2C_Stop();
        return 1; // Nu s-a primit ACK de la SHT21
    }
    
    if (!I2C_Write(sht_measure_cmd)) {
        I2C_Stop();
        return 2; // Nu s-a primit ACK pentru comanda
    }
    I2C_Stop();

    // Asteapta finalizarea masuratorii
    if (sht_measure_cmd == SHT21_CMD_MEASURE_TEMP_NO_HOLD) {
        __delay_ms(100);
    } else if (sht_measure_cmd == SHT21_CMD_MEASURE_HUMID_NO_HOLD) {
        __delay_ms(50);
    } else {
        __delay_ms(75);
    }

    // Incearca sa citeasca datele cu reincercari
    for (retry_count = 0; retry_count < 3; retry_count++) {
        I2C_Start();
        if (I2C_Write(SHT21_ADDRESS_READ)) {
            msb = I2C_Read(1);      // Citeste MSB, trimite ACK
            lsb = I2C_Read(1);      // Citeste LSB, trimite ACK  
            crc_byte = I2C_Read(0); // Citeste CRC, trimite NACK
            
            *value = (unsigned int)(msb << 8) | lsb;
            *value &= ~0x0003U; // Masca bitii de status
            
            I2C_Stop();
            return 0; // Success
        }
        I2C_Stop();
        __delay_ms(10); // Asteapta inainte de reincercare
    }

    return 3; // Esuat dupa reincercari
}

float SHT21_CalcTemperature(unsigned int rawValue) {
    // Formula: T_C = -46.85 + 175.72 * (S_T / 2^16)
    return -46.85f + 175.72f * (float)rawValue / 65536.0f;
}

float SHT21_CalcHumidity(unsigned int rawValue) {
    // Formula: RH_true = -6.0 + 125.0 * (S_RH / 2^16)
    float rh = -6.0f + 125.0f * (float)rawValue / 65536.0f;
    
    if(rh > 100.0f) rh = 100.0f;
    if(rh < 0.0f) rh = 0.0f;
    
    return rh;
}

float getLM35Temperature(void) {
    unsigned int adc_value = readADC(LM35_CHANNEL);
    float voltage = (adc_value * 5.0) / 1024.0;
    return voltage * 100.0;
}

float getHIH5030Humidity(void) {
    unsigned int adc_value = readADC(HIH_CHANNEL);
    float voltage = (adc_value * 5.0) / 1024.0;
    float humid = (voltage / 5.0 - 0.1515) / 0.00636;
    
    if(humid > 100.0f) humid = 100.0f;
    if(humid < 0.0f) humid = 0.0f;
    
    return humid;
}

float getLDRValue(void) {
    unsigned int adc_value = readADC(LDR_CHANNEL);
    
    // Previne impartirea la zero
    if (adc_value == 0) adc_value = 1;
    if (adc_value > 1023) adc_value = 1023;
    
    // Converteste valoarea ADC in procente
    // LDR: valoare ADC mica = mai multa lumina
    float light_percent = 100.0f - ((float)adc_value / 1023.0f) * 100.0f;
    
    return light_percent;
}

// Functie pentru verificare butoane
unsigned char isButtonPressed(unsigned char pin) {
    static unsigned char prev[5] = {1,1,1,1,1}; // RA4, RB0, RB1, RB2, RB3
    unsigned char current, result = 0;
    unsigned char idx;
    
    switch(pin) {
        case 4: current = BUTTON_PIN; idx = 0; break;          // RA4
        case 0: current = PORTBbits.RB0; idx = 1; break;       // RB0
        case 1: current = PORTBbits.RB1; idx = 2; break;       // RB1
        case 2: current = PORTBbits.RB2; idx = 3; break;       // RB2
        case 3: current = PORTBbits.RB3; idx = 4; break;       // RB3
        default: return 0;
    }
    
    if (prev[idx] == 1 && current == 0) {
        result = 1;
        __delay_ms(20);
    }
    prev[idx] = current;
    return result;
}

unsigned char isAlarmButtonPressed(void) {
    static unsigned char previous = 1;
    unsigned char current = BUTTON_PIN;
    unsigned char result = 0;
    
    if (previous == 1 && current == 0) {
        result = 1;
        __delay_ms(20);  // debounce
    }
    
    previous = current;
    return result;
}

unsigned char isAnalogButtonPressed(void) {
    static unsigned char previous = 1;
    unsigned char current = PORTBbits.RB2;
    unsigned char result = 0;
    
    if (previous == 1 && current == 0) {
        result = 1;
        __delay_ms(20);  // debounce
    }
    
    previous = current;
    return result;
}

unsigned char isDigitalButtonPressed(void) {
    static unsigned char previous = 1;
    unsigned char current = PORTBbits.RB0;
    unsigned char result = 0;
    
    if (previous == 1 && current == 0) {
        result = 1;
        __delay_ms(20);  // debounce
    }
    
    previous = current;
    return result;
}

unsigned char isLDRButtonPressed(void) {
    static unsigned char previous = 1;
    unsigned char current = PORTBbits.RB3;
    unsigned char result = 0;
    
    if (previous == 1 && current == 0) {
        result = 1;
        __delay_ms(20);  // debounce
    }
    
    previous = current;
    return result;
}

unsigned char isTimeButtonPressed(void) {
    static unsigned char previous = 1;
    unsigned char current = PORTBbits.RB1;
    unsigned char result = 0;
    
    if (previous == 1 && current == 0) {
        result = 1;
        __delay_ms(20);  // debounce
    }
    
    previous = current;
    return result;
}

void soundBuzzer(unsigned int duration_ms) {
    BUZZER_PIN = 1;
    
    for (unsigned int i = 0; i < duration_ms; i++) {
        __delay_ms(1);
    }
    
    BUZZER_PIN = 0;
}

void displayAlarmCountdown(unsigned int seconds) {
    LCD_Command(0x01);
    LCD_Command(0x80);
    LCD_String(alarm_txt);
    
    unsigned int mins = seconds / 60U, secs = seconds % 60U;
    LCD_Char((unsigned char)('0' + (mins / 10U)));
    LCD_Char((unsigned char)('0' + (mins % 10U)));
    LCD_Char(':');
    LCD_Char((unsigned char)('0' + (secs / 10U)));
    LCD_Char((unsigned char)('0' + (secs % 10U)));
    
    LCD_Command(0xC0);
    LCD_String(alarm_add);
}

// Bara de incarcare simplificata
void displayLoadingBar(unsigned int duration_ms) {
    unsigned char progress = 0;
    LCD_Command(0x01);
    LCD_Command(0x80);
    LCD_String("Pornire...");
    LCD_Command(0xC0);
    
    while (progress < 16U) {
        LCD_Char(0xFF);
        progress++;
        __delay_ms(150);
    }
    __delay_ms(500);
}

void setupUART(void) {
    TRISC6 = 0;  // TX ca output
    TRISC7 = 1;  // RX ca input
    
    SPBRG = 25;  // 9600 baud la 4MHz
    SPBRGH = 0;
    
    TXSTA = 0x24; // Porneste transmiterul
    RCSTA = 0x90; // Porneste receptorul si portul serial
    BAUDCTL = 0x00;
    
    // Porneste intreruperea pentru receptie UART
    PIE1bits.RCIE = 1;
    INTCONbits.PEIE = 1;
    
    __delay_ms(100);
}

void UART_SendByte(unsigned char data) {
    while (!TXIF);
    TXREG = data;
}

void UART_SendString(const char *str) {
    while(*str) {
        UART_SendByte(*str++);
    }
}

void UART_SendFloat(float value, unsigned char precision) {
    unsigned int int_part = (unsigned int)value;
    
    // Trimite partea intreaga
    if (int_part >= 100U) UART_SendByte((unsigned char)('0' + (int_part / 100U)));
    if (int_part >= 10U) UART_SendByte((unsigned char)('0' + ((int_part / 10U) % 10U)));
    UART_SendByte((unsigned char)('0' + (int_part % 10U)));
    
    if (precision > 0) {
        UART_SendByte('.');
        unsigned int dec_part = (unsigned int)((value - (float)int_part) * 10.0f);
        UART_SendByte((unsigned char)('0' + dec_part));
    }
}

void sendSensorDataToESP(float temp_lm35, float humid_hih, float light_ldr, 
                        float temp_sht, float humid_sht, unsigned char error_status) {
    UART_SendString("{");
    
    UART_SendString("\"lm35_temp\":");
    UART_SendFloat(temp_lm35, 1);
    UART_SendString(",");
    
    UART_SendString("\"hih_humid\":");
    UART_SendFloat(humid_hih, 0);
    UART_SendString(",");
    
    UART_SendString("\"light\":");
    UART_SendFloat(light_ldr, 0);
    UART_SendString(",");
    
    UART_SendString("\"sht_temp\":");
    if (error_status & 0x01) {
        UART_SendString("\"error\"");
    } else {
        UART_SendFloat(temp_sht, 1);
    }
    UART_SendString(",");
    
    UART_SendString("\"sht_humid\":");
    if (error_status & 0x02) {
        UART_SendString("\"error\"");
    } else {
        UART_SendFloat(humid_sht, 0);
    }
    
    UART_SendString("}\r\n");
}

void incrementTime(void) {
    // Aceasta functie e doar backup cand ESP32 nu trimite timp
    if (time_valid) return; // Nu incrementa daca avem timp valid de la ESP32
    
    unsigned int hh, mm, ss;
    // Parseaza timpul curent din time_str
    hh = (unsigned int)((time_str[0] - '0') * 10 + (time_str[1] - '0'));
    mm = (unsigned int)((time_str[3] - '0') * 10 + (time_str[4] - '0'));
    ss = (unsigned int)((time_str[6] - '0') * 10 + (time_str[7] - '0'));

    ss++; // Incrementeaza secundele

    if (ss >= 60U) {
        ss = 0;
        mm++;
        if (mm >= 60U) {
            mm = 0;
            hh++;
            if (hh >= 24U) {
                hh = 0;
            }
        }
    }

    // Formeaza time_str manual
    time_str[0] = (char)('0' + (hh / 10U));
    time_str[1] = (char)('0' + (hh % 10U));
    time_str[3] = (char)('0' + (mm / 10U));
    time_str[4] = (char)('0' + (mm % 10U));
    time_str[6] = (char)('0' + (ss / 10U));
    time_str[7] = (char)('0' + (ss % 10U));
}

unsigned char my_strlen(const char* str) {
    unsigned char len = 0;
    while (str[len++]);
    return --len;
}

char* my_strstr(const char* haystack, const char* needle) {
    unsigned char nlen = my_strlen(needle), i, j;
    if (nlen == 0) return (char*)haystack;
    
    for (i = 0; haystack[i]; i++) {
        for (j = 0; j < nlen && haystack[i + j] == needle[j]; j++);
        if (j == nlen) return (char*)&haystack[i];
    }
    return 0;
}

// Parseaza timpul de la ESP32
void parseESPTimeData(char* data) {
    char* time_start = my_strstr(data, "TIME:");
    if (time_start) {
        time_start += 5;
        for (unsigned char i = 0; i < 8; i++) time_str[i] = time_start[i];
        time_str[8] = '\0';
        time_valid = 1;
    }
}

void processUARTData(void) {
    if (!uart_data_ready) return;
    uart_data_ready = 0;
    
    if (my_strstr(uart_buffer, "TIME:")) parseESPTimeData(uart_buffer);
    
    uart_index = 0;
    uart_buffer[0] = '\0';
}

void setupTimer1(void) {
    T1CON = 0x30; // Timer1 ON, prescaler 1:8
    TMR1H = TMR1_PRELOAD_H;
    TMR1L = TMR1_PRELOAD_L;
    PIE1bits.TMR1IE = 1; // Porneste intreruperea Timer1
    INTCONbits.PEIE = 1; // Porneste intreruperile periferice
}

void __interrupt() timer_isr(void) {
    // Intrerupere receptie UART
    if (PIR1bits.RCIF) {
        char received_char = RCREG;
        
        if (received_char == '\n' || received_char == '\r') {
            uart_buffer[uart_index] = '\0';
            uart_data_ready = 1;
        } else if (uart_index < 63) {
            uart_buffer[uart_index] = received_char;
            uart_index++;
        }
        
        PIR1bits.RCIF = 0; // Sterge flag-ul
    }
    
    // Intrerupere Timer1
    if (PIR1bits.TMR1IF) {
        PIR1bits.TMR1IF = 0;

        TMR1H = TMR1_PRELOAD_H;
        TMR1L = TMR1_PRELOAD_L;

        timer1_count++;

        if (timer1_count >= 10) {
            timer1_count = 0;
            incrementTime(); // Incrementeaza doar daca timpul ESP32 nu e valid
        }
    }
}

void main(void) {
    OSCCON = 0x60;
    while(!OSCCONbits.HTS);
    
    setupPins();
    LCD_Init();
    setupADC();
    SHT21_Init();
    setupUART();
    setupTimer1();

    displayLoadingBar(3000);
    
    LCD_Command(0x01);
    LCD_Command(0x80);
    LCD_String("Sistem Gata");
    __delay_ms(2000);
    
    unsigned char disp_mode = DISP_WELCOME, alarm_active = 0, data_cnt = 0;
    unsigned int alarm_sec = 0;
    
    UART_SendString("PIC16F887 Porneste\r\n");
    INTCONbits.GIE = 1;

    while(1) {
        processUARTData();
        
        // Verifica butoanele
        if(isButtonPressed(4)) { // RA4 - Alarma
            if(alarm_active) alarm_sec += 15;
            else { alarm_sec = 15; alarm_active = 1; }
        }
        
        if(!alarm_active) {
            if(isButtonPressed(2)) { disp_mode = DISP_LM35; LCD_Command(0x01); }      // RB2
            else if(isButtonPressed(0)) { disp_mode = DISP_SHT21; LCD_Command(0x01); } // RB0
            else if(isButtonPressed(3)) { disp_mode = DISP_LDR; LCD_Command(0x01); }   // RB3
            else if(isButtonPressed(1)) { disp_mode = DISP_TIME; LCD_Command(0x01); }  // RB1
        }
        
        if(alarm_active) {
            displayAlarmCountdown(alarm_sec);
            __delay_ms(1000);
            alarm_sec--;
            if(alarm_sec == 0) {
                soundBuzzer(1000);
                alarm_active = 0;
                UART_SendString("alarm_end\r\n");
            }
        } else {
            float temp1 = getLM35Temperature(), humid1 = getHIH5030Humidity(), light = getLDRValue();
            unsigned int raw_temp = 0, raw_humid = 0;
            unsigned char err_temp = 0, err_humid = 0;
            float temp2 = 0.0, humid2 = 0.0;
            
            err_temp = SHT21_Measure(SHT21_CMD_MEASURE_TEMP_NO_HOLD, &raw_temp);
            if(!err_temp) {
                temp2 = SHT21_CalcTemperature(raw_temp);
                __delay_ms(10); 
                err_humid = SHT21_Measure(SHT21_CMD_MEASURE_HUMID_NO_HOLD, &raw_humid);
                if(!err_humid) humid2 = SHT21_CalcHumidity(raw_humid);
            }
            
            if(++data_cnt >= 5) {
                UART_SendString("T1:");
                UART_SendFloat(temp1, 1);
                UART_SendString(",H1:");
                UART_SendFloat(humid1, 0);
                UART_SendString(",L:");
                UART_SendFloat(light, 0);
                UART_SendString(",T2:");
                if (!err_temp) UART_SendFloat(temp2, 1);
                else UART_SendString("ERR");
                UART_SendString(",H2:");
                if (!err_humid) UART_SendFloat(humid2, 0);
                else UART_SendString("ERR");
                UART_SendString("\r\n");
                data_cnt = 0;
            }
            
            LCD_Command(0x01);
            __delay_ms(5);
            LCD_Command(0x80);
            
            switch(disp_mode) {
                case DISP_WELCOME:
                    LCD_String(welcome1);
                    LCD_Command(0xC0);
                    LCD_String(welcome2);
                    __delay_ms(200);
                    break;
                
                case DISP_LM35:
                    LCD_String("LM35 T: ");
                    LCD_WriteTemp(temp1);
                    LCD_Command(0xC0);
                    LCD_String("HIH H: ");
                    LCD_WriteInt((int)(humid1 + 0.5f));
                    LCD_Char('%');
                    __delay_ms(1000);
                    break;
                
                case DISP_SHT21:
                    LCD_String("SHT21 T: ");
                    if(!err_temp) LCD_WriteTemp(temp2);
                    else LCD_String("Eroare");
                    LCD_Command(0xC0);
                    LCD_String("SHT21 H: ");
                    if(!err_humid) { LCD_WriteInt((int)(humid2 + 0.5f)); LCD_Char('%'); }
                    else LCD_String("Eroare");
                    __delay_ms(1000);
                    break;
                
                case DISP_LDR:
                    LCD_String("Nivel Lumina:");
                    LCD_Command(0xC0);
                    LCD_WriteInt((int)(light + 0.5f));
                    LCD_Char('%');
                    __delay_ms(1000);
                    break;
                
                case DISP_TIME:
                    LCD_String("Timpul Curent:");
                    LCD_Command(0xC0);
                    LCD_String(time_str);
                    __delay_ms(1000);
                    break;
                    
                default:
                    disp_mode = DISP_WELCOME;
                    break;
            }
        }
    }
}

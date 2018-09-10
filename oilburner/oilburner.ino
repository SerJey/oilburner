#include <EEPROM.h>

#include "U8glib.h"
#include "OneWire.h"
#include "Timer.h"
#include "rus6x10.h"
#include "ClickButton.h"
#include "ACS712.h"

// gsm library
#define TINY_GSM_MODEM_SIM800

#include "TinyGsmClient.h"

// relays
#define RELAY_OILPUMP   30
#define RELAY_INJECTOR  32
#define RELAY_HEATING   34
#define RELAY_FAN       36
#define RELAY_IGNITION  38
#define RELAY_VALVE     40
#define RELAY_VALVE_2   42

// temperature sensors
#define TEMP_OIL        24
#define TEMP_INJECTOR   26
#define TEMP_WATER      28

// float switches
#define FLOAT_LEVEL     5
#define FLOAT_OVERFLOW  6

// flame sensor
#define FLAME_SENSOR    7

// lcd
#define LCD_RS  31
#define LCD_RW  33
#define LCD_E   35

// encoder
#define ENCODER_CLK 2
#define ENCODER_DT  3
#define ENCODER_SW  4

#define SerialAT Serial1

#define CURRENT_SENSOR A1

volatile boolean TurnDetected;
volatile int encDif = 0;
volatile boolean isMainMenu;
volatile boolean isInfo;
volatile boolean isSetupMenu;
volatile boolean isSetup;
volatile boolean isTestMenu;
volatile boolean isNotificationMenu;
volatile boolean isPhoneSetup;
volatile boolean isFuncMenu;

volatile boolean isFirstPart;
volatile boolean isSecondPart;

volatile boolean ignitionIsOn;
volatile boolean heatingIsOn;
volatile boolean fanIsOn;
volatile boolean injectorIsOn;
volatile boolean valveIsOn;
volatile boolean valve2IsOn;
volatile boolean pompIsOn;

ACS712 curSensor(ACS712_05B, CURRENT_SENSOR);

U8GLIB_ST7920_128X64 u8g(LCD_E, LCD_RW, LCD_RS, U8G_PIN_NONE);

int selectedMenu = 0;
int selectedSubMenu = 1;
int selectedTestMenu = 0;
int selectedNotifMenu = 0;
int selectedFuncMenu = 0;
String menu[7] = {"Температура масла", "Температура форсунки", "Температура воды", "Режим тестирования", "Оповещения",
                  "Функции", "Выход"};
int tempVal;
int tempMin;
int tempMax;

String phoneNumberStr = "";
byte phoneNumber[11];
int numSelIndex = 0;
byte tempNum = 0;
boolean notificationIsOn;

boolean injectorFuncIsOn;
boolean waterFuncIsOn;

float tOil;
float tInjector;
float tWater;
float tOilCur;
float tInjectorCur;
float tWaterCur;

boolean ignitionAlreadySent;
boolean oilAlreadySent;
boolean waterAlreadySent;
boolean injectorAlreadySent;
boolean currentAlreadySent;
boolean currentAlreadyShown;
boolean currentFineAlreadySent;

String info[6] = {};
String errorInfo[6] = {};

String ignitionError[6] = {
        "",
        "   ОШИБКА ПОДЖИГА!   ",
        "    ПЕРЕЗАПУСТИТЕ    ",
        "     КОНТРОЛЛЕР!     ",
        "",
        ""
};

String oilSensorError[6] = {
        "",
        "       ОШИБКА!       ",
        "   ПРОВЕРЬТЕ ДАТЧИК  ",
        "     ТЕМПЕРАТУРЫ     ",
        "        МАСЛА        ",
        ""
};

String waterSensorError[6] = {
        "",
        "       ОШИБКА!       ",
        "   ПРОВЕРЬТЕ ДАТЧИК  ",
        "     ТЕМПЕРАТУРЫ     ",
        "        ВОДЫ         ",
        ""
};

String injectorSensorError[6] = {
        "",
        "       ОШИБКА!       ",
        "   ПРОВЕРЬТЕ ДАТЧИК  ",
        "     ТЕМПЕРАТУРЫ     ",
        "      ФОРСУНКИ       ",
        ""
};

String injectorOverheatSensorError[6] = {
        "",
        "       ОШИБКА!       ",
        "      ПЕРЕГРЕВ       ",
        "      ФОРСУНКИ       ",
        "",
        ""
};

String waterOverheatSensorError[6] = {
        "",
        "       ОШИБКА!       ",
        "      ПЕРЕГРЕВ       ",
        "        ВОДЫ         ",
        "",
        ""
};

String oilOverheatSensorError[6] = {
        "",
        "       ОШИБКА!       ",
        "      ПЕРЕГРЕВ       ",
        "        МАСЛА        ",
        "",
        ""
};

String oilOverflowSensorError[6] = {
        "",
        "       ОШИБКА!       ",
        "       ПЕРЕЛИВ       ",
        "        МАСЛА        ",
        "",
        ""
};

String currentWarning[6] = {
        "",
        "      ВНИМАНИЕ!      ",
        "    СБОЙ  СИСТЕМЫ    ",
        "       ПИТАНИЯ       ",
        "       ОТ СЕТИ       ",
        ""
};

int tempOilMin;
int tempOilMax;
int tempInjectorMin;
int tempInjectorMax;
int tempWaterMin;
int tempWaterMax;

Timer tempTimer;
Timer valveTimer;
Timer valve2Timer;
Timer purgeTimer;
Timer ignitionTimer;
Timer oilPompTimer;

Timer errorCheckTimer;

OneWire oilSensor(TEMP_OIL);
OneWire injectorSensor(TEMP_INJECTOR);
OneWire waterSensor(TEMP_WATER);

boolean ignition;
boolean start;
boolean needsRestart;
boolean firstOilHeating;
boolean firstStart;

byte oilCheck = 0;
byte waterCheck = 0;
byte injectorCheck = 0;
byte currentCheck = 0;
boolean isNeedCheck;
boolean isSensorError;
boolean isCurrentWarning;

int flame;
int attempt;

int testFrameA = 0;
int testFrameB = 3;
int testDif = 0;

int mainFrameA = 0;
int mainFrameB = 3;
int mainDif = 0;

ClickButton encoderBtn(ENCODER_SW, LOW, CLICKBTN_PULLUP);

TinyGsm modem(SerialAT);

void encoder() {
    boolean a = (boolean) digitalRead(ENCODER_DT);
    boolean b = (boolean) digitalRead(ENCODER_CLK);
    encDif = a ^ b ? 1 : -1;
    TurnDetected = true;
}

void setup() {
    //setup inputs
    pinMode(ENCODER_DT, INPUT);
    pinMode(ENCODER_CLK, INPUT);

    pinMode(FLAME_SENSOR, INPUT_PULLUP); //with pullup resistor

    pinMode(FLOAT_LEVEL, INPUT_PULLUP); //with pullup resistor
    pinMode(FLOAT_OVERFLOW, INPUT_PULLUP); //with pullup resistor

    pinMode(RELAY_OILPUMP, OUTPUT);
    pinMode(RELAY_INJECTOR, OUTPUT);
    pinMode(RELAY_HEATING, OUTPUT);
    pinMode(RELAY_FAN, OUTPUT);
    pinMode(RELAY_IGNITION, OUTPUT);
    pinMode(RELAY_VALVE, OUTPUT);
    pinMode(RELAY_VALVE_2, OUTPUT);

    digitalWrite(RELAY_OILPUMP, HIGH);
    digitalWrite(RELAY_INJECTOR, HIGH);
    digitalWrite(RELAY_HEATING, HIGH);
    digitalWrite(RELAY_FAN, HIGH);
    digitalWrite(RELAY_IGNITION, HIGH);
    digitalWrite(RELAY_VALVE, HIGH);
    digitalWrite(RELAY_VALVE_2, HIGH);

    attachInterrupt(0, encoder, CHANGE);
    isInfo = true;
    isMainMenu = false;
    isSetupMenu = false;
    isSetup = false;
    isTestMenu = false;
    isNotificationMenu = false;
    isPhoneSetup = false;

    isFirstPart = true;
    isSecondPart = false;

    ignition = false;
    needsRestart = false;
    firstOilHeating = true;
    attempt = 0;
    firstStart = true;

    isNeedCheck = true;
    isSensorError = false;
    isCurrentWarning = false;

    ignitionAlreadySent = false;
    oilAlreadySent = false;
    waterAlreadySent = false;
    injectorAlreadySent = false;
    currentAlreadySent = false;
    currentAlreadyShown = false;
    currentFineAlreadySent = false;

    //Read values from EEPROM
    byte high = EEPROM.read(0);
    byte low = EEPROM.read(1);
    tempOilMin = word(high, low);
    high = EEPROM.read(2);
    low = EEPROM.read(3);
    tempOilMax = word(high, low);
    high = EEPROM.read(4);
    low = EEPROM.read(5);
    tempInjectorMin = word(high, low);
    high = EEPROM.read(6);
    low = EEPROM.read(7);
    tempInjectorMax = word(high, low);
    high = EEPROM.read(8);
    low = EEPROM.read(9);
    tempWaterMin = word(high, low);
    high = EEPROM.read(10);
    low = EEPROM.read(11);
    tempWaterMax = word(high, low);

    encoderBtn.longClickTime = 2000;
    encoderBtn.multiclickTime = 0;
    encoderBtn.debounceTime = -20;

    delay(5000);
    SerialAT.begin(115200);

    //Read phone number
    for (int i = 0; i < 11; i++) {
        phoneNumber[i] = EEPROM.read(i + 12);
    }
    notificationIsOn = (boolean) EEPROM.read(23);
    injectorFuncIsOn = (boolean) EEPROM.read(24);
    waterFuncIsOn = (boolean) EEPROM.read(25);

//    curSensor.calibrate();
}

void getPhoneNumber() {
    phoneNumberStr = "";
    for (int i = 0; i < 11; i++) {
        phoneNumberStr += String(phoneNumber[i]);
    }
}

void saveNotificationSettings() {
    for (int i = 0; i < 11; i++) {
        EEPROM.write(i + 12, phoneNumber[i]);
    }
    EEPROM.write(23, (byte) notificationIsOn);
}

void saveFunctionSettings() {
    EEPROM.write(24, (byte) injectorFuncIsOn);
    EEPROM.write(25, (byte) waterFuncIsOn);
}

void sendSms(const void *text, size_t len) {
    if (notificationIsOn) {
        getPhoneNumber();
        if (modem.init()) {
            modem.sendSMS_UTF16(phoneNumberStr, text, len);
        }
    }
}

void drawPhoneMenu(int x, int y) {
    for (int i = 0; i < 11; i++) {
        u8g.setColorIndex(1);
        int xx = (i * 6) + x;
        if (i == numSelIndex) {
            u8g.drawBox(xx - 1, (y - 10), 7, 13);
            u8g.setColorIndex(0);
        }
        u8g.setPrintPos(xx, y);
        u8g.setFont(rus6x10);
        u8g.print(phoneNumber[i]);
    }
}

void drawMenuItem(int x, int y, int draw, int selected, String text) {
    u8g.setColorIndex(1);
    if (draw == selected && !isSetup && !isPhoneSetup) {
        u8g.drawBox(2, (y - 10), 124, 13);
        u8g.setColorIndex(0);
    }
    u8g.setPrintPos(x, y);
    u8g.setFont(rus6x10);
    u8g.print(text);
    if (isSetup) {
        if (draw == selected) {
            u8g.setColorIndex(1);
            u8g.drawBox(82, (y - 10), 28, 13);
            u8g.setPrintPos(88, y);
            u8g.setColorIndex(0);
            u8g.print(String(tempVal));
        }
    }
    if (isPhoneSetup && draw == selected) {
        drawPhoneMenu(58, y);
    }
}

void displayLargeMenu(String menu[], int &selectedItemIndex, int &frameA, int &frameB, int &dif, int itemsSize) {
    u8g.firstPage();
    if (TurnDetected) {
        selectedItemIndex += encDif;
        TurnDetected = false;
    }

    int lastItemIndex = itemsSize - 1;
    if (selectedItemIndex > lastItemIndex) selectedItemIndex = 0;
    if (selectedItemIndex < 0) selectedItemIndex = lastItemIndex;

    if (selectedItemIndex > frameB) {
        dif = selectedItemIndex - 3;
        frameB = selectedItemIndex;
        frameA = frameB - 3;
    }
    if (selectedItemIndex < frameA) {
        frameA = selectedItemIndex;
        frameB = frameA + 3;
        dif = selectedItemIndex;
    }
    do {
        for (int i = frameA; i <= frameB; i++) {
            drawMenuItem(4, ((((i - dif) + 1) * 13) + 2), i, selectedItemIndex, menu[i]);
        }
    } while (u8g.nextPage());
}

void displaySimpleMenu(String menu[], int &selectedItemIndex, int itemsSize) {
    u8g.firstPage();
    if (TurnDetected) {
        selectedItemIndex += encDif;
        TurnDetected = false;
    }

    int lastItemIndex = itemsSize - 1;
    if (selectedItemIndex > lastItemIndex) selectedItemIndex = 0;
    if (selectedItemIndex < 0) selectedItemIndex = lastItemIndex;
    do {
        for (int i = 0; i < itemsSize; ++i) {
            drawMenuItem(4, (((i + 1) * 13) + 2), i, selectedItemIndex, menu[i]);
        }
    } while (u8g.nextPage());
}

void displaySetup(String menuItem, int min, int max) {
    String minimum = String("Минимальная   ") + String(min);
    String maximum = String("Максимальная  ") + String(max);
    String setupMenu[4] = {menuItem, minimum, maximum, "Сохранить"};
    u8g.firstPage();
    do {
        for (int i = 0; i < 4; ++i) {
            drawMenuItem(4, (((i + 1) * 13) + 2), i, selectedSubMenu, setupMenu[i]);
        }
    } while (u8g.nextPage());
}

void fillErrorInfo(String error[]) {
    errorInfo[0] = error[0];
    errorInfo[1] = error[1];
    errorInfo[2] = error[2];
    errorInfo[3] = error[3];
    errorInfo[4] = error[4];
    errorInfo[5] = error[5];
}

void displayInfo() { //Основной дисплей
    String oil = String(" Масло:     ") + String(tOil) + String("`C");
    String inj = String(" Форсунка:  ") + String(tInjector) + String("`C");
    String wat = String(" Вода:      ") + String(tWater) + String("`C");
    String status = "       CТАТУС";
    if (ignition && attempt > 0) {
        status = String(" Поджиг:    ") + String(attempt) + String(" п.");
    }
    String flm = " Огонь:     " + String(flame ? "нет" : "есть");
    String oilPmp = " Насос:     " + String(pompIsOn ? "вкл" : "выкл");
    info[0] = status;
    info[1] = oil;
    info[2] = inj;
    info[3] = wat;
    info[4] = flm;
    info[5] = oilPmp;
    if (needsRestart || isSensorError || (isCurrentWarning && !currentAlreadyShown)) {
        info[0] = errorInfo[0];
        info[1] = errorInfo[1];
        info[2] = errorInfo[2];
        info[3] = errorInfo[3];
        info[4] = errorInfo[4];
        info[5] = errorInfo[5];
    }
    u8g.firstPage();
    do {
        for (int i = 0; i < 6; ++i) {
            drawMenuItem(4, (((i + 1) * 10) + 1), i, -1, info[i]);
        }
    } while (u8g.nextPage());
}

void toggleSecondPart() {
    isSecondPart = true;
}

float convertTemperature(OneWire sensor) {
    byte i;
    byte data[12];

    for (i = 0; i < 9; i++) {           // we need 9 bytes
        data[i] = sensor.read();
    }

    // Convert the data to actual temperature
    // because the result is a 16 bit signed integer, it should
    // be stored to an "int16_t" type, which is always 16 bits
    // even when compiled on a 32 bit processor.
    int16_t raw = (data[1] << 8) | data[0];

    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time

    return (float) raw / 16.0;
}

void getTemp() {
    if (isFirstPart) {
        oilSensor.reset();
        oilSensor.skip();
        oilSensor.write(0x44);        // start conversion, use ds.write(0x44,1) with parasite power on at the end

        if (injectorFuncIsOn) {
            injectorSensor.reset();
            injectorSensor.skip();
            injectorSensor.write(0x44);
        }

        if (waterFuncIsOn) {
            waterSensor.reset();
            waterSensor.skip();
            waterSensor.write(0x44);
        }

        tempTimer.after(1000, toggleSecondPart);
        isFirstPart = false;
    }

    if (isSecondPart) {
        oilSensor.reset();
        oilSensor.skip();
        oilSensor.write(0xBE);         // Read Scratchpad
        tOilCur = convertTemperature(oilSensor);

        if (injectorFuncIsOn) {
            injectorSensor.reset();
            injectorSensor.skip();
            injectorSensor.write(0xBE);
            tInjectorCur = convertTemperature(injectorSensor);
        }

        if (waterFuncIsOn) {
            waterSensor.reset();
            waterSensor.skip();
            waterSensor.write(0xBE);
            tWaterCur = convertTemperature(waterSensor);
        }

        isFirstPart = true;
        isSecondPart = false;
    }
}

void turnOnValve() {
    valveIsOn = true;
    digitalWrite(RELAY_VALVE, LOW); //включаем клапан
}

void turnOffValve() {
    valveIsOn = false;
    digitalWrite(RELAY_VALVE, HIGH); //отключаем клапан
}

void turnOnSecondValve() {
    valve2IsOn = true;
    digitalWrite(RELAY_VALVE_2, LOW); //включаем клапан 2
}

void turnOffSecondValve() {
    valve2IsOn = false;
    digitalWrite(RELAY_VALVE_2, HIGH); //отключаем клапан 2
}

void turnOnPomp() {
    pompIsOn = true;
    digitalWrite(RELAY_OILPUMP, LOW); //включаем помпу
}

void turnOffPomp() {
    pompIsOn = false;
    digitalWrite(RELAY_OILPUMP, HIGH); //отключаем помпу
}

void turnOnIgnition() {
    ignitionIsOn = true;
    digitalWrite(RELAY_IGNITION, LOW); //включаем поджиг
}

void turnOffIgnition() {
    ignitionIsOn = false;
    digitalWrite(RELAY_IGNITION, HIGH); //отключаем поджиг
}

void turnOnFan() {
    fanIsOn = true;
    digitalWrite(RELAY_FAN, LOW); //включаем вентилятор
}

void turnOffFan() {
    fanIsOn = false;
    digitalWrite(RELAY_FAN, HIGH); //отключаем вентилятор
}

void turnOnInjector() {
    injectorIsOn = true;
    digitalWrite(RELAY_INJECTOR, LOW); //включаем форсунку
}

void turnOffInjector() {
    injectorIsOn = false;
    digitalWrite(RELAY_INJECTOR, HIGH); //отключаем форсунку
}

void turnOnHeating() {
    heatingIsOn = true;
    digitalWrite(RELAY_HEATING, LOW); //включаем тен
}

void turnOffHeating() {
    heatingIsOn = false;
    digitalWrite(RELAY_HEATING, HIGH); //отключаем тен
}

void checkFlame() { //Проверка наличия огня
    turnOffIgnition();
    turnOffFan(); //Отключаем вентилятор
    turnOffValve(); //Отключаем клапан
    turnOffSecondValve(); //Отключаем клапан 2
    if (attempt < 5) { //Если попыток меньше 5
        if (flame) { //если нету огня
            ignition = false;
            delay(3000); //Ждём 3 секунды
        } else { //огонь есть
            ignition = true; //деактивируем поджиг
            attempt = 0;//обнуляем попытки
            turnOnFan(); //Включаем вентилятор
            turnOnValve(); //Включаем клапан
            turnOnSecondValve(); //Включаем клапан 2
        }
    } else {
        needsRestart = true;
        fillErrorInfo(ignitionError);
        if (!ignitionAlreadySent) {
            sendSms(u"Поджиг не удался. Требуется перезапуск", 38);
            ignitionAlreadySent = true;
        }
        ignition = true;// чтобы не запускался поджиг
    }
}

void checkSensors() {
    float current = curSensor.getCurrentDC();
    if (current < 0.07) {
        currentCheck++;
    } else {
        currentCheck = 0;
        if (isCurrentWarning) {
            if (!currentFineAlreadySent) {
                sendSms(u"Питание восстановлено", 21);
                currentFineAlreadySent = true;
            }
        }
        isCurrentWarning = false;
        currentAlreadyShown = false;
        currentAlreadySent = false;
    }
    if (currentCheck > 5) {
        isCurrentWarning = true;
        fillErrorInfo(currentWarning);
        if (!currentAlreadySent) {
            sendSms(u"Сбой питания от сети 220. Питание переключено на батарею", 56);
            currentAlreadySent = true;
            currentFineAlreadySent = false;
        }
    }

    //Проверка датчика температуры масла
    if (tOilCur == 0 || (tOilCur < 1 && tOilCur > -1)) {
        oilCheck++;
    } else {
        oilCheck = 0;
    }
    if (oilCheck > 5) {
        isSensorError = true;
        fillErrorInfo(oilSensorError);
        if (!oilAlreadySent) {
            sendSms(u"Отказ датчика температуры масла. Система остановлена", 52);
            oilAlreadySent = true;
        }
    }

    //Проверка датчика температуры воды
    if (waterFuncIsOn) {
        if (tWaterCur == 0 || (tWaterCur < 1 && tWaterCur > -1)) {
            waterCheck++;
        } else {
            waterCheck = 0;
        }
        if (waterCheck > 5) {
            isSensorError = true;
            fillErrorInfo(waterSensorError);
            if (!waterAlreadySent) {
                sendSms(u"Отказ датчика температуры воды. Система остановлена", 51);
                waterAlreadySent = true;
            }
        }
    }

    //Проверка датчика температуры форсунки
    if (injectorFuncIsOn) {
        if (tInjectorCur == 0 || (tInjectorCur < 1 && tInjectorCur > -1)) {
            injectorCheck++;
        } else {
            injectorCheck = 0;
        }
        if (injectorCheck > 5) {
            isSensorError = true;
            fillErrorInfo(injectorSensorError);
            if (!injectorAlreadySent) {
                sendSms(u"Отказ датчика температуры форсунки. Система остановлена", 55);
                injectorAlreadySent = true;
            }
        }
    }

    isNeedCheck = true;
}

void toggleInjectorFunc() {
    injectorFuncIsOn = !injectorFuncIsOn;
}

void toggleWaterFunc() {
    waterFuncIsOn = !waterFuncIsOn;
}

void loop() {
    int encoderClick = 0; //0 - кнопка не нажата; 1 - короткое нажатие; -1 - длительное нажатие
    encoderBtn.Update();
    if (encoderBtn.clicks != 0) encoderClick = encoderBtn.clicks;

    if (isInfo) {
        //Read sensors
        float deltaOil = tOil - tOilCur;
        float deltaWater = tWater - tWaterCur;
        float deltaInjector = tInjector - tInjectorCur;
        tOil = abs(deltaOil) > 50 ? tOil : tOilCur;
        tWater = abs(deltaWater) > 50 ? tWater : tWaterCur;
        tInjector = abs(deltaInjector) > 50 ? tInjector : tInjectorCur;

        flame = digitalRead(FLAME_SENSOR);
        int floatLevel = digitalRead(FLOAT_LEVEL);
        int floatOverflow = digitalRead(FLOAT_OVERFLOW);

        if (isNeedCheck) {
            errorCheckTimer.after(10 * 1000, checkSensors);
            isNeedCheck = false;
        }

        // Условия запуска горелки
        if (waterFuncIsOn) { //Если включен датчик температуры воды
            if (tWater < tempWaterMin) {// Если вода остыла, то запускаем процесс
                start = true;
            }

            if (tWater > tempWaterMax) { //если вода нагрелась
                start = false;  //отключаем всё
                ignition = false;
                firstOilHeating = true; //при следующем старте будем ждать прогрева масла
            }
        } else { //Иначе стартуем процесс не обращая внимания на воду
            start = true;
        }

        if (!floatLevel && firstStart) { //Если уровень масла низкий при первом запуске
            if (!pompIsOn) { //если помпа ещё не запущена
                turnOnPomp(); //Включаем масляную помпу
                oilPompTimer.after(10 * 60 * 1000, turnOffPomp);
            }
            start = false;
        } else if (firstStart) {
            turnOffPomp(); // Иначе, отключаем помпу
            firstStart = false;
        }

        if (floatOverflow) { //если перелив
            start = false;        //отключаем всё
            isSensorError = true;
            fillErrorInfo(oilOverflowSensorError);
        }

        if (injectorFuncIsOn && tInjector > 120) { // Если температура форсунки превышает 120 градусов
            start = false;
            isSensorError = true;
            fillErrorInfo(injectorOverheatSensorError);
        }

        if (tOil > 120) { // Если температура масла превышает 120 градусов
            start = false;
            isSensorError = true;
            fillErrorInfo(oilOverheatSensorError);
        }

        if (waterFuncIsOn && tWater > 95) { // Если температура воды превышает 95 градусов
            start = false;
            isSensorError = true;
            fillErrorInfo(waterOverheatSensorError);
        }

        // Если процесс запущен и датчики в норме
        if (start && !isSensorError) {

            if (!floatLevel) { //Если уровень масла низкий
                if (!pompIsOn) { //если помпа ещё не запущена
                    turnOnPomp(); //Включаем масляную помпу
                    oilPompTimer.after(10 * 60 * 1000, turnOffPomp);
                }
            } else {
                turnOffPomp(); // Иначе, отключаем помпу
            }

            if (tOil != 0 && tOil < tempOilMin) { //Если температура масла низкая
                turnOnHeating(); //включаем подогрев

            } else if (tOil != 0 && tOil > tempOilMax) { //Если температура масла достигла максимума
                turnOffHeating(); // отключаем подогрев
                firstOilHeating = false;
            }
            if ((tOil < tempOilMax && tOil > tempOilMin && !firstOilHeating) ||
                //если первый прогрев масла уже был, то запускаем поджиг если температура масла между max и min
                (tOil > tempOilMax)) {                   //либо ждём максимального прогрева масла

                if (needsRestart) { //Если поджиг не удался
                    turnOffFan(); //Отключаем вентилятор
                    turnOffValve(); //Отключаем клапан
                    turnOffSecondValve(); //Отключаем клапан 2
                }
                if (!ignition) { //Если поджиг не активирован
                    ignition = true; //активируем
                    attempt++; // считаем попытки поджига
                    turnOnFan(); //включаем вентилятор
                    turnOffValve(); //выключаем клапан
                    turnOffSecondValve(); //выключаем клапан 2
                    purgeTimer.after(5 * 1000, turnOnIgnition); //через 5 секунд после продувки включаем поджиг
                    valveTimer.after(7 * 1000, turnOnValve); //через 7 секунд включим клапан
                    valve2Timer.after(40 * 1000, turnOnSecondValve); //TODO: ? через 40 секунд включим второй клапан
                    ignitionTimer.after(17 * 1000, checkFlame); //через 17 секунд проверим наличие пламя
                }
                if (ignition && attempt == 0) {
                    if (flame) { //если нету огня
                        ignition = false; //сообщаем системе, что снова нужен поджиг
                    }
                }
            }

            if (injectorFuncIsOn) {
                if (tInjector != 0 && tInjector < tempInjectorMin) { //Если температура форсунки упала
                    turnOnInjector();               //включаем реле форсунки
                } else if (tInjector != 0 && tInjector > tempInjectorMax) { //Если форсунка прогрета
                    turnOffInjector(); //отключаем реле форсунки
                }
            }
        } else { //Если нет условий для процесса, выключаем всё
            if (!firstStart) {
                turnOffPomp();
            }
            turnOffHeating();
            turnOffInjector();
            turnOffIgnition();
            turnOffFan();
            turnOffValve();
            turnOffSecondValve();
        }

        displayInfo();

        getTemp();
        tempTimer.update();
        valveTimer.update();
        valve2Timer.update();
        purgeTimer.update();
        ignitionTimer.update();
        oilPompTimer.update();
        errorCheckTimer.update();
    }

    if (encoderClick == -1 && isInfo) {
        encoderClick = 0;
        isInfo = false;
        isMainMenu = true;
    }

    if (isMainMenu) {
        if (isCurrentWarning) {
            currentAlreadyShown = true;
        }
        turnOffValve();
        turnOffSecondValve();
        turnOffIgnition();
        turnOffFan();
        turnOffHeating();
        turnOffInjector();
        turnOffPomp();

        displayLargeMenu(menu, selectedMenu, mainFrameA, mainFrameB, mainDif, 7);
        if (encoderClick == 1) {
            encoderClick = 0;
            if (selectedMenu == 6) {
                isInfo = true;
                isMainMenu = false;
            } else if (selectedMenu == 3) {
                isMainMenu = false;
                isTestMenu = true;
            } else if (selectedMenu == 4) {
                isMainMenu = false;
                isNotificationMenu = true;
                getPhoneNumber();
            } else if (selectedMenu == 5) {
                isMainMenu = false;
                isFuncMenu = true;
            } else {
                isSetupMenu = true;
                isMainMenu = false;
                if (selectedMenu == 0) { //если настраиваем температуру масла
                    tempMin = tempOilMin;
                    tempMax = tempOilMax;
                } else if (selectedMenu == 1) { // если настраиваем температуру форсунки
                    tempMin = tempInjectorMin;
                    tempMax = tempInjectorMax;
                } else if (selectedMenu == 2) { //если настраиваем температуру воды
                    tempMin = tempWaterMin;
                    tempMax = tempWaterMax;
                }
            }
        }
    }

    if (isTestMenu) {
        String testMenu[8] = {
                "Помпа        " + String(pompIsOn ? "вкл" : "выкл"),
                "Форсунка     " + String(injectorIsOn ? "вкл" : "выкл"),
                "Тен          " + String(heatingIsOn ? "вкл" : "выкл"),
                "Вентилятор   " + String(fanIsOn ? "вкл" : "выкл"),
                "Поджиг       " + String(ignitionIsOn ? "вкл" : "выкл"),
                "Клапан       " + String(valveIsOn ? "вкл" : "выкл"),
                "Клапан 2     " + String(valve2IsOn ? "вкл" : "выкл"),
                "Выход"};
        displayLargeMenu(testMenu, selectedTestMenu, testFrameA, testFrameB, testDif, 8);
        if (encoderClick == 1) {
            encoderClick = 0;
            if (selectedTestMenu == 7) {
                isMainMenu = true;
                isTestMenu = false;
            } else if (selectedTestMenu == 0) {
                if (pompIsOn) {
                    turnOffPomp();
                } else {
                    turnOnPomp();
                }
            } else if (selectedTestMenu == 1) {
                if (injectorIsOn) {
                    turnOffInjector();
                } else {
                    turnOnInjector();
                }
            } else if (selectedTestMenu == 2) {
                if (heatingIsOn) {
                    turnOffHeating();
                } else {
                    turnOnHeating();
                }
            } else if (selectedTestMenu == 3) {
                if (fanIsOn) {
                    turnOffFan();
                } else {
                    turnOnFan();
                }
            } else if (selectedTestMenu == 4) {
                if (ignitionIsOn) {
                    turnOffIgnition();
                } else {
                    turnOnIgnition();
                }
            } else if (selectedTestMenu == 5) {
                if (valveIsOn) {
                    turnOffValve();
                } else {
                    turnOnValve();
                }
            } else if (selectedTestMenu == 6) {
                if (valve2IsOn) {
                    turnOffSecondValve();
                } else {
                    turnOnSecondValve();
                }
            }
        }
    }

    if (isNotificationMenu) {
        if (encoderClick == 1 && !isPhoneSetup) {
            encoderClick = 0;
            if (selectedNotifMenu == 0) {
                notificationIsOn = !notificationIsOn;
            } else if (selectedNotifMenu == 2) {
                //save phone number and notification toggle
                saveNotificationSettings();
                isPhoneSetup = false;
                isNotificationMenu = false;
                isMainMenu = true;
            } else if (selectedNotifMenu == 1) {
                isPhoneSetup = true;
                numSelIndex = 0;
                tempNum = phoneNumber[0];
            }
        }
        if (isPhoneSetup) {
            if (TurnDetected) {
                tempNum += encDif;
                TurnDetected = false;
            }
            if (tempNum > 9) tempNum = 0;
            if (tempNum < 0) tempNum = 9;
            phoneNumber[numSelIndex] = tempNum;
            if (encoderClick == 1) {
                numSelIndex++;
                tempNum = phoneNumber[numSelIndex];
                if (numSelIndex > 10) {
                    getPhoneNumber();
                    isPhoneSetup = false;
                }
            }
        }

        String notifMenu[3] = {
                "SMS оповещения: " + String(notificationIsOn ? " вкл" : "выкл"),
                "Номер:   " + String(isPhoneSetup ? "" : phoneNumberStr),
                "Сохранить"};
        displaySimpleMenu(notifMenu, selectedNotifMenu, 3);
    }

    if (isFuncMenu) {
        if (encoderClick == 1) {
            encoderClick = 0;
            if (selectedFuncMenu == 2) {
                saveFunctionSettings();
                isFuncMenu = false;
                isMainMenu = true;
            } else if (selectedFuncMenu == 0) {
                toggleInjectorFunc();
            } else if (selectedFuncMenu == 1) {
                toggleWaterFunc();
            }
        }
        String funcMenu[3] = {
                "Форсунка:       " + String(injectorFuncIsOn ? "вкл" : "выкл"),
                "Датчик воды:    " + String(waterFuncIsOn ? "вкл" : "выкл"),
                "Сохранить"};
        displaySimpleMenu(funcMenu, selectedFuncMenu, 3);
    }

    if (isSetupMenu) {
        if (TurnDetected && !isSetup) {
            selectedSubMenu += encDif;
            TurnDetected = false;
        }
        if (selectedSubMenu > 3) selectedSubMenu = 1;
        if (selectedSubMenu < 1) selectedSubMenu = 3;
        displaySetup(menu[selectedMenu], tempMin, tempMax);
        if (encoderClick == 1 && !isSetup) {
            if (selectedSubMenu == 3) { //если нажимаем Сохранить
                isSetupMenu = false;
                isMainMenu = true;
                selectedSubMenu = 1;
                if (selectedMenu == 0) { //если настраиваем температуру масла
                    tempOilMin = tempMin;
                    EEPROM.write(0, highByte(tempOilMin));
                    EEPROM.write(1, lowByte(tempOilMin));

                    tempOilMax = tempMax;
                    EEPROM.write(2, highByte(tempOilMax));
                    EEPROM.write(3, lowByte(tempOilMax));
                } else if (selectedMenu == 1) { // если настраиваем температуру форсунки
                    tempInjectorMin = tempMin;
                    EEPROM.write(4, highByte(tempInjectorMin));
                    EEPROM.write(5, lowByte(tempInjectorMin));

                    tempInjectorMax = tempMax;
                    EEPROM.write(6, highByte(tempInjectorMax));
                    EEPROM.write(7, lowByte(tempInjectorMax));
                } else if (selectedMenu == 2) { //если настраиваем температуру воды
                    tempWaterMin = tempMin;
                    EEPROM.write(8, highByte(tempWaterMin));
                    EEPROM.write(9, lowByte(tempWaterMin));

                    tempWaterMax = tempMax;
                    EEPROM.write(10, highByte(tempWaterMax));
                    EEPROM.write(11, lowByte(tempWaterMax));
                }

            } else {
                if (selectedSubMenu == 1) {
                    tempVal = tempMin;
                } else if (selectedSubMenu == 2) {
                    tempVal = tempMax;
                }
                isSetup = true;
            }

        } else if (encoderClick == 1 && isSetup) {
            if (selectedSubMenu == 1) {
                tempMin = tempVal;
            } else if (selectedSubMenu == 2) {
                tempMax = tempVal;
            }
            isSetup = false;
        }
    }

    if (isSetup) {
        if (TurnDetected) {
            tempVal += encDif;
            TurnDetected = false;
        }
    }
}

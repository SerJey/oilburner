#include <EEPROM.h>

#include "U8glib.h"
#include "OneWire.h"
#include "Timer.h"
#include "rus6x10.h"
#include "ClickButton.h"

// relays
#define RELAY_OILPUMP   30
#define RELAY_HEATING   34
#define RELAY_FAN       36
#define RELAY_IGNITION  38
#define RELAY_VALVE     40

// temperature sensors
#define TEMP_OIL        24
#define TEMP_WATER      28

// float switches
#define FLOAT_LEVEL     5
#define FLOAT_OVERFLOW  6

// flame sensor
#define FLAME_SENSOR    7

//lcd
#define LCD_RS  31
#define LCD_RW  33
#define LCD_E   35

//encoder
#define ENCODER_CLK 2
#define ENCODER_DT  3
#define ENCODER_SW  4

volatile boolean TurnDetected;
volatile boolean up;
volatile boolean isMainMenu;
volatile boolean isInfo;
volatile boolean isSetupMenu;
volatile boolean isSetup;
volatile boolean isTestMenu;

volatile boolean isFirstPart;
volatile boolean isSecondPart;

volatile boolean ignitionIsOn;
volatile boolean heatingIsOn;
volatile boolean fanIsOn;
volatile boolean valveIsOn;
volatile boolean pompIsOn;

U8GLIB_ST7920_128X64 u8g(LCD_E, LCD_RW, LCD_RS, U8G_PIN_NONE);

int selectedMenu = 0;
int selectedSubMenu = 1;
int selectedTestMenu = 0;
String menu[4] = {"Температура масла", "Температура воды", "Режим тестирования", "Выход"};
int tempVal;
int tempMin;
int tempMax;

float tOil;
float tWater;

int tempOilMin;
int tempOilMax;
int tempWaterMin;
int tempWaterMax;

Timer tempTimer;
Timer funTimer;
Timer ignitionTimer;
Timer oilPompTimer;

OneWire oilSensor(TEMP_OIL);
OneWire waterSensor(TEMP_WATER);

boolean ignition;
boolean start;
boolean needsRestart;
boolean firstOilHeating;
boolean firstStart;

int flame;
int attempt;

int testFrameA = 0;
int testFrameB = 3;
int testDif = 0;

int mainFrameA = 0;
int mainFrameB = 3;
int mainDif = 0;

ClickButton encoderBtn(ENCODER_SW, LOW, CLICKBTN_PULLUP);

void encoder() {
    if (digitalRead(ENCODER_CLK))
        up = digitalRead(ENCODER_DT);
    else
        up = !digitalRead(ENCODER_DT);
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
    pinMode(RELAY_HEATING, OUTPUT);
    pinMode(RELAY_FAN, OUTPUT);
    pinMode(RELAY_IGNITION, OUTPUT);
    pinMode(RELAY_VALVE, OUTPUT);

    attachInterrupt(0, encoder, FALLING);
    isInfo = true;
    isMainMenu = false;
    isSetupMenu = false;
    isSetup = false;

    isFirstPart = true;
    isSecondPart = false;

    ignition = false;
    needsRestart = false;
    firstOilHeating = true;
    attempt = 0;
    firstStart = true;

    //Read values from EEPROM
    byte high = EEPROM.read(0);
    byte low = EEPROM.read(1);
    tempOilMin = word(high, low);
    high = EEPROM.read(2);
    low = EEPROM.read(3);
    tempOilMax = word(high, low);
    high = EEPROM.read(8);
    low = EEPROM.read(9);
    tempWaterMin = word(high, low);
    high = EEPROM.read(10);
    low = EEPROM.read(11);
    tempWaterMax = word(high, low);

    encoderBtn.longClickTime = 2000;
    encoderBtn.multiclickTime = 0;
    encoderBtn.debounceTime = -20;
}


void drawMenuItem(int x, int y, int draw, int selected, String text) {
    u8g.setColorIndex(1);
    if (draw == selected && !isSetup) {
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
}

void displayMenu() {
    u8g.firstPage();
    if (selectedMenu > mainFrameB) {
        mainDif = selectedMenu - 3;
        mainFrameB = selectedMenu;
        mainFrameA = mainFrameB - 3;
    }
    if (selectedMenu < mainFrameA) {
        mainFrameA = selectedMenu;
        mainFrameB = mainFrameA + 3;
        mainDif = selectedMenu;
    }
    do {
        for (int i = mainFrameA; i <= mainFrameB; i++) {
            drawMenuItem(4, (((i - mainDif + 1) * 13) + 2), i, selectedMenu, menu[i]);
        }
    } while (u8g.nextPage());
}

void displayTestMenu(String testMenu[]) {
    u8g.firstPage();
    if (selectedTestMenu > testFrameB) {
        testDif = selectedTestMenu - 3;
        testFrameB = selectedTestMenu;
        testFrameA = testFrameB - 3;
    }
    if (selectedTestMenu < testFrameA) {
        testFrameA = selectedTestMenu;
        testFrameB = testFrameA + 3;
        testDif = selectedTestMenu;
    }
    do {
        for (int i = testFrameA; i <= testFrameB; i++) {
            drawMenuItem(4, ((((i - testDif) + 1) * 13) + 2), i, selectedTestMenu, testMenu[i]);
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

void displayInfo() {
    String oil = String(" Масло:     ") + String(tOil) + String("`C");
    String wat = String(" Вода:      ") + String(tWater) + String("`C");
    String status = "       CТАТУС";
    if (ignition && attempt > 0) {
        status = String(" Поджиг:    ") + String(attempt) + String(" п.");
    }
    String flm = String(" Огонь:     есть");
    if (flame) {
        flm = String(" Огонь:     нет");
    }
    String oilPmp = String(" Насос:     выкл");
    if (pompIsOn) {
        oilPmp = String(" Насос:     вкл");
    }
    String info[6] = {status, oil, wat, flm, oilPmp};
    if (needsRestart) {
        info[0] = "";
        info[1] = "   ОШИБКА ПОДЖИГА!";
        info[2] = "    ПЕРЕЗАПУСТИТЕ";
        info[3] = "     КОНТРОЛЛЕР!";
        info[4] = "";
        info[5] = "";
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

        waterSensor.reset();
        waterSensor.skip();
        waterSensor.write(0x44);

        tempTimer.after(1000, toggleSecondPart);
        isFirstPart = false;
    }

    if (isSecondPart) {
        oilSensor.reset();
        oilSensor.skip();
        oilSensor.write(0xBE);         // Read Scratchpad
        tOil = convertTemperature(oilSensor);

        waterSensor.reset();
        waterSensor.skip();
        waterSensor.write(0xBE);
        tWater = convertTemperature(waterSensor);

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
    if (attempt < 5) { //Если попыток меньше 5
        if (flame) { //если нету огня
            ignition = false;
            delay(3000); //Ждём 3 секунды
        } else { //огонь есть
            ignition = true; //деактивируем поджиг
            attempt = 0;//обнуляем попытки
            turnOnFan(); //Включаем вентилятор
            turnOnValve(); //Включаем клапан
        }
    } else {
        needsRestart = true;
        ignition = true;// чтобы не запускался поджиг
    }

}

void loop() {
    //Read sensors
    int encoderClick = 0; //0 - кнопка не нажата; 1 - короткое нажатие; -1 - длительное нажатие
    encoderBtn.Update();
    if (encoderBtn.clicks != 0) encoderClick = encoderBtn.clicks;

    flame = digitalRead(FLAME_SENSOR);
    int floatLevel = digitalRead(FLOAT_LEVEL);
    int floatOverflow = digitalRead(FLOAT_OVERFLOW);

    if (isInfo) {

        // Условия запуска горелки
        if ((tWater != 0) && (tWater < tempWaterMin)) {// Если вода остыла, то запускаем процесс
            start = true;
        }

        if (tWater > tempWaterMax) { //если вода нагрелась
            start = false;  //отключаем всё
            ignition = false;
            firstOilHeating = true; //при следующем старте будем ждать прогрева масла
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
        }

        if (tOil > 125) { // Если температура масла превышает 125 градусов
            start = false;
        }


        // Если процесс запущен
        if (start) {

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
                }
                if (!ignition) { //Если поджиг не активирован
                    ignition = true; //активируем
                    attempt++; // считаем попытки поджига
                    turnOnFan(); //включаем вентилятор
                    turnOnIgnition(); //включаем реле поджига
                    turnOffValve(); //выключаем клапан
                    funTimer.after(5 * 1000, turnOnValve); //через 5 секунд включим клапан
                    ignitionTimer.after(10 * 1000, checkFlame); //через 10 секунд проверим наличие пламя
                }
                if (ignition && attempt == 0) {
                    if (flame) { //если нету огня
                        ignition = false; //сообщаем системе, что снова нужен поджиг
                    }
                }
            }

        } else { //Если нет условий для процесса, выключаем всё
            if (!firstStart) {
                turnOffPomp();
            }
            turnOffHeating();
            turnOffIgnition();
            turnOffFan();
            turnOffValve();
        }
    }

    if (encoderClick == -1 && isInfo) {
        encoderClick = 0;
        isInfo = false;
        isMainMenu = true;
    }

    if (isInfo) {
        displayInfo();
    }

    if (isMainMenu) {
        turnOffValve();
        turnOffIgnition();
        turnOffFan();
        turnOffHeating();
        turnOffPomp();
        if (TurnDetected) {
            if (up)
                selectedMenu++;
            else
                selectedMenu--;
            TurnDetected = false;
        }
        if (selectedMenu > 3) selectedMenu = 0;
        if (selectedMenu < 0) selectedMenu = 3;
        displayMenu();
        if (encoderClick == 1) {
            encoderClick = 0;
            if (selectedMenu == 3) {
                isInfo = true;
                isMainMenu = false;
                isSetupMenu = false;
                isSetup = false;
                isTestMenu = false;
            } else if (selectedMenu == 2) {
                isSetupMenu = false;
                isMainMenu = false;
                isTestMenu = true;
            } else {
                isSetupMenu = true;
                isMainMenu = false;
                isTestMenu = false;
                if (selectedMenu == 0) { //если настраиваем температуру масла
                    tempMin = tempOilMin;
                    tempMax = tempOilMax;
                } else if (selectedMenu == 1) { //если настраиваем температуру воды
                    tempMin = tempWaterMin;
                    tempMax = tempWaterMax;
                }
            }
        }
    }

    if (isTestMenu) {
        if (TurnDetected && !isSetup) {
            if (up)
                selectedTestMenu++;
            else
                selectedTestMenu--;
            TurnDetected = false;
        }
        if (selectedTestMenu > 5) selectedTestMenu = 0;
        if (selectedTestMenu < 0) selectedTestMenu = 5;

        String testMenu[6] = {
                "Помпа        " + String(pompIsOn ? "вкл" : "выкл"),
                "Тен          " + String(heatingIsOn ? "вкл" : "выкл"),
                "Вентилятор   " + String(fanIsOn ? "вкл" : "выкл"),
                "Поджиг       " + String(ignitionIsOn ? "вкл" : "выкл"),
                "Клапан       " + String(valveIsOn ? "вкл" : "выкл"),
                "Выход"};

        displayTestMenu(testMenu);
        if (encoderClick == 1) {
            encoderClick = 0;
            if (selectedTestMenu == 5) {
                isMainMenu = true;
                isTestMenu = false;
            } else if (selectedTestMenu == 0) {
                if (pompIsOn) {
                    turnOffPomp();
                } else {
                    turnOnPomp();
                }
            } else if (selectedTestMenu == 1) {
                if (heatingIsOn) {
                    turnOffHeating();
                } else {
                    turnOnHeating();
                }
            } else if (selectedTestMenu == 2) {
                if (fanIsOn) {
                    turnOffFan();
                } else {
                    turnOnFan();
                }
            } else if (selectedTestMenu == 3) {
                if (ignitionIsOn) {
                    turnOffIgnition();
                } else {
                    turnOnIgnition();
                }
            } else if (selectedTestMenu == 4) {
                if (valveIsOn) {
                    turnOffValve();
                } else {
                    turnOnValve();
                }
            }
        }

    }

    if (isSetupMenu) {
        if (TurnDetected && !isSetup) {
            if (up)
                selectedSubMenu++;
            else
                selectedSubMenu--;
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
                } else if (selectedMenu == 1) { //если настраиваем температуру воды
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
                encoderClick = 0;
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
            if (up)
                tempVal++;
            else
                tempVal--;
            TurnDetected = false;
        }
    }

    getTemp();
    tempTimer.update();
    funTimer.update();
    ignitionTimer.update();
    oilPompTimer.update();
}


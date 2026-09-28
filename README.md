# GS32BIOS

**GS32BIOS** — это библиотека для ESP32 (Arduino IDE), которая позволяет создать профессиональный текстовый интерфейс (BIOS) прямо в терминале. Она идеально подходит для настройки устройств через Serial-порт без необходимости создания веб-интерфейса или использования дисплеев.

![Интерфейс](https://dev.piminoff.ru/img/github/UI-Terminal-blue.svg) ![Платформа](https://dev.piminoff.ru/img/github/Platform-ESP32-orange.svg)

## Скриншоты
![ГлавноеОкно](https://dev.piminoff.ru/img/github/GS32Bios/1.png)
![Редактирование](https://dev.piminoff.ru/img/github/GS32Bios/2.png)
![Выбор](https://dev.piminoff.ru/img/github/GS32Bios/3.png)

## ✨ Особенности

*   **Полноценный UI в терминале:** Использование ANSI-последовательностей для отрисовки рамок, цветов и навигации.
*   **Система вкладок:** Разделение настроек по страницам (например, Info, Wireless, Advanced).
*   **Иерархические страницы и подменю**: Поддержка вложенных разделов (например, `Network -> Wi-Fi`), управляемых по нажатию клавиши `Enter`.
*   **Расширенная типизация целых чисел:**
    *   `TYPE_INT` — стандартные целые числа (`int*`).
    *   `TYPE_UINT8` / `TYPE_INT8` — безопасная работа с байтовыми переменными (`uint8_t*`, `int8_t*`).
    *   `TYPE_UINT16` / `TYPE_INT16` — работа с 16-битными переменными (`uint16_t*`, `int16_t*`).
*   **Типы данных:**
    *   `TYPE_INFO` — вывод статической информации.
    *   `TYPE_TEXT` — редактирование строк с модальным окном ввода.
    *   `TYPE_BOOL` — переключатели [Enabled/Disabled].
    *   `TYPE_SELECT` — выпадающие списки выбора.
    *   `TYPE_DYNAMIC_SELECT` — Динамические списки, сканируемые «на лету».
    *   `TYPE_ACTION` — кнопки для выполнения функций (Save, Reset, etc).
*   **Автоматическая страница Info:** Отображение модели чипа, объема Flash, свободной ОЗУ и времени работы (Uptime).
*   **Гибкий брендинг:** Настройка заголовка шапки, названия продукта и версии.
*   **Система колбэков:** Легкая интеграция с `Preferences.h` для сохранения настроек и перехват специальных клавиш (включая **F1–F12**, стрелки, Home/End и др.).
*   **Защита от сбоев:** Предотвращение входа на пустые страницы и безопасное отображение диапазонов в формате `(range: -64 .. 64)`.

## 🚀 Установка

1.  Скачайте файлы `GS32BIOS.h` и `GS32BIOS.cpp`.
2.  Создайте папку `GS32BIOS` в вашей директории библиотек Arduino (обычно `Documents/Arduino/libraries/`).
3.  Поместите скачанные файлы в созданную папку.
4.  Перезапустите Arduino IDE.

## ⌨️ Навигация

| Клавиша | Действие |
| :--- | :--- |
| **Стрелки влево/вправо** | Переключение между вкладками (страницами) |
| **Стрелки вверх/вниз** | Перемещение по пунктам меню |
| **Enter** | Редактирование параметра или выполнение действия |
| **Esc** | Отмена ввода текста или закрытие списка выбора |
| **Backspace** | Удаление символа при вводе текста |
| **F1 – F12** | Специальные функциональные клавиши (доступны в колбэке) |

## 📖 Быстрый старт

> ⚠️ **Важно:** Встроенный Serial Monitor в Arduino IDE **не поддерживает** ANSI-терминал и псевдографику! Используйте сторонние терминалы, такие как **PuTTY** или **Tera Term** (скорость порта `115200`).

```cpp
#include <Arduino.h>
#include "GS32BIOS.h"

GS32BIOS bios;

// Переменные для примера
char ssidBuffer[32] = "MyHomeWiFi";
int channelNum = 6;
uint8_t brightness = 80;
int16_t temperatureOffset = -5;
bool dhcpEnabled = true;
int selectedAuthMode = 0;
const char* authModes[] = {"Open", "WPA2-PSK", "WPA3-SAE", "WEE"};

int selectedWifiNetwork = 0;
bool customShellMode = false; // Флаг для демонстрации переключения на кастомный Shell

// Демо-функция для динамического списка (сканирование сетей "на лету")
std::vector<String> scanAvailableNetworks() {
  std::vector<String> networks;
  networks.push_back("Home_Net_5G (-45dBm)");
  networks.push_back("Guest_WiFi (-62dBm)");
  networks.push_back("IoT_Smart_House (-70dBm)");
  networks.push_back("Neighbor_Network (-85dBm)");
  return networks;
}

void setup() {
  Serial.begin(115200);
  while(!Serial); // Ожидание подключения Serial в PuTTY

  // 1. Настройка заголовков бренда
  bios.setHeaderTitle("GS-32 PRO");
  bios.setProductInfo("IoT Controller", "v2.5.1");

  // 2. Создание страниц и иерархии (родительские страницы)
  bios.addPage("Network");                  // Страница верхнего уровня
  bios.addPage("Settings");                 // Страница верхнего уровня
  bios.addPage("Tools");                    // Страница с действиями и переключением на Shell

  // Страницы подменю (указываем родителя вторым параметром)
  bios.addPage("Wi-Fi", "Network");         // Network -> Wi-Fi
  bios.addPage("Ethernet", "Network");      // Network -> Ethernet

  // 4. Добавление пунктов-ссылок для перехода в подменю по Enter
  bios.addSubMenuAction("Network", "Configure Wi-Fi...     ", "Wi-Fi");
  bios.addSubMenuAction("Network", "Configure Ethernet...  ", "Ethernet");

  // 5. Наполнение пунктов меню по страницам
  // Страница: Settings
  bios.addBool("Settings", "Enable MQTT Logs  ", &dhcpEnabled);
  bios.addInt("Settings", "Heartbeat Interval", &channelNum, 1, 60);
  bios.addUInt8("Settings", "Display Brightness", &brightness, 0, 100);
  bios.addInt16("Settings", "Temp Offset       ", &temperatureOffset, -40, 40);
  bios.addText("Settings", "Device Hostname   ", ssidBuffer, 32, false);

  // Страница: Wi-Fi (внутри Network)
  bios.addText("Wi-Fi", "SSID Name         ", ssidBuffer, 32, false);
  bios.addSelect("Wi-Fi", "Security Mode     ", &selectedAuthMode, 4, authModes);
  bios.addDynamicSelect("Wi-Fi", "Select AP (Scan)  ", &selectedWifiNetwork, scanAvailableNetworks);

  // Страница: Ethernet (внутри Network)
  bios.addBool("Ethernet", "Use DHCP Client   ", &dhcpEnabled);

  // Страница: Tools (Демонстрация disable / enable для Shell)
  bios.addAction("Tools", "Switch to Custom Shell  ", []() {
    customShellMode = true;
    bios.disable(); // Отключаем BIOS, освобождаем Serial и возвращаем видимый курсор
    Serial.println("\n--- GS-32 Custom Shell Mode ---");
    Serial.println("Type 'help' for commands, or type 'bios' to return.");
    Serial.print("shell> ");
  });

  // 6. Обработчики
  bios.onSave([]() {
    Serial.println("\n[CALLBACK] User saved settings! Writing to EEPROM/SPIFFS...");
  });

  bios.onFactoryReset([]() {
    Serial.println("\n[CALLBACK] Factory reset triggered!");
  });

  bios.onKeyPress([](int key, const char* activePage) {
    if (key == KEY_F1) {
      Serial.printf("\n[F1] Pressed on page: %s\n", activePage);
    } else if (key == 'h' || key == 'H') {
      Serial.printf("\n[HELP] Pressed 'H' on page: %s\n", activePage);
    }
  });

  // Запуск системы
  bios.begin();
}

void loop() {
  // Если активирован режим кастомного Shell, обрабатываем команды вручную
  if (customShellMode) {
    if (Serial.available()) {
      String cmd = Serial.readStringUntil('\n');
      cmd.trim();
      
      if (cmd.equalsIgnoreCase("bios")) {
        customShellMode = false;
        bios.enable(); // Возвращаем управление BIOS, очищаем экран и скрываем курсор
      } else if (cmd.equalsIgnoreCase("help")) {
        Serial.println("Available commands: help, uptime, bios");
      } else if (cmd.equalsIgnoreCase("uptime")) {
        Serial.printf("Uptime: %lu seconds\n", millis() / 1000);
      } else if (cmd.length() > 0) {
        Serial.printf("Unknown command: '%s'. Type 'bios' to return to BIOS.\n", cmd.c_str());
      }
      
      if (customShellMode) {
        Serial.print("shell> ");
      }
    }
    return;
  }

  // Обязательный вызов обработчика BIOS в штатном режиме
  bios.handle();
}
```

## 🕹 Управление в терминале

* **`←` / `→` (Стрелки влево/вправо)**: Переключение между вкладками верхнего уровня (Info, Network, Settings, Tools, Exit).
* **`↑` / `↓` (Стрелки вверх/вниз)**: Перемещение по пунктам меню текущей страницы или элементам выпадающего списка.
* **`Enter`**: 
  * Открыть выбранный пункт / войти в подменю (`>>`).
  * Переключить булев параметр (`Enabled/Disabled`).
  * Открыть модальное окно редактирования текста или числа (`INT`, `UINT8`, `INT8`, `UINT16`, `INT16`).
  * Подтвердить выбор во всплывающем списке.
* **`Esc`**: 
  * Закрыть модальное окно / отменить изменения.
  * Вернуться на уровень выше из подменю.

## 🛠 Методы API

### Инициализация и настройка
* `GS32BIOS()` — конструктор.
* `void setHeaderTitle(const String &title)` — задает заголовок в левом углу шапки (по умолчанию `"GS-32"`).
* `void setProductInfo(const String &name, const String &version)` — задает имя и версию продукта для системной страницы `Info`.
* `void setTheme(const char* bgWork, const char* bgHeader, const char* highlight, const char* tabActive, const char* popupBg, const char* popupHighlight)` — настраивает цветовую палитру (ANSI-коды).
* `void begin()` — инициализирует работу системы и отрисовывает стартовый экран.
* `void handle()` — основной метод опроса входящих команд (вызывать в `loop()`).

### Управление страницами
* `void addPage(const String &pageName, const String &parentPage = "")` — создает страницу. Если указан `parentPage`, страница становится вложенным подменю.
* `void addSubMenuAction(const String &pageName, const String &label, const String &targetPageName)` — создает пункт-ссылку на странице, по которому пользователь переходит в дочернее подменю по `Enter`.

### Добавление элементов управления
* `void addInfo(const String &label, const String &value)` — информационное поле (только для чтения, на странице `Info`).
* `void addInfoEx(const String &pageName, const String &label, const String &value)` — информационное поле (на любую страницу).
* `void addText(const String &pageName, const String &label, char* valPtr, size_t maxLen, bool allowEmpty = true)` — редактируемое текстовое поле.
* `void addInt(const String &pageName, const String &label, int* valPtr, int minVal, int maxVal)` — числовое поле (`int`).
* `void addUInt8(const String &pageName, const String &label, uint8_t* valPtr, uint8_t minVal, uint8_t maxVal)` — байтовое числовое поле без знака (`uint8_t`).
* `void addInt8(const String &pageName, const String &label, int8_t* valPtr, int8_t minVal, int8_t maxVal)` — байтовое числовое поле со знаком (`int8_t`).
* `void addUInt16(const String &pageName, const String &label, uint16_t* valPtr, uint16_t minVal, uint16_t maxVal)` — 16-битное числовое поле без знака (`uint16_t`).
* `void addInt16(const String &pageName, const String &label, int16_t* valPtr, int16_t minVal, int16_t maxVal)` — 16-битное числовое поле со знаком (`int16_t`).
* `void addBool(const String &pageName, const String &label, bool* valPtr)` — переключатель состояний (`true / false`).
* `void addSelect(const String &pageName, const String &label, int* valPtr, int optionsCount, const char** options)` — выпадающий список вариантов.
* `void addDynamicSelect(const String &pageName, const String &label, int* valPtr, std::function<std::vector<String>()> fetchOptionsFunc)` — динамический список, генерируемый функцией «на лету» (например, для сканирования сетей).
* `void addAction(const String &pageName, const String &label, std::function<void()> action)` — кнопка-действие, выполняющая переданную лямбда-функцию.

### Интеграция с Callbacks
* `void enable()` — включает обработку BIOS и выводит интерфейс в терминал.
* `void disable()` — выключает обработку BIOS, освобождая Serial-порт для внешних задач.
* `bool isActive()` — возвращает текущий статус активности BIOS.
* `void onSave(std::function<void()> callback)` — callback при выборе стандартного действия сохранения.
* `void onFactoryReset(std::function<void()> callback)` — callback при сбросе к заводским настройкам.
* `void onKeyPress(std::function<void(int, const char*)> callback)` — перехватчик нажатий клавиш (принимает код клавиши `int` и имя активной страницы). Поддерживает константы специальных клавиш (`KEY_F1` – `KEY_F12`, `KEY_UP`, `KEY_ENTER` и т.д.).

## 🎨 Темы оформления (ANSI цвета)

Метод `setTheme()` принимает строки с ANSI-кодами форматирования (цвет текста и фона через точку с запятой `;`):
* `bgWork` — фон рабочей зоны (например, `"37;44"` — белый текст на синем фоне).
* `bgHeader` — фон шапки (`"30;47"` — черный текст на белом фоне).
* `highlight` — подсветка выбранной строки меню.
* `tabActive` — цвет активной вкладки в шапке.
* `popupBg` — фон всплывающего окна.
* `popupHighlight` — подсветка пункта в всплывающем окне.

## 📝 Требования
*   Плата: **ESP32** (любая версия).
*   Терминал: Любой с поддержкой **ANSI/VT100** (PuTTY, TeraTerm, встроенный монитор порта VS Code/PlatformIO). 
    *   *Примечание: Стандартный монитор порта Arduino IDE не поддерживает ANSI-цвета.*

## Лицензия
MIT Лицензия. Используйте в любых проектах!

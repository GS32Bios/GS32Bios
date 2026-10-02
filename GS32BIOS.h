#ifndef GS32BIOS_H
#define GS32BIOS_H

#include <Arduino.h>
#include <vector>
#include <functional>

enum MenuItemType {
  TYPE_INFO,
  TYPE_DYNAMIC_INFO,
  TYPE_TEXT,
  TYPE_INT,
  TYPE_UINT8,
  TYPE_INT8,
  TYPE_UINT16,
  TYPE_INT16,
  TYPE_BOOL,
  TYPE_SELECT,
  TYPE_DYNAMIC_SELECT,
  TYPE_ACTION,
  TYPE_SUBMENU_LINK
};

enum BIOSKey {
  KEY_NONE = 0,
  KEY_ENTER = 10, KEY_ESC = 27, KEY_BACKSPACE = 127,
  KEY_UP = 256, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
  KEY_HOME, KEY_END, KEY_PAGE_UP, KEY_PAGE_DOWN,
  KEY_INSERT, KEY_DELETE,
  KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
  KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12
};


struct MenuItem {
  String page;
  String label;
  MenuItemType type;
  void* valPtr;
  int minVal;
  int maxVal;
  int maxOptions;
  const char** options;
  std::function<std::vector<String>()> dynamicOptionsFunc;
  std::function<String()> dynamicInfoFunc; 
  std::function<void()> action;
  size_t maxLen;
  bool allowEmpty;
  String targetPage;
  std::function<void()> onChange = nullptr;
};

struct NavState {
    int currentPageIdx;
    int localActiveIndex;
    int activeItemIndex;
};

class GS32BIOS {
public:
  GS32BIOS();
  ~GS32BIOS();

  void begin();
  void handle();
  void enable();
  void disable();

  void setHeaderTitle(const String &title);
  void setProductInfo(const String &name, const String &version);
  void setTheme(const char* bgWork, const char* bgHeader,
                const char* highlight, const char* tabActive,
                const char* popupBg, const char* popupHighlight);

  void addPage(const String &pageName, const String &parentPage = "");
  void addSubMenuAction(const String &pageName, const String &label, const String &targetPageName);
  void addInfo(const String &label, const String &value);
  void addInfoEx(const String &pageName, const String &label, const String &value);
  void addDynamicInfo(const String &label, std::function<String()> fetchInfoFunc);
  void addDynamicInfoEx(const String &pageName, const String &label, std::function<String()> fetchInfoFunc);
  void addText(const String &pageName, const String &label, char* valPtr, size_t maxLen, bool allowEmpty = true, std::function<void()> onChange = nullptr);
  void addInt(const String &pageName, const String &label, int* valPtr, int minVal, int maxVal, std::function<void()> onChange = nullptr);
  void addBool(const String &pageName, const String &label, bool* valPtr, std::function<void()> onChange = nullptr);
  void addSelect(const String &pageName, const String &label, int* valPtr, int optionsCount, const char** options, std::function<void()> onChange = nullptr);
  void addDynamicSelect(const String &pageName, const String &label, int* valPtr, std::function<std::vector<String>()> fetchOptionsFunc, std::function<void()> onChange = nullptr);
  void addAction(const String &pageName, const String &label, std::function<void()> action);
  void addUInt8(const String& pageName, const String& label,
                uint8_t* valPtr, uint8_t minVal, uint8_t maxVal, std::function<void()> onChange = nullptr);
  void addInt8(const String& pageName, const String& label,
              int8_t* valPtr, int8_t minVal, int8_t maxVal, std::function<void()> onChange = nullptr);
  void addUInt16(const String& pageName, const String& label,
                uint16_t* valPtr, uint16_t minVal, uint16_t maxVal, std::function<void()> onChange = nullptr);
  void addInt16(const String& pageName, const String& label,
                int16_t* valPtr, int16_t minVal, int16_t maxVal, std::function<void()> onChange = nullptr);

  void onSave(std::function<void()> callback);
  void onFactoryReset(std::function<void()> callback);
  void onKeyPress(std::function<void(int, const char*)> keyPressCallback);


  struct PageNode {
    String name;
    String parent;
    bool isSubmenu;
  };

  struct ThemeConfig {
    const char* bgWork;
    const char* bgHeader;
    const char* highlight;
    const char* tabActive;
    const char* popupBg;
    const char* popupHighlight;
  };

private:
  String headerTitle;
  String productName;
  String productVersion;
  ThemeConfig theme;

  std::vector<PageNode> pages;
  std::vector<MenuItem> menuItems;

  int currentPageIdx;
  int activeItemIndex;
  int localActiveIndex;
  bool isEditing;
  bool isSubMenuOpen;
  int subMenuSelectionIndex;

  String inputBuffer;
  char originalTextValue[64];
  int32_t originalIntValue;

  std::vector<String> activeSubmenuPath;
  int currentNavDepth;
  bool _isActive;
  std::vector<NavState> navHistory;
  std::function<void()> saveCallback;
  std::function<void()> resetCallback;
  std::function<void(char, const char*)> keyPressCallback;

  char sys_chip_model[32];
  char sys_flash_size[16];

  std::vector<PageNode> getCurrentLevelPages();
  void updateActiveIndex();
  void moveActiveItem(int dir);

  void setCursor(Stream &c, int r, int col);
  void setColors(Stream &c, const char* code);
  void drawRect(Stream &c, int sr, int sc, int h, int w, const char* clr);
  void drawBoxBorder(Stream &client, int br, int bc, int bw, int bh);
  void renderMenu(Stream &client);
  void handleInput(Stream &client);
  bool inputLevelCheckEmpty(MenuItem *it, const String &buf);
  std::vector<size_t> getPageItemIndices(const String& pageName);
};

#endif

#include "GS32BIOS.h"
#include <stdint.h>
#include <stdlib.h>

namespace {
bool isIntegerItem(MenuItemType type)
{
  return type == TYPE_INT || type == TYPE_UINT8 || type == TYPE_INT8 ||
         type == TYPE_UINT16 || type == TYPE_INT16;
}

int32_t readIntegerValue(const MenuItem& item)
{
  switch (item.type)
  {
    case TYPE_UINT8: return *static_cast<const uint8_t*>(item.valPtr);
    case TYPE_INT8: return *static_cast<const int8_t*>(item.valPtr);
    case TYPE_UINT16: return *static_cast<const uint16_t*>(item.valPtr);
    case TYPE_INT16: return *static_cast<const int16_t*>(item.valPtr);
    case TYPE_INT: return *static_cast<const int*>(item.valPtr);
    default: return 0;
  }
}

void writeIntegerValue(const MenuItem& item, int32_t value)
{
  switch (item.type)
  {
    case TYPE_UINT8: *static_cast<uint8_t*>(item.valPtr) = static_cast<uint8_t>(value); break;
    case TYPE_INT8: *static_cast<int8_t*>(item.valPtr) = static_cast<int8_t>(value); break;
    case TYPE_UINT16: *static_cast<uint16_t*>(item.valPtr) = static_cast<uint16_t>(value); break;
    case TYPE_INT16: *static_cast<int16_t*>(item.valPtr) = static_cast<int16_t>(value); break;
    case TYPE_INT: *static_cast<int*>(item.valPtr) = static_cast<int>(value); break;
    default: break;
  }
}

int parseCsiKey(Stream& client)
{
  // Called after ESC '[' has been consumed. CSI arrows end with A/B/C/D.
  // Numeric keys such as F5 (15~) end with '~'.
  char seq[12];
  size_t n = 0;
  unsigned long start = millis();
  while (millis() - start < 40 && n < sizeof(seq) - 1)
  {
    if (!client.available()) { delay(1); continue; }
    char c = (char)client.read();
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '~')
    {
      seq[n] = '\0';
      if (c == 'A') return KEY_UP;
      if (c == 'B') return KEY_DOWN;
      if (c == 'C') return KEY_RIGHT;
      if (c == 'D') return KEY_LEFT;
      if (c == 'H') return KEY_HOME;
      if (c == 'F') return KEY_END;
      if (c == '~')
      {
        int code = atoi(seq);
        switch (code)
        {
          case 1: case 7: return KEY_HOME;
          case 2: return KEY_INSERT;
          case 3: return KEY_DELETE;
          case 4: case 8: return KEY_END;
          case 5: return KEY_PAGE_UP;
          case 6: return KEY_PAGE_DOWN;
          case 11: return KEY_F1;
          case 12: return KEY_F2;
          case 13: return KEY_F3;
          case 14: return KEY_F4;
          case 15: return KEY_F5;
          case 17: return KEY_F6;
          case 18: return KEY_F7;
          case 19: return KEY_F8;
          case 20: return KEY_F9;
          case 21: return KEY_F10;
          case 23: return KEY_F11;
          case 24: return KEY_F12;
          default: return KEY_NONE;
        }
      }
      return KEY_NONE;
    }
    if (n + 1 < sizeof(seq)) seq[n++] = c;
  }
  return KEY_NONE;
}

int readKey(Stream& client)
{
  int c = client.read();
  if (c < 0) return KEY_NONE;
  if (c == '\r' || c == '\n') return KEY_ENTER;
  if (c == 8 || c == 127) return KEY_BACKSPACE;
  if (c != 27) return c;

  unsigned long start = millis();
  while (!client.available() && millis() - start < 25) delay(1);
  if (!client.available()) return KEY_ESC;
  int introducer = client.read();
  if (introducer == '[') return parseCsiKey(client);
  if (introducer == 'O')
  {
    start = millis();
    while (!client.available() && millis() - start < 25) delay(1);
    if (!client.available()) return KEY_ESC;
    switch (client.read())
    {
      case 'P': return KEY_F1;
      case 'Q': return KEY_F2;
      case 'R': return KEY_F3;
      case 'S': return KEY_F4;
      case 'H': return KEY_HOME;
      case 'F': return KEY_END;
      default: return KEY_NONE;
    }
  }
  return KEY_ESC;
}
} // namespace

GS32BIOS::GS32BIOS()
    : headerTitle("GS-32"), productName("GS-32 Engine"), productVersion("v1.0.0"),
      currentPageIdx(0), activeItemIndex(0), localActiveIndex(0),
      isEditing(false), isSubMenuOpen(false), subMenuSelectionIndex(0),
      saveCallback(nullptr), resetCallback(nullptr), keyPressCallback(nullptr),
      originalIntValue(0), currentNavDepth(0), _isActive(false)
{
  theme = {"30;47", "37;44", "44;37", "30;47", "37;44", "30;47"};
  pages.push_back({"Info", "", false});
  pages.push_back({"Exit", "", false});
}

GS32BIOS::~GS32BIOS()
{
  for (auto& item : menuItems)
    if (item.type == TYPE_INFO && item.valPtr && item.maxLen == 64)
      delete[] static_cast<char*>(item.valPtr);
}

void GS32BIOS::enable() { _isActive = true; Serial.print(F("\e[2J\e[H")); updateSystemStats(); renderMenu(Serial); }
void GS32BIOS::disable() { _isActive = false; Serial.print(F("\e[?25h")); Serial.println(F("\n[GS-32 BIOS] Disabled. Serial console released.")); }
void GS32BIOS::setHeaderTitle(const String& title) { headerTitle = title; }
void GS32BIOS::setProductInfo(const String& name, const String& version) { productName = name; productVersion = version; }

void GS32BIOS::setTheme(const char* bgWork, const char* bgHeader, const char* highlight,
                        const char* tabActive, const char* popupBg, const char* popupHighlight)
{
  theme = {bgWork, bgHeader, highlight, tabActive, popupBg, popupHighlight};
}

void GS32BIOS::addPage(const String& pageName, const String& parentPage)
{
  if (pageName == "Info" || pageName == "Exit") return;
  for (const auto& p : pages) if (p.name == pageName) return;
  pages.insert(pages.end() - 1, {pageName, parentPage, false});
}

void GS32BIOS::addSubMenuAction(const String& pageName, const String& label, const String& targetPageName)
{
  MenuItem item{}; item.page = pageName; item.label = label; item.type = TYPE_SUBMENU_LINK;
  item.targetPage = targetPageName; menuItems.push_back(item);
}

void GS32BIOS::addInfo(const String& label, const String& value)
{
  char* valStr = new char[64]; strlcpy(valStr, value.c_str(), 64);
  MenuItem item{}; item.page = "Info"; item.label = label; item.type = TYPE_INFO;
  item.valPtr = valStr; item.maxLen = 64; item.allowEmpty = true; menuItems.push_back(item);
}

void GS32BIOS::addText(const String& pageName, const String& label, char* valPtr, size_t maxLen, bool allowEmpty)
{
  MenuItem item{}; item.page = pageName; item.label = label; item.type = TYPE_TEXT;
  item.valPtr = valPtr; item.maxLen = maxLen; item.allowEmpty = allowEmpty; menuItems.push_back(item);
}

void GS32BIOS::addInt(const String& pageName, const String& label, int* valPtr, int minVal, int maxVal)
{
  MenuItem item{}; item.page = pageName; item.label = label; item.type = TYPE_INT;
  item.valPtr = valPtr; item.minVal = minVal; item.maxVal = maxVal; menuItems.push_back(item);
}

void GS32BIOS::addUInt8(const String& pageName, const String& label, uint8_t* valPtr, uint8_t minVal, uint8_t maxVal)
{
  MenuItem item{}; item.page = pageName; item.label = label; item.type = TYPE_UINT8;
  item.valPtr = valPtr; item.minVal = minVal; item.maxVal = maxVal; menuItems.push_back(item);
}

void GS32BIOS::addInt8(const String& pageName, const String& label, int8_t* valPtr, int8_t minVal, int8_t maxVal)
{
  MenuItem item{}; item.page = pageName; item.label = label; item.type = TYPE_INT8;
  item.valPtr = valPtr; item.minVal = minVal; item.maxVal = maxVal; menuItems.push_back(item);
}

void GS32BIOS::addUInt16(const String& pageName, const String& label, uint16_t* valPtr, uint16_t minVal, uint16_t maxVal)
{
  MenuItem item{}; item.page = pageName; item.label = label; item.type = TYPE_UINT16;
  item.valPtr = valPtr; item.minVal = minVal; item.maxVal = maxVal; menuItems.push_back(item);
}

void GS32BIOS::addInt16(const String& pageName, const String& label, int16_t* valPtr, int16_t minVal, int16_t maxVal)
{
  MenuItem item{}; item.page = pageName; item.label = label; item.type = TYPE_INT16;
  item.valPtr = valPtr; item.minVal = minVal; item.maxVal = maxVal; menuItems.push_back(item);
}

void GS32BIOS::addBool(const String& pageName, const String& label, bool* valPtr)
{
  MenuItem item{}; item.page = pageName; item.label = label; item.type = TYPE_BOOL; item.valPtr = valPtr; menuItems.push_back(item);
}

void GS32BIOS::addSelect(const String& pageName, const String& label, int* valPtr, int optionsCount, const char** options)
{
  MenuItem item{}; item.page = pageName; item.label = label; item.type = TYPE_SELECT;
  item.valPtr = valPtr; item.maxOptions = optionsCount; item.options = options; menuItems.push_back(item);
}

void GS32BIOS::addDynamicSelect(const String& pageName, const String& label, int* valPtr, std::function<std::vector<String>()> fetchOptionsFunc)
{
  MenuItem item{}; item.page = pageName; item.label = label; item.type = TYPE_DYNAMIC_SELECT;
  item.valPtr = valPtr; item.dynamicOptionsFunc = fetchOptionsFunc; menuItems.push_back(item);
}

void GS32BIOS::addAction(const String& pageName, const String& label, std::function<void()> action)
{
  MenuItem item{}; item.page = pageName; item.label = label; item.type = TYPE_ACTION; item.action = action; menuItems.push_back(item);
}

void GS32BIOS::onSave(std::function<void()> callback) { saveCallback = callback; }
void GS32BIOS::onFactoryReset(std::function<void()> callback) { resetCallback = callback; }
void GS32BIOS::onKeyPress(std::function<void(int, const char*)> callback) { keyPressCallback = callback; }

void GS32BIOS::begin()
{
  Serial.println(F("\n=================================================================="));
  Serial.println(F(" [!] WARNING: use PuTTY/TeraTerm or another ANSI/VT100 terminal."));
  Serial.println(F("==================================================================\n")); delay(1000);
  snprintf(sys_chip_model, sizeof(sys_chip_model), "%s Rev.%d", ESP.getChipModel(), ESP.getChipRevision());
  snprintf(sys_flash_size, sizeof(sys_flash_size), "%d MB", (int)(ESP.getFlashChipSize() / (1024 * 1024)));
  addAction("Exit", "Save Changes and Reset", [this]() { if (saveCallback) saveCallback(); ESP.restart(); });
  addAction("Exit", "Discard Changes and Exit", []() { ESP.restart(); });
  addAction("Exit", "Factory Reset", [this]() { if (resetCallback) resetCallback(); ESP.restart(); });

  std::vector<MenuItem> baseInfo;
  auto createSysInfo = [](const char* label, char* value) {
    MenuItem item{}; item.page = "Info"; item.label = label; item.type = TYPE_INFO; item.valPtr = value; return item;
  };
  baseInfo.push_back(createSysInfo("Product Name", (char*)productName.c_str()));
  baseInfo.push_back(createSysInfo("Version", (char*)productVersion.c_str()));
  baseInfo.push_back(createSysInfo("Core Architecture", sys_chip_model));
  baseInfo.push_back(createSysInfo("Flash Memory Size", sys_flash_size));
  baseInfo.push_back(createSysInfo("System Free RAM", sys_free_ram));
  baseInfo.push_back(createSysInfo("System Uptime", sys_uptime));
  menuItems.insert(menuItems.begin(), baseInfo.begin(), baseInfo.end()); enable();
}

void GS32BIOS::handle() { if (_isActive && Serial.available()) handleInput(Serial); }

void GS32BIOS::updateSystemStats()
{
  snprintf(sys_free_ram, sizeof(sys_free_ram), "%d KB", (int)(ESP.getFreeHeap() / 1024));
  unsigned long sec = millis() / 1000;
  snprintf(sys_uptime, sizeof(sys_uptime), "%02d:%02d:%02d", (int)(sec / 3600), (int)((sec % 3600) / 60), (int)(sec % 60));
}

std::vector<GS32BIOS::PageNode> GS32BIOS::getCurrentLevelPages()
{
  std::vector<PageNode> result;
  String parent = (currentNavDepth == 0 || activeSubmenuPath.empty()) ? "" : activeSubmenuPath.back();
  for (const auto& p : pages) if (p.parent == parent) result.push_back(p);
  return result;
}

std::vector<size_t> GS32BIOS::getPageItemIndices(const String& pageName)
{
  std::vector<size_t> indices;
  for (size_t i = 0; i < menuItems.size(); ++i) if (menuItems[i].page == pageName) indices.push_back(i);
  return indices;
}

void GS32BIOS::updateActiveIndex()
{
  auto level = getCurrentLevelPages();
  if (level.empty()) { activeItemIndex = -1; localActiveIndex = 0; return; }
  if (currentPageIdx < 0 || currentPageIdx >= (int)level.size()) currentPageIdx = 0;
  auto indices = getPageItemIndices(level[currentPageIdx].name);
  localActiveIndex = 0;
  activeItemIndex = indices.empty() ? -1 : (int)indices[0];
}

void GS32BIOS::moveActiveItem(int dir)
{
  auto level = getCurrentLevelPages();
  if (level.empty() || currentPageIdx < 0 || currentPageIdx >= (int)level.size()) return;
  auto indices = getPageItemIndices(level[currentPageIdx].name);
  if (indices.empty()) { activeItemIndex = -1; localActiveIndex = 0; return; }
  localActiveIndex = (localActiveIndex + dir + (int)indices.size()) % (int)indices.size();
  activeItemIndex = (int)indices[localActiveIndex];
}

void GS32BIOS::setCursor(Stream& c, int r, int col) { c.print(F("\e[")); c.print(r); c.print(';'); c.print(col); c.print('H'); }
void GS32BIOS::setColors(Stream& c, const char* code) { c.print(F("\e[")); c.print(code); c.print('m'); }

void GS32BIOS::drawRect(Stream& c, int sr, int sc, int h, int w, const char* clr)
{
  setColors(c, clr);
  for (int r = 0; r < h; ++r) { setCursor(c, sr + r, sc); for (int i = 0; i < w; ++i) c.print(' '); }
}

void GS32BIOS::drawBoxBorder(Stream& client, int br, int bc, int bw, int bh)
{
  setCursor(client, br, bc); client.print('+'); for (int i = 0; i < bw - 2; ++i) client.print('-'); client.print('+');
  for (int i = 1; i < bh - 1; ++i) { setCursor(client, br + i, bc); client.print('|'); setCursor(client, br + i, bc + bw - 1); client.print('|'); }
  setCursor(client, br + bh - 1, bc); client.print('+'); for (int i = 0; i < bw - 2; ++i) client.print('-'); client.print('+');
}

void GS32BIOS::renderMenu(Stream& client)
{
  client.print(F("\e[?25l")); drawRect(client, 2, 1, 22, 80, theme.bgWork); drawRect(client, 1, 1, 1, 80, theme.bgHeader);
  setCursor(client, 1, 3); client.print(headerTitle);
  auto level = getCurrentLevelPages();
  if (currentNavDepth > 0) {
    setColors(client, theme.bgHeader); setCursor(client, 1, 14); client.print(F("> "));
    if (!level.empty() && currentPageIdx >= 0 && currentPageIdx < (int)level.size()) client.print(level[currentPageIdx].name);
  } else {
    int col = 14;
    for (size_t i = 0; i < level.size(); ++i) { setCursor(client, 1, col); setColors(client, (int)i == currentPageIdx ? theme.tabActive : theme.bgHeader); client.print(' '); client.print(level[i].name); client.print(' '); col += level[i].name.length() + 3; }
  }

  if (!level.empty() && currentPageIdx >= 0 && currentPageIdx < (int)level.size()) {
    String pageName = level[currentPageIdx].name; int row = 3;
    for (size_t i = 0; i < menuItems.size(); ++i) {
      const auto& item = menuItems[i]; if (item.page != pageName) continue;
      setCursor(client, row++, 2); setColors(client, (int)i == activeItemIndex ? theme.highlight : theme.bgWork);
      char value[44] = "";
      if ((item.type == TYPE_INFO || item.type == TYPE_TEXT) && item.valPtr) snprintf(value, sizeof(value), "%s", (char*)item.valPtr);
      else if (isIntegerItem(item.type) && item.valPtr) snprintf(value, sizeof(value), "%ld", (long)readIntegerValue(item));
      else if (item.type == TYPE_BOOL && item.valPtr) snprintf(value, sizeof(value), "%s", *(bool*)item.valPtr ? "[Enabled]" : "[Disabled]");
      else if (item.type == TYPE_SELECT && item.valPtr) { int s = *(int*)item.valPtr; if (item.options && s >= 0 && s < item.maxOptions && item.options[s]) snprintf(value, sizeof(value), "[ %s ]", item.options[s]); else snprintf(value, sizeof(value), "[ invalid ]"); }
      else if (item.type == TYPE_DYNAMIC_SELECT && item.valPtr && item.dynamicOptionsFunc) { auto opts = item.dynamicOptionsFunc(); int s = *(int*)item.valPtr; if (s >= 0 && s < (int)opts.size()) snprintf(value, sizeof(value), "[ %s ]", opts[s].c_str()); else snprintf(value, sizeof(value), "[ invalid ]"); }
      else if (item.type == TYPE_SUBMENU_LINK) snprintf(value, sizeof(value), ">>");
      char line[81];
      if (item.type == TYPE_ACTION || item.type == TYPE_SUBMENU_LINK) snprintf(line, sizeof(line), "   %-30s %-45s", item.label.c_str(), value);
      else snprintf(line, sizeof(line), "   %-28s : %-43s", item.label.c_str(), value);
      client.print(line);
    }
  }

  if (isSubMenuOpen && activeItemIndex >= 0 && activeItemIndex < (int)menuItems.size()) {
    const auto& item = menuItems[activeItemIndex]; std::vector<String> dyn; int count = item.maxOptions;
    if (item.type == TYPE_DYNAMIC_SELECT && item.dynamicOptionsFunc) { dyn = item.dynamicOptionsFunc(); count = (int)dyn.size(); }
    if (count > 0) { int bh = count + 2, br = 5, bc = 22, bw = 36; drawRect(client, br, bc, bh, bw, theme.popupBg); drawBoxBorder(client, br, bc, bw, bh);
      for (int i = 0; i < count; ++i) { setCursor(client, br + 1 + i, bc + 1); setColors(client, i == subMenuSelectionIndex ? theme.popupHighlight : theme.popupBg); String n = item.type == TYPE_SELECT ? String(item.options[i]) : dyn[i]; client.print(' '); client.print(n); for (size_t p = n.length(); p < (size_t)(bw - 4); ++p) client.print(' '); }
    }
  }

  if (isEditing && activeItemIndex >= 0 && activeItemIndex < (int)menuItems.size()) {
    const auto& item = menuItems[activeItemIndex]; int br = 8, bc = 15, bw = 50, bh = 6; drawRect(client, br, bc, bh, bw, theme.popupBg); drawBoxBorder(client, br, bc, bw, bh);
    setCursor(client, br + 1, bc + 2); client.print(F("EDIT: ")); client.print(item.label); setCursor(client, br + 3, bc + 2); setColors(client, theme.popupHighlight); client.print(' '); client.print(inputBuffer); client.print('_');
    for (size_t p = inputBuffer.length() + 1; p < (size_t)(bw - 6); ++p) client.print(' ');
    if (isIntegerItem(item.type)) { setCursor(client, br + 4, bc + 2); setColors(client, theme.popupBg); client.print(F("(range: ")); client.print(item.minVal); client.print(F(" .. ")); client.print(item.maxVal); client.print(')'); }
  }
  drawRect(client, 24, 1, 1, 80, theme.bgHeader); setCursor(client, 24, 3); client.print(F("NAV: [Arrows] Move, [Enter] Select, [Esc] Back."));
}

void GS32BIOS::handleInput(Stream& client)
{
  int key = readKey(client);
  if (key == KEY_NONE) return;
  auto level = getCurrentLevelPages();
  const char* pageName = (!level.empty() && currentPageIdx >= 0 && currentPageIdx < (int)level.size()) ? level[currentPageIdx].name.c_str() : "";
  if (keyPressCallback) keyPressCallback((char)key, pageName);

  if (key == KEY_ESC) {
    if (isEditing && activeItemIndex >= 0 && activeItemIndex < (int)menuItems.size()) {
      auto& it = menuItems[activeItemIndex];
      if (it.type == TYPE_TEXT && it.valPtr) strncpy((char*)it.valPtr, originalTextValue, it.maxLen);
      else if (isIntegerItem(it.type)) writeIntegerValue(it, originalIntValue);
      isEditing = false;
    } else if (isSubMenuOpen) isSubMenuOpen = false;
    else if (currentNavDepth > 0) { activeSubmenuPath.pop_back(); --currentNavDepth; currentPageIdx = 0; updateActiveIndex(); }
    renderMenu(client); return;
  }

  if (isEditing) {
    if (key == KEY_ENTER) {
      MenuItem& it = menuItems[activeItemIndex];
      if (isIntegerItem(it.type)) {
        String text = inputBuffer; text.trim();
        if (text.length()) { char* end = nullptr; long v = strtol(text.c_str(), &end, 10); if (end != text.c_str() && *end == '\0' && v >= it.minVal && v <= it.maxVal) writeIntegerValue(it, (int32_t)v); }
      } else if (inputLevelCheckEmpty(&it, inputBuffer) && it.valPtr) strlcpy((char*)it.valPtr, inputBuffer.c_str(), it.maxLen);
      isEditing = false; renderMenu(client); return;
    }
    if (key == KEY_BACKSPACE) { if (inputBuffer.length()) inputBuffer.remove(inputBuffer.length() - 1); renderMenu(client); return; }
    if (key >= 32 && key <= 126) { inputBuffer += (char)key; renderMenu(client); }
    return;
  }

  if (isSubMenuOpen) {
    if (activeItemIndex < 0 || activeItemIndex >= (int)menuItems.size()) { isSubMenuOpen = false; return; }
    auto& item = menuItems[activeItemIndex]; int count = item.maxOptions;
    if (item.type == TYPE_DYNAMIC_SELECT && item.dynamicOptionsFunc) count = (int)item.dynamicOptionsFunc().size();
    if (count <= 0) { isSubMenuOpen = false; renderMenu(client); return; }
    if (key == KEY_UP) subMenuSelectionIndex = (subMenuSelectionIndex + count - 1) % count;
    else if (key == KEY_DOWN) subMenuSelectionIndex = (subMenuSelectionIndex + 1) % count;
    else if (key == KEY_LEFT || key == KEY_ESC) isSubMenuOpen = false;
    else if (key == KEY_ENTER && item.valPtr) { *(int*)item.valPtr = subMenuSelectionIndex; isSubMenuOpen = false; }
    renderMenu(client); return;
  }

  level = getCurrentLevelPages();
  if (level.empty() || currentPageIdx < 0 || currentPageIdx >= (int)level.size()) return;
  String currentPage = level[currentPageIdx].name;
  auto indices = getPageItemIndices(currentPage);
  if (indices.empty()) { activeItemIndex = -1; localActiveIndex = 0; if (key == KEY_ENTER) renderMenu(client); return; }
  if (activeItemIndex < 0 || activeItemIndex >= (int)menuItems.size() || menuItems[activeItemIndex].page != currentPage) { localActiveIndex = 0; activeItemIndex = (int)indices[0]; }

  if (key == KEY_UP || key == KEY_DOWN) { moveActiveItem(key == KEY_UP ? -1 : 1); renderMenu(client); return; }
  if (currentNavDepth == 0 && (key == KEY_LEFT || key == KEY_RIGHT)) { currentPageIdx = (currentPageIdx + (key == KEY_RIGHT ? 1 : -1) + (int)level.size()) % (int)level.size(); updateActiveIndex(); renderMenu(client); return; }
  if (key != KEY_ENTER) return;

  MenuItem& it = menuItems[activeItemIndex];
  if (it.type == TYPE_SUBMENU_LINK) {
    bool pageExists = false, hasItems = false;
    for (const auto& p : pages) if (p.name == it.targetPage) { pageExists = true; break; }
    for (const auto& m : menuItems) if (m.page == it.targetPage) { hasItems = true; break; }
    if (!pageExists || !hasItems) return;
    activeSubmenuPath.push_back(currentPage); ++currentNavDepth;
    auto children = getCurrentLevelPages(); currentPageIdx = 0;
    for (size_t i = 0; i < children.size(); ++i) if (children[i].name == it.targetPage) { currentPageIdx = (int)i; break; }
    updateActiveIndex(); renderMenu(client); return;
  }
  if (it.type == TYPE_BOOL && it.valPtr) { *(bool*)it.valPtr = !*(bool*)it.valPtr; renderMenu(client); return; }
  if (it.type == TYPE_SELECT || it.type == TYPE_DYNAMIC_SELECT) {
    int count = it.maxOptions;
    if (it.type == TYPE_DYNAMIC_SELECT && it.dynamicOptionsFunc) count = (int)it.dynamicOptionsFunc().size();
    if (!it.valPtr || count <= 0) return;
    isSubMenuOpen = true; subMenuSelectionIndex = *(int*)it.valPtr;
    if (subMenuSelectionIndex < 0 || subMenuSelectionIndex >= count) subMenuSelectionIndex = 0;
    renderMenu(client); return;
  }
  if ((it.type == TYPE_TEXT || isIntegerItem(it.type)) && it.valPtr) {
    isEditing = true;
    if (it.type == TYPE_TEXT) { inputBuffer = (char*)it.valPtr; strlcpy(originalTextValue, (char*)it.valPtr, sizeof(originalTextValue)); }
    else { originalIntValue = readIntegerValue(it); inputBuffer = String((long)originalIntValue); }
    renderMenu(client); return;
  }
  if (it.type == TYPE_ACTION && it.action) it.action();
}

bool GS32BIOS::inputLevelCheckEmpty(MenuItem* it, const String& buf)
{
  return buf.length() > 0 || it->allowEmpty;
}

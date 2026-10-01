// Erzeugt Excel-Dateien (.xlsx = ZIP mit XML, unkomprimiert) und CSV
// direkt auf dem ESP32. Der Inhalt wird zweimal deterministisch erzeugt:
// 1. Durchlauf berechnet CRC32 und Länge, 2. Durchlauf streamt an den Client.
#include "exporter.h"
#include <functional>
#include <vector>
#include "storage.h"
#include "timeutil.h"

namespace {

// ---------------------------------------------------------------- Ausgabe
class Out {
 public:
  virtual ~Out() {}
  virtual void put(const char *s, size_t n) = 0;
  void str(const char *s) { put(s, strlen(s)); }
  void fmt(const char *f, ...) __attribute__((format(printf, 2, 3))) {
    char b[160];
    va_list ap;
    va_start(ap, f);
    int n = vsnprintf(b, sizeof(b), f, ap);
    va_end(ap);
    if (n > 0) put(b, n < (int)sizeof(b) ? n : sizeof(b) - 1);
  }
};

uint32_t crcTable[256];
void crcInit() {
  if (crcTable[1]) return;
  for (uint32_t i = 0; i < 256; i++) {
    uint32_t c = i;
    for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    crcTable[i] = c;
  }
}

class CrcOut : public Out {
 public:
  uint32_t crc = 0xFFFFFFFF;
  uint32_t len = 0;
  void put(const char *s, size_t n) override {
    len += n;
    for (size_t i = 0; i < n; i++) crc = crcTable[(crc ^ (uint8_t)s[i]) & 0xFF] ^ (crc >> 8);
  }
  uint32_t value() const { return crc ^ 0xFFFFFFFF; }
};

class HttpOut : public Out {
 public:
  explicit HttpOut(WebServer &s) : srv(s) {}
  ~HttpOut() { flush(); }
  void put(const char *s, size_t n) override {
    while (n) {
      size_t c = min(n, sizeof(buf) - used);
      memcpy(buf + used, s, c);
      used += c; s += c; n -= c;
      if (used == sizeof(buf)) flush();
    }
  }
  void flush() {
    if (used) srv.sendContent(buf, used);
    used = 0;
  }

 private:
  WebServer &srv;
  char buf[1436];
  size_t used = 0;
};

// ------------------------------------------------------------ Kalender
int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

// Excel-Seriennummer (lokales Datum, ganzzahlig)
double excelDate(time_t t) {
  struct tm r = timeutil::local(t);
  return (double)(daysFromCivil(r.tm_year + 1900, r.tm_mon + 1, r.tm_mday) - daysFromCivil(1899, 12, 30));
}
double excelTimeOfDay(time_t t) {
  struct tm r = timeutil::local(t);
  return (r.tm_hour * 3600 + r.tm_min * 60 + r.tm_sec) / 86400.0;
}

// ------------------------------------------------------------ XLSX-Zellen
enum Style { S_TEXT = 0, S_BOLD, S_DATE, S_TIME, S_DUR, S_DEC, S_MONTH, S_BOLD_DUR, S_BOLD_DEC };

void cellRef(char *b, int col, int row) { snprintf(b, 12, "%c%d", 'A' + col, row); }

void cNum(Out &o, int col, int row, Style s, double v) {
  char r[12];
  cellRef(r, col, row);
  o.fmt("<c r=\"%s\" s=\"%d\"><v>%.10g</v></c>", r, (int)s, v);
}
void cStr(Out &o, int col, int row, Style s, const char *txt) {
  char r[12];
  cellRef(r, col, row);
  o.fmt("<c r=\"%s\" s=\"%d\" t=\"inlineStr\"><is><t>", r, (int)s);
  o.str(txt);  // nur feste Texte ohne XML-Sonderzeichen
  o.str("</t></is></c>");
}
void cSum(Out &o, int col, int row, Style s, int firstRow, int lastRow) {
  char r[12];
  cellRef(r, col, row);
  if (lastRow < firstRow) {
    o.fmt("<c r=\"%s\" s=\"%d\"><v>0</v></c>", r, (int)s);
  } else {
    o.fmt("<c r=\"%s\" s=\"%d\"><f>SUM(%c%d:%c%d)</f></c>", r, (int)s, 'A' + col, firstRow, 'A' + col, lastRow);
  }
}
void rowOpen(Out &o, int row) { o.fmt("<row r=\"%d\">", row); }
void rowClose(Out &o) { o.str("</row>"); }

void sheetOpen(Out &o, std::initializer_list<int> widths) {
  o.str("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<sheetViews><sheetView workbookViewId=\"0\"><pane ySplit=\"1\" topLeftCell=\"A2\" "
        "activePane=\"bottomLeft\" state=\"frozen\"/></sheetView></sheetViews><cols>");
  int i = 1;
  for (int w : widths) {
    o.fmt("<col min=\"%d\" max=\"%d\" width=\"%d\" customWidth=\"1\"/>", i, i, w);
    i++;
  }
  o.str("</cols><sheetData>");
}
void sheetClose(Out &o) { o.str("</sheetData></worksheet>"); }

void header(Out &o, std::initializer_list<const char *> titles) {
  rowOpen(o, 1);
  int c = 0;
  for (const char *t : titles) cStr(o, c++, 1, S_BOLD, t);
  rowClose(o);
}

struct Ctx {
  exporter::Range r;
  uint32_t now;
};

// Sitzungen (inkl. laufender) mit Start im Zeitraum
void forEachSession(const Ctx &c, std::function<void(uint32_t s, uint32_t e, bool running)> fn) {
  for (const auto &s : storage::sessions()) {
    if (s.start >= c.r.from && s.start < c.r.to) fn(s.start, s.end, false);
  }
  if (storage::running()) {
    uint32_t s = storage::runningSince();
    if (s >= c.r.from && s < c.r.to) fn(s, c.now, true);
  }
}

void sheetEntries(Out &o, const Ctx &c) {
  sheetOpen(o, {12, 6, 9, 9, 10, 10});
  header(o, {"Datum", "Tag", "Beginn", "Ende", "Dauer", "Stunden"});
  int row = 2;
  forEachSession(c, [&](uint32_t s, uint32_t e, bool running) {
    rowOpen(o, row);
    cNum(o, 0, row, S_DATE, excelDate(s));
    cStr(o, 1, row, S_TEXT, timeutil::weekdayShort(timeutil::local(s).tm_wday));
    cNum(o, 2, row, S_TIME, excelTimeOfDay(s));
    if (running) cStr(o, 3, row, S_TEXT, "l\xC3\xA4uft");
    else cNum(o, 3, row, S_TIME, excelTimeOfDay(e));
    cNum(o, 4, row, S_DUR, (e - s) / 86400.0);
    cNum(o, 5, row, S_DEC, (e - s) / 3600.0);
    rowClose(o);
    row++;
  });
  rowOpen(o, row);
  cStr(o, 0, row, S_BOLD, "Summe");
  cSum(o, 4, row, S_BOLD_DUR, 2, row - 1);
  cSum(o, 5, row, S_BOLD_DEC, 2, row - 1);
  rowClose(o);
  sheetClose(o);
}

void sheetDays(Out &o, const Ctx &c) {
  sheetOpen(o, {12, 6, 12, 10});
  header(o, {"Datum", "Tag", "Arbeitszeit", "Stunden"});
  int row = 2;
  for (time_t d = timeutil::dayStart(c.r.from); d < (time_t)c.r.to; d = timeutil::addDays(d, 1)) {
    time_t n = timeutil::addDays(d, 1);
    uint32_t w = storage::workedBetween(d, n, c.now);
    if (!w && !c.r.monthMode) continue;
    rowOpen(o, row);
    cNum(o, 0, row, S_DATE, excelDate(d));
    cStr(o, 1, row, S_TEXT, timeutil::weekdayShort(timeutil::local(d).tm_wday));
    cNum(o, 2, row, S_DUR, w / 86400.0);
    cNum(o, 3, row, S_DEC, w / 3600.0);
    rowClose(o);
    row++;
  }
  rowOpen(o, row);
  cStr(o, 0, row, S_BOLD, "Summe");
  cSum(o, 2, row, S_BOLD_DUR, 2, row - 1);
  cSum(o, 3, row, S_BOLD_DEC, 2, row - 1);
  rowClose(o);
  sheetClose(o);
}

int workDays(time_t from, time_t to, uint32_t now) {
  int n = 0;
  for (time_t d = from; d < to; d = timeutil::addDays(d, 1)) {
    if (storage::workedBetween(d, timeutil::addDays(d, 1), now)) n++;
  }
  return n;
}

void sheetWeeks(Out &o, const Ctx &c) {
  sheetOpen(o, {8, 6, 12, 12, 12, 10, 12});
  header(o, {"Jahr", "KW", "von", "bis", "Arbeitszeit", "Stunden", "Arbeitstage"});
  int row = 2;
  for (time_t w = timeutil::weekStart(c.r.from); w < (time_t)c.r.to; w = timeutil::addDays(w, 7)) {
    time_t n = timeutil::addDays(w, 7);
    uint32_t sec = storage::workedBetween(w, n, c.now);
    if (!sec && !c.r.monthMode) continue;
    int y;
    int kw = timeutil::isoWeek(w, &y);
    rowOpen(o, row);
    cNum(o, 0, row, S_TEXT, y);
    cNum(o, 1, row, S_TEXT, kw);
    cNum(o, 2, row, S_DATE, excelDate(w));
    cNum(o, 3, row, S_DATE, excelDate(timeutil::addDays(w, 6)));
    cNum(o, 4, row, S_DUR, sec / 86400.0);
    cNum(o, 5, row, S_DEC, sec / 3600.0);
    cNum(o, 6, row, S_TEXT, workDays(w, n, c.now));
    rowClose(o);
    row++;
  }
  sheetClose(o);
}

void sheetMonths(Out &o, const Ctx &c) {
  sheetOpen(o, {16, 12, 10, 12});
  header(o, {"Monat", "Arbeitszeit", "Stunden", "Arbeitstage"});
  int row = 2;
  for (time_t m = timeutil::monthStart(c.r.from); m < (time_t)c.r.to; m = timeutil::addMonths(m, 1)) {
    time_t n = timeutil::addMonths(m, 1);
    uint32_t sec = storage::workedBetween(m, n, c.now);
    rowOpen(o, row);
    cNum(o, 0, row, S_MONTH, excelDate(m));
    cNum(o, 1, row, S_DUR, sec / 86400.0);
    cNum(o, 2, row, S_DEC, sec / 3600.0);
    cNum(o, 3, row, S_TEXT, workDays(m, n, c.now));
    rowClose(o);
    row++;
  }
  rowOpen(o, row);
  cStr(o, 0, row, S_BOLD, "Summe");
  cSum(o, 1, row, S_BOLD_DUR, 2, row - 1);
  cSum(o, 2, row, S_BOLD_DEC, 2, row - 1);
  rowClose(o);
  sheetClose(o);
}

const char *kXmlHead = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";

void fileContentTypes(Out &o, const Ctx &) {
  o.str(kXmlHead);
  o.str("<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
        "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>");
  for (int i = 1; i <= 4; i++) {
    o.fmt("<Override PartName=\"/xl/worksheets/sheet%d.xml\" ContentType=\"application/"
          "vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>", i);
  }
  o.str("</Types>");
}

void fileRels(Out &o, const Ctx &) {
  o.str(kXmlHead);
  o.str("<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" "
        "Target=\"xl/workbook.xml\"/></Relationships>");
}

void fileWorkbook(Out &o, const Ctx &) {
  o.str(kXmlHead);
  o.str("<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
        "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets>"
        "<sheet name=\"Eintr\xC3\xA4ge\" sheetId=\"1\" r:id=\"rId1\"/>"
        "<sheet name=\"Tage\" sheetId=\"2\" r:id=\"rId2\"/>"
        "<sheet name=\"Wochen\" sheetId=\"3\" r:id=\"rId3\"/>"
        "<sheet name=\"Monate\" sheetId=\"4\" r:id=\"rId4\"/>"
        "</sheets><calcPr fullCalcOnLoad=\"1\"/></workbook>");
}

void fileWorkbookRels(Out &o, const Ctx &) {
  o.str(kXmlHead);
  o.str("<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">");
  for (int i = 1; i <= 4; i++) {
    o.fmt("<Relationship Id=\"rId%d\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
          "relationships/worksheet\" Target=\"worksheets/sheet%d.xml\"/>", i, i);
  }
  o.str("<Relationship Id=\"rId5\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
        "relationships/styles\" Target=\"styles.xml\"/></Relationships>");
}

void fileStyles(Out &o, const Ctx &) {
  o.str(kXmlHead);
  o.str("<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<numFmts count=\"4\">"
        "<numFmt numFmtId=\"164\" formatCode=\"dd.mm.yyyy\"/>"
        "<numFmt numFmtId=\"165\" formatCode=\"hh:mm\"/>"
        "<numFmt numFmtId=\"166\" formatCode=\"[h]:mm\"/>"
        "<numFmt numFmtId=\"167\" formatCode=\"mmmm yyyy\"/>"
        "</numFmts>"
        "<fonts count=\"2\"><font><sz val=\"11\"/><name val=\"Calibri\"/></font>"
        "<font><b/><sz val=\"11\"/><name val=\"Calibri\"/></font></fonts>"
        "<fills count=\"2\"><fill><patternFill patternType=\"none\"/></fill>"
        "<fill><patternFill patternType=\"gray125\"/></fill></fills>"
        "<borders count=\"1\"><border><left/><right/><top/><bottom/><diagonal/></border></borders>"
        "<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>"
        "<cellXfs count=\"9\">"
        "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>"
        "<xf numFmtId=\"0\" fontId=\"1\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyFont=\"1\"/>"
        "<xf numFmtId=\"164\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyNumberFormat=\"1\"/>"
        "<xf numFmtId=\"165\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyNumberFormat=\"1\"/>"
        "<xf numFmtId=\"166\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyNumberFormat=\"1\"/>"
        "<xf numFmtId=\"2\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyNumberFormat=\"1\"/>"
        "<xf numFmtId=\"167\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyNumberFormat=\"1\"/>"
        "<xf numFmtId=\"166\" fontId=\"1\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyNumberFormat=\"1\" applyFont=\"1\"/>"
        "<xf numFmtId=\"2\" fontId=\"1\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyNumberFormat=\"1\" applyFont=\"1\"/>"
        "</cellXfs>"
        "<cellStyles count=\"1\"><cellStyle name=\"Normal\" xfId=\"0\" builtinId=\"0\"/></cellStyles>"
        "</styleSheet>");
}

// ------------------------------------------------------------ ZIP
struct Entry {
  const char *name;
  void (*gen)(Out &, const Ctx &);
  uint32_t crc;
  uint32_t size;
};

void le16(Out &o, uint16_t v) {
  char b[2] = {(char)(v & 0xFF), (char)(v >> 8)};
  o.put(b, 2);
}
void le32(Out &o, uint32_t v) {
  char b[4] = {(char)(v & 0xFF), (char)((v >> 8) & 0xFF), (char)((v >> 16) & 0xFF), (char)(v >> 24)};
  o.put(b, 4);
}

void sendZip(WebServer &srv, std::vector<Entry> &entries, const Ctx &c, const String &filename,
             const char *mime) {
  crcInit();
  uint32_t total = 22;
  for (auto &e : entries) {
    CrcOut co;
    e.gen(co, c);
    e.crc = co.value();
    e.size = co.len;
    total += 30 + strlen(e.name) + e.size + 46 + strlen(e.name);
  }
  struct tm lt = timeutil::local(c.now);
  uint16_t dosTime = (lt.tm_hour << 11) | (lt.tm_min << 5) | (lt.tm_sec / 2);
  uint16_t dosDate = ((lt.tm_year - 80) << 9) | ((lt.tm_mon + 1) << 5) | lt.tm_mday;

  srv.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
  srv.sendHeader("Cache-Control", "no-store");
  srv.setContentLength(total);
  srv.send(200, mime, "");

  HttpOut o(srv);
  std::vector<uint32_t> offsets;
  uint32_t off = 0;
  for (auto &e : entries) {
    offsets.push_back(off);
    uint16_t nl = strlen(e.name);
    le32(o, 0x04034b50); le16(o, 20); le16(o, 0); le16(o, 0);
    le16(o, dosTime); le16(o, dosDate);
    le32(o, e.crc); le32(o, e.size); le32(o, e.size);
    le16(o, nl); le16(o, 0);
    o.put(e.name, nl);
    e.gen(o, c);
    off += 30 + nl + e.size;
  }
  uint32_t cdStart = off, cdSize = 0;
  for (size_t i = 0; i < entries.size(); i++) {
    auto &e = entries[i];
    uint16_t nl = strlen(e.name);
    le32(o, 0x02014b50); le16(o, 20); le16(o, 20); le16(o, 0); le16(o, 0);
    le16(o, dosTime); le16(o, dosDate);
    le32(o, e.crc); le32(o, e.size); le32(o, e.size);
    le16(o, nl); le16(o, 0); le16(o, 0); le16(o, 0); le16(o, 0); le32(o, 0);
    le32(o, offsets[i]);
    o.put(e.name, nl);
    cdSize += 46 + nl;
  }
  le32(o, 0x06054b50); le16(o, 0); le16(o, 0);
  le16(o, entries.size()); le16(o, entries.size());
  le32(o, cdSize); le32(o, cdStart); le16(o, 0);
  o.flush();
}

// ------------------------------------------------------------ CSV
void csvBody(Out &o, const Ctx &c) {
  o.str("\xEF\xBB\xBF" "Datum;Tag;Beginn;Ende;Dauer (h:mm);Stunden\r\n");
  uint32_t sum = 0;
  forEachSession(c, [&](uint32_t s, uint32_t e, bool running) {
    uint32_t d = e - s;
    sum += d;
    o.fmt("%s;%s;%s;%s;%s;%lu,%02lu\r\n", timeutil::fmtDate(s).c_str(),
          timeutil::weekdayShort(timeutil::local(s).tm_wday), timeutil::fmtClock(s).c_str(),
          running ? "l\xC3\xA4uft" : timeutil::fmtClock(e).c_str(), timeutil::fmtDuration(d).c_str(),
          (unsigned long)(d / 3600), (unsigned long)((d % 3600) * 100 / 3600));
  });
  o.fmt("Summe;;;;%s;%lu,%02lu\r\n", timeutil::fmtDuration(sum).c_str(), (unsigned long)(sum / 3600),
        (unsigned long)((sum % 3600) * 100 / 3600));
}

}  // namespace

namespace exporter {

Range rangeForMonth(int year, int month) {
  Range r;
  r.from = timeutil::monthStart(year, month);
  r.to = timeutil::addMonths(r.from, 1);
  r.monthMode = true;
  char b[12];
  snprintf(b, sizeof(b), "%04d-%02d", year, month);
  r.fileLabel = b;
  return r;
}

Range rangeAll(uint32_t now) {
  Range r;
  uint32_t first = now;
  if (!storage::sessions().empty()) first = storage::sessions().front().start;
  if (storage::running() && storage::runningSince() < first) first = storage::runningSince();
  r.from = timeutil::dayStart(first);
  r.to = timeutil::addDays(timeutil::dayStart(now), 1);
  r.monthMode = false;
  r.fileLabel = "gesamt";
  return r;
}

void sendXlsx(WebServer &srv, const Range &r, uint32_t now) {
  Ctx c{r, now};
  std::vector<Entry> entries = {
      {"[Content_Types].xml", fileContentTypes, 0, 0},
      {"_rels/.rels", fileRels, 0, 0},
      {"xl/workbook.xml", fileWorkbook, 0, 0},
      {"xl/_rels/workbook.xml.rels", fileWorkbookRels, 0, 0},
      {"xl/styles.xml", fileStyles, 0, 0},
      {"xl/worksheets/sheet1.xml", sheetEntries, 0, 0},
      {"xl/worksheets/sheet2.xml", sheetDays, 0, 0},
      {"xl/worksheets/sheet3.xml", sheetWeeks, 0, 0},
      {"xl/worksheets/sheet4.xml", sheetMonths, 0, 0},
  };
  sendZip(srv, entries, c, "Arbeitszeit_" + r.fileLabel + ".xlsx",
          "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet");
}

void sendCsv(WebServer &srv, const Range &r, uint32_t now) {
  Ctx c{r, now};
  crcInit();
  CrcOut counter;
  csvBody(counter, c);
  srv.sendHeader("Content-Disposition", "attachment; filename=\"Arbeitszeit_" + r.fileLabel + ".csv\"");
  srv.sendHeader("Cache-Control", "no-store");
  srv.setContentLength(counter.len);
  srv.send(200, "text/csv; charset=utf-8", "");
  HttpOut o(srv);
  csvBody(o, c);
  o.flush();
}

}  // namespace exporter

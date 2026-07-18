/*
 * --------------------------------------------------------------------------
 *  Rogue Mails - Simple Email Client For The Cardputer ADV
 *
 *  ETHICAL HACKERS DEN - SECURITY TOOLS SUITE
 *  Website: www.ethicalhackersden.org
 *  GitHub:  https://github.com/M5RogueOps
 *
 *  DISCLAIMER: This software is licensed under the MIT License. 
 *  Under the terms of this license, these credit links must remain 
 *  included within the source code.
 * --------------------------------------------------------------------------
 */
#include <M5Cardputer.h>
#include <WiFi.h>
#include <ESP_Mail_Client.h>
#include <Preferences.h>
#include <mbedtls/aes.h>
#include <mbedtls/base64.h>

// --- UI COLOR PALETTE ---
#define BG_COLOR      0xFFFF // White
#define TEXT_MAIN     0x0000 // Black
#define TEXT_MUTED    0x7BEF // Grey
#define HEADER_BG     0xD800 // Red Header
#define HEADER_TEXT   0xFFFF // White
#define ACCENT_COLOR  0x03EF // Blue for active field
#define DIVIDER_COLOR 0xE71C // Light Grey

// --- AES ENCRYPTION KEY (Keep secret!) ---
const unsigned char AES_KEY[16] = { 
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C 
};

SMTPSession smtp;
IMAPSession imap;
M5Canvas canvas(&M5Cardputer.Display);
Preferences prefs;

// --- APP STATE SYSTEM ---
enum AppState { STATE_COMPOSE, STATE_INBOX, STATE_SETTINGS };
enum ComposeField { FIELD_TO, FIELD_SUB, FIELD_BODY };
enum SettingsField { SET_SSID, SET_PASS, SET_NAME, SET_EMAIL, SET_EMAIL_PASS, SET_SMTP, SET_PORT, SET_IMAP, SET_IMAP_PORT };
enum MailProvider { PROV_GMAIL, PROV_OUTLOOK, PROV_YAHOO, PROV_CUSTOM };

AppState currentState = STATE_SETTINGS;
ComposeField currentComposeField = FIELD_TO;
SettingsField currentSettingsField = SET_SSID;
MailProvider currentProvider = PROV_GMAIL;

// --- DYNAMIC BUFFERS ---
String strSSID = "";
String strPass = "";
String strSenderName = "Cardputer User";
String strSenderEmail = "";
String strSenderPass = "";
String strSMTPServer = "";
String strSMTPPort = "";
String strIMAPServer = "";
String strIMAPPort = "";

String strTo = "";
String strSub = "";
String strBody = "";
String statusMsg = "";
bool sending = false;
bool isConnected = false;

// --- INBOX BUFFERS (10 Emails) ---
String inboxSender[10];
String inboxSubject[10];
String inboxBody[10];
int inboxCount = 0;
int selectedMsg = 0;
int scrollOffset = 0;

// --- FORWARD DECLARATIONS (Fixes 'not declared in this scope' errors) ---
void drawUI();
void drawComposeView();
void drawInboxView();
void drawSettingsView();
void checkInbox();
void sendEmail();
void connectNetwork();
void saveSettings();
void loadSettings();
void applyProviderPreset(MailProvider prov);
String encryptData(String plainText);
String decryptData(String cipherText);

// --- CRYPTO HELPERS ---
String encryptData(String plainText) {
    if (plainText.length() == 0) return "";
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, AES_KEY, 128);
    
    int paddedLen = ((plainText.length() / 16) + 1) * 16;
    unsigned char input[paddedLen]; unsigned char output[paddedLen];
    memset(input, 0, paddedLen);
    memcpy(input, plainText.c_str(), plainText.length());
    
    for (int i = 0; i < paddedLen; i += 16) {
        mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, input + i, output + i);
    }
    mbedtls_aes_free(&aes);
    
    unsigned char base64[paddedLen * 2]; size_t base64Len = 0;
    mbedtls_base64_encode(base64, sizeof(base64), &base64Len, output, paddedLen);
    return String((char*)base64);
}

String decryptData(String cipherText) {
    if (cipherText.length() == 0) return "";
    unsigned char decoded[cipherText.length()]; size_t decodedLen = 0;
    mbedtls_base64_decode(decoded, sizeof(decoded), &decodedLen, (const unsigned char*)cipherText.c_str(), cipherText.length());
    
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, AES_KEY, 128);
    
    unsigned char output[decodedLen]; memset(output, 0, decodedLen);
    for (size_t i = 0; i < decodedLen; i += 16) {
        mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, decoded + i, output + i);
    }
    mbedtls_aes_free(&aes);
    return String((char*)output);
}

void applyProviderPreset(MailProvider prov) {
    currentProvider = prov;
    if (prov == PROV_GMAIL) {
        strSMTPServer = "smtp.gmail.com"; strSMTPPort = "465";
        strIMAPServer = "imap.gmail.com"; strIMAPPort = "993";
        statusMsg = "Preset: Gmail";
    } else if (prov == PROV_OUTLOOK) {
        strSMTPServer = "smtp.office365.com"; strSMTPPort = "587";
        strIMAPServer = "outlook.office365.com"; strIMAPPort = "993";
        statusMsg = "Preset: Outlook";
    } else if (prov == PROV_YAHOO) {
        strSMTPServer = "smtp.mail.yahoo.com"; strSMTPPort = "465";
        strIMAPServer = "imap.mail.yahoo.com"; strIMAPPort = "993";
        statusMsg = "Preset: Yahoo";
    } else {
        strSMTPServer = ""; strSMTPPort = "";
        strIMAPServer = ""; strIMAPPort = "";
        statusMsg = "Preset: Custom";
    }
}

void loadSettings() {
    prefs.begin("mail_settings", false);
    strSSID = prefs.getString("ssid", "");
    strPass = decryptData(prefs.getString("pass", ""));
    strSenderName = prefs.getString("name", "Cardputer User");
    strSenderEmail = prefs.getString("email", "");
    strSenderPass = decryptData(prefs.getString("email_pass", ""));
    strSMTPServer = prefs.getString("smtp_srv", "");
    strSMTPPort = prefs.getString("smtp_port", "");
    strIMAPServer = prefs.getString("imap_srv", "");
    strIMAPPort = prefs.getString("imap_port", "");
    prefs.end();

    if (strSMTPServer == "") applyProviderPreset(PROV_GMAIL);
    else if (strSSID.length() > 0 && strSenderEmail.length() > 0) currentState = STATE_COMPOSE;
}

void saveSettings() {
    prefs.begin("mail_settings", false);
    prefs.putString("ssid", strSSID);
    prefs.putString("pass", encryptData(strPass));
    prefs.putString("name", strSenderName);
    prefs.putString("email", strSenderEmail);
    prefs.putString("email_pass", encryptData(strSenderPass));
    prefs.putString("smtp_srv", strSMTPServer);
    prefs.putString("smtp_port", strSMTPPort);
    prefs.putString("imap_srv", strIMAPServer);
    prefs.putString("imap_port", strIMAPPort);
    prefs.end();
    statusMsg = "Saved!";
}

void connectNetwork() {
    if (strSSID.length() == 0) return;
    statusMsg = "Connecting..."; drawUI();
    WiFi.disconnect(true);
    WiFi.begin(strSSID.c_str(), strPass.c_str());
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) { delay(500); retries++; }
    if (WiFi.status() == WL_CONNECTED) {
        isConnected = true; 
        configTime(0, 0, "pool.ntp.org");
        statusMsg = "Connected!";
    } else {
        isConnected = false; statusMsg = "Wi-Fi Fail!";
    }
}

void sendEmail() {
    if (!isConnected) { statusMsg = "No Wi-Fi!"; return; }
    sending = true; statusMsg = "Sending..."; drawUI();

    ESP_Mail_Session session;
    session.server.host_name = strSMTPServer.c_str();
    session.server.port = strSMTPPort.toInt();
    session.login.email = strSenderEmail.c_str();
    session.login.password = strSenderPass.c_str();

    SMTP_Message message;
    message.sender.name = strSenderName.c_str();
    message.sender.email = strSenderEmail.c_str();
    message.subject = strSub.c_str();
    message.addRecipient("Recipient", strTo.c_str());
    message.text.content = strBody.c_str();

    if (!smtp.connect(&session)) { 
        statusMsg = "SMTP: " + smtp.errorReason().substring(0, 14); 
        sending = false; 
        return; 
    }
    if (!MailClient.sendMail(&smtp, &message)) {
        statusMsg = "Fail: " + smtp.errorReason().substring(0, 14);
    } else { 
        statusMsg = "Sent!"; strTo = ""; strSub = ""; strBody = ""; currentComposeField = FIELD_TO; 
    }
    sending = false;
}

// --- OPTIMIZED INBOX (10 Messages with Safe Type Checking) ---
void checkInbox() {
    if (!isConnected) { statusMsg = "No Wi-Fi!"; return; }
    statusMsg = "Connecting IMAP..."; drawUI();

    ESP_Mail_Session session;
    session.server.host_name = strIMAPServer.c_str();
    session.server.port = strIMAPPort.toInt();
    session.login.email = strSenderEmail.c_str();
    session.login.password = strSenderPass.c_str();

    IMAP_Config config;
    config.enable.text = true;
    config.enable.html = true;
    config.enable.recent_sort = true;

    if (!imap.connect(&session, &config)) { 
        statusMsg = "Err: " + imap.errorReason().substring(0, 14); 
        drawUI(); 
        return; 
    }
    
    statusMsg = "Opening INBOX..."; drawUI();
    if (!imap.selectFolder("INBOX")) { 
        statusMsg = "Err: No INBOX!"; 
        drawUI(); 
        return; 
    }

    statusMsg = "Downloading..."; drawUI();
    if (!MailClient.readMail(&imap)) {
        statusMsg = "Read Err: " + imap.errorReason().substring(0, 10);
        drawUI();
        return;
    }

    inboxCount = 0;
    auto msgList = imap.data();
    for (size_t i = 0; i < msgList.msgItems.size() && i < 10; i++) {
        auto msg = msgList.msgItems[i];
        inboxSender[i]  = msg.from;
        inboxSubject[i] = msg.subject;
        
        // Convert const char* to Arduino String before checking length
        String textContent = msg.text.content;
        String htmlContent = msg.html.content;

        if (textContent.length() > 0) {
            inboxBody[i] = textContent;
        } else if (htmlContent.length() > 0) {
            inboxBody[i] = htmlContent;
        } else {
            inboxBody[i] = "[Empty or Attachment Only]";
        }
        inboxCount++;
    }
    statusMsg = String(inboxCount) + " Mails Loaded!";
    selectedMsg = 0; scrollOffset = 0;
}

// --- INPUT HANDLERS ---
void handleComposeInput(Keyboard_Class::KeysState status) {
    String *targetStr = &strTo;
    if (currentComposeField == FIELD_SUB) targetStr = &strSub;
    if (currentComposeField == FIELD_BODY) targetStr = &strBody;

    if (status.del && targetStr->length() > 0) targetStr->remove(targetStr->length() - 1);
    if (status.tab || (status.enter && currentComposeField != FIELD_BODY)) {
        if (currentComposeField == FIELD_TO) currentComposeField = FIELD_SUB;
        else if (currentComposeField == FIELD_SUB) currentComposeField = FIELD_BODY;
        else currentComposeField = FIELD_TO;
        return;
    }
    if (status.enter && currentComposeField == FIELD_BODY && status.fn) { sendEmail(); return; }
    else if (status.enter && currentComposeField == FIELD_BODY) { *targetStr += "\n"; return; }
    for (auto i : status.word) *targetStr += i;
    if (status.space) *targetStr += " ";
}

void handleInboxInput(Keyboard_Class::KeysState status) {
    if (status.fn && status.word.size() > 0 && status.word[0] == 'r') { checkInbox(); return; }
    if (status.tab) { 
        if (inboxCount > 0) {
            selectedMsg = (selectedMsg + 1) % inboxCount; 
            scrollOffset = 0;
        }
        return; 
    }
    for (auto i : status.word) {
        if (i == ';' && scrollOffset > 0) scrollOffset -= 12;      // Scroll Up
        if (i == '.' && scrollOffset < 500) scrollOffset += 12;    // Scroll Down
    }
}

void handleSettingsInput(Keyboard_Class::KeysState status) {
    String *targetStr = &strSSID;
    if (currentSettingsField == SET_PASS) targetStr = &strPass;
    if (currentSettingsField == SET_NAME) targetStr = &strSenderName;
    if (currentSettingsField == SET_EMAIL) targetStr = &strSenderEmail;
    if (currentSettingsField == SET_EMAIL_PASS) targetStr = &strSenderPass;
    if (currentSettingsField == SET_SMTP) targetStr = &strSMTPServer;
    if (currentSettingsField == SET_PORT) targetStr = &strSMTPPort;
    if (currentSettingsField == SET_IMAP) targetStr = &strIMAPServer;
    if (currentSettingsField == SET_IMAP_PORT) targetStr = &strIMAPPort;

    if (status.del && targetStr->length() > 0) targetStr->remove(targetStr->length() - 1);
    if (status.tab || status.enter) {
        currentSettingsField = (SettingsField)((currentSettingsField + 1) % 9);
        return;
    }
    for (auto i : status.word) *targetStr += i;
    if (status.space) *targetStr += " ";
}

// --- UI RENDERING ---
void drawComposeView() {
    canvas.fillRect(0, 0, 240, 20, HEADER_BG);
    canvas.setTextColor(HEADER_TEXT); canvas.drawString("Mail - Compose", 6, 4);
    canvas.drawRightString(statusMsg, 234, 4);

    int y = 24;
    canvas.setTextColor(currentComposeField == FIELD_TO ? ACCENT_COLOR : TEXT_MUTED); canvas.drawString("To: ", 4, y);
    canvas.setTextColor(TEXT_MAIN); canvas.drawString(strTo + (currentComposeField == FIELD_TO ? "_" : ""), 28, y);
    canvas.drawLine(0, y + 12, 240, y + 12, DIVIDER_COLOR);

    y += 16;
    canvas.setTextColor(currentComposeField == FIELD_SUB ? ACCENT_COLOR : TEXT_MUTED); canvas.drawString("Sub: ", 4, y);
    canvas.setTextColor(TEXT_MAIN); canvas.drawString(strSub + (currentComposeField == FIELD_SUB ? "_" : ""), 32, y);
    canvas.drawLine(0, y + 12, 240, y + 12, DIVIDER_COLOR);

    y += 16; canvas.setTextColor(TEXT_MAIN);
    String displayBody = strBody + (currentComposeField == FIELD_BODY ? "_" : "");
    int lineOffset = 0;
    while (displayBody.length() > 0 && y + lineOffset < 120) {
        int cut = displayBody.length() > 38 ? 38 : displayBody.length();
        canvas.drawString(displayBody.substring(0, cut), 4, y + lineOffset);
        displayBody = displayBody.substring(cut); lineOffset += 12;
    }
    canvas.fillRect(0, 124, 240, 11, DIVIDER_COLOR);
    canvas.setTextColor(TEXT_MAIN); canvas.drawString("TAB:Field | OPT:View | FN+ENT:Send", 4, 125);
}

void drawInboxView() {
    canvas.fillRect(0, 0, 240, 20, ACCENT_COLOR);
    canvas.setTextColor(HEADER_TEXT); 
    
    String headerText = "Inbox [" + String(inboxCount > 0 ? selectedMsg + 1 : 0) + "/" + String(inboxCount) + "]";
    canvas.drawString(headerText, 6, 4);
    canvas.drawRightString(statusMsg, 234, 4);

    if (inboxCount == 0) {
        canvas.setTextColor(TEXT_MUTED); canvas.drawString("No messages. Press FN+R to fetch.", 10, 50);
    } else {
        canvas.setTextColor(ACCENT_COLOR); canvas.drawString("From: ", 4, 24);
        canvas.setTextColor(TEXT_MAIN); canvas.drawString(inboxSender[selectedMsg].substring(0, 32), 36, 24);
        canvas.setTextColor(ACCENT_COLOR); canvas.drawString("Sub: ", 4, 38);
        canvas.setTextColor(TEXT_MAIN); canvas.drawString(inboxSubject[selectedMsg].substring(0, 32), 30, 38);
        canvas.drawLine(0, 52, 240, 52, DIVIDER_COLOR);

        int y = 56 - scrollOffset;
        String displayBody = inboxBody[selectedMsg];
        int lineOffset = 0;
        while (displayBody.length() > 0 && y + lineOffset < 350) {
            int cut = displayBody.length() > 38 ? 38 : displayBody.length();
            if (y + lineOffset >= 54 && y + lineOffset < 120) {
                canvas.drawString(displayBody.substring(0, cut), 4, y + lineOffset);
            }
            displayBody = displayBody.substring(cut); lineOffset += 12;
        }
    }
    canvas.fillRect(0, 124, 240, 11, DIVIDER_COLOR);
    canvas.setTextColor(TEXT_MAIN); canvas.drawString("TAB:Next | FN+R:Refresh | OPT:View", 4, 125);
}

void drawSettingsView() {
    canvas.fillRect(0, 0, 240, 20, TEXT_MUTED);
    canvas.setTextColor(HEADER_TEXT); canvas.drawString("Configuration", 6, 4);
    canvas.drawRightString(statusMsg, 234, 4);

    struct FieldDef { String label; String val; bool isPass; bool active; };
    FieldDef fields[] = {
        {"WiFi SSID:", strSSID, false, currentSettingsField == SET_SSID},
        {"WiFi Pass:", strPass, true, currentSettingsField == SET_PASS},
        {"My Name  :", strSenderName, false, currentSettingsField == SET_NAME},
        {"My Email :", strSenderEmail, false, currentSettingsField == SET_EMAIL},
        {"App Pass :", strSenderPass, true, currentSettingsField == SET_EMAIL_PASS},
        {"SMTP Srv :", strSMTPServer, false, currentSettingsField == SET_SMTP},
        {"SMTP Port:", strSMTPPort, false, currentSettingsField == SET_PORT},
        {"IMAP Srv :", strIMAPServer, false, currentSettingsField == SET_IMAP},
        {"IMAP Port:", strIMAPPort, false, currentSettingsField == SET_IMAP_PORT}
    };

    int viewOffset = 0;
    if (currentSettingsField > 4) viewOffset = (currentSettingsField - 4) * 14;

    int y = 22 - viewOffset;
    for (int i = 0; i < 9; i++) {
        if (y >= 20 && y < 115) {
            canvas.setTextColor(fields[i].active ? ACCENT_COLOR : TEXT_MAIN);
            canvas.drawString(fields[i].label, 4, y);
            String displayVal = fields[i].val;
            if (fields[i].isPass) {
                displayVal = "";
                for (size_t j = 0; j < fields[i].val.length(); j++) displayVal += "*";
            }
            if (fields[i].active) displayVal += "_";
            canvas.setTextColor(TEXT_MAIN); canvas.drawString(displayVal, 75, y);
        }
        y += 14;
    }
    canvas.fillRect(0, 124, 240, 11, DIVIDER_COLOR);
    canvas.setTextColor(TEXT_MAIN); canvas.drawString("TAB:Next | FN+P:Preset | FN+S:Save", 4, 125);
}

void drawUI() {
    canvas.fillSprite(BG_COLOR);
    if (currentState == STATE_COMPOSE) drawComposeView();
    else if (currentState == STATE_INBOX) drawInboxView();
    else drawSettingsView();
    canvas.pushSprite(0, 0);
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    canvas.createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());
    
    loadSettings();
    connectNetwork();
    drawUI();
}

void loop() {
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
        
        if (status.opt) {
            currentState = (AppState)((currentState + 1) % 3);
            statusMsg = ""; drawUI(); return;
        }
        if (currentState == STATE_SETTINGS && status.fn && status.word.size() > 0 && status.word[0] == 's') {
            saveSettings(); connectNetwork(); drawUI(); return;
        }
        if (currentState == STATE_SETTINGS && status.fn && status.word.size() > 0 && status.word[0] == 'p') {
            applyProviderPreset((MailProvider)((currentProvider + 1) % 4)); drawUI(); return;
        }

        if (currentState == STATE_COMPOSE) handleComposeInput(status);
        else if (currentState == STATE_INBOX) handleInboxInput(status);
        else handleSettingsInput(status);
        
        drawUI();
    }
    delay(10);
}

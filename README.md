# 📬 Cardputer ADV Secure Mail Terminal

![Platform: M5Cardputer](https://img.shields.io/badge/Platform-M5Cardputer%20ADV-d80000?style=for-the-badge&logo=espressif)
![Framework: Arduino](https://img.shields.io/badge/Framework-Arduino%20IDE-00979D?style=for-the-badge&logo=arduino&logoColor=white)
![Security: AES--128](https://img.shields.io/badge/Security-AES--128%20Encrypted-4c1?style=for-the-badge)

A minimalist, hardware-encrypted email client and terminal designed specifically for the **M5Stack Cardputer ADV (ESP32-S3)**. This firmware transforms your handheld portable terminal into a standalone, secure communicator capable of sending and receiving emails over SSL/TLS without relying on a smartphone or cloud proxy.

---

## ✨ Key Features

* **🔒 Application-Level AES-128 Encryption:** Your Wi-Fi passwords and Email App Passwords are never stored in plain text. They are encrypted using an onboard AES-128 block cipher before being written to the ESP32's Non-Volatile Storage (NVS). Even if the flash memory is physically dumped, your credentials remain secure.
* **📬 Bidirectional Communication (SMTP & IMAP):** * **Compose & Send:** Draft multi-line emails and transmit them securely over SSL/TLS (Port 465/587).
  * **Inbox Reader:** Fetch, read, and scroll through your top 10 most recent emails directly on the screen.
* **⚡ Instant Provider Presets:** Switch between **Gmail**, **Outlook / Office365**, **Yahoo Mail**, and **Custom Server** configurations with a single keystroke (`FN + P`).
* **🎨 Flicker-Free UI:** Built using `M5Canvas` memory sprites for clean, instant frame pushes with zero screen tearing. Features a Gmail-inspired color palette with active visual field indicators.
* **🔄 Multi-Firmware Compatible:** Because it uses application-level encryption rather than system-wide eFuse flash locking, this firmware is 100% compatible with bootloaders like **M5Launcher**.

---

## 🛠️ Hardware Requirements

* **M5Stack Cardputer ADV** (ESP32-S3 Microcontroller with integrated mechanical keyboard and ST7789 display).
* Wi-Fi connection with Internet access (Note: Ensure your network does not block outgoing Port `465`/`587` or incoming Port `993`).

---

## 📦 Installation & Setup

### 1. Install Required Libraries
Open the Arduino IDE, go to **Tools > Manage Libraries...**, and search for/install the following:
* `M5Cardputer` by M5Stack
* `M5Unified` by M5Stack
* `ESP Mail Client` by Mobizt *(Ensure you install all dependencies when prompted)*

### 2. Configure Arduino IDE Board Settings
To avoid `Sketch too big` compilation errors caused by the heavy SSL/TLS and cryptography libraries, you must allocate sufficient Flash memory:
1. Select Board: **Tools > Board > esp32 > M5Cardputer** *(or ESP32S3 Dev Module)*.
2. Select Partition Scheme: **Tools > Partition Scheme > "8M with spiffs (3MB APP/1.5MB SPIFFS)"** *(or any scheme that provides at least **3MB APP** space)*.

### 3. Customize Your Private AES Key (IMPORTANT)
Before uploading the sketch, open `sketch.ino` and locate the encryption key array around Line 15. Change these 16 hexadecimal values to your own secret key:
```cpp
// Change these 16 hex bytes to your own private random sequence!
const unsigned char AES_KEY[16] = { 
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C 
};

```

### 4. Compile & Upload

Connect your Cardputer via USB-C and click **Upload**.

---

## 🚀 First-Time Onboarding Guide

Because modern email providers block standard passwords for third-party apps, **you must use an App Password**.

### Step A: Get Your Gmail App Password

1. Enable **2-Step Verification** on your Google Account.
2. Go to your [Google App Passwords Page](https://myaccount.google.com/apppasswords).
3. Generate a new password for "Mail / Custom Device".
4. Copy the 16-character code *(remove any spaces)*.

### Step B: Configure the Cardputer

1. Boot up the device. It will start in the **Configuration (Settings)** view.
2. Press **`FN + P`** until your provider preset appears in the top right corner (e.g., `Preset: Gmail`). This automatically configures your SMTP and IMAP servers and ports.
3. Use **`TAB`** to cycle through the input fields and enter:
* **WiFi SSID** & **WiFi Pass**
* **My Name:** *(Your display name, e.g., Randell)*
* **My Email:** *(Your full email address)*
* **App Pass:** *(The 16-character code generated in Step A)*


4. Press **`FN + S`** to encrypt your credentials, save them to memory, sync the NTP atomic clock, and connect to Wi-Fi. Notice the status change to **`Connected!`**.

---

## ⌨️ Keyboard Shortcuts Reference

The interface is entirely keyboard-driven. Use these shortcuts to navigate the terminal efficiently:

### 🌐 Global Controls (All Screens)

| Shortcut | Action | Description |
| --- | --- | --- |
| **`OPT`** | **Cycle Views** | Seamlessly switch between **Compose** $\rightarrow$ **Inbox** $\rightarrow$ **Settings**. |
| **`DEL`** | **Backspace** | Delete the character to the left of the cursor in the active field. |

### ⚙️ Settings View (`Configuration`)

| Shortcut | Action | Description |
| --- | --- | --- |
| **`TAB`** | **Next Field** | Move typing focus down to the next configuration option. |
| **`FN + P`** | **Provider Preset** | Cycle auto-fill configurations: **Gmail** $\rightarrow$ **Outlook** $\rightarrow$ **Yahoo** $\rightarrow$ **Custom**. |
| **`FN + S`** | **Save & Connect** | Encrypt passwords, write to NVS, sync time via NTP, and connect to Wi-Fi. |

### ✉️ Compose View (`Draft & Send`)

| Shortcut | Action | Description |
| --- | --- | --- |
| **`TAB`** | **Switch Field** | Cycle focus between **To:**, **Sub:** (Subject), and **Body**. |
| **`ENTER`** | **Next Field / New Line** | Move to the next field (in To/Sub) or insert a line break (in Body). |
| **`FN + ENTER`** | **SEND EMAIL** | Transmit message over SSL/TLS and clear draft buffer upon success. |

### 📥 Inbox View (`Read & Scroll`)

| Shortcut | Action | Description |
| --- | --- | --- |
| **`FN + R`** | **Fetch Mail** | Connect to IMAP server and download the 10 latest messages. |
| **`TAB`** | **Next Message** | Cycle through loaded emails (`[1/10]` through `[10/10]`). |
| **`;`** *(Semicolon)* | **Scroll Up** | Scroll the message body text up by one line. |
| **`.`** *(Period)* | **Scroll Down** | Scroll the message body text down by one line. |

---

## 🔧 Troubleshooting

* **`IMAP Fail!` or `Err: LOGIN failed`:**
* Ensure you are using a **16-character App Password**, not your regular login password.
* Check that IMAP is enabled in your webmail settings (In Gmail: *Settings > Forwarding and POP/IMAP > Enable IMAP*).


* **`Err: Certificate` or Connection Rejections:**
* SSL/TLS requires an accurate system clock. When you press `FN + S`, give the device 3–5 seconds to synchronize with `pool.ntp.org` before attempting to fetch or send email.


* **Can send email, but Inbox refresh hangs:**
* Some strict corporate, university, or public Wi-Fi networks block incoming Port `993`. Try connecting to a personal mobile hotspot or home Wi-Fi network.



---

## 📄 License

This project is open-source and available under the [MIT License](https://www.google.com/search?q=LICENSE).

*Disclaimer: This tool is intended for educational and personal communication purposes. Always ensure your AES keys are kept private and never commit your unencrypted Wi-Fi or email passwords to public repositories.*

```


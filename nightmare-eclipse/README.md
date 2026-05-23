# Nightmare-Eclipse Arsenal

**GitHub account:** @Nightmare-Eclipse (4.7k followers, "Microsoft's nightmare", Redmond WA)
**Status:** Account taken down from GitHub (likely DMCA by Microsoft ~2024-2025). WarCDev mirror org also taken down. Forks exist transiently but get removed quickly.

**CVEs assigned:**
- BlueHammer: CVE-2026-33825
- YellowKey: CVE-2026-45585
- MiniPlasma: (CVE-2020-17103, originally from Google Project Zero, found still unpatched)
- GreenPlasma: No CVE (incomplete PoC)
- RedSun: No CVE assigned
- UnDefend: No CVE (DOS tool)

**Active exploitation confirmed** by Huntress, Barracuda, Vectra - these PoCs are being used in the wild.

All 6 repos targeted Windows security mechanisms. Every single one was a production-grade PoC, not a CTF toy.

---

## 1. YellowKey (3,899 ★) - BitLocker Bypass
**Language:** Shell/USB  
**Target:** Windows 11 / Server 2022/2025 BitLocker  
**Status:** ❌ Repo gone

Filesystem trick via WinRE. Copy a FsTx folder to USB, boot to recovery, hold CTRL during restart → unrestricted shell on BitLocker-protected volume.

Author's note: "Almost feels like a backdoor. The component responsible is in WinRE image only, not in normal Windows. Why?"

**Thanks to MORSE, MSTIC, Microsoft GHOST for disclosure.**

---

## 2. RedSun (2,222 ★) - Defender Self-Destruct LPE  
**Language:** C++  
**Target:** Windows Defender  
**Status:** ❌ Repo gone

Abuses Defender's cloud-tag behavior: When Defender finds a malicious file with a cloud tag, it **rewrites** the file to its original location. The PoC exploits this to overwrite SYSTEM-protected files → gain admin privileges.

Author's words: "I think antimalware products are supposed to remove malicious files, not be sure they are there."

---

## 3. BlueHammer (2,133 ★) - Unknown Vulnerability  
**Language:** C  
**Target:** Unknown  
**Status:** ❌ Repo gone

README was a PGP-signed message only:
"I'm just really wondering what was the math behind their decision, like you knew this was going to happen and you still did whatever you did? Are they serious?"

Never publicly disclosed what the vulnerability actually was. PGP-signed for authenticity.

---

## 4. MiniPlasma (689 ★) - CVE-2020-17103 (Still Unpatched)  
**Language:** C#  
**Target:** cldflt.sys (Windows Cloud Files driver)  
**Status:** ❌ Repo gone

James Forshaw (Google Project Zero) found this 6 years ago. Microsoft supposedly patched it. But the patch was either silently rolled back or never applied properly.

Weaponized original PoC → spawns SYSTEM shell. Race condition. Works on ALL Windows versions.

This confirms a 6-year-old vulnerability is still live on every Windows machine.

---

## 5. GreenPlasma (646 ★) - CTFMON EoP  
**Language:** C++  
**Target:** CTFMON (Windows Text Services Framework)  
**Status:** ❌ Repo gone

Creates arbitrary memory section objects in SYSTEM-writable directories. Stripped PoC (intentionally incomplete - challenge for CTF community). Full exploit would give SYSTEM shell.

Confirmed working on: Windows 11, Server 2022, Server 2026. Unknown Windows 10.

---

## 6. UnDefend (541 ★) - Defender Signature Blocker  
**Language:** C++  
**Target:** Windows Defender (no admin needed)  
**Status:** ✅ Source preserved in this folder

Blocks Defender from receiving signature updates. File-locks update files immediately on creation. Two modes: passive (block updates) and aggressive (disable completely on platform update).

Author also discovered how to fake the EDR console (show Defender as healthy when it's blinded) but never published due to "waaay too much damage."
